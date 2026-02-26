; RUN: llc -mtriple=ez80 < %s | FileCheck %s --check-prefix=EZ80

define i16 @cmp0_o16(i16 %x, ptr nocapture readonly %p) {
; EZ80-LABEL: _cmp0_o16:
; EZ80:       BB0_1:
; EZ80:       sbc.sis hl, hl
; EZ80-NEXT:    adc.sis hl, de
; EZ80-NEXT:    jr z
entry:
  br label %loop

loop:
  %v = phi i16 [ %x, %entry ], [ %dec, %body ]
  %acc = phi i16 [ 0, %entry ], [ %acc2, %body ]
  %iszero = icmp eq i16 %v, 0
  br i1 %iszero, label %exit, label %body

body:
  %mask = and i16 %v, 15
  %idx = zext i16 %mask to i24
  %ptr = getelementptr i8, ptr %p, i24 %idx
  %b = load i8, ptr %ptr, align 1
  %ext = zext i8 %b to i16
  %acc2 = add i16 %acc, %ext
  %dec = add i16 %v, -1
  br label %loop

exit:
  ret i16 %acc
}

define i24 @cmp0_o24(i24 %x, ptr nocapture readonly %p) {
; EZ80-LABEL: _cmp0_o24:
; EZ80:       BB1_1:
; EZ80:       sbc hl, hl
; EZ80-NEXT:    adc hl, de
; EZ80-NEXT:    jr z
entry:
  br label %loop

loop:
  %v = phi i24 [ %x, %entry ], [ %dec, %body ]
  %acc = phi i24 [ 0, %entry ], [ %acc2, %body ]
  %iszero = icmp eq i24 %v, 0
  br i1 %iszero, label %exit, label %body

body:
  %mask = and i24 %v, 31
  %ptr = getelementptr i8, ptr %p, i24 %mask
  %b = load i8, ptr %ptr, align 1
  %ext = zext i8 %b to i24
  %acc2 = add i24 %acc, %ext
  %dec = add i24 %v, -1
  br label %loop

exit:
  ret i24 %acc
}
