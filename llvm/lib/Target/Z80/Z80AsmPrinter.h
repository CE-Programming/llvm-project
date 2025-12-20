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

#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/MC/SectionKind.h"

namespace llvm {

class Z80Subtarget;

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

  void SetupMachineFunction(MachineFunction &MF) override;

  void emitStartOfAsmFile(Module &M) override;
  void emitInlineAsmEnd(const MCSubtargetInfo &StartInfo,
                        const MCSubtargetInfo *EndInfo) const override;
  void emitEndOfAsmFile(Module &M) override;
  void emitGlobalVariable(const GlobalVariable *GV) override;
  void emitGlobalAlias(Module &M, const GlobalAlias &GA) override;
  void emitInstruction(const MachineInstr *MI) override;

private:
  SectionKind SwitchSectionForGlobal(const GlobalObject *GO);

  void PrintOperand(const MachineInstr *MI, unsigned OpNum, unsigned SubRegIdx,
                    raw_ostream &O);
  bool PrintAsmOperand(const MachineInstr *MI, unsigned OpNum,
                       const char *ExtraCode, raw_ostream &O) override;
  bool PrintAsmMemoryOperand(const MachineInstr *MI, unsigned OpNum,
                             const char *ExtraCode, raw_ostream &O) override;
};
} // End llvm namespace

#endif
