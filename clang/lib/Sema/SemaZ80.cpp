//===------ SemaZ80.cpp ---------- Z80 target-specific routines -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file implements semantic analysis functions specific to Z80.
//
//===----------------------------------------------------------------------===//

#include "clang/Sema/SemaZ80.h"
#include "clang/Basic/DiagnosticSema.h"
#include "clang/Basic/TargetBuiltins.h"
#include "clang/Sema/Attr.h"
#include "clang/Sema/ParsedAttr.h"
#include "clang/Sema/Sema.h"
#include "llvm/ADT/APSInt.h"
#include "llvm/TargetParser/Triple.h"
#include <bitset>

namespace clang {

SemaZ80::SemaZ80(Sema &S) : SemaBase(S) {}

void SemaZ80::handleZ80InterruptAttr(Decl *D, const ParsedAttr &AL) {
  if (!D->getDeclContext()->isFunctionOrMethod()) {
    Diag(D->getLocation(), diag::warn_attribute_wrong_decl_type)
        << "'interrupt'" << ExpectedFunction;
    return;
  }

  if (hasFunctionProto(D) && getFunctionOrMethodNumParams(D) != 0) {
    Diag(D->getLocation(), diag::warn_interrupt_attribute_invalid)
        << /*Z80*/ 3 << 0;
    return;
  }

  if (!getFunctionOrMethodResultType(D)->isVoidType()) {
    Diag(D->getLocation(), diag::warn_interrupt_attribute_invalid)
        << /*Z80*/ 3 << 1;
    return;
  }

  // The attribute takes an optional string argument.
  if (!AL.checkAtMostNumArgs(SemaRef, 1))
    return;

  StringRef Str;
  SourceLocation ArgLoc;
  if (AL.getNumArgs() && !SemaRef.checkStringLiteralArgumentAttr(AL, 0, Str, &ArgLoc))
    return;

  AnyZ80InterruptAttr::InterruptType Kind;
  if (!AnyZ80InterruptAttr::ConvertStrToInterruptType(Str, Kind)) {
    Diag(AL.getLoc(), diag::warn_attribute_type_not_supported)
        << AL << ("\"" + Str + "\"").str() << ArgLoc;
    Kind = AnyZ80InterruptAttr::InterruptType::Generic;
  }

  D->addAttr(::new (SemaRef.Context) AnyZ80InterruptAttr(SemaRef.Context, AL, Kind));
}

} // namespace clang
