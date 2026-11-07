// RUN: %clang_cc1 -triple ez80 -std=c++11 -fsyntax-only -fexperimental-new-constant-interpreter %s
// RUN: %clang_cc1 -triple ez80 -std=c++11 -emit-llvm -O1 -o - %s | FileCheck %s --check-prefix=ADL
// RUN: %clang_cc1 -triple z80 -std=c++11 -emit-llvm -O1 -o - %s | FileCheck %s --check-prefix=Z80

static_assert(__builtin_bswapg(true), "one-bit values are unchanged");
static_assert(!__builtin_bswapg(false), "one-bit values are unchanged");
#if __SIZEOF_INT__ == 3
static_assert(__builtin_bswapg(0x123456U) == 0x563412U, "24-bit constant byteswap");
static_assert(__builtin_bswapg(-2) == -65537, "signed 24-bit constant byteswap");
#endif

// ADL-LABEL: define{{.*}} @swap_unsigned(
// ADL-SAME: i24{{[^%]*}}[[ARG:%[^ )]+]]
// ADL: [[EXT:%.*]] = zext i24 [[ARG]] to i32
// ADL: [[SWAP:%.*]] = tail call i32 @llvm.bswap.i32(i32 [[EXT]])
// ADL: [[SHIFT:%.*]] = lshr exact i32 [[SWAP]], 8
// ADL: [[RESULT:%.*]] = trunc nuw i32 [[SHIFT]] to i24
// ADL: ret i24 [[RESULT]]
// Z80-LABEL: define{{.*}} @swap_unsigned(
// Z80-SAME: i16{{[^%]*}}[[ARG:%[^ )]+]]
// Z80: [[SWAP:%.*]] = {{.*}}call i16 @llvm.bswap.i16(i16 [[ARG]])
// Z80: ret i16 [[SWAP]]
extern "C" unsigned swap_unsigned(unsigned x) { return __builtin_bswapg(x); }

// ADL-LABEL: define{{.*}} @swap_signed(
// ADL-SAME: i24{{[^%]*}}[[ARG:%[^ )]+]]
// ADL: [[EXT:%.*]] = zext i24 [[ARG]] to i32
// ADL: [[SWAP:%.*]] = {{.*}}call i32 @llvm.bswap.i32(i32 [[EXT]])
// ADL: [[SHIFT:%.*]] = lshr exact i32 [[SWAP]], 8
// ADL: [[RESULT:%.*]] = trunc nuw i32 [[SHIFT]] to i24
// ADL: ret i24 [[RESULT]]
// Z80-LABEL: define{{.*}} @swap_signed(
// Z80-SAME: i16{{[^%]*}}[[ARG:%[^ )]+]]
// Z80: [[SWAP:%.*]] = {{.*}}call i16 @llvm.bswap.i16(i16 [[ARG]])
// Z80: ret i16 [[SWAP]]
extern "C" int swap_signed(int x) { return __builtin_bswapg(x); }

// ADL-LABEL: define{{.*}} @swap_bool(
// ADL-SAME: i1{{[^%]*}}[[ARG:%[^ )]+]]
// ADL-NOT: llvm.bswap
// ADL: ret i1 [[ARG]]
// Z80-LABEL: define{{.*}} @swap_bool(
// Z80-SAME: i1{{[^%]*}}[[ARG:%[^ )]+]]
// Z80-NOT: llvm.bswap
// Z80: ret i1 [[ARG]]
extern "C" bool swap_bool(bool x) { return __builtin_bswapg(x); }
