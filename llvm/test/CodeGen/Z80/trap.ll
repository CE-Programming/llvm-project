; RUN: llc -mtriple=z80 < %s | FileCheck %s --check-prefix=Z80
; RUN: llc -mtriple=ez80 < %s | FileCheck %s --check-prefix=EZ80
; RUN: llc -mtriple=ez80 --z80-trap-rst-vector=56 < %s | FileCheck %s --check-prefix=EZ80-RST56

declare void @llvm.trap()
declare void @llvm.debugtrap()
declare void @llvm.ubsantrap(i8 immarg)

define void @trap_now() {
; Z80-LABEL: _trap_now:
; Z80:       rst     0
; EZ80-LABEL: _trap_now:
; EZ80:       rst     0
; EZ80-RST56-LABEL: _trap_now:
; EZ80-RST56:       rst     56
entry:
  call void @llvm.trap()
  unreachable
}

define void @debugtrap_now() {
; Z80-LABEL: _debugtrap_now:
; Z80:       rst     0
; EZ80-LABEL: _debugtrap_now:
; EZ80:       rst     0
; EZ80-RST56-LABEL: _debugtrap_now:
; EZ80-RST56:       rst     56
entry:
  call void @llvm.debugtrap()
  unreachable
}

define void @ubsantrap_now() {
; Z80-LABEL: _ubsantrap_now:
; Z80:       rst     0
; EZ80-LABEL: _ubsantrap_now:
; EZ80:       rst     0
; EZ80-RST56-LABEL: _ubsantrap_now:
; EZ80-RST56:       rst     56
entry:
  call void @llvm.ubsantrap(i8 7)
  unreachable
}
