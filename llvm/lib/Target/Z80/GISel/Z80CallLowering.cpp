//===- llvm/lib/Target/Z80/Z80CallLowering.cpp - Call lowering ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
/// \file
/// This file implements the lowering of LLVM calls to machine code calls for
/// GlobalISel.
//
//===----------------------------------------------------------------------===//

#include "Z80CallLowering.h"
#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "Z80CallingConv.h"
#include "Z80ISelLowering.h"
#include "Z80MachineFunctionInfo.h"
#include "Z80Subtarget.h"
#include "llvm/CodeGen/Analysis.h"
#include "llvm/CodeGen/GlobalISel/MachineIRBuilder.h"
#include "llvm/Support/Debug.h"
#include "llvm/Target/TargetMachine.h"
using namespace llvm;

#define DEBUG_TYPE "z80-call-lowering"

Z80CallLowering::Z80CallLowering(const Z80TargetLowering &TLI)
    : CallLowering(&TLI) {}

namespace {

struct Z80OutgoingValueHandler : public CallLowering::OutgoingValueHandler {
  Z80OutgoingValueHandler(MachineIRBuilder &MIRBuilder,
                          MachineRegisterInfo &MRI, MachineInstrBuilder &MIB,
                          CCAssignFn *AssignFn)
      : OutgoingValueHandler(MIRBuilder, MRI, AssignFn), MIB(MIB),
        DL(MIRBuilder.getMF().getDataLayout()),
        STI(MIRBuilder.getMF().getSubtarget<Z80Subtarget>()) {}

  Register getStackAddress(uint64_t Size, int64_t Offset,
                           MachinePointerInfo &MPO,
                           ISD::ArgFlagsTy Flags) override {
    LLT p0 = LLT::pointer(0, DL.getPointerSizeInBits(0));
    LLT SType = LLT::scalar(DL.getPointerSizeInBits(0));
    Register SPReg = MRI.createGenericVirtualRegister(p0);
    MIRBuilder.buildCopy(SPReg, STI.getRegisterInfo()->getStackRegister());

    Register OffsetReg = MRI.createGenericVirtualRegister(SType);
    MIRBuilder.buildConstant(OffsetReg, Offset);

    Register AddrReg = MRI.createGenericVirtualRegister(p0);
    MIRBuilder.buildPtrAdd(AddrReg, SPReg, OffsetReg);

    MPO = MachinePointerInfo::getStack(MIRBuilder.getMF(), Offset);
    return AddrReg;
  }

  void assignValueToReg(Register ValVReg, Register PhysReg,
                        CCValAssign &VA) override {
    MIB.addUse(PhysReg, RegState::Implicit);
    MIRBuilder.buildCopy(PhysReg, ValVReg);
  }

  void assignValueToAddress(Register ValVReg, Register Addr, uint64_t Size,
                            MachinePointerInfo &MPO, CCValAssign &VA) override {
    auto MMO = MIRBuilder.getMF().getMachineMemOperand(
        MPO, MachineMemOperand::MOStore, VA.getLocVT().getStoreSize(),
        Align());
    MIRBuilder.buildStore(ValVReg, Addr, *MMO);
  }

protected:
  MachineInstrBuilder &MIB;
  const DataLayout &DL;
  const Z80Subtarget &STI;
};

struct CallArgHandler : public Z80OutgoingValueHandler {
  CallArgHandler(MachineIRBuilder &MIRBuilder, MachineRegisterInfo &MRI,
                 MachineInstrBuilder &MIB, CCAssignFn *AssignFn)
      : Z80OutgoingValueHandler(MIRBuilder, MRI, MIB, AssignFn),
        StackPushes(MIRBuilder.getInsertPt()), RegCopies(StackPushes) {}

  void assignValueToReg(Register ValVReg, Register PhysReg,
                        CCValAssign &VA) override {
    auto SaveInsertPt = std::prev(MIRBuilder.getInsertPt());
    --StackPushes;
    MIRBuilder.setInsertPt(MIRBuilder.getMBB(), RegCopies);
    Z80OutgoingValueHandler::assignValueToReg(ValVReg, PhysReg, VA);
    ++StackPushes;
    MIRBuilder.setInsertPt(MIRBuilder.getMBB(), std::next(SaveInsertPt));
  }

  Register getStackAddress(uint64_t Size, int64_t Offset,
                           MachinePointerInfo &MPO,
                           ISD::ArgFlagsTy Flags) override {
    return Z80OutgoingValueHandler::getStackAddress(Size, Offset, MPO, Flags);
  }

  void assignValueToAddress(Register ValVReg, Register Addr, uint64_t Size,
                            MachinePointerInfo &MPO, CCValAssign &VA) override {
    if (MachineInstr *AddrMI = MRI.getVRegDef(Addr)) {
      unsigned SlotSize = STI.is24Bit() ? 3 : 2;
      if (VA.getLocVT().getStoreSize() == SlotSize &&
          AddrMI->getOpcode() == TargetOpcode::G_PTR_ADD) {
        if (MachineInstr *BaseMI =
                getDefIgnoringCopies(AddrMI->getOperand(1).getReg(), MRI)) {
          if (auto OffConst = getConstantVRegValWithLookThrough(
                  AddrMI->getOperand(2).getReg(), MRI)) {
            if (BaseMI->getOpcode() == TargetOpcode::COPY &&
                BaseMI->getOperand(1).getReg() ==
                    STI.getRegisterInfo()->getStackRegister() &&
                OffConst->Value == SetupFrameAdjustment) {
              auto SaveInsertPt = std::prev(MIRBuilder.getInsertPt());
              MIRBuilder.setInsertPt(MIRBuilder.getMBB(), StackPushes);
              --StackPushes;
              if (Size != SlotSize)
                ValVReg =
                    MIRBuilder.buildAnyExt(LLT::scalar(SlotSize * 8), ValVReg)
                        .getReg(0);
              MIRBuilder.buildInstr(SlotSize == 3 ? Z80::PUSH24r : Z80::PUSH16r,
                                    {}, {ValVReg});
              ++StackPushes;
              MIRBuilder.setInsertPt(MIRBuilder.getMBB(),
                                     std::next(SaveInsertPt));
              SetupFrameAdjustment += SlotSize;
              return;
            }
          }
        }
      }
    }
    LLT PtrTy = LLT::pointer(0, DL.getPointerSizeInBits(0));
    LLT OffTy = LLT::scalar(DL.getIndexSizeInBits(0));
    auto OffI = MIRBuilder.buildConstant(OffTy, -SetupFrameAdjustment);
    Addr = MIRBuilder.buildPtrAdd(PtrTy, Addr, OffI).getReg(0);
    Z80OutgoingValueHandler::assignValueToAddress(ValVReg, Addr, Size, MPO, VA);
  }

  bool finalize(CCState &State) override {
    FrameSize = State.getNextStackOffset();
    bool Success = Z80OutgoingValueHandler::finalize(State);
    MIRBuilder.setInsertPt(MIRBuilder.getMBB(), RegCopies);
    return Success;
  }

  unsigned getPreFrameAdjustment() const {
    return 0;
  }

  unsigned getFrameSize() const {
    return FrameSize;
  }

  unsigned getFrameTotalSize() const {
    return getPreFrameAdjustment() + getFrameSize();
  }

  unsigned getSetupFrameAdjustment() const {
    return SetupFrameAdjustment;
  }

  unsigned getDestroyFrameAdjustment() const {
    return 0;
  }

protected:
  MachineBasicBlock::iterator StackPushes, RegCopies;
  unsigned FrameSize, SetupFrameAdjustment = 0;
};

struct Z80IncomingValueHandler : public CallLowering::IncomingValueHandler {
  Z80IncomingValueHandler(MachineIRBuilder &MIRBuilder,
                          MachineRegisterInfo &MRI, CCAssignFn *AssignFn)
      : IncomingValueHandler(MIRBuilder, MRI, AssignFn),
        DL(MIRBuilder.getMF().getDataLayout()) {}

  Register getStackAddress(uint64_t Size, int64_t Offset,
                           MachinePointerInfo &MPO,
                           ISD::ArgFlagsTy Flags) override {
    auto &MFI = MIRBuilder.getMF().getFrameInfo();
    int FI = MFI.CreateFixedObject(Size, Offset, true);
    MPO = MachinePointerInfo::getFixedStack(MIRBuilder.getMF(), FI);
    LLT p0 = LLT::pointer(0, DL.getPointerSizeInBits(0));
    return MIRBuilder.buildFrameIndex(p0, FI).getReg(0);
  }

  void assignValueToAddress(Register ValVReg, Register Addr, uint64_t Size,
                            MachinePointerInfo &MPO, CCValAssign &VA) override {
    auto MMO = MIRBuilder.getMF().getMachineMemOperand(
        MPO, MachineMemOperand::MOLoad | MachineMemOperand::MOInvariant, Size,
        Align());
    MIRBuilder.buildLoad(ValVReg, Addr, *MMO);
  }

  void assignValueToReg(Register ValVReg, Register PhysReg,
                        CCValAssign &VA) override {
    markPhysRegUsed(PhysReg);
    MIRBuilder.buildCopy(ValVReg, PhysReg);
  }

  /// How the physical register gets marked varies between formal
  /// parameters (it's a basic-block live-in), and a call instruction
  /// (it's an implicit-def of the BL).
  virtual void markPhysRegUsed(MCRegister PhysReg) = 0;

protected:
  const DataLayout &DL;
};

struct FormalArgHandler : public Z80IncomingValueHandler {
  FormalArgHandler(MachineIRBuilder &MIRBuilder, MachineRegisterInfo &MRI,
                   CCAssignFn *AssignFn)
      : Z80IncomingValueHandler(MIRBuilder, MRI, AssignFn) {}

  void markPhysRegUsed(MCRegister PhysReg) override {
    MIRBuilder.getMRI()->addLiveIn(PhysReg);
    MIRBuilder.getMBB().addLiveIn(PhysReg);
  }

  bool finalize(CCState &State) override {
    MachineFunction &MF = MIRBuilder.getMF();
    MachineFrameInfo &MFI = MF.getFrameInfo();
    if (State.isVarArg()) {
      Z80MachineFunctionInfo &FuncInfo = *MF.getInfo<Z80MachineFunctionInfo>();
      int FrameIdx = MFI.CreateFixedObject(1, State.getNextStackOffset(), true);
      FuncInfo.setVarArgsFrameIndex(FrameIdx);
    }
    return true;
  }
};

struct CallReturnHandler : public Z80IncomingValueHandler {
  CallReturnHandler(MachineIRBuilder &MIRBuilder, MachineRegisterInfo &MRI,
                    CCAssignFn *AssignFn, MachineInstrBuilder &MIB)
      : Z80IncomingValueHandler(MIRBuilder, MRI, AssignFn), MIB(MIB) {}

  void markPhysRegUsed(MCRegister PhysReg) override {
    MIB.addDef(PhysReg, RegState::Implicit);
  }

protected:
  MachineInstrBuilder &MIB;
};

} // end anonymous namespace

void Z80CallLowering::splitToValueTypes(const ArgInfo &OrigArg,
                                        SmallVectorImpl<ArgInfo> &SplitArgs,
                                        const DataLayout &DL,
                                        MachineRegisterInfo &MRI) const {
  const Z80TargetLowering &TLI = *getTLI<Z80TargetLowering>();
  LLVMContext &Ctx = OrigArg.Ty->getContext();

  if (OrigArg.Ty->isVoidTy())
    return;

  SmallVector<EVT, 4> SplitVTs;
  SmallVector<uint64_t, 4> Offsets;
  ComputeValueVTs(TLI, DL, OrigArg.Ty, SplitVTs, &Offsets, 0);

  for (unsigned I = 0, E = SplitVTs.size(); I != E; ++I) {
    Type *SplitTy = SplitVTs[I].getTypeForEVT(Ctx);
    SplitArgs.emplace_back(OrigArg.Regs[I], SplitTy, OrigArg.Flags[0],
                           OrigArg.IsFixed);
  }
}

bool Z80CallLowering::lowerCall(MachineIRBuilder &MIRBuilder,
                                CallLoweringInfo &Info) const {
  MachineFunction &MF = MIRBuilder.getMF();
  const Function &F = MF.getFunction();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  const DataLayout &DL = F.getParent()->getDataLayout();
  const auto &STI = MF.getSubtarget<Z80Subtarget>();
  const Z80InstrInfo &TII = *STI.getInstrInfo();
  const Z80FrameLowering &TFI = *STI.getFrameLowering();
  const Z80RegisterInfo &TRI = *STI.getRegisterInfo();

  // Look through bitcasts of the callee.
  while (Info.Callee.isReg()) {
    if (MachineInstr *MI = MRI.getVRegDef(Info.Callee.getReg())) {
      switch (MI->getOpcode()) {
      case TargetOpcode::COPY:
      case TargetOpcode::G_GLOBAL_VALUE:
      case TargetOpcode::G_INTTOPTR:
      case TargetOpcode::G_CONSTANT:
        Info.Callee = MI->getOperand(1);
        continue;
      }
    }
    break;
  }

  SmallVector<ArgInfo, 8> OutArgs;
  for (const auto &OrigArg : Info.OrigArgs) {
    if (OrigArg.Regs.size() > 1)
      return false;
    splitToValueTypes(OrigArg, OutArgs, DL, MRI);
  }

  SmallVector<ArgInfo, 8> InArgs;
  if (!Info.OrigRet.Ty->isVoidTy()) {
    if (Info.OrigRet.Regs.size() > 1)
      return false;
    splitToValueTypes(Info.OrigRet, InArgs, DL, MRI);
  }

  auto CallSeqStart = MIRBuilder.buildInstr(TII.getCallFrameSetupOpcode());

  // Create a temporarily-floating call instruction so we can add the implicit
  // uses of arg registers.
  bool Is24Bit = STI.is24Bit();
  unsigned CallOpc = Info.Callee.isReg()
                         ? Is24Bit ? Z80::CALL24r : Z80::CALL16r
                         : Is24Bit ? Z80::CALL24 : Z80::CALL16;

  auto MIB = MIRBuilder.buildInstrNoInsert(CallOpc)
                 .add(Info.Callee)
                 .addRegMask(TRI.getCallPreservedMask(MF, Info.CallConv));

  // Do the actual argument marshalling.
  CallArgHandler Handler(MIRBuilder, MRI, MIB, CC_Z80);
  if (!handleAssignments(MIRBuilder, OutArgs, Handler, Info.CallConv,
                         Info.IsVarArg))
    return false;

  // Now we can add the actual call instruction to the correct basic block.
  MIRBuilder.insertInstr(MIB);

  // If Callee is a reg, since it is used by a target specific
  // instruction, it must have a register class matching the
  // constraint of that instruction.
  if (Info.Callee.isReg())
    constrainOperandRegClass(MF, TRI, MRI, TII,
                             *MF.getSubtarget().getRegBankInfo(), *MIB,
                             MIB->getDesc(), Info.Callee, 0);

  // Finally we can copy the returned value back into its virtual-register. In
  // symmetry with the arguments, the physical register must be an
  // implicit-define of the call instruction.

  if (!InArgs.empty()) {
    SmallVector<Register, 8> NewRegs;

    CallReturnHandler Handler(MIRBuilder, MRI, RetCC_Z80, MIB);
    if (!handleAssignments(MIRBuilder, InArgs, Handler, Info.CallConv,
                           Info.IsVarArg))
      return false;

    if (!NewRegs.empty()) {
      SmallVector<uint64_t, 8> Indices;
      uint64_t Index = 0;
      for (Register Reg : NewRegs) {
        Indices.push_back(Index);
        Index += MRI.getType(Reg).getSizeInBits();
      }
      MIRBuilder.buildSequence(Info.OrigRet.Regs[0], NewRegs, Indices);
    }
  }

  CallSeqStart.addImm(Handler.getFrameSize())
      .addImm(Handler.getPreFrameAdjustment())
      .addImm(Handler.getSetupFrameAdjustment());

  auto CallSeqEnd = MIRBuilder.buildInstr(TII.getCallFrameDestroyOpcode())
                        .addImm(Handler.getFrameTotalSize())
                        .addImm(Handler.getDestroyFrameAdjustment());

  // It is too early to know exactly which method will be used, however
  // sometimes a better method can be guaranteed and we can adjust the operands
  // accordingly.
  for (auto CallSeq : {CallSeqStart, CallSeqEnd}) {
    const TargetRegisterClass *ScratchRC = nullptr;
    switch (TFI.getOptimalStackAdjustmentMethod(
        MF, TII.getFrameAdjustment(*CallSeq))) {
    case Z80FrameLowering::SAM_None:
    case Z80FrameLowering::SAM_Tiny:
    case Z80FrameLowering::SAM_All:
      // These methods do not need anything.
      break;
    case Z80FrameLowering::SAM_Small:
      // This method clobbers an R register.
      ScratchRC = Is24Bit ? &Z80::R24RegClass : &Z80::R16RegClass;
      break;
    case Z80FrameLowering::SAM_Large:
      // This method also clobbers flags.
      CallSeq.addDef(Z80::F, RegState::Implicit | RegState::Dead);
      LLVM_FALLTHROUGH;
    case Z80FrameLowering::SAM_Medium:
      // These methods clobber an A register.
      ScratchRC = Is24Bit ? &Z80::A24RegClass : &Z80::A16RegClass;
      break;
    }
    if (ScratchRC)
      CallSeq.addDef(MRI.createVirtualRegister(ScratchRC),
                     RegState::Implicit | RegState::Dead);
  }

  return true;
}

bool Z80CallLowering::lowerFormalArguments(MachineIRBuilder &MIRBuilder,
                                           const Function &F,
                                           ArrayRef<ArrayRef<Register>> VRegs,
                                           FunctionLoweringInfo &FLI) const {
  MachineFunction &MF = MIRBuilder.getMF();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  const DataLayout &DL = MF.getDataLayout();
  auto &FuncInfo = *MF.getInfo<Z80MachineFunctionInfo>();

  SmallVector<ArgInfo, 8> SplitArgs;
  unsigned Idx = 0;
  for (auto &Arg : F.args()) {
    if (!DL.getTypeStoreSize(Arg.getType()))
      continue;

    // TODO: handle not simple cases.
    if (Arg.hasAttribute(Attribute::InReg) ||
        Arg.hasAttribute(Attribute::SwiftSelf) ||
        Arg.hasAttribute(Attribute::SwiftError) ||
        Arg.hasAttribute(Attribute::Nest) || VRegs[Idx].size() > 1)
      return false;

    if (Arg.hasAttribute(Attribute::StructRet))
      FuncInfo.setSRetReturnReg(VRegs[Idx][0]);

    ArgInfo OrigArg(VRegs[Idx], Arg.getType());
    setArgFlags(OrigArg, Idx + AttributeList::FirstArgIndex, DL, F);
    splitToValueTypes(OrigArg, SplitArgs, DL, MRI);
    Idx++;
  }

  MachineBasicBlock &MBB = MIRBuilder.getMBB();
  if (!MBB.empty())
    MIRBuilder.setInstr(*MBB.begin());

  FormalArgHandler Handler(MIRBuilder, MRI, CC_Z80);
  if (!handleAssignments(MIRBuilder, SplitArgs, Handler, F.getCallingConv(),
                         F.isVarArg()))
    return false;

  // Move back to the end of the basic block.
  MIRBuilder.setMBB(MBB);

  return true;
}

bool Z80CallLowering::lowerReturn(MachineIRBuilder &MIRBuilder,
                                  const Value *Val, ArrayRef<Register> VRegs,
                                  FunctionLoweringInfo &FLI) const {
  assert(!Val == VRegs.empty() && "Return value without a vreg");
  MachineFunction &MF = MIRBuilder.getMF();
  LLVMContext &Ctx = MF.getFunction().getContext();
  auto &FuncInfo = *MF.getInfo<Z80MachineFunctionInfo>();
  const auto &STI = MF.getSubtarget<Z80Subtarget>();
  auto MIB =
      MIRBuilder.buildInstrNoInsert(STI.is24Bit() ? Z80::RET24 : Z80::RET16);

  Register SRetReturnReg = FuncInfo.getSRetReturnReg();
  assert((!SRetReturnReg || VRegs.empty()) &&
         "Struct ret should have void return");
  Type *RetTy = nullptr;
  if (SRetReturnReg) {
    VRegs = SRetReturnReg;
    RetTy = Type::getInt8PtrTy(Ctx);
  } else if (!VRegs.empty())
    RetTy = Val->getType();

  if (!VRegs.empty()) {
    const Function &F = MF.getFunction();
    MachineRegisterInfo &MRI = MF.getRegInfo();
    const DataLayout &DL = MF.getDataLayout();
    const Z80TargetLowering &TLI = *getTLI<Z80TargetLowering>();

    SmallVector<EVT, 4> SplitEVTs;
    ComputeValueVTs(TLI, DL, RetTy, SplitEVTs);
    assert(VRegs.size() == SplitEVTs.size() &&
           "For each split Type there should be exactly one VReg.");

    SmallVector<ArgInfo, 8> SplitArgs;
    for (unsigned I = 0; I < SplitEVTs.size(); ++I) {
      ArgInfo CurArgInfo = ArgInfo{VRegs[I], SplitEVTs[I].getTypeForEVT(Ctx)};
      setArgFlags(CurArgInfo, AttributeList::ReturnIndex, DL, F);
      splitToValueTypes(CurArgInfo, SplitArgs, DL, MRI);
    }

    Z80OutgoingValueHandler Handler(MIRBuilder, MRI, MIB, RetCC_Z80);
    if (!handleAssignments(MIRBuilder, SplitArgs, Handler, F.getCallingConv(),
                           F.isVarArg()))
      return false;
  }

  MIRBuilder.insertInstr(MIB);
  return true;
}
