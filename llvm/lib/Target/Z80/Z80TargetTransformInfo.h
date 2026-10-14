//===- Z80TargetTransformInfo.h - Z80 specific TTI -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_Z80_Z80TARGETTRANSFORMINFO_H
#define LLVM_LIB_TARGET_Z80_Z80TARGETTRANSFORMINFO_H

#include "Z80Subtarget.h"
#include "Z80TargetMachine.h"
#include "llvm/CodeGen/BasicTTIImpl.h"

namespace llvm {

class Z80TTIImpl final : public BasicTTIImplBase<Z80TTIImpl> {
  using BaseT = BasicTTIImplBase<Z80TTIImpl>;
  using TTI = TargetTransformInfo;
  friend BaseT;

  const Z80Subtarget *ST;
  const Z80TargetLowering *TLI;
  const Function &F;

  const Z80Subtarget *getST() const { return ST; }
  const Z80TargetLowering *getTLI() const { return TLI; }

  // Costs are bytes for size and RAM bus-transfer units for speed. The latter
  // are normalized to a native register ADD (four RAM cycles in CEmu).
  InstructionCost cost(unsigned Bytes, unsigned Transfers,
                       TTI::TargetCostKind Kind) const;
  std::optional<unsigned> stackSlots(ArrayRef<Type *> Args) const;
  InstructionCost helperCost(TTI::TargetCostKind Kind,
                             bool ConstantOperand = false) const;

public:
  explicit Z80TTIImpl(const Z80TargetMachine *TM, const Function &Fn)
      : BaseT(TM, Fn.getDataLayout()), ST(TM->getSubtargetImpl(Fn)),
        TLI(ST->getTargetLowering()), F(Fn) {}

  TypeSize getRegisterBitWidth(TTI::RegisterKind K) const override;
  unsigned getNumberOfRegisters(unsigned ClassID) const override;

  InstructionCost getInstructionCost(const User *U,
                                     ArrayRef<const Value *> Operands,
                                     TTI::TargetCostKind Kind) const override;
  InstructionCost getArithmeticInstrCost(
      unsigned Opcode, Type *Ty, TTI::TargetCostKind Kind,
      TTI::OperandValueInfo Op1Info = {TTI::OK_AnyValue, TTI::OP_None},
      TTI::OperandValueInfo Op2Info = {TTI::OK_AnyValue, TTI::OP_None},
      ArrayRef<const Value *> Args = {},
      const Instruction *I = nullptr) const override;
  InstructionCost getMemoryOpCost(
      unsigned Opcode, Type *Ty, Align Alignment, unsigned AddressSpace,
      TTI::TargetCostKind Kind,
      TTI::OperandValueInfo OpInfo = {TTI::OK_AnyValue, TTI::OP_None},
      const Instruction *I = nullptr) const override;
  InstructionCost getCallInstrCost(Function *Callee, Type *RetTy,
                                   ArrayRef<Type *> Args,
                                   TTI::TargetCostKind Kind) const override;
  unsigned getInlineCallPenalty(const Function *Caller, const CallBase &Call,
                                unsigned DefaultPenalty) const override;
};

} // namespace llvm

#endif
