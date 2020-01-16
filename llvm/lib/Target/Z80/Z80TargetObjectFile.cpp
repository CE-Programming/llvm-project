//===-- Z80TargetObjectFile.cpp - Z80 Object Info -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Z80TargetObjectFile.h"
#include "llvm/MC/SectionKind.h"
using namespace llvm;

void Z80ELFTargetObjectFile::anchor() {}

MCSection *Z80ELFTargetObjectFile::SelectSectionForGlobal(
    const GlobalObject *GO, SectionKind Kind, const TargetMachine &TM) const {
  // Disable mergeable sections for now.
  if (Kind.isMergeableCString() || Kind.isMergeableConst())
    Kind = SectionKind::getReadOnly();
  return TargetLoweringObjectFileELF::SelectSectionForGlobal(GO, Kind, TM);
}
