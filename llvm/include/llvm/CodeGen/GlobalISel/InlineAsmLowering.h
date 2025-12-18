//===- llvm/CodeGen/GlobalISel/InlineAsmLowering.h --------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file describes how to lower LLVM inline asm to machine code INLINEASM.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_CODEGEN_GLOBALISEL_INLINEASMLOWERING_H
#define LLVM_CODEGEN_GLOBALISEL_INLINEASMLOWERING_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/CodeGen/TargetLowering.h"
#include <functional>

namespace llvm {
class CallBase;
class MachineIRBuilder;
class MachineInstrBuilder;
class MachineOperand;
class Register;
class TargetLowering;
class Value;

/// GISelAsmOperandInfo - This contains information for each constraint that we
/// are lowering.
class GISelAsmOperandInfo : public TargetLowering::AsmOperandInfo {
public:
  /// Regs - If this is a register or register class operand, this
  /// contains the set of assigned registers corresponding to the operand.
  SmallVector<Register, 1> Regs;

  explicit GISelAsmOperandInfo(const TargetLowering::AsmOperandInfo &Info)
      : TargetLowering::AsmOperandInfo(Info) {}
};

class InlineAsmLowering {
  const TargetLowering *TLI;

  virtual void anchor();

public:
  /// Lower the given inline asm call instruction
  /// \p GetOrCreateVRegs is a callback to materialize a register for the
  /// input and output operands of the inline asm
  /// \return True if the lowering succeeds, false otherwise.
  bool lowerInlineAsm(MachineIRBuilder &MIRBuilder, const CallBase &CB,
                      std::function<ArrayRef<Register>(const Value &Val)>
                          GetOrCreateVRegs) const;

  /// Lower the specified input operand into the Ops vector.
  /// \p OpInfo is information about the operand being lowered
  /// \p Ops is the vector to be filled with the lowered operands
  /// \return True if the lowering succeeds, false otherwise.
  virtual bool lowerInputAsmOperandForConstraint(
      GISelAsmOperandInfo &OpInfo, MachineInstrBuilder &Inst,
      MachineIRBuilder &MIRBuilder) const;

  /// Lower the specified output operand into the Ops vector.
  /// \p OpInfo is information about the operand being lowered
  /// \p Ops is the vector to be filled with the lowered operands
  /// \p ResRegs is the result registers, drop the ones that are used
  /// \return True if the lowering succeeds, false otherwise.
  virtual bool lowerOutputAsmOperandForConstraint(
      GISelAsmOperandInfo &OpInfo, MachineInstrBuilder &Inst,
      MachineIRBuilder &MIRBuilder, ArrayRef<Register> &ResRegs) const;

protected:
  /// Getter for generic TargetLowering class.
  const TargetLowering *getTLI() const { return TLI; }

  /// Getter for target specific TargetLowering class.
  template <class XXXTargetLowering> const XXXTargetLowering *getTLI() const {
    return static_cast<const XXXTargetLowering *>(TLI);
  }

public:
  InlineAsmLowering(const TargetLowering *TLI) : TLI(TLI) {}
  virtual ~InlineAsmLowering() = default;
};

} // end namespace llvm

#endif // LLVM_CODEGEN_GLOBALISEL_INLINEASMLOWERING_H
