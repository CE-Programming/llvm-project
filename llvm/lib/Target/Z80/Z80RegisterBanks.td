//=- Z80RegisterBanks.td - Describe the Z80 Register Banks --*- tablegen -*-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

/// General Purpose Registers
def GPRRegBank : RegisterBank<"GPR", [R64_24, R64_16, R48_24, R32_24, R32_16,
                                      R24, Z24, Z16, Z8]>;
