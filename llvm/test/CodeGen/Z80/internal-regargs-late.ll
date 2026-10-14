; NOTE: Do not autogenerate
; RUN: split-file %s %t
; RUN: llc -mtriple=ez80 -O2 %t/marked.ll -o - | FileCheck %s --check-prefix=REG
; RUN: llc -mtriple=ez80 -O2 -z80-internal-regargs=false %t/marked.ll -o - | FileCheck %s --check-prefix=STACK
; RUN: llc -mtriple=ez80 -O2 %t/plain.ll -o - | FileCheck %s --check-prefix=STACK
; RUN: llc -mtriple=ez80 -O2 -z80-internal-regargs-on-merged-ir %t/plain.ll -o - | FileCheck %s --check-prefix=REG
; RUN: llc -mtriple=ez80 -O0 -z80-internal-regargs-on-merged-ir %t/plain.ll -o - | FileCheck %s --check-prefix=STACK

; OptimizerLastEP precedes MergeFunctions for ordinary/ThinLTO pipelines. The
; late codegen module pass must see private merged bodies created afterwards,
; while retaining C for public wrappers. Marked modules or explicit merged-IR
; opt-in activate it. Ordinary unmarked modules and O0 remain unchanged.

; REG-LABEL: _caller:
; REG: ld hl, 11
; REG: ld de, 22
; REG-NOT: push
; REG: jp _sum
; REG-LABEL: _sum:
; REG-NOT: add iy, sp
; REG: add hl, de
; REG: ret
; STACK-LABEL: _sum:
; STACK: add iy, sp
; STACK: add hl, de
; STACK: ret

;--- marked.ll
!z80.internal.regargs.postlink = !{}
define i24 @caller() {
 %r = tail call i24 @sum(i24 11,i24 22)
 ret i24 %r
}
define private i24 @sum(i24 %a,i24 %b) {
 %r = add i24 %a,%b
 ret i24 %r
}

;--- plain.ll
define i24 @caller() {
 %r = tail call i24 @sum(i24 11,i24 22)
 ret i24 %r
}
define private i24 @sum(i24 %a,i24 %b) {
 %r = add i24 %a,%b
 ret i24 %r
}
