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
#include "Z80RegisterInfo.h"
#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/ADT/Statistic.h"

#define DEBUG_TYPE "z80-machine-late-opt"

using namespace llvm;

STATISTIC(NumPushPopPairsSeen,
          "Number of PUSH16/24r instructions inspected");
STATISTIC(NumPushPopAdjacentPairs,
          "Number of adjacent PUSH16/24r + POP16/24r pairs seen");
STATISTIC(NumPushPopDEHLCandidates,
          "Number of DE/HL exchange-eligible PUSH/POP pairs seen");
STATISTIC(NumPushPopRejectedSrcLive,
          "Number of DE/HL PUSH/POP pairs rejected because source stays live");
STATISTIC(NumPushPopExchangeFolds,
          "Number of PUSH/POP DE<->HL copies folded into EX");
STATISTIC(NumPushPopPopCandidates,
          "Number of PUSH/POP/POP stack-shuffle candidates seen");
STATISTIC(NumPushPopPopRejectedSameRegs,
          "Number of PUSH/POP/POP candidates rejected due to same src/mid reg");
STATISTIC(NumPushPopPopRejectedUnsupportedSrc,
          "Number of PUSH/POP/POP candidates rejected due to unsupported EX (SP),reg src");
STATISTIC(NumPushPopPopFolds,
          "Number of PUSH/POP/POP stack shuffles folded into EX (SP),reg + POP");
STATISTIC(NumPushPopPopLongCandidates,
          "Number of long PUSH/POP/POP.../PUSH/POP stack-shuffle candidates seen");
STATISTIC(NumPushPopPopLongFolds,
          "Number of long PUSH/POP/POP.../PUSH/POP stack-shuffles folded");
STATISTIC(NumCopyExchangeCandidates,
          "Number of DE<->HL COPY instructions considered for EX folding");
STATISTIC(NumCopyExchangeRejectedSrcLive,
          "Number of DE<->HL COPY instructions rejected because source is live");
STATISTIC(NumCopyExchangeFolds,
          "Number of DE<->HL COPY instructions folded into EX");

namespace {
class RegVal {
  static constexpr int UnknownOff = ~0;
  const GlobalValue *GV = nullptr;
  int Off = UnknownOff, Mask = 0;
  MachineInstr *DeadOrKilledBy = nullptr;

public:
  RegVal() {}
  RegVal(MCRegister Reg, const TargetRegisterInfo &TRI) {
    const TargetRegisterClass *RC = nullptr;
    for (const TargetRegisterClass *C : TRI.regclasses()) {
      if (C->contains(Reg) && (!RC || RC->hasSubClass(C)))
        RC = C;
    }
    if (RC)
      if (unsigned Bits = TRI.getRegSizeInBits(*RC); Bits <= 32)
        Mask = maskTrailingOnes<unsigned>(Bits);
  }
  RegVal(const MachineOperand &MO, MCRegister Reg,
         const TargetRegisterInfo &TRI)
      : RegVal(Reg, TRI) {
    if (!Mask)
      return;
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
    if (!Mask)
      return;
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
    unsigned Width = 2 + divideCeil(1 + Log2_32(Val.Mask), 4);
    OS << " & " << format_hex(Val.Mask, Width, true) << " == ";
    if (!Val.valid())
      return OS << "?";
    if (Val.GV)
      OS << Val.GV << '+';
    return OS << format_hex(Val.Off, Width, true);
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
  
  // track when Z flag correctly reflects whether A is zero
  // after SBC A,A: A is a member of {0, 0xFF} and Z = (A == 0), so OR A,A is redundant
  bool ZFlagReflectsAZero = false;

  // track redundant A<->reg copies, after "ld X, a", X mirrors A
  // if A hasnt changed when we see "ld a, X", we can eliminate the copy
  // AMirroredInReg = X means the value currently in A is also in X
  MCRegister AMirroredInReg = Z80::NoRegister;

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
      if (MO.isReg() && (MO.*Pred)())
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

  bool isKnownSpecificImm(MCRegister Reg, int Val) const {
    return RegVals[Reg].matches({Val, Reg, *TRI});
  }
  bool isKnownSpecificImm(const MachineOperand &MO, int Val) const {
    if (MO.isImm())
      return MO.getImm() == Val;
    if (MO.isCImm())
      return MO.getCImm()->getSExtValue() == Val;
    return MO.isReg() && isKnownSpecificImm(MO.getReg(), Val);
  }

  // fold recurring indexed counter idioms emitted after RA
  // 1. LD8ro + INC8r/DEC8r + LD8or -> INC8o/DEC8o
  // 2. forward an indexed byte load from a compare predecessor into a
  //    successor reload via COPY, when both access the same stack slot
  bool foldIndexedCounterPeepholes(MachineFunction &MF,
                                   const TargetInstrInfo &TII);
  // fold compare to zero helpers when addend is already known:
  //   ADDxxao dst, src ; OR A,A ; SBCxxao src
  // -> OR A,A ; ADCxxao src when src == 0
  // -> SCF    ; ADCxxao src when src == -1
  // guarded to cases where the next consumer is Z/NZ conditional
  bool foldZeroCompareKnownHelper(MachineFunction &MF,
                                  const TargetInstrInfo &TII);
  bool foldCopyExchangeCopies(MachineFunction &MF, const TargetInstrInfo &TII);
  bool foldPushPopPopStackShuffles(MachineFunction &MF,
                                   const TargetInstrInfo &TII);
  bool foldPushPopExchangeCopies(MachineFunction &MF,
                                 const TargetInstrInfo &TII);

  void debug(const MachineInstr &MI);

  std::tuple<uint8_t, uint8_t, MCRegister, RegVal>
  getKnownVal(const MachineInstr &MI) const;

  struct KnownFlags {
    static const KnownFlags PreserveDocumented;
    const uint8_t SetMask = 0, ResetMask = 0, PreserveMask = 0;
    operator bool() const { return SetMask | ResetMask; }
    uint8_t getKnownVal(uint8_t KnownFlagsVal) const {
      return SetMask | (KnownFlagsVal & PreserveMask);
    }
    uint8_t getKnownMask(uint8_t KnownFlagsMask) const {
      return SetMask | ResetMask | (KnownFlagsMask & PreserveMask);
    }
  };
  KnownFlags getKnownFlags(const MachineInstr &MI, uint8_t KnownFlagsVal,
                           uint8_t KnownFlagsMask) const;

public:
  static char ID;

  Z80MachineLateOptimization() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override {
    return "Z80 Machine Late Optimization";
  }

  bool runOnMachineFunction(MachineFunction &MF) override;
};
} // end anonymous namespace

static bool isDEHLExchangePair(Register RegA, Register RegB,
                               const TargetRegisterInfo &TRI) {
  bool HasDE = false, HasHL = false;
  for (Register Reg : {RegA, RegB}) {
    if (TRI.isSubRegisterEq(Z80::UDE, Reg))
      HasDE = true;
    else if (TRI.isSubRegisterEq(Z80::UHL, Reg))
      HasHL = true;
  }
  return HasDE && HasHL;
}

static bool is16DEHLReg(Register Reg) {
  return Reg == Z80::DE || Reg == Z80::HL;
}

static bool is24DEHLReg(Register Reg) {
  return Reg == Z80::UDE || Reg == Z80::UHL;
}

static bool isHLStackReg(Register Reg) {
  return Reg == Z80::HL || Reg == Z80::UHL;
}

static bool isBCDEStackReg(Register Reg) {
  return Reg == Z80::BC || Reg == Z80::DE || Reg == Z80::UBC ||
         Reg == Z80::UDE;
}

static bool isPhysRegLiveAt(const MachineBasicBlock &MBB,
                            MachineBasicBlock::const_iterator Pos,
                            MCPhysReg Reg, const TargetRegisterInfo &TRI,
                            const MachineRegisterInfo &MRI) {
  LivePhysRegs Live(TRI);
  Live.addLiveOuts(MBB);
  for (auto I = MBB.end(); I != Pos;) {
    --I;
    Live.stepBackward(*I);
  }
  return !Live.available(MRI, Reg);
}

static bool isZOrNZConditionalUse(const MachineInstr &MI) {
  int CCIdx = -1;
  switch (MI.getOpcode()) {
  default:
    return false;
  case Z80::CALL16CC:
  case Z80::CALL24CC:
  case Z80::TCRETURN16CC:
  case Z80::TCRETURN24CC:
  case Z80::JQCC:
  case Z80::JRCC:
  case Z80::JP16CC:
  case Z80::JP24CC:
    CCIdx = 1;
    break;
  case Z80::RET16CC:
  case Z80::RET24CC:
    CCIdx = 0;
    break;
  }

  if (CCIdx < 0 || MI.getNumExplicitOperands() <= static_cast<unsigned>(CCIdx))
    return false;
  const MachineOperand &CCMO = MI.getOperand(CCIdx);
  if (!CCMO.isImm())
    return false;

  int64_t CC = CCMO.getImm();
  // in Z80InstrInfo.td condition code immediates are NZ = 0, Z = 1
  return CC == 0 || CC == 1;
}

Z80MachineLateOptimization::KnownFlags const
    Z80MachineLateOptimization::KnownFlags::PreserveDocumented{
        0, 0,
        SignFlag | ZeroFlag | HalfCarryFlag | ParityOverflowFlag |
            SubtractFlag | CarryFlag};

void Z80MachineLateOptimization::debug(const MachineInstr &MI) {
  for (unsigned Reg = 1; Reg != Z80::NUM_TARGET_REGS; ++Reg)
    if (RegVals[Reg].valid())
      LLVM_DEBUG(dbgs() << TRI->getName(Reg) << RegVals[Reg] << ", ");
  LLVM_DEBUG(MI.dump());
}

std::tuple<uint8_t, uint8_t, MCRegister, RegVal>
Z80MachineLateOptimization::getKnownVal(const MachineInstr &MI) const {
  uint8_t KnownFlagsVal, KnownFlagsMask;
  std::tie(KnownFlagsVal, KnownFlagsMask) = RegVals[Z80::F].getKnownBits();
  MCRegister DstReg;
  RegVal DstVal;
  if (MI.getNumExplicitDefs() == 1)
    DstReg = MI.getOperand(0).getReg();
  switch (unsigned Opc = MI.getOpcode()) {
  case Z80::RCF: case Z80::SCF:
    DstReg = Z80::F;
    if (!(KnownFlagsMask & CarryFlag) ||
        (Opc == Z80::RCF) ^ !(KnownFlagsVal & CarryFlag))
      break;
    DstVal = {KnownFlagsVal, KnownFlagsMask, DstReg, *TRI};
    break;
  case Z80:: LD8ai: case Z80::LD16ai: case Z80::LD24ai:
  case Z80:: LD8ar: case Z80::LD8amb: case Z80:: LD8am: case Z80:: LD8ap:
  case Z80::SExt8: case Z80::CPL: case Z80::NEG: case Z80::DAA:
  case Z80::RLCA: case Z80::RRCA: case Z80::RLA: case Z80::RRA:
  case Z80::RRD16: case Z80::RLD16: case Z80::RRD24: case Z80::RLD24:
  case Z80::ADD8ar: case Z80::ADD8ai: case Z80::ADD8ap: case Z80::ADD8ao:
  case Z80::ADC8ar: case Z80::ADC8ai: case Z80::ADC8ap: case Z80::ADC8ao:
  case Z80::SUB8ar: case Z80::SUB8ai: case Z80::SUB8ap: case Z80::SUB8ao:
  case Z80::SBC8ar: case Z80::SBC8ai: case Z80::SBC8ap: case Z80::SBC8ao:
  case Z80::AND8ar: case Z80::AND8ai: case Z80::AND8ap: case Z80::AND8ao:
  case Z80::XOR8ar: case Z80::XOR8ai: case Z80::XOR8ap: case Z80::XOR8ao:
  case Z80:: OR8ar: case Z80:: OR8ai: case Z80:: OR8ap: case Z80:: OR8ao:
  case Z80:: CP8ar: case Z80:: CP8ai: case Z80:: CP8ap: case Z80:: CP8ao:
  case Z80::TST8ag: case Z80::TST8ai: case Z80::TST8ap:
    DstReg = Z80::A;
    switch (Opc) {
    case Z80::SUB8ar:
    case Z80::XOR8ar:
      if (MI.getOperand(0).getReg() != DstReg)
        break;
      DstVal = {0, DstReg, *TRI};
      break;
    }
    break;
  case Z80:: LD8ia: case Z80::LD16ia:
    DstReg = Z80::I;
    break;
  case Z80::LD8ra:
    DstReg = Z80::R;
    break;
  case Z80::LD8mba:
    DstReg = Z80::MB;
    break;
  case Z80::POP16AF: case Z80::POP24AF:
    DstReg = Z80::AF;
    break;
  case Z80::INC16s: case Z80::DEC16s:
    DstReg = Z80::SPS;
    break;
  case Z80::INC24s: case Z80::DEC24s:
    DstReg = Z80::SPL;
    break;
  case Z80::LD8ri:
  case Z80::LD16ri:
  case Z80::LD24ri:
    DstVal = {MI.getOperand(1), DstReg, *TRI};
    break;
  case Z80::LD8r0:
    DstVal = {0, DstReg, *TRI};
    break;
  case Z80::LD24r0:
    DstVal = {0, DstReg, *TRI};
    break;
  case Z80::LD24r_1:
    DstVal = {-1, DstReg, *TRI};
    break;
  case Z80::SExt16: case Z80::Sub16ao: case Z80::Cmp16ao:
  case Z80::SBC16aa: case Z80::SBC16ao: case Z80::SBC16as:
  case Z80::ADC16aa: case Z80::ADC16ao: case Z80::ADC16as:
    DstReg = Z80::HL;
    break;
  case Z80::SExt24: case Z80::Sub24ao: case Z80::Cmp24ao:
  case Z80::SBC24aa: case Z80::SBC24ao: case Z80::SBC24as:
  case Z80::ADC24aa: case Z80::ADC24ao: case Z80::ADC24as:
    DstReg = Z80::UHL;
    break;
  }
  return {KnownFlagsVal, KnownFlagsMask, DstReg, DstVal};
}

Z80MachineLateOptimization::KnownFlags
Z80MachineLateOptimization::getKnownFlags(const MachineInstr &MI,
                                          uint8_t KnownFlagsVal,
                                          uint8_t KnownFlagsMask) const {
  switch (unsigned Opc = MI.getOpcode()) {
  case Z80::LD8r0:
    if (MI.getOperand(0).getReg() != Z80::A)
      return KnownFlags::PreserveDocumented;
    return {ZeroFlag, SignFlag | HalfCarryFlag | ParityOverflowFlag |
                          SubtractFlag | CarryFlag};
  case Z80::LD24r0:
    if (MI.getOperand(0).getReg() != Z80::UHL)
      return KnownFlags::PreserveDocumented;
    return {ZeroFlag | SubtractFlag,
            SignFlag | HalfCarryFlag | ParityOverflowFlag | CarryFlag};
  case Z80::LD24r_1:
    if (MI.getOperand(0).getReg() != Z80::UHL)
      return KnownFlags::PreserveDocumented;
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
        isKnownSpecificImm(MI.getOperand(0), 0))
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
    if (isKnownSpecificImm(MI.getOperand(0), 0))
      return {SubtractFlag, HalfCarryFlag | ParityOverflowFlag | CarryFlag};
    LLVM_FALLTHROUGH;
  case Z80::SUB8ap:
  case Z80::SUB8ao:
  case Z80::SBC8ar:
  case Z80::SBC8ai:
  case Z80::SBC8ap:
  case Z80::SBC8ao:
    if (Opc != Z80::SBC8ar || MI.getOperand(0).getReg() != Z80::A)
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
  case Z80::ADD16as:
  case Z80::ADD24as:
    if (isKnownSpecificImm(Opc == Z80::ADD16as ? Z80::SPS : Z80::SPL, 0))
      return {0, HalfCarryFlag | SubtractFlag | CarryFlag, SignFlag | ZeroFlag};
    LLVM_FALLTHROUGH;
  case Z80::ADD16aa:
  case Z80::ADD24aa:
  case Z80::ADD16ao:
  case Z80::ADD24ao:
    for (const MachineOperand &MO : MI.explicit_uses())
      if (isKnownSpecificImm(MO, 0))
        return {0, HalfCarryFlag | SubtractFlag | CarryFlag,
                SignFlag | ZeroFlag};
    return {0, SubtractFlag, SignFlag | ZeroFlag};
  case Z80::ADC16aa:
  case Z80::ADC24aa:
    return {0, SubtractFlag};
  case Z80::ADC16ao:
  case Z80::ADC24ao:
  case Z80::ADC16as:
  case Z80::ADC24as:
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
         isKnownSpecificImm(MI.getOperand(0), 0)))
      return {SubtractFlag, HalfCarryFlag | ParityOverflowFlag | CarryFlag};
    return {SubtractFlag};
  case Z80::SBC16as:
  case Z80::SBC24as:
    if ((~KnownFlagsVal & KnownFlagsMask & CarryFlag) &&
        (isKnownSpecificImm(Opc == Z80::SBC16as ? Z80::HL : Z80::UHL, 0) ||
         isKnownSpecificImm(Opc == Z80::SBC16as ? Z80::SPS : Z80::SPL, 0)))
      return {SubtractFlag, HalfCarryFlag | ParityOverflowFlag | CarryFlag};
    return {SubtractFlag};
  case Z80::Sub16ao:
  case Z80::Sub24ao:
    return {SubtractFlag};
  }
  return {};
}

bool Z80MachineLateOptimization::foldIndexedCounterPeepholes(
    MachineFunction &MF, const TargetInstrInfo &TII) {
  bool Changed = false;

  auto sameIndexedOff = [](const MachineInstr &LoadMI,
                           const MachineInstr &StoreMI) {
    return LoadMI.getOperand(1).isReg() && StoreMI.getOperand(0).isReg() &&
           LoadMI.getOperand(1).getReg() == StoreMI.getOperand(0).getReg() &&
           LoadMI.getOperand(2).isImm() && StoreMI.getOperand(1).isImm() &&
           LoadMI.getOperand(2).getImm() == StoreMI.getOperand(1).getImm();
  };

  for (MachineBasicBlock &MBB : MF) {
    for (MachineBasicBlock::iterator I = MBB.begin(), E = MBB.end(); I != E;) {
      if (I->isDebugInstr()) {
        ++I;
        continue;
      }
      MachineInstr &LoadMI = *I;
      bool Is8BitReload = LoadMI.getOpcode() == Z80::LD8ro;
      bool IsWideReload = LoadMI.getOpcode() == Z80::LD88ro ||
                          LoadMI.getOpcode() == Z80::LD16ro ||
                          LoadMI.getOpcode() == Z80::LD24ro;
      if (!Is8BitReload && !IsWideReload) {
        ++I;
        continue;
      }

      MachineBasicBlock::iterator IncI = std::next(I);
      while (IncI != E && IncI->isDebugInstr())
        ++IncI;
      if (IncI == E) {
        ++I;
        continue;
      }
      MachineInstr &IncMI = *IncI;
      unsigned NewOpc = Z80::INSTRUCTION_LIST_END;
      if (IncMI.getOpcode() == Z80::INC8r)
        NewOpc = Z80::INC8o;
      else if (IncMI.getOpcode() == Z80::DEC8r)
        NewOpc = Z80::DEC8o;
      else {
        ++I;
        continue;
      }

      MachineBasicBlock::iterator StoreI = std::next(IncI);
      while (StoreI != E && StoreI->isDebugInstr())
        ++StoreI;
      if (StoreI == E) {
        ++I;
        continue;
      }
      MachineInstr &StoreMI = *StoreI;
      if (!LoadMI.getOperand(0).isReg() || !IncMI.getOperand(0).isReg() ||
          !sameIndexedOff(LoadMI, StoreMI)) {
        ++I;
        continue;
      }

      bool IsMatching8BitStore =
          Is8BitReload && StoreMI.getOpcode() == Z80::LD8or &&
          StoreMI.getOperand(2).isReg() && StoreMI.getOperand(2).isKill() &&
          StoreMI.getOperand(2).getReg() == LoadMI.getOperand(0).getReg() &&
          StoreMI.getOperand(2).getReg() == IncMI.getOperand(0).getReg();
      bool IsMatchingWideStore =
          IsWideReload &&
          (StoreMI.getOpcode() == Z80::LD88or || StoreMI.getOpcode() == Z80::LD16or ||
           StoreMI.getOpcode() == Z80::LD24or) &&
          StoreMI.getOperand(2).isReg() && StoreMI.getOperand(2).isKill() &&
          (LoadMI.getOperand(0).getReg() == Z80::HL ||
           LoadMI.getOperand(0).getReg() == Z80::UHL) &&
          StoreMI.getOperand(2).getReg() == LoadMI.getOperand(0).getReg() &&
          IncMI.getOperand(0).getReg() == Z80::L;
      if (!IsMatching8BitStore && !IsMatchingWideStore) {
        ++I;
        continue;
      }

      LLVM_DEBUG(dbgs() << "folding indexed load/inc/store counter update in "
                        << MF.getName() << ":\n";
                 LoadMI.dump(); IncMI.dump(); StoreMI.dump());

      MachineInstrBuilder NewMI =
          BuildMI(MBB, I, LoadMI.getDebugLoc(), TII.get(NewOpc));
      NewMI.add(LoadMI.getOperand(1));
      NewMI.add(LoadMI.getOperand(2));

      MachineBasicBlock::iterator NextI = std::next(StoreI);
      LoadMI.eraseFromParent();
      IncMI.eraseFromParent();
      StoreMI.eraseFromParent();
      I = NextI;
      Changed = true;
    }
  }

  for (MachineBasicBlock &MBB : MF) {
    MachineBasicBlock *Pred = MBB.getSinglePredecessor();
    if (!Pred || !Pred->isLayoutSuccessor(&MBB))
      continue;

    auto FirstI = MBB.begin();
    while (FirstI != MBB.end() && FirstI->isDebugInstr())
      ++FirstI;
    if (FirstI == MBB.end())
      continue;
    MachineInstr &ReloadMI = *FirstI;
    if (ReloadMI.getOpcode() != Z80::LD8ro ||
        !ReloadMI.getOperand(0).isReg() ||
        ReloadMI.getOperand(0).getReg() != Z80::E ||
        !ReloadMI.getOperand(1).isReg() ||
        ReloadMI.getOperand(1).getReg() != Z80::UIX ||
        !ReloadMI.getOperand(2).isImm())
      continue;
    int64_t ReloadOff = ReloadMI.getOperand(2).getImm();

    MachineBasicBlock::iterator BrI = Pred->getLastNonDebugInstr();
    if (BrI == Pred->end())
      continue;
    MachineInstr &BrMI = *BrI;
    if (BrMI.getOpcode() != Z80::JQCC)
      continue;

    MachineBasicBlock::iterator CPI = BrI;
    do {
      if (CPI == Pred->begin()) {
        CPI = Pred->end();
        break;
      }
      --CPI;
    } while (CPI->isDebugInstr());
    if (CPI == Pred->end())
      continue;
    MachineInstr &CPMI = *CPI;
    if (CPMI.getOpcode() != Z80::CP8ai || !CPMI.readsRegister(Z80::A, TRI) ||
        CPMI.modifiesRegister(Z80::A, TRI))
      continue;

    MachineBasicBlock::iterator LoadI = CPI;
    do {
      if (LoadI == Pred->begin()) {
        LoadI = Pred->end();
        break;
      }
      --LoadI;
    } while (LoadI->isDebugInstr());
    if (LoadI == Pred->end())
      continue;
    MachineInstr &PredLoadMI = *LoadI;
    if (PredLoadMI.getOpcode() != Z80::LD8ro ||
        !PredLoadMI.getOperand(0).isReg() ||
        PredLoadMI.getOperand(0).getReg() != Z80::A ||
        !PredLoadMI.getOperand(1).isReg() ||
        PredLoadMI.getOperand(1).getReg() != Z80::UIX ||
        !PredLoadMI.getOperand(2).isImm() ||
        PredLoadMI.getOperand(2).getImm() != ReloadOff)
      continue;

    LLVM_DEBUG(dbgs() << "forwarding indexed counter reload through A in "
                      << MF.getName() << ":\n";
               PredLoadMI.dump(); ReloadMI.dump());

    ReloadMI.dropMemRefs(MF);
    ReloadMI.removeOperand(2); // displacement
    ReloadMI.setDesc(TII.get(TargetOpcode::COPY));
    MachineOperand &SrcMO = ReloadMI.getOperand(1);
    SrcMO.ChangeToRegister(Z80::A, /*isDef=*/false, /*isImp=*/false,
                           /*isKill=*/false);
    Changed = true;
  }

  // if a compare predecessor already loaded the same indexed byte/word into HL
  // forward it into a successor body that immediately reloads before push
  for (MachineBasicBlock &MBB : MF) {
    MachineBasicBlock *Pred = MBB.getSinglePredecessor();
    if (!Pred || !Pred->isLayoutSuccessor(&MBB))
      continue;

    MachineBasicBlock::iterator BrI = Pred->getLastNonDebugInstr();
    if (BrI == Pred->end() || BrI->getOpcode() != Z80::JQCC)
      continue;

    MachineBasicBlock::iterator CPI = BrI;
    do {
      if (CPI == Pred->begin()) {
        CPI = Pred->end();
        break;
      }
      --CPI;
    } while (CPI->isDebugInstr());
    if (CPI == Pred->end() || CPI->getOpcode() != Z80::CP8ai)
      continue;

    MachineInstr *LowByteCopyMI = nullptr;
    MachineBasicBlock::iterator PredLoadI = CPI;
    do {
      if (PredLoadI == Pred->begin()) {
        PredLoadI = Pred->end();
        break;
      }
      --PredLoadI;
    } while (PredLoadI->isDebugInstr());
    if (PredLoadI == Pred->end())
      continue;
    if (PredLoadI->getOpcode() == TargetOpcode::COPY &&
        PredLoadI->getOperand(0).isReg() &&
        PredLoadI->getOperand(0).getReg() == Z80::A &&
        PredLoadI->getOperand(1).isReg() &&
        PredLoadI->getOperand(1).getReg() == Z80::L) {
      LowByteCopyMI = &*PredLoadI;
      do {
        if (PredLoadI == Pred->begin()) {
          PredLoadI = Pred->end();
          break;
        }
        --PredLoadI;
      } while (PredLoadI->isDebugInstr());
      if (PredLoadI == Pred->end())
        continue;
    }
    MachineInstr &PredLoadMI = *PredLoadI;
    if ((PredLoadMI.getOpcode() != Z80::LD88ro &&
         PredLoadMI.getOpcode() != Z80::LD16ro &&
         PredLoadMI.getOpcode() != Z80::LD24ro) ||
        !PredLoadMI.getOperand(0).isReg() ||
        (PredLoadMI.getOperand(0).getReg() != Z80::HL &&
         PredLoadMI.getOperand(0).getReg() != Z80::UHL) ||
        !PredLoadMI.getOperand(1).isReg() ||
        PredLoadMI.getOperand(1).getReg() != Z80::UIX ||
        !PredLoadMI.getOperand(2).isImm())
      continue;
    MCRegister WideReg = PredLoadMI.getOperand(0).getReg();

    bool HLClobberedInPred = false;
    for (auto I = std::next(PredLoadI); I != BrI; ++I) {
      if (I->isDebugInstr())
        continue;
      if (I->modifiesRegister(WideReg, TRI)) {
        HLClobberedInPred = true;
        break;
      }
    }
    if (HLClobberedInPred)
      continue;

    MachineBasicBlock::iterator ReloadI = MBB.end();
    for (auto I = MBB.begin(), E = MBB.end(); I != E; ++I) {
      if (I->isDebugInstr())
        continue;
      if ((I->getOpcode() == Z80::LD88ro || I->getOpcode() == Z80::LD16ro ||
           I->getOpcode() == Z80::LD24ro) &&
          I->getOperand(0).isReg() && I->getOperand(0).getReg() == WideReg &&
          I->getOperand(1).isReg() && I->getOperand(1).getReg() == Z80::UIX &&
          I->getOperand(2).isImm() &&
          I->getOperand(2).getImm() == PredLoadMI.getOperand(2).getImm()) {
        auto NextI = std::next(I);
        while (NextI != E && NextI->isDebugInstr())
          ++NextI;
        if (NextI != E && NextI->getOpcode() == Z80::PUSH24r &&
            NextI->getOperand(0).isReg() &&
            NextI->getOperand(0).getReg() == WideReg) {
          ReloadI = I;
        }
        break;
      }
      if (I->modifiesRegister(WideReg, TRI))
        break;
    }
    if (ReloadI == MBB.end())
      continue;

    LLVM_DEBUG(dbgs() << "forwarding indexed HL reload into push in "
                      << MF.getName() << ":\n";
               PredLoadMI.dump(); ReloadI->dump());

    if (LowByteCopyMI && LowByteCopyMI->killsRegister(WideReg, TRI))
      LowByteCopyMI->clearRegisterKills(WideReg, TRI);

    ReloadI->eraseFromParent();
    Changed = true;
  }

  return Changed;
}

bool Z80MachineLateOptimization::foldZeroCompareKnownHelper(
    MachineFunction &MF, const TargetInstrInfo &TII) {
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    auto MBBEnd = MBB.end();
    auto nextNonDebug = [&](MachineBasicBlock::iterator It) {
      while (It != MBBEnd && It->isDebugInstr())
        ++It;
      return It;
    };
    auto prevNonDebug = [&](MachineBasicBlock::iterator It) {
      while (It != MBB.begin()) {
        --It;
        if (!It->isDebugInstr())
          return It;
      }
      return MBB.end();
    };

    for (MachineBasicBlock::iterator I = MBB.begin(); I != MBBEnd;) {
      if (I->isDebugInstr()) {
        ++I;
        continue;
      }

      unsigned AddOpc = I->getOpcode();
      bool Is24 = false;
      switch (AddOpc) {
      default:
        ++I;
        continue;
      case Z80::ADD16ao:
        Is24 = false;
        break;
      case Z80::ADD24ao:
        Is24 = true;
        break;
      }

      if (I->getNumOperands() < 3 || !I->getOperand(2).isReg() ||
          I->getOperand(2).isUndef()) {
        ++I;
        continue;
      }
      Register SrcReg = I->getOperand(2).getReg();

      MachineBasicBlock::iterator LoadI = prevNonDebug(I);
      if (LoadI == MBB.end() || LoadI->getOpcode() != (Is24 ? Z80::LD24ri : Z80::LD16ri) ||
          LoadI->getNumOperands() < 2 || !LoadI->getOperand(0).isReg() ||
          !LoadI->getOperand(1).isImm() || LoadI->getOperand(0).getReg() != SrcReg) {
        ++I;
        continue;
      }

      int64_t SrcImm = LoadI->getOperand(1).getImm();
      bool SrcIsZero = SrcImm == 0;
      bool SrcIsMinusOne = SrcImm == -1;
      if (!SrcIsZero && !SrcIsMinusOne) {
        ++I;
        continue;
      }

      MachineBasicBlock::iterator OrI = nextNonDebug(std::next(I));
      if (OrI == MBBEnd || OrI->getOpcode() != Z80::OR8ar ||
          OrI->getNumOperands() < 1 || !OrI->getOperand(0).isReg() ||
          OrI->getOperand(0).getReg() != Z80::A) {
        ++I;
        continue;
      }

      unsigned SbcOpc = Is24 ? Z80::SBC24ao : Z80::SBC16ao;
      unsigned AdcOpc = Is24 ? Z80::ADC24ao : Z80::ADC16ao;
      MachineBasicBlock::iterator SbcI = nextNonDebug(std::next(OrI));
      if (SbcI == MBBEnd || SbcI->getOpcode() != SbcOpc ||
          SbcI->getNumOperands() < 1 || !SbcI->getOperand(0).isReg() ||
          SbcI->getOperand(0).isUndef() || SbcI->getOperand(0).getReg() != SrcReg) {
        ++I;
        continue;
      }

      MachineBasicBlock::iterator UserI = nextNonDebug(std::next(SbcI));
      if (UserI == MBBEnd || !isZOrNZConditionalUse(*UserI)) {
        ++I;
        continue;
      }

      LLVM_DEBUG(dbgs() << "folding zero-compare helper sequence in "
                        << MF.getName() << ":\n";
                 I->dump(); OrI->dump(); SbcI->dump());

      if (SrcIsMinusOne) {
        BuildMI(MBB, OrI, OrI->getDebugLoc(), TII.get(Z80::SCF));
        OrI->eraseFromParent();
      }

      SbcI->setDesc(TII.get(AdcOpc));
      I = MBB.erase(I);
      Changed = true;
    }
  }

  return Changed;
}

bool Z80MachineLateOptimization::foldCopyExchangeCopies(
    MachineFunction &MF, const TargetInstrInfo &TII) {
  bool Changed = false;
  const MachineRegisterInfo &MRI = MF.getRegInfo();

  for (MachineBasicBlock &MBB : MF) {
    for (MachineBasicBlock::iterator I = MBB.begin(); I != MBB.end();) {
      MachineInstr &MI = *I;
      if (!MI.isCopy()) {
        ++I;
        continue;
      }
      if (MI.getNumOperands() != 2) {
        ++I;
        continue;
      }
      if (!MI.getOperand(0).isReg() || !MI.getOperand(1).isReg()) {
        ++I;
        continue;
      }

      Register DstReg = MI.getOperand(0).getReg();
      Register SrcReg = MI.getOperand(1).getReg();

      bool Is16BitCopy = is16DEHLReg(DstReg) && is16DEHLReg(SrcReg);
      bool Is24BitCopy = is24DEHLReg(DstReg) && is24DEHLReg(SrcReg);
      if ((!Is16BitCopy && !Is24BitCopy) || DstReg == SrcReg) {
        ++I;
        continue;
      }
      ++NumCopyExchangeCandidates;

      if (isPhysRegLiveAt(MBB, std::next(I), SrcReg, *TRI, MRI)) {
        ++NumCopyExchangeRejectedSrcLive;
        ++I;
        continue;
      }

      unsigned ExOpc = Is24BitCopy ? Z80::EX24DE : Z80::EX16DE;
      LLVM_DEBUG(dbgs() << "Folding DE/HL copy into EX in " << MF.getName()
                        << ":\n";
                 MI.dump());

      MachineInstrBuilder ExMIB =
          BuildMI(MBB, I, MI.getDebugLoc(), TII.get(ExOpc));

      if (MachineOperand *SrcUse =
              ExMIB->findRegisterUseOperand(SrcReg, nullptr))
        SrcUse->setIsKill();
      if (MachineOperand *DstUse =
              ExMIB->findRegisterUseOperand(DstReg, nullptr))
        DstUse->setIsUndef();
      if (MachineOperand *SrcDef = ExMIB->findRegisterDefOperand(
              SrcReg, /*TRI=*/nullptr, /*isDead=*/false, /*Overlap=*/false))
        SrcDef->setIsDead();

      I = MBB.erase(I);
      ++NumCopyExchangeFolds;
      Changed = true;
    }
  }

  return Changed;
}

bool Z80MachineLateOptimization::foldPushPopExchangeCopies(
    MachineFunction &MF, const TargetInstrInfo &TII) {
  bool Changed = false;
  const MachineRegisterInfo &MRI = MF.getRegInfo();

  for (MachineBasicBlock &MBB : MF) {
    for (MachineBasicBlock::iterator I = MBB.begin(); I != MBB.end();) {
      MachineInstr &PushMI = *I;
      bool Is24BitPush = PushMI.getOpcode() == Z80::PUSH24r;
      if (!Is24BitPush && PushMI.getOpcode() != Z80::PUSH16r) {
        ++I;
        continue;
      }
      ++NumPushPopPairsSeen;

      MachineBasicBlock::iterator PopI = std::next(I);
      while (PopI != MBB.end() && PopI->isDebugInstr())
        ++PopI;
      if (PopI == MBB.end()) {
        ++I;
        continue;
      }

      unsigned ExpectedPopOpc = Is24BitPush ? Z80::POP24r : Z80::POP16r;
      if (PopI->getOpcode() != ExpectedPopOpc) {
        ++I;
        continue;
      }
      ++NumPushPopAdjacentPairs;

      Register SrcReg = PushMI.getOperand(0).getReg();
      Register DstReg = PopI->getOperand(0).getReg();
      if (!isDEHLExchangePair(SrcReg, DstReg, *TRI)) {
        ++I;
        continue;
      }
      ++NumPushPopDEHLCandidates;

      if (isPhysRegLiveAt(MBB, std::next(PopI), SrcReg, *TRI, MRI)) {
        ++NumPushPopRejectedSrcLive;
        ++I;
        continue;
      }

      LLVM_DEBUG(dbgs() << "Folding push/pop copy into EX in " << MF.getName()
                        << ":\n";
                 PushMI.dump(); PopI->dump());

      unsigned ExOpc = Is24BitPush ? Z80::EX24DE : Z80::EX16DE;
      MachineInstrBuilder ExMIB = BuildMI(MBB, I, PushMI.getDebugLoc(),
                                          TII.get(ExOpc));
      if (MachineOperand *SrcUse =
              ExMIB->findRegisterUseOperand(SrcReg, nullptr))
        SrcUse->setIsKill();
      if (MachineOperand *DstUse =
              ExMIB->findRegisterUseOperand(DstReg, nullptr))
        DstUse->setIsUndef();
      if (MachineOperand *SrcDef = ExMIB->findRegisterDefOperand(
              SrcReg, /*TRI=*/nullptr, /*isDead=*/false, /*Overlap=*/false))
        SrcDef->setIsDead();

      PopI->eraseFromParent();
      I = MBB.erase(I);
      ++NumPushPopExchangeFolds;
      Changed = true;
    }
  }

  return Changed;
}

bool Z80MachineLateOptimization::foldPushPopPopStackShuffles(
    MachineFunction &MF, const TargetInstrInfo &TII) {
  bool Changed = false;

  auto NextNonDebug = [](MachineBasicBlock &MBB,
                         MachineBasicBlock::iterator It) {
    while (It != MBB.end() && It->isDebugInstr())
      ++It;
    return It;
  };

  for (MachineBasicBlock &MBB : MF) {
    for (MachineBasicBlock::iterator I = MBB.begin(); I != MBB.end();) {
      // fold:
      //   ex(sp), src
      //   pop mid
      //   pop src (repeated N times)
      //   push mid
      //   pop src
      // ->
      //   pop mid (repeated N+1 times)
      //   push src
      //   pop mid
      if (I->getOpcode() == Z80::EX24sa || I->getOpcode() == Z80::EX16sa) {
        bool Is24BitEx = I->getOpcode() == Z80::EX24sa;
        unsigned ExpectedPopOpc = Is24BitEx ? Z80::POP24r : Z80::POP16r;
        unsigned ExpectedPushOpc = Is24BitEx ? Z80::PUSH24r : Z80::PUSH16r;
        Register SrcReg = I->getOperand(0).getReg();

        if (isHLStackReg(SrcReg)) {
          MachineBasicBlock::iterator PopMidI = NextNonDebug(MBB, std::next(I));
          if (PopMidI != MBB.end() && PopMidI->getOpcode() == ExpectedPopOpc) {
            Register MidReg = PopMidI->getOperand(0).getReg();
            if (isBCDEStackReg(MidReg)) {
              SmallVector<MachineBasicBlock::iterator, 8> SrcPops;
              MachineBasicBlock::iterator ScanI =
                  NextNonDebug(MBB, std::next(PopMidI));
                while (ScanI != MBB.end() &&
                       ScanI->getOpcode() == ExpectedPopOpc &&
                       ScanI->getOperand(0).getReg() == SrcReg) {
                  SrcPops.push_back(ScanI);
                  ScanI = NextNonDebug(MBB, std::next(ScanI));
                }

              if (!SrcPops.empty()) {
                MachineBasicBlock::iterator PushMidI = ScanI;
                MachineBasicBlock::iterator TailPopI =
                    PushMidI != MBB.end()
                        ? NextNonDebug(MBB, std::next(PushMidI))
                        : MBB.end();
                if (PushMidI != MBB.end() && TailPopI != MBB.end() &&
                    PushMidI->getOpcode() == ExpectedPushOpc &&
                    PushMidI->getOperand(0).getReg() == MidReg &&
                    TailPopI->getOpcode() == ExpectedPopOpc &&
                    TailPopI->getOperand(0).getReg() == SrcReg) {
                  ++NumPushPopPopLongCandidates;
                  LLVM_DEBUG(dbgs() << "Folding long ex/pop/pop.../push/pop "
                                       "stack shuffle in "
                                    << MF.getName() << ":\n";
                             I->dump(); PopMidI->dump());

                  for (unsigned Idx = 0, E = SrcPops.size() + 1; Idx != E;
                       ++Idx) {
                    MachineInstrBuilder PopMIB =
                        BuildMI(MBB, I, PopMidI->getDebugLoc(),
                                TII.get(ExpectedPopOpc), MidReg);
                    PopMIB->getOperand(0).setIsDead();
                  }

                  BuildMI(MBB, I, I->getDebugLoc(), TII.get(ExpectedPushOpc))
                      .addReg(SrcReg,
                              getKillRegState(I->getOperand(0).isKill()));
                  MachineInstrBuilder FinalPop =
                      BuildMI(MBB, I, TailPopI->getDebugLoc(),
                              TII.get(ExpectedPopOpc), MidReg);
                  if (PushMidI->getOperand(0).isKill())
                    FinalPop->getOperand(0).setIsDead();

                  TailPopI->eraseFromParent();
                  PushMidI->eraseFromParent();
                  for (auto It : SrcPops)
                    It->eraseFromParent();
                  PopMidI->eraseFromParent();
                  I = MBB.erase(I);
                  ++NumPushPopPopLongFolds;
                  Changed = true;
                  continue;
                }
              }
            }
          }
        }
      }

      MachineInstr &PushMI = *I;
      bool Is24BitPush = PushMI.getOpcode() == Z80::PUSH24r;
      if (!Is24BitPush && PushMI.getOpcode() != Z80::PUSH16r) {
        ++I;
        continue;
      }

      MachineBasicBlock::iterator Pop1I = std::next(I);
      while (Pop1I != MBB.end() && Pop1I->isDebugInstr())
        ++Pop1I;
      if (Pop1I == MBB.end()) {
        ++I;
        continue;
      }

      unsigned ExpectedPopOpc = Is24BitPush ? Z80::POP24r : Z80::POP16r;
      if (Pop1I->getOpcode() != ExpectedPopOpc) {
        ++I;
        continue;
      }

      Register SrcReg = PushMI.getOperand(0).getReg();
      Register MidReg = Pop1I->getOperand(0).getReg();
      unsigned ExpectedPushOpc = Is24BitPush ? Z80::PUSH24r : Z80::PUSH16r;

      // prefer the longer fold first
      //   push src
      //   pop  mid
      //   pop  src (repeated N times
      //   push mid
      //   pop  src
      // ->
      //   pop  mid (repeated N times)
      //   push src
      //   pop  mid
      if (isHLStackReg(SrcReg) && isBCDEStackReg(MidReg)) {
        SmallVector<MachineBasicBlock::iterator, 8> SrcPops;
        MachineBasicBlock::iterator ScanI = NextNonDebug(MBB, std::next(Pop1I));
        while (ScanI != MBB.end() && ScanI->getOpcode() == ExpectedPopOpc &&
               ScanI->getOperand(0).getReg() == SrcReg) {
          SrcPops.push_back(ScanI);
          ScanI = NextNonDebug(MBB, std::next(ScanI));
        }

        if (!SrcPops.empty()) {
          MachineBasicBlock::iterator PushMidI = ScanI;
          MachineBasicBlock::iterator TailPopI =
              PushMidI != MBB.end() ? NextNonDebug(MBB, std::next(PushMidI))
                                    : MBB.end();
          if (PushMidI != MBB.end() && TailPopI != MBB.end() &&
              PushMidI->getOpcode() == ExpectedPushOpc &&
              PushMidI->getOperand(0).getReg() == MidReg &&
              TailPopI->getOpcode() == ExpectedPopOpc &&
              TailPopI->getOperand(0).getReg() == SrcReg) {
            ++NumPushPopPopLongCandidates;
            LLVM_DEBUG(dbgs() << "Folding long push/pop/pop.../push/pop stack "
                                 "shuffle in "
                              << MF.getName() << ":\n";
                       PushMI.dump(); Pop1I->dump());

            // Emit N POP Mid replacements.
            for (unsigned Idx = 0, E = SrcPops.size(); Idx != E; ++Idx) {
              MachineInstrBuilder PopMIB =
                  BuildMI(MBB, I, SrcPops[Idx]->getDebugLoc(),
                          TII.get(ExpectedPopOpc), MidReg);
              bool IsFinalValuePop = (Idx + 1 == E);
              bool MidDeadAfter = PushMidI->getOperand(0).isKill();
              if (!IsFinalValuePop || MidDeadAfter)
                PopMIB->getOperand(0).setIsDead();
            }

            BuildMI(MBB, I, PushMI.getDebugLoc(), TII.get(ExpectedPushOpc))
                .addReg(SrcReg,
                        getKillRegState(PushMI.getOperand(0).isKill()));
            MachineInstrBuilder FinalPop =
                BuildMI(MBB, I, TailPopI->getDebugLoc(),
                        TII.get(ExpectedPopOpc), MidReg);
            if (PushMidI->getOperand(0).isKill())
              FinalPop->getOperand(0).setIsDead();

            TailPopI->eraseFromParent();
            PushMidI->eraseFromParent();
            for (auto It : SrcPops)
              It->eraseFromParent();
            Pop1I->eraseFromParent();
            I = MBB.erase(I);
            ++NumPushPopPopLongFolds;
            Changed = true;
            continue;
          }
        }
      }

      MachineBasicBlock::iterator Pop2I = std::next(Pop1I);
      while (Pop2I != MBB.end() && Pop2I->isDebugInstr())
        ++Pop2I;
      if (Pop2I == MBB.end() || Pop2I->getOpcode() != ExpectedPopOpc) {
        ++I;
        continue;
      }

      ++NumPushPopPopCandidates;
      Register TailReg = Pop2I->getOperand(0).getReg();
      if (SrcReg != TailReg) {
        ++I;
        continue;
      }

      if (SrcReg == MidReg) {
        ++NumPushPopPopRejectedSameRegs;
        ++I;
        continue;
      }

      if ((Is24BitPush && !Z80::A24RegClass.contains(SrcReg)) ||
          (!Is24BitPush && !Z80::A16RegClass.contains(SrcReg))) {
        ++NumPushPopPopRejectedUnsupportedSrc;
        ++I;
        continue;
      }

      LLVM_DEBUG(dbgs() << "folding push/pop/pop stack shuffle into ex(sp),reg "
                        << "in " << MF.getName() << ":\n";
                 PushMI.dump(); Pop1I->dump(); Pop2I->dump());

      unsigned ExOpc = Is24BitPush ? Z80::EX24sa : Z80::EX16sa;
      MachineInstrBuilder ExMIB =
          BuildMI(MBB, I, PushMI.getDebugLoc(), TII.get(ExOpc), SrcReg)
              .addReg(SrcReg, RegState::Kill);

      if (MachineOperand *DefMO = ExMIB->findRegisterDefOperand(
              SrcReg, /*TRI=*/nullptr, /*isDead=*/false, /*Overlap=*/false))
        DefMO->setIsDead(Pop2I->getOperand(0).isDead());

      Pop2I->eraseFromParent();
      I = MBB.erase(I);
      ++NumPushPopPopFolds;
      Changed = true;
    }
  }

  return Changed;
}

bool Z80MachineLateOptimization::runOnMachineFunction(MachineFunction &MF) {
  bool Changed = false;
  TRI = MF.getSubtarget().getRegisterInfo();
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  MachineRegisterInfo &MRI = MF.getRegInfo();

  Changed |= foldCopyExchangeCopies(MF, TII);
  Changed |= foldPushPopPopStackShuffles(MF, TII);
  Changed |= foldPushPopExchangeCopies(MF, TII);
  Changed |= foldIndexedCounterPeepholes(MF, TII);
  Changed |= foldZeroCompareKnownHelper(MF, TII);

  LivePhysRegs LiveUnits(*TRI);
  for (MachineBasicBlock &MBB : MF) {
    LiveUnits.clear();
    LiveUnits.addLiveIns(MBB);
    clobberAll();
    ZFlagReflectsAZero = false;
    AMirroredInReg = Z80::NoRegister;
    for (MachineBasicBlock::iterator I = MBB.begin(), E = MBB.end(); I != E;) {
      MachineInstrBuilder MIB(MF, I);
      // Keep the upstream v18+ liveness walk semantics here. This may be a bit
      // more conservative than the old fork-local LiveRegUnits::stepForward
      // helper, but it avoids carrying a core CodeGen API fork.
      SmallVector<std::pair<MCPhysReg, const MachineOperand *>, 4> Clobbers;
      LiveUnits.stepForward(*I, Clobbers);
      ++I;

      uint8_t KnownFlagsVal, KnownFlagsMask;
      MCRegister DstReg;
      RegVal DstVal;
      std::tie(KnownFlagsVal, KnownFlagsMask, DstReg, DstVal) =
          getKnownVal(*MIB);

      switch (unsigned Opc = MIB->getOpcode()) {
      case Z80::LD8r0:
      case Z80::LD8ri:
      case Z80::LD8pi:
      case Z80::LD8oi:
      case Z80::ADD8ai:
      case Z80::ADC8ai:
      case Z80::SUB8ai:
      case Z80::SBC8ai:
      case Z80::AND8ai:
      case Z80::XOR8ai:
      case Z80::OR8ai:
      case Z80::CP8ai:
      case Z80::TST8ai: {
        MachineOperand *ImmMO;
        RegVal ImmVal;
        if (Opc == Z80::LD8r0) {
          if (DstReg == Z80::A)
            break;
          ImmMO = nullptr;
          ImmVal = {0, DstReg, *TRI};
        } else {
          ImmMO = &MIB->getOperand(MIB->getNumExplicitOperands() - 1);
          ImmVal = {*ImmMO, Z80::A, *TRI};
        }
        for (MCRegister SrcReg = Z80::NoRegister + 1;
             SrcReg != Z80::NUM_TARGET_REGS; SrcReg = SrcReg + 1) {
          if (!RegVals[SrcReg].matches(ImmVal))
            continue;
          unsigned NewOpc = Z80::INSTRUCTION_LIST_END;
          switch (Opc) {
          default:
            llvm_unreachable("Unexpected opcode");
          case Z80::LD8r0:
          case Z80::LD8ri:
            if (Z80::G8RegClass.contains(DstReg) &&
                Z80::G8RegClass.contains(SrcReg))
              NewOpc = Z80::LD8gg;
            else if (Z80::X8RegClass.contains(DstReg) &&
                     Z80::X8RegClass.contains(SrcReg))
              NewOpc = Z80::LD8xx;
            else if (Z80::Y8RegClass.contains(DstReg) &&
                     Z80::Y8RegClass.contains(SrcReg))
              NewOpc = Z80::LD8yy;
            break;
          case Z80::LD8pi:
            if (Z80::G8RegClass.contains(SrcReg))
              NewOpc = Z80::LD8pg;
            break;
          case Z80::LD8oi:
            if (Z80::G8RegClass.contains(SrcReg))
              NewOpc = Z80::LD8og;
            break;
          case Z80::ADD8ai:
            if (Z80::R8RegClass.contains(SrcReg))
              NewOpc = Z80::ADD8ar;
            break;
          case Z80::ADC8ai:
            if (Z80::R8RegClass.contains(SrcReg))
              NewOpc = Z80::ADC8ar;
            break;
          case Z80::SUB8ai:
            if (Z80::R8RegClass.contains(SrcReg))
              NewOpc = Z80::SUB8ar;
            break;
          case Z80::SBC8ai:
            if (Z80::R8RegClass.contains(SrcReg))
              NewOpc = Z80::SBC8ar;
            break;
          case Z80::AND8ai:
            if (Z80::R8RegClass.contains(SrcReg))
              NewOpc = Z80::AND8ar;
            break;
          case Z80::XOR8ai:
            if (Z80::R8RegClass.contains(SrcReg))
              NewOpc = Z80::XOR8ar;
            break;
          case Z80:: OR8ai:
            if (Z80::R8RegClass.contains(SrcReg))
              NewOpc = Z80:: OR8ar;
            break;
          case Z80:: CP8ai:
            if (Z80::R8RegClass.contains(SrcReg))
              NewOpc = Z80:: CP8ar;
            break;
          case Z80::TST8ai:
            if (Z80::G8RegClass.contains(SrcReg))
              NewOpc = Z80::TST8ag;
            break;
          }
          if (NewOpc == Z80::INSTRUCTION_LIST_END)
            continue;
          MIB->setDesc(TII.get(NewOpc));
          if (ImmMO) {
            ImmMO->ChangeToRegister(SrcReg, /*isDef=*/false, /*isImp=*/false,
                                    /*isKill=*/reuse(SrcReg));
            break;
          }
          assert(Opc == Z80::LD8r0);
          MIB.addReg(SrcReg, getKillRegState(reuse(SrcReg)));
          int ImpDefIdx = MIB->findRegisterDefOperandIdx(Z80::F, nullptr, true);
          if (ImpDefIdx >= 0)
            MIB->removeOperand(ImpDefIdx);
          break;
        }
        break;
      }
      case Z80::LD24r0:
      case Z80::LD24r_1:
        if (DstReg != Z80::UHL || !(KnownFlagsMask & CarryFlag) ||
            ((Opc == Z80::LD24r0) ^ !(KnownFlagsVal & CarryFlag)))
          break;
        LLVM_DEBUG(dbgs() << "Replacing: "; MIB->dump();
                   dbgs() << "     With: ");
        MIB->setDesc(TII.get(Z80::SBC24aa));
        MIB->getOperand(0).setImplicit();
        MIB->getOperand(1).setImplicit();
        MIB.addReg(DstReg, RegState::Implicit | RegState::Undef);
        MIB.addReg(Z80::F, RegState::Implicit | getKillRegState(reuse(Z80::F)));
        break;
      case Z80::SLA8g:
        if (DstReg != Z80::A || !LiveUnits.available(MRI, Z80::F))
          break;
        LLVM_DEBUG(dbgs() << "Replacing: "; MIB->dump();
                   dbgs() << "     With: ");
        MIB->setDesc(TII.get(Z80::ADD8ar));
        MIB->untieRegOperand(1);
        MIB->getOperand(0).setImplicit();
        MIB->getOperand(1).setImplicit();
        MIB.addReg(DstReg, getKillRegState(MIB->getOperand(1).isKill()));
        break;
      case Z80::RLC8g:
      case Z80::RRC8g:
      case Z80:: RL8g:
      case Z80:: RR8g:
        if (DstReg != Z80::A || !LiveUnits.available(MRI, Z80::F))
          break;
        switch (Opc) {
        case Z80::RLC8g: Opc = Z80::RLCA; break;
        case Z80::RRC8g: Opc = Z80::RRCA; break;
        case Z80:: RL8g: Opc = Z80:: RLA; break;
        case Z80:: RR8g: Opc = Z80:: RRA; break;
        }
        LLVM_DEBUG(dbgs() << "Replacing: "; MIB->dump();
                   dbgs() << "     With: ");
        MIB->setDesc(TII.get(Opc));
        MIB->getOperand(0).setImplicit();
        MIB->getOperand(1).setImplicit();
        break;
      // dead OR A,A after SBC A,A
      // after SBC A,A, A is a member of {0x00, 0xFF} and Z = (A == 0). OR A,A also sets
      // Z = (A == 0), so its redundant. tracked via ZFlagReflectsAZero
      case Z80::OR8ar:
        if (MIB->getOperand(0).getReg() == Z80::A && ZFlagReflectsAZero) {
          LLVM_DEBUG(dbgs() << "Erasing redundant OR A,A (after SBC A,A): ";
                     MIB->dump());
          MIB->eraseFromParent();
          Changed = true;
          continue;
        }
        break;
      // redundant copy elimination: track COPY from A and eliminate COPY back if A unchanged
      // this runs before pseudo expansion, so we see COPY, not LD8gg/xx/yy
      case TargetOpcode::COPY: {
        MCRegister CopyDst = MIB->getOperand(0).getReg();
        MCRegister CopySrc = MIB->getOperand(1).getReg();
        // only handle 8-bit A-related copies
        if (!Z80::R8RegClass.contains(CopyDst) || !Z80::R8RegClass.contains(CopySrc))
          break;
        // if copying TO A from the register that mirrors A, the copy is redundant
        if (CopyDst == Z80::A && CopySrc == AMirroredInReg) {
          LLVM_DEBUG(dbgs() << "Erasing redundant copy to A from " 
                            << TRI->getName(CopySrc) << " (A mirrors "
                            << TRI->getName(AMirroredInReg) << "): ";
                     MIB->dump());
          MIB->eraseFromParent();
          Changed = true;
          continue;
        }
        // if copying FROM A to X, record that X now mirrors A
        if (CopySrc == Z80::A) {
          AMirroredInReg = CopyDst;
        }
        break;
      }
      // 8 bit ALU pseudos: ADD8_gisel, SUB8_gisel, etc. have an explicit 8 bit def
      // and implicit def A (because Z80 ALU ops always write to A)
      // after these instructions, A and the dest register hold the same value
      case Z80::ADD8_gisel:
      case Z80::SUB8_gisel:
      case Z80::AND8_gisel:
      case Z80::OR8_gisel:
      case Z80::XOR8_gisel: {
        MCRegister ExplicitDst = MIB->getOperand(0).getReg();
        // after ALU pseudo, explicit dest and A have the same value if dest is not A itself, record that dest mirrors A
        if (ExplicitDst != Z80::A) {
          AMirroredInReg = ExplicitDst;
        }
        break;
      }
      case Z80::Sub16ao:
      case Z80::Sub24ao:
      case Z80::Cmp16ao:
      case Z80::Cmp24ao: {
        if (!(~KnownFlagsVal & KnownFlagsMask & CarryFlag))
          break;
        unsigned SubOpc, AddOpc;
        switch (Opc) {
        case Z80::Sub16ao:
        case Z80::Cmp16ao:
          SubOpc = Z80::SBC16ao;
          AddOpc = Z80::ADD16ao;
          break;
        case Z80::Sub24ao:
        case Z80::Cmp24ao:
          SubOpc = Z80::SBC24ao;
          AddOpc = Z80::ADD24ao;
          break;
        }
        LLVM_DEBUG(dbgs() << "Replacing: "; MIB->dump();
                   dbgs() << "     With: ");
        MIB->setDesc(TII.get(SubOpc));
        MIB.addReg(Z80::F, RegState::Implicit | getKillRegState(reuse(Z80::F)));
        switch (Opc) {
        case Z80::Cmp16ao:
        case Z80::Cmp24ao:
          MIB.addReg(DstReg, RegState::ImplicitDefine);
          if (LiveUnits.available(MRI, DstReg))
            break;
          MachineOperand &SrcMO = MIB->getOperand(0);
          MIB = BuildMI(MBB, I, MIB->getDebugLoc(), TII.get(AddOpc), DstReg)
                    .addReg(DstReg).add(SrcMO);
          SrcMO.setIsKill(false);
          break;
        }
        break;
      }
      }

      if (RegVals[DstReg].matches(DstVal)) {
        LLVM_DEBUG(dbgs() << "Erasing redundant: "; MIB->dump());
        MIB->eraseFromParent();
        reuse(DstReg);
        Changed = true;
        continue;
      }

      bool NeedInc = RegVals[DstReg].matches(DstVal, -1);
      if (NeedInc || RegVals[DstReg].matches(DstVal, +1)) {
        const TargetRegisterClass *DstRC = nullptr;
        for (const TargetRegisterClass *C : TRI->regclasses())
          if (C->contains(DstReg) && (!DstRC || DstRC->hasSubClass(C)))
            DstRC = C;
        unsigned NewOpc = Z80::INSTRUCTION_LIST_END;
        if (DstRC) {
          switch (TRI->getRegSizeInBits(*DstRC)) {
          default:
            break; // Unknown width, skip optimization
          case 8:
            if (!LiveUnits.available(MRI, Z80::F))
              break;
            NewOpc = NeedInc ? Z80::INC8r : Z80::DEC8r;
            break;
          case 16:
            NewOpc = NeedInc ? Z80::INC16r : Z80::DEC16r;
            break;
          case 24:
            NewOpc = NeedInc ? Z80::INC24r : Z80::DEC24r;
            break;
          }
        }
        if (NewOpc != Z80::INSTRUCTION_LIST_END) {
          LLVM_DEBUG(dbgs() << "Replacing: "; MIB->dump();
                     dbgs() << "     With: ");
          MIB->setDesc(TII.get(NewOpc));
          MIB->removeOperand(1);
          MIB.addReg(DstReg, getKillRegState(reuse(DstReg)));
          MIB->addImplicitDefUseOperands(MF);
        }
      }

      // Update killed uses after reuse and before clobbering defs.
      updateDeadOrKilledBy(MIB, &MachineOperand::isKill);

      // Get KnownFlags before clobbering defs.
      KnownFlags KnownFlags =
          getKnownFlags(*MIB, KnownFlagsVal, KnownFlagsMask);

      // Clobber defs and track ZFlagReflectsAZero
      bool ClobberedA = false, ClobberedF = false;
      for (MachineOperand &MO : MIB->operands()) {
        if (MO.isReg() && MO.isDef() &&
            !(DstReg.isValid() && MO.isImplicit() &&
              TRI->isSuperRegister(DstReg, MO.getReg()))) {
          clobber<MCRegAliasIterator>(MO.getReg(), true);
          // check if A or F is clobbered
          for (MCRegAliasIterator AI(MO.getReg(), TRI, true); AI.isValid(); ++AI) {
            if (*AI == Z80::A) ClobberedA = true;
            if (*AI == Z80::F) ClobberedF = true;
          }
        } else if (MO.isRegMask()) {
          for (MCRegister Reg = Z80::NoRegister + 1;
               Reg != Z80::NUM_TARGET_REGS; Reg = Reg + 1)
            if (MO.clobbersPhysReg(Reg))
              assign(Reg);
          if (MO.clobbersPhysReg(Z80::A)) ClobberedA = true;
          if (MO.clobbersPhysReg(Z80::F)) ClobberedF = true;
        }
      }
      
      // SBC A,A sets Z=(A==0). INC/DEC A preserve this. Track to eliminate redundant OR A,A
      unsigned Opc = MIB->getOpcode();
      if (Opc == Z80::SBC8ar && MIB->getOperand(0).getReg() == Z80::A) {
        ZFlagReflectsAZero = true;
      } else if (ZFlagReflectsAZero && 
                 (Opc == Z80::INC8r || Opc == Z80::DEC8r) &&
                 MIB->getOperand(0).getReg() == Z80::A) {
        // INC/DEC A preserves Z=(A==0)
      } else if (ClobberedA || ClobberedF) {
        ZFlagReflectsAZero = false;
      }

      // invalidate A<->reg mirroring if A or mirrored reg is clobbered (except by tracked ops)
      if (ClobberedA) {
        bool isCopyFromA = Opc == TargetOpcode::COPY &&
                           MIB->getOperand(1).getReg() == Z80::A;
        bool isALUPseudo = (Opc == Z80::ADD8_gisel || Opc == Z80::SUB8_gisel ||
                            Opc == Z80::AND8_gisel || Opc == Z80::OR8_gisel ||
                            Opc == Z80::XOR8_gisel);
        if (!isCopyFromA && !isALUPseudo)
          AMirroredInReg = Z80::NoRegister;
      }
      if (AMirroredInReg != Z80::NoRegister) {
        bool isALUPseudoOrCopy = (Opc == Z80::ADD8_gisel || Opc == Z80::SUB8_gisel ||
                                  Opc == Z80::AND8_gisel || Opc == Z80::OR8_gisel ||
                                  Opc == Z80::XOR8_gisel || Opc == TargetOpcode::COPY);
        if (!isALUPseudoOrCopy) {
          for (MachineOperand &MO : MIB->operands()) {
            if (MO.isReg() && MO.isDef()) {
              for (MCRegAliasIterator AI(MO.getReg(), TRI, true); AI.isValid(); ++AI) {
                if (*AI == AMirroredInReg) {
                  AMirroredInReg = Z80::NoRegister;
                  break;
                }
              }
            }
          }
        }
      }

      // Apply KnownFlags after clobbering defs.
      if (KnownFlags)
        assign(Z80::F, KnownFlags.getKnownVal(KnownFlagsVal),
               KnownFlags.getKnownMask(KnownFlagsMask));

      // Apply known val after clobbering defs.
      if (DstVal.valid()) {
        clobber<MCSuperRegIterator>(DstReg, false);
        RegVals[DstReg] = DstVal;
        for (MCSubRegIndexIterator SRII(DstReg, TRI); SRII.isValid(); ++SRII)
          assign(SRII.getSubReg(), DstVal, SRII.getSubRegIndex());
      }

      // Update Dead Defs after reuse and after clobbering defs.
      updateDeadOrKilledBy(MIB, &MachineOperand::isDead);

      debug(*MIB);
    }
  }
  return Changed;
}

char Z80MachineLateOptimization::ID = 0;
INITIALIZE_PASS(Z80MachineLateOptimization, DEBUG_TYPE,
                "Optimize Z80 machine instrs after regselect", false, false)

FunctionPass *llvm::createZ80MachineLateOptimizationPass() {
  return new Z80MachineLateOptimization();
}
