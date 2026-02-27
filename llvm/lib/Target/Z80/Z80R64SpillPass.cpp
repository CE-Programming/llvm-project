//===-- Z80R64SpillPass.cpp - Wide Register Decomposition Pass ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass decomposes wide pseudo register classes (R64_24 and R32_24) into
// their component registers to prevent register allocation failures.
//
// R64_24 Decomposition:
// - R64_24 is decomposed into (r24, r24, r16) components
// - The Z80's R64_24 register class has very few valid tuples (5-8)
// - When multiple R64_24 values need to be allocated simultaneously, the
//   allocator runs out
//
// R32_24 Decomposition:
// - R32_24 is decomposed into (r24, r8) components
// - When operations require the 24-bit accumulator (A24 = UHL), the R32_24
//   gets constrained to r32_24_with_sub_low24_in_a24, which has only ONE
//   valid allocation (using UHL)
// - Long-lived R32_24 values can block all other accumulator-needing code
//
// The pass:
// 1. Finds all R64_24 and R32_24 virtual registers
// 2. Creates replacement component registers
// 3. Rewrites REG_SEQUENCE/INSERT_SUBREG defs to populate components
// 4. Rewrites EXTRACT_SUBREG uses to read from the appropriate component
// 5. Erases the original definitions once all uses are rewritten
//
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

/// tracks the decomposed components of an R32_24 register
struct DecomposedR32 {
  Register Lo24;   // sub_low24 component
  Register Hi8;    // sub_high8 component
  bool IsValid = false;
};

class Z80R64SpillPass : public MachineFunctionPass {
public:
  static char ID;
  Z80R64SpillPass() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override {
    return "Z80 Wide Register Decomposition Pass";
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  const Z80Subtarget *STI = nullptr;
  const Z80InstrInfo *TII = nullptr;
  const Z80RegisterInfo *TRI = nullptr;
  MachineRegisterInfo *MRI = nullptr;

  /// from original R64_24 vreg to its decomposed components
  DenseMap<Register, DecomposedR64> DecompMap64;
  
  /// from original R32_24 vreg to its decomposed components
  DenseMap<Register, DecomposedR32> DecompMap32;

  // R64_24 decomposition methods
  bool decomposeR64Registers(MachineFunction &MF);
  void rewriteR64Def(MachineInstr &MI, Register R64Reg);
  void rewriteR64Uses(Register R64Reg, SmallVectorImpl<MachineInstr *> &ToErase);
  DecomposedR64 getOrCreateDecomp64(Register R64Reg);
  
  // R32_24 decomposition methods
  bool decomposeR32Registers(MachineFunction &MF);
  void rewriteR32Def(MachineInstr &MI, Register R32Reg);
  void rewriteR32Uses(Register R32Reg, SmallVectorImpl<MachineInstr *> &ToErase);
  DecomposedR32 getOrCreateDecomp32(Register R32Reg);
};

}

char Z80R64SpillPass::ID = 0;

INITIALIZE_PASS(Z80R64SpillPass, DEBUG_TYPE,
                "Decompose wide registers into components", false, false)

/// get or create decomposed component registers for an R64_24 register
DecomposedR64 Z80R64SpillPass::getOrCreateDecomp64(Register R64Reg) {
  auto It = DecompMap64.find(R64Reg);
  if (It != DecompMap64.end())
    return It->second;

  DecomposedR64 D;
  D.Lo24 = MRI->createVirtualRegister(&Z80::R24RegClass);
  D.Mid24 = MRI->createVirtualRegister(&Z80::R24RegClass);
  D.Hi16 = MRI->createVirtualRegister(&Z80::R16RegClass);
  D.IsValid = true;
  
  DecompMap64[R64Reg] = D;
  
  LLVM_DEBUG(dbgs() << "Decomposing R64 " << printReg(R64Reg, TRI) << " into "
                    << printReg(D.Lo24, TRI) << ", "
                    << printReg(D.Mid24, TRI) << ", "
                    << printReg(D.Hi16, TRI) << "\n");
  
  return D;
}

/// get or create decomposed component registers for an R32_24 register
DecomposedR32 Z80R64SpillPass::getOrCreateDecomp32(Register R32Reg) {
  auto It = DecompMap32.find(R32Reg);
  if (It != DecompMap32.end())
    return It->second;

  DecomposedR32 D;
  D.Lo24 = MRI->createVirtualRegister(&Z80::R24RegClass);
  D.Hi8 = MRI->createVirtualRegister(&Z80::R8RegClass);
  D.IsValid = true;
  
  DecompMap32[R32Reg] = D;
  
  LLVM_DEBUG(dbgs() << "Decomposing R32 " << printReg(R32Reg, TRI) << " into "
                    << printReg(D.Lo24, TRI) << ", "
                    << printReg(D.Hi8, TRI) << "\n");
  
  return D;
}

/// rewrite the definition of an R64_24 register
/// handles REG_SEQUENCE, INSERT_SUBREG, and IMPLICIT_DEF patterns
void Z80R64SpillPass::rewriteR64Def(MachineInstr &MI, Register R64Reg) {
  MachineBasicBlock &MBB = *MI.getParent();
  DebugLoc DL = MI.getDebugLoc();
  DecomposedR64 D = getOrCreateDecomp64(R64Reg);

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
      SrcD = getOrCreateDecomp64(SrcReg);
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

  if (Opc == TargetOpcode::COPY) {
    // COPY from another R64_24
    Register SrcReg = MI.getOperand(1).getReg();
    if (SrcReg.isVirtual() &&
        MRI->getRegClassOrNull(SrcReg) == &Z80::R64_24RegClass) {
      DecomposedR64 SrcD = getOrCreateDecomp64(SrcReg);
      BuildMI(MBB, InsertPt, DL, TII->get(TargetOpcode::COPY), D.Lo24)
          .addReg(SrcD.Lo24);
      BuildMI(MBB, InsertPt, DL, TII->get(TargetOpcode::COPY), D.Mid24)
          .addReg(SrcD.Mid24);
      BuildMI(MBB, InsertPt, DL, TII->get(TargetOpcode::COPY), D.Hi16)
          .addReg(SrcD.Hi16);
      LLVM_DEBUG(dbgs() << "  Decomposed R64 COPY: " << MI);
      return;
    }
  }

  // generic fallback for defs that write only a subregister of the R64_24
  // aggregate. rebind the def directly to the decomposed component to 
  // avoid forcing allocation of the full tuple
  bool RewroteSubregDef = false;
  for (MachineOperand &MO : MI.operands()) {
    if (!MO.isReg() || !MO.isDef() || MO.getReg() != R64Reg)
      continue;

    unsigned SubReg = MO.getSubReg();
    if (SubReg == Z80::sub_low24) {
      MO.setReg(D.Lo24);
      MO.setSubReg(0);
      RewroteSubregDef = true;
    } else if (SubReg == Z80::sub_mid24) {
      MO.setReg(D.Mid24);
      MO.setSubReg(0);
      RewroteSubregDef = true;
    } else if (SubReg == Z80::sub_word3) {
      MO.setReg(D.Hi16);
      MO.setSubReg(0);
      RewroteSubregDef = true;
    }
  }
  if (RewroteSubregDef) {
    LLVM_DEBUG(dbgs() << "  Rebound R64 subreg def to components: " << MI);
    return;
  }
  
  // for other defs (COPY, loads, etc.), we need to extract components
  LLVM_DEBUG(dbgs() << "  Unhandled R64_24 def: " << MI);
}

/// rewrite all uses of an R64_24 register to use decomposed components
void Z80R64SpillPass::rewriteR64Uses(Register R64Reg,
                                     SmallVectorImpl<MachineInstr *> &ToErase) {
  DecomposedR64 D = getOrCreateDecomp64(R64Reg);
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
      bool StillDefinesReg = false;
      for (const MachineOperand &MO : DefMI->operands()) {
        if (!MO.isReg() || !MO.isDef())
          continue;
        if (MO.getReg() == Reg) {
          StillDefinesReg = true;
          break;
        }
      }
      if (StillDefinesReg) {
        LLVM_DEBUG(dbgs() << "  Removing dead R64_24 def: " << *DefMI);
        DefMI->eraseFromParent();
      }
    } else {
      LLVM_DEBUG(dbgs() << "  WARNING: R64_24 still has uses: " << *DefMI);
      for (MachineInstr &Use : MRI->use_instructions(Reg)) {
        LLVM_DEBUG(dbgs() << "    Used by: " << Use);
      }
    }
  }

  return true;
}

//===----------------------------------------------------------------------===//
// R32_24 Decomposition
//===----------------------------------------------------------------------===//

/// rewrite the definition of an R32_24 register
void Z80R64SpillPass::rewriteR32Def(MachineInstr &MI, Register R32Reg) {
  MachineBasicBlock &MBB = *MI.getParent();
  DebugLoc DL = MI.getDebugLoc();
  DecomposedR32 D = getOrCreateDecomp32(R32Reg);

  unsigned Opc = MI.getOpcode();
  auto InsertPt = std::next(MI.getIterator());
  
  if (Opc == TargetOpcode::IMPLICIT_DEF) {
    // IMPLICIT_DEF -> create IMPLICIT_DEF for each component
    BuildMI(MBB, InsertPt, DL, TII->get(TargetOpcode::IMPLICIT_DEF), D.Lo24);
    BuildMI(MBB, InsertPt, DL, TII->get(TargetOpcode::IMPLICIT_DEF), D.Hi8);
    
    LLVM_DEBUG(dbgs() << "  Decomposed R32 IMPLICIT_DEF: " << MI);
    return;
  }
  
  if (Opc == TargetOpcode::REG_SEQUENCE) {
    // REG_SEQUENCE %dst, %lo, sub_low24, %hi, sub_high8
    // -> COPY each component to decomposed regs
    
    Register Lo = Register(), Hi = Register();
    
    for (unsigned I = 1, E = MI.getNumOperands(); I < E; I += 2) {
      if (I + 1 >= E) break;
      Register SrcReg = MI.getOperand(I).getReg();
      unsigned SubIdx = MI.getOperand(I + 1).getImm();
      
      if (SubIdx == Z80::sub_low24)
        Lo = SrcReg;
      else if (SubIdx == Z80::sub_high8)
        Hi = SrcReg;
    }
    
    if (Lo.isValid())
      BuildMI(MBB, InsertPt, DL, TII->get(TargetOpcode::COPY), D.Lo24).addReg(Lo);
    if (Hi.isValid())
      BuildMI(MBB, InsertPt, DL, TII->get(TargetOpcode::COPY), D.Hi8).addReg(Hi);
    
    LLVM_DEBUG(dbgs() << "  Decomposed R32 REG_SEQUENCE: " << MI);
    return;
  }
  
  if (Opc == TargetOpcode::INSERT_SUBREG) {
    // INSERT_SUBREG %dst, %src, %insert, subidx
    // the %src is usually an IMPLICIT_DEF or previous R32_24
    // we need to track which component is being set
    
    Register SrcReg = MI.getOperand(1).getReg();
    Register InsertReg = MI.getOperand(2).getReg();
    unsigned SubIdx = MI.getOperand(3).getImm();
    
    DecomposedR32 SrcD;
    if (SrcReg.isVirtual() && 
        MRI->getRegClassOrNull(SrcReg) == &Z80::R32_24RegClass) {
      SrcD = getOrCreateDecomp32(SrcReg);
    }
    
    if (SrcD.IsValid) {
      if (SubIdx != Z80::sub_low24)
        BuildMI(MBB, InsertPt, DL, TII->get(TargetOpcode::COPY), D.Lo24)
            .addReg(SrcD.Lo24);
      if (SubIdx != Z80::sub_high8)
        BuildMI(MBB, InsertPt, DL, TII->get(TargetOpcode::COPY), D.Hi8)
            .addReg(SrcD.Hi8);
    }
    
    if (SubIdx == Z80::sub_low24)
      BuildMI(MBB, InsertPt, DL, TII->get(TargetOpcode::COPY), D.Lo24)
          .addReg(InsertReg);
    else if (SubIdx == Z80::sub_high8)
      BuildMI(MBB, InsertPt, DL, TII->get(TargetOpcode::COPY), D.Hi8)
          .addReg(InsertReg);
    
    LLVM_DEBUG(dbgs() << "  Decomposed R32 INSERT_SUBREG: " << MI);
    return;
  }
  
  if (Opc == TargetOpcode::COPY) {
    // COPY from another R32_24
    Register SrcReg = MI.getOperand(1).getReg();
    if (SrcReg.isVirtual() && 
        MRI->getRegClassOrNull(SrcReg) == &Z80::R32_24RegClass) {
      DecomposedR32 SrcD = getOrCreateDecomp32(SrcReg);
      BuildMI(MBB, InsertPt, DL, TII->get(TargetOpcode::COPY), D.Lo24)
          .addReg(SrcD.Lo24);
      BuildMI(MBB, InsertPt, DL, TII->get(TargetOpcode::COPY), D.Hi8)
          .addReg(SrcD.Hi8);
      LLVM_DEBUG(dbgs() << "  Decomposed R32 COPY: " << MI);
      return;
    }
  }
  
  if (Opc == TargetOpcode::PHI) {
    // this is complicated, just warn for now
    LLVM_DEBUG(dbgs() << "  WARNING: R32 PHI decomposition not yet handled: " << MI);
    return;
  }
  
  // for other defs (loads, etc.) components need extraction
  LLVM_DEBUG(dbgs() << "  Unhandled R32_24 def: " << MI);
}

/// rewrite all uses of an R32_24 register to use decomposed components
void Z80R64SpillPass::rewriteR32Uses(Register R32Reg,
                                     SmallVectorImpl<MachineInstr *> &ToErase) {
  DecomposedR32 D = getOrCreateDecomp32(R32Reg);
  if (!D.IsValid)
    return;

  SmallVector<MachineInstr *, 16> UsesToRewrite;
  
  for (MachineInstr &UseMI : MRI->use_instructions(R32Reg)) {
    UsesToRewrite.push_back(&UseMI);
  }

  for (MachineInstr *UseMI : UsesToRewrite) {
    unsigned Opc = UseMI->getOpcode();
    MachineBasicBlock &MBB = *UseMI->getParent();
    DebugLoc DL = UseMI->getDebugLoc();
    
    if (Opc == TargetOpcode::EXTRACT_SUBREG) {
      // EXTRACT_SUBREG %dst, %r32, subidx
      // -> COPY %dst, %component (possibly with nested subreg)
      Register DstReg = UseMI->getOperand(0).getReg();
      unsigned SubIdx = UseMI->getOperand(2).getImm();
      
      Register SrcComp;
      unsigned NestedSubReg = 0;
      
      if (SubIdx == Z80::sub_low24) {
        SrcComp = D.Lo24;
      } else if (SubIdx == Z80::sub_high8) {
        SrcComp = D.Hi8;
      } else if (SubIdx == Z80::sub_short) {
        // sub_short is the low 16 bits of sub_low24
        SrcComp = D.Lo24;
        NestedSubReg = Z80::sub_short;
      } else if (SubIdx == Z80::sub_low) {
        // sub_low is the low 8 bits, extract from Lo24
        SrcComp = D.Lo24;
        NestedSubReg = Z80::sub_low;
      } else if (SubIdx == Z80::sub_high) {
        // sub_high might be bits 8-15, extract from Lo24
        SrcComp = D.Lo24;
        NestedSubReg = Z80::sub_high;
      } else {
        LLVM_DEBUG(dbgs() << "  Unknown subreg index " << SubIdx 
                          << " in R32 EXTRACT: " << *UseMI);
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
        LLVM_DEBUG(dbgs() << "  Rewrote R32 EXTRACT_SUBREG: " << *UseMI);
      }
      continue;
    }
    
    if (Opc == TargetOpcode::INSERT_SUBREG) {
      // if this INSERT_SUBREG uses R32Reg as source, its already handled
      // in rewriteR32Def for the destination
      if (UseMI->getOperand(0).getReg() != R32Reg) {
        // R32Reg is being used as the source (operand 1)
        // this is handled when we process the destination R32_24
        continue;
      }
      continue;
    }
    
    // Handle COPY with subreg
    if (Opc == TargetOpcode::COPY) {
      MachineOperand &SrcMO = UseMI->getOperand(1);
      if (SrcMO.getReg() == R32Reg) {
        unsigned SubReg = SrcMO.getSubReg();
        if (SubReg == Z80::sub_low24) {
          SrcMO.setReg(D.Lo24);
          SrcMO.setSubReg(0);
          LLVM_DEBUG(dbgs() << "  Rewrote R32 COPY sub_low24: " << *UseMI);
        } else if (SubReg == Z80::sub_high8) {
          SrcMO.setReg(D.Hi8);
          SrcMO.setSubReg(0);
          LLVM_DEBUG(dbgs() << "  Rewrote R32 COPY sub_high8: " << *UseMI);
        } else if (SubReg == 0) {
          // register COPY to another R32_24, let def handle it
        }
        continue;
      }
    }
    
    for (MachineOperand &MO : UseMI->operands()) {
      if (!MO.isReg() || MO.getReg() != R32Reg || !MO.isUse())
        continue;
      
      unsigned SubReg = MO.getSubReg();
      if (SubReg == Z80::sub_low24) {
        MO.setReg(D.Lo24);
        MO.setSubReg(0);
      } else if (SubReg == Z80::sub_high8) {
        MO.setReg(D.Hi8);
        MO.setSubReg(0);
      } else if (SubReg == 0) {
        // whole register use, may need reconstruction
        LLVM_DEBUG(dbgs() << "  WARNING: Whole R32_24 use not yet handled: " << *UseMI);
      }
    }
  }
}

bool Z80R64SpillPass::decomposeR32Registers(MachineFunction &MF) {
  SmallVector<std::pair<Register, MachineInstr *>, 16> R32Defs;
  
  for (unsigned I = 0, E = MRI->getNumVirtRegs(); I != E; ++I) {
    Register Reg = Register::index2VirtReg(I);
    const TargetRegisterClass *RC = MRI->getRegClassOrNull(Reg);
    if (RC == &Z80::R32_24RegClass) {
      MachineInstr *DefMI = MRI->getVRegDef(Reg);
      if (DefMI)
        R32Defs.push_back({Reg, DefMI});
    }
  }

  LLVM_DEBUG(dbgs() << "Z80R64SpillPass: Found " << R32Defs.size() 
                    << " R32_24 registers to decompose\n");

  if (R32Defs.empty())
    return false;

  // process defs first
  for (auto &[Reg, DefMI] : R32Defs) {
    rewriteR32Def(*DefMI, Reg);
  }

  // then rewrite uses and collect instructions to erase
  SmallVector<MachineInstr *, 32> ToErase;
  for (auto &[Reg, DefMI] : R32Defs) {
    rewriteR32Uses(Reg, ToErase);
  }

  // erase rewritten EXTRACT_SUBREG instructions
  for (MachineInstr *MI : ToErase) {
    MI->eraseFromParent();
  }

  // erase original R32_24 defs
  for (auto &[Reg, DefMI] : R32Defs) {
    if (MRI->use_empty(Reg)) {
      LLVM_DEBUG(dbgs() << "  Removing dead R32_24 def: " << *DefMI);
      DefMI->eraseFromParent();
    } else {
      LLVM_DEBUG(dbgs() << "  WARNING: R32_24 still has uses: " << *DefMI);
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
  
  DecompMap64.clear();
  DecompMap32.clear();

  bool Changed = false;
  Changed |= decomposeR64Registers(MF);
  Changed |= decomposeR32Registers(MF);
  return Changed;
}

FunctionPass *llvm::createZ80R64SpillPass() {
  return new Z80R64SpillPass();
}
