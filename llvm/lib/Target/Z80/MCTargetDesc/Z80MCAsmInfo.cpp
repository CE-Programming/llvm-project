//===-- Z80MCAsmInfo.cpp - Z80 asm properties -----------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the declarations of the Z80MCAsmInfo properties.
//
//===----------------------------------------------------------------------===//

#include "Z80MCAsmInfo.h"
#include "llvm/TargetParser/Triple.h"
#include "llvm/Support/CommandLine.h"
using namespace llvm;

cl::opt<bool> Z80GasStyle(
    "z80-gas-style",
    cl::desc("Use GAS style assembly syntax instead of FASMG style."),
    cl::NotHidden);

void Z80MCAsmInfoELF::anchor() { }

Z80MCAsmInfoELF::Z80MCAsmInfoELF(const Triple &T) {
  bool Is16Bit = T.isArch16Bit() || T.getEnvironment() == Triple::CODE16;
  CodePointerSize = CalleeSaveStackSlotSize = Is16Bit ? 2 : 3;
  MaxInstLength = 6;

  // Common to both GAS and fasmg
  CommentString = ";";
  UseIntegratedAssembler = false;
  AssemblerDialect = !Is16Bit;
  HasFunctionAlignment = false;
  ExceptionsType = ExceptionHandling::SjLj;
  UseOctalEscapeForQuote = true;
  SupportsDebugInformation = true;
  HasIdentDirective = Z80GasStyle;

  if (Z80GasStyle) {
    AsciiDirective = "\t.ascii\t";
    AscizDirective = "\t.asciz\t";
    ZeroDirective = "\t.zero\t";
    Data8bitsDirective = "\tdb\t";
    Data16bitsDirective = "\tdw\t";
    Data24bitsDirective = "\td24\t";
    Data32bitsDirective = "\td32\t";
  } else {
    DollarIsPC = true;
    SeparatorString = nullptr;
    PrivateGlobalPrefix = PrivateLabelPrefix = "";
    SupportsQuotedNames = false;
    ZeroDirective = nullptr;
    AscizDirective = nullptr;
    AsciiDirective = Data8bitsDirective = "\tdb\t";
    CharacterLiteralSyntax = ACLS_SingleQuotePrefix;
    Data16bitsDirective = "\tdw\t";
    Data24bitsDirective = "\tdl\t";
    Data32bitsDirective = "\tdd\t";
    Data64bitsDirective = "\tdq\t";
    GlobalDirective = "\tpublic\t";
    HasDotTypeDotSizeDirective = false;
    WeakDirective = "\tweak\t";
    UseLogicalShr = false;
    HasSingleParameterDotFile = false;
  }
}

bool Z80MCAsmInfoELF::isAcceptableChar(char C) const {
  return Z80GasStyle ? MCAsmInfo::isAcceptableChar(C) : (MCAsmInfo::isAcceptableChar(C) || C == '%' || C == '^');
}

bool Z80MCAsmInfoELF::shouldOmitSectionDirective(StringRef SectionName) const {
  return false;
}
