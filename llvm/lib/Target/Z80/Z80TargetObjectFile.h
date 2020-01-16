//===-- Z80TargetObjectFile.h - Z80 Object Info -----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_Z80_Z80TARGETOBJECTFILE_H
#define LLVM_LIB_TARGET_Z80_Z80TARGETOBJECTFILE_H

#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"

namespace llvm {

  /// This implementation is used for Z80 ELF targets that don't
  /// have a further specialization.
  class Z80ELFTargetObjectFile : public TargetLoweringObjectFileELF {
    virtual void anchor();

  public:
    Z80ELFTargetObjectFile() {}

    MCSection *SelectSectionForGlobal(const GlobalObject *GO, SectionKind Kind,
                                      const TargetMachine &TM) const override;
  };

} // end namespace llvm

#endif
