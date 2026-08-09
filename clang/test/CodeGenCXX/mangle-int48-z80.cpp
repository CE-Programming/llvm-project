// RUN: %clang_cc1 -triple ez80-none-elf -emit-llvm -std=gnu++17 %s -o - | FileCheck %s

void f(__int48_t) {}
void f(__uint48_t) {}

// CHECK-LABEL: define{{.*}} void @_Z1fu3i48(i48 noundef %0)
// CHECK-LABEL: define{{.*}} void @_Z1fu3u48(i48 noundef %0)
