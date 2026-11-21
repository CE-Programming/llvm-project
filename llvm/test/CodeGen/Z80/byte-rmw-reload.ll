; NOTE: Do not autogenerate
; RUN: llc -mtriple=ez80 -O0 -stop-after=instruction-select -verify-machineinstrs -o - %s | FileCheck %s
; RUN: llc -mtriple=ez80 -O0 -verify-machineinstrs -o /dev/null %s
; RUN: llc -mtriple=z80 -O0 -verify-machineinstrs -o /dev/null %s

declare void @zero()
declare void @nonzero()

define void @test_updated_byte(ptr %p) {
; CHECK-LABEL: name: test_updated_byte
; CHECK:       INC8p {{.*}}(store (s8) into %ir.p), (load (s8) from %ir.p)
; CHECK-NEXT:  {{.*}} = LD8gp {{.*}} :: (load (s8) from %ir.p){{$}}
  %old = load i8, ptr %p
  %updated = add i8 %old, 1
  store i8 %updated, ptr %p
  %iszero = icmp eq i8 %updated, 0
  br i1 %iszero, label %one, label %two

one:
  tail call void @zero()
  ret void

two:
  tail call void @nonzero()
  ret void
}
