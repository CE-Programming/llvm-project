; NOTE: Do not autogenerate
; RUN: llc -mtriple=ez80 -O2 -o - %s | FileCheck %s --check-prefix=EZ80
; RUN: llc -mtriple=ez80 -O2 -stop-after=legalizer -verify-machineinstrs -o - %s | FileCheck %s --check-prefix=MIR
; RUN: llc -mtriple=ez80 -O2 -z80-wide-byte-mul=false -o - %s | FileCheck %s --check-prefix=OFF

; Both complete partial products contribute to a 24-bit result. Overflow is
; wrapping at 16 bits in the second function; no signed operand assumptions are
; introduced merely because an input originated in a narrow integer type.
; EZ80-LABEL: _byte_word24:
; EZ80: mlt
; EZ80: mlt
; EZ80-NOT: call __imulu
; EZ80: ret
; MIR-LABEL: name: byte_word24
; MIR: MLT24r
; MIR: MLT24r
; OFF-LABEL: _byte_word24:
; OFF: call __imulu

define i24 @byte_word24(i8 %a, i16 %b) {
  %aa = zext i8 %a to i24
  %bb = zext i16 %b to i24
  %r = mul i24 %aa, %bb
  ret i24 %r
}

; EZ80-LABEL: _word_byte16:
; EZ80: mlt
; EZ80: mlt
; EZ80-NOT: call __smulu
; EZ80: ret
; MIR-LABEL: name: word_byte16
; MIR: MLT16r
; MIR: MLT16r
; OFF-LABEL: _word_byte16:
; OFF: call __smulu

define i16 @word_byte16(i16 %a, i8 %b) {
  %bb = zext i8 %b to i16
  %r = mul i16 %a, %bb
  ret i16 %r
}

; EZ80-LABEL: _masked_word24:
; EZ80: mlt
; EZ80: mlt
; EZ80-NOT: call __imulu
; EZ80: ret
; MIR-LABEL: name: masked_word24
; MIR: MLT24r
; MIR: MLT24r

define i24 @masked_word24(i24 %a, i24 %b) {
  %aa = and i24 %a, 255
  %bb = and i24 %b, 65535
  %r = mul i24 %aa, %bb
  ret i24 %r
}

; Preserve the compact runtime call under size optimization.
; EZ80-LABEL: _size_word24:
; EZ80-NOT: mlt
; EZ80: call __imulu

define i24 @size_word24(i8 %a, i16 %b) optsize {
  %aa = zext i8 %a to i24
  %bb = zext i16 %b to i24
  %r = mul i24 %aa, %bb
  ret i24 %r
}

; EZ80-LABEL: _signed_word24:
; EZ80-NOT: mlt
; EZ80: call __imulu

define i24 @signed_word24(i8 %a, i16 %b) {
  %aa = zext i8 %a to i24
  %bb = sext i16 %b to i24
  %r = mul i24 %aa, %bb
  ret i24 %r
}

; EZ80-LABEL: _wide_word24:
; EZ80-NOT: mlt
; EZ80: call __imulu

define i24 @wide_word24(i8 %a, i24 %b) {
  %aa = zext i8 %a to i24
  %bb = and i24 %b, 131071
  %r = mul i24 %aa, %bb
  ret i24 %r
}
