; NOTE: Do not autogenerate
; RUN: llc -mtriple=ez80 -O2 -o - %s | FileCheck %s --check-prefix=ADL
; RUN: llc -mtriple=ez80 -O2 -stop-after=legalizer -verify-machineinstrs -o /dev/null %s
; RUN: llc -mtriple=ez80 -O0 -o - %s | FileCheck %s --check-prefix=O0
; RUN: llc -mtriple=ez80 -O2 -z80-constant-mul=false -o - %s | FileCheck %s --check-prefix=OFF
; RUN: llc -mtriple=ez80-code16 -O2 -o - %s | FileCheck %s --check-prefix=SHORT
; RUN: llc -mtriple=z80 -O2 -o - %s | FileCheck %s --check-prefix=SHORT
; RUN: llc -mtriple=z80 -O2 -stop-after=legalizer -verify-machineinstrs -o /dev/null %s

; ADL-LABEL: _mul16_size_3:
; ADL-NOT: call __smulu
; ADL: add
; ADL-NOT: call __smulu
; ADL: ret
; O0-LABEL: _mul16_size_3:
; O0-NOT: call __smulu
; O0: add
; O0-NOT: call __smulu
; O0: ret
; SHORT-LABEL: _mul16_size_3:
; SHORT-NOT: call __smulu
; SHORT: add
; SHORT-NOT: call __smulu
; SHORT: ret
define i16 @mul16_size_3(i16 %x) minsize optsize {
  %r = mul i16 %x, 3
  ret i16 %r
}

; ADL-LABEL: _mul16_speed_3:
; ADL-NOT: call __smulu
; ADL: add
; ADL-NOT: call __smulu
; ADL: ret
; O0-LABEL: _mul16_speed_3:
; O0-NOT: call __smulu
; O0: add
; O0-NOT: call __smulu
; O0: ret
; SHORT-LABEL: _mul16_speed_3:
; SHORT-NOT: call __smulu
; SHORT: add
; SHORT-NOT: call __smulu
; SHORT: ret
define i16 @mul16_speed_3(i16 %x) {
  %r = mul i16 3, %x
  ret i16 %r
}

; ADL-LABEL: _mul16_size_5:
; ADL-NOT: call __smulu
; ADL: add
; ADL-NOT: call __smulu
; ADL: ret
; OFF-LABEL: _mul16_size_5:
; OFF: call __smulu
; SHORT-LABEL: _mul16_size_5:
; SHORT-NOT: call __smulu
; SHORT: add
; SHORT-NOT: call __smulu
; SHORT: ret
define i16 @mul16_size_5(i16 %x) minsize optsize {
  %r = mul i16 %x, 5
  ret i16 %r
}

; ADL-LABEL: _mul16_speed_5:
; ADL-NOT: call __smulu
; ADL: add
; ADL-NOT: call __smulu
; ADL: ret
; OFF-LABEL: _mul16_speed_5:
; OFF: call __smulu
; SHORT-LABEL: _mul16_speed_5:
; SHORT-NOT: call __smulu
; SHORT: add
; SHORT-NOT: call __smulu
; SHORT: ret
define i16 @mul16_speed_5(i16 %x) {
  %r = mul i16 5, %x
  ret i16 %r
}

; ADL-LABEL: _mul16_size_7:
; ADL: call __smulu
; SHORT-LABEL: _mul16_size_7:
; SHORT-NOT: call __smulu
; SHORT: add
; SHORT-NOT: call __smulu
; SHORT: ret
define i16 @mul16_size_7(i16 %x) minsize optsize {
  %r = mul i16 %x, 7
  ret i16 %r
}

; ADL-LABEL: _mul16_speed_7:
; ADL-NOT: call __smulu
; ADL: add
; ADL-NOT: call __smulu
; ADL: ret
; SHORT-LABEL: _mul16_speed_7:
; SHORT-NOT: call __smulu
; SHORT: add
; SHORT-NOT: call __smulu
; SHORT: ret
define i16 @mul16_speed_7(i16 %x) {
  %r = mul i16 7, %x
  ret i16 %r
}

; ADL-LABEL: _mul16_size_9:
; ADL: call __smulu
; SHORT-LABEL: _mul16_size_9:
; SHORT-NOT: call __smulu
; SHORT: add
; SHORT-NOT: call __smulu
; SHORT: ret
define i16 @mul16_size_9(i16 %x) minsize optsize {
  %r = mul i16 %x, 9
  ret i16 %r
}

; ADL-LABEL: _mul16_speed_9:
; ADL-NOT: call __smulu
; ADL: add
; ADL-NOT: call __smulu
; ADL: ret
; SHORT-LABEL: _mul16_speed_9:
; SHORT-NOT: call __smulu
; SHORT: add
; SHORT-NOT: call __smulu
; SHORT: ret
define i16 @mul16_speed_9(i16 %x) {
  %r = mul i16 9, %x
  ret i16 %r
}

; ADL-LABEL: _large_constant:
; ADL: call __smulu
define i16 @large_constant(i16 %x) minsize optsize {
  %r = mul i16 %x, 12345
  ret i16 %r
}

; ADL-LABEL: _negative_constant:
; ADL: call __smulu
define i16 @negative_constant(i16 %x) minsize optsize {
  %r = mul i16 %x, -3
  ret i16 %r
}

; ADL-LABEL: _variable:
; ADL: call __smulu
define i16 @variable(i16 %x, i16 %y) {
  %r = mul i16 %x, %y
  ret i16 %r
}
