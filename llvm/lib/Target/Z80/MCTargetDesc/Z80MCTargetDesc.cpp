//===-- Z80MCTargetDesc.cpp - Z80 Target Descriptions ---------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file provides Z80 specific target descriptions.
//
//===----------------------------------------------------------------------===//

#include "Z80MCTargetDesc.h"
#include "EZ80InstPrinter.h"
#include "Z80InstPrinter.h"
#include "Z80MCAsmInfo.h"
#include "Z80TargetStreamer.h"
#include "llvm/MC/MCDwarf.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
using namespace llvm;

#define GET_REGINFO_MC_DESC
#include "Z80GenRegisterInfo.inc"

#define GET_INSTRINFO_MC_DESC
#include "Z80GenInstrInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
#include "Z80GenSubtargetInfo.inc"

std::string Z80_MC::ParseZ80Triple(const Triple &TT) {
  std::string FS;
  if (TT.getArch() == Triple::ez80)
    FS = "+24bit-mode,-16bit-mode";
  else
    FS = "-24bit-mode,+16bit-mode";

  return FS;
}

MCSubtargetInfo *Z80_MC::createZ80MCSubtargetInfo(const Triple &TT,
                                                  StringRef CPU, StringRef FS) {
  std::string ArchFS = Z80_MC::ParseZ80Triple(TT);
  assert(!ArchFS.empty() && "Failed to parse Z80 triple");
  if (!FS.empty())
    ArchFS = (Twine(ArchFS) + "," + FS).str();

  if (CPU.empty())
    CPU = "generic";

  return createZ80MCSubtargetInfoImpl(TT, CPU, /*TuneCPU*/ CPU, ArchFS);
}

static MCInstrInfo *createZ80MCInstrInfo() {
  MCInstrInfo *X = new MCInstrInfo;
  InitZ80MCInstrInfo(X);
  return X;
}

static MCRegisterInfo *createZ80MCRegisterInfo(const Triple &TT) {
  MCRegisterInfo *X = new MCRegisterInfo;
  InitZ80MCRegisterInfo(X, 0, 0, 0, Z80::PC);
  return X;
}

static MCAsmInfo *createZ80MCAsmInfo(const MCRegisterInfo &MRI,
                                     const Triple &TheTriple,
                                     const MCTargetOptions &Options) {
  bool Is16Bit =
      TheTriple.isArch16Bit() || TheTriple.getEnvironment() == Triple::CODE16;
  MCAsmInfo *X = new Z80MCAsmInfoELF(TheTriple);

  int SlotSize = Is16Bit ? 2 : 3;
  MCRegister StackReg = Is16Bit ? Z80::SPS : Z80::SPL;
  X->addInitialFrameState(MCCFIInstruction::cfiDefCfa(
      nullptr, MRI.getDwarfRegNum(StackReg, true), SlotSize));
  X->addInitialFrameState(MCCFIInstruction::createSameValue(
      nullptr, MRI.getDwarfRegNum(Is16Bit ? Z80::IX : Z80::UIX, true)));
  X->addInitialFrameState(MCCFIInstruction::createValOffset(
      nullptr, MRI.getDwarfRegNum(StackReg, true), 0));
  X->addInitialFrameState(MCCFIInstruction::createOffset(
      nullptr, MRI.getDwarfRegNum(MRI.getProgramCounter(), true), -SlotSize));

  return X;
}

static MCInstPrinter *createZ80MCInstPrinter(const Triple &T,
                                             unsigned SyntaxVariant,
                                             const MCAsmInfo &MAI,
                                             const MCInstrInfo &MII,
                                             const MCRegisterInfo &MRI) {
  if (SyntaxVariant == 0)
    return new Z80InstPrinter(MAI, MII, MRI);
  if (SyntaxVariant == 1)
    return new Z80EInstPrinter(MAI, MII, MRI);
  return nullptr;
}

static MCTargetStreamer *
createZ80AsmTargetStreamer(MCStreamer &S, formatted_raw_ostream &OS,
                           MCInstPrinter */*InstPrint*/,
                           bool /*isVerboseAsm*/) {
  return new Z80TargetAsmStreamer(S, OS);
}

// Force static initialization.
extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeZ80TargetMC() {
  for (Target *T : {&getTheZ80Target(), &getTheEZ80Target()}) {
    // Register the MC asm info.
    RegisterMCAsmInfoFn X(*T, createZ80MCAsmInfo);

    // Register the MC instruction info.
    TargetRegistry::RegisterMCInstrInfo(*T, createZ80MCInstrInfo);

    // Register the MC register info.
    TargetRegistry::RegisterMCRegInfo(*T, createZ80MCRegisterInfo);

    // Register the MC subtarget info.
    TargetRegistry::RegisterMCSubtargetInfo(*T,
                                            Z80_MC::createZ80MCSubtargetInfo);

    // Register the MCInstPrinter.
    TargetRegistry::RegisterMCInstPrinter(*T, createZ80MCInstPrinter);

    // Register the asm target streamer.
    TargetRegistry::RegisterAsmTargetStreamer(*T, createZ80AsmTargetStreamer);
  }
}
