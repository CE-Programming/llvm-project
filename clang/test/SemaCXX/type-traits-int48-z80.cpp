// RUN: %clang_cc1 -triple z80-none-elf -std=c++11 -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple z80-none-elf -std=c++17 -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple z80-none-elf -std=c++20 -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple z80-none-elf -std=c++23 -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple ez80-none-elf -std=c++11 -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple ez80-none-elf -std=c++17 -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple ez80-none-elf -std=c++20 -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple ez80-none-elf -std=c++23 -fsyntax-only -verify %s
// expected-no-diagnostics

typedef __int48 Int48;
typedef unsigned __int48 UInt48;

static_assert(__is_same(__make_signed(unsigned __int48), Int48), "");
static_assert(__is_same(__make_unsigned(__int48), UInt48), "");

template <class T, class Signed, class Unsigned> struct CheckSignedness {
  static_assert(__is_same(__make_signed(T), Signed), "");
  static_assert(__is_same(__make_signed(const T), const Signed), "");
  static_assert(__is_same(__make_signed(volatile T), volatile Signed), "");
  static_assert(__is_same(__make_signed(const volatile T), const volatile Signed), "");
  static_assert(__is_same(__make_unsigned(T), Unsigned), "");
  static_assert(__is_same(__make_unsigned(const T), const Unsigned), "");
  static_assert(__is_same(__make_unsigned(volatile T), volatile Unsigned), "");
  static_assert(__is_same(__make_unsigned(const volatile T), const volatile Unsigned), "");
};

template struct CheckSignedness<Int48, Int48, UInt48>;
template struct CheckSignedness<UInt48, Int48, UInt48>;

enum SignedEnum : Int48 { SignedValue = -1 };
enum UnsignedEnum : UInt48 { UnsignedValue = 1ULL << 47 };
enum class ScopedSignedEnum : Int48 { Value = -1 };
enum class ScopedUnsignedEnum : UInt48 { Value = 1ULL << 47 };

template struct CheckSignedness<SignedEnum, Int48, UInt48>;
template struct CheckSignedness<UnsignedEnum, Int48, UInt48>;
template struct CheckSignedness<ScopedSignedEnum, Int48, UInt48>;
template struct CheckSignedness<ScopedUnsignedEnum, Int48, UInt48>;

// Keep the standard integer transformations intact on both target ABIs.
template struct CheckSignedness<signed char, signed char, unsigned char>;
template struct CheckSignedness<unsigned short, short, unsigned short>;
template struct CheckSignedness<unsigned int, int, unsigned int>;
template struct CheckSignedness<unsigned long, long, unsigned long>;
template struct CheckSignedness<unsigned long long, long long, unsigned long long>;
enum class StandardEnum : unsigned long { Value = 1 };
template struct CheckSignedness<StandardEnum, long, unsigned long>;
