; NOTE: Do not autogenerate
; RUN: llc -O2 -mtriple=z80 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -O2 -mtriple=ez80-code16 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -O2 -mtriple=ez80 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -O0 -mtriple=z80 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -O0 -mtriple=ez80-code16 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -O0 -mtriple=ez80 -verify-machineinstrs < %s | FileCheck %s
;
; Ordered/unordered predicates must inspect NaNs instead of returning constants.
; Strict numerical predicates also canonicalize signed zero for the ROM helper.
; Explicit nnan+nsz permits the existing compact comparison path.

; CHECK-LABEL: unordered32:
; CHECK: call __lcmpu
; CHECK: ret
define i1 @unordered32(float %a, float %b) {
  %r = fcmp uno float %a, %b
  ret i1 %r
}

; CHECK-LABEL: ordered64:
; CHECK: call __llcmpu
; CHECK: ret
define i1 @ordered64(double %a, double %b) {
  %r = fcmp ord double %a, %b
  ret i1 %r
}

; CHECK-LABEL: less32:
; CHECK: call __fcmp
; CHECK: ret
define i1 @less32(float %a, float %b) {
  %r = fcmp olt float %a, %b
  ret i1 %r
}

; CHECK-LABEL: unordered_less64:
; CHECK: call __dcmp
; CHECK: ret
define i1 @unordered_less64(double %a, double %b) {
  %r = fcmp ult double %a, %b
  ret i1 %r
}

; CHECK-LABEL: finite_fast32:
; CHECK-NOT: call __lcmpu
; CHECK: call __fcmp
; CHECK-NOT: call __lcmpu
; CHECK: ret
define i1 @finite_fast32(float %a, float %b) {
  %r = fcmp nnan nsz olt float %a, %b
  ret i1 %r
}
