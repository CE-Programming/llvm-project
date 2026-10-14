; NOTE: Do not autogenerate
; RUN: llc -mtriple=ez80 -O2 -o - %s | FileCheck %s --check-prefix=EZ80
; RUN: llc -mtriple=ez80 -O2 -stop-after=legalizer -verify-machineinstrs -o - %s | FileCheck %s --check-prefix=MIR
; RUN: llc -mtriple=ez80 -O2 -z80-byte-constant-div=false -o - %s | FileCheck %s --check-prefix=OFF

; The reciprocal is checked over the complete 0..255 dividend domain, not a
; signed range or an approximation. Dividing a widened byte must reach MLT
; before the generic combiner expands it into full-width multiply libcalls.

; EZ80-LABEL: _udiv8_3:
; EZ80: mlt
; EZ80-NOT: call
; EZ80: ret
; MIR-LABEL: name: udiv8_3
; MIR: MLT16r

define i8 @udiv8_3(i8 %a) {
  %r = udiv i8 %a, 3
  ret i8 %r
}

; EZ80-LABEL: _urem8_3:
; EZ80: mlt
; EZ80: mlt
; EZ80-NOT: call
; EZ80: ret
; MIR-LABEL: name: urem8_3
; MIR: MLT16r
; MIR: MLT16r
; OFF-LABEL: _urem8_3:
; OFF: call __bremu

define i8 @urem8_3(i8 %a) {
  %r = urem i8 %a, 3
  ret i8 %r
}

; EZ80-LABEL: _udiv16_5:
; EZ80: mlt
; EZ80-NOT: call
; EZ80: ret
; MIR-LABEL: name: udiv16_5
; MIR: MLT16r
; OFF-LABEL: _udiv16_5:
; OFF-NOT: mlt
; OFF: call

define i16 @udiv16_5(i8 %a) {
  %n = zext i8 %a to i16
  %r = udiv i16 %n, 5
  ret i16 %r
}

; EZ80-LABEL: _urem16_5:
; EZ80: mlt
; EZ80: mlt
; EZ80-NOT: call
; EZ80: ret
; MIR-LABEL: name: urem16_5
; MIR: MLT16r
; MIR: MLT16r
; OFF-LABEL: _urem16_5:
; OFF: call __sremu

define i16 @urem16_5(i8 %a) {
  %n = zext i8 %a to i16
  %r = urem i16 %n, 5
  ret i16 %r
}

; EZ80-LABEL: _udiv24_3:
; EZ80: mlt
; EZ80-NOT: call
; EZ80: ret
; MIR-LABEL: name: udiv24_3
; MIR: MLT16r
; OFF-LABEL: _udiv24_3:
; OFF-NOT: mlt
; OFF: call

define i24 @udiv24_3(i8 %a) {
  %n = zext i8 %a to i24
  %r = udiv i24 %n, 3
  ret i24 %r
}

; EZ80-LABEL: _urem24_3:
; EZ80: mlt
; EZ80: mlt
; EZ80-NOT: call
; EZ80: ret
; MIR-LABEL: name: urem24_3
; MIR: MLT16r
; MIR: MLT16r
; OFF-LABEL: _urem24_3:
; OFF: call __iremu

define i24 @urem24_3(i8 %a) {
  %n = zext i8 %a to i24
  %r = urem i24 %n, 3
  ret i24 %r
}

; EZ80-LABEL: _udiv24_10:
; EZ80: mlt
; EZ80-NOT: call
; EZ80: ret
; MIR-LABEL: name: udiv24_10
; MIR: MLT16r
; OFF-LABEL: _udiv24_10:
; OFF-NOT: mlt
; OFF: call

define i24 @udiv24_10(i8 %a) {
  %n = zext i8 %a to i24
  %r = udiv i24 %n, 10
  ret i24 %r
}

; EZ80-LABEL: _urem24_10:
; EZ80: mlt
; EZ80: mlt
; EZ80-NOT: call
; EZ80: ret
; MIR-LABEL: name: urem24_10
; MIR: MLT16r
; MIR: MLT16r
; OFF-LABEL: _urem24_10:
; OFF: call __iremu

define i24 @urem24_10(i8 %a) {
  %n = zext i8 %a to i24
  %r = urem i24 %n, 10
  ret i24 %r
}

; EZ80-LABEL: _udiv24_255:
; EZ80-NOT: mlt
; EZ80-NOT: call
; EZ80: ret
; MIR-LABEL: name: udiv24_255
; MIR: G_ICMP intpred(uge)
; OFF-LABEL: _udiv24_255:
; OFF-NOT: mlt
; OFF: call

define i24 @udiv24_255(i8 %a) {
  %n = zext i8 %a to i24
  %r = udiv i24 %n, 255
  ret i24 %r
}

; EZ80-LABEL: _urem24_255:
; EZ80-NOT: mlt
; EZ80-NOT: call
; EZ80: ret
; MIR-LABEL: name: urem24_255
; MIR: G_ICMP intpred(uge)
; OFF-LABEL: _urem24_255:
; OFF: call __iremu

define i24 @urem24_255(i8 %a) {
  %n = zext i8 %a to i24
  %r = urem i24 %n, 255
  ret i24 %r
}

; Rejected cases retain the previous runtime lowering.
; EZ80-LABEL: _inexact7:
; EZ80-NOT: mlt
; EZ80: call

define i24 @inexact7(i8 %a) {
  %n = zext i8 %a to i24
  %r = udiv i24 %n, 7
  ret i24 %r
}

; EZ80-LABEL: _unknown_dividend:
; EZ80-NOT: mlt
; EZ80: call

define i24 @unknown_dividend(i24 %a) {
  %r = udiv i24 %a, 3
  ret i24 %r
}

; EZ80-LABEL: _signed_extension:
; EZ80-NOT: mlt
; EZ80: call

define i24 @signed_extension(i8 %a) {
  %n = sext i8 %a to i24
  %r = urem i24 %n, 3
  ret i24 %r
}

; EZ80-LABEL: _signed_division:
; EZ80-NOT: mlt
; EZ80: call

define i24 @signed_division(i8 %a) {
  %n = zext i8 %a to i24
  %r = sdiv i24 %n, 3
  ret i24 %r
}

; Long reciprocal sequences exceed the size-function budget.
; EZ80-LABEL: _size_div:
; EZ80-NOT: mlt
; EZ80: call

define i24 @size_div(i8 %a) optsize {
  %n = zext i8 %a to i24
  %r = udiv i24 %n, 3
  ret i24 %r
}

; EZ80-LABEL: _size_rem:
; EZ80-NOT: mlt
; EZ80: call

define i8 @size_rem(i8 %a) minsize optsize {
  %r = urem i8 %a, 3
  ret i8 %r
}

; A divisor above the proven maximum gives an exact constant zero quotient
; and the original dividend as remainder, including unsigned-negative APInts.
; EZ80-LABEL: _above_byte_range:
; EZ80-NOT: call
; EZ80-NOT: mlt
; EZ80: ret

define i24 @above_byte_range(i8 %a) {
  %n = zext i8 %a to i24
  %q = udiv i24 %n, 256
  %r = urem i24 %n, -1
  %sum = add i24 %q, %r
  ret i24 %sum
}

; Separate / and % operations must not combine into a divrem libcall before
; their hardware expansions have been applied.
; EZ80-LABEL: _quotient_and_remainder:
; EZ80: mlt
; EZ80-NOT: call
; EZ80: ret

define i24 @quotient_and_remainder(i8 %a) {
  %n = zext i8 %a to i24
  %q = udiv i24 %n, 3
  %r = urem i24 %n, 3
  %sum = add i24 %q, %r
  ret i24 %sum
}

; EZ80-LABEL: _masked_dividend:
; EZ80: mlt
; EZ80-NOT: call __idivu
; EZ80: ret

define i24 @masked_dividend(i24 %a) {
  %n = and i24 %a, 255
  %r = udiv i24 %n, 10
  ret i24 %r
}

; EZ80-LABEL: _variable_divisor:
; EZ80-NOT: mlt
; EZ80: call __idivu

define i24 @variable_divisor(i8 %a, i24 %b) {
  %n = zext i8 %a to i24
  %r = udiv i24 %n, %b
  ret i24 %r
}

; EZ80-LABEL: _nine_bit_dividend:
; EZ80-NOT: mlt
; EZ80: call

define i24 @nine_bit_dividend(i24 %a) {
  %n = and i24 %a, 511
  %r = udiv i24 %n, 3
  ret i24 %r
}

; A byte divided by a value in 128..255 has a boolean quotient. This comparison
; is compact enough for a size build, even when a reciprocal would need seven
; shifts; the remainder selects the unchanged input or one subtraction.
; EZ80-LABEL: _size_boolean_quotient:
; EZ80-NOT: mlt
; EZ80-NOT: call __idivu
; EZ80-NOT: call __i48mulu
; EZ80: ret
; MIR-LABEL: name: size_boolean_quotient
; MIR: G_ICMP intpred(uge)

define i24 @size_boolean_quotient(i8 %a) minsize optsize {
  %n = zext i8 %a to i24
  %r = udiv i24 %n, 129
  ret i24 %r
}

; EZ80-LABEL: _size_boolean_remainder:
; EZ80-NOT: mlt
; EZ80-NOT: call __iremu
; EZ80: ret
; MIR-LABEL: name: size_boolean_remainder
; MIR: G_ICMP intpred(uge)
; MIR: G_SELECT

define i24 @size_boolean_remainder(i8 %a) minsize optsize {
  %n = zext i8 %a to i24
  %r = urem i24 %n, 129
  ret i24 %r
}
