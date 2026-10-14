; NOTE: Do not autogenerate
; RUN: llc -mtriple=ez80 -O2 -z80-gas-style -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=ez80 -O0 -z80-gas-style -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=ez80-unknown-unknown-code16 -O2 -z80-gas-style -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=z80 -O2 -z80-gas-style -verify-machineinstrs < %s | FileCheck %s
;
; A volatile RMW cannot discard a loaded value that is still used, or obtain
; a used result by rereading volatile memory after the store.
target triple = "ez80"

define i8 @postincrement(ptr %p) {
; CHECK-LABEL: _postincrement:
; CHECK:      ld a, (hl)
; CHECK-NEXT: ld [[RESULT:[bcdehl]]], a
; CHECK-NEXT: inc [[RESULT]]
; CHECK-NEXT: ld (hl), [[RESULT]]
; CHECK:      ret
  %x = load volatile i8, ptr %p
  %y = add i8 %x, 1
  store volatile i8 %y, ptr %p
  ret i8 %x
}

define i8 @preincrement(ptr %p) {
; CHECK-LABEL: _preincrement:
; CHECK:      ld a, (hl)
; CHECK-NEXT: inc a
; CHECK-NEXT: ld (hl), a
; CHECK:      ret
  %x = load volatile i8, ptr %p
  %y = add i8 %x, 1
  store volatile i8 %y, ptr %p
  ret i8 %y
}

define void @discarded_result(ptr %p) {
; CHECK-LABEL: _discarded_result:
; CHECK:      inc (hl)
; CHECK-NEXT: ret
  %x = load volatile i8, ptr %p
  %y = add i8 %x, 1
  store volatile i8 %y, ptr %p
  ret void
}

define i8 @load_ordered_old_result(ptr %p) {
; CHECK-LABEL: _load_ordered_old_result:
; CHECK:      ld a, (hl)
; CHECK-NEXT: ld [[RESULT2:[bcdehl]]], a
; CHECK-NEXT: inc [[RESULT2]]
; CHECK-NEXT: ld (hl), [[RESULT2]]
; CHECK:      ret
  %x = load volatile i8, ptr %p
  %y = add i8 %x, 1
  store i8 %y, ptr %p
  ret i8 %x
}

define i8 @store_ordered_old_result(ptr %p) {
; CHECK-LABEL: _store_ordered_old_result:
; CHECK:      ld a, (hl)
; CHECK-NEXT: ld [[RESULT3:[bcdehl]]], a
; CHECK-NEXT: inc [[RESULT3]]
; CHECK-NEXT: ld (hl), [[RESULT3]]
; CHECK:      ret
  %x = load i8, ptr %p
  %y = add i8 %x, 1
  store volatile i8 %y, ptr %p
  ret i8 %x
}
