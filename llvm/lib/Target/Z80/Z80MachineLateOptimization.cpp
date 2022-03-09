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
  MachineInstr *DeadOrKilledBy = nullptr;

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
    DeadOrKilledBy = nullptr;
  }

  bool valid() const { return Off != UnknownOff; }
  bool isImm() const { return valid() && !GV; }
  bool isGlobal() const { return valid() && GV; }

  bool matches(const RegVal &Val, int Delta = 0) const {
    return valid() && Val.valid() && GV == Val.GV && Mask == Val.Mask &&
           Off == ((Val.Off + Delta) & Mask);
  }
  std::pair<int, int> getKnownBits(int KnownMask = ~0) const {
    if (!isImm())
      return {0, 0};
    return {Off & KnownMask, Mask & KnownMask};
  }

  void setDeadOrKilledBy(MachineInstr *MI) { DeadOrKilledBy = MI; }
  MachineInstr *takeDeadOrKilledBy() {
    return std::exchange(DeadOrKilledBy, nullptr);
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
  void clobberAll() {
    for (unsigned Reg = 1; Reg != Z80::NUM_TARGET_REGS; ++Reg)
      assign(Reg);
  }
  template <typename MCRegIterator>
  void clobber(MCRegister Reg, bool IncludeSelf) {
    for (MCRegIterator I(Reg, TRI, IncludeSelf); I.isValid(); ++I)
      assign(*I);
  }
  void updateDeadOrKilledBy(MachineInstr *MI,
                            bool (MachineOperand::*Pred)() const) {
    for (const MachineOperand &MO : MI->operands())
      if (MO.isReg() && (MO.*Pred)() && MO.getReg().isPhysical())
        for (MCRegAliasIterator I(MO.getReg(), TRI, true); I.isValid(); ++I)
          RegVals[*I].setDeadOrKilledBy(MI);
  }
  bool reuse(MCRegister Reg) {
    if (MachineInstr *DeadOrKilledBy = RegVals[Reg].takeDeadOrKilledBy()) {
      if (DeadOrKilledBy->registerDefIsDead(Reg, TRI)) {
        DeadOrKilledBy->clearRegisterDeads(Reg);
        return true;
      }
      if (DeadOrKilledBy->killsRegister(Reg, TRI)) {
        DeadOrKilledBy->clearRegisterKills(Reg, TRI);
        return true;
      }
    }
    return false;
  }

  bool isKnownSpecificImm(Register Reg, int Val) const {
    return Reg.isPhysical() && RegVals[Reg].matches({Val, Reg, *TRI});
  }
  bool isKnownSpecificImm(const MachineOperand &MO, int Val) const {
    if (MO.isImm())
      return MO.getImm() == Val;
    if (MO.isCImm())
      return MO.getCImm()->getSExtValue() == Val;
    return MO.isReg() && isKnownSpecificImm(MO.getReg(), Val);
  }

  void debug(const MachineInstr *MI);

  struct KnownFlags {
    const std::uint8_t SetMask = 0, ResetMask = 0, PreserveMask = 0;
    operator bool() const { return SetMask | ResetMask; }
    std::uint8_t getKnownVal(std::uint8_t KnownFlagsVal) const {
      return SetMask | (KnownFlagsVal & PreserveMask);
    }
    std::uint8_t getKnownMask(std::uint8_t KnownFlagsMask) const {
      return SetMask | ResetMask | (KnownFlagsMask & PreserveMask);
    }
  };
  KnownFlags getKnownFlags(const MachineInstr *MI, std::uint8_t KnownFlagsVal,
                           std::uint8_t KnownFlagsMask) const;

public:
  static char ID;

  Z80MachineLateOptimization() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override {
    return "Z80 Machine Late Optimization";
  }

  bool runOnMachineFunction(MachineFunction &MF) override;
};
} // end anonymous namespace

void Z80MachineLateOptimization::debug(const MachineInstr *MI) {
  for (unsigned Reg = 1; Reg != Z80::NUM_TARGET_REGS; ++Reg)
    if (RegVals[Reg].valid())
      LLVM_DEBUG(dbgs() << TRI->getName(Reg) << RegVals[Reg] << ", ");
  LLVM_DEBUG(MI->dump());
}

Z80MachineLateOptimization::KnownFlags
Z80MachineLateOptimization::getKnownFlags(const MachineInstr *MI,
                                          std::uint8_t KnownFlagsVal,
                                          std::uint8_t KnownFlagsMask) const {
  switch (unsigned Opc = MI->getOpcode()) {
  case Z80::LD24r0:
    return {ZeroFlag | SubtractFlag,
            SignFlag | HalfCarryFlag | ParityOverflowFlag | CarryFlag};
  case Z80::LD24r_1:
    return {SignFlag | HalfCarryFlag | SubtractFlag | CarryFlag,
            ZeroFlag | ParityOverflowFlag};
  case Z80::SCF:
    return {CarryFlag, HalfCarryFlag | SubtractFlag,
            SignFlag | ZeroFlag | ParityOverflowFlag};
  case Z80::CCF:
    return {0, SubtractFlag, SignFlag | ZeroFlag | ParityOverflowFlag};
  case Z80::ADD8ar:
  case Z80::ADD8ai:
    if (isKnownSpecificImm(Z80::A, 0) ||
        isKnownSpecificImm(MI->getOperand(0), 0))
      return {0, HalfCarryFlag | ParityOverflowFlag | SubtractFlag | CarryFlag};
    LLVM_FALLTHROUGH;
  case Z80::ADD8ap:
  case Z80::ADD8ao:
  case Z80::ADC8ar:
  case Z80::ADC8ai:
  case Z80::ADC8ap:
  case Z80::ADC8ao:
    return {0, SubtractFlag};
  case Z80::SUB8ar:
  case Z80::SUB8ai:
    if (isKnownSpecificImm(MI->getOperand(0), 0))
      return {SubtractFlag, HalfCarryFlag | ParityOverflowFlag | CarryFlag};
    LLVM_FALLTHROUGH;
  case Z80::SUB8ap:
  case Z80::SUB8ao:
  case Z80::SBC8ar:
  case Z80::SBC8ai:
  case Z80::SBC8ap:
  case Z80::SBC8ao:
    if (Opc != Z80::SBC8ar || MI->getOperand(0).getReg() != Z80::A)
      return {SubtractFlag};
    if (!(KnownFlagsMask & CarryFlag))
      return {SubtractFlag, ParityOverflowFlag};
    if (KnownFlagsVal & CarryFlag)
      return {SignFlag | HalfCarryFlag | SubtractFlag | CarryFlag,
              ZeroFlag | ParityOverflowFlag};
    return {ZeroFlag | SubtractFlag,
            SignFlag | HalfCarryFlag | ParityOverflowFlag | CarryFlag};
  case Z80::AND8ar:
  case Z80::AND8ai:
  case Z80::AND8ap:
  case Z80::AND8ao:
    return {HalfCarryFlag, SubtractFlag, CarryFlag};
  case Z80::RCF:
  case Z80::XOR8ar:
  case Z80::XOR8ai:
  case Z80::XOR8ap:
  case Z80::XOR8ao:
  case Z80::OR8ar:
  case Z80::OR8ai:
  case Z80::OR8ap:
  case Z80::OR8ao:
    return {0, HalfCarryFlag | SubtractFlag, CarryFlag};
  case Z80::ADD16SP:
  case Z80::ADD24SP:
    if (isKnownSpecificImm(Opc == Z80::ADD16SP ? Z80::SPS : Z80::SPL, 0))
      return {0, HalfCarryFlag | SubtractFlag | CarryFlag, SignFlag | ZeroFlag};
    LLVM_FALLTHROUGH;
  case Z80::ADD16aa:
  case Z80::ADD24aa:
  case Z80::ADD16ao:
  case Z80::ADD24ao:
    for (const MachineOperand &MO : MI->explicit_uses())
      if (isKnownSpecificImm(MO, 0))
        return {0, HalfCarryFlag | SubtractFlag | CarryFlag,
                SignFlag | ZeroFlag};
    return {0, SubtractFlag, SignFlag | ZeroFlag};
  case Z80::ADC16aa:
  case Z80::ADC24aa:
    return {0, SubtractFlag};
  case Z80::ADC16ao:
  case Z80::ADC24ao:
  case Z80::ADC16SP:
  case Z80::ADC24SP:
    return {0, SubtractFlag};
  case Z80::SBC16aa:
  case Z80::SBC24aa:
    if (!(KnownFlagsMask & CarryFlag))
      return {SubtractFlag, ParityOverflowFlag};
    if (KnownFlagsVal & CarryFlag)
      return {SignFlag | HalfCarryFlag | SubtractFlag | CarryFlag,
              ZeroFlag | ParityOverflowFlag};
    return {ZeroFlag | SubtractFlag,
            SignFlag | HalfCarryFlag | ParityOverflowFlag | CarryFlag};
  case Z80::SBC16ao:
  case Z80::SBC24ao:
    if ((~KnownFlagsVal & KnownFlagsMask & CarryFlag) &&
        (isKnownSpecificImm(Opc == Z80::SBC16ao ? Z80::HL : Z80::UHL, 0) ||
         isKnownSpecificImm(MI->getOperand(0), 0)))
      return {SubtractFlag, HalfCarryFlag | ParityOverflowFlag | CarryFlag};
    return {SubtractFlag};
  case Z80::SBC16SP:
  case Z80::SBC24SP:
    if ((~KnownFlagsVal & KnownFlagsMask & CarryFlag) &&
        (isKnownSpecificImm(Opc == Z80::SBC16SP ? Z80::HL : Z80::UHL, 0) ||
         isKnownSpecificImm(Opc == Z80::SBC16SP ? Z80::SPS : Z80::SPL, 0)))
      return {SubtractFlag, HalfCarryFlag | ParityOverflowFlag | CarryFlag};
    return {SubtractFlag};
  case Z80::SUB16ao:
  case Z80::SUB24ao:
    return {SubtractFlag};
  }
  return {};
}

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
      MachineInstrBuilder MIB(MF, I);
      LiveUnits.stepForward(*I);
      ++I;
      RegVal Val;
      Register Reg;
      std::uint8_t KnownFlagsVal, KnownFlagsMask;
      std::tie(KnownFlagsVal, KnownFlagsMask) = RegVals[Z80::F].getKnownBits();
      switch (unsigned Opc = MIB->getOpcode()) {
      case Z80::LD8ri:
      case Z80::LD16ri:
      case Z80::LD24ri:
      case Z80::LD24r0:
      case Z80::LD24r_1:
        Reg = MIB->getOperand(0).getReg();
        switch (Opc) {
        default:
          Val = {MIB->getOperand(1), Reg, *TRI};
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
        if (Opc != MIB->getOpcode()) {
          LLVM_DEBUG(dbgs() << "Replacing: "; MIB->dump();
                     dbgs() << "     With: ");
          MIB->setDesc(TII.get(Opc));
          MIB->RemoveOperand(1);
          MIB.addReg(Reg, getKillRegState(reuse(Reg)));
          MIB->addImplicitDefUseOperands(MF);
          break;
        }
        switch (Opc) {
        case Z80::LD24r0:
        case Z80::LD24r_1:
          if (Reg != Z80::UHL || !(KnownFlagsMask & CarryFlag) ||
              ((Opc == Z80::LD24r0) ^ !(KnownFlagsVal & CarryFlag)))
            break;
          LLVM_DEBUG(dbgs() << "Replacing: "; MIB->dump();
                     dbgs() << "     With: ");
          MIB->setDesc(TII.get(Z80::SBC24aa));
          MIB->getOperand(0).setImplicit();
          MIB->getOperand(1).setImplicit();
          MIB.addReg(Reg, RegState::Implicit | RegState::Undef);
          MIB.addReg(Z80::F,
                     RegState::Implicit | getKillRegState(reuse(Z80::F)));
          break;
        }
        break;
      case Z80::RCF:
      case Z80::SCF:
        Reg = Z80::F;
        if (!(KnownFlagsMask & CarryFlag) ||
            (Opc == Z80::RCF) ^ !(KnownFlagsVal & CarryFlag))
          break;
        Val = {KnownFlagsVal, KnownFlagsMask, Reg, *TRI};
        break;
      case Z80::RLC8r:
      case Z80::RRC8r:
      case Z80::RL8r:
      case Z80::RR8r:
        Reg = MIB->getOperand(0).getReg();
        if (Reg != Z80::A || !LiveUnits.available(Z80::F))
          break;
        switch (Opc) {
        case Z80::RLC8r: Opc = Z80::RLCA; break;
        case Z80::RRC8r: Opc = Z80::RRCA; break;
        case Z80::RL8r:  Opc = Z80::RLA;  break;
        case Z80::RR8r:  Opc = Z80::RRA;  break;
        }
        LLVM_DEBUG(dbgs() << "Replacing: "; MIB->dump();
                   dbgs() << "     With: ");
        MIB->setDesc(TII.get(Opc));
        MIB->getOperand(0).setImplicit();
        MIB->getOperand(1).setImplicit();
        break;
      case Z80::SUB8ar:
      case Z80::XOR8ar:
        Reg = MIB->getOperand(0).getReg();
        if (Reg != Z80::A)
          break;
        Val = {0, Reg, *TRI};
        break;
      case Z80::SUB16ao:
      case Z80::SUB24ao:
      case Z80::CP16ao:
      case Z80::CP24ao: {
        if (!(~KnownFlagsVal & KnownFlagsMask & CarryFlag))
          break;
        unsigned RestoreOpc;
        switch (Opc) {
        case Z80::SUB16ao:
        case Z80::CP16ao:
          Reg = Z80::HL;
          Opc = Z80::SBC16ao;
          RestoreOpc = Z80::ADD16ao;
          break;
        case Z80::SUB24ao:
        case Z80::CP24ao:
          Reg = Z80::UHL;
          Opc = Z80::SBC24ao;
          RestoreOpc = Z80::ADD24ao;
          break;
        }
        LLVM_DEBUG(dbgs() << "Replacing: "; MIB->dump();
                   dbgs() << "     With: ");
        MIB->setDesc(TII.get(Opc));
        MIB.addReg(Z80::F, RegState::Implicit | getKillRegState(reuse(Z80::F)));
        switch (Opc) {
        case Z80::CP16ao:
        case Z80::CP24ao:
          if (LiveUnits.available(Reg))
            break;
          MachineOperand &SrcMO = MIB->getOperand(0);
          MIB = BuildMI(MBB, I, MIB->getDebugLoc(), TII.get(RestoreOpc), Reg)
                    .addReg(Reg).add(SrcMO);
          SrcMO.setIsKill(false);
          Val = RegVals[Reg];
          break;
        }
        break;
      }
      }
      if (Val.matches(RegVals[Reg])) {
        LLVM_DEBUG(dbgs() << "Erasing redundant: "; MIB->dump());
        MIB->eraseFromParent();
        reuse(Reg);
        Changed = true;
        continue;
      }

      // Update killed uses after reuse and before clobbering defs.
      updateDeadOrKilledBy(MIB, &MachineOperand::isKill);

      // Get KnownFlags before clobbering defs.
      KnownFlags KnownFlags = getKnownFlags(MIB, KnownFlagsVal, KnownFlagsMask);

      // Clobber defs.
      for (MachineOperand &MO : MIB->operands()) {
        if (MO.isReg() && MO.isDef() && MO.getReg().isPhysical() &&
            !(MIB->isCopy() && MO.isImplicit()))
          clobber<MCRegAliasIterator>(MO.getReg(), true);
        else if (MO.isRegMask())
          for (unsigned Reg = 1; Reg != Z80::NUM_TARGET_REGS; ++Reg)
            if (MO.clobbersPhysReg(Reg))
              assign(Reg);
      }

      // Apply KnownFlags after clobbering defs.
      if (KnownFlags)
        assign(Z80::F, KnownFlags.getKnownVal(KnownFlagsVal),
               KnownFlags.getKnownMask(KnownFlagsMask));

      // Apply known val after clobbering defs.
      if (Val.valid()) {
        clobber<MCSuperRegIterator>(Reg, false);
        RegVals[Reg] = Val;
        for (MCSubRegIndexIterator SRII(Reg, TRI); SRII.isValid(); ++SRII)
          assign(SRII.getSubReg(), Val, SRII.getSubRegIndex());
      }

      // Update Dead Defs after reuse and after clobbering defs.
      updateDeadOrKilledBy(MIB, &MachineOperand::isDead);

      debug(MIB);
    }
  }
  return Changed;
}

char Z80MachineLateOptimization::ID = 0;
INITIALIZE_PASS(Z80MachineLateOptimization, DEBUG_TYPE,
                "Optimize Z80 machine instrs after regselect", false, false)

namespace llvm {
FunctionPass *createZ80MachineLateOptimizationPass() {
  return new Z80MachineLateOptimization();
}
} // end namespace llvm
