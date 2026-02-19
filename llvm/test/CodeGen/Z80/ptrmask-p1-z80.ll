; RUN: llc -mtriple=z80 < %s | FileCheck %s --check-prefixes=Z80

declare i8 addrspace(1)* @llvm.ptrmask.p1i8.i16(i8 addrspace(1)*, i16)

define i8 addrspace(1)* @ptrmask.p1i8.i16(i8 addrspace(1)* %0, i16 %1) {
; Z80-LABEL: ptrmask.p1i8.i16:
; Z80:       ; %bb.0:
; Z80-NEXT:    ld iy, 0
; Z80-NEXT:    add iy, sp
; Z80-NEXT:    ld l, (iy + 2)
; Z80-NEXT:    ld h, (iy + 3)
; Z80-NEXT:    ld c, (iy + 4)
; Z80-NEXT:    ld b, (iy + 5)
; Z80-NEXT:    call __sand
; Z80-NEXT:    ret
  %3 = call i8 addrspace(1)* @llvm.ptrmask.p1i8.i16(i8 addrspace(1)* %0, i16 %1)
  ret i8 addrspace(1)* %3
}
