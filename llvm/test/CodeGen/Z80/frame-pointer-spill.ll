; RUN: llc -mtriple=ez80 -O3 -verify-machineinstrs < %s | FileCheck %s

; Register allocation can introduce spill slots after reserved registers have
; been computed. IX must already be reserved when that happens because the
; spills make it the frame pointer.

define void @frame_spill() nounwind {
; CHECK-LABEL: frame_spill:
; CHECK:       push ix
; CHECK:       ld ix, 0
; CHECK-NOT:   ixl
; CHECK-NOT:   pop ix
; CHECK:       ld sp, ix
; CHECK-NEXT:  pop ix
; CHECK-NEXT:  ret
entry:
  %running = tail call i24 @keep_running()
  %stopped = icmp eq i24 %running, 0
  br i1 %stopped, label %exit, label %loop

loop:
  %vy = phi float [ 0.000000e+00, %entry ], [ %next.vy, %loop ]
  %x = phi float [ 1.600000e+02, %entry ], [ %next.x, %loop ]
  %y = phi float [ 1.200000e+02, %entry ], [ %next.y, %loop ]
  %next.vy = fadd fast float %vy, 9.800000e+01
  %next.x = fadd fast float %x, 3.000000e+01
  %next.y = fadd fast float %next.vy, %y
  tail call void @sink(float %next.x, float %next.y, float 3.000000e+01,
                       float %next.vy)
  %again = tail call i24 @keep_running()
  %done = icmp eq i24 %again, 0
  br i1 %done, label %exit, label %loop

exit:
  ret void
}

declare i24 @keep_running()
declare void @sink(float, float, float, float)
