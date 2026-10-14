// RUN: %clang_cc1 -triple z80 -fsyntax-only %s
// RUN: %clang_cc1 -triple ez80-code16 -fsyntax-only %s
// RUN: %clang_cc1 -triple ez80 -fsyntax-only %s
// REQUIRES: z80-registered-target

// CODE16 uses 16-bit unsigned int, while ADL uses 24 bits. The target's
// predefined limit must agree with the actual unsigned type in both modes.
_Static_assert(__UINT_MAX__ == (unsigned)-1,
               "target UINT_MAX disagrees with unsigned int width");
