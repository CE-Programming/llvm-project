; NOTE: Do not autogenerate
; RUN: llc -mtriple=ez80 -O0 -z80-gas-style < %s | FileCheck %s
; RUN: llc -mtriple=ez80 -O2 -z80-gas-style < %s | FileCheck %s
; RUN: llc -mtriple=ez80 -O3 -z80-gas-style < %s | FileCheck %s
;
; Unsigned inequalities against zero are constants. The wide compare-zero
; helpers define sign and zero only, so their carry flag cannot implement
; these predicates. Use optnone to keep this a legalization regression test.

; CHECK-LABEL: _ult32:
; CHECK-NOT: call
; CHECK: xor a, a
; CHECK: ret
define i8 @ult32(i32 %x) optnone noinline nounwind {
  %c = icmp ult i32 %x, 0
  %r = zext i1 %c to i8
  ret i8 %r
}

; CHECK-LABEL: _uge32:
; CHECK-NOT: call
; CHECK: ld a, 1
; CHECK: ret
define i8 @uge32(i32 %x) optnone noinline nounwind {
  %c = icmp uge i32 %x, 0
  %r = zext i1 %c to i8
  ret i8 %r
}

; CHECK-LABEL: _ult48:
; CHECK-NOT: call
; CHECK: xor a, a
; CHECK: ret
define i8 @ult48(i48 %x) optnone noinline nounwind {
  %c = icmp ult i48 %x, 0
  %r = zext i1 %c to i8
  ret i8 %r
}

; CHECK-LABEL: _uge48:
; CHECK-NOT: call
; CHECK: ld a, 1
; CHECK: ret
define i8 @uge48(i48 %x) optnone noinline nounwind {
  %c = icmp uge i48 %x, 0
  %r = zext i1 %c to i8
  ret i8 %r
}

; CHECK-LABEL: _ult64:
; CHECK-NOT: call
; CHECK: xor a, a
; CHECK: ret
define i8 @ult64(i64 %x) optnone noinline nounwind {
  %c = icmp ult i64 %x, 0
  %r = zext i1 %c to i8
  ret i8 %r
}

; CHECK-LABEL: _uge64:
; CHECK-NOT: call
; CHECK: ld a, 1
; CHECK: ret
define i8 @uge64(i64 %x) optnone noinline nounwind {
  %c = icmp uge i64 %x, 0
  %r = zext i1 %c to i8
  ret i8 %r
}

; Operand swapping turns these into the same zero-RHS conditions.
; CHECK-LABEL: _zero_ugt32:
; CHECK-NOT: call
; CHECK: xor a, a
; CHECK: ret
define i8 @zero_ugt32(i32 %x) optnone noinline nounwind {
  %c = icmp ugt i32 0, %x
  %r = zext i1 %c to i8
  ret i8 %r
}

; CHECK-LABEL: _zero_ule48:
; CHECK-NOT: call
; CHECK: ld a, 1
; CHECK: ret
define i8 @zero_ule48(i48 %x) optnone noinline nounwind {
  %c = icmp ule i48 0, %x
  %r = zext i1 %c to i8
  ret i8 %r
}

; CHECK-LABEL: _zero_ugt64:
; CHECK-NOT: call
; CHECK: xor a, a
; CHECK: ret
define i8 @zero_ugt64(i64 %x) optnone noinline nounwind {
  %c = icmp ugt i64 0, %x
  %r = zext i1 %c to i8
  ret i8 %r
}

; Sign and equality conditions can still use the optimized helpers.
; CHECK-LABEL: _slt32:
; CHECK: call __lcmpzero
; CHECK: ret
define i8 @slt32(i32 %x) optnone noinline nounwind {
  %c = icmp slt i32 %x, 0
  %r = zext i1 %c to i8
  ret i8 %r
}

; CHECK-LABEL: _eq64:
; CHECK: call __llcmpzero
; CHECK: ret
define i8 @eq64(i64 %x) optnone noinline nounwind {
  %c = icmp eq i64 %x, 0
  %r = zext i1 %c to i8
  ret i8 %r
}
