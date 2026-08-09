//===-- Z80R64SpillPass.cpp - Wide Register Decomposition Pass ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Decompose eZ80's artificial 32- and 64-bit register tuples into native
// scalar registers that are allocated independently.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "Z80.h"
#include "Z80InstrInfo.h"
#include "Z80RegisterInfo.h"
#include "Z80Subtarget.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "z80-r64-spill"

using namespace llvm;

namespace {

struct Part {
  unsigned SubReg;
  const TargetRegisterClass *RegClass;
};

struct PartRef {
  unsigned Index;
  unsigned SubReg;
};

class Z80R64SpillPass : public MachineFunctionPass {
public:
  static char ID;
  Z80R64SpillPass() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override {
    return "Z80 Wide Register Decomposition Pass";
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  const Z80InstrInfo *TII = nullptr;
  const Z80RegisterInfo *TRI = nullptr;
  MachineRegisterInfo *MRI = nullptr;
  DenseMap<Register, SmallVector<Register, 3>> Components;

  ArrayRef<Part> getParts(const TargetRegisterClass *RegClass) const;
  ArrayRef<Part> getParts(Register Reg) const;
  std::optional<PartRef> getPart(ArrayRef<Part> Parts,
                                 unsigned SubReg) const;
  SmallVector<Register, 3> getOrCreateComponents(Register Reg);
  SmallVector<Register, 3> getSourceComponents(Register Reg,
                                                ArrayRef<Part> Parts,
                                                const MachineInstr &MI);
  bool rewriteDef(MachineInstr &MI, Register Reg);
  void rewriteUses(Register Reg, DenseSet<MachineInstr *> &ToErase);
  [[noreturn]] void fail(const Twine &Reason, const MachineInstr &MI) const;
};

} // end anonymous namespace

char Z80R64SpillPass::ID = 0;

INITIALIZE_PASS(Z80R64SpillPass, DEBUG_TYPE,
                "Decompose wide registers into components", false, false)

ArrayRef<Part>
Z80R64SpillPass::getParts(const TargetRegisterClass *RegClass) const {
  static const Part R32Parts[] = {
      {Z80::sub_low24, &Z80::R24RegClass},
      {Z80::sub_high8, &Z80::R8RegClass},
  };
  static const Part R64Parts[] = {
      {Z80::sub_low24, &Z80::R24RegClass},
      {Z80::sub_mid24, &Z80::R24RegClass},
      {Z80::sub_word3, &Z80::R16RegClass},
  };

  if (!RegClass)
    return {};
  if (Z80::R32_24RegClass.hasSubClassEq(RegClass))
    return R32Parts;
  if (Z80::R64_24RegClass.hasSubClassEq(RegClass))
    return R64Parts;
  return {};
}

ArrayRef<Part> Z80R64SpillPass::getParts(Register Reg) const {
  if (!Reg.isVirtual())
    return {};
  return getParts(MRI->getRegClassOrNull(Reg));
}

std::optional<PartRef>
Z80R64SpillPass::getPart(ArrayRef<Part> Parts, unsigned SubReg) const {
  for (auto [Index, P] : enumerate(Parts))
    if (P.SubReg == SubReg)
      return PartRef{static_cast<unsigned>(Index), 0};

  if (!Parts.empty() && Parts.front().SubReg == Z80::sub_low24 &&
      (SubReg == Z80::sub_short || SubReg == Z80::sub_low ||
       SubReg == Z80::sub_high))
    return PartRef{0, SubReg};
  return std::nullopt;
}

SmallVector<Register, 3>
Z80R64SpillPass::getOrCreateComponents(Register Reg) {
  auto [It, Inserted] = Components.try_emplace(Reg);
  if (Inserted) {
    for (const Part &P : getParts(Reg))
      It->second.push_back(MRI->createVirtualRegister(P.RegClass));
    LLVM_DEBUG(dbgs() << "Decomposing " << printReg(Reg, TRI) << '\n');
  }
  return It->second;
}

SmallVector<Register, 3> Z80R64SpillPass::getSourceComponents(
    Register Reg, ArrayRef<Part> Parts, const MachineInstr &MI) {
  if (Reg.isVirtual()) {
    if (getParts(Reg).size() != Parts.size())
      fail("wide source has an incompatible register class", MI);
    return getOrCreateComponents(Reg);
  }

  SmallVector<Register, 3> Result;
  for (const Part &P : Parts) {
    Register SubReg = TRI->getSubReg(Reg, P.SubReg);
    if (!SubReg)
      fail("physical wide source lacks a component", MI);
    Result.push_back(SubReg);
  }
  return Result;
}

[[noreturn]] void Z80R64SpillPass::fail(const Twine &Reason,
                                        const MachineInstr &MI) const {
  std::string Message;
  raw_string_ostream OS(Message);
  OS << Reason << ": " << MI;
  report_fatal_error(StringRef(OS.str()));
}

bool Z80R64SpillPass::rewriteDef(MachineInstr &MI, Register Reg) {
  ArrayRef<Part> Parts = getParts(Reg);
  SmallVector<Register, 3> Dst = getOrCreateComponents(Reg);
  MachineBasicBlock &MBB = *MI.getParent();
  auto InsertPt = std::next(MI.getIterator());
  DebugLoc DL = MI.getDebugLoc();

  switch (MI.getOpcode()) {
  case TargetOpcode::IMPLICIT_DEF:
    for (Register DstReg : Dst)
      BuildMI(MBB, InsertPt, DL, TII->get(TargetOpcode::IMPLICIT_DEF),
              DstReg);
    return true;

  case TargetOpcode::REG_SEQUENCE:
    for (auto [Index, P] : enumerate(Parts)) {
      const MachineOperand *Source = nullptr;
      for (unsigned I = 1; I + 1 < MI.getNumOperands(); I += 2)
        if (MI.getOperand(I + 1).getImm() == P.SubReg) {
          Source = &MI.getOperand(I);
          break;
        }

      if (Source)
        BuildMI(MBB, InsertPt, DL, TII->get(TargetOpcode::COPY), Dst[Index])
            .add(*Source);
      else
        BuildMI(MBB, InsertPt, DL, TII->get(TargetOpcode::IMPLICIT_DEF),
                Dst[Index]);
    }
    return true;

  case TargetOpcode::INSERT_SUBREG: {
    SmallVector<Register, 3> Src =
        getSourceComponents(MI.getOperand(1).getReg(), Parts, MI);
    Register InsertReg = MI.getOperand(2).getReg();
    unsigned SubReg = MI.getOperand(3).getImm();
    std::optional<PartRef> InsertPart = getPart(Parts, SubReg);
    if (!InsertPart)
      fail("unsupported wide INSERT_SUBREG index", MI);

    for (unsigned I = 0; I != Parts.size(); ++I) {
      if (I != InsertPart->Index) {
        BuildMI(MBB, InsertPt, DL, TII->get(TargetOpcode::COPY), Dst[I])
            .addReg(Src[I]);
        continue;
      }
      if (!InsertPart->SubReg) {
        BuildMI(MBB, InsertPt, DL, TII->get(TargetOpcode::COPY), Dst[I])
            .addReg(InsertReg);
        continue;
      }
      BuildMI(MBB, InsertPt, DL, TII->get(TargetOpcode::INSERT_SUBREG),
              Dst[I])
          .addReg(Src[I])
          .addReg(InsertReg)
          .addImm(InsertPart->SubReg);
    }
    return true;
  }

  case TargetOpcode::COPY: {
    SmallVector<Register, 3> Src =
        getSourceComponents(MI.getOperand(1).getReg(), Parts, MI);
    for (unsigned I = 0; I != Parts.size(); ++I)
      BuildMI(MBB, InsertPt, DL, TII->get(TargetOpcode::COPY), Dst[I])
          .addReg(Src[I]);
    return true;
  }

  case TargetOpcode::PHI:
    for (unsigned PartIndex = 0; PartIndex != Parts.size(); ++PartIndex) {
      MachineInstrBuilder Phi =
          BuildMI(MBB, MI.getIterator(), DL, TII->get(TargetOpcode::PHI),
                  Dst[PartIndex]);
      for (unsigned I = 1; I + 1 < MI.getNumOperands(); I += 2) {
        SmallVector<Register, 3> Src =
            getSourceComponents(MI.getOperand(I).getReg(), Parts, MI);
        MachineOperand Source = MI.getOperand(I);
        Source.setReg(Src[PartIndex]);
        Source.setSubReg(0);
        Phi.add(Source).add(MI.getOperand(I + 1));
      }
    }
    return true;
  }

  bool Rewrote = false;
  for (MachineOperand &MO : MI.operands()) {
    if (!MO.isReg() || !MO.isDef() || MO.getReg() != Reg)
      continue;
    std::optional<PartRef> P = getPart(Parts, MO.getSubReg());
    if (!P)
      fail("unsupported wide register definition", MI);
    MO.setReg(Dst[P->Index]);
    MO.setSubReg(P->SubReg);
    Rewrote = true;
  }
  if (!Rewrote)
    fail("wide register has no rewritable definition", MI);
  return false;
}

void Z80R64SpillPass::rewriteUses(Register Reg,
                                  DenseSet<MachineInstr *> &ToErase) {
  ArrayRef<Part> Parts = getParts(Reg);
  SmallVector<Register, 3> Src = getOrCreateComponents(Reg);
  SmallPtrSet<MachineInstr *, 16> Uses;
  for (MachineInstr &MI : MRI->use_nodbg_instructions(Reg))
    Uses.insert(&MI);

  for (MachineInstr *MI : Uses) {
    if (ToErase.contains(MI))
      continue;

    if (MI->getOpcode() == TargetOpcode::EXTRACT_SUBREG &&
        MI->getOperand(1).getReg() == Reg) {
      std::optional<PartRef> P =
          getPart(Parts, MI->getOperand(2).getImm());
      if (!P)
        fail("unsupported wide EXTRACT_SUBREG index", *MI);
      BuildMI(*MI->getParent(), MI->getIterator(), MI->getDebugLoc(),
              TII->get(TargetOpcode::COPY), MI->getOperand(0).getReg())
          .addReg(Src[P->Index], 0, P->SubReg);
      ToErase.insert(MI);
      continue;
    }

    for (MachineOperand &MO : MI->operands()) {
      if (!MO.isReg() || !MO.isUse() || MO.getReg() != Reg)
        continue;
      std::optional<PartRef> P = getPart(Parts, MO.getSubReg());
      if (!P)
        fail("unsupported whole wide register use", *MI);
      MO.setReg(Src[P->Index]);
      MO.setSubReg(P->SubReg);
    }
  }
}

bool Z80R64SpillPass::runOnMachineFunction(MachineFunction &MF) {
  const Z80Subtarget &STI = MF.getSubtarget<Z80Subtarget>();
  if (!STI.is24Bit())
    return false;

  TII = STI.getInstrInfo();
  TRI = STI.getRegisterInfo();
  MRI = &MF.getRegInfo();
  Components.clear();

  SmallVector<std::pair<Register, MachineInstr *>, 16> WideDefs;
  unsigned NumVirtRegs = MRI->getNumVirtRegs();
  for (unsigned I = 0; I != NumVirtRegs; ++I) {
    Register Reg = Register::index2VirtReg(I);
    if (!getParts(Reg).empty())
      if (MachineInstr *Def = MRI->getVRegDef(Reg))
        WideDefs.push_back({Reg, Def});
  }
  if (WideDefs.empty())
    return false;

  DenseSet<MachineInstr *> ToErase;
  for (auto [Reg, Def] : WideDefs)
    if (rewriteDef(*Def, Reg))
      ToErase.insert(Def);

  for (auto [Reg, Def] : WideDefs) {
    MRI->markUsesInDebugValueAsUndef(Reg);
    rewriteUses(Reg, ToErase);
  }

  for (MachineInstr *MI : ToErase)
    MI->eraseFromParent();

#ifndef NDEBUG
  for (auto [Reg, Def] : WideDefs)
    assert(MRI->use_nodbg_empty(Reg) &&
           "wide register survived decomposition");
#endif
  return true;
}

FunctionPass *llvm::createZ80R64SpillPass() {
  return new Z80R64SpillPass();
}
