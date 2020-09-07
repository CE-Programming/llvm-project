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
#include "llvm/CodeGen/MachineInstrBuilder.h"
using namespace llvm;

#define DEBUG_TYPE "z80-inline-asm-lowering"

Z80InlineAsmLowering::Z80InlineAsmLowering(const Z80TargetLowering &TLI)
    : InlineAsmLowering(&TLI) {}

bool Z80InlineAsmLowering::lowerInputAsmOperandForConstraint(
    GISelAsmOperandInfo &OpInfo, MachineInstrBuilder &Inst,
    MachineIRBuilder &MIRBuilder) const {
  if (OpInfo.ConstraintType == TargetLowering::C_Immediate &&
      OpInfo.ConstraintCode.size() == 1)
    switch (OpInfo.ConstraintCode[0]) {
    case 'I':
      if (ConstantInt *CI = dyn_cast<ConstantInt>(OpInfo.CallOperandVal)) {
        if (CI->getValue().isIntN(3)) {
          Inst.addImm(InlineAsm::getFlagWord(InlineAsm::Kind_Imm, 1));
          Inst.addImm(CI->getZExtValue());
          return true;
        }
      }
      break;
    case 'J':
      if (ConstantInt *CI = dyn_cast<ConstantInt>(OpInfo.CallOperandVal)) {
        if (CI->getValue().isIntN(8)) {
          Inst.addImm(InlineAsm::getFlagWord(InlineAsm::Kind_Imm, 1));
          Inst.addImm(CI->getZExtValue());
          return true;
        }
      }
      break;
    case 'M':
      if (ConstantInt *CI = dyn_cast<ConstantInt>(OpInfo.CallOperandVal)) {
        if (CI->getValue().ule(2)) {
          Inst.addImm(InlineAsm::getFlagWord(InlineAsm::Kind_Imm, 1));
          Inst.addImm(CI->getZExtValue());
          return true;
        }
      }
      break;
    case 'N':
      if (ConstantInt *CI = dyn_cast<ConstantInt>(OpInfo.CallOperandVal)) {
        if (CI->getValue().isIntN(6) && !(CI->getZExtValue() & 7)) {
          Inst.addImm(InlineAsm::getFlagWord(InlineAsm::Kind_Imm, 1));
          Inst.addImm(CI->getZExtValue());
          return true;
        }
      }
      break;
    case 'O':
      if (ConstantInt *CI = dyn_cast<ConstantInt>(OpInfo.CallOperandVal)) {
        if (CI->getValue().isSignedIntN(8)) {
          Inst.addImm(InlineAsm::getFlagWord(InlineAsm::Kind_Imm, 1));
          Inst.addImm(CI->getSExtValue());
          return true;
        }
      }
      break;
    }

  return InlineAsmLowering::lowerInputAsmOperandForConstraint(OpInfo, Inst,
                                                              MIRBuilder);
}

bool Z80InlineAsmLowering::lowerOutputAsmOperandForConstraint(
    GISelAsmOperandInfo &OpInfo, MachineInstrBuilder &Inst,
    MachineIRBuilder &MIRBuilder, ArrayRef<Register> &ResRegs) const {
  if (OpInfo.ConstraintType == TargetLowering::C_Other) {
    Z80::CondCode CC = Z80::parseConstraintCode(OpInfo.ConstraintCode);
    if (CC != Z80::COND_INVALID) {
      Inst.addImm(InlineAsm::getFlagWord(
          OpInfo.isEarlyClobber ? InlineAsm::Kind_RegDefEarlyClobber
                                : InlineAsm::Kind_RegDef,
          1));
      Inst.addReg(Z80::F,
                  RegState::ImplicitDefine |
                      (OpInfo.isEarlyClobber ? RegState::EarlyClobber : 0));
      MIRBuilder.buildBoolExt(
          ResRegs.front(),
          MIRBuilder.buildInstr(Z80::SetCC, {LLT::scalar(1)}, {int64_t(CC)}),
          false);
      ResRegs = ResRegs.drop_front();
      return true;
    }
  }

  return InlineAsmLowering::lowerOutputAsmOperandForConstraint(
      OpInfo, Inst, MIRBuilder, ResRegs);
}
