//===-- Z80AsmPrinter.h - Z80 implementation of AsmPrinter ------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_Z80_Z80ASMPRINTER_H
#define LLVM_LIB_TARGET_Z80_Z80ASMPRINTER_H

#include "Z80Subtarget.h"
#include "llvm/CodeGen/AsmPrinter.h"

namespace llvm {

class LLVM_LIBRARY_VISIBILITY Z80AsmPrinter : public AsmPrinter {
  const Z80Subtarget *Subtarget;

public:
  explicit Z80AsmPrinter(TargetMachine &TM,
                         std::unique_ptr<MCStreamer> Streamer)
    : AsmPrinter(TM, std::move(Streamer)) {}

  StringRef getPassName() const override {
    return "Z80 Assembly / Object Emitter";
  }

  const Z80Subtarget &getSubtarget() const { return *Subtarget; }


  void emitStartOfAsmFile(Module &M) override;
  void emitInlineAsmEnd(const MCSubtargetInfo &StartInfo,
                        const MCSubtargetInfo *EndInfo) const override;
  void emitEndOfAsmFile(Module &M) override;
  void emitGlobalVariable(const GlobalVariable *GV) override;
  void emitInstruction(const MachineInstr *MI) override;
};
} // End llvm namespace

#endif
