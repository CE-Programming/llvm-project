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

; MLT-LABEL: _corrected7:
; MLT: mlt
; MLT-NOT: call
; MLT: ret
; MIR-LABEL: name: corrected7
; MIR: MLT16r
; MIR: G_SUB
; MIR: G_ADD
; Z80-LABEL: _corrected7:
; Z80-NOT: mlt
; Z80: call

define i16 @corrected7(i8 %a) {
  %n = zext i8 %a to i16
  %r = udiv i16 %n, 7
  ret i16 %r
}

; The expensive short-mode remainder correction exceeds its size estimate's
; speed budget. Retain the ordinary divrem combine for this mixed-budget
; pair; the current DIVREM legalization lowers its two members independently.
; MLT-LABEL: _separate_pair63:
; MLT: mlt
; MLT: call __sremu
; MLT: ret
; MIR-LABEL: name: separate_pair63
; MIR: MLT16r
; Z80-LABEL: _separate_pair63:
; Z80: call

define i16 @separate_pair63(i8 %a) {
  %n = zext i8 %a to i16
  %r = urem i16 %n, 63
  %q = udiv i16 %n, 63
  %sum = add i16 %r, %q
  ret i16 %sum
}

; Both size-mode members exceed their budgets, so keep the paired fallback.
; MLT-LABEL: _size_pair3:
; MLT-NOT: mlt
; MLT: call __sdivu
; MLT: call __sremu
; MLT: ret
; MIR-LABEL: name: size_pair3
; MIR-NOT: MLT16r
; Z80-LABEL: _size_pair3:
; Z80: call __sdivu
; Z80: call __sremu

define i16 @size_pair3(i8 %a) minsize optsize {
  %n = zext i8 %a to i16
  %q = udiv i16 %n, 3
  %r = urem i16 %n, 3
  %sum = add i16 %q, %r
  ret i16 %sum
}
