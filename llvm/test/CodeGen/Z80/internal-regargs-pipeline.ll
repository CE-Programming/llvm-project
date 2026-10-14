; NOTE: Do not autogenerate
; RUN: opt -mtriple=ez80 -passes='lto<Oz>' -verify-each -S %s | FileCheck %s --check-prefix=REG
; RUN: opt -mtriple=ez80 -passes='thinlto<Oz>' -verify-each -S %s | FileCheck %s --check-prefix=REG
; RUN: opt -mtriple=ez80 -passes='default<Oz>' -z80-internal-regargs-on-merged-ir -verify-each -S %s | FileCheck %s --check-prefix=REG
; RUN: opt -mtriple=ez80 -passes='default<Oz>' -verify-each -S %s | FileCheck %s --check-prefix=STACK
; RUN: opt -mtriple=ez80 -passes='lto-pre-link<Oz>' -verify-each -S %s | FileCheck %s --check-prefix=STACK
; RUN: opt -mtriple=ez80 -passes='thinlto-pre-link<Oz>' -verify-each -S %s | FileCheck %s --check-prefix=STACK
; RUN: opt -mtriple=ez80 -passes='lto<Oz>' -z80-internal-regargs=false -verify-each -S %s | FileCheck %s --check-prefix=STACK
; RUN: opt -mtriple=ez80 -passes='default<Oz>' -z80-internal-regargs-on-merged-ir -z80-internal-regargs=false -verify-each -S %s | FileCheck %s --check-prefix=STACK
; RUN: opt -mtriple=ez80 -passes='lto<O0>' -verify-each -S %s | FileCheck %s --check-prefix=STACK
;
; Apply only after LTO, or explicitly to already merged IR. GlobalOpt's earlier
; promotion to fastcc must be handled without changing its ordinary stack ABI.
target triple = "ez80"

declare void @consume(i24)

; REG-LABEL: define internal cc136 i24 @sum(
; STACK-LABEL: define internal {{(fastcc )?}}i24 @sum(
define internal i24 @sum(i24 %x, i24 %y) noinline {
  %r = add i24 %x, %y
  call void @consume(i24 %r)
  ret i24 %r
}

; REG-LABEL: define{{.*}} i24 @caller(
; REG: call cc136 i24 @sum(
; STACK-LABEL: define{{.*}} i24 @caller(
; STACK-NOT: cc136
; STACK: ret i24
define i24 @caller(i24 %a, i24 %b) {
  %r = call i24 @sum(i24 %a, i24 %b)
  ret i24 %r
}
