//===-- Z80AsmPrinter.cpp - Convert Z80 LLVM code to AT&T assembly --------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains a printer that converts from our internal representation
// of machine-dependent LLVM code to Z80 machine code.
//
//===----------------------------------------------------------------------===//

#include "Z80AsmPrinter.h"
#include "Z80.h"
#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "MCTargetDesc/Z80TargetStreamer.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Target/TargetLoweringObjectFile.h"
#include "llvm/Target/TargetMachine.h"
using namespace llvm;

//===----------------------------------------------------------------------===//
// Target Registry Stuff
//===----------------------------------------------------------------------===//

static bool isCode16(const Triple &TT) {
  return TT.getEnvironment() == Triple::CODE16;
}

void Z80AsmPrinter::emitStartOfAsmFile(Module &M) {
  const Triple &TT = TM.getTargetTriple();
  if (TT.getArch() == Triple::ez80)
    OutStreamer->emitAssemblerFlag(isCode16(TT) ? MCAF_Code16 : MCAF_Code24);
}

void Z80AsmPrinter::emitInlineAsmEnd(const MCSubtargetInfo &StartInfo,
                                     const MCSubtargetInfo *EndInfo) const {
  bool Was16 = isCode16(StartInfo.getTargetTriple());
  if (!EndInfo || Was16 != isCode16(EndInfo->getTargetTriple()))
    OutStreamer->emitAssemblerFlag(Was16 ? MCAF_Code16 : MCAF_Code24);
}

void Z80AsmPrinter::emitEndOfAsmFile(Module &M) {
  Z80TargetStreamer *TS =
      static_cast<Z80TargetStreamer *>(OutStreamer->getTargetStreamer());
  for (const auto &Symbol : OutContext.getSymbols())
    if (!Symbol.second->isDefined())
      TS->emitExtern(Symbol.second);
}

void Z80AsmPrinter::emitGlobalVariable(const GlobalVariable *GV) {
  Z80TargetStreamer *TS =
      static_cast<Z80TargetStreamer *>(OutStreamer->getTargetStreamer());
  const DataLayout &DL = GV->getParent()->getDataLayout();

  if (GV->hasInitializer()) {
    // Check to see if this is a special global used by LLVM, if so, emit it.
    if (emitSpecialLLVMGlobal(GV))
      return;
  }

  if (!GV->hasInitializer())   // External globals require no extra code.
    return;

  MCSymbol *GVSym = getSymbol(GV);

  GVSym->redefineIfPossible();
  if (GVSym->isDefined() || GVSym->isVariable())
    report_fatal_error("symbol '" + Twine(GVSym->getName()) +
                       "' is already defined");

  SectionKind GVKind = TargetLoweringObjectFile::getKindForGlobal(GV, TM);

  // Determine to which section this global should be emitted.
  OutStreamer->SwitchSection(
      getObjFileLowering().SectionForGlobal(GV, GVKind, TM));

  // If the alignment is specified, we *must* obey it.  Overaligning a global
  // with a specified alignment is a prompt way to break globals emitted to
  // sections and expected to be contiguous (e.g. ObjC metadata).
  TS->emitAlign(DL.getPreferredAlign(GV));

  if (!GV->hasLocalLinkage())
    TS->emitGlobal(GVSym);
  OutStreamer->emitLabel(GVSym);
  if (GVKind.isBSS())
    TS->emitBlock(DL.getTypeAllocSize(GV->getType()->getElementType()));
  else
    emitGlobalConstant(DL, GV->getInitializer());
  OutStreamer->AddBlankLine();
}

// Force static initialization.
extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeZ80AsmPrinter() {
  RegisterAsmPrinter<Z80AsmPrinter> X(getTheZ80Target());
  RegisterAsmPrinter<Z80AsmPrinter> Y(getTheEZ80Target());
}
