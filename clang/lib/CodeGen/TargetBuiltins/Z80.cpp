//===---------- Z80.cpp - Emit LLVM Code for builtins ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "CGBuiltin.h"
#include "CodeGenFunction.h"
#include "clang/Basic/TargetBuiltins.h"
#include "llvm/IR/Intrinsics.h"

using namespace clang;
using namespace CodeGen;
using namespace llvm;

Value *CodeGenFunction::EmitZ80BuiltinExpr(unsigned BuiltinID,
                                           const CallExpr *E) {
  switch (BuiltinID) {
  case Z80::BI__builtin_bitreverse24:
    return emitBuiltinWithOneOverloadedType<1>(*this, E,
                                               Intrinsic::bitreverse);
  default:
    return nullptr;
  }
}
