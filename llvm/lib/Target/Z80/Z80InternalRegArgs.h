//===-- Z80InternalRegArgs.h - Internal register arguments -------*- C++
//-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_Z80_Z80INTERNALREGARGS_H
#define LLVM_LIB_TARGET_Z80_Z80INTERNALREGARGS_H

namespace llvm {
class PassBuilder;
class Z80TargetMachine;

void registerZ80InternalRegArgsCallbacks(PassBuilder &PB,
                                         const Z80TargetMachine &TM);
} // namespace llvm

#endif
