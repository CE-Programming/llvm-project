//=== lib/CodeGen/GlobalISel/Z80MachineLateOptimization.cpp ---------------===//
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
#include "llvm/CodeGen/LiveRegUnits.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "z80-machine-late-opt"

using namespace llvm;

namespace {
class ImmVal {
  static constexpr unsigned UnknownOff = ~0u;
  const GlobalValue *GV = nullptr;
  unsigned Off = UnknownOff, Mask = 0;
  MachineInstr *KilledBy = nullptr;

public:
  ImmVal() {}
  ImmVal(MCRegister Reg, const TargetRegisterInfo &TRI) {
    assert(Register::isPhysicalRegister(Reg) && "Expected physical register");
    if (auto *RC = TRI.getMinimalPhysRegClass(Reg))
      Mask = maskTrailingOnes<decltype(Mask)>(TRI.getRegSizeInBits(*RC));
  }
  ImmVal(MachineOperand &MO, MCRegister Reg, const TargetRegisterInfo &TRI)
      : ImmVal(Reg, TRI) {
    switch (MO.getType()) {
    case MachineOperand::MO_Immediate:
      Off = MO.getImm();
      break;
    case MachineOperand::MO_CImmediate:
      Off = MO.getCImm()->getZExtValue();
      break;
    case MachineOperand::MO_GlobalAddress:
      GV = MO.getGlobal();
      Off = MO.getOffset();
      break;
    default:
      return;
    }
    Off &= Mask;
    assert(valid() && "Mask should have been less than 32 bits");
  }
  ImmVal(unsigned Imm, MCRegister Reg, const TargetRegisterInfo &TRI)
      : ImmVal(Reg, TRI) {
    Off = Imm & Mask;
    assert(valid() && "Mask should have been less than 32 bits");
  }
  ImmVal(const ImmVal &SuperVal, unsigned Idx, const TargetRegisterInfo &TRI) {
    Mask = maskTrailingOnes<decltype(Mask)>(TRI.getSubRegIdxSize(Idx));
    if (!SuperVal.isImm())
      return;
    Off = SuperVal.Off >> TRI.getSubRegIdxOffset(Idx) & Mask;
    assert(valid() && "Mask should have been less than 32 bits");
  }

  void clobber() {
    Off = UnknownOff;
    KilledBy = nullptr;
  }

  bool valid() const {
    return Off != UnknownOff;
  }
  bool isImm() const {
    return valid() && !GV;
  }
  bool isGlobal() const {
    return valid() && GV;
  }

  bool match(ImmVal &Val, int Delta = 0) const {
    return valid() && Val.valid() && GV == Val.GV && Mask == Val.Mask &&
           Off == ((Val.Off + Delta) & Mask);
  }

  void setKilledBy(MachineInstr *MI) {
    KilledBy = MI;
  }

  void reuse(Register Reg, const TargetRegisterInfo &TRI) {
    if (KilledBy) {
      assert(KilledBy->killsRegister(Reg, &TRI) &&
             "KilledBy should kill register");
      KilledBy->clearRegisterKills(Reg, &TRI);
      KilledBy = nullptr;
    }
  }

#ifndef NDEBUG
  friend raw_ostream &operator<<(raw_ostream &OS, ImmVal &Val) {
    if (!Val.valid())
      return OS << "?";
    if (Val.GV)
      OS << Val.GV << '+';
    return OS << Val.Off;
  }
#endif
};

class Z80MachineLateOptimization : public MachineFunctionPass {
  const TargetRegisterInfo *TRI;
  ImmVal Vals[Z80::NUM_TARGET_REGS];

  void clobberAll() {
    for (unsigned Reg = 1; Reg != Z80::NUM_TARGET_REGS; ++Reg)
      Vals[Reg] = ImmVal(Reg, *TRI);
  }
  template<typename MCRegIterator>
  void clobber(Register Reg, bool IncludeSelf) {
    for (MCRegIterator I(Reg, TRI, IncludeSelf); I.isValid(); ++I)
      Vals[*I] = ImmVal(*I, *TRI);
  }

  bool tryReplaceReg(MachineRegisterInfo &MRI, MCRegister FromReg,
                     MCRegister ToReg);
  void debug(const MachineInstr &MI);

public:
  static char ID;

  Z80MachineLateOptimization() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override {
    return "Z80 Machine Late Optimization";
  }

  bool runOnMachineFunction(MachineFunction &MF) override;
};
} // end anonymous namespace

bool Z80MachineLateOptimization::runOnMachineFunction(MachineFunction &MF) {
  bool Changed = false;
  TRI = MF.getSubtarget().getRegisterInfo();
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  LiveRegUnits LiveUnits(*TRI);
  for (MachineBasicBlock &MBB : MF) {
    LiveUnits.clear();
    LiveUnits.addLiveIns(MBB);
    clobberAll();
    for (MachineBasicBlock::iterator I = MBB.begin(), E = MBB.end(); I != E;) {
      MachineInstr &MI = *I;
      ++I;
      LiveUnits.stepForward(MI);
      switch (unsigned Opc = MI.getOpcode()) {
      case Z80::LD8ri:
      case Z80::LD16ri:
      case Z80::LD24ri:
      case Z80::LD24r0:
      case Z80::LD24r_1: {
        Register Reg = MI.getOperand(0).getReg();
        ImmVal Val;
        switch (Opc) {
        default:
          Val = ImmVal(MI.getOperand(1), Reg, *TRI);
          break;
        case Z80::LD24r0:
          Val = ImmVal(0, Reg, *TRI);
          break;
        case Z80::LD24r_1:
          Val = ImmVal(-1, Reg, *TRI);
          break;
        }
        if (Val.match(Vals[Reg])) {
          LLVM_DEBUG(dbgs() << "Erasing redundant: "; MI.dump());
          MI.eraseFromParent();
          Vals[Reg].reuse(Reg, *TRI);
          Changed = true;
          continue;
        }
        if (Val.match(Vals[Reg], 1))
          switch (Opc) {
          case Z80::LD8ri:
            if (!LiveUnits.available(Z80::F))
              break;
            Opc = Z80::INC8r;
            break;
          case Z80::LD16ri:
            Opc = Z80::INC16r;
            break;
          case Z80::LD24ri:
          case Z80::LD24r0:
          case Z80::LD24r_1:
            Opc = Z80::INC24r;
            break;
          }
        else if (Val.match(Vals[Reg], -1))
          switch (Opc) {
          case Z80::LD8ri:
            if (!LiveUnits.available(Z80::F))
              break;
            Opc = Z80::DEC8r;
            break;
          case Z80::LD16ri:
            Opc = Z80::DEC16r;
            break;
          case Z80::LD24ri:
          case Z80::LD24r0:
          case Z80::LD24r_1:
            Opc = Z80::DEC24r;
            break;
          }
        if (Opc != MI.getOpcode()) {
          LLVM_DEBUG(dbgs() << "Replacing: "; MI.dump(););
          MI.setDesc(TII.get(Opc));
          MI.RemoveOperand(1);
          MI.addImplicitDefUseOperands(MF);
          Vals[Reg].reuse(Reg, *TRI);
          LLVM_DEBUG(dbgs() << "With: "; MI.dump(););
        }
        clobber<MCSuperRegIterator>(Reg, false);
        Vals[Reg] = Val;
        for (MCSubRegIndexIterator SRII(Reg, TRI); SRII.isValid(); ++SRII)
          Vals[SRII.getSubReg()] = ImmVal(Val, SRII.getSubRegIndex(), *TRI);
        debug(MI);
        continue;
      }
      case Z80::RLC8r:
      case Z80::RRC8r:
      case Z80::RL8r:
      case Z80::RR8r: {
        if (MI.getOperand(0).getReg() != Z80::A || !LiveUnits.available(Z80::F))
          break;
        switch (Opc) {
        case Z80::RLC8r: Opc = Z80::RLCA; break;
        case Z80::RRC8r: Opc = Z80::RRCA; break;
        case Z80::RL8r:  Opc = Z80::RLA;  break;
        case Z80::RR8r:  Opc = Z80::RRA;  break;
        }
        LLVM_DEBUG(dbgs() << "Replacing: "; MI.dump(); dbgs() << "     With: ");
        MI.setDesc(TII.get(Opc));
        MI.getOperand(0).setImplicit();
        MI.getOperand(1).setImplicit();
        break;
      }
      }
      for (MachineOperand &MO : MI.operands())
        if (MO.isReg() && MO.isKill() && MO.getReg().isPhysical())
            for (MCRegAliasIterator I(MO.getReg(), TRI, true); I.isValid(); ++I)
              Vals[*I].setKilledBy(&MI);
      for (MachineOperand &MO : MI.operands()) {
        if (MO.isReg() && MO.isDef() && MO.getReg().isPhysical() &&
            !(MI.isCopy() && MO.isImplicit()))
          clobber<MCRegAliasIterator>(MO.getReg(), true);
        else if (MO.isRegMask())
          for (unsigned Reg = 1; Reg != Z80::NUM_TARGET_REGS; ++Reg)
            if (MO.clobbersPhysReg(Reg))
              Vals[Reg] = ImmVal(Reg, *TRI);
      }
      debug(MI);
    }
  }
  return Changed;
}

void Z80MachineLateOptimization::debug(const MachineInstr &MI) {
  for (unsigned Reg = 1; Reg != Z80::NUM_TARGET_REGS; ++Reg)
    if (Vals[Reg].valid())
      LLVM_DEBUG(dbgs() << TRI->getName(Reg) << " = " << Vals[Reg] << ' ');
  LLVM_DEBUG(MI.dump());
}

char Z80MachineLateOptimization::ID = 0;
INITIALIZE_PASS(Z80MachineLateOptimization, DEBUG_TYPE,
                "Optimize Z80 machine instrs after regselect", false, false)

namespace llvm {
FunctionPass *createZ80MachineLateOptimizationPass() {
  return new Z80MachineLateOptimization();
}
} // end namespace llvm
