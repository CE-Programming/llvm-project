; NOTE: Do not autogenerate
; RUN: llc -mtriple=z80 -stop-after=irtranslator -verify-machineinstrs -o - %s | FileCheck %s
; RUN: llc -mtriple=ez80 -stop-after=irtranslator -verify-machineinstrs -o - %s | FileCheck %s
;
; A byval argument is the callee's writable copy. Its address can escape to a
; mutator, so a load before the call must not be rematerialized from that slot
; after the call. Ordinary incoming scalar argument slots remain immutable.

%Iterator = type { ptr }

declare void @mutate(ptr)

; CHECK-LABEL: name: mutable_byval
; CHECK: fixedStack:
; CHECK: size: {{[23]}}, alignment: 1
; CHECK: isImmutable: false
define ptr @mutable_byval(ptr byval(%Iterator) align 1 %position) {
  %original = load ptr, ptr %position, align 1
  call void @mutate(ptr %position)
  ret ptr %original
}

; CHECK-LABEL: name: scalar_argument
; CHECK: fixedStack:
; CHECK: size: {{[23]}}, alignment: 1
; CHECK: isImmutable: true
define ptr @scalar_argument(ptr %value) {
  ret ptr %value
}
