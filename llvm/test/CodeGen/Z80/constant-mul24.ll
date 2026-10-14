; NOTE: Do not autogenerate
; RUN: llc -mtriple=ez80 -O2 -o - %s | FileCheck %s --check-prefix=ADL
; RUN: llc -mtriple=ez80 -O2 -stop-after=legalizer -verify-machineinstrs -o /dev/null %s
; RUN: llc -mtriple=ez80 -O0 -o - %s | FileCheck %s --check-prefix=O0
; RUN: llc -mtriple=ez80 -O2 -z80-constant-mul=false -o - %s | FileCheck %s --check-prefix=OFF

; ADL-LABEL: _mul24_size_3:
; ADL-NOT: call __imulu
; ADL: add
; ADL-NOT: call __imulu
; ADL: ret
; O0-LABEL: _mul24_size_3:
; O0-NOT: call __imulu
; O0: add
; O0-NOT: call __imulu
; O0: ret
define i24 @mul24_size_3(i24 %x) minsize optsize {
  %r = mul i24 %x, 3
  ret i24 %r
}

; ADL-LABEL: _mul24_speed_3:
; ADL-NOT: call __imulu
; ADL: add
; ADL-NOT: call __imulu
; ADL: ret
; O0-LABEL: _mul24_speed_3:
; O0-NOT: call __imulu
; O0: add
; O0-NOT: call __imulu
; O0: ret
define i24 @mul24_speed_3(i24 %x) {
  %r = mul i24 3, %x
  ret i24 %r
}

; ADL-LABEL: _mul24_size_5:
; ADL-NOT: call __imulu
; ADL: add
; ADL-NOT: call __imulu
; ADL: ret
; OFF-LABEL: _mul24_size_5:
; OFF: call __imulu
define i24 @mul24_size_5(i24 %x) minsize optsize {
  %r = mul i24 %x, 5
  ret i24 %r
}

; ADL-LABEL: _mul24_speed_5:
; ADL-NOT: call __imulu
; ADL: add
; ADL-NOT: call __imulu
; ADL: ret
; OFF-LABEL: _mul24_speed_5:
; OFF: call __imulu
define i24 @mul24_speed_5(i24 %x) {
  %r = mul i24 5, %x
  ret i24 %r
}

; ADL-LABEL: _mul24_size_6:
; ADL-NOT: call __imulu
; ADL: add
; ADL-NOT: call __imulu
; ADL: ret
define i24 @mul24_size_6(i24 %x) minsize optsize {
  %r = mul i24 %x, 6
  ret i24 %r
}

; ADL-LABEL: _mul24_speed_6:
; ADL-NOT: call __imulu
; ADL: add
; ADL-NOT: call __imulu
; ADL: ret
define i24 @mul24_speed_6(i24 %x) {
  %r = mul i24 6, %x
  ret i24 %r
}

; ADL-LABEL: _mul24_size_7:
; ADL-NOT: call __imulu
; ADL: add
; ADL-NOT: call __imulu
; ADL: ret
define i24 @mul24_size_7(i24 %x) minsize optsize {
  %r = mul i24 %x, 7
  ret i24 %r
}

; ADL-LABEL: _mul24_speed_7:
; ADL-NOT: call __imulu
; ADL: add
; ADL-NOT: call __imulu
; ADL: ret
define i24 @mul24_speed_7(i24 %x) {
  %r = mul i24 7, %x
  ret i24 %r
}

; ADL-LABEL: _mul24_size_9:
; ADL-NOT: call __imulu
; ADL: add
; ADL-NOT: call __imulu
; ADL: ret
define i24 @mul24_size_9(i24 %x) minsize optsize {
  %r = mul i24 %x, 9
  ret i24 %r
}

; ADL-LABEL: _mul24_speed_9:
; ADL-NOT: call __imulu
; ADL: add
; ADL-NOT: call __imulu
; ADL: ret
define i24 @mul24_speed_9(i24 %x) {
  %r = mul i24 9, %x
  ret i24 %r
}

; ADL-LABEL: _mul24_size_10:
; ADL-NOT: call __imulu
; ADL: add
; ADL-NOT: call __imulu
; ADL: ret
define i24 @mul24_size_10(i24 %x) minsize optsize {
  %r = mul i24 %x, 10
  ret i24 %r
}

; ADL-LABEL: _mul24_speed_10:
; ADL-NOT: call __imulu
; ADL: add
; ADL-NOT: call __imulu
; ADL: ret
define i24 @mul24_speed_10(i24 %x) {
  %r = mul i24 10, %x
  ret i24 %r
}

; ADL-LABEL: _mul24_size_15:
; ADL-NOT: call __imulu
; ADL: add
; ADL-NOT: call __imulu
; ADL: ret
define i24 @mul24_size_15(i24 %x) minsize optsize {
  %r = mul i24 %x, 15
  ret i24 %r
}

; ADL-LABEL: _mul24_speed_15:
; ADL-NOT: call __imulu
; ADL: add
; ADL-NOT: call __imulu
; ADL: ret
define i24 @mul24_speed_15(i24 %x) {
  %r = mul i24 15, %x
  ret i24 %r
}

; ADL-LABEL: _mul24_size_17:
; ADL-NOT: call __imulu
; ADL: add
; ADL-NOT: call __imulu
; ADL: ret
define i24 @mul24_size_17(i24 %x) minsize optsize {
  %r = mul i24 %x, 17
  ret i24 %r
}

; ADL-LABEL: _mul24_speed_17:
; ADL-NOT: call __imulu
; ADL: add
; ADL-NOT: call __imulu
; ADL: ret
define i24 @mul24_speed_17(i24 %x) {
  %r = mul i24 17, %x
  ret i24 %r
}

; ADL-LABEL: _mul24_size_33:
; ADL-NOT: call __imulu
; ADL: add
; ADL-NOT: call __imulu
; ADL: ret
define i24 @mul24_size_33(i24 %x) minsize optsize {
  %r = mul i24 %x, 33
  ret i24 %r
}

; ADL-LABEL: _mul24_speed_33:
; ADL-NOT: call __imulu
; ADL: add
; ADL-NOT: call __imulu
; ADL: ret
define i24 @mul24_speed_33(i24 %x) {
  %r = mul i24 33, %x
  ret i24 %r
}

; ADL-LABEL: _mul24_size_65:
; ADL: call __imulu
define i24 @mul24_size_65(i24 %x) minsize optsize {
  %r = mul i24 %x, 65
  ret i24 %r
}

; ADL-LABEL: _mul24_speed_65:
; ADL-NOT: call __imulu
; ADL: add
; ADL-NOT: call __imulu
; ADL: ret
define i24 @mul24_speed_65(i24 %x) {
  %r = mul i24 65, %x
  ret i24 %r
}

; ADL-LABEL: _mul24_size_255:
; ADL: call __imulu
define i24 @mul24_size_255(i24 %x) minsize optsize {
  %r = mul i24 %x, 255
  ret i24 %r
}

; ADL-LABEL: _mul24_speed_255:
; ADL-NOT: call __imulu
; ADL: add
; ADL-NOT: call __imulu
; ADL: ret
define i24 @mul24_speed_255(i24 %x) {
  %r = mul i24 %x, 255
  ret i24 %r
}

; ADL-LABEL: _live_input:
; ADL-NOT: call __imulu
; ADL: add
; ADL-NOT: call __imulu
; ADL: ret
define i24 @live_input(i24 %x, ptr %p) minsize optsize {
  %r = mul i24 %x, 5
  store i24 %r, ptr %p
  ret i24 %x
}

; ADL-LABEL: _large_constant:
; ADL: call __imulu
define i24 @large_constant(i24 %x) minsize optsize {
  %r = mul i24 %x, 12345
  ret i24 %r
}

; ADL-LABEL: _negative_constant:
; ADL: call __imulu
define i24 @negative_constant(i24 %x) minsize optsize {
  %r = mul i24 %x, -3
  ret i24 %r
}

; ADL-LABEL: _variable:
; ADL: call __imulu
define i24 @variable(i24 %x, i24 %y) {
  %r = mul i24 %x, %y
  ret i24 %r
}
