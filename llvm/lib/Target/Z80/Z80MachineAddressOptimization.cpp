//===-- Z80MachineAddressOptimization.cpp - Fold eZ80 addresses -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Fold physical address calculations after frame lowering and pseudo expansion.
// Run before branch selection so shorter instructions also shorten branches.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "Z80.h"
#include "Z80InstrInfo.h"
#include "Z80Subtarget.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/MathExtras.h"

using namespace llvm;

#define DEBUG_TYPE "z80-machine-address-opt"

STATISTIC(NumLEAChains, "Number of eZ80 LEA chains folded");
STATISTIC(NumIndexedAddresses,
          "Number of eZ80 temporary indexed addresses folded");
STATISTIC(NumSmallAdds, "Number of eZ80 small index-register additions folded");

static cl::opt<bool> EnableAddressOpt("z80-enable-machine-address-opt", cl::Hidden,
                                      cl::init(true));
static cl::opt<bool> EnableLEAChains("z80-address-opt-lea-chains", cl::Hidden,
                                     cl::init(true));
static cl::opt<bool> EnableIndexedAddresses("z80-address-opt-indexed-memory",
                                            cl::Hidden, cl::init(true));
static cl::opt<bool> EnableSmallAdds("z80-address-opt-small-add", cl::Hidden,
                                     cl::init(true));

namespace {
class Z80MachineAddressOptimization : public MachineFunctionPass {
public:
  static char ID;
  Z80MachineAddressOptimization() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override {
    return "Z80 Machine Address Optimization";
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

static bool isLEA(const MachineInstr &MI) {
  return MI.getOpcode() == Z80::LEA16ro || MI.getOpcode() == Z80::LEA24ro;
}

static bool canFold(const MachineInstr &MI) {
  return !MI.isMetaInstruction() && !MI.isBundled() &&
         !MI.getFlag(MachineInstr::FrameSetup) &&
         !MI.getFlag(MachineInstr::FrameDestroy);
}

static int indexedBaseOperand(unsigned Opcode) {
  switch (Opcode) {
  case Z80::LD8go:
  case Z80::LD16ro:
  case Z80::LD24ro:
    return 1;
  case Z80::LD8og:
  case Z80::LD16or:
  case Z80::LD24or:
  case Z80::LD8oi:
    return 0;
  default:
    return -1;
  }
}

bool Z80MachineAddressOptimization::runOnMachineFunction(MachineFunction &MF) {
  const auto &STI = MF.getSubtarget<Z80Subtarget>();
  if (skipFunction(MF.getFunction()) || !EnableAddressOpt || !STI.hasEZ80Ops())
    return false;

  const auto &TII = *STI.getInstrInfo();
  const auto &TRI = *STI.getRegisterInfo();
  const auto &MRI = MF.getRegInfo();
  bool Changed = false;
  LivePhysRegs Live(TRI);

  // Walking backwards makes liveness after each candidate available without
  // rescanning the rest of a potentially very large basic block.
  for (MachineBasicBlock &MBB : MF) {
    Live.clear();
    Live.addLiveOuts(MBB);
    for (auto I = MBB.end(); I != MBB.begin();) {
      I = std::prev(I);
      MachineInstr &Last = *I;
      if (I != MBB.begin() && canFold(Last)) {
        auto FirstI = std::prev(I);
        MachineInstr &First = *FirstI;
        if (canFold(First) && isLEA(First) &&
            First.getNumExplicitOperands() == 3 &&
            First.getOperand(0).isReg() && First.getOperand(1).isReg() &&
            !First.getOperand(0).getSubReg() &&
            !First.getOperand(1).getSubReg() && First.getOperand(2).isImm()) {
          Register Temp = First.getOperand(0).getReg();
          Register Base = First.getOperand(1).getReg();
          int64_t Offset = First.getOperand(2).getImm();
          int BaseIdx = -1;
          bool IsChain = EnableLEAChains && isLEA(Last) &&
                         Last.getOpcode() == First.getOpcode();
          if (IsChain)
            BaseIdx = 1;
          else if (EnableIndexedAddresses &&
                   (First.getOpcode() == Z80::LEA24ro) == STI.is24Bit())
            BaseIdx = indexedBaseOperand(Last.getOpcode());

          if (BaseIdx >= 0 && Last.getOperand(BaseIdx).isReg() &&
              !Last.getOperand(BaseIdx).getSubReg() &&
              Last.getOperand(BaseIdx).getReg() == Temp &&
              Last.getOperand(BaseIdx + 1).isImm()) {
            int64_t Combined = Offset + Last.getOperand(BaseIdx + 1).getImm();
            bool OverwritesTemp =
                IsChain && Last.getOperand(0).getReg() == Temp;
            bool OtherUse = false;
            for (unsigned N = 0, E = Last.getNumOperands(); N != E; ++N) {
              const MachineOperand &MO = Last.getOperand(N);
              if (N != static_cast<unsigned>(BaseIdx) && MO.isReg() &&
                  MO.isUse() && MO.getReg() &&
                  TRI.regsOverlap(MO.getReg(), Temp))
                OtherUse = true;
            }
            if (isInt<8>(Combined) && !OtherUse &&
                (OverwritesTemp || Live.available(MRI, Temp))) {
              bool BaseKilled = First.getOperand(1).isKill();
              bool BaseUndef = First.getOperand(1).isUndef();
              Last.getOperand(BaseIdx).setReg(Base);
              Last.getOperand(BaseIdx).setIsKill(BaseKilled);
              Last.getOperand(BaseIdx).setIsUndef(BaseUndef);
              Last.getOperand(BaseIdx + 1).setImm(Combined);
              First.eraseFromParent();
              if (IsChain)
                ++NumLEAChains;
              else
                ++NumIndexedAddresses;
              Changed = true;
            }
          }
        } else if (canFold(First) && EnableSmallAdds) {
          bool Is24 = Last.getOpcode() == Z80::ADD24ao;
          bool IsAdd = Is24 || Last.getOpcode() == Z80::ADD16ao;
          unsigned LoadOpcode = Is24 ? Z80::LD24ri : Z80::LD16ri;
          if (IsAdd && First.getOpcode() == LoadOpcode &&
              First.getOperand(1).isImm() && Last.getNumOperands() == 4 &&
              Last.getOperand(2).isReg()) {
            Register Dst = Last.getOperand(0).getReg();
            Register Temp = First.getOperand(0).getReg();
            bool IsIndex = Is24 ? Dst == Z80::UIX || Dst == Z80::UIY
                                : Dst == Z80::IX || Dst == Z80::IY;
            int64_t Offset =
                Is24 ? SignExtend64<24>(First.getOperand(1).getImm())
                     : SignExtend64<16>(First.getOperand(1).getImm());
            if (IsIndex && Last.getOperand(1).getReg() == Dst &&
                Last.getOperand(2).getReg() == Temp &&
                !TRI.regsOverlap(Temp, Dst) && isInt<8>(Offset) &&
                Live.available(MRI, Temp) && Live.available(MRI, Z80::F)) {
              Last.untieRegOperand(1);
              Last.setDesc(TII.get(Is24 ? Z80::LEA24ro : Z80::LEA16ro));
              Last.getOperand(2).ChangeToImmediate(Offset);
              Last.removeOperand(3); // ADD's flags definition.
              First.eraseFromParent();
              ++NumSmallAdds;
              Changed = true;
            }
          }
        }
      }
      Live.stepBackward(Last);
    }
  }
  return Changed;
}

char Z80MachineAddressOptimization::ID = 0;
INITIALIZE_PASS(Z80MachineAddressOptimization, DEBUG_TYPE,
                "Z80 Machine Address Optimization", false, false)

FunctionPass *llvm::createZ80MachineAddressOptimizationPass() {
  return new Z80MachineAddressOptimization();
}
