// RUN: %clang_cc1 -triple ez80-none-elf -Wno-strict-prototypes -emit-llvm -o - %s | FileCheck %s

void __attribute__((tiflags)) decl(void);
void __attribute__((tiflags)) def(void) {}
void __attribute__((tiflags)) knr();

void caller(void) {
  decl();
  def();
  void (__attribute__((tiflags)) *pf)(void) = decl;
  pf();
}

void call_knr(void) { knr(); }

// CHECK-LABEL: define dso_local cc128 void @def()
// CHECK-LABEL: define dso_local void @caller()
// CHECK: call cc128 void @decl()
// CHECK: call cc128 void @def()
// CHECK: call cc128 void %{{.*}}()
// CHECK: declare cc128 void @decl()
// CHECK-LABEL: define dso_local void @call_knr()
// CHECK: call cc128 void @knr()
// CHECK: declare cc128 void @knr(...)
