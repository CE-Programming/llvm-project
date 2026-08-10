// RUN: %clang_cc1 %s -fsyntax-only -triple ez80-none-elf -verify

void __attribute__((tiflags)) redecl(int); // expected-note {{previous declaration is here}}
void redecl(int); // expected-error {{conflicting types for 'redecl'}}

void __attribute__((tiflags)) only_ti(int);

void (__attribute__((cdecl)) *p1)(int) = only_ti; // expected-error {{incompatible function pointer types initializing 'void (*)(int) __attribute__((cdecl))' with an expression of type 'void (int) __attribute__((tiflags))'}}
void (*p2)(int) = only_ti; // expected-error {{incompatible function pointer types initializing 'void (*)(int)' with an expression of type 'void (int) __attribute__((tiflags))'}}
void (__attribute__((tiflags)) *p3)(int) = only_ti;

void (__attribute__((tiflags)) *np1)() =
    (void (__attribute__((tiflags)) *)())0;
void (*np2)() = (void (__attribute__((tiflags)) *)())0; // expected-error {{incompatible function pointer types initializing 'void (*)()' with an expression of type 'void (*)() __attribute__((tiflags))'}}
