//===-- Z80ISelLowering.cpp - Z80 DAG Lowering Implementation -------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the interfaces that Z80 uses to lower LLVM code into a
// selection DAG.
//
//===----------------------------------------------------------------------===//

#include "Z80ISelLowering.h"
#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "Z80TargetMachine.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/Support/KnownBits.h"
using namespace llvm;

#define DEBUG_TYPE "z80-isel"

Z80TargetLowering::Z80TargetLowering(const Z80TargetMachine &TM,
                                     const Z80Subtarget &STI)
    : TargetLowering(TM, STI), Subtarget(STI) {
  bool Is24Bit = Subtarget.is24Bit();

  setSchedulingPreference(Sched::RegPressure);
  setLibcallImpl(RTLIB::MULO_I64, RTLIB::impl___mulodi4);


  // Set up the register classes.
  addRegisterClass(MVT::i8, &Z80::R8RegClass);
  addRegisterClass(MVT::i16, &Z80::R16RegClass);
  if (Is24Bit)
    addRegisterClass(MVT::i24, &Z80::R24RegClass);

  setStackPointerRegisterToSaveRestore(Is24Bit ? Z80::SPL : Z80::SPS);

  // Compute derived properties from the register classes
  computeRegisterProperties(STI.getRegisterInfo());
}

unsigned Z80TargetLowering::getJumpTableEncoding() const {
  return MachineJumpTableInfo::EK_BlockAddress;
}

/// Return true if the target has native support for the specified value type
/// and it is 'desirable' to use the type for the given node type. e.g. On ez80
/// i16 is legal, but undesirable since i16 instruction encodings are longer and
/// slower.
bool Z80TargetLowering::isTypeDesirableForOp(unsigned Opc, EVT VT) const {
  // Check if it is a legal type.
  if (!TargetLowering::isTypeDesirableForOp(Opc, VT))
    return false;
  if (Subtarget.is16Bit())
    return true;

  switch (Opc) {
  default:
  case ISD::SIGN_EXTEND:
  case ISD::ZERO_EXTEND:
  case ISD::ANY_EXTEND:
    return true;
  case ISD::LOAD:
  case ISD::STORE:
  case ISD::ADD:
  case ISD::SUB:
    return VT != MVT::i16;
  case ISD::MUL:
  case ISD::AND:
  case ISD::OR:
  case ISD::XOR:
  case ISD::SHL:
  case ISD::SRA:
  case ISD::SRL:
    return VT != MVT::i24;
  }
}

/// Return true if x op y -> (SrcVT)((DstVT)x op (DstVT)y) is beneficial.
bool Z80TargetLowering::isDesirableToShrinkOp(unsigned Opc, EVT SrcVT,
                                              EVT DstVT) const {
  if (!isTypeLegal(DstVT))
    return false;
  switch (Opc) {
  default:
    return false;
  case ISD::ADD:
  case ISD::SUB:
  case ISD::ADDC:
  case ISD::SUBC:
  case ISD::ADDE:
  case ISD::SUBE:
    // These require a .sis suffix for i24 -> i16
    return DstVT != MVT::i16 || Subtarget.is16Bit();
  case ISD::MUL:
  case ISD::AND:
  case ISD::OR:
  case ISD::XOR:
  case ISD::SHL:
  case ISD::SRA:
  case ISD::SRL:
  case ISD::ROTL:
  case ISD::ROTR:
    // These are more expensive on larger types, so always shrink.
    return true;
  }
}

/// This method query the target whether it is beneficial for dag combiner to
/// promote the specified node. If true, it should return the desired promotion
/// type by reference.
bool Z80TargetLowering::IsDesirableToPromoteOp(SDValue Op, EVT &PVT) const {
  if (isDesirableToShrinkOp(Op.getOpcode(), MVT::i24, Op.getValueType()))
      return false;
  PVT = MVT::i24;
  return true;
}

MachineBasicBlock *
Z80TargetLowering::EmitInstrWithCustomInserter(MachineInstr &MI,
                                               MachineBasicBlock *BB) const {
  switch (MI.getOpcode()) {
  default: llvm_unreachable("Unexpected instr type to insert");
  case Z80::SetCC:
  case Z80::Select8:
  case Z80::Select16:
  case Z80::Select24:
    return EmitLoweredSelect(MI, BB);
  case Z80::SExt8:
  case Z80::SExt16:
  case Z80::SExt24:
    return EmitLoweredSExt(MI, BB);
  case Z80::LDR16:
  case Z80::LDR24:
    return EmitLoweredMemMove(MI, BB);
  }
}

void Z80TargetLowering::AdjustInstrPostInstrSelection(MachineInstr &MI,
                                                      SDNode *Node) const {
  switch (MI.getOpcode()) {
  default: llvm_unreachable("Unexpected instr type to insert");
  case Z80::ADJCALLSTACKUP16:
  case Z80::ADJCALLSTACKUP24:
  case Z80::ADJCALLSTACKDOWN16:
  case Z80::ADJCALLSTACKDOWN24:
    return AdjustAdjCallStack(MI);
  }
}

void Z80TargetLowering::AdjustAdjCallStack(MachineInstr &MI) const {
  bool Is24Bit = MI.getOpcode() == Z80::ADJCALLSTACKUP24 ||
                 MI.getOpcode() == Z80::ADJCALLSTACKDOWN24;
  assert((Is24Bit || MI.getOpcode() == Z80::ADJCALLSTACKUP16 ||
                     MI.getOpcode() == Z80::ADJCALLSTACKDOWN16) &&
         "Unexpected opcode");
  MachineRegisterInfo &MRI = MI.getMF()->getRegInfo();
  unsigned Reg = MRI.createVirtualRegister(Is24Bit ? &Z80::A24RegClass
                                                   : &Z80::A16RegClass);
  MachineInstrBuilder(*MI.getMF(), MI)
      .addReg(Reg, RegState::ImplicitDefine | RegState::Dead);
  LLVM_DEBUG(MI.dump());
}

static bool isSinkableSelectArmInstr(const MachineInstr &MI) {
  if (MI.isPHI() || MI.isTerminator() || MI.isInlineAsm())
    return false;
  if (MI.isCall() || MI.mayLoadOrStore() || MI.hasUnmodeledSideEffects())
    return false;
  return MI.getNumExplicitDefs() == 1;
}

static bool isBeforeInBlock(const MachineInstr &A, const MachineInstr &B) {
  if (A.getParent() != B.getParent())
    return false;
  const MachineBasicBlock *MBB = A.getParent();
  for (const MachineInstr &MI : *MBB) {
    if (&MI == &A)
      return true;
    if (&MI == &B)
      return false;
  }
  return false;
}

static bool sinkSelectTrueArmComputation(MachineInstr &SelectMI,
                                         MachineBasicBlock *ThisMBB,
                                         MachineBasicBlock *TrueMBB,
                                         Register FalseReg, Register TrueReg,
                                         MachineRegisterInfo &MRI) {
  if (!FalseReg.isVirtual() || !TrueReg.isVirtual())
    return false;
  if (!MRI.hasOneNonDBGUse(TrueReg))
    return false;

  MachineInstr *TrueDef = MRI.getVRegDef(TrueReg);
  if (!TrueDef || TrueDef->getParent() != ThisMBB ||
      !isBeforeInBlock(*TrueDef, SelectMI) ||
      !isSinkableSelectArmInstr(*TrueDef))
    return false;

  MachineInstr *CopyFromFalse = nullptr;
  bool DependsOnFalse = false;
  for (const MachineOperand &MO : TrueDef->explicit_uses()) {
    if (!MO.isReg())
      continue;
    Register UseReg = MO.getReg();
    if (!UseReg || !UseReg.isVirtual())
      continue;
    if (UseReg == FalseReg) {
      DependsOnFalse = true;
      continue;
    }
    MachineInstr *DefI = MRI.getVRegDef(UseReg);
    if (!DefI || DefI->getParent() != ThisMBB ||
        DefI->getOpcode() != TargetOpcode::COPY ||
        !isBeforeInBlock(*DefI, *TrueDef) || !MRI.hasOneNonDBGUse(UseReg))
      continue;
    if (!DefI->getOperand(1).isReg() || DefI->getOperand(1).getReg() != FalseReg)
      continue;
    DependsOnFalse = true;
    CopyFromFalse = DefI;
  }

  if (!DependsOnFalse)
    return false;

  if (CopyFromFalse)
    TrueMBB->splice(TrueMBB->end(), ThisMBB,
                    MachineBasicBlock::iterator(CopyFromFalse));
  TrueMBB->splice(TrueMBB->end(), ThisMBB, MachineBasicBlock::iterator(TrueDef));
  return true;
}

MachineBasicBlock *
Z80TargetLowering::EmitLoweredSelect(MachineInstr &MI,
                                     MachineBasicBlock *BB) const {
  MachineRegisterInfo &MRI = BB->getParent()->getRegInfo();
  const TargetInstrInfo *TII = Subtarget.getInstrInfo();
  DebugLoc DL = MI.getDebugLoc();

  Register FalseReg, TrueReg;
  int CCIdx;
  if (MI.getOpcode() == Z80::SetCC) {
    FalseReg = MRI.createVirtualRegister(&Z80::R8RegClass);
    BuildMI(BB, DL, TII->get(Z80::LD8ri), FalseReg).addImm(0);
    TrueReg = MRI.createVirtualRegister(&Z80::R8RegClass);
    BuildMI(BB, DL, TII->get(Z80::LD8ri), TrueReg).addImm(1);
    CCIdx = 1;
  } else {
    FalseReg = MI.getOperand(1).getReg();
    TrueReg = MI.getOperand(2).getReg();
    CCIdx = 3;
  }

  // To "insert" a SELECT_CC instruction, we actually have to insert the
  // diamond control-flow pattern.  The incoming instruction knows the
  // destination vreg to set, the condition code register to branch on, the
  // true/false values to select between, and a branch opcode to use.
  const BasicBlock *LLVM_BB = BB->getBasicBlock();
  MachineFunction::iterator I = ++BB->getIterator();

  //  thisMBB:
  //  ...
  //   %FalseVal = ...
  //   cmpTY ccX, r1, r2
  //   bCC copy1MBB
  //   fallthrough --> copy0MBB
  MachineBasicBlock *thisMBB = BB;
  MachineFunction *F = BB->getParent();
  MachineBasicBlock *copy0MBB = F->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *copy1MBB = F->CreateMachineBasicBlock(LLVM_BB);
  F->insert(I, copy0MBB);
  F->insert(I, copy1MBB);

  if (MI.getOpcode() != Z80::SetCC)
    (void)sinkSelectTrueArmComputation(MI, BB, copy0MBB, FalseReg, TrueReg,
                                       MRI);

  // Update machine-CFG edges by transferring all successors of the current
  // block to the new block which will contain the Phi node for the select.
  copy1MBB->splice(copy1MBB->begin(), BB,
                   std::next(MachineBasicBlock::iterator(MI)), BB->end());
  copy1MBB->transferSuccessorsAndUpdatePHIs(BB);
  // Next, add the true and fallthrough blocks as its successors.
  BB->addSuccessor(copy0MBB);
  BB->addSuccessor(copy1MBB);

  BuildMI(BB, DL, TII->get(Z80::JQCC)).addMBB(copy1MBB)
      .add(MI.getOperand(CCIdx));

  //  copy0MBB:
  //   %TrueVal = ...
  //   # fallthrough to copy1MBB
  BB = copy0MBB;

  // Update machine-CFG edges
  BB->addSuccessor(copy1MBB);

  //  copy1MBB:
  //   %Result = phi [ %FalseValue, copy0MBB ], [ %TrueValue, thisMBB ]
  //  ...
  BB = copy1MBB;
  BuildMI(*BB, BB->begin(), DL, TII->get(Z80::PHI), MI.getOperand(0).getReg())
      .addReg(FalseReg).addMBB(thisMBB).addReg(TrueReg).addMBB(copy0MBB);

  MI.eraseFromParent();   // The pseudo instruction is gone now.
  LLVM_DEBUG(F->dump());
  return BB;
}

MachineBasicBlock *Z80TargetLowering::EmitLoweredSExt(
    MachineInstr &MI, MachineBasicBlock *BB) const {
  const TargetInstrInfo *TII = Subtarget.getInstrInfo();
  DebugLoc DL = MI.getDebugLoc();
  unsigned Opc, Reg;
  switch (MI.getOpcode()) {
  default: llvm_unreachable("Unexpected opcode");
  case Z80::SExt8:
    Opc = Z80::SBC8ar;
    Reg = Z80::A;
    break;
  case Z80::SExt16:
    Opc = Z80::SBC16aa;
    Reg = Z80::HL;
    break;
  case Z80::SExt24:
    Opc = Z80::SBC24aa;
    Reg = Z80::UHL;
    break;
  }
  MachineInstrBuilder MIB = BuildMI(*BB, MI, DL, TII->get(Opc));
  MIB->findRegisterUseOperand(Reg, nullptr)->setIsUndef();
  if (Reg == Z80::A)
    MIB.addReg(Reg, RegState::Undef);
  MI.eraseFromParent();
  return BB;
}

MachineBasicBlock *
Z80TargetLowering::EmitLoweredMemMove(MachineInstr &MI,
                                      MachineBasicBlock *BB) const {
  bool Is24Bit = MI.getOpcode() == Z80::LDR24;
  Register DE = Is24Bit ? Z80::UDE : Z80::DE;
  Register HL = Is24Bit ? Z80::UHL : Z80::HL;
  Register BC = Is24Bit ? Z80::UBC : Z80::BC;
  Register PhysRegs[] = { DE, HL, BC };
  Register VirtRegs[3] = {};
  assert((Is24Bit || MI.getOpcode() == Z80::LDR16) && "Unexpected opcode");

  const TargetRegisterInfo *TRI = Subtarget.getRegisterInfo();
  const TargetInstrInfo *TII = Subtarget.getInstrInfo();
  DebugLoc DL = MI.getDebugLoc();

  // A memmove needs to choose between a forward and backwards copy.
  const BasicBlock *LLVM_BB = BB->getBasicBlock();
  MachineFunction::iterator I = ++BB->getIterator();

  MachineFunction *F = BB->getParent();
  MachineRegisterInfo &MRI = F->getRegInfo();
  MachineBasicBlock *LDIR_BB = F->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *LDDR_BB = F->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *NextBB = F->CreateMachineBasicBlock(LLVM_BB);
  for (auto *MBB : {LDIR_BB, LDDR_BB}) {
    F->insert(I, MBB);
    for (auto LiveIn : PhysRegs)
      MBB->addLiveIn(LiveIn);
  }
  F->insert(I, NextBB);

  // Update machine-CFG edges by transferring all successors of the current
  // block to the new block.
  NextBB->splice(NextBB->begin(), BB,
                 std::next(MachineBasicBlock::iterator(MI)), BB->end());
  NextBB->transferSuccessorsAndUpdatePHIs(BB);

  // BB:
  //   COPY DE, DstReg
  //   COPY HL, SrcReg
  //   COPY BC, LenReg
  //   SUB HL, DE   (or a, a; sbc hl, de - sets flags: C if HL < DE)
  //   <save to virtregs>
  //   JP C, LDDR_BB
  //   fallthrough --> LDIR_BB
  for (int J = 0; J != 3; ++J)
    BuildMI(BB, DL, TII->get(Z80::COPY), PhysRegs[J])
        .add(MI.getOperand(J));
  // sub24ao/Sub16ao: HL = HL - DE, sets flags
  // this always uses valid registers (HL and DE) which avoiods sbc hl, iy
  BuildMI(BB, DL, TII->get(Is24Bit ? Z80::Sub24ao : Z80::Sub16ao))
      .addReg(DE);
  // preserve physical registers for the successors
  for (int J = 0; J != 3; ++J) {
    VirtRegs[J] = MRI.createVirtualRegister(TRI->getRegClass(PhysRegs[J]));
    BuildMI(BB, DL, TII->get(Z80::COPY), VirtRegs[J]).addReg(PhysRegs[J]);
  }
  BuildMI(BB, DL, TII->get(Z80::JQCC)).addMBB(LDDR_BB)
      .addImm(Z80::COND_C);
  // Next, add the LDIR and LDDR blocks as its successors.
  BB->addSuccessor(LDIR_BB);
  BB->addSuccessor(LDDR_BB);

  // LDIR_BB:
  //   ADD HL, DE  (restore HL since subtraction destroyed it)
  //   LDIR
  //   JP NextBB
  for (int J = 0; J != 3; ++J)
    BuildMI(LDIR_BB, DL, TII->get(Z80::COPY), PhysRegs[J])
        .addReg(VirtRegs[J]);
  // restore HL = HL + DE (since we computed HL - DE earlier)
  BuildMI(LDIR_BB, DL, TII->get(Is24Bit ? Z80::ADD24ao : Z80::ADD16ao), HL)
      .addReg(HL).addReg(DE);
  BuildMI(LDIR_BB, DL, TII->get(Is24Bit ? Z80::LDIR24 : Z80::LDIR16));
  BuildMI(LDIR_BB, DL, TII->get(Z80::JQ)).addMBB(NextBB);
  // Update machine-CFG edges
  LDIR_BB->addSuccessor(NextBB);

  // LDDR_BB:
  //   (virtregs have HL = HL - DE after subtraction, which is negative)
  //   EX DE, HL     (DE = HL - DE = negative offset, HL = original DE)
  //   ADD HL, BC    (HL = DE + BC = DE + len)
  //   DEC HL        (HL = DE + len - 1 = end of dest)
  //   EX DE, HL     (DE = end of dest, HL = negative offset)
  //   ADD HL, DE    (HL = negative offset + end of dest = SrcOrig - DstOrig + DstEnd
  //                    = SrcOrig + len - 1 = end of source)
  //   LDDR
  // # Fallthrough to Next MBB
  for (int J = 0; J != 3; ++J)
    BuildMI(LDDR_BB, DL, TII->get(Z80::COPY), PhysRegs[J])
        .addReg(VirtRegs[J]);
  BuildMI(LDDR_BB, DL, TII->get(Is24Bit ? Z80::EX24DE : Z80::EX16DE));
  BuildMI(LDDR_BB, DL, TII->get(Is24Bit ? Z80::ADD24ao : Z80::ADD16ao), HL)
      .addReg(HL).addReg(BC);
  BuildMI(LDDR_BB, DL, TII->get(Is24Bit ? Z80::DEC24r : Z80::DEC16r), HL)
      .addReg(HL);
  BuildMI(LDDR_BB, DL, TII->get(Is24Bit ? Z80::EX24DE : Z80::EX16DE));
  BuildMI(LDDR_BB, DL, TII->get(Is24Bit ? Z80::ADD24ao : Z80::ADD16ao), HL)
      .addReg(HL).addReg(DE);
  BuildMI(LDDR_BB, DL, TII->get(Is24Bit ? Z80::LDDR24 : Z80::LDDR16));
  LDDR_BB->addSuccessor(NextBB);

  // replace virtual register usages with the corresponding physical
  // registers to ensure no-op copies.
  for (int J = 0; J != 3; ++J)
    MRI.replaceRegWith(VirtRegs[J], PhysRegs[J]);

  MI.eraseFromParent();   // The pseudo instruction is gone now.
  LLVM_DEBUG(F->dump());
  return NextBB;
}


void Z80TargetLowering::computeKnownBitsForTargetInstr(
    GISelValueTracking &Analysis, Register Reg, KnownBits &Known,
    const APInt &DemandedElts, const MachineRegisterInfo &MRI,
    unsigned Depth) const {
  const MachineInstr &MI = *MRI.getVRegDef(Reg);
  switch (MI.getOpcode()) {
  case Z80::SetCC:
    Known.Zero.setBitsFrom(1);
    break;
  default:
    TargetLowering::computeKnownBitsForTargetInstr(Analysis, Reg, Known,
                                                   DemandedElts, MRI, Depth);
    break;
  }
}

/// HandleByVal - Target-specific cleanup for ByVal support.
void Z80TargetLowering::HandleByVal(CCState *State, unsigned &Size,
                                    Align Alignment) const {
  // Round up to a multiple of the stack slot size.
  Size = alignTo(Size, Subtarget.is24Bit() ? 3 : 2);
}

//===----------------------------------------------------------------------===//
//                           Z80 Inline Assembly Support
//===----------------------------------------------------------------------===//

/// Given a constraint letter, return the type of constraint for this target.
TargetLowering::ConstraintType
Z80TargetLowering::getConstraintType(StringRef Constraint) const {
  if (Constraint.size() == 1)
    switch (Constraint[0]) {
    case 'I':
    case 'J':
    case 'M':
    case 'N':
    case 'O':
      return C_Immediate;
    case 'R':
      return C_RegisterClass;
    }
  else if (Z80::parseConstraintCode(Constraint) != Z80::COND_INVALID)
    return C_Other;
  return TargetLowering::getConstraintType(Constraint);
}

std::pair<unsigned, const TargetRegisterClass *>
Z80TargetLowering::getRegForInlineAsmConstraint(const TargetRegisterInfo *TRI,
                                                StringRef Constraint,
                                                MVT VT) const {
  if (Constraint.size() == 1)
    switch (Constraint[0]) {
    case 'r':
      if (VT == MVT::i8 || VT == MVT::i8)
        return std::make_pair(Z80::NoRegister, &Z80::G8RegClass);
      if (VT == MVT::i16)
        return std::make_pair(Z80::NoRegister, &Z80::G16RegClass);
      if (VT == MVT::i24)
        return std::make_pair(Z80::NoRegister, &Z80::G24RegClass);
      break;

    case 'R':
      if (VT == MVT::i8 || VT == MVT::i8)
        return std::make_pair(Z80::NoRegister, &Z80::R8RegClass);
      if (VT == MVT::i16)
        return std::make_pair(Z80::NoRegister, &Z80::R16RegClass);
      if (VT == MVT::i24)
        return std::make_pair(Z80::NoRegister, &Z80::R24RegClass);
      break;
    }

  if (Z80::parseConstraintCode(Constraint) != Z80::COND_INVALID)
    return std::make_pair(Z80::F, &Z80::F8RegClass);

  // Use the default implementation in TargetLowering to convert the register
  // constraint into a member of a register class.
  auto Res = TargetLowering::getRegForInlineAsmConstraint(TRI, Constraint, VT);

  if (!Res.second) {
    if (Constraint.equals_insensitive("{f}"))
      return std::make_pair(Z80::F, &Z80::F8RegClass);

    return Res;
  }

  // Otherwise, check to see if this is a register class of the wrong value
  // type.  For example, we want to map "{l},i16" -> {hl}.
  // MVT::Other is used to specify clobber names.
  if (TRI->isTypeLegalForClass(*Res.second, VT) || VT == MVT::Other)
    return Res;   // Correct type already, nothing to do.

  if (VT == MVT::i8)
    Res.second = &Z80::R8RegClass;
  else if (VT == MVT::i16)
    Res.second = &Z80::R16RegClass;
  else if (VT == MVT::i24)
    Res.second = &Z80::R24RegClass;

  for (unsigned SubIdx : {Z80::sub_low, Z80::sub_short})
    if (MCRegister SuperReg =
            TRI->getMatchingSuperReg(Res.first, SubIdx, Res.second))
      Res.first = SuperReg;

  if (Res.second && Res.second->contains(Res.first))
    return Res;

  return std::make_pair(Z80::NoRegister, nullptr);
}

InlineAsm::ConstraintCode
Z80TargetLowering::getInlineAsmMemConstraint(StringRef Constraint) const {
  if (Constraint.size() == 1)
    switch (Constraint[0]) {
    case 'V':
      return InlineAsm::ConstraintCode::v;
    case 'o':
      return InlineAsm::ConstraintCode::o;
    case 'X':
      return InlineAsm::ConstraintCode::X;
    }
  return TargetLowering::getInlineAsmMemConstraint(Constraint);
}
