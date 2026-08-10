; RUN: llc -mtriple=z80 < %s | FileCheck %s
; RUN: llc -mtriple=ez80-code16 < %s | FileCheck %s
; RUN: llc -mtriple=ez80 < %s | FileCheck %s

; CHECK-LABEL: generic:
; CHECK:       ei
; CHECK-NEXT:  reti
define preserve_allcc void @generic() #0 {
  ret void
}

; CHECK-LABEL: nested:
; CHECK:       ei
; CHECK-NEXT:  reti
define preserve_allcc void @nested() #1 {
  ret void
}

; CHECK-LABEL: nmi:
; CHECK-NOT:   ei
; CHECK:       retn
define preserve_allcc void @nmi() #2 {
  ret void
}

attributes #0 = { "interrupt"="Generic" }
attributes #1 = { "interrupt"="Nested" }
attributes #2 = { "interrupt"="NMI" }
