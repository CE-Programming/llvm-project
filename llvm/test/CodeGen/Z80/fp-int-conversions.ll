; NOTE: Do not autogenerate
; RUN: llc -mtriple=z80 -O0 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=z80 -O2 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=ez80 -mattr=-adl -O0 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=ez80 -mattr=-adl -O2 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=ez80 -O0 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=ez80 -O2 -verify-machineinstrs < %s | FileCheck %s
;
; Non-power-of-two integer inputs/results must use a supported libcall width.
; Floating-point-to-integer conversions truncate the widened result back to the requested width.
; Keep the existing 24-, 32- and 64-bit paths covered alongside i48.

; CHECK-LABEL: s24_to_float:
; CHECK: call __ltof
; CHECK: ret
define float @s24_to_float(i24 %x) {
  %r = sitofp i24 %x to float
  ret float %r
}

; CHECK-LABEL: s24_to_double:
; CHECK: call __ltod
; CHECK: ret
define double @s24_to_double(i24 %x) {
  %r = sitofp i24 %x to double
  ret double %r
}

; CHECK-LABEL: u24_to_float:
; CHECK: call __ultof
; CHECK: ret
define float @u24_to_float(i24 %x) {
  %r = uitofp i24 %x to float
  ret float %r
}

; CHECK-LABEL: u24_to_double:
; CHECK: call __ultod
; CHECK: ret
define double @u24_to_double(i24 %x) {
  %r = uitofp i24 %x to double
  ret double %r
}

; CHECK-LABEL: s32_to_float:
; CHECK: call __ltof
; CHECK: ret
define float @s32_to_float(i32 %x) {
  %r = sitofp i32 %x to float
  ret float %r
}

; CHECK-LABEL: s32_to_double:
; CHECK: call __ltod
; CHECK: ret
define double @s32_to_double(i32 %x) {
  %r = sitofp i32 %x to double
  ret double %r
}

; CHECK-LABEL: u32_to_float:
; CHECK: call __ultof
; CHECK: ret
define float @u32_to_float(i32 %x) {
  %r = uitofp i32 %x to float
  ret float %r
}

; CHECK-LABEL: u32_to_double:
; CHECK: call __ultod
; CHECK: ret
define double @u32_to_double(i32 %x) {
  %r = uitofp i32 %x to double
  ret double %r
}

; CHECK-LABEL: s48_to_float:
; CHECK: call __lltof
; CHECK: ret
define float @s48_to_float(i48 %x) {
  %r = sitofp i48 %x to float
  ret float %r
}

; CHECK-LABEL: s48_to_double:
; CHECK: call __lltod
; CHECK: ret
define double @s48_to_double(i48 %x) {
  %r = sitofp i48 %x to double
  ret double %r
}

; CHECK-LABEL: u48_to_float:
; CHECK: call __ulltof
; CHECK: ret
define float @u48_to_float(i48 %x) {
  %r = uitofp i48 %x to float
  ret float %r
}

; CHECK-LABEL: u48_to_double:
; CHECK: call __ulltod
; CHECK: ret
define double @u48_to_double(i48 %x) {
  %r = uitofp i48 %x to double
  ret double %r
}

; CHECK-LABEL: s64_to_float:
; CHECK: call __lltof
; CHECK: ret
define float @s64_to_float(i64 %x) {
  %r = sitofp i64 %x to float
  ret float %r
}

; CHECK-LABEL: s64_to_double:
; CHECK: call __lltod
; CHECK: ret
define double @s64_to_double(i64 %x) {
  %r = sitofp i64 %x to double
  ret double %r
}

; CHECK-LABEL: u64_to_float:
; CHECK: call __ulltof
; CHECK: ret
define float @u64_to_float(i64 %x) {
  %r = uitofp i64 %x to float
  ret float %r
}

; CHECK-LABEL: u64_to_double:
; CHECK: call __ulltod
; CHECK: ret
define double @u64_to_double(i64 %x) {
  %r = uitofp i64 %x to double
  ret double %r
}

; CHECK-LABEL: float_to_s24:
; CHECK: call __ftol
; CHECK: ret
define i24 @float_to_s24(float %x) {
  %r = fptosi float %x to i24
  ret i24 %r
}

; CHECK-LABEL: double_to_s24:
; CHECK: call __dtol
; CHECK: ret
define i24 @double_to_s24(double %x) {
  %r = fptosi double %x to i24
  ret i24 %r
}

; CHECK-LABEL: float_to_u24:
; CHECK: call __ftoul
; CHECK: ret
define i24 @float_to_u24(float %x) {
  %r = fptoui float %x to i24
  ret i24 %r
}

; CHECK-LABEL: double_to_u24:
; CHECK: call __dtoul
; CHECK: ret
define i24 @double_to_u24(double %x) {
  %r = fptoui double %x to i24
  ret i24 %r
}

; CHECK-LABEL: float_to_s32:
; CHECK: call __ftol
; CHECK: ret
define i32 @float_to_s32(float %x) {
  %r = fptosi float %x to i32
  ret i32 %r
}

; CHECK-LABEL: double_to_s32:
; CHECK: call __dtol
; CHECK: ret
define i32 @double_to_s32(double %x) {
  %r = fptosi double %x to i32
  ret i32 %r
}

; CHECK-LABEL: float_to_u32:
; CHECK: call __ftoul
; CHECK: ret
define i32 @float_to_u32(float %x) {
  %r = fptoui float %x to i32
  ret i32 %r
}

; CHECK-LABEL: double_to_u32:
; CHECK: call __dtoul
; CHECK: ret
define i32 @double_to_u32(double %x) {
  %r = fptoui double %x to i32
  ret i32 %r
}

; CHECK-LABEL: float_to_s48:
; CHECK: call __ftoll
; CHECK: ret
define i48 @float_to_s48(float %x) {
  %r = fptosi float %x to i48
  ret i48 %r
}

; CHECK-LABEL: double_to_s48:
; CHECK: call __dtoll
; CHECK: ret
define i48 @double_to_s48(double %x) {
  %r = fptosi double %x to i48
  ret i48 %r
}

; CHECK-LABEL: float_to_u48:
; CHECK: call __ftoull
; CHECK: ret
define i48 @float_to_u48(float %x) {
  %r = fptoui float %x to i48
  ret i48 %r
}

; CHECK-LABEL: double_to_u48:
; CHECK: call __dtoull
; CHECK: ret
define i48 @double_to_u48(double %x) {
  %r = fptoui double %x to i48
  ret i48 %r
}

; CHECK-LABEL: float_to_s64:
; CHECK: call __ftoll
; CHECK: ret
define i64 @float_to_s64(float %x) {
  %r = fptosi float %x to i64
  ret i64 %r
}

; CHECK-LABEL: double_to_s64:
; CHECK: call __dtoll
; CHECK: ret
define i64 @double_to_s64(double %x) {
  %r = fptosi double %x to i64
  ret i64 %r
}

; CHECK-LABEL: float_to_u64:
; CHECK: call __ftoull
; CHECK: ret
define i64 @float_to_u64(float %x) {
  %r = fptoui float %x to i64
  ret i64 %r
}

; CHECK-LABEL: double_to_u64:
; CHECK: call __dtoull
; CHECK: ret
define i64 @double_to_u64(double %x) {
  %r = fptoui double %x to i64
  ret i64 %r
}
