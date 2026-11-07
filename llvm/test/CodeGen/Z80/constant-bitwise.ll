; NOTE: Do not autogenerate
; RUN: llc -mtriple=ez80 %s -o - | FileCheck %s
; RUN: llc -mtriple=ez80 -z80-constant-bitwise=false -z80-byte-immediates=false %s -o - | FileCheck %s --check-prefix=OFF

; CHECK-LABEL: _top:
; CHECK: add hl, hl
; CHECK-NEXT: call __ishru_1
; CHECK-NOT: __iand
; OFF-LABEL: _top:
; OFF: ld bc, 8388607
; OFF: call __iand
define i24 @top(i24 %x) optsize {
 %a = and i24 %x, 8388607
 ret i24 %a
}
; CHECK-LABEL: _low:
; CHECK: and a, 31
; CHECK-NOT: __iand
; OFF-LABEL: _low:
; OFF: call __iand
define i24 @low(i24 %x) optsize {
 %a = and i24 %x, 31
 ret i24 %a
}
; CHECK-LABEL: _word:
; CHECK: sbc hl, hl
; CHECK-NOT: __iand
; OFF-LABEL: _word:
; OFF: call __iand
define i24 @word(i24 %x) optsize {
 %a = and i24 %x, 65535
 ret i24 %a
}
; CHECK-LABEL: _clr:
; CHECK: res 0, {{[a-z]}}
; CHECK-NOT: __iand
; OFF-LABEL: _clr:
; OFF: call __iand
define i24 @clr(i24 %x) optsize {
 %a = and i24 %x, -2
 ret i24 %a
}
; CHECK-LABEL: _set:
; CHECK: set 1, {{[a-z]}}
; CHECK-NOT: __ior
; OFF-LABEL: _set:
; OFF: call __ior
define i24 @set(i24 %x) optsize {
 %a = or i24 %x, 512
 ret i24 %a
}
; CHECK-LABEL: _clr16:
; CHECK: res 0, {{[a-z]}}
; CHECK-NOT: __sand
; OFF-LABEL: _clr16:
; OFF: call __sand
define i16 @clr16(i16 %x) optsize {
 %a = and i16 %x, -2
 ret i16 %a
}

; CHECK-LABEL: _xor_byte:
; CHECK: xor a, 85
; OFF-LABEL: _xor_byte:
; OFF: xor a,
define i8 @xor_byte(i8 %x) optsize {
 %a = xor i8 %x, 85
 ret i8 %a
}

; CHECK-LABEL: _upper_other:
; CHECK: call __iand
; OFF-LABEL: _upper_other:
; OFF: call __iand
define i24 @upper_other(i24 %x) optsize {
 %a = and i24 %x, 11163050
 ret i24 %a
}

declare ptr @llvm.ptrmask.p0.i24(ptr, i24)
; CHECK-LABEL: _align_ptr:
; CHECK: and a, 248
; CHECK-NOT: call __iand
; OFF-LABEL: _align_ptr:
; OFF: call __iand
define ptr @align_ptr(ptr %p) optsize {
 %a = call ptr @llvm.ptrmask.p0.i24(ptr %p, i24 -8)
 ret ptr %a
}
