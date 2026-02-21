//===- Z80InstPrinter.cpp - Convert Z80 MCInst to assembly ------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file includes code common for rendering MCInst instances as (e)Z80
// assembly.
//
//===----------------------------------------------------------------------===//

#include "Z80InstPrinterCommon.h"
#include "Z80InstPrinter.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCRegister.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FormattedStream.h"

using namespace llvm;

#define DEBUG_TYPE "asm-printer"

static cl::opt<bool> AddNegativeOffset(
    "z80-add-negative-offset",
    cl::desc("Print ix + -1 instead of ix - 1"),
    cl::init(false),
    cl::Hidden);

Z80InstPrinterCommon::Z80InstPrinterCommon(const MCAsmInfo &MAI,
                                           const MCInstrInfo &MII,
                                           const MCRegisterInfo &MRI)
    : MCInstPrinter(MAI, MII, MRI), AddNegativeOffset(::AddNegativeOffset) {}

bool Z80InstPrinterCommon::applyTargetSpecificCLOption(StringRef Opt) {
  if (Opt == "add-negative-offset") {
    AddNegativeOffset = true;
    return true;
  }
  if (Opt == "subtract-positive-offset") {
    AddNegativeOffset = false;
    return true;
  }
  return false;
}

void Z80InstPrinterCommon::printRegName(raw_ostream &OS, MCRegister Reg) const {
  markup(OS, Markup::Register) << getRegName(Reg);
}

void Z80InstPrinterCommon::printInst(const MCInst *MI, uint64_t Address,
                                     StringRef Annot,
                                     const MCSubtargetInfo &STI,
                                     raw_ostream &OS) {
  printInstruction(MI, Address, OS);
  printAnnotation(OS, Annot);
}

void Z80InstPrinterCommon::printOperand(const MCInst *MI, unsigned OpNo,
                                        raw_ostream &OS) {
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isReg())
    printRegName(OS, Op.getReg());
  else if (Op.isImm())
    markup(OS, Markup::Immediate) << formatImm(Op.getImm());
  else {
    assert(Op.isExpr() && "unknown operand kind in printOperand");
    WithMarkup M = markup(OS, Markup::Immediate);
    Op.getExpr()->print(OS, &MAI);
  }
}

void Z80InstPrinterCommon::printOperand(const MCInst *MI, uint64_t Address,
                                        unsigned Op, raw_ostream &OS) {
  return printOperand(MI, Op, OS);
}

void Z80InstPrinterCommon::printCondCode(const MCInst *MI, unsigned Op,
                                         raw_ostream &OS) {
  WithMarkup M = markup(OS, Markup::Immediate);
  switch (unsigned CondCode = MI->getOperand(Op).getImm()) {
  default:
    llvm_unreachable("Invalid condition code operand!");
  case 0:
    OS << 'n';
    LLVM_FALLTHROUGH;
  case 1:
    OS << 'z';
    break;
  case 2:
    OS << 'n';
    LLVM_FALLTHROUGH;
  case 3:
    OS << 'c';
    break;
  case 4:
  case 5:
  case 6:
    OS << 'p';
    switch (CondCode) {
    case 4:
      OS << 'o';
      break;
    case 5:
      OS << 'e';
      break;
    }
    break;
  case 7:
    OS << 'm';
    break;
  }
}

void Z80InstPrinterCommon::printOffset(const MCInst *MI, unsigned Op,
                                       raw_ostream &OS) {
  printOperand(MI, Op, OS);
  auto Offset = MI->getOperand(Op + 1).getImm();
  assert(isInt<8>(Offset) && "Offset out of range!");
  OS << ' ' << (Offset >= 0 || AddNegativeOffset ? '+' : '-') << ' '
       << formatImm(AddNegativeOffset ? Offset : std::abs(Offset));
}

void Z80InstPrinterCommon::printIndirect(const MCInst *MI, unsigned Op,
                                         raw_ostream &OS) {
  WithMarkup M = markup(OS, Markup::Memory);
  OS << '(';
  printOperand(MI, Op, OS);
  OS << ')';
}

void Z80InstPrinterCommon::printIndirectOffset(const MCInst *MI, unsigned Op,
                                               raw_ostream &OS) {
  WithMarkup M = markup(OS, Markup::Memory);
  OS << '(';
  printOffset(MI, Op, OS);
  OS << ')';
}
