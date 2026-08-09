// RUN: %clang_cc1 -triple ez80-none-elf -ast-print %s | FileCheck %s
// RUN: %clang_cc1 -triple ez80-none-elf -emit-pch -o %t %s
// RUN: %clang_cc1 -triple ez80-none-elf -include-pch %t -ast-print /dev/null | FileCheck %s

// CHECK: __attribute__((tiflags)) __attribute__((tiflags)) int proto(int);
int __attribute__((tiflags)) proto(int);

// CHECK: __attribute__((tiflags)) __attribute__((tiflags)) int knr();
int __attribute__((tiflags)) knr();

// CHECK: __attribute__((tiflags)) int (*ti_proto)(int) __attribute__((tiflags));
int (__attribute__((tiflags)) *ti_proto)(int);

// CHECK: __attribute__((tiflags)) int (*ti_knr)() __attribute__((tiflags));
int (__attribute__((tiflags)) *ti_knr)();
