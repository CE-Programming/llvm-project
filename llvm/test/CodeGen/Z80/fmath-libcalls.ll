; RUN: llc -mtriple=ez80 < %s | FileCheck %s

declare float @llvm.atan2.f32(float, float)
declare { double, double } @llvm.modf.f64(double)

define float @atan2f32(float %y, float %x) {
; CHECK-LABEL: _atan2f32:
; CHECK: call _atan2f
  %result = call float @llvm.atan2.f32(float %y, float %x)
  ret float %result
}

define float @negf32(float %x) {
; CHECK-LABEL: _negf32:
; CHECK: call __fneg
  %result = fneg float %x
  ret float %result
}

define double @modf64(double %x, ptr %intpart) {
; CHECK-LABEL: _modf64:
; CHECK: call _modfl
  %result = call { double, double } @llvm.modf.f64(double %x)
  %fractional = extractvalue { double, double } %result, 0
  %integral = extractvalue { double, double } %result, 1
  store double %integral, ptr %intpart
  ret double %fractional
}
