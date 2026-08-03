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
#include "llvm/CodeGen/VirtRegMap.h"
#include "llvm/ADT/SmallPtrSet.h"
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

const TargetRegisterClass *
Z80RegisterInfo::getPointerRegClass(const MachineFunction &MF,
                                    unsigned Kind) const {
  switch (Kind) {
  default: llvm_unreachable("Unexpected Kind!");
  case 0: return Is24Bit ? &Z80::R24RegClass : &Z80::R16RegClass;
  case 1: return Is24Bit ? &Z80::G24RegClass : &Z80::G16RegClass;
  case 2: return Is24Bit ? &Z80::O24RegClass : &Z80::O16RegClass;
  case 3: return Is24Bit ? &Z80::A24RegClass : &Z80::A16RegClass;
  case 4: return Is24Bit ? &Z80::I24RegClass : &Z80::I16RegClass;
  }
}

const TargetRegisterClass *
Z80RegisterInfo::getPointerRegClassForConstraint(const MachineFunction &MF,
                                                 InlineAsm::ConstraintCode Constraint) const {
  unsigned Kind;
  switch (Constraint) {
  default: llvm_unreachable("Unexpected Constraint!");
  case InlineAsm::ConstraintCode::v:
    Kind = 1;
    break;
  case InlineAsm::ConstraintCode::m:
    Kind = 2;
    break;
  case InlineAsm::ConstraintCode::o:
    Kind = 3;
    break;
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
  default:
    llvm_unreachable("Unsupported calling convention");
  case CallingConv::C:
  case CallingConv::Fast:
    return Is24Bit ? CSR_EZ80_C_SaveList : CSR_Z80_C_SaveList;
  case CallingConv::Z80_LibCall:
  case CallingConv::Z80_LibCall_AB:
  case CallingConv::Z80_LibCall_AC:
  case CallingConv::Z80_LibCall_BC:
  case CallingConv::Z80_LibCall_L:
  case CallingConv::Z80_LibCall_F:
    return Is24Bit ? CSR_EZ80_AllRegs_SaveList : CSR_Z80_AllRegs_SaveList;
  case CallingConv::Z80_LibCall_16:
    return Is24Bit ? CSR_EZ80_AllRegs16_SaveList : CSR_Z80_AllRegs_SaveList;
  case CallingConv::PreserveAll:
    return Is24Bit ? CSR_EZ80_AllRegsAndFlags_SaveList
                   : CSR_Z80_AllRegsAndFlags_SaveList;
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
  // only reserve when a frame pointer is actually used
  const Z80FrameLowering *TFI = getFrameLowering(MF);
  // with -Oz, __frameset ALWAYS sets up IX as the frame pointer, regardless of
  // what hasFP() returns at this point. reserve IX unconditionally for
  // -Oz to prevent register allocation from using IX as a general register
  // there might be a better way of going about this
  bool NeedsFP = TFI->hasFP(MF) || MF.getFunction().hasOptSize();
  if (NeedsFP) {
    Register FPReg = Is24Bit ? Z80::UIX : Z80::IX;
    for (MCRegAliasIterator I(FPReg, this, /*IncludeSelf=*/true); I.isValid();
         ++I)
      Reserved.set(*I);
  }

  // reserve IY when used as secondary frame base for deep stack access
  // This decision needs to be made here (before register allocation) to
  // properly reserve the register.
  // only activate SFB when estimated stack size exceeds the reach of
  // single LEA instruction
  auto &FuncInfo = *const_cast<MachineFunction &>(MF).getInfo<Z80MachineFunctionInfo>();
  
  // SFB activation causes I24 register exhaustion because it reserves
  // both UIX (frame pointer) and UIY (secondary frame base), leaving no registers
  // for instructions that architecturally require I24 (like LEA24ro, LD24ro, probably more)
#if 0
  if (!FuncInfo.getUsesSecondaryFrameBase() && !FuncInfo.getUsesAltFP() &&
      MF.getSubtarget().getFrameLowering()->hasFP(MF) && Is24Bit) {
    uint64_t EstStackSize = MF.getFrameInfo().estimateStackSize(MF);
    // NOTE: if both index registers (UIX/UIY) are reserved, the I24 class 
    // becomes empty, causing register allocation failures for instructions 
    // like LEA24ro. to avoid this, SFB is limited to a 256 byte threshold 
    // for now. This is compatible with later expanding SFB to lower IX indexes
    // only enable for stacks where IX+IY can cover all offsets
    if (EstStackSize > 127 && EstStackSize <= 256) {
      FuncInfo.setUsesSecondaryFrameBase(true);
      FuncInfo.setSecondaryFrameBaseOffset(128);
    }
  }
#endif

  if (FuncInfo.getUsesSecondaryFrameBase()) {
    Register IYReg = Is24Bit ? Z80::UIY : Z80::IY;
    for (MCRegAliasIterator I(IYReg, this, /*IncludeSelf=*/true); I.isValid();
         ++I)
      Reserved.set(*I);
  }

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

bool Z80RegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator II,
                                          int SPAdj, unsigned FIOperandNum,
                                          RegScavenger *RS) const {
  MachineInstr &MI = *II;
  MachineBasicBlock &MBB = *MI.getParent();
  MachineFunction &MF = *MBB.getParent();
  const Z80Subtarget &STI = MF.getSubtarget<Z80Subtarget>();
  const Z80InstrInfo &TII = *STI.getInstrInfo();
  const Z80FrameLowering *TFI = getFrameLowering(MF);
  int64_t FrameBias = TFI->ensureFramePointerBias(MF);
  const Z80MachineFunctionInfo &FuncInfo = *MF.getInfo<Z80MachineFunctionInfo>();
  int FrameIndex = MI.getOperand(FIOperandNum).getIndex();
  Register BaseReg = getFrameRegister(MF);
  assert(TFI->hasFP(MF) && "Stack slot use without fp unimplemented");
  auto Offset = MF.getFrameInfo().getObjectOffset(FrameIndex) -
                TFI->getOffsetOfLocalArea();
  if (FrameIndex < 0)
    // For fixed indices, skip over callee save slots.
    Offset += FuncInfo.getCalleeSavedFrameSize();
  Offset += FrameBias;

  // SFB handling is disabled. see #if 0 block in getReservedRegs
#if 0
  // try using secondary frame base (IY) for deep offsets
  // IY points to (IX - SecondaryFrameBaseOffset), so offset via IY is:
  // Offset + SecondaryFrameBaseOffset
  const auto &FuncInfo = *MF.getInfo<Z80MachineFunctionInfo>();
  if (FuncInfo.getUsesSecondaryFrameBase()) {
    int64_t SecOffset = FuncInfo.getSecondaryFrameBaseOffset();
    int64_t IYOffset = Offset + SecOffset;  // offset from IY's pov
    // if IY can reach it with a simple offset but IX cannot, use IY
    if (!isInt<8>(Offset) && isInt<8>(IYOffset)) {
      Register IYReg = Is24Bit ? Z80::UIY : Z80::IY;
      return TII.rewriteFrameIndex(MI, FIOperandNum, IYReg, IYOffset, RS, SPAdj);
    }
  }
#endif

  return TII.rewriteFrameIndex(MI, FIOperandNum, BaseReg, Offset, RS, SPAdj);
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
  return !isFrameOffsetLegal(MI, getFrameRegister(*MI->getMF()), Offset);
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
  bool IsSplit = isSplitLoadStoreOpc(MI->getOpcode());

  // check if reachable via primary frame register (IX)
  if (isInt<8>(Offset) && (!IsSplit || isInt<8>(Offset + 1)))
    return true;

  // SFB handling is disabled. see #if 0 block in getReservedRegs
#if 0
  // check if reachable via secondary frame base (IY)
  const MachineFunction &MF = *MI->getMF();
  const auto &FuncInfo = *MF.getInfo<Z80MachineFunctionInfo>();
  if (FuncInfo.getUsesSecondaryFrameBase()) {
    int64_t IYOffset = Offset + FuncInfo.getSecondaryFrameBaseOffset();
    if (isInt<8>(IYOffset) && (!IsSplit || isInt<8>(IYOffset + 1)))
      return true;
  }
#endif

  return false;
}


