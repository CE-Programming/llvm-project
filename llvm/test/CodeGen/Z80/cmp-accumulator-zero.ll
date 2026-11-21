; NOTE: Do not autogenerate
; RUN: llc -mtriple=ez80 -O2 -stop-after=instruction-select -verify-machineinstrs < %s | FileCheck %s --check-prefix=ISEL
; RUN: llc -mtriple=ez80 -O2 -verify-machineinstrs < %s | FileCheck %s --check-prefix=ASM

declare i8 @get_byte()
declare void @zero()
declare void @nonzero()
declare i8 @llvm.fshl.i8(i8, i8, i8)

define void @test_byte() {
; ISEL-LABEL: name: test_byte
; ISEL:       Cmp8a0 implicit-def $f, implicit $a
; ISEL-NEXT:  JQCC
; ASM-LABEL: _test_byte:
; ASM:       call _get_byte
; ASM-NEXT:  or a, a
; ASM-NEXT:  jr
  %x = call i8 @get_byte()
  %zero = icmp eq i8 %x, 0
  br i1 %zero, label %one, label %two

one:
  tail call void @zero()
  ret void

two:
  tail call void @nonzero()
  ret void
}

define void @test_double(i8 %x) {
; ASM-LABEL: _test_double:
; ASM:       add a, a
; ASM-NEXT:  jr nz
  %v = shl i8 %x, 1
  %z = icmp eq i8 %v, 0
  br i1 %z, label %one, label %two
one:
  tail call void @zero()
  ret void
two:
  tail call void @nonzero()
  ret void
}

define void @test_shift_right(i8 %x) {
; ASM-LABEL: _test_shift_right:
; ASM:       srl a
; ASM-NEXT:  jr nz
  %v = lshr i8 %x, 1
  %z = icmp eq i8 %v, 0
  br i1 %z, label %one, label %two
one:
  tail call void @zero()
  ret void
two:
  tail call void @nonzero()
  ret void
}

define void @test_sub(i8 %x, i8 %y) {
; ASM-LABEL: _test_sub:
; ASM:       sub a, (ix +
; ASM-NEXT:  jr nz
  %v = sub i8 %x, %y
  %z = icmp eq i8 %v, 0
  br i1 %z, label %one, label %two
one:
  tail call void @zero()
  ret void
two:
  tail call void @nonzero()
  ret void
}

define void @test_adjacent_reload(ptr %p) {
; ASM-LABEL: _test_adjacent_reload:
; ASM:       dec (iy + 3)
; ASM-NEXT:  ld a, (iy + 3)
; ASM-NEXT:  jr z
  %q = getelementptr i8, ptr %p, i32 3
  %old = load i8, ptr %q
  %new = add i8 %old, -1
  store i8 %new, ptr %q
  %v = load i8, ptr %q
  %z = icmp ne i8 %v, 0
  br i1 %z, label %one, label %two
one:
  tail call void @nonzero()
  ret void
two:
  tail call void @zero()
  ret void
}

define void @test_rotate(i8 %x) {
; The short rotate leaves S and Z alone.
; ASM-LABEL: _test_rotate:
; ASM:       rlca
; ASM-NEXT:  or a, a
; ASM-NEXT:  jr nz
  %v = call i8 @llvm.fshl.i8(i8 %x, i8 %x, i8 1)
  %z = icmp eq i8 %v, 0
  br i1 %z, label %one, label %two
one:
  tail call void @zero()
  ret void
two:
  tail call void @nonzero()
  ret void
}
