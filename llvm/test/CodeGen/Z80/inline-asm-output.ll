; RUN: llc -mtriple=ez80 -verify-machineinstrs < %s | FileCheck %s

define i24 @register_output() {
; CHECK-LABEL: _register_output:
; CHECK:       ;APP
; CHECK-NEXT:  ;NO_APP
; CHECK-NEXT:  ret
  %result = call i24 asm "", "=r"()
  ret i24 %result
}

define void @immediate_inputs() {
; CHECK-LABEL: _immediate_inputs:
; CHECK:       ; 7
; CHECK:       ; 42
; CHECK:       ret
  call void asm sideeffect "; $0", "I"(i24 7)
  call void asm sideeffect "; $0", "i"(i24 42)
  ret void
}
