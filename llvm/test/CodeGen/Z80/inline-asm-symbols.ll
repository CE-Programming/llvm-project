; NOTE: Do not autogenerate
; RUN: llc -mtriple=z80 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=ez80-code16 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=ez80 -verify-machineinstrs < %s | FileCheck %s
;
; Catch-all X operands must preserve symbols, not materialize their addresses in
; registers. libc++ uses c modifiers for assembler aliases and a register
; barrier when comparing the default implementation with an interposed function.

declare void @target()
declare void @alias()
@data = external global i8

; CHECK-LABEL: aliases:
; CHECK: _alias = _target
; CHECK: _data_alias = _data
; CHECK: 37
; CHECK: ret
define void @aliases() {
  call void asm sideeffect "${0:c} = ${1:c}", "X,X"(ptr @alias, ptr @target)
  call void asm sideeffect "_data_alias = ${0:c}", "i"(ptr @data)
  call void asm sideeffect "; ${0:c}", "X"(i24 37)
  ret void
}

; CHECK-LABEL: barrier:
; CHECK: ret
define ptr @barrier(ptr %p) {
  %q = call ptr asm sideeffect "", "=r,0"(ptr %p)
  ret ptr %q
}
