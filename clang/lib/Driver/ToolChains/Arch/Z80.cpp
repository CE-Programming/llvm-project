//===--- Z80.cpp - Z80 Helpers for Tools ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Z80.h"
#include "ToolChains/CommonArgs.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/DriverDiagnostic.h"
#include "clang/Driver/Options.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/Host.h"

using namespace clang::driver;
using namespace clang::driver::tools;
using namespace clang;
using namespace llvm::opt;

const char *z80::getZ80TargetCPU(const ArgList &Args,
                                 const llvm::Triple &Triple) {
  if (const Arg *A = Args.getLastArg(clang::driver::options::OPT_march_EQ))
    return A->getValue();

  // Select the default CPU if none was given (or detection failed).

  if (!Triple.isZ80())
    return nullptr; // This routine is only handling z80 targets.

  bool Is24Bit = Triple.getArch() == llvm::Triple::ez80;

  // Everything else goes to ez80 in 24-bit mode.
  if (Is24Bit)
    return "ez80";

  // Fallback to generic
  return "z80";
}

void z80::getZ80TargetFeatures(const Driver &D, const llvm::Triple &Triple,
                               const ArgList &Args,
                               std::vector<StringRef> &Features) {
  // Now add any that the user explicitly requested on the command line,
  // which may override the defaults.
  handleTargetFeaturesGroup(Args, Features, options::OPT_m_z80_Features_Group);
}
