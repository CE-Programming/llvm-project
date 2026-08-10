// RUN: %clang_cc1 -triple ez80-none-elf -emit-llvm -o - %s | FileCheck %s

unsigned int reverse24(unsigned int value) {
  return __builtin_bitreverse24(value);
}

// CHECK-LABEL: define {{.*}}i24 @reverse24(i24
// CHECK: call i24 @llvm.bitreverse.i24(i24
