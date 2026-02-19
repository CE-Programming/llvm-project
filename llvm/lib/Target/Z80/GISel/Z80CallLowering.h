//===- llvm/lib/Target/Z80/Z80CallLowering.h - Call lowering ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
/// \file
/// This file describes how to lower LLVM calls to machine code calls.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_Z80_Z80CALLLOWERING_H
#define LLVM_LIB_TARGET_Z80_Z80CALLLOWERING_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/CodeGen/GlobalISel/CallLowering.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

namespace llvm {

class Z80TargetLowering;

class Z80CallLowering : public CallLowering {
public:
  Z80CallLowering(const Z80TargetLowering &TLI);

  bool lowerCall(MachineIRBuilder &MIRBuilder,
                 CallLoweringInfo &Info) const override;

  bool lowerFormalArguments(MachineIRBuilder &MIRBuilder, const Function &F,
                            ArrayRef<ArrayRef<Register>> VRegs,
                            FunctionLoweringInfo &FLI) const override;

  bool lowerReturn(MachineIRBuilder &MIRBuilder, const Value *Val,
                   ArrayRef<Register> VRegs,
                   FunctionLoweringInfo &FLI) const override;

  MachineInstrBuilder buildSCMP(MachineIRBuilder &MIRBuilder) const;

private:
  bool
  doCallerAndCalleePassArgsTheSameWay(CallLoweringInfo &Info,
                                      MachineFunction &MF,
                                      SmallVectorImpl<ArgInfo> &InArgs) const;

  bool
  areCalleeOutgoingArgsTailCallable(CallLoweringInfo &Info, MachineFunction &MF,
                                    SmallVectorImpl<ArgInfo> &OutArgs) const;

  bool
  isEligibleForTailCallOptimization(MachineIRBuilder &MIRBuilder,
                                    CallLoweringInfo &Info,
                                    SmallVectorImpl<ArgInfo> &InArgs,
                                    SmallVectorImpl<ArgInfo> &OutArgs) const;

  bool lowerTailCall(MachineIRBuilder &MIRBuilder, CallLoweringInfo &Info,
                     SmallVectorImpl<ArgInfo> &OutArgs) const;
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_Z80_Z80CALLLOWERING_H
