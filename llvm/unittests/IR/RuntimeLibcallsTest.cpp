//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/RuntimeLibcalls.h"
#include "llvm/ADT/STLExtras.h"
#include "gtest/gtest.h"
using namespace llvm;

namespace {

TEST(RuntimeLibcallsTest, LibcallImplByName) {
  EXPECT_TRUE(RTLIB::RuntimeLibcallsInfo::lookupLibcallImplName("").empty());
  EXPECT_TRUE(
      RTLIB::RuntimeLibcallsInfo::lookupLibcallImplName("unknown").empty());
  EXPECT_TRUE(
      RTLIB::RuntimeLibcallsInfo::lookupLibcallImplName("Unsupported").empty());
  EXPECT_TRUE(
      RTLIB::RuntimeLibcallsInfo::lookupLibcallImplName("unsupported").empty());

  for (RTLIB::LibcallImpl LC : RTLIB::libcall_impls()) {
    StringRef Name = RTLIB::RuntimeLibcallsInfo::getLibcallImplName(LC);
    EXPECT_TRUE(is_contained(
        RTLIB::RuntimeLibcallsInfo::lookupLibcallImplName(Name), LC));
  }

  // Test first libcall name
  EXPECT_EQ(
      RTLIB::impl_arm64ec__Unwind_Resume,
      *RTLIB::RuntimeLibcallsInfo::lookupLibcallImplName("#_Unwind_Resume")
           .begin());
  // Test longest libcall names
  EXPECT_EQ(RTLIB::impl___hexagon_memcpy_likely_aligned_min32bytes_mult8bytes,
            *RTLIB::RuntimeLibcallsInfo::lookupLibcallImplName(
                 "__hexagon_memcpy_likely_aligned_min32bytes_mult8bytes")
                 .begin());

  {
    auto SquirtleSquad =
        RTLIB::RuntimeLibcallsInfo::lookupLibcallImplName("sqrtl");
    ASSERT_EQ(size(SquirtleSquad), 4);
    auto I = SquirtleSquad.begin();
    EXPECT_EQ(*I++, RTLIB::impl_sqrtl_f128);
    EXPECT_EQ(*I++, RTLIB::impl_sqrtl_f80);
    EXPECT_EQ(*I++, RTLIB::impl_sqrtl_ppcf128);
    EXPECT_EQ(*I++, RTLIB::impl_z80_sqrt_f64);
  }

  // Last libcall
  {
    auto Truncs = RTLIB::RuntimeLibcallsInfo::lookupLibcallImplName("truncl");
    ASSERT_EQ(size(Truncs), 4);
    auto I = Truncs.begin();
    EXPECT_EQ(*I++, RTLIB::impl_truncl_f128);
    EXPECT_EQ(*I++, RTLIB::impl_truncl_f80);
    EXPECT_EQ(*I++, RTLIB::impl_truncl_ppcf128);
    EXPECT_EQ(*I++, RTLIB::impl_z80_trunc_f64);
  }
}

TEST(RuntimeLibcallsTest, Z80SystemLibrary) {
  RTLIB::RuntimeLibcallsInfo Info(Triple("z80-unknown-none"));

  EXPECT_TRUE(Info.isAvailable(RTLIB::impl_z80_zext_i16_i24));
  EXPECT_EQ(Info.getLibcallFromImpl(RTLIB::impl_z80_zext_i16_i24),
            RTLIB::ZEXT_I16_I24);
  EXPECT_EQ(
      Info.getLibcallImplCallingConv(RTLIB::impl_z80_zext_i16_i24),
      CallingConv::Z80_LibCall);

  EXPECT_TRUE(Info.isAvailable(RTLIB::impl_z80_shl_i32));
  EXPECT_FALSE(Info.isAvailable(RTLIB::impl___ashlsi3));
  EXPECT_EQ(Info.getLibcallImplCallingConv(RTLIB::impl_z80_shl_i32),
            CallingConv::Z80_LibCall_L);

  EXPECT_TRUE(Info.isAvailable(RTLIB::impl_z80_abs_f64));
  EXPECT_EQ(Info.getLibcallFromImpl(RTLIB::impl_z80_abs_f64), RTLIB::ABS_F64);
  EXPECT_EQ(Info.getLibcallImplCallingConv(RTLIB::impl_z80_abs_f64),
            CallingConv::C);

  EXPECT_TRUE(Info.isAvailable(RTLIB::impl_memcpy));
}

} // namespace
