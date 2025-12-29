//===-- Z80R64SpillPass.cpp - R64_24 Decomposition Pass -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass decomposes R64_24 virtual registers into separate (r24, r24, r16)
// component registers to prevent register allocation failures. The Z80's
// R64_24 register class has very few valid tuples (5-8), and when multiple
// R64_24 values need to be allocated simultaneously, the allocator runs out.
//
// The pass:
// 1. Finds all R64_24 virtual registers
// 2. Creates three replacement registers: Lo24 (r24), Mid24 (r24), Hi16 (r16)
// 3. Rewrites REG_SEQUENCE defs to populate the component registers
// 4. Rewrites EXTRACT_SUBREG uses to read from the appropriate component
// 5. Erases the original R64_24 definitions once all uses are rewritten
//
// This matches the v15 backend's approach of keeping 64-bit values decomposed.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "Z80.h"
#include "Z80InstrInfo.h"
#include "Z80RegisterInfo.h"
#include "Z80Subtarget.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "z80-r64-spill"

using namespace llvm;

namespace {

/// tracks the decomposed components of an R64_24 register
struct DecomposedR64 {
  Register Lo24;   // sub_low24 component
  Register Mid24;  // sub_mid24 component  
  Register Hi16;   // sub_word3 component
  bool IsValid = false;
};

class Z80R64SpillPass : public MachineFunctionPass {
public:
  static char ID;
  Z80R64SpillPass() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override {
    return "Z80 R64_24 Decomposition Pass";
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  const Z80Subtarget *STI = nullptr;
  const Z80InstrInfo *TII = nullptr;
  const Z80RegisterInfo *TRI = nullptr;
  MachineRegisterInfo *MRI = nullptr;

  /// from original R64_24 vreg to its decomposed components
  DenseMap<Register, DecomposedR64> DecompMap;

  bool decomposeR64Registers(MachineFunction &MF);
  void rewriteR64Def(MachineInstr &MI, Register R64Reg);
  void rewriteR64Uses(Register R64Reg, SmallVectorImpl<MachineInstr *> &ToErase);
  DecomposedR64 getOrCreateDecomp(Register R64Reg);
};

}

char Z80R64SpillPass::ID = 0;

INITIALIZE_PASS(Z80R64SpillPass, DEBUG_TYPE,
                "Decompose R64_24 registers into components", false, false)

/// get or create decomposed component registers for an R64_24 register
DecomposedR64 Z80R64SpillPass::getOrCreateDecomp(Register R64Reg) {
  auto It = DecompMap.find(R64Reg);
  if (It != DecompMap.end())
    return It->second;

  DecomposedR64 D;
  D.Lo24 = MRI->createVirtualRegister(&Z80::R24RegClass);
  D.Mid24 = MRI->createVirtualRegister(&Z80::R24RegClass);
  D.Hi16 = MRI->createVirtualRegister(&Z80::R16RegClass);
  D.IsValid = true;
  
  DecompMap[R64Reg] = D;
  
  LLVM_DEBUG(dbgs() << "Decomposing " << printReg(R64Reg, TRI) << " into "
                    << printReg(D.Lo24, TRI) << ", "
                    << printReg(D.Mid24, TRI) << ", "
                    << printReg(D.Hi16, TRI) << "\n");
  
  return D;
}

/// rewrite the definition of an R64_24 register
/// handles REG_SEQUENCE, INSERT_SUBREG, and IMPLICIT_DEF patterns
void Z80R64SpillPass::rewriteR64Def(MachineInstr &MI, Register R64Reg) {
  MachineBasicBlock &MBB = *MI.getParent();
  DebugLoc DL = MI.getDebugLoc();
  DecomposedR64 D = getOrCreateDecomp(R64Reg);

  unsigned Opc = MI.getOpcode();
  auto InsertPt = std::next(MI.getIterator());
  
  if (Opc == TargetOpcode::IMPLICIT_DEF) {
    // IMPLICIT_DEF -> create IMPLICIT_DEF for each component
    BuildMI(MBB, InsertPt, DL, TII->get(TargetOpcode::IMPLICIT_DEF), D.Lo24);
    BuildMI(MBB, InsertPt, DL, TII->get(TargetOpcode::IMPLICIT_DEF), D.Mid24);
    BuildMI(MBB, InsertPt, DL, TII->get(TargetOpcode::IMPLICIT_DEF), D.Hi16);
    
    LLVM_DEBUG(dbgs() << "  Decomposed IMPLICIT_DEF: " << MI);
    return;
  }
  
  if (Opc == TargetOpcode::REG_SEQUENCE) {
    // REG_SEQUENCE %dst, %lo, sub_low24, %mid, sub_mid24, %hi, sub_word3
    // -> COPY each component to decomposed regs
    
    Register Lo = Register(), Mid = Register(), Hi = Register();
    
    for (unsigned I = 1, E = MI.getNumOperands(); I < E; I += 2) {
      if (I + 1 >= E) break;
      Register SrcReg = MI.getOperand(I).getReg();
      unsigned SubIdx = MI.getOperand(I + 1).getImm();
      
      if (SubIdx == Z80::sub_low24)
        Lo = SrcReg;
      else if (SubIdx == Z80::sub_mid24)
        Mid = SrcReg;
      else if (SubIdx == Z80::sub_word3)
        Hi = SrcReg;
    }
    
    if (Lo.isValid())
      BuildMI(MBB, InsertPt, DL, TII->get(TargetOpcode::COPY), D.Lo24).addReg(Lo);
    if (Mid.isValid())
      BuildMI(MBB, InsertPt, DL, TII->get(TargetOpcode::COPY), D.Mid24).addReg(Mid);
    if (Hi.isValid())
      BuildMI(MBB, InsertPt, DL, TII->get(TargetOpcode::COPY), D.Hi16).addReg(Hi);
    
    LLVM_DEBUG(dbgs() << "  Decomposed REG_SEQUENCE: " << MI);
    return;
  }
  
  if (Opc == TargetOpcode::INSERT_SUBREG) {
    // INSERT_SUBREG %dst, %src, %insert, subidx
    // the %src is usually an IMPLICIT_DEF or previous R64_24
    // we need to track which component is being set
    
    Register SrcReg = MI.getOperand(1).getReg();
    Register InsertReg = MI.getOperand(2).getReg();
    unsigned SubIdx = MI.getOperand(3).getImm();
    
    // if src is also R64_24, get its decomposition
    DecomposedR64 SrcD;
    if (SrcReg.isVirtual() && 
        MRI->getRegClassOrNull(SrcReg) == &Z80::R64_24RegClass) {
      SrcD = getOrCreateDecomp(SrcReg);
    }
    
    if (SrcD.IsValid) {
      if (SubIdx != Z80::sub_low24)
        BuildMI(MBB, InsertPt, DL, TII->get(TargetOpcode::COPY), D.Lo24)
            .addReg(SrcD.Lo24);
      if (SubIdx != Z80::sub_mid24)
        BuildMI(MBB, InsertPt, DL, TII->get(TargetOpcode::COPY), D.Mid24)
            .addReg(SrcD.Mid24);
      if (SubIdx != Z80::sub_word3)
        BuildMI(MBB, InsertPt, DL, TII->get(TargetOpcode::COPY), D.Hi16)
            .addReg(SrcD.Hi16);
    }
    
    if (SubIdx == Z80::sub_low24)
      BuildMI(MBB, InsertPt, DL, TII->get(TargetOpcode::COPY), D.Lo24)
          .addReg(InsertReg);
    else if (SubIdx == Z80::sub_mid24)
      BuildMI(MBB, InsertPt, DL, TII->get(TargetOpcode::COPY), D.Mid24)
          .addReg(InsertReg);
    else if (SubIdx == Z80::sub_word3)
      BuildMI(MBB, InsertPt, DL, TII->get(TargetOpcode::COPY), D.Hi16)
          .addReg(InsertReg);
    
    LLVM_DEBUG(dbgs() << "  Decomposed INSERT_SUBREG: " << MI);
    return;
  }
  
  // for other defs (COPY, loads, etc.), we need to extract components
  LLVM_DEBUG(dbgs() << "  Unhandled R64_24 def: " << MI);
}

/// rewrite all uses of an R64_24 register to use decomposed components
void Z80R64SpillPass::rewriteR64Uses(Register R64Reg,
                                     SmallVectorImpl<MachineInstr *> &ToErase) {
  DecomposedR64 D = getOrCreateDecomp(R64Reg);
  if (!D.IsValid)
    return;

  SmallVector<MachineInstr *, 16> UsesToRewrite;
  
  for (MachineInstr &UseMI : MRI->use_instructions(R64Reg)) {
    UsesToRewrite.push_back(&UseMI);
  }

  for (MachineInstr *UseMI : UsesToRewrite) {
    unsigned Opc = UseMI->getOpcode();
    MachineBasicBlock &MBB = *UseMI->getParent();
    DebugLoc DL = UseMI->getDebugLoc();
    
    if (Opc == TargetOpcode::EXTRACT_SUBREG) {
      // EXTRACT_SUBREG %dst, %r64, subidx
      // -> COPY %dst, %component (possibly with subidx extraction)
      Register DstReg = UseMI->getOperand(0).getReg();
      unsigned SubIdx = UseMI->getOperand(2).getImm();
      
      Register SrcComp;
      unsigned NestedSubReg = 0;
      
      if (SubIdx == Z80::sub_low24) {
        SrcComp = D.Lo24;
      } else if (SubIdx == Z80::sub_mid24) {
        SrcComp = D.Mid24;
      } else if (SubIdx == Z80::sub_word3) {
        SrcComp = D.Hi16;
      } else if (SubIdx == Z80::sub_short) {
        // sub_short is the low 16 bits of sub_low24
        SrcComp = D.Lo24;
        NestedSubReg = Z80::sub_short;
      } else if (SubIdx == Z80::sub_low) {
        // sub_low is the low 8 bits, extract from Lo24
        SrcComp = D.Lo24;
        NestedSubReg = Z80::sub_low;
      } else if (SubIdx == Z80::sub_high) {
        // sub_high might be bits 8-15
        SrcComp = D.Lo24;
        NestedSubReg = Z80::sub_high;
      } else {
        LLVM_DEBUG(dbgs() << "  Unknown subreg index " << SubIdx 
                          << " in EXTRACT: " << *UseMI);
        continue;
      }
      
      if (SrcComp.isValid()) {
        auto InsertPt = UseMI->getIterator();
        if (NestedSubReg) {
          BuildMI(MBB, InsertPt, DL, TII->get(TargetOpcode::COPY), DstReg)
              .addReg(SrcComp, 0, NestedSubReg);
        } else {
          BuildMI(MBB, InsertPt, DL, TII->get(TargetOpcode::COPY), DstReg)
              .addReg(SrcComp);
        }
        ToErase.push_back(UseMI);
        LLVM_DEBUG(dbgs() << "  Rewrote EXTRACT_SUBREG: " << *UseMI);
      }
      continue;
    }
    
    if (Opc == TargetOpcode::INSERT_SUBREG) {
      // if this INSERT_SUBREG uses R64Reg as source, it's already handled
      // in rewriteR64Def for the destination
      // just mark it for erasure since we have created decomposed copies
      if (UseMI->getOperand(0).getReg() != R64Reg) {
        // R64Reg is being used as the source (operand 1)
        // this is handled when we process the destination R64_24
        continue;
      }
      continue;
    }
    
    // handle other uses with subreg access
    for (MachineOperand &MO : UseMI->operands()) {
      if (!MO.isReg() || MO.getReg() != R64Reg || !MO.isUse())
        continue;
      
      unsigned SubReg = MO.getSubReg();
      if (SubReg == Z80::sub_low24) {
        MO.setReg(D.Lo24);
        MO.setSubReg(0);
      } else if (SubReg == Z80::sub_mid24) {
        MO.setReg(D.Mid24);
        MO.setSubReg(0);
      } else if (SubReg == Z80::sub_word3) {
        MO.setReg(D.Hi16);
        MO.setSubReg(0);
      } else if (SubReg == 0) {
        // whole register use reconstruct R64_24
        // not sure when this would happen
        LLVM_DEBUG(dbgs() << "  WARNING: Whole R64_24 use not yet handled: " << *UseMI);
      }
    }
  }
}

bool Z80R64SpillPass::decomposeR64Registers(MachineFunction &MF) {
  // collect all R64_24 virtual registers and their defs
  SmallVector<std::pair<Register, MachineInstr *>, 16> R64Defs;
  
  for (unsigned I = 0, E = MRI->getNumVirtRegs(); I != E; ++I) {
    Register Reg = Register::index2VirtReg(I);
    const TargetRegisterClass *RC = MRI->getRegClassOrNull(Reg);
    if (RC == &Z80::R64_24RegClass) {
      MachineInstr *DefMI = MRI->getVRegDef(Reg);
      if (DefMI)
        R64Defs.push_back({Reg, DefMI});
    }
  }

  LLVM_DEBUG(dbgs() << "Z80R64SpillPass: Found " << R64Defs.size() 
                    << " R64_24 registers to decompose\n");

  if (R64Defs.empty())
    return false;

  // process defs first, this creates the decomposed component defs
  for (auto &[Reg, DefMI] : R64Defs) {
    rewriteR64Def(*DefMI, Reg);
  }

  // then rewrite uses and collect instructions to erase
  SmallVector<MachineInstr *, 32> ToErase;
  for (auto &[Reg, DefMI] : R64Defs) {
    rewriteR64Uses(Reg, ToErase);
  }

  // erase rewritten EXTRACT_SUBREG instructions
  for (MachineInstr *MI : ToErase) {
    MI->eraseFromParent();
  }

  // erase original R64_24 defs
  for (auto &[Reg, DefMI] : R64Defs) {
    if (MRI->use_empty(Reg)) {
      LLVM_DEBUG(dbgs() << "  Removing dead R64_24 def: " << *DefMI);
      DefMI->eraseFromParent();
    } else {
      LLVM_DEBUG(dbgs() << "  WARNING: R64_24 still has uses: " << *DefMI);
      for (MachineInstr &Use : MRI->use_instructions(Reg)) {
        LLVM_DEBUG(dbgs() << "    Used by: " << Use);
      }
    }
  }

  return true;
}

bool Z80R64SpillPass::runOnMachineFunction(MachineFunction &MF) {
  STI = &MF.getSubtarget<Z80Subtarget>();

  if (!STI->is24Bit())
    return false;

  TII = STI->getInstrInfo();
  TRI = STI->getRegisterInfo();
  MRI = &MF.getRegInfo();
  
  DecompMap.clear();

  return decomposeR64Registers(MF);
}

FunctionPass *llvm::createZ80R64SpillPass() {
  return new Z80R64SpillPass();
}
