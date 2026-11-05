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
// ADL: [[EXT:%.*]] = zext i24 {{%.*}} to i32
// ADL: [[SWAP:%.*]] = tail call i32 @llvm.bswap.i32(i32 [[EXT]])
// ADL: [[SHIFT:%.*]] = lshr exact i32 [[SWAP]], 8
// ADL: [[RESULT:%.*]] = trunc nuw i32 [[SHIFT]] to i24
// ADL: ret i24 [[RESULT]]
// Z80-LABEL: define{{.*}} @swap_unsigned(
// Z80: call i16 @llvm.bswap.i16
extern "C" unsigned swap_unsigned(unsigned x) { return __builtin_bswapg(x); }

// ADL-LABEL: define{{.*}} @swap_signed(
// ADL: zext i24 {{%.*}} to i32
// ADL: call i32 @llvm.bswap.i32
// ADL: lshr exact i32 {{%.*}}, 8
// ADL: trunc nuw i32 {{%.*}} to i24
// Z80-LABEL: define{{.*}} @swap_signed(
// Z80: call i16 @llvm.bswap.i16
extern "C" int swap_signed(int x) { return __builtin_bswapg(x); }

// ADL-LABEL: define{{.*}} @swap_bool(
// ADL-NOT: llvm.bswap
// ADL: ret i1
// Z80-LABEL: define{{.*}} @swap_bool(
// Z80-NOT: llvm.bswap
// Z80: ret i1
extern "C" bool swap_bool(bool x) { return __builtin_bswapg(x); }
