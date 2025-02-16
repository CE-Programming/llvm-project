//===- Z80TargetStreamer.cpp - Z80TargetStreamer class --*- C++ -*---------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the Z80TargetStreamer class.
//
//===----------------------------------------------------------------------===//

#include "Z80TargetStreamer.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FormattedStream.h"

using namespace llvm;

extern cl::opt<bool> GasStyle;

Z80TargetStreamer::Z80TargetStreamer(MCStreamer &S)
    : MCTargetStreamer(S) {}

Z80TargetAsmStreamer::Z80TargetAsmStreamer(MCStreamer &S,
                                           formatted_raw_ostream &OS)
    : Z80TargetStreamer(S), MAI(S.getContext().getAsmInfo()), OS(OS) {}

void Z80TargetAsmStreamer::emitLabel(MCSymbol *Symbol) {
  if (Symbol->isTemporary())
    emitLocal(Symbol);
}

void Z80TargetAsmStreamer::emitAlign(Align Alignment) {
  if (auto Mask = Alignment.value() - 1)
    GasStyle ? OS << "\t.skip\t($$ - $) and " << Mask << '\n' : OS << "\trb\t($$ - $) and " << Mask << '\n';
}

void Z80TargetAsmStreamer::emitBlock(uint64_t NumBytes) {
  if (NumBytes)
    GasStyle ? OS << "\t.skip\t" << NumBytes << '\n' : OS << "\trb\t" << NumBytes << '\n';
}

void Z80TargetAsmStreamer::emitLocal(MCSymbol *Symbol) {
  if (!GasStyle) {
    OS << "\tprivate\t";
    Symbol->print(OS, MAI);
    OS << '\n';
  }
}

void Z80TargetAsmStreamer::emitWeakGlobal(MCSymbol *Symbol) {
  GasStyle ? OS << "\t.weak\t" : OS << "\tweak\t";
  Symbol->print(OS, MAI);
  OS << '\n';
}

void Z80TargetAsmStreamer::emitGlobal(MCSymbol *Symbol) {
  GasStyle ? OS << "\t.globl\t" : OS << "\tpublic\t";
  Symbol->print(OS, MAI);
  OS << '\n';
}

void Z80TargetAsmStreamer::emitExtern(MCSymbol *Symbol) {
  GasStyle ? OS << "\t.extern\t" : OS << "\textern\t";
  Symbol->print(OS, MAI);
  OS << '\n';
}
