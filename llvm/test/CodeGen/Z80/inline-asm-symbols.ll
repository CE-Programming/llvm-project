; NOTE: Do not autogenerate
; RUN: llc -mtriple=z80 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=ez80-code16 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=ez80 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=z80 -stop-after=irtranslator -verify-machineinstrs < %s | FileCheck %s --check-prefix=MIR
; RUN: llc -mtriple=ez80-code16 -stop-after=irtranslator -verify-machineinstrs < %s | FileCheck %s --check-prefix=MIR
; RUN: llc -mtriple=ez80 -stop-after=irtranslator -verify-machineinstrs < %s | FileCheck %s --check-prefix=MIR
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
; CHECK: ; 37
; CHECK: ret
define void @aliases() {
  call void asm sideeffect "${0:c} = ${1:c}", "X,X"(ptr @alias, ptr @target)
  call void asm sideeffect "_data_alias = ${0:c}", "i"(ptr @data)
  call void asm sideeffect "; ${0:c}", "X"(i24 37)
  ret void
}

; CHECK-LABEL: barrier:
; CHECK: ;APP
; CHECK: ;NO_APP
; CHECK: ret
; MIR-LABEL: name: barrier
; MIR: %[[IN:[0-9]+]]:_(p0) = G_LOAD
; MIR: %[[INREG:[0-9]+]]:g{{(16|24)}} = COPY %[[IN]](p0)
; MIR: INLINEASM &"", {{.*}}def %[[OUT:[0-9]+]], {{.*}}reguse tiedto:$0{{.*}}%[[INREG]](tied-def 3)
; MIR: %[[PTR:[0-9]+]]:_(p0) = COPY %[[OUT]]
; MIR: $[[RETREG:[a-z]+]] = COPY %[[PTR]](p0)
; MIR: RET{{(16|24)}} {{.*}}implicit $[[RETREG]]{{$}}
define ptr @barrier(ptr %p) {
  %q = call ptr asm sideeffect "", "=r,0"(ptr %p)
  ret ptr %q
}
