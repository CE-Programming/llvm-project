//===- Z80TargetTransformInfo.cpp - Z80 specific TTI ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Z80TargetTransformInfo.h"
#include "Z80CodeGenOptions.h"
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
  bool NativeWidth = Bits == 8 || Bits == 16 || (Bits == 24 && ST->is24Bit());
  if (!NativeWidth) {
    // These scalar operations map to software helpers. Do not inherit the
    // generic one-instruction price simply because their IR type is scalar.
    if (Bits == 32 || Bits == 64 || (Bits == 48 && ST->is24Bit())) {
      switch (Opcode) {
      case Instruction::Add:
      case Instruction::Sub:
        // ADL 48-bit add/sub are instead narrowed to two native words with
        // carry. Register moves and spills remain outside this local estimate.
        if (Bits == 48)
          return cost(Opcode == Instruction::Add ? 3 : 5,
                      Opcode == Instruction::Add ? 3 : 5, Kind);
        [[fallthrough]];
      case Instruction::Mul:
      case Instruction::And:
      case Instruction::Or:
      case Instruction::Xor:
      case Instruction::Shl:
      case Instruction::LShr:
      case Instruction::AShr:
      case Instruction::UDiv:
      case Instruction::SDiv:
      case Instruction::URem:
      case Instruction::SRem:
        return helperCost(Kind);
      default:
        break;
      }
    }
    return BaseT::getArithmeticInstrCost(Opcode, Ty, Kind, Op1Info, Op2Info,
                                         Args, I);
  }
  unsigned Prefix = ST->is24Bit() && Bits == 16;
  unsigned NativePrefix = z80UseNativeWordArithmetic() ? 0 : Prefix;
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
    unsigned Bytes = Opcode == Instruction::Sub && Bits != 8 ? 3 + NativePrefix
                                                             : 1 + NativePrefix;
    if (C)
      Bytes += Bits == 8 ? 1 : 1 + ((Prefix && !NativePrefix) ? 3 : Bits / 8);
    return cost(Bytes, Bytes, Kind);
  }
  case Instruction::Mul: {
    bool ByteOperands = Bits == 8;
    if (Args.size() == 2)
      ByteOperands = llvm::all_of(Args, [&](const Value *V) {
        return computeKnownBits(V, DL).countMaxActiveBits() <= 8;
      });
    if (ByteOperands && ST->hasZ180Ops() &&
        (Bits == 8 || z80UseWideByteMultiply())) {
      // Two byte moves pack the operands, followed by MLT (two fetched bytes
      // and four internal cycles). MLT itself needs no short-mode prefix.
      return cost(4, 5, Kind);
    }
    if (z80UseConstantMultiply() && C && !C->getValue().isNegative()) {
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
        // Lowering keeps its conservative prefix budget; once selected,
        // native ADL arithmetic can remove those prefixes from the sequence.
        Bytes = (Doubles + Adds) * (1 + NativePrefix) + (Adds ? 2 : 0);
        unsigned Units =
            (Doubles + Adds) * (1 + NativePrefix) +
            (Adds ? 2 + divideCeil(3 * (ST->is24Bit() ? 3 : 2), 2) : 0);
        return cost(Bytes, Units, Kind);
      }
    }
    return helperCost(Kind, C != nullptr);
  }
  case Instruction::Shl:
    if (C && C->getValue().uge(1) && C->getValue().ule(6) && Bits != 8) {
      unsigned Bytes = C->getZExtValue() * (1 + NativePrefix);
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
    if (Bits == 8) {
      // A byte immediate occupies two bytes; without the comparison switch,
      // the imported pattern materializes a constant register first.
      unsigned Bytes = C ? (z80UseByteImmediates() ? 2 : 3) : 1;
      return cost(Bytes, Bytes, Kind);
    }
    if (C && z80UseConstantBitwise()) {
      const APInt &Mask = C->getValue();
      APInt Changed = Opcode == Instruction::And ? ~Mask : Mask;
      if (Changed.isZero())
        return 0;
      if (Opcode == Instruction::And && Bits == 24 &&
          Mask == APInt::getSignedMaxValue(24))
        return cost(1, 1, Kind) + helperCost(Kind);
      if (Opcode == Instruction::And && Mask.getActiveBits() <= 8) {
        // Zero extension is five caller bytes in ADL, three in native short
        // mode; a nonidentity byte mask adds a two-byte immediate operation.
        unsigned Bytes = Bits == 24 ? 5 : 3;
        if (!Mask.isMask(8))
          Bytes += z80UseByteImmediates() ? 2 : 3;
        return cost(Bytes, Bytes, Kind);
      }
      unsigned Offset = Changed.getActiveBits() <= 8 ? 0 : 8;
      if (Changed.getActiveBits() <= 16 &&
          (Changed.lshr(Offset).getZExtValue() & ~uint64_t(255)) == 0 &&
          (!Offset || Changed.extractBits(8, 0).isZero())) {
        // SET/RES preserves the complete register and avoids the accumulator.
        // Other immediates may need two moves through A. Ignore coalesced
        // moves for a single bit, include them in the ordinary byte estimate.
        bool Bit = Opcode != Instruction::Xor && Changed.isPowerOf2() &&
                   z80UseByteImmediates();
        unsigned Bytes = Bit ? 2 : (z80UseByteImmediates() ? 4 : 5);
        return cost(Bytes, Bytes, Kind);
      }
      if (Opcode == Instruction::And && Bits == 24 &&
          Mask.getActiveBits() <= 16) {
        if (Mask.isMask(16))
          return cost(5, 5, Kind);
        Type *WordTy = IntegerType::get(F.getContext(), 16);
        const Value *WordArgs[] = {PoisonValue::get(WordTy),
                                   ConstantInt::get(WordTy, Mask.trunc(16))};
        return cost(5, 5, Kind) + getArithmeticInstrCost(Opcode, WordTy, Kind,
                                                         Op1Info, Op2Info,
                                                         WordArgs, I);
      }
    }
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
      (Callee->getCallingConv() != CallingConv::C &&
       Callee->getCallingConv() != CallingConv::Fast))
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
          Callee &&
          (CB->getCallingConv() == CallingConv::C ||
           CB->getCallingConv() == CallingConv::Fast) &&
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
      (Call.getCallingConv() != CallingConv::C &&
       Call.getCallingConv() != CallingConv::Fast) ||
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
