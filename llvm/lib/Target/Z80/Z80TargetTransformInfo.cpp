//===- Z80TargetTransformInfo.cpp - Z80 specific TTI ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Z80TargetTransformInfo.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/KnownBits.h"
#include "llvm/Support/MathExtras.h"

using namespace llvm;

InstructionCost Z80TTIImpl::cost(unsigned Bytes, unsigned Units,
                                 TTI::TargetCostKind Kind) const {
  if (Kind == TTI::TCK_CodeSize)
    return Bytes;
  if (Kind == TTI::TCK_SizeAndLatency)
    return std::max(Bytes, Units);
  return Units;
}

TypeSize Z80TTIImpl::getRegisterBitWidth(TTI::RegisterKind K) const {
  if (K != TTI::RGK_Scalar)
    return TypeSize::getFixed(0);
  return TypeSize::getFixed(ST->is24Bit() ? 24 : 16);
}

unsigned Z80TTIImpl::getNumberOfRegisters(unsigned ClassID) const {
  // BC, DE, HL and IY. Conservatively reserve IX for the frame pointer.
  return ClassID == 0 ? 4 : 0;
}

std::optional<unsigned> Z80TTIImpl::stackSlots(ArrayRef<Type *> Args) const {
  unsigned Slots = 0;
  unsigned WordBits = ST->is24Bit() ? 24 : 16;
  for (Type *Ty : Args) {
    if (!Ty->isSized() || Ty->isVectorTy() || Ty->isAggregateType())
      return std::nullopt;
    // The C ABI promotes narrow scalars to a full stack word. Wider scalars
    // are split into words.
    unsigned Bits = DL.getTypeSizeInBits(Ty).getFixedValue();
    Slots += divideCeil(Bits, WordBits);
  }
  return Slots;
}

InstructionCost Z80TTIImpl::helperCost(TTI::TargetCostKind Kind,
                                       bool ConstantOperand) const {
  unsigned WordBytes = ST->is24Bit() ? 3 : 2;
  unsigned CallBytes = WordBytes + 1;
  unsigned ImmediateBytes = ConstantOperand ? CallBytes : 0;
  // Count CALL, its return-address stores, RET and its stack reads. The
  // helper's body is deliberately not priced: it depends on the runtime and
  // often on the operands. This is a conservative dispatch-only lower bound.
  return cost(CallBytes + ImmediateBytes,
              CallBytes + ImmediateBytes + 1 + 2 * WordBytes, Kind);
}

InstructionCost Z80TTIImpl::getArithmeticInstrCost(
    unsigned Opcode, Type *Ty, TTI::TargetCostKind Kind,
    TTI::OperandValueInfo Op1Info, TTI::OperandValueInfo Op2Info,
    ArrayRef<const Value *> Args, const Instruction *I) const {
  if (!ST->hasEZ80Ops() || !Ty->isIntegerTy())
    return BaseT::getArithmeticInstrCost(Opcode, Ty, Kind, Op1Info, Op2Info,
                                         Args, I);
  unsigned Bits = Ty->getIntegerBitWidth();
  if (Bits != 8 && Bits != 16 && !(Bits == 24 && ST->is24Bit()))
    return BaseT::getArithmeticInstrCost(Opcode, Ty, Kind, Op1Info, Op2Info,
                                         Args, I);
  unsigned Prefix = ST->is24Bit() && Bits == 16;
  const ConstantInt *C =
      Args.size() == 2 ? dyn_cast<ConstantInt>(Args[1]) : nullptr;
  if (!C && Args.size() == 2 && Instruction::isCommutative(Opcode))
    C = dyn_cast<ConstantInt>(Args[0]);

  switch (Opcode) {
  case Instruction::Add:
  case Instruction::Sub: {
    if (C && C->isZero())
      return 0;
    if (C && (C->isOne() || C->isMinusOne()))
      return cost(1 + Prefix, 1 + Prefix, Kind);
    // Native pair subtraction clears carry with OR A then uses SBC HL,rr.
    unsigned Bytes =
        Opcode == Instruction::Sub && Bits != 8 ? 3 + Prefix : 1 + Prefix;
    if (C)
      Bytes += Bits == 8 ? 1 : 1 + Bits / 8;
    return cost(Bytes, Bytes, Kind);
  }
  case Instruction::Mul: {
    bool ByteOperands = Bits == 8;
    if (Args.size() == 2)
      ByteOperands = llvm::all_of(Args, [&](const Value *V) {
        return computeKnownBits(V, DL).countMaxActiveBits() <= 8;
      });
    if (ByteOperands) {
      // Two byte moves pack the operands, followed by MLT (two fetched bytes
      // and four internal cycles). MLT itself needs no short-mode prefix.
      return cost(4, 5, Kind);
    }
    if (C && !C->getValue().isNegative()) {
      const APInt &V = C->getValue();
      if (V.isZero() || V.isOne())
        return cost(V.isZero() ? 1 + Bits / 8 : 0,
                    V.isZero() ? 1 + Bits / 8 : 0, Kind);
      unsigned Doubles = V.getActiveBits() - 1;
      unsigned Adds = V.popcount() - 1;
      unsigned Bytes = (Doubles + Adds) * (1 + Prefix) + (Adds ? 2 : 0);
      unsigned CallBytes = ST->is24Bit() ? 8 : 6;
      if (Bytes <= (F.hasOptSize() ? CallBytes : 2 * CallBytes)) {
        // PUSH/POP preserves the original value when addition needs it.
        unsigned Units =
            (Doubles + Adds) * (1 + Prefix) +
            (Adds ? 2 + divideCeil(3 * (ST->is24Bit() ? 3 : 2), 2) : 0);
        return cost(Bytes, Units, Kind);
      }
    }
    return helperCost(Kind, C != nullptr);
  }
  case Instruction::Shl:
    if (C && C->getValue().uge(1) && C->getValue().ule(6) && Bits != 8) {
      unsigned Bytes = C->getZExtValue() * (1 + Prefix);
      return cost(Bytes, Bytes, Kind);
    }
    [[fallthrough]];
  case Instruction::LShr:
  case Instruction::AShr:
    if (C && C->isZero())
      return 0;
    if (C && C->getValue().uge(1) && C->getValue().ule(2) && Bits == 8)
      return cost(2 * C->getZExtValue(), 2 * C->getZExtValue(), Kind);
    if (C && C->equalsInt(8) && Bits == 16 && Opcode != Instruction::AShr)
      return cost(3, 3, Kind);
    if (C && C->isOne() && Bits == 16 && Opcode != Instruction::Shl)
      return cost(4, 4, Kind);
    return helperCost(Kind, C != nullptr);
  case Instruction::And:
  case Instruction::Or:
  case Instruction::Xor:
    if (Bits == 8)
      return cost(C ? 2 : 1, C ? 2 : 1, Kind);
    // Full-width logical operations use helpers in size builds. Speed builds
    // currently expand 16-bit operations into their byte operations and moves.
    if (Bits == 16 && !F.hasOptSize())
      return cost(6, 6, Kind);
    return helperCost(Kind, C != nullptr);
  case Instruction::UDiv:
  case Instruction::SDiv:
  case Instruction::URem:
  case Instruction::SRem:
    return helperCost(Kind, C != nullptr);
  default:
    return BaseT::getArithmeticInstrCost(Opcode, Ty, Kind, Op1Info, Op2Info,
                                         Args, I);
  }
}

InstructionCost Z80TTIImpl::getMemoryOpCost(unsigned Opcode, Type *Ty,
                                            Align Alignment,
                                            unsigned AddressSpace,
                                            TTI::TargetCostKind Kind,
                                            TTI::OperandValueInfo OpInfo,
                                            const Instruction *I) const {
  if (!ST->hasEZ80Ops() || AddressSpace != 0 ||
      !(Ty->isIntegerTy() || Ty->isPointerTy()))
    return BaseT::getMemoryOpCost(Opcode, Ty, Alignment, AddressSpace, Kind,
                                  OpInfo, I);
  unsigned Bits = DL.getTypeSizeInBits(Ty).getFixedValue();
  if (Bits != 8 && Bits != 16 && !(Bits == 24 && ST->is24Bit()))
    return BaseT::getMemoryOpCost(Opcode, Ty, Alignment, AddressSpace, Kind,
                                  OpInfo, I);
  // A byte through HL uses one opcode; eZ80 pair accesses use ED+opcode.
  // Add a prefix for a short pair in ADL mode. This excludes address setup,
  // which is costed separately, and assumes register-indirect accesses.
  unsigned Bytes = Bits == 8 ? 1 : 2 + (ST->is24Bit() && Bits == 16);
  unsigned AccessUnits =
      Opcode == Instruction::Store ? divideCeil(Bits / 8, 2) : Bits / 8;
  return cost(Bytes, Bytes + AccessUnits, Kind);
}

InstructionCost Z80TTIImpl::getCallInstrCost(Function *Callee, Type *RetTy,
                                             ArrayRef<Type *> Args,
                                             TTI::TargetCostKind Kind) const {
  if (!ST->hasEZ80Ops() || !Callee ||
      Callee->getCallingConv() != CallingConv::C)
    return BaseT::getCallInstrCost(Callee, RetTy, Args, Kind);
  auto Slots = stackSlots(Args);
  if (!Slots)
    return BaseT::getCallInstrCost(Callee, RetTy, Args, Kind);
  unsigned WordBytes = ST->is24Bit() ? 3 : 2;
  // A PUSH per C argument word plus one POP per word for a small cleanup.
  // Larger cleanups may use arithmetic instead; this is a local estimate.
  return helperCost(Kind) +
         cost(2 * *Slots, *Slots * (2 + divideCeil(3 * WordBytes, 2)), Kind);
}

InstructionCost Z80TTIImpl::getInstructionCost(const User *U,
                                               ArrayRef<const Value *> Operands,
                                               TTI::TargetCostKind Kind) const {
  if (ST->hasEZ80Ops()) {
    if (auto *CB = dyn_cast<CallBase>(U); CB && !isa<IntrinsicInst>(CB)) {
      if (Function *Callee = CB->getCalledFunction();
          Callee && CB->getCallingConv() == CallingConv::C &&
          isLoweredToCall(Callee)) {
        SmallVector<Type *> Types;
        for (const Value *Arg : CB->args())
          Types.push_back(Arg->getType());
        return getCallInstrCost(Callee, CB->getType(), Types, Kind);
      }
    }
  }
  return BaseT::getInstructionCost(U, Operands, Kind);
}

unsigned Z80TTIImpl::getInlineCallPenalty(const Function *Caller,
                                          const CallBase &Call,
                                          unsigned DefaultPenalty) const {
  // Preserve existing size inlining (and explicit inline-threshold tweaks).
  // For speed, account for native C stack traffic relative to one-register
  // arithmetic; generic inlining already charges one instruction per argument.
  if (!ST->hasEZ80Ops() || !Caller || Caller->hasOptSize() ||
      Call.getCallingConv() != CallingConv::C ||
      Call.hasFnAttr(Attribute::Cold) ||
      (Call.getCalledFunction() &&
       Call.getCalledFunction()->hasFnAttribute(Attribute::Cold)))
    return DefaultPenalty;
  SmallVector<Type *> Types;
  for (const Value *Arg : Call.args())
    Types.push_back(Arg->getType());
  auto Slots = stackSlots(Types);
  if (!Slots)
    return DefaultPenalty;
  unsigned WordBytes = ST->is24Bit() ? 3 : 2;
  // InlineCost's instruction unit is five. Charge only return-address and
  // argument stack traffic here; no runtime helper or callee-body estimate.
  return DefaultPenalty +
         5 * (divideCeil(3 * WordBytes, 2) + *Slots * divideCeil(WordBytes, 2));
}
