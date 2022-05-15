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
#include "MCTargetDesc/Z80InstPrinter.h"
#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "MCTargetDesc/Z80TargetStreamer.h"
#include "Z80.h"
#include "Z80Subtarget.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Target/TargetLoweringObjectFile.h"
#include "llvm/Target/TargetMachine.h"
using namespace llvm;

//===----------------------------------------------------------------------===//
// Target Registry Stuff
//===----------------------------------------------------------------------===//

static bool isCode16(const Triple &TT) {
  return TT.getEnvironment() == Triple::CODE16;
}

void Z80AsmPrinter::SetupMachineFunction(MachineFunction &MF) {
  Subtarget = &MF.getSubtarget<Z80Subtarget>();
  AsmPrinter::SetupMachineFunction(MF);
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

  if (GV->hasLocalLinkage())
    TS->emitLocal(GVSym);
  else if (GV->isWeakForLinker())
    TS->emitWeakGlobal(GVSym);
  else
    TS->emitGlobal(GVSym);
  OutStreamer->emitLabel(GVSym);
  if (GVKind.isBSS())
    TS->emitBlock(DL.getTypeAllocSize(GV->getValueType()));
  else
    emitGlobalConstant(DL, GV->getInitializer());
  OutStreamer->AddBlankLine();
}

void Z80AsmPrinter::PrintOperand(const MachineInstr *MI, unsigned OpNum,
                                 unsigned SubRegIdx, raw_ostream &O) {
  const MachineOperand &MO = MI->getOperand(OpNum);
  switch (MO.getType()) {
  default:
    llvm_unreachable("<unknown operand type>");
  case MachineOperand::MO_Register: {
    Register Reg = MO.getReg();
    assert(Register::isPhysicalRegister(Reg));
    assert(!MO.getSubReg() && "Subregs should be eliminated!");
    if (SubRegIdx)
      if (Register SubReg =
              getSubtarget().getRegisterInfo()->getSubReg(Reg, SubRegIdx))
        Reg = SubReg;
    O << Z80InstPrinter::getRegisterName(Reg);
    break;
  }
  case MachineOperand::MO_Immediate: {
    O << MO.getImm();
    break;
  }
  case MachineOperand::MO_GlobalAddress: {
    PrintSymbolOperand(MO, O);
    break;
  }
  case MachineOperand::MO_BlockAddress: {
    MCSymbol *Sym = GetBlockAddressSymbol(MO.getBlockAddress());
    Sym->print(O, MAI);
    break;
  }
  }
}

bool Z80AsmPrinter::PrintAsmOperand(const MachineInstr *MI, unsigned OpNum,
                                    const char *ExtraCode, raw_ostream &O) {
  // First try the generic code, which knows about modifiers like 'c' and 'n'.
  if (!AsmPrinter::PrintAsmOperand(MI, OpNum, ExtraCode, O))
    return false;

  // Does this asm operand have a single letter operand modifier?
  unsigned SubIdx = Z80::NoSubRegister;
  if (ExtraCode && ExtraCode[0])
    switch (ExtraCode[0]) {
    default:
      return true; // Unknown modifier.
    case 'b':
      SubIdx = Z80::sub_low;
      break;
    case 'h':
      SubIdx = Z80::sub_high;
      break;
    case 'w':
      SubIdx = Z80::sub_short;
      break;
    }

  PrintOperand(MI, OpNum, SubIdx, O);
  return false;
}

bool Z80AsmPrinter::PrintAsmMemoryOperand(const MachineInstr *MI,
                                          unsigned OpNum, const char *ExtraCode,
                                          raw_ostream &O) {
  // First try the generic code.
  if (!AsmPrinter::PrintAsmMemoryOperand(MI, OpNum, ExtraCode, O))
    return false;

  // Does this asm operand have a single letter operand modifier?
  if (ExtraCode && ExtraCode[0])
    return true; // Unknown modifier.

  O << '(';
  PrintOperand(MI, OpNum, Z80::NoSubRegister, O);
  O << ')';
  return false;
}

// Force static initialization.
extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeZ80AsmPrinter() {
  RegisterAsmPrinter<Z80AsmPrinter> X(getTheZ80Target());
  RegisterAsmPrinter<Z80AsmPrinter> Y(getTheEZ80Target());
}
