; NOTE: Do not autogenerate
; RUN: split-file %s %t
; RUN: llc -mtriple=ez80 -O2 -verify-machineinstrs -o - %t/adl.ll | FileCheck %s --check-prefix=ADL
; RUN: llc -mtriple=ez80-code16 -O2 -verify-machineinstrs -o - %t/short.ll | FileCheck %s --check-prefix=SHORT
; RUN: llc -mtriple=z80 -O2 -verify-machineinstrs -o - %t/short.ll | FileCheck %s --check-prefix=SHORT
; RUN: llc -mtriple=ez80 -O0 -stop-after=irtranslator -verify-machineinstrs -o - %t/adl.ll | FileCheck %s --check-prefix=MIR
;
; C and existing fastcc callers still use the public stack ABI. cc 136 passes
; native-width arguments in HL/DE/BC, returns normally and preserves IX.

; ADL-LABEL: _reg_add:
; ADL: add hl, de
; ADL-NEXT: add hl, bc
; ADL-NEXT: ret
; ADL-LABEL: _public_add:
; ADL: ld iy,
; ADL: add iy, sp
; ADL: ret
; ADL-LABEL: _fast_add:
; ADL: ld iy,
; ADL: add iy, sp
; ADL: ret
; ADL-LABEL: _caller:
; ADL: ld hl, 11
; ADL: ld de, 22
; ADL: ld bc, 33
; ADL: jp _reg_add
; ADL-LABEL: _narrow:
; ADL-NOT: add iy, sp
; ADL: ld a, l
; ADL: ret
; ADL-LABEL: _reg_pointer:
; ADL: ld a, (hl)
; ADL-NEXT: ret
; MIR-LABEL: name: reg_add
; MIR: liveins: $ubc, $ude, $uhl
; MIR: COPY $uhl
; MIR: COPY $ude
; MIR: COPY $ubc
; MIR-LABEL: name: caller
; MIR: $uhl = COPY
; MIR: $ude = COPY
; MIR: $ubc = COPY
; MIR: TCRETURN24 @reg_add, csr_ez80_c, implicit $spl, implicit $uhl, implicit $ude, implicit $ubc

;--- adl.ll
define cc 136 i24 @reg_add(i24 %a, i24 %b, i24 %c) {
  %x = add i24 %a, %b
  %y = add i24 %x, %c
  ret i24 %y
}

define i24 @public_add(i24 %a, i24 %b, i24 %c) {
  %x = add i24 %a, %b
  %y = add i24 %x, %c
  ret i24 %y
}

define fastcc i24 @fast_add(i24 %a, i24 %b, i24 %c) {
  %x = add i24 %a, %b
  %y = add i24 %x, %c
  ret i24 %y
}

define i24 @caller() {
  %r = tail call cc 136 i24 @reg_add(i24 11, i24 22, i24 33)
  ret i24 %r
}

define cc 136 i8 @narrow(i8 %x, i16 %y) {
  %b = trunc i16 %y to i8
  %r = add i8 %x, %b
  ret i8 %r
}

define cc 136 i8 @reg_pointer(ptr %p) {
  %v = load i8, ptr %p
  ret i8 %v
}

; SHORT-LABEL: _reg_add:
; SHORT: add hl, de
; SHORT-NEXT: add hl, bc
; SHORT-NEXT: ret
; SHORT-LABEL: _public_add:
; SHORT: ld iy,
; SHORT: add iy, sp
; SHORT: ret
; SHORT-LABEL: _caller:
; SHORT: ld hl, 11
; SHORT: ld de, 22
; SHORT: ld bc, 33
; SHORT: jp _reg_add
; SHORT-LABEL: _narrow:
; SHORT-NOT: add iy, sp
; SHORT: ld a, l
; SHORT: ret
;--- short.ll
define cc 136 i16 @reg_add(i16 %a, i16 %b, i16 %c) {
  %x = add i16 %a, %b
  %y = add i16 %x, %c
  ret i16 %y
}

define i16 @public_add(i16 %a, i16 %b, i16 %c) {
  %x = add i16 %a, %b
  %y = add i16 %x, %c
  ret i16 %y
}

define i16 @caller() {
  %r = tail call cc 136 i16 @reg_add(i16 11, i16 22, i16 33)
  ret i16 %r
}

define cc 136 i8 @narrow(i8 %x, i16 %y) {
  %b = trunc i16 %y to i8
  %r = add i8 %x, %b
  ret i8 %r
}
