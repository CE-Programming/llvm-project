//===- llvm/lib/Target/Z80/Z80InlineAsmLowering.cpp - Inline asm lowering -===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
/// \file
/// This file implements the lowering from LLVM IR inline asm to MIR INLINEASM
//
//===----------------------------------------------------------------------===//

#include "Z80InlineAsmLowering.h"
#include "Z80ISelLowering.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/IR/Constants.h"
using namespace llvm;

Z80InlineAsmLowering::Z80InlineAsmLowering(const Z80TargetLowering &TLI)
    : InlineAsmLowering(&TLI) {}

bool Z80InlineAsmLowering::lowerAsmOperandForConstraint(
    Value *Val, StringRef Constraint, std::vector<MachineOperand> &Ops,
    MachineIRBuilder &MIRBuilder) const {
  if (Constraint.size() == 1)
    if (const auto *CI = dyn_cast<ConstantInt>(Val))
      switch (Constraint[0]) {
      case 'I':
        if (CI->getValue().isIntN(3)) {
          Ops.push_back(MachineOperand::CreateImm(CI->getZExtValue()));
          return true;
        }
        break;
      case 'J':
        if (CI->getValue().isIntN(8)) {
          Ops.push_back(MachineOperand::CreateImm(CI->getZExtValue()));
          return true;
        }
        break;
      case 'M':
        if (CI->getValue().ule(2)) {
          Ops.push_back(MachineOperand::CreateImm(CI->getZExtValue()));
          return true;
        }
        break;
      case 'N':
        if (CI->getValue().isIntN(6) && !(CI->getZExtValue() & 7)) {
          Ops.push_back(MachineOperand::CreateImm(CI->getZExtValue()));
          return true;
        }
        break;
      case 'O':
        if (CI->getValue().isSignedIntN(8)) {
          Ops.push_back(MachineOperand::CreateImm(CI->getSExtValue()));
          return true;
        }
        break;
      }

  return InlineAsmLowering::lowerAsmOperandForConstraint(
      Val, Constraint, Ops, MIRBuilder);
}
