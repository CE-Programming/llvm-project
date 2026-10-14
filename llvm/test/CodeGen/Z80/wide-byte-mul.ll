; NOTE: Do not autogenerate
; RUN: llc -mtriple=ez80 -O2 -o - %s | FileCheck %s --check-prefix=EZ80
; RUN: llc -mtriple=ez80 -O2 -stop-after=legalizer -verify-machineinstrs -o - %s | FileCheck %s --check-prefix=MIR
; RUN: llc -mtriple=ez80 -O2 -z80-wide-byte-mul=false -o - %s | FileCheck %s --check-prefix=OFF

; EZ80-LABEL: _byte_product24:
; EZ80-NOT: call __imulu
; EZ80: mlt
; EZ80-NOT: call __imulu
; EZ80: ret
; MIR-LABEL: name: byte_product24
; MIR: MLT24r
; OFF-LABEL: _byte_product24:
; OFF: call __imulu

define i24 @byte_product24(i8 %a, i8 %b) {
  %aa = zext i8 %a to i24
  %bb = zext i8 %b to i24
  %r = mul i24 %aa, %bb
  ret i24 %r
}

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

; EZ80-LABEL: _masked_product:
; EZ80: mlt
; EZ80-NOT: call __imulu
; EZ80: ret
; MIR-LABEL: name: masked_product
; MIR: MLT24r

define i24 @masked_product(i24 %a, i24 %b) {
  %aa = and i24 %a, 255
  %bb = and i24 %b, 255
  %r = mul i24 %aa, %bb
  ret i24 %r
}

; EZ80-LABEL: _byte_times255:
; EZ80: mlt
; EZ80: ret

define i24 @byte_times255(i8 %a) {
  %aa = zext i8 %a to i24
  %r = mul i24 %aa, 255
  ret i24 %r
}

; EZ80-LABEL: _unknown_product:
; EZ80-NOT: mlt
; EZ80: call __imulu

define i24 @unknown_product(i24 %a, i24 %b) {
  %r = mul i24 %a, %b
  ret i24 %r
}

; EZ80-LABEL: _signed_byte_product:
; EZ80-NOT: mlt
; EZ80: call __imulu

define i24 @signed_byte_product(i8 %a, i8 %b) {
  %aa = sext i8 %a to i24
  %bb = sext i8 %b to i24
  %r = mul i24 %aa, %bb
  ret i24 %r
}

; EZ80-LABEL: _one_unknown_operand:
; EZ80-NOT: mlt
; EZ80: call __imulu

define i24 @one_unknown_operand(i8 %a, i24 %b) {
  %aa = zext i8 %a to i24
  %r = mul i24 %aa, %b
  ret i24 %r
}

; EZ80-LABEL: _nine_bit_operand:
; EZ80-NOT: mlt
; EZ80: call __imulu

define i24 @nine_bit_operand(i24 %a, i8 %b) {
  %aa = and i24 %a, 511
  %bb = zext i8 %b to i24
  %r = mul i24 %aa, %bb
  ret i24 %r
}
