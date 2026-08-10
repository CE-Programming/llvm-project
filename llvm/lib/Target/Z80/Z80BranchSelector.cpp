//===-- Z80BranchSelector.cpp - Select relative or absolute branches -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "Z80.h"
#include "Z80InstrInfo.h"
#include "Z80Subtarget.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/Support/MathExtras.h"

using namespace llvm;

#define DEBUG_TYPE "z80-branch-select"

namespace {

class Z80BranchSelector : public MachineFunctionPass {
public:
  static char ID;
  Z80BranchSelector() : MachineFunctionPass(ID) {}

  MachineFunctionProperties getRequiredProperties() const override {
    return MachineFunctionProperties().set(
        MachineFunctionProperties::Property::NoVRegs);
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) override;
  StringRef getPassName() const override { return "Z80 Branch Selector"; }
};

} // end anonymous namespace

bool Z80BranchSelector::runOnMachineFunction(MachineFunction &MF) {
  bool Changed = false;
  const auto &STI = MF.getSubtarget<Z80Subtarget>();
  const auto &TII = *STI.getInstrInfo();

  const auto isConditional = [](unsigned Opcode) {
    switch (Opcode) {
    case Z80::JQCC:
    case Z80::JRCC:
    case Z80::JP16CC:
    case Z80::JP24CC:
      return true;
    default:
      return false;
    }
  };
  const auto makeRelative = [&](MachineInstr &MI) {
    unsigned Opcode = isConditional(MI.getOpcode()) ? Z80::JRCC : Z80::JR;
    if (MI.getOpcode() != Opcode) {
      MI.setDesc(TII.get(Opcode));
      Changed = true;
    }
  };
  const auto makeAbsolute = [&](MachineInstr &MI) {
    unsigned Opcode = isConditional(MI.getOpcode())
                          ? STI.is24Bit() ? Z80::JP24CC : Z80::JP16CC
                          : STI.is24Bit() ? Z80::JP24 : Z80::JP16;
    if (MI.getOpcode() != Opcode) {
      MI.setDesc(TII.get(Opcode));
      Changed = true;
    }
  };

  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      switch (MI.getOpcode()) {
      case Z80::JQCC:
        if (Z80::CondCode(MI.getOperand(1).getImm()) >
            Z80::CondCode::LAST_SIMPLE_COND) {
          makeAbsolute(MI);
          break;
        }
        [[fallthrough]];
      case Z80::JQ:
        if (MI.getOperand(0).isMBB())
          makeRelative(MI);
        else
          makeAbsolute(MI);
        break;
      case Z80::JRCC:
        if (Z80::CondCode(MI.getOperand(1).getImm()) >
            Z80::CondCode::LAST_SIMPLE_COND)
          makeAbsolute(MI);
        break;
      case Z80::JR:
        if (!MI.getOperand(0).isMBB())
          makeAbsolute(MI);
        break;
      default:
        break;
      }
    }
  }

  const unsigned LongestInstructionSize =
      4 + STI.hasEZ80Ops() + STI.is24Bit();
  const auto getSize = [&](const MachineInstr &MI) {
    if (!MI.isPseudo())
      return TII.getInstSizeInBytes(MI);
    switch (MI.getOpcode()) {
    case TargetOpcode::INLINEASM:
    case TargetOpcode::INLINEASM_BR: {
      bool BeginningOfLine = true;
      unsigned Lines = 0;
      for (const char *C = MI.getOperand(0).getSymbolName(); *C; ++C) {
        switch (*C) {
        default:
          Lines += BeginningOfLine;
          [[fallthrough]];
        case ';':
          BeginningOfLine = false;
          break;
        case '\n':
        case '\r':
          BeginningOfLine = true;
          break;
        case '\f':
        case '\t':
        case '\v':
        case ' ':
          break;
        }
      }
      return Lines * LongestInstructionSize;
    }
    default:
      return 128U;
    }
  };

  MF.RenumberBlocks();
  SmallVector<int64_t> BlockOffsets(MF.getNumBlockIDs());
  bool Widened;
  do {
    int64_t Offset = 0;
    for (MachineBasicBlock &MBB : MF) {
      BlockOffsets[MBB.getNumber()] = Offset;
      for (MachineInstr &MI : MBB)
        Offset += getSize(MI);
    }

    Widened = false;
    for (MachineBasicBlock &MBB : MF) {
      Offset = BlockOffsets[MBB.getNumber()];
      for (MachineInstr &MI : MBB) {
        const unsigned Size = getSize(MI);
        if ((MI.getOpcode() == Z80::JR || MI.getOpcode() == Z80::JRCC) &&
            MI.getOperand(0).isMBB()) {
          const MachineBasicBlock &Target = *MI.getOperand(0).getMBB();
          const int64_t Displacement =
              BlockOffsets[Target.getNumber()] - (Offset + Size);
          if (!isInt<8>(Displacement)) {
            makeAbsolute(MI);
            Widened = true;
          }
        }
        Offset += Size;
      }
    }
  } while (Widened);

  return Changed;
}

char Z80BranchSelector::ID = 0;
INITIALIZE_PASS(Z80BranchSelector, DEBUG_TYPE, "Z80 Branch Selector", false,
                false)

FunctionPass *llvm::createZ80BranchSelectorPass() {
  return new Z80BranchSelector();
}
