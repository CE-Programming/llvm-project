; NOTE: Do not autogenerate
; RUN: llc -mtriple=z80 -O2 -verify-machineinstrs < %s | FileCheck %s --check-prefixes=CHECK,MODE16
; RUN: llc -mtriple=ez80-code16 -O2 -verify-machineinstrs < %s | FileCheck %s --check-prefixes=CHECK,MODE16
; RUN: llc -mtriple=ez80 -O2 -verify-machineinstrs < %s | FileCheck %s --check-prefixes=CHECK,ADL

@input_word = external global i16, align 1

; The upper bits of the input need not match bit 0.
define i8 @sext_bit0_i8(i8 %x) {
; CHECK-LABEL: _sext_bit0_i8:
; CHECK-NOT: call
; CHECK: {{rrca|rrc[ \t]+a}}
; CHECK-NEXT: sbc a, a
; CHECK-NEXT: ret
  %bit = trunc i8 %x to i1
  %ext = sext i1 %bit to i8
  ret i8 %ext
}

define i16 @sext_bit0_i16(i16 %x) {
; CHECK-LABEL: _sext_bit0_i16:
; CHECK-NOT: call
; CHECK: {{rrca|rrc[ \t]+a}}
; MODE16-NEXT: sbc hl, hl
; ADL-NEXT: sbc.sis hl, hl
; CHECK-NEXT: ret
  %bit = trunc i16 %x to i1
  %ext = sext i1 %bit to i16
  ret i16 %ext
}

; The volatile load keeps a full i16 source for the low-byte extraction.
define i16 @select_adjacent_constants_i16() {
; CHECK-LABEL: _select_adjacent_constants_i16:
; CHECK-NOT: call
; CHECK: {{rrca|rrc[ \t]+a}}
; MODE16-NEXT: sbc hl, hl
; ADL-NEXT: sbc.sis hl, hl
; CHECK-NOT: call
; CHECK: ld [[TWO:[a-z]+]], 2
; CHECK-NEXT: add hl, [[TWO]]
; CHECK-NOT: call
; CHECK: ret
  %word = load volatile i16, ptr @input_word, align 1
  %bit = trunc i16 %word to i1
  %result = select i1 %bit, i16 1, i16 2
  ret i16 %result
}

; A wider field still needs its original sign bit.
define i8 @sext_low_7_bits_i8(i8 %x) {
; CHECK-LABEL: _sext_low_7_bits_i8:
; CHECK: add a, a
; CHECK-NEXT: sra a
; CHECK-NEXT: ret
  %bits = trunc i8 %x to i7
  %ext = sext i7 %bits to i8
  ret i8 %ext
}

define i24 @sext_bit0_i24(i24 %x) {
; ADL-LABEL: _sext_bit0_i24:
; ADL-NOT: call
; ADL: {{rrca|rrc[ \t]+a}}
; ADL-NEXT: sbc hl, hl
; ADL-NEXT: ret
  %bit = trunc i24 %x to i1
  %ext = sext i1 %bit to i24
  ret i24 %ext
}

; Widening the boolean AND used to turn the sign extension in 2 + sext(%both)
; into a shift libcall. The rotate only needs the low byte of the AND result.
define i24 @select_adjacent_constants(i24 %x, i24 %y) {
; ADL-LABEL: _select_adjacent_constants:
; ADL-NOT: call
; ADL: {{rrca|rrc[ \t]+[a-z]+}}
; ADL-NEXT: sbc hl, hl
; ADL-NOT: call
; ADL: ret
  %x.lt = icmp ult i24 %x, 9
  %y.lt = icmp ult i24 %y, 9
  %both = and i1 %x.lt, %y.lt
  %result = select i1 %both, i24 1, i24 2
  ret i24 %result
}
