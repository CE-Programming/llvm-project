//=== lib/Target/Z80/Z80MachineLateOptimization.cpp -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass does combining of machine instructions at the generic MI level,
// after register allocation.
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
class RegVal {
  static constexpr int UnknownOff = ~0;
  const GlobalValue *GV = nullptr;
  int Off = UnknownOff, Mask = 0;
  MachineInstr *KilledBy = nullptr;

public:
  RegVal() {}
  RegVal(MCRegister Reg, const TargetRegisterInfo &TRI) {
    assert(Register::isPhysicalRegister(Reg) && "Expected physical register");
    if (auto *RC = TRI.getMinimalPhysRegClass(Reg))
      Mask = maskTrailingOnes<unsigned>(TRI.getRegSizeInBits(*RC));
  }
  RegVal(MachineOperand &MO, MCRegister Reg, const TargetRegisterInfo &TRI)
      : RegVal(Reg, TRI) {
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
  RegVal(int Imm, MCRegister Reg, const TargetRegisterInfo &TRI)
      : RegVal(Reg, TRI) {
    Off = Imm & Mask;
    assert(valid() && "Mask should have been less than 32 bits");
  }
  RegVal(int Imm, int KnownMask, MCRegister Reg, const TargetRegisterInfo &TRI)
      : RegVal{Imm, Reg, TRI} {
    Mask &= KnownMask;
  }
  RegVal(const RegVal &SuperVal, unsigned Idx, MCRegister Reg,
         const TargetRegisterInfo &TRI) {
    Mask = maskTrailingOnes<unsigned>(TRI.getSubRegIdxSize(Idx));
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

  bool matches(RegVal &Val, int Delta = 0) const {
    return valid() && Val.valid() && GV == Val.GV && Mask == Val.Mask &&
           Off == ((Val.Off + Delta) & Mask);
  }
  std::pair<int, int> getKnownBits(int KnownMask = ~0) const {
    if (!isImm())
      return {0, 0};
    return {Off & KnownMask, Mask & KnownMask};
  }

  void setKilledBy(MachineInstr *MI) {
    KilledBy = MI;
  }
  MachineInstr *takeKilledBy() {
    return std::exchange(KilledBy, nullptr);
  }

#ifndef NDEBUG
  friend raw_ostream &operator<<(raw_ostream &OS, RegVal &Val) {
    OS << " & " << format_hex(Val.Mask, 4, true) << " == ";
    if (!Val.valid())
      return OS << "?";
    if (Val.GV)
      OS << Val.GV << '+';
    return OS << format_hex(Val.Off, 4, true);
  }
#endif
};

class Z80MachineLateOptimization : public MachineFunctionPass {
  enum {
    CarryFlag = 1 << 0,
    SubtractFlag = 1 << 1,
    ParityOverflowFlag = 1 << 2,
    HalfCarryFlag = 1 << 4,
    ZeroFlag = 1 << 6,
    SignFlag = 1 << 7,
  };

  const TargetRegisterInfo *TRI;
  RegVal RegVals[Z80::NUM_TARGET_REGS];

  template <typename... Args> void assign(MCRegister Reg, Args &&...args) {
    RegVals[Reg] = RegVal(std::forward<Args>(args)..., Reg, *TRI);
  }
  void updateFlags(int ResetMask, int SetMask, int PreserveMask) {
    int KnownVal, KnownMask;
    std::tie(KnownVal, KnownMask) = RegVals[Z80::F].getKnownBits(PreserveMask);
    assign(Z80::F, SetMask | KnownVal, ResetMask | SetMask | KnownMask);
  }
  void clobberAll() {
    for (unsigned Reg = 1; Reg != Z80::NUM_TARGET_REGS; ++Reg)
      assign(Reg);
  }
  template<typename MCRegIterator>
  void clobber(MCRegister Reg, bool IncludeSelf) {
    for (MCRegIterator I(Reg, TRI, IncludeSelf); I.isValid(); ++I)
      assign(*I);
  }
  bool reuse(MCRegister Reg) {
    bool WasKilled = false;
    if (MachineInstr *KilledBy = RegVals[Reg].takeKilledBy()) {
      WasKilled = KilledBy->killsRegister(Reg, TRI);
      KilledBy->clearRegisterKills(Reg, TRI);
    }
    return WasKilled;
  }

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
      RegVal Val;
      Register Reg;
      int KnownFlagVal, KnownFlagMask;
      std::tie(KnownFlagVal, KnownFlagMask) = RegVals[Z80::F].getKnownBits();
      switch (unsigned Opc = MI.getOpcode()) {
      case Z80::LD8ri:
      case Z80::LD16ri:
      case Z80::LD24ri:
      case Z80::LD24r0:
      case Z80::LD24r_1:
        Reg = MI.getOperand(0).getReg();
        switch (Opc) {
        default:
          Val = {MI.getOperand(1), Reg, *TRI};
          break;
        case Z80::LD24r0:
          Val = {0, Reg, *TRI};
          break;
        case Z80::LD24r_1:
          Val = {-1, Reg, *TRI};
          break;
        }
        if (Val.matches(RegVals[Reg], 1))
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
        else if (Val.matches(RegVals[Reg], -1))
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
          LLVM_DEBUG(dbgs() << "Replacing: "; MI.dump();
                     dbgs() << "     With: ");
          MI.setDesc(TII.get(Opc));
          MI.RemoveOperand(1);
          MI.addOperand(MachineOperand::CreateReg(Reg, /*isDef=*/false,
                                                  /*isImp=*/false,
                                                  /*isKill=*/reuse(Reg)));
          MI.addImplicitDefUseOperands(MF);
          break;
        }
        switch (Opc) {
        case Z80::LD24r0:
        case Z80::LD24r_1:
          if (Reg != Z80::UHL || !(KnownFlagMask & CarryFlag) ||
              ((Opc == Z80::LD24r0) ^ !(KnownFlagVal & CarryFlag)))
            break;
          LLVM_DEBUG(dbgs() << "Replacing: "; MI.dump();
                     dbgs() << "     With: ");
          MI.setDesc(TII.get(Z80::SBC24aa));
          MI.getOperand(0).setImplicit();
          MI.getOperand(1).setImplicit();
          MI.addOperand(MachineOperand::CreateReg(
              Reg, /*isDef=*/false, /*isImp=*/true, /*isKill=*/false,
              /*isDead=*/false, /*isUndef=*/true));
          MI.addOperand(MachineOperand::CreateReg(Z80::F, /*isDef=*/false,
                                                  /*isImp=*/true,
                                                  /*isKill=*/reuse(Z80::F)));
          break;
        }
        break;
      case Z80::RCF:
      case Z80::SCF:
        Reg = Z80::F;
        if (!(KnownFlagMask & CarryFlag) ||
            (Opc == Z80::RCF) ^ !(KnownFlagVal & CarryFlag))
          break;
        Val = {KnownFlagVal, KnownFlagMask, Reg, *TRI};
        break;
      case Z80::RLC8r:
      case Z80::RRC8r:
      case Z80::RL8r:
      case Z80::RR8r:
        Reg = MI.getOperand(0).getReg();
        if (Reg != Z80::A || !LiveUnits.available(Z80::F))
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
      case Z80::SUB8ar:
      case Z80::XOR8ar:
        Reg = MI.getOperand(0).getReg();
        if (Reg != Z80::A)
          break;
        Val = {0, Reg, *TRI};
        break;
      }
      if (Val.matches(RegVals[Reg])) {
        LLVM_DEBUG(dbgs() << "Erasing redundant: "; MI.dump());
        MI.eraseFromParent();
        reuse(Reg);
        Changed = true;
        continue;
      }
      for (MachineOperand &MO : MI.operands())
        if (MO.isReg() && MO.isKill() && MO.getReg().isPhysical())
            for (MCRegAliasIterator I(MO.getReg(), TRI, true); I.isValid(); ++I)
              RegVals[*I].setKilledBy(&MI);
      for (MachineOperand &MO : MI.operands()) {
        if (MO.isReg() && MO.isDef() && MO.getReg().isPhysical() &&
            !(MI.isCopy() && MO.isImplicit()))
          clobber<MCRegAliasIterator>(MO.getReg(), true);
        else if (MO.isRegMask())
          for (unsigned Reg = 1; Reg != Z80::NUM_TARGET_REGS; ++Reg)
            if (MO.clobbersPhysReg(Reg))
              assign(Reg);
      }
      if (Val.valid()) {
        clobber<MCSuperRegIterator>(Reg, false);
        RegVals[Reg] = Val;
        for (MCSubRegIndexIterator SRII(Reg, TRI); SRII.isValid(); ++SRII)
          assign(SRII.getSubReg(), Val, SRII.getSubRegIndex());
      }
      switch (MI.getOpcode()) {
      case Z80::LD24r0:
        assign(Z80::F, ZeroFlag | SubtractFlag,
               SignFlag | ZeroFlag | HalfCarryFlag | ParityOverflowFlag |
                   SubtractFlag | CarryFlag);
        break;
      case Z80::LD24r_1:
        assign(Z80::F, SignFlag | HalfCarryFlag | SubtractFlag | CarryFlag,
               SignFlag | ZeroFlag | HalfCarryFlag | ParityOverflowFlag |
                   SubtractFlag | CarryFlag);
        break;
      case Z80::SCF:
        assign(Z80::F, CarryFlag, HalfCarryFlag | SubtractFlag | CarryFlag);
        break;
      case Z80::CCF:
      case Z80::ADD8ar:
      case Z80::ADD8ai:
      case Z80::ADD8ap:
      case Z80::ADD8ao:
      case Z80::ADC8ar:
      case Z80::ADC8ai:
      case Z80::ADC8ap:
      case Z80::ADC8ao:
        assign(Z80::F, 0, SubtractFlag);
        break;
      case Z80::SUB8ar:
      case Z80::SUB8ai:
      case Z80::SUB8ap:
      case Z80::SUB8ao:
      case Z80::SBC8ar:
      case Z80::SBC8ai:
      case Z80::SBC8ap:
      case Z80::SBC8ao:
        assign(Z80::F, SubtractFlag, SubtractFlag);
        break;
      case Z80::AND8ar:
      case Z80::AND8ai:
      case Z80::AND8ap:
      case Z80::AND8ao:
        assign(Z80::F, HalfCarryFlag, HalfCarryFlag | SubtractFlag | CarryFlag);
        break;
      case Z80::RCF:
      case Z80::XOR8ar:
      case Z80::XOR8ai:
      case Z80::XOR8ap:
      case Z80::XOR8ao:
      case Z80::OR8ar:
      case Z80::OR8ai:
      case Z80::OR8ap:
      case Z80::OR8ao:
        assign(Z80::F, 0, HalfCarryFlag | SubtractFlag | CarryFlag);
        break;
      }
      debug(MI);
    }
  }
  return Changed;
}

void Z80MachineLateOptimization::debug(const MachineInstr &MI) {
  for (unsigned Reg = 1; Reg != Z80::NUM_TARGET_REGS; ++Reg)
    if (RegVals[Reg].valid())
      LLVM_DEBUG(dbgs() << TRI->getName(Reg) << RegVals[Reg] << ", ");
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
