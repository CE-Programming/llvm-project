; RUN: llc -mtriple=ez80 < %s | FileCheck %s

declare float @llvm.atan2.f32(float, float)

define float @atan2f32(float %y, float %x) {
; CHECK-LABEL: _atan2f32:
; CHECK: call _atan2f
  %result = call float @llvm.atan2.f32(float %y, float %x)
  ret float %result
}
