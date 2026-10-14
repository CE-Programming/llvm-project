//===- Z80CodeGenOptions.h - Shared lowering choices ----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_Z80_Z80CODEGENOPTIONS_H
#define LLVM_LIB_TARGET_Z80_Z80CODEGENOPTIONS_H

namespace llvm {

// Let IR cost queries reflect the same comparison switches as lowering.
bool z80UseNativeWordArithmetic();
bool z80UseByteImmediates();
bool z80UseConstantBitwise();
bool z80UseConstantMultiply();
bool z80UseWideByteMultiply();

} // namespace llvm

#endif
