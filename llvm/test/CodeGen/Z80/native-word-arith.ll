; NOTE: Do not autogenerate
; RUN: llc -mtriple=ez80 %s -o - | FileCheck %s
; RUN: llc -mtriple=ez80 -z80-native-word-arith=false %s -o - | FileCheck %s --check-prefix=OFF
; RUN: llc -mtriple=ez80 -stop-after=instruction-select -verify-machineinstrs %s -o /dev/null

; CHECK-LABEL: _add16:
; CHECK: add hl, de
; CHECK-NOT: .sis
; OFF-LABEL: _add16:
; OFF: add.sis
define i16 @add16(i16 %x,i16 %y) optsize {
 %a=add i16 %x,%y
 ret i16 %a
}
; CHECK-LABEL: _sub16:
; CHECK: sbc hl, de
; CHECK-NOT: .sis
; OFF-LABEL: _sub16:
; OFF: sbc.sis
define i16 @sub16(i16 %x,i16 %y) optsize {
 %a=sub i16 %x,%y
 ret i16 %a
}
; CHECK-LABEL: _dbl16:
; CHECK: add hl, hl
; CHECK-NOT: .sis
; OFF-LABEL: _dbl16:
; OFF: add.sis
define i16 @dbl16(i16 %x) optsize {
 %a=add i16 %x,%x
 ret i16 %a
}
; CHECK-LABEL: _dbl24:
; CHECK: add hl, hl
; CHECK-NOT: .sis
; OFF-LABEL: _dbl24:
; OFF: add hl, hl
define i24 @dbl24(i24 %x) optsize {
 %a=add i24 %x,%x
 ret i24 %a
}
; CHECK-LABEL: _shl16:
; CHECK: add hl, hl
; CHECK-NOT: .sis
; OFF-LABEL: _shl16:
; OFF: add.sis
define i16 @shl16(i16 %x) optsize {
 %a=shl i16 %x,1
 ret i16 %a
}

; Overflow operations must retain 16-bit carry semantics.
; CHECK-LABEL: _carry16:
; CHECK: add.sis
; OFF-LABEL: _carry16:
; OFF: add.sis
declare {i16,i1} @llvm.uadd.with.overflow.i16(i16,i16)
define i1 @carry16(i16 %x,i16 %y) optsize {
 %pair = call {i16,i1} @llvm.uadd.with.overflow.i16(i16 %x,i16 %y)
 %carry = extractvalue {i16,i1} %pair,1
 ret i1 %carry
}
