; RUN: llc -mtriple=ez80-code16 < %s | FileCheck %s --check-prefixes=EZ80-CODE16

declare i8 addrspace(1)* @llvm.ptrmask.p1i8.i24(i8 addrspace(1)*, i24)

define i8 addrspace(1)* @ptrmask.p1i8.i24(i8 addrspace(1)* %0, i24 %1) {
; EZ80-CODE16-LABEL: ptrmask.p1i8.i24:
; EZ80-CODE16:       ; %bb.0:
; EZ80-CODE16-NEXT:    ld iy, 0
; EZ80-CODE16-NEXT:    add iy, sp
; EZ80-CODE16-NEXT:    ld hl, (iy + 2)
; EZ80-CODE16-NEXT:    ld bc, (iy + 6)
; EZ80-CODE16-NEXT:    ld e, (iy + 4)
; EZ80-CODE16-NEXT:    ld a, (iy + 8)
; EZ80-CODE16-NEXT:    call __iand
; EZ80-CODE16-NEXT:    ret
  %3 = call i8 addrspace(1)* @llvm.ptrmask.p1i8.i24(i8 addrspace(1)* %0, i24 %1)
  ret i8 addrspace(1)* %3
}
