//===--- Z80.cpp - Implement Z80 target feature support -------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements Z80 TargetInfo objects.
//
//===----------------------------------------------------------------------===//

#include "Z80.h"
#include "clang/Basic/TargetBuiltins.h"
#include "llvm/ADT/StringSwitch.h"

namespace clang {
namespace targets {

const Builtin::Info BuiltinInfoZ80[] = {
#define BUILTIN(ID, TYPE, ATTRS)                                               \
  {#ID, TYPE, ATTRS, nullptr, ALL_LANGUAGES, nullptr},
#define TARGET_BUILTIN(ID, TYPE, ATTRS, FEATURE)                               \
  {#ID, TYPE, ATTRS, nullptr, ALL_LANGUAGES, FEATURE},
#define TARGET_HEADER_BUILTIN(ID, TYPE, ATTRS, HEADER, LANGS, FEATURE)         \
  {#ID, TYPE, ATTRS, HEADER, LANGS, FEATURE},
#include "clang/Basic/BuiltinsZ80.def"

#define BUILTIN(ID, TYPE, ATTRS)                                               \
  {#ID, TYPE, ATTRS, nullptr, ALL_LANGUAGES, nullptr},
#define TARGET_BUILTIN(ID, TYPE, ATTRS, FEATURE)                               \
  {#ID, TYPE, ATTRS, nullptr, ALL_LANGUAGES, FEATURE},
#define TARGET_HEADER_BUILTIN(ID, TYPE, ATTRS, HEADER, LANGS, FEATURE)         \
  {#ID, TYPE, ATTRS, HEADER, LANGS, FEATURE},
#include "clang/Basic/BuiltinsEZ80.def"
};

static const char *const Z80GCCRegNames[] = {
    "a", "bc", "de", "hl", "ix", "iy", "sp",
};

static const char *const EZ80GCCRegNames[] = {
    "a", "bc", "de", "hl", "ix", "iy", "sps", "spl",
};

const TargetInfo::AddlRegName AddlRegNames[] = {
    {{"b", "c"}, 1},
    {{"d", "e"}, 2},
    {{"h", "l"}, 3},
    {{"ixh", "ixl"}, 4},
    {{"iyh", "iyl"}, 5},
};

} // namespace targets
} // namespace clang

using namespace clang;
using namespace clang::targets;

static unsigned matchAsmCCConstraint(const char *&Name) {
  return llvm::StringSwitch<unsigned>(Name)
      .Case("@ccnz", 5)
      .Case("@ccz", 4)
      .Case("@ccnc", 5)
      .Case("@ccc", 4)
      .Case("@ccpo", 5)
      .Case("@ccpe", 5)
      .Case("@ccp", 4)
      .Case("@ccm", 4)
      .Default(0);
}

void Z80TargetInfoBase::getTargetDefines(const LangOptions &Opts,
                                         MacroBuilder &Builder) const {
  // Inline assembly supports Z80 flag outputs.
  Builder.defineMacro("__GCC_ASM_FLAG_OUTPUTS__");
}

StringRef Z80TargetInfoBase::getConstraintRegister(StringRef Constraint,
                                                   StringRef Expression) const {
  Constraint =
      Constraint.drop_until([](char C) { return isalpha(C) || C == '@'; });
  if (!Constraint.empty())
    switch (Constraint.front()) {
    case 'a':
      return "a";
    case 'b':
    case 'c':
      return "bc";
    case 'd':
    case 'e':
      return "de";
    case 'h':
    case 'l':
      return "hl";
    case 'x':
      return "ix";
    case 'y':
      return "iy";
    case 'r':
    case 'R':
      return Expression;
    }
  return "";
}

bool Z80TargetInfoBase::validateAsmConstraint(
    const char *&Name, TargetInfo::ConstraintInfo &Info) const {
  switch (Name[0]) {
  case 'I': // bit offset within byte [0,7]
    Info.setRequiresImmediate(0, 7);
    return true;
  case 'J': // immediate [0,0xFF]
    Info.setRequiresImmediate(0, 0xFF);
    return true;
  case 'K': // immediate [0,0xFFFF]
    Info.setRequiresImmediate(0, 0xFFFF);
    return true;
  case 'L': // immediate [0,0xFFFFFF]
    Info.setRequiresImmediate(0, 0xFFFFFF);
    return true;
  case 'M': // im mode [0,2]
    Info.setRequiresImmediate(0, 2);
    return true;
  case 'N': // rst target [0,7]<<3
    Info.setRequiresImmediate(
        {0 << 3, 1 << 3, 2 << 3, 3 << 3, 4 << 3, 5 << 3, 6 << 3, 7 << 3});
    return true;
  case 'O': // signed offset [-128,127]
    Info.setRequiresImmediate(-128, 127);
    return true;
  case 'R': // reg including index
  case 'a': // reg a
  case 'b': // reg b
  case 'c': // reg c
  case 'd': // reg d
  case 'e': // reg e
  case 'h': // reg h
  case 'l': // reg l
    Info.setAllowsRegister();
    return true;
  case 'x': // reg ixh, ixl, ix
  case 'y': // reg iyh, iyl, iy
    switch (Name[1]) {
    case 'h': // reg ixh, iyh
    case 'l': // reg ixl, iyl
      ++Name;
      LLVM_FALLTHROUGH;
    default:
      Info.setAllowsRegister();
      return true;
    }
    break;
  case '@':
    // CC condition changes.
    if (unsigned Len = matchAsmCCConstraint(Name)) {
      Name += Len - 1;
      Info.setAllowsRegister();
      return true;
    }
    break;
  }
  return false;
}

std::string
Z80TargetInfoBase::convertConstraint(const char *&Constraint) const {
  StringRef Prefix = "{", Suffix = "}";
  unsigned Len = 0;
  switch (Constraint[0]) {
  case 'a':
  case 'b':
  case 'c':
  case 'd':
  case 'e':
  case 'h':
  case 'l':
    Len = 1;
    break;
  case 'x':
  case 'y':
    Prefix = "{i";
    switch (Constraint[1]) {
    case 'h':
    case 'l':
      Len = 2;
      break;
    default:
      Len = 1;
      break;
    }
    break;
  case '@':
    Len = matchAsmCCConstraint(Constraint);
    break;
  }
  if (!Len)
    return std::string(1, Constraint[0]);
  auto Converted = (Prefix + StringRef(Constraint, Len) + Suffix).str();
  Constraint += Len - 1;
  return Converted;
}

ArrayRef<TargetInfo::AddlRegName> Z80TargetInfoBase::getGCCAddlRegNames() const {
  return llvm::makeArrayRef(AddlRegNames);
}

bool Z80TargetInfo::setCPU(const std::string &Name) {
  return llvm::StringSwitch<bool>(Name)
    .Case("generic", true)
    .Case("z80",     true)
    .Case("z180",    true)
    .Default(false);
}

bool Z80TargetInfo::
initFeatureMap(llvm::StringMap<bool> &Features,
               DiagnosticsEngine &Diags, StringRef CPU,
               const std::vector<std::string> &FeaturesVec) const {
  if (CPU == "z80")
    Features["undoc"] = true;
  if (CPU == "z180")
    Features["z180"] = true;
  return TargetInfo::initFeatureMap(Features, Diags, CPU, FeaturesVec);
}

void Z80TargetInfo::getTargetDefines(const LangOptions &Opts,
                                     MacroBuilder &Builder) const {
  Z80TargetInfoBase::getTargetDefines(Opts, Builder);
  defineCPUMacros(Builder, "z80", /*Tuning=*/false);
  Builder.defineMacro("_Z80");
  if (getTargetOpts().CPU == "undoc") {
    defineCPUMacros(Builder, "z80_undoc", /*Tuning=*/false);
    Builder.defineMacro("_Z80_UNDOC");
  } else if (getTargetOpts().CPU == "z180") {
    defineCPUMacros(Builder, "z180", /*Tuning=*/false);
    Builder.defineMacro("_Z180");
  }
}

ArrayRef<Builtin::Info> Z80TargetInfo::getTargetBuiltins() const {
  return llvm::makeArrayRef(BuiltinInfoZ80, Z80::LastZ80CommonBuiltin -
                                                Builtin::FirstTSBuiltin + 1);
}

ArrayRef<const char *> Z80TargetInfo::getGCCRegNames() const {
  return llvm::makeArrayRef(Z80GCCRegNames);
}

bool EZ80TargetInfo::setCPU(const std::string &Name) {
  return llvm::StringSwitch<bool>(Name)
    .Case("generic", true)
    .Case("ez80",    true)
    .Default(false);
}

void EZ80TargetInfo::getTargetDefines(const LangOptions &Opts,
                                      MacroBuilder &Builder) const {
  Z80TargetInfoBase::getTargetDefines(Opts, Builder);
  defineCPUMacros(Builder, "ez80", /*Tuning=*/false);
  Builder.defineMacro("_EZ80");
}

ArrayRef<Builtin::Info> EZ80TargetInfo::getTargetBuiltins() const {
  return llvm::makeArrayRef(BuiltinInfoZ80,
                            Z80::LastTSBuiltin - Builtin::FirstTSBuiltin);
}

ArrayRef<const char *> EZ80TargetInfo::getGCCRegNames() const {
  return llvm::makeArrayRef(EZ80GCCRegNames);
}
