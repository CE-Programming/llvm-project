// RUN: %clang_cc1 -triple ez80-none-elf -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple ez80-none-elf -ast-dump %s | FileCheck %s --check-prefix=AST
// RUN: %clang_cc1 -triple ez80-none-elf -emit-llvm -o - %s | FileCheck %s --check-prefix=IR

void __attribute__((interrupt)) generic(void) {}
void __attribute__((interrupt("nested"))) nested(void) {}
void __attribute__((interrupt("nmi"))) nmi(void) {}

// expected-warning@+1 {{Z80 'interrupt' attribute only applies to functions that have no parameters}}
void __attribute__((interrupt)) with_parameter(int value) {}
// expected-warning@+1 {{Z80 'interrupt' attribute only applies to functions that have a 'void' return type}}
int __attribute__((interrupt)) returns_int(void) { return 0; }
// expected-warning@+1 {{'interrupt' attribute argument not supported: 'invalid'}}
void __attribute__((interrupt("invalid"))) invalid_kind(void) {}

// AST-LABEL: FunctionDecl {{.*}} generic
// AST: AnyZ80InterruptAttr {{.*}} Generic
// AST-LABEL: FunctionDecl {{.*}} nested
// AST: AnyZ80InterruptAttr {{.*}} Nested
// AST-LABEL: FunctionDecl {{.*}} nmi
// AST: AnyZ80InterruptAttr {{.*}} NMI

// IR-LABEL: define {{.*}}preserve_allcc void @generic()
// IR-LABEL: define {{.*}}preserve_allcc void @nested()
// IR-LABEL: define {{.*}}preserve_allcc void @nmi()
// IR-DAG: "interrupt"="Generic"
// IR-DAG: "interrupt"="Nested"
// IR-DAG: "interrupt"="NMI"
