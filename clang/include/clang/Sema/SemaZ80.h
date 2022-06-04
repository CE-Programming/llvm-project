//===----- SemaZ80.h ------- Z80 target-specific routines -----*- C++ -*---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
/// \file
/// This file declares semantic analysis functions specific to Z80.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_SEMA_SEMAZ80_H
#define LLVM_CLANG_SEMA_SEMAZ80_H

#include "clang/AST/DeclBase.h"
#include "clang/AST/Expr.h"
#include "clang/Basic/LLVM.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Sema/SemaBase.h"

namespace clang {
class ParsedAttr;

class SemaZ80 : public SemaBase {
public:
  SemaZ80(Sema &S);

  void handleZ80InterruptAttr(Sema &S, Decl *D, const ParsedAttr &AL)
};
} // namespace clang

#endif // LLVM_CLANG_SEMA_SEMAZ80_H
