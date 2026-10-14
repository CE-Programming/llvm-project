//===-- Z80MachinePeepholeOptimization.cpp - Final eZ80 peepholes --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Selected peepholes inspired by ZERICO2005/ez80_optimizer, implemented on
// machine instructions with register, width, and flag checks. See
// Z80PeepholePatterns.md for upstream attribution and supported patterns.
// In particular,
// two exchanges cancel entirely; they must not be replaced by one exchange.
// Run after pseudo expansion and address folding, before branch selection.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "Z80.h"
#include "Z80InstrInfo.h"
#include "Z80Subtarget.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/MathExtras.h"

using namespace llvm;

#define DEBUG_TYPE "z80-machine-peephole-opt"
STATISTIC(NumLEASteps, "Number of eZ80 LEA increment/decrement folds");
STATISTIC(NumExchanges, "Number of cancelling eZ80 exchange pairs removed");
STATISTIC(NumStackCopies, "Number of final eZ80 stack copies folded");
STATISTIC(NumCarryRotates,
          "Number of carry-only eZ80 accumulator rotates folded");
STATISTIC(NumSignExtensions,
          "Number of eZ80 accumulator sign extensions folded");
STATISTIC(NumBooleanTests,
          "Number of final redundant eZ80 boolean tests removed");

static cl::opt<bool> EnablePeepholeOpt("z80-machine-peephole-opt", cl::Hidden,
                                       cl::init(true));

namespace {
class Z80MachinePeepholeOptimization : public MachineFunctionPass {
public:
  static char ID;
  Z80MachinePeepholeOptimization() : MachineFunctionPass(ID) {}
  StringRef getPassName() const override {
    return "Z80 Machine Peephole Optimization";
  }
  MachineFunctionProperties getRequiredProperties() const override {
    return MachineFunctionProperties().set(
        MachineFunctionProperties::Property::NoVRegs);
  }
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    MachineFunctionPass::getAnalysisUsage(AU);
  }
  bool runOnMachineFunction(MachineFunction &MF) override;
};
} // namespace

static bool canFold(const MachineInstr &MI) {
  return !MI.isMetaInstruction() && !MI.isBundled() &&
         !MI.getFlag(MachineInstr::FrameSetup) &&
         !MI.getFlag(MachineInstr::FrameDestroy);
}

static bool isReg(const MachineOperand &MO, Register Reg) {
  return MO.isReg() && !MO.getSubReg() && MO.getReg() == Reg;
}

static bool isAccumulatorSBC(const MachineInstr &MI) {
  return MI.getOpcode() == Z80::SBC8ar && isReg(MI.getOperand(0), Z80::A);
}

static bool isBooleanProducer(const MachineInstr &MI) {
  return isAccumulatorSBC(MI) ||
         (MI.getOpcode() == Z80::AND8ai && MI.getOperand(0).isImm() &&
          MI.getOperand(0).getImm() == 1);
}

bool Z80MachinePeepholeOptimization::runOnMachineFunction(MachineFunction &MF) {
  const auto &STI = MF.getSubtarget<Z80Subtarget>();
  if (!EnablePeepholeOpt || !STI.hasEZ80Ops() || skipFunction(MF.getFunction()))
    return false;
  const auto &TII = *STI.getInstrInfo();
  const auto &TRI = *STI.getRegisterInfo();
  const auto &MRI = MF.getRegInfo();
  LivePhysRegs Live(TRI);
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    Live.clear();
    Live.addLiveOuts(MBB);
    for (auto I = MBB.end(); I != MBB.begin();) {
      --I;
      MachineInstr &Last = *I;
      if (!canFold(Last) || I == MBB.begin()) {
        Live.stepBackward(Last);
        continue;
      }
      auto FirstI = std::prev(I);
      MachineInstr &First = *FirstI;
      if (!canFold(First)) {
        Live.stepBackward(Last);
        continue;
      }

      // Both instructions leave flags unchanged, including carry.
      bool Is24LEA = First.getOpcode() == Z80::LEA24ro;
      if ((Is24LEA || First.getOpcode() == Z80::LEA16ro) &&
          (Last.getOpcode() == (Is24LEA ? Z80::INC24r : Z80::INC16r) ||
           Last.getOpcode() == (Is24LEA ? Z80::DEC24r : Z80::DEC16r)) &&
          isReg(Last.getOperand(0), First.getOperand(0).getReg()) &&
          isReg(Last.getOperand(1), First.getOperand(0).getReg())) {
        bool IsInc =
            Last.getOpcode() == Z80::INC24r || Last.getOpcode() == Z80::INC16r;
        int64_t Offset = First.getOperand(2).getImm() + (IsInc ? 1 : -1);
        if (isInt<8>(Offset)) {
          First.getOperand(2).setImm(Offset);
          First.getOperand(0).setIsDead(Last.getOperand(0).isDead());
          I = FirstI;
          Last.eraseFromParent();
          ++NumLEASteps;
          Changed = true;
          Live.stepBackward(*I);
          continue;
        }
      }

      // Short exchanges also clear upper bytes on eZ80. Two such
      // exchanges are not an identity for a later mixed-width observer.
      if (Last.getOpcode() == Z80::EX24DE &&
          First.getOpcode() == Last.getOpcode()) {
        First.eraseFromParent();
        I = MBB.erase(I);
        ++NumExchanges;
        Changed = true;
        continue;
      }

      bool Is24Push = First.getOpcode() == Z80::PUSH24r;
      if ((Is24Push || First.getOpcode() == Z80::PUSH16r) &&
          Last.getOpcode() == (Is24Push ? Z80::POP24r : Z80::POP16r) &&
          First.memoperands_empty() && Last.memoperands_empty()) {
        Register Src = First.getOperand(0).getReg();
        Register Dst = Last.getOperand(0).getReg();
        Register HL = Is24Push ? Z80::UHL : Z80::HL;
        Register DE = Is24Push ? Z80::UDE : Z80::DE;
        if (Is24Push && Src == Dst) {
          First.eraseFromParent();
          I = MBB.erase(I);
          ++NumStackCopies;
          Changed = true;
          continue;
        }
        // Unlike a stack copy, EX overwrites the source. Require the original
        // copy's kill proof as well as alias-aware liveness: pseudo-expanded
        // index-half copies can hide a live carrier behind undef EX operands.
        if (((Src == HL && Dst == DE) || (Src == DE && Dst == HL)) &&
            First.getOperand(0).isKill() &&
            Live.available(MRI, Src)) {
          auto Ex = BuildMI(MBB, I, Last.getDebugLoc(),
                            TII.get(Is24Push ? Z80::EX24DE : Z80::EX16DE));
          Ex->findRegisterUseOperand(Src, nullptr)
              ->setIsKill(First.getOperand(0).isKill());
          Ex->findRegisterUseOperand(Src, nullptr)
              ->setIsUndef(First.getOperand(0).isUndef());
          Ex->findRegisterUseOperand(Dst, nullptr)->setIsUndef();
          Ex->findRegisterDefOperand(Src, nullptr)->setIsDead();
          Ex->findRegisterDefOperand(Dst, nullptr)
              ->setIsDead(Last.getOperand(0).isDead());
          First.eraseFromParent();
          Last.eraseFromParent();
          I = Ex->getIterator();
          ++NumStackCopies;
          Changed = true;
          Live.stepBackward(*I);
          continue;
        }
      }

      // SBC A,A consumes only carry and overwrites all flags, so the S/Z/PV
      // differences between RLC/RRC A and RLCA/RRCA cannot escape this pair.
      if (isAccumulatorSBC(Last) &&
          (First.getOpcode() == Z80::RLC8g ||
           First.getOpcode() == Z80::RRC8g) &&
          isReg(First.getOperand(0), Z80::A) &&
          isReg(First.getOperand(1), Z80::A)) {
        auto Rotate = BuildMI(
            MBB, FirstI, First.getDebugLoc(),
            TII.get(First.getOpcode() == Z80::RLC8g ? Z80::RLCA : Z80::RRCA));
        Rotate->findRegisterUseOperand(Z80::A, nullptr)
            ->setIsUndef(First.getOperand(1).isUndef());
        Rotate->findRegisterUseOperand(Z80::A, nullptr)
            ->setIsKill(First.getOperand(1).isKill());
        First.eraseFromParent();
        ++NumCarryRotates;
        Changed = true;
      }

      // LD L,A; RLC L; SBC HL,HL -> RLCA; RRCA; SBC HL,HL. A is restored
      // and the final SBC uses the original sign bit as carry. Its result and
      // flags are independent of the incoming value of HL.
      // First may have been erased by the preceding rotate fold.
      FirstI = std::prev(I);
      MachineInstr &Prev = *FirstI;
      if ((Last.getOpcode() == Z80::SBC24aa ||
           Last.getOpcode() == Z80::SBC16aa) &&
          Prev.getOpcode() == Z80::RLC8g && isReg(Prev.getOperand(0), Z80::L) &&
          isReg(Prev.getOperand(1), Z80::L) && FirstI != MBB.begin()) {
        auto LoadI = std::prev(FirstI);
        MachineInstr &Load = *LoadI;
        if (canFold(Load) && Load.getOpcode() == Z80::LD8gg &&
            isReg(Load.getOperand(0), Z80::L) &&
            isReg(Load.getOperand(1), Z80::A)) {
          auto Left =
              BuildMI(MBB, LoadI, Load.getDebugLoc(), TII.get(Z80::RLCA));
          Left->findRegisterUseOperand(Z80::A, nullptr)
              ->setIsUndef(Load.getOperand(1).isUndef());
          BuildMI(MBB, FirstI, Prev.getDebugLoc(), TII.get(Z80::RRCA));
          Register HL = Last.getOpcode() == Z80::SBC24aa ? Z80::UHL : Z80::HL;
          Last.findRegisterUseOperand(HL, nullptr)->setIsUndef();
          Load.eraseFromParent();
          Prev.eraseFromParent();
          ++NumSignExtensions;
          Changed = true;
        }
      }

      // Only remove boolean tests when the sole observer is a Z/NZ branch.
      // BIT's other flag differences must be dead on every successor.
      FirstI = std::prev(I);
      MachineInstr &Test = *FirstI;
      bool IsZBranch =
          (Last.getOpcode() == Z80::JQCC || Last.getOpcode() == Z80::JRCC) &&
          (Last.getOperand(1).getImm() == Z80::COND_Z ||
           Last.getOperand(1).getImm() == Z80::COND_NZ);
      bool IsBit0 = Test.getOpcode() == Z80::BIT8gb &&
                    isReg(Test.getOperand(0), Z80::A) &&
                    Test.getOperand(1).getImm() == 0;
      bool IsOrA =
          Test.getOpcode() == Z80::OR8ar && isReg(Test.getOperand(0), Z80::A);
      if (IsZBranch && (IsBit0 || IsOrA) && FirstI != MBB.begin() &&
          Live.available(MRI, Z80::F)) {
        MachineInstr &Producer = *std::prev(FirstI);
        if (canFold(Producer) && isBooleanProducer(Producer)) {
          Producer.clearRegisterDeads(Z80::F);
          Test.eraseFromParent();
          ++NumBooleanTests;
          Changed = true;
        }
      }
      Live.stepBackward(Last);
    }
  }
  return Changed;
}

char Z80MachinePeepholeOptimization::ID = 0;
INITIALIZE_PASS(Z80MachinePeepholeOptimization, DEBUG_TYPE,
                "Z80 Machine Peephole Optimization", false, false)

FunctionPass *llvm::createZ80MachinePeepholeOptimizationPass() {
  return new Z80MachinePeepholeOptimization();
}
