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
#include "llvm/ADT/StringSwitch.h"
#include "llvm/ADT/Triple.h"
using namespace llvm;

void Z80MCAsmInfoELF::anchor() { }

Z80MCAsmInfoELF::Z80MCAsmInfoELF(const Triple &T) {
  bool Is16Bit = T.isArch16Bit() || T.getEnvironment() == Triple::CODE16;
  CodePointerSize = CalleeSaveStackSlotSize = Is16Bit ? 2 : 3;
  MaxInstLength = 6;
  DollarIsPC = true;
  SeparatorString = nullptr;
  CommentString = ";";
  PrivateGlobalPrefix = PrivateLabelPrefix = "";
  Code16Directive = "assume\tadl = 0";
  Code24Directive = "assume\tadl = 1";
  Code32Directive = Code64Directive = nullptr;
  AssemblerDialect = !Is16Bit;
  SupportsQuotedNames = false;
  ZeroDirective = AscizDirective = nullptr;
  BlockSeparator = " dup ";
  AsciiDirective = ByteListDirective = Data8bitsDirective = "\tdb\t";
  NumberLiteralSyntax = ANLS_PlainDecimal;
  CharacterLiteralSyntax = ACLS_SingleQuotes;
  HasPairedDoubleQuoteStringConstants = true;
  HasBackslashEscapesInStringConstants = false;
  StringConstantsEscapeNonPrint = false;
  StringConstantsRequiredEscapes = {"\n\r\32", 4}; // include null
  Data16bitsDirective = "\tdw\t";
  Data24bitsDirective = "\tdl\t";
  Data32bitsDirective = "\tdd\t";
  Data64bitsDirective = "\tdq\t";
  DataULEB128Directive = "\tuleb128\t";
  DataSLEB128Directive = "\tsleb128\t";
  SectionDirective = "\tsection\t";
  AlwaysChangeSection = true;
  GlobalDirective = "\tpublic\t";
  LGloblDirective = "\tprivate\t";
  SetDirective = "\tlabel\t";
  SetSeparator = " at ";
  HasFunctionAlignment = false;
  HasDotTypeDotSizeDirective = false;
  IdentDirective = "\tident\t";
  WeakDirective = "\tweak\t";
  UseIntegratedAssembler = false;
  UseLogicalShr = false;
  HasSingleParameterDotFile = false;
  SupportsDebugInformation = SupportsCFI = true;
  ExceptionsType = ExceptionHandling::SjLj;
  DwarfFileDirective = "\tfile\t";
  DwarfLocDirective = "\tloc\t";
  DwarfCFIDirectivePrefix = "\tcfi_";
}

MCSection *Z80MCAsmInfoELF::getNonexecutableStackSection(MCContext &Ctx) const {
  return nullptr;
}

bool Z80MCAsmInfoELF::isAcceptableChar(char C) const {
  return MCAsmInfo::isAcceptableChar(C) || C == '%' || C == '^';
}

bool Z80MCAsmInfoELF::shouldOmitSectionDirective(StringRef SectionName) const {
  return false;
}

const char *Z80MCAsmInfoELF::getBlockDirective(int64_t Size) const {
  switch (Size) {
  default: return nullptr;
  case 1: return "\tdb\t";
  case 2: return "\tdw\t";
  case 3: return "\tdl\t";
  case 4: return "\tdd\t";
  }
}
