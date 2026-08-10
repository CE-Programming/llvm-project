; RUN: llc -mtriple=z80 < %s | FileCheck %s

@quoted = constant [4 x i8] c"a\22b\00"

; CHECK: _quoted:
; CHECK-NEXT: db "a\042b\000"
