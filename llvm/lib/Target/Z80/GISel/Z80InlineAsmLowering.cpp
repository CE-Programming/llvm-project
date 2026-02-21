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
#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "Z80ISelLowering.h"
#include "Z80InstrInfo.h"
#include "llvm/CodeGen/GlobalISel/MachineIRBuilder.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/TargetLowering.h"
#include "llvm/IR/Constants.h"
using namespace llvm;

#define DEBUG_TYPE "z80-inline-asm-lowering"

Z80InlineAsmLowering::Z80InlineAsmLowering(const Z80TargetLowering &TLI)
    : InlineAsmLowering(&TLI) {}

bool Z80InlineAsmLowering::lowerInputAsmOperandForConstraint(
    GISelAsmOperandInfo &OpInfo, MachineInstrBuilder &Inst,
    MachineIRBuilder &MIRBuilder) const {
  const StringRef Constraint = OpInfo.ConstraintCode;
  Value *Val = OpInfo.CallOperandVal;
  if (Constraint.size() == 1 &&
      getTLI()->getConstraintType(Constraint) == TargetLowering::C_Immediate)
    switch (Constraint[0]) {
    case 'I':
      if (ConstantInt *CI = dyn_cast<ConstantInt>(Val)) {
        if (CI->getValue().isIntN(3)) {
          Inst.addImm(InlineAsm::Flag(InlineAsm::Kind::Imm, 1));
          Inst.addImm(CI->getZExtValue());
          return true;
        }
      }
      break;
    case 'J':
      if (ConstantInt *CI = dyn_cast<ConstantInt>(Val)) {
        if (CI->getValue().isIntN(8)) {
          Inst.addImm(InlineAsm::Flag(InlineAsm::Kind::Imm, 1));
          Inst.addImm(CI->getZExtValue());
          return true;
        }
      }
      break;
    case 'M':
      if (ConstantInt *CI = dyn_cast<ConstantInt>(Val)) {
        if (CI->getValue().ule(2)) {
          Inst.addImm(InlineAsm::Flag(InlineAsm::Kind::Imm, 1));
          Inst.addImm(CI->getZExtValue());
          return true;
        }
      }
      break;
    case 'N':
      if (ConstantInt *CI = dyn_cast<ConstantInt>(Val)) {
        if (CI->getValue().isIntN(6) && !(CI->getZExtValue() & 7)) {
          Inst.addImm(InlineAsm::Flag(InlineAsm::Kind::Imm, 1));
          Inst.addImm(CI->getZExtValue());
          return true;
        }
      }
      break;
    case 'O':
      if (ConstantInt *CI = dyn_cast<ConstantInt>(Val)) {
        if (CI->getValue().isSignedIntN(8)) {
          Inst.addImm(InlineAsm::Flag(InlineAsm::Kind::Imm, 1));
          Inst.addImm(CI->getSExtValue());
          return true;
        }
      }
      break;
    }

  return InlineAsmLowering::lowerInputAsmOperandForConstraint(OpInfo, Inst,
                                                              MIRBuilder);
}
