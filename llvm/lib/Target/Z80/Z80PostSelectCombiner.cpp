//=== lib/CodeGen/GlobalISel/Z80PostSelectCombiner.cpp --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass does combining of machine instructions at the generic MI level,
// before the legalizer.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "Z80.h"
#include "Z80InstrInfo.h"
#include "Z80Subtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/Debug.h"
#include "llvm/Target/TargetMachine.h"

#define DEBUG_TYPE "z80-postselect-combiner"

using namespace llvm;

namespace {
class Z80PostSelectCombiner : public MachineFunctionPass {
public:
  static char ID;

  Z80PostSelectCombiner();

  StringRef getPassName() const override { return "Z80 Post Select Combiner"; }

  bool runOnMachineFunction(MachineFunction &MF) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override;
};

} // end anonymous namespace

void Z80PostSelectCombiner::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesCFG();
  MachineFunctionPass::getAnalysisUsage(AU);
}

Z80PostSelectCombiner::Z80PostSelectCombiner() : MachineFunctionPass(ID) {
  initializeZ80PostSelectCombinerPass(*PassRegistry::getPassRegistry());
}

bool Z80PostSelectCombiner::runOnMachineFunction(MachineFunction &MF) {
  MachineRegisterInfo &MRI = MF.getRegInfo();
  auto &STI = MF.getSubtarget<Z80Subtarget>();
  auto &TII = *STI.getInstrInfo();

  bool Changed = false;
  for (MachineBasicBlock &MBB : MF) {
    auto I = MBB.begin(), E = MBB.end();
    while (I != E) {
      MachineInstr &MI = *I;
      ++I;

      switch (MI.getOpcode()) {
      case TargetOpcode::COPY: {
        Register DstReg = MI.getOperand(0).getReg();
        if (DstReg != Z80::SPS && DstReg != Z80::SPL)
          break;
        Register TmpReg =
            MRI.createVirtualRegister(DstReg == Z80::SPL ? &Z80::A24RegClass
                                                         : &Z80::A16RegClass);
        BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(TargetOpcode::COPY), TmpReg)
            .add(MI.getOperand(1));
        MI.setDesc(TII.get(DstReg == Z80::SPL ? Z80::LD24sa : Z80::LD16sa));
        MI.removeOperand(0);
        MI.getOperand(0).setReg(TmpReg);
        MachineInstrBuilder(MF, MI).addReg(DstReg, RegState::ImplicitDefine);
        Changed = true;
        break;
      }
      case Z80::PUSH16r:
      case Z80::PUSH24r: {
        if (!STI.hasEZ80Ops())
          break;
        bool IsPush24 = MI.getOpcode() == Z80::PUSH24r;
        Register SrcReg = MI.getOperand(0).getReg();
        if (!SrcReg.isVirtual())
          break;
        if (!MRI.hasOneUse(SrcReg))
          break;
        // Some late MIR can still contain multiple defs for a vreg
        // (e.g. duplicate COPYs). This fold requires a unique defining LEA.
        MachineInstr *SrcMI = MRI.getUniqueVRegDef(SrcReg);
        if (!SrcMI ||
            SrcMI->getOpcode() != (IsPush24 ? Z80::LEA24ro : Z80::LEA16ro))
          break;
        MachineOperand &BaseMO = SrcMI->getOperand(1);
        auto NewOff = SrcMI->getOperand(2).getImm();
        if (!BaseMO.isReg() || NewOff) {
          MI.removeOperand(0);
          MI.setDesc(TII.get(IsPush24 ? Z80::PEA24o : Z80::PEA16o));
          MachineInstrBuilder(MF, MI).add(SrcMI->getOperand(1)).addImm(NewOff);
        } else
          MI.getOperand(0).setReg(BaseMO.getReg());
        SrcMI->eraseFromParent();
        Changed = true;
        break;
      }
      case Z80::BIT8gb: {
        auto Prev = MI.getIterator();

        while (Prev != MBB.begin()) {
          --Prev;
          if (!Prev->isDebugInstr())
            break;
        }

        if (Prev->getOpcode() != Z80::AND8_gisel ||
            MI.getOperand(0).getReg() != Prev->getOperand(0).getReg() ||
            MI.getOperand(0).getSubReg() ||
            Prev->getOperand(0).getSubReg())
          break;

        int64_t Bit = MI.getOperand(1).getImm();

        if (Bit < 0 || Bit > 7)
          break;

        bool MatchesMask = false;
        for (unsigned OpNo : {1u, 2u}) {
          const MachineOperand &MO = Prev->getOperand(OpNo);

          if (!MO.getReg().isVirtual() || MO.getSubReg() || MO.isUndef())
            continue;

          MachineInstr *Def = MRI.getUniqueVRegDef(MO.getReg());

          if (Def && Def->getOpcode() == Z80::LD8ri &&
              Def->getOperand(1).isImm() &&
              static_cast<uint8_t>(Def->getOperand(1).getImm()) == (1u << Bit)) {

            MatchesMask = true;
            break;
          }
        }

        if (!MatchesMask)
          break;

        Prev->clearRegisterDeads(Z80::F);
        MI.eraseFromParent();
        Changed = true;
        continue;
      }
      default:
        break;
      }
    }
  }

  return Changed;
}

char Z80PostSelectCombiner::ID = 0;
INITIALIZE_PASS_BEGIN(Z80PostSelectCombiner, DEBUG_TYPE,
                      "Combine Z80 machine instrs after inst selection", false,
                      false)
INITIALIZE_PASS_DEPENDENCY(TargetPassConfig);
INITIALIZE_PASS_DEPENDENCY(InstructionSelect);
INITIALIZE_PASS_END(Z80PostSelectCombiner, DEBUG_TYPE,
                    "Combine Z80 machine instrs after inst selection", false,
                    false)


FunctionPass *llvm::createZ80PostSelectCombiner() {
  return new Z80PostSelectCombiner;
}
