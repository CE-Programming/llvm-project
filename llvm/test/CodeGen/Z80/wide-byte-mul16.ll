; NOTE: Do not autogenerate
; RUN: llc -mtriple=z80 -O2 -o - %s | FileCheck %s --check-prefix=Z80
; RUN: llc -mtriple=ez80-code16 -O2 -o - %s | FileCheck %s --check-prefix=EZ80
; RUN: llc -mtriple=z80 -mcpu=z180 -O2 -o - %s | FileCheck %s --check-prefix=EZ80
; RUN: llc -mtriple=ez80-code16 -O2 -stop-after=legalizer -verify-machineinstrs -o - %s | FileCheck %s --check-prefix=MIR

; Z80-LABEL: _byte_product16:
; Z80-NOT: mlt
; Z80: call __smulu
; EZ80-LABEL: _byte_product16:
; EZ80: mlt
; EZ80-NOT: call __smulu
; EZ80: ret
; MIR-LABEL: name: byte_product16
; MIR: MLT16r

define i16 @byte_product16(i8 %a, i8 %b) {
  %aa = zext i8 %a to i16
  %bb = zext i8 %b to i16
  %r = mul i16 %aa, %bb
  ret i16 %r
}
