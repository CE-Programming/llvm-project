// Test this without PCH.
// RUN: %clang_cc1 -triple ez80-none-elf -include %s -fsyntax-only -verify %s

// Test with PCH.
// RUN: %clang_cc1 -triple ez80-none-elf -emit-pch -o %t %s
// RUN: %clang_cc1 -triple ez80-none-elf -include-pch %t -fsyntax-only -verify %s

#ifndef HEADER
#define HEADER

void __attribute__((tiflags)) from_pch(int);
void (__attribute__((tiflags)) *pf_from_pch)(int);

#else

void from_pch(int); // expected-error {{conflicting types for 'from_pch'}}
                    // expected-note@-6 {{previous declaration is here}}
void (*pf_plain)(int) = from_pch; // expected-error {{incompatible function pointer types initializing 'void (*)(int)' with an expression of type 'void (int) __attribute__((tiflags))'}}
void (__attribute__((tiflags)) *pf_ok)(int) = from_pch;

#endif
