; NOTE: Do not autogenerate
; RUN: llc -mtriple=z80 -O0 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=z80 -O2 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=ez80-code16 -O0 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=ez80-code16 -O2 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=ez80 -O0 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=ez80 -O2 -verify-machineinstrs < %s | FileCheck %s
; Mixed signed/unsigned overflow arithmetic needs one extra sign bit.
; Narrowing its addition into native limbs can leave a partial top limb.

; CHECK-LABEL: slt25:
; CHECK: ret
define i1 @slt25(i24 %x, i24 %y) {
  %a = zext i24 %x to i25
  %b = sext i24 %y to i25
  %r = icmp slt i25 %a, %b
  ret i1 %r
}

; CHECK-LABEL: ult25:
; CHECK: ret
define i1 @ult25(i24 %x, i24 %y) {
  %a = zext i24 %x to i25
  %b = sext i24 %y to i25
  %r = icmp ult i25 %a, %b
  ret i1 %r
}

; CHECK-LABEL: slt33:
; CHECK: ret
define i1 @slt33(i32 %x, i32 %y) {
  %a = zext i32 %x to i33
  %b = sext i32 %y to i33
  %r = icmp slt i33 %a, %b
  ret i1 %r
}

; CHECK-LABEL: ult33:
; CHECK: ret
define i1 @ult33(i32 %x, i32 %y) {
  %a = zext i32 %x to i33
  %b = sext i32 %y to i33
  %r = icmp ult i33 %a, %b
  ret i1 %r
}

; CHECK-LABEL: slt49:
; CHECK: ret
define i1 @slt49(i48 %x, i48 %y) {
  %a = zext i48 %x to i49
  %b = sext i48 %y to i49
  %r = icmp slt i49 %a, %b
  ret i1 %r
}

; CHECK-LABEL: ult49:
; CHECK: ret
define i1 @ult49(i48 %x, i48 %y) {
  %a = zext i48 %x to i49
  %b = sext i48 %y to i49
  %r = icmp ult i49 %a, %b
  ret i1 %r
}

declare {i25, i1} @llvm.sadd.with.overflow.i25(i25, i25)
; CHECK-LABEL: add25:
; CHECK: ret
define i1 @add25(i24 %x, i24 %y, ptr %out) {
  %a = zext i24 %x to i25
  %b = sext i24 %y to i25
  %pair = call {i25, i1} @llvm.sadd.with.overflow.i25(i25 %a, i25 %b)
  %sum = extractvalue {i25, i1} %pair, 0
  %trunc = trunc i25 %sum to i24
  store i24 %trunc, ptr %out, align 1
  %overflow = extractvalue {i25, i1} %pair, 1
  ret i1 %overflow
}

declare {i33, i1} @llvm.sadd.with.overflow.i33(i33, i33)
; CHECK-LABEL: add33:
; CHECK: ret
define i1 @add33(i32 %x, i32 %y, ptr %out) {
  %a = zext i32 %x to i33
  %b = sext i32 %y to i33
  %pair = call {i33, i1} @llvm.sadd.with.overflow.i33(i33 %a, i33 %b)
  %sum = extractvalue {i33, i1} %pair, 0
  %trunc = trunc i33 %sum to i32
  store i32 %trunc, ptr %out, align 1
  %overflow = extractvalue {i33, i1} %pair, 1
  ret i1 %overflow
}

declare {i49, i1} @llvm.sadd.with.overflow.i49(i49, i49)
; CHECK-LABEL: add49:
; CHECK: ret
define i1 @add49(i48 %x, i48 %y, ptr %out) {
  %a = zext i48 %x to i49
  %b = sext i48 %y to i49
  %pair = call {i49, i1} @llvm.sadd.with.overflow.i49(i49 %a, i49 %b)
  %sum = extractvalue {i49, i1} %pair, 0
  %trunc = trunc i49 %sum to i48
  store i48 %trunc, ptr %out, align 1
  %overflow = extractvalue {i49, i1} %pair, 1
  ret i1 %overflow
}
