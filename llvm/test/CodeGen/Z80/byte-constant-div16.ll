; NOTE: Do not autogenerate
; RUN: llc -mtriple=ez80-code16 -O2 -o - %s | FileCheck %s --check-prefix=MLT
; RUN: llc -mtriple=z80 -mcpu=z180 -O2 -o - %s | FileCheck %s --check-prefix=MLT
; RUN: llc -mtriple=z80 -O2 -o - %s | FileCheck %s --check-prefix=Z80
; RUN: llc -mtriple=ez80-code16 -O2 -stop-after=legalizer -verify-machineinstrs -o - %s | FileCheck %s --check-prefix=MIR

; MLT-LABEL: _divide3:
; MLT: mlt
; MLT-NOT: call
; MLT: ret
; MIR-LABEL: name: divide3
; MIR: MLT16r
; Z80-LABEL: _divide3:
; Z80-NOT: mlt
; Z80: call

define i16 @divide3(i8 %a) {
  %n = zext i8 %a to i16
  %r = udiv i16 %n, 3
  ret i16 %r
}

; MLT-LABEL: _remainder3:
; MLT: mlt
; MLT: mlt
; MLT-NOT: call
; MLT: ret
; MIR-LABEL: name: remainder3
; MIR: MLT16r
; MIR: MLT16r
; Z80-LABEL: _remainder3:
; Z80-NOT: mlt
; Z80: call __sremu

define i16 @remainder3(i8 %a) {
  %n = zext i8 %a to i16
  %r = urem i16 %n, 3
  ret i16 %r
}

; An eight-bit mask is cheap at either optimization goal.
; MLT-LABEL: _size_remainder16:
; MLT-NOT: call __sremu
; MLT-NOT: mlt
; MLT: ret

define i16 @size_remainder16(i8 %a) minsize optsize {
  %n = zext i8 %a to i16
  %r = urem i16 %n, 16
  ret i16 %r
}
