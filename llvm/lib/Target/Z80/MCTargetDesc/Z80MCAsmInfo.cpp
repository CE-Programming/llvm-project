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
#include "llvm/MC/MCExpr.h"
#include "llvm/Support/CommandLine.h"
using namespace llvm;

static cl::opt<bool> EscapeNonPrint(
    "z80-escape-non-print",
    cl::desc(
        "Avoid outputting non-printable ascii characters to assembly files."),
    cl::Hidden);

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
  AscizDirective = nullptr;
  Code32Directive = Code64Directive = nullptr;
  UseIntegratedAssembler = false;
  AssemblerDialect = !Is16Bit;
  HasFunctionAlignment = false;
  ExceptionsType = ExceptionHandling::SjLj;

  if (Z80GasStyle) {
    Code16Directive = ".assume\tADL = 0";
    Code24Directive = ".assume\tADL = 1";
    AsciiDirective = ByteListDirective = Data8bitsDirective = "\tdb\t";
    Data16bitsDirective = "\tdw\t";
    Data24bitsDirective = "\td24\t";
    Data32bitsDirective = "\td32\t";
  } else {
    Code16Directive = "assume\tadl = 0";
    Code24Directive = "assume\tadl = 1";
    DollarIsPC = true;
    SeparatorString = nullptr;
    PrivateGlobalPrefix = PrivateLabelPrefix = "";
    SupportsQuotedNames = false;
    ZeroDirective = nullptr;
    BlockSeparator = " dup ";
    AsciiDirective = ByteListDirective = Data8bitsDirective = "\tdb\t";
    NumberLiteralSyntax = ANLS_PlainDecimal;
    CharacterLiteralSyntax = ACLS_SingleQuotes;
    HasPairedDoubleQuoteStringConstants = true;
    HasBackslashEscapesInStringConstants = false;
    StringConstantsEscapeNonPrint = EscapeNonPrint;
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
    HasDotTypeDotSizeDirective = false;
    IdentDirective = "\tident\t";
    WeakDirective = "\tweak\t";
    UseLogicalShr = false;
    HasSingleParameterDotFile = false;
    SupportsDebugInformation = SupportsCFI = true;
    DwarfFileDirective = "\tfile\t";
    DwarfLocDirective = "\tloc\t";
    DwarfCFIDirectivePrefix = "\tcfi_";
  }
}

MCSection *Z80MCAsmInfoELF::getNonexecutableStackSection(MCContext &Ctx) const {
  return nullptr;
}

bool Z80MCAsmInfoELF::isAcceptableChar(char C) const {
  return Z80GasStyle ? MCAsmInfo::isAcceptableChar(C) : (MCAsmInfo::isAcceptableChar(C) || C == '%' || C == '^');
}

bool Z80MCAsmInfoELF::shouldOmitSectionDirective(StringRef SectionName) const {
  return false;
}

const char *Z80MCAsmInfoELF::getBlockDirective(int64_t Size) const {
  if (Z80GasStyle) {
    return MCAsmInfoELF::getBlockDirective(Size);
  }
  switch (Size) {
  default: return nullptr;
  case 1: return "\tdb\t";
  case 2: return "\tdw\t";
  case 3: return "\tdl\t";
  case 4: return "\tdd\t";
  }
}

const char *Z80MCAsmInfoELF::getUnaryOperator(unsigned Opc) const {
  if (Z80GasStyle) {
    return MCAsmInfoELF::getUnaryOperator(Opc);
  }
  switch (Opc) {
  default: llvm_unreachable("unknown opcode");
  case MCUnaryExpr::LNot:  return "~";
  case MCUnaryExpr::Minus: return "-";
  case MCUnaryExpr::Not:   return "not ";
  case MCUnaryExpr::Plus:  return "+";
  }
}

const char *Z80MCAsmInfoELF::getBinaryOperator(unsigned Opc) const {
  if (Z80GasStyle) {
    return MCAsmInfoELF::getBinaryOperator(Opc);
  }
  switch (Opc) {
  default: llvm_unreachable("unknown opcode");
  case MCBinaryExpr::Add:   return     "+";
  case MCBinaryExpr::AShr:  return " shr ";
  case MCBinaryExpr::And:   return " and ";
  case MCBinaryExpr::Div:   return     "/";
  case MCBinaryExpr::EQ:    return     "=";
  case MCBinaryExpr::GT:    return     ">";
  case MCBinaryExpr::GTE:   return    ">=";
  case MCBinaryExpr::LAnd:  return     "&";
  case MCBinaryExpr::LOr:   return     "|";
  case MCBinaryExpr::LShr:  return " shr ";
  case MCBinaryExpr::LT:    return     "<";
  case MCBinaryExpr::LTE:   return    "<=";
  case MCBinaryExpr::Mod:   return " mod ";
  case MCBinaryExpr::Mul:   return     "*";
  case MCBinaryExpr::NE:    return    "<>";
  case MCBinaryExpr::Or:    return  " or ";
  case MCBinaryExpr::OrNot: return     "~";
  case MCBinaryExpr::Shl:   return " shl ";
  case MCBinaryExpr::Sub:   return     "-";
  case MCBinaryExpr::Xor:   return " xor ";
  }
}
