; NOTE: Do not autogenerate
; RUN: llc -mtriple=z80 -O2 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=ez80-code16 -O2 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=ez80 -O2 -verify-machineinstrs < %s | FileCheck %s

declare i8 @get_byte()
declare void @yes()
declare void @no()
declare void @use_byte(i8)

define void @branch_from_a() {
; CHECK-LABEL: _branch_from_a:
; CHECK: call _get_byte
; CHECK-NEXT: rrca
; CHECK-NEXT: jr nc, [[FALSE:BB[0-9_]+]]
; CHECK: jp _yes
; CHECK: [[FALSE]]:
; CHECK: jp _no
  %x = call i8 @get_byte()
  %bit = trunc i8 %x to i1
  br i1 %bit, label %one, label %two
one:
  tail call void @yes()
  ret void
two:
  tail call void @no()
  ret void
}

define void @branch_keeps_a() {
; CHECK-LABEL: _branch_keeps_a:
; CHECK: call _get_byte
; CHECK-NEXT: bit 0, a
; CHECK-NEXT: jr z, [[FALSE:BB[0-9_]+]]
; CHECK: ld l, a
; CHECK: call _use_byte
; CHECK: [[FALSE]]:
; CHECK: jp _no
  %x = call i8 @get_byte()
  %bit = trunc i8 %x to i1
  br i1 %bit, label %one, label %two
one:
  tail call void @use_byte(i8 %x)
  ret void
two:
  tail call void @no()
  ret void
}
