//=== lib/Target/Z80/Z80MachinePreRAOptimization.cpp ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass does combining of machine instructions at the generic MI level,
// before register allocation.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "Z80.h"
#include "Z80InstrInfo.h"
#include "llvm/CodeGen/LiveRegUnits.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "z80-machine-prera-opt"

using namespace llvm;

namespace {
class Z80MachinePreRAOptimization : public MachineFunctionPass {
public:
  static char ID;

  Z80MachinePreRAOptimization() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override {
    return "Z80 Machine Pre-RA Optimization";
  }

  bool runOnMachineFunction(MachineFunction &MF) override;
};
} // end anonymous namespace

bool Z80MachinePreRAOptimization::runOnMachineFunction(MachineFunction &MF) {
  bool Changed = false;
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  SmallSet<Register, 16> FoldAsLoadDefCandidates;

  for (MachineBasicBlock &MBB : MF) {
    for (auto MII = MBB.begin(), MIE = MBB.end(); MII != MIE;) {
      MachineInstr *MI = &*MII;
      ++MII;

      if (MI->canFoldAsLoad() && MI->mayLoad() &&
          MI->getNumExplicitDefs() == 1) {
        MachineOperand &DefMO = MI->getOperand(0);
        if (DefMO.isReg()) {
          Register DefReg = DefMO.getReg();
          if (DefReg.isVirtual() && !DefMO.getSubReg() &&
              MRI.hasOneNonDBGUser(DefReg)) {
            FoldAsLoadDefCandidates.insert(DefReg);
            continue;
          }
        }
      }

      if (!FoldAsLoadDefCandidates.empty()) {
        for (MachineOperand &MO : MI->operands()) {
          if (!MO.isReg())
            continue;
          Register Reg = MO.getReg();
          if (MO.isDef()) {
            FoldAsLoadDefCandidates.erase(Reg);
            continue;
          }
          if (!FoldAsLoadDefCandidates.count(Reg))
            continue;
          MachineInstr *DefMI = nullptr;
          Register FoldAsLoadDefReg = Reg;
          if (MachineInstr *FoldMI =
                  TII.optimizeLoadInstr(*MI, &MRI, FoldAsLoadDefReg, DefMI)) {
            // Update LocalMIs since we replaced MI with FoldMI and deleted
            // DefMI.
            LLVM_DEBUG(dbgs() << "Replacing: " << *MI);
            LLVM_DEBUG(dbgs() << "     With: " << *FoldMI);
            // Update the call site info.
            if (MI->shouldUpdateCallSiteInfo())
              MF.moveCallSiteInfo(MI, FoldMI);
            MI->eraseFromParent();
            DefMI->eraseFromParent();
            MRI.markUsesInDebugValueAsUndef(Reg);
            FoldAsLoadDefCandidates.erase(Reg);

            // MI is replaced with FoldMI so we can continue trying to fold
            Changed = true;
            MI = FoldMI;
          }
        }
      }

      if (MI->isLoadFoldBarrier()) {
        LLVM_DEBUG(dbgs() << "Encountered load fold barrier on " << *MI);
        FoldAsLoadDefCandidates.clear();
      }
    }
  }
  return Changed;
}

char Z80MachinePreRAOptimization::ID = 0;
INITIALIZE_PASS(Z80MachinePreRAOptimization, DEBUG_TYPE,
                "Optimize Z80 machine instrs before regalloc", false, false)

FunctionPass *llvm::createZ80MachinePreRAOptimizationPass() {
  return new Z80MachinePreRAOptimization();
}
