//===-- Z80RegisterInfo.cpp - Z80 Register Information --------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the Z80 implementation of the TargetRegisterInfo class.
//
//===----------------------------------------------------------------------===//

#include "Z80RegisterInfo.h"
#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "Z80FrameLowering.h"
#include "Z80MachineFunctionInfo.h"
#include "Z80Subtarget.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/Support/Debug.h"
using namespace llvm;

#define DEBUG_TYPE "z80reginfo"

#define GET_REGINFO_TARGET_DESC
#include "Z80GenRegisterInfo.inc"

Z80RegisterInfo::Z80RegisterInfo(const Triple &TT)
    : Z80GenRegisterInfo(0, 0, 0, Z80::PC) {
  // Cache some information.
  Is24Bit = !TT.isArch16Bit() && TT.getEnvironment() != Triple::CODE16;
  StackPtr = Is24Bit ? Z80::SPL : Z80::SPS;
}

unsigned Z80RegisterInfo::getSpillSize(const TargetRegisterClass &RC) const {
  unsigned SpillSize = TargetRegisterInfo::getSpillSize(RC);
  if (Is24Bit && SpillSize == 2)
    SpillSize = 3;
  return SpillSize;
}

const TargetRegisterClass *
Z80RegisterInfo::getPointerRegClass(const MachineFunction &MF,
                                    unsigned Kind) const {
  switch (Kind) {
  default: llvm_unreachable("Unexpected Kind!");
  case 0: return Is24Bit ? &Z80::R24RegClass : &Z80::R16RegClass;
  case 1: return Is24Bit ? &Z80::G24RegClass : &Z80::G16RegClass;
  case 2: return Is24Bit ? &Z80::A24RegClass : &Z80::A16RegClass;
  case 3: return Is24Bit ? &Z80::I24RegClass : &Z80::I16RegClass;
  }
}

const TargetRegisterClass *
Z80RegisterInfo::getPointerRegClassForConstraint(const MachineFunction &MF,
                                                 unsigned Constraint) const {
  unsigned Kind;
  switch (Constraint) {
  default: llvm_unreachable("Unexpected Constraint!");
  case InlineAsm::Constraint_V: Kind = 1; break;
  case InlineAsm::Constraint_m: Kind = 2; break;
  case InlineAsm::Constraint_o: Kind = 3; break;
  }
  return getPointerRegClass(MF, Kind);
}

const TargetRegisterClass *
Z80RegisterInfo::getLargestLegalSuperClass(const TargetRegisterClass *RC,
                                           const MachineFunction &) const {
  const TargetRegisterClass *Super = RC;
  TargetRegisterClass::sc_iterator I = RC->getSuperClasses();
  do {
    switch (Super->getID()) {
    case Z80::R8RegClassID:
    case Z80::R16RegClassID:
    case Z80::R24RegClassID:
      return Super;
    }
    Super = *I++;
  } while (Super);
  return RC;
}

unsigned Z80RegisterInfo::getRegPressureLimit(const TargetRegisterClass *RC,
                                              MachineFunction &MF) const {
  return 3;

  switch (RC->getID()) {
  default:
    return 0;
  case Z80::R16RegClassID:
  case Z80::R24RegClassID:
    return 2;
  }
}

const MCPhysReg *
Z80RegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const {
  switch (MF->getFunction().getCallingConv()) {
  default: llvm_unreachable("Unsupported calling convention");
  case CallingConv::C:
  case CallingConv::Fast:
    return Is24Bit ? CSR_EZ80_C_SaveList : CSR_Z80_C_SaveList;
  case CallingConv::PreserveAll:
  case CallingConv::Z80_LibCall:
  case CallingConv::Z80_LibCall_AB:
  case CallingConv::Z80_LibCall_AC:
  case CallingConv::Z80_LibCall_BC:
  case CallingConv::Z80_LibCall_L:
  case CallingConv::Z80_LibCall_F:
    return Is24Bit ? CSR_EZ80_AllRegs_SaveList : CSR_Z80_AllRegs_SaveList;
  case CallingConv::Z80_LibCall_16:
    return Is24Bit ? CSR_EZ80_AllRegs16_SaveList : CSR_Z80_AllRegs_SaveList;
  case CallingConv::Z80_TIFlags:
    return Is24Bit ? CSR_EZ80_TIFlags_SaveList : CSR_Z80_TIFlags_SaveList;
  }
}

const uint32_t *
Z80RegisterInfo::getCallPreservedMask(const MachineFunction &MF,
                                      CallingConv::ID CC) const {
  switch (CC) {
  default: llvm_unreachable("Unsupported calling convention");
  case CallingConv::C:
  case CallingConv::Fast:
    return Is24Bit ? CSR_EZ80_C_RegMask : CSR_Z80_C_RegMask;
  case CallingConv::PreserveAll:
  case CallingConv::Z80_LibCall:
  case CallingConv::Z80_LibCall_AB:
  case CallingConv::Z80_LibCall_AC:
  case CallingConv::Z80_LibCall_BC:
  case CallingConv::Z80_LibCall_L:
  case CallingConv::Z80_LibCall_F:
    return Is24Bit ? CSR_EZ80_AllRegs_RegMask : CSR_Z80_AllRegs_RegMask;
  case CallingConv::Z80_LibCall_16:
    return Is24Bit ? CSR_EZ80_AllRegs16_RegMask : CSR_Z80_AllRegs_RegMask;
  case CallingConv::Z80_TIFlags:
    return Is24Bit ? CSR_EZ80_TIFlags_RegMask : CSR_Z80_TIFlags_RegMask;
  }
}
const uint32_t *Z80RegisterInfo::getNoPreservedMask() const {
  return CSR_NoRegs_RegMask;
}

BitVector Z80RegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  BitVector Reserved(getNumRegs());

  // Set the stack-pointer registers as reserved.
  Reserved.set(Z80::SPS);
  Reserved.set(Z80::SPL);

  // Set the program-counter register as reserved.
  Reserved.set(getProgramCounter());

  // Set the frame-pointer register and its aliases as reserved if needed.
  for (Register Reg :
       {Register(Is24Bit ? Z80::UIX : Z80::IX), getFrameRegister(MF)})
    for (MCRegAliasIterator I(Reg, this, /*IncludeSelf=*/true); I.isValid();
         ++I)
      Reserved.set(*I);

  return Reserved;
}

bool Z80RegisterInfo::saveScavengerRegister(MachineBasicBlock &MBB,
                                            MachineBasicBlock::iterator MI,
                                            MachineBasicBlock::iterator &UseMI,
                                            const TargetRegisterClass *RC,
                                            Register Reg) const {
  return false;
  const Z80Subtarget &STI = MBB.getParent()->getSubtarget<Z80Subtarget>();
  const TargetInstrInfo &TII = *STI.getInstrInfo();
  DebugLoc DL;
  if (Reg == Z80::AF)
    BuildMI(MBB, MI, DL, TII.get(Is24Bit ? Z80::PUSH24AF : Z80::PUSH16AF));
  else
    BuildMI(MBB, MI, DL, TII.get(Is24Bit ? Z80::PUSH24r : Z80::PUSH16r))
        .addReg(Reg);
  for (MachineBasicBlock::iterator II = MI; II != UseMI; ++II) {
    if (II->isDebugValue())
      continue;
    if (II->modifiesRegister(Reg, this))
      UseMI = II;
  }
  if (Reg == Z80::AF)
    BuildMI(MBB, UseMI, DL, TII.get(Is24Bit ? Z80::POP24AF : Z80::POP16AF));
  else
    BuildMI(MBB, UseMI, DL, TII.get(Is24Bit ? Z80::POP24r : Z80::POP16r), Reg);
  return true;
}

void Z80RegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator II,
                                          int SPAdj, unsigned FIOperandNum,
                                          RegScavenger *RS) const {
  MachineInstr &MI = *II;
  MachineBasicBlock &MBB = *MI.getParent();
  MachineFunction &MF = *MBB.getParent();
  const Z80Subtarget &STI = MF.getSubtarget<Z80Subtarget>();
  const Z80InstrInfo &TII = *STI.getInstrInfo();
  const Z80FrameLowering *TFI = getFrameLowering(MF);
  int FrameIndex = MI.getOperand(FIOperandNum).getIndex();
  Register BaseReg = getFrameRegister(MF);
  assert(TFI->hasFP(MF) && "Stack slot use without fp unimplemented");
  auto Offset = MF.getFrameInfo().getObjectOffset(FrameIndex) -
                TFI->getOffsetOfLocalArea();
  if (FrameIndex >= 0)
    // For non-fixed indices, skip over callee save slots.
    Offset -= MF.getInfo<Z80MachineFunctionInfo>()->getCalleeSavedFrameSize();
  else if (TFI->isFPSaved(MF))
    // For fixed indices, skip over FP save slot if it exists.
    Offset += TFI->getSlotSize();
  TII.rewriteFrameIndex(MI, FIOperandNum, BaseReg, Offset, RS, SPAdj);
}

Register Z80RegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  return getFrameLowering(MF)->hasFP(MF)
             ? MF.getInfo<Z80MachineFunctionInfo>()->getUsesAltFP()
                   ? Is24Bit ? Z80::UIY : Z80::IY
                   : Is24Bit ? Z80::UIX : Z80::IX
             : Is24Bit ? Z80::SPL : Z80::SPS;
}

bool Z80RegisterInfo::
shouldCoalesce(MachineInstr *MI,
               const TargetRegisterClass *SrcRC, unsigned SrcSubReg,
               const TargetRegisterClass *DstRC, unsigned DstSubReg,
               const TargetRegisterClass *NewRC, LiveIntervals &LIS) const {
  LLVM_DEBUG(
      dbgs() << getRegClassName(SrcRC) << '[' << SrcRC->getNumRegs()
             << "]:" << (SrcSubReg ? getSubRegIndexName(SrcSubReg) : "")
             << " -> " << getRegClassName(DstRC) << '[' << DstRC->getNumRegs()
             << "]:" << (DstSubReg ? getSubRegIndexName(DstSubReg) : "") << ' '
             << getRegClassName(NewRC) << '[' << NewRC->getNumRegs() << "]\n");
  // Don't coalesce if SrcRC and DstRC have a small intersection.
  return std::min(SrcRC->getNumRegs(), DstRC->getNumRegs()) <=
         NewRC->getNumRegs();
}

bool Z80RegisterInfo::requiresVirtualBaseRegisters(
    const MachineFunction &MF) const {
  return true;
}
int64_t Z80RegisterInfo::getFrameIndexInstrOffset(const MachineInstr *MI,
                                                  int FIOperandNum) const {
  return MI->getOperand(FIOperandNum + 1).getImm();
}
bool Z80RegisterInfo::needsFrameBaseReg(MachineInstr *MI,
                                        int64_t Offset) const {
  const MachineFunction &MF = *MI->getParent()->getParent();
  return !isFrameOffsetLegal(MI, getFrameRegister(MF), Offset);
}
Register Z80RegisterInfo::materializeFrameBaseRegister(MachineBasicBlock *MBB,
                                                       int FrameIdx,
                                                       int64_t Offset) const {
  MachineFunction &MF = *MBB->getParent();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  const Z80Subtarget &STI = MF.getSubtarget<Z80Subtarget>();
  const Z80InstrInfo &TII = *STI.getInstrInfo();
  MachineBasicBlock::iterator II = MBB->begin();
  DebugLoc DL = MBB->findDebugLoc(II);
  const MCInstrDesc &MCID = TII.get(Is24Bit ? Z80::LEA24ro : Z80::LEA16ro);
  Register BaseReg = MRI.createVirtualRegister(TII.getRegClass(MCID, 0, this, MF));
  BuildMI(*MBB, II, DL, MCID, BaseReg).addFrameIndex(FrameIdx).addImm(Offset);
  return BaseReg;
}

static bool isSplitLoadStoreOpc(unsigned Opc) {
  switch (Opc) {
  default:
    return false;
  case Z80::LD88rp:
  case Z80::LD88ro:
  case Z80::LD88pr:
  case Z80::LD88or:
    return true;
  }
}
static unsigned getFIOperandNum(const MachineInstr &MI) {
  for (const auto &MO : MI.explicit_uses())
    if (MO.isFI())
      return MI.getOperandNo(&MO);
  llvm_unreachable("Instr doesn't have a FrameIndex operand!");
}

void Z80RegisterInfo::resolveFrameIndex(MachineInstr &MI, Register BaseReg,
                                        int64_t Offset) const {
  MachineBasicBlock &MBB = *MI.getParent();
  MachineFunction &MF = *MBB.getParent();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  const Z80Subtarget &STI = MF.getSubtarget<Z80Subtarget>();
  const Z80InstrInfo &TII = *STI.getInstrInfo();

  MRI.constrainRegClass(BaseReg,
                        Is24Bit ? &Z80::I24RegClass : &Z80::I16RegClass);
  TII.rewriteFrameIndex(MI, getFIOperandNum(MI), BaseReg, Offset);
}
bool Z80RegisterInfo::isFrameOffsetLegal(const MachineInstr *MI,
                                         Register BaseReg,
                                         int64_t Offset) const {
  Offset += getFrameIndexInstrOffset(MI, getFIOperandNum(*MI));
  return isInt<8>(Offset) &&
         (!isSplitLoadStoreOpc(MI->getOpcode()) || isInt<8>(Offset + 1));
}
