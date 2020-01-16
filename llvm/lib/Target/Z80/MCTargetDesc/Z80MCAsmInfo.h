//===-- Z80MCAsmInfo.h - Z80 asm properties --------------------*- C++ -*--===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the declaration of the Z80MCAsmInfo class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_Z80_MCTARGETDESC_Z80MCASMINFO_H
#define LLVM_LIB_TARGET_Z80_MCTARGETDESC_Z80MCASMINFO_H

#include "llvm/MC/MCAsmInfoELF.h"

namespace llvm {
class Triple;

class Z80MCAsmInfoELF : public MCAsmInfoELF {
  void anchor() override;

public:
  explicit Z80MCAsmInfoELF(const Triple &Triple);

  MCSection *getNonexecutableStackSection(MCContext &Ctx) const override;

  bool isAcceptableChar(char C) const override;

  bool shouldOmitSectionDirective(StringRef SectionName) const override;

  const char *getBlockDirective(int64_t Size) const override;
};
} // End llvm namespace

#endif
