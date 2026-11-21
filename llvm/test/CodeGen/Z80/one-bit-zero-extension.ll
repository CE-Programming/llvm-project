; NOTE: Do not autogenerate
; RUN: llc -mtriple=z80 -O2 -verify-machineinstrs < %s | FileCheck %s --check-prefixes=CHECK,MODE16
; RUN: llc -mtriple=ez80-code16 -O2 -verify-machineinstrs < %s | FileCheck %s --check-prefixes=CHECK,MODE16
; RUN: llc -mtriple=ez80 -O2 -verify-machineinstrs < %s | FileCheck %s --check-prefixes=CHECK,ADL

declare i8 @get_byte()
declare void @use_byte_and_word(i8, i16)

define i8 @inverted_i8(i8 %x) {
; CHECK-LABEL: _inverted_i8:
; CHECK-NOT: xor
; CHECK-NOT: and
; CHECK: rrca
; CHECK-NEXT: sbc a, a
; CHECK-NEXT: inc a
; CHECK-NEXT: ret
  %bit = trunc i8 %x to i1
  %inv = xor i1 %bit, true
  %result = zext i1 %inv to i8
  ret i8 %result
}

define i16 @inverted_i16(i8 %x) minsize {
; CHECK-LABEL: _inverted_i16:
; CHECK-NOT: xor
; CHECK-NOT: and
; CHECK: rrca
; MODE16-NEXT: sbc hl, hl
; MODE16-NEXT: inc hl
; ADL-NEXT: sbc.sis hl, hl
; ADL-NEXT: inc.sis hl
  %bit = trunc i8 %x to i1
  %inv = xor i1 %bit, true
  %result = zext i1 %inv to i16
  ret i16 %result
}

define i24 @inverted_i24(i8 %x) minsize {
; ADL-LABEL: _inverted_i24:
; ADL-NOT: xor
; ADL-NOT: and
; ADL: rrca
; ADL-NEXT: sbc hl, hl
; ADL-NEXT: inc hl
  %bit = trunc i8 %x to i1
  %inv = xor i1 %bit, true
  %result = zext i1 %inv to i24
  ret i24 %result
}

define i24 @plain_i24(i8 %x) {
; ADL-LABEL: _plain_i24:
; ADL: and a, 1
; ADL-NEXT: sbc hl, hl
; ADL-NEXT: ld l, a
; ADL-NEXT: ret
  %bit = trunc i8 %x to i1
  %result = zext i1 %bit to i24
  ret i24 %result
}

define i8 @wide_cmp_to_i8(i24 %x) {
; MODE16-LABEL: _wide_cmp_to_i8:
; MODE16: call __lcmpu
; MODE16-NEXT: sbc a, a
; MODE16-NEXT: inc a
; MODE16-NEXT: ret
  %bit = icmp uge i24 %x, 42
  %result = zext i1 %bit to i8
  ret i8 %result
}

define i24 @uge_i24(i24 %x, i24 %y) {
; ADL-LABEL: _uge_i24:
; ADL: sbc hl, de
; ADL-NEXT: sbc hl, hl
; ADL-NEXT: inc hl
; ADL-NEXT: ret
  %bit = icmp uge i24 %x, %y
  %result = zext i1 %bit to i24
  ret i24 %result
}

define void @extension_keeps_a(i8 %x) {
; CHECK-LABEL: _extension_keeps_a:
; CHECK: call _get_byte
; CHECK-NEXT: ld e, a
; CHECK-NEXT: ld a, (ix +
; CHECK-NEXT: rrca
; CHECK-NEXT: sbc hl, hl
; CHECK-NEXT: inc hl
; CHECK: push hl
; CHECK-NEXT: push de
; CHECK-NEXT: call _use_byte_and_word
  %live = call i8 @get_byte()
  %bit = trunc i8 %x to i1
  %inv = xor i1 %bit, true
  %result = zext i1 %inv to i16
  call void @use_byte_and_word(i8 %live, i16 %result)
  ret void
}
