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
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/CodeGen/LiveRegUnits.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
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

static Register resolveExtractSubregSource(Register SrcReg, unsigned SubIdx,
                                           MachineInstr &UseMI,
                                           MachineRegisterInfo &MRI) {
  // chase a unique-def chain in SSA form
  for (Register CurReg = SrcReg;;) {
    if (!CurReg.isVirtual())
      break;
    MachineInstr *DefMI = MRI.getUniqueVRegDef(CurReg);
    if (!DefMI)
      break;
    switch (DefMI->getOpcode()) {
    case TargetOpcode::COPY:
      CurReg = DefMI->getOperand(1).getReg();
      continue;
    case TargetOpcode::REG_SEQUENCE: {
      // REG_SEQUENCE operands are (reg, subidx) pairs
      for (unsigned Op = 1, E = DefMI->getNumOperands(); Op + 1 < E; Op += 2) {
        const MachineOperand &RegMO = DefMI->getOperand(Op);
        const MachineOperand &SubMO = DefMI->getOperand(Op + 1);
        if (!RegMO.isReg() || !SubMO.isImm())
          return Register();
        if (unsigned(SubMO.getImm()) == SubIdx)
          return RegMO.getReg();
      }
      return Register();
    }
    case TargetOpcode::INSERT_SUBREG: {
      // INSERT_SUBREG dst, base, insert, subidx
      const MachineOperand &SubMO = DefMI->getOperand(3);
      if (!SubMO.isImm())
        return Register();
      unsigned InsertedSubIdx = SubMO.getImm();
      if (InsertedSubIdx == SubIdx)
        return DefMI->getOperand(2).getReg();
      CurReg = DefMI->getOperand(1).getReg();
      continue;
    }
    default:
      return Register();
    }
  }

  // local backward scan for non SSA in place subreg construction
  // this is intentionally conservative and only looks within the same block
  if (!SrcReg.isVirtual())
    return Register();

  MachineBasicBlock &MBB = *UseMI.getParent();
  for (auto It = UseMI.getIterator(); It != MBB.begin();) {
    --It;
    MachineInstr &DefMI = *It;

    const MachineOperand *DefMO = nullptr;
    for (const MachineOperand &MO : DefMI.operands()) {
      if (MO.isReg() && MO.isDef() && MO.getReg() == SrcReg) {
        DefMO = &MO;
        break;
      }
    }
    if (!DefMO)
      continue;

    // a full def clobbers the whole register, stop
    if (!DefMO->getSubReg())
      return Register();
    if (DefMO->getSubReg() != SubIdx)
      continue;

    switch (DefMI.getOpcode()) {
    case TargetOpcode::COPY:
      return DefMI.getOperand(1).getReg();
    case TargetOpcode::REG_SEQUENCE: {
      for (unsigned Op = 1, E = DefMI.getNumOperands(); Op + 1 < E; Op += 2) {
        const MachineOperand &RegMO = DefMI.getOperand(Op);
        const MachineOperand &SubMO = DefMI.getOperand(Op + 1);
        if (!RegMO.isReg() || !SubMO.isImm())
          return Register();
        if (unsigned(SubMO.getImm()) == SubIdx)
          return RegMO.getReg();
      }
      return Register();
    }
    case TargetOpcode::INSERT_SUBREG:
      return DefMI.getOperand(2).getReg();
    default:
      return Register();
    }
  }

  return Register();
}

static void tryEraseDeadVRegs(ArrayRef<Register> Roots,
                              MachineRegisterInfo &MRI) {
  SmallVector<Register, 8> Worklist;
  Worklist.append(Roots.begin(), Roots.end());

  while (!Worklist.empty()) {
    Register VReg = Worklist.pop_back_val();
    if (!VReg.isVirtual() || !MRI.use_nodbg_empty(VReg))
      continue;

    MachineInstr *DefMI = MRI.getUniqueVRegDef(VReg);
    if (!DefMI || DefMI->hasUnmodeledSideEffects() || DefMI->mayLoadOrStore() ||
        DefMI->isCall() || DefMI->isInlineAsm() ||
        DefMI->getNumExplicitDefs() != 1)
      continue;

    SmallVector<Register, 4> SrcRegs;
    for (const MachineOperand &MO : DefMI->operands()) {
      if (MO.isReg() && MO.isUse()) {
        Register Reg = MO.getReg();
        if (Reg)
          SrcRegs.push_back(Reg);
      }
    }

    DefMI->eraseFromParent();
    for (Register Reg : SrcRegs)
      Worklist.push_back(Reg);
  }
}

static bool isRegClassCompatibleForReplace(const TargetRegisterClass *DstRC,
                                           const TargetRegisterClass *RepRC) {
  if (!DstRC || !RepRC)
    return false;
  if (DstRC == RepRC)
    return true;

  // allow replacing a vreg with a vreg in a subclass register class
  // this preserves all operand constraints that were satisfied by DstRC while
  // still letting us eliminate redundant pack/unpack scaffolding
  return DstRC->hasSubClassEq(RepRC);
}

static bool isPhysicalCopyDef(const MachineInstr &MI) {
  if (MI.getOpcode() != TargetOpcode::COPY)
    return false;
  return MI.getOperand(0).getReg().isPhysical();
}

static bool isCallSetupGlue(const MachineInstr &MI) {
  switch (MI.getOpcode()) {
  default:
    return false;
  case TargetOpcode::IMPLICIT_DEF:
  case TargetOpcode::INSERT_SUBREG:
    return true;
  case TargetOpcode::COPY:
    return MI.getOperand(0).getReg().isVirtual() &&
           MI.getOperand(1).getReg().isVirtual();
  }
}

static bool callUsesReg(const MachineInstr &CallMI, Register Reg,
                        const TargetRegisterInfo &TRI) {
  for (const MachineOperand &MO : CallMI.operands())
    if (MO.isReg() && MO.isUse() && MO.getReg() != Register() &&
        TRI.regsOverlap(MO.getReg(), Reg))
      return true;

  return false;
}

static bool reachesCallSetupUse(MachineInstr &MI, Register PhysReg,
                                const TargetRegisterInfo &TRI) {
  MachineBasicBlock &MBB = *MI.getParent();
  auto E = MBB.end();
  for (auto I = std::next(MI.getIterator()); I != E; ++I) {
    if (I->isDebugInstr())
      continue;

    if (I->isCall())
      return PhysReg == Register() || callUsesReg(*I, PhysReg, TRI);

    if (PhysReg != Register() && I->modifiesRegister(PhysReg, &TRI))
      return false;

    if (I->getOpcode() == Z80::PUSH16r || I->getOpcode() == Z80::PUSH24r ||
        isPhysicalCopyDef(*I) ||
        isCallSetupGlue(*I))
      continue;

    return false;
  }

  return false;
}

static bool isAtOrBefore(MachineInstr &MI, MachineInstr &UseMI) {
  if (MI.getParent() != UseMI.getParent())
    return false;

  for (auto I = MachineBasicBlock::iterator(MI), E = MI.getParent()->end();
       I != E; ++I)
    if (&*I == &UseMI)
      return true;

  return false;
}

static bool canMoveDefBeforeUse(Register Reg, MachineInstr &UserMI,
                                MachineInstr &InsertMI,
                                MachineRegisterInfo &MRI) {
  if (MRI.hasOneNonDBGUse(Reg))
    return true;

  for (MachineOperand &MO : MRI.use_nodbg_operands(Reg)) {
    MachineInstr *OtherMI = MO.getParent();
    if (OtherMI == &UserMI)
      continue;
    if (OtherMI->getParent() != InsertMI.getParent())
      return false;
    if (isAtOrBefore(*OtherMI, InsertMI))
      return false;
  }

  return true;
}

static bool isImmutableFixedStackLoad(const MachineInstr &MI,
                                      const MachineFrameInfo &MFI,
                                      const TargetInstrInfo &TII) {
  int FI = 0;
  return TII.isLoadFromStackSlot(MI, FI) != Register() &&
         MFI.isFixedObjectIndex(FI) && MFI.isImmutableObjectIndex(FI) &&
         !MFI.isAliasedObjectIndex(FI);
}

static bool collectFixedStackArgDefChain(
    Register Reg, MachineInstr &UserMI, MachineInstr &InsertMI,
    MachineRegisterInfo &MRI, const MachineFrameInfo &MFI,
    const TargetInstrInfo &TII,
    SmallVectorImpl<MachineInstr *> &Defs,
    SmallPtrSetImpl<MachineInstr *> &Seen, bool &HasFixedStackLoad) {
  if (!Reg.isVirtual())
    return false;

  MachineInstr *DefMI = MRI.getUniqueVRegDef(Reg);
  if (!DefMI || !isAtOrBefore(*DefMI, UserMI) ||
      !canMoveDefBeforeUse(Reg, UserMI, InsertMI, MRI))
    return false;

  if (!Seen.insert(DefMI).second)
    return true;

  switch (DefMI->getOpcode()) {
  case TargetOpcode::COPY:
    if (!collectFixedStackArgDefChain(DefMI->getOperand(1).getReg(), *DefMI,
                                      InsertMI, MRI, MFI, TII, Defs, Seen,
                                      HasFixedStackLoad))
      return false;
    break;
  case TargetOpcode::IMPLICIT_DEF:
    break;
  case TargetOpcode::INSERT_SUBREG:
    if (!collectFixedStackArgDefChain(DefMI->getOperand(1).getReg(), *DefMI,
                                      InsertMI, MRI, MFI, TII, Defs, Seen,
                                      HasFixedStackLoad) ||
        !collectFixedStackArgDefChain(DefMI->getOperand(2).getReg(), *DefMI,
                                      InsertMI, MRI, MFI, TII, Defs, Seen,
                                      HasFixedStackLoad))
      return false;
    break;
  default:
    if (!isImmutableFixedStackLoad(*DefMI, MFI, TII))
      return false;

    HasFixedStackLoad = true;
    break;
  }

  Defs.push_back(DefMI);
  return true;
}

static bool sinkFixedStackCallArgDefChain(MachineInstr &MI,
                                          MachineRegisterInfo &MRI,
                                          const MachineFrameInfo &MFI,
                                          const TargetInstrInfo &TII,
                                          const TargetRegisterInfo &TRI) {
  Register UseReg = Register();
  Register PhysReg = Register();

  if (MI.getOpcode() == Z80::PUSH16r || MI.getOpcode() == Z80::PUSH24r) {
    UseReg = MI.getOperand(0).getReg();
    if (!reachesCallSetupUse(MI, Register(), TRI))
      return false;
  } else if (isPhysicalCopyDef(MI)) {
    PhysReg = MI.getOperand(0).getReg();
    UseReg = MI.getOperand(1).getReg();
    if (!reachesCallSetupUse(MI, PhysReg, TRI))
      return false;
  } else {
    return false;
  }

  SmallVector<MachineInstr *, 4> Defs;
  SmallPtrSet<MachineInstr *, 4> Seen;
  bool HasFixedStackLoad = false;
  if (!collectFixedStackArgDefChain(UseReg, MI, MI, MRI, MFI, TII, Defs, Seen,
                                    HasFixedStackLoad) ||
      !HasFixedStackLoad)
    return false;

  MachineBasicBlock &MBB = *MI.getParent();
  for (MachineInstr *DefMI : Defs)
    MBB.splice(MachineBasicBlock::iterator(MI), &MBB,
               MachineBasicBlock::iterator(*DefMI));

  return true;
}

bool Z80MachinePreRAOptimization::runOnMachineFunction(MachineFunction &MF) {
  bool Changed = false;
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  const TargetRegisterInfo &TRI = *MF.getSubtarget().getRegisterInfo();
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  SmallSet<Register, 16> FoldAsLoadDefCandidates;

  for (MachineBasicBlock &MBB : MF) {
    for (auto MII = MBB.begin(), MIE = MBB.end(); MII != MIE;) {
      MachineInstr *MI = &*MII++;
      if (MI->getOpcode() != TargetOpcode::EXTRACT_SUBREG)
        continue;

      Register DstReg = MI->getOperand(0).getReg();
      Register SrcReg = MI->getOperand(1).getReg();
      const MachineOperand &SubMO = MI->getOperand(2);
      if (!DstReg.isVirtual() || !SubMO.isImm())
        continue;
      unsigned SubIdx = SubMO.getImm();

      Register Replacement =
          resolveExtractSubregSource(SrcReg, SubIdx, *MI, MRI);
      if (!Replacement)
        continue;

      const TargetRegisterClass *DstRC = MRI.getRegClassOrNull(DstReg);
      if (!DstRC)
        continue;
      if (Replacement.isVirtual()) {
        const TargetRegisterClass *RepRC = MRI.getRegClassOrNull(Replacement);
        if (!isRegClassCompatibleForReplace(DstRC, RepRC))
          continue;
      } else if (!DstRC->contains(Replacement)) {
        continue;
      }

      // LLVM_DEBUG(dbgs() << "Propagating EXTRACT_SUBREG: " << *MI);
      MRI.replaceRegWith(DstReg, Replacement);
      MI->eraseFromParent();
      Changed = true;

      tryEraseDeadVRegs({DstReg, SrcReg}, MRI);
    }
  }

  for (MachineBasicBlock &MBB : MF) {
    for (auto MII = MBB.begin(), MIE = MBB.end(); MII != MIE;) {
      MachineInstr *MI = &*MII++;
      Changed |= sinkFixedStackCallArgDefChain(*MI, MRI, MFI, TII, TRI);
    }
  }

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
            if (MI->shouldUpdateAdditionalCallInfo())
              MF.moveAdditionalCallInfo(MI, FoldMI);
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
