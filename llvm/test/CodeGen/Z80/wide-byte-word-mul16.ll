; NOTE: Do not autogenerate
; RUN: llc -mtriple=ez80-code16 -O2 -o - %s | FileCheck %s --check-prefix=MLT
; RUN: llc -mtriple=z80 -mcpu=z180 -O2 -o - %s | FileCheck %s --check-prefix=MLT
; RUN: llc -mtriple=z80 -O2 -o - %s | FileCheck %s --check-prefix=Z80
; RUN: llc -mtriple=ez80-code16 -O2 -stop-after=legalizer -verify-machineinstrs -o - %s | FileCheck %s --check-prefix=MIR

; MLT-LABEL: _byte_word:
; MLT: mlt
; MLT: mlt
; MLT-NOT: call __smulu
; MLT: ret
; MIR-LABEL: name: byte_word
; MIR: MLT16r
; MIR: MLT16r
; Z80-LABEL: _byte_word:
; Z80-NOT: mlt
; Z80: call __smulu

define i16 @byte_word(i8 %a, i16 %b) {
  %aa = zext i8 %a to i16
  %r = mul i16 %aa, %b
  ret i16 %r
}
