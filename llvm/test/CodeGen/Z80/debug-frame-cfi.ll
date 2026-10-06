; RUN: llc -mtriple=ez80 -O0 -z80-gas-style -verify-machineinstrs < %S/debug-frame-locations.ll | FileCheck %s --check-prefixes=FRAME,ADL
; RUN: llc -mtriple=z80 -O0 -z80-gas-style -verify-machineinstrs < %S/debug-frame-locations.ll | FileCheck %s --check-prefixes=FRAME,Z80
; RUN: llc -mtriple=ez80 -O0 -verify-machineinstrs < %S/debug-frame-locations.ll | FileCheck %s --check-prefix=LEGACY --implicit-check-not=.cfi_
; RUN: llc -mtriple=ez80 -O0 -z80-gas-style -verify-machineinstrs < %s | FileCheck %s --check-prefix=CALL
; RUN: sed 's/uwtable//g' %s | llc -mtriple=ez80 -O0 -z80-gas-style -verify-machineinstrs | FileCheck %s --check-prefix=RELEASE --implicit-check-not=.cfi_
;
; FRAME-LABEL: _ix_frame:
; FRAME: .cfi_startproc
; ADL: .cfi_def_cfa 4, 3
; ADL-NEXT: .cfi_offset 5, -3
; Z80: .cfi_def_cfa 4, 2
; Z80-NEXT: .cfi_offset 5, -2
; FRAME-NEXT: .cfi_same_value 6
; FRAME-NEXT: push ix
; ADL-NEXT: .cfi_def_cfa_offset 6
; ADL-NEXT: .cfi_offset 6, -6
; Z80-NEXT: .cfi_def_cfa_offset 4
; Z80-NEXT: .cfi_offset 6, -4
; FRAME-NEXT: ld ix, 0
; FRAME-NEXT: add ix, sp
; FRAME-NEXT: .cfi_def_cfa_register 6
; FRAME: pop ix
; ADL-NEXT: .cfi_def_cfa 4, 3
; Z80-NEXT: .cfi_def_cfa 4, 2
; FRAME-NEXT: .cfi_restore 6
; FRAME-NEXT: ret
; FRAME-LABEL: _iy_frame:
; FRAME: .cfi_same_value 6
; FRAME-NEXT: ld iy, 0
; FRAME-NEXT: add iy, sp
; FRAME-NEXT: .cfi_def_cfa_register 7
; ADL: .cfi_def_cfa 4, 6
; Z80: .cfi_def_cfa 4, 5
; FRAME: pop iy
; ADL-NEXT: .cfi_adjust_cfa_offset -3
; Z80-NEXT: .cfi_adjust_cfa_offset -2
; ADL-NEXT: .cfi_def_cfa 4, 3
; Z80: .cfi_def_cfa 4, 2
; FRAME-NEXT: ret
; LEGACY: _ix_frame:
; LEGACY: _iy_frame:
; RELEASE: _sp_calls:
; RELEASE: _multiple_returns:
;
; CALL-LABEL: _sp_calls:
; CALL: .cfi_def_cfa 4, 3
; CALL: ld hl, 1
; CALL-NEXT: ld de, 2
; CALL-NEXT: push de
; CALL-NEXT: .cfi_adjust_cfa_offset 3
; CALL-NEXT: push hl
; CALL-NEXT: .cfi_adjust_cfa_offset 3
; CALL-NEXT: call _sink
; CALL-NEXT: ld hl, 6
; CALL-NEXT: add hl, sp
; CALL-NEXT: ld sp, hl
; CALL-NEXT: .cfi_adjust_cfa_offset -6
; CALL-NEXT: .cfi_def_cfa 4, 3
; CALL-NEXT: ret
; CALL-LABEL: _multiple_returns:
; CALL: .cfi_remember_state
; CALL-NEXT: ld sp, ix
; CALL-NEXT: .cfi_def_cfa 4, 6
; CALL-NEXT: pop ix
; CALL-NEXT: .cfi_def_cfa 4, 3
; CALL-NEXT: .cfi_restore 6
; CALL-NEXT: ret
; CALL: .cfi_restore_state
; CALL: ld hl, (ix - 3)
; CALL-NEXT: ld sp, ix
; CALL-NEXT: .cfi_def_cfa 4, 6
; CALL-NEXT: pop ix
; CALL-NEXT: .cfi_def_cfa 4, 3
; CALL-NEXT: .cfi_restore 6
; CALL-NEXT: ret

declare void @sink(i24, i24)
define void @sp_calls() nounwind uwtable "frame-pointer"="none" {
  call void @sink(i24 1, i24 2)
  ret void
}
define i24 @multiple_returns(i24 %x) nounwind uwtable "frame-pointer"="all" {
  %local = alloca i24, align 1
  store volatile i24 %x, ptr %local, align 1
  %test = icmp eq i24 %x, 0
  br i1 %test, label %left, label %right
left:
  call void asm sideeffect "; left", ""()
  ret i24 1
right:
  call void asm sideeffect "; right", ""()
  %value = load volatile i24, ptr %local, align 1
  ret i24 %value
}
