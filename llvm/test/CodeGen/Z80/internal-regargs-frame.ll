; NOTE: Do not autogenerate
; RUN: llc -mtriple=ez80 -O2 -verify-machineinstrs -o - %s | FileCheck %s --check-prefix=ADL
; RUN: llc -mtriple=ez80-code16 -O2 -verify-machineinstrs -o - %s | FileCheck %s --check-prefix=ADL
; RUN: llc -mtriple=z80 -O2 -verify-machineinstrs -o - %s | FileCheck %s --check-prefix=SHORT
;
; The usual size-oriented frame libcalls consume HL (and DE for _frameset).
; Incoming register arguments therefore need inline frame setup and IY scratch.
; ADL-LABEL: _framed:
; ADL-NOT: call __frameset
; ADL-NOT: ld hl, -
; ADL: push ix
; ADL: ld ix, 0
; ADL: add ix, sp
; ADL: lea iy, ix - 64
; ADL: ld sp, iy
; ADL: ld a, l
; ADL: ld (ix{{.*}}), a
; ADL: ld sp, ix
; ADL: pop ix
; ADL: ret
; SHORT-LABEL: _framed:
; SHORT-NOT: call __frameset
; SHORT-NOT: ld hl, -
; SHORT: push ix
; SHORT: ld ix, 0
; SHORT: add ix, sp
; SHORT: ld iy, -
; SHORT: add iy, sp
; SHORT: ld sp, iy
; SHORT: ld a, l
; SHORT: ld (ix{{.*}}), a
; SHORT: ld sp, ix
; SHORT: pop ix
; SHORT: ret

define cc 136 i16 @framed(i16 %a, i16 %b, i16 %c) minsize optsize {
  %buf = alloca [64 x i8], align 1
  %byte = trunc i16 %a to i8
  store volatile i8 %byte, ptr %buf
  %p = getelementptr [64 x i8], ptr %buf, i16 0, i16 63
  %other = trunc i16 %b to i8
  store volatile i8 %other, ptr %p
  %s = add i16 %a, %b
  %r = add i16 %s, %c
  ret i16 %r
}
