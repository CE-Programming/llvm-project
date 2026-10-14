; NOTE: Do not autogenerate
; RUN: opt -mtriple=ez80 -passes=z80-internal-regargs -verify-each -S %s | FileCheck %s
; RUN: opt -mtriple=ez80 -passes=z80-internal-regargs -z80-internal-regargs=false -verify-each -S %s | FileCheck %s --check-prefix=OFF
; RUN: opt -mtriple=ez80 -passes=z80-internal-regargs -S %s | llc -mtriple=ez80 -stop-after=irtranslator -verify-machineinstrs -o - | FileCheck %s --check-prefix=MIR

; First three scalar arguments are passed in registers; later scalar arguments
; keep ordinary native-width stack slots. The return convention is unchanged,
; so return types wider than native words and sret pointers are also eligible.

%triple = type {i24,i24,i24}

; CHECK-LABEL: define internal cc136 i24 @six(
; OFF-LABEL: define internal i24 @six(
; MIR-LABEL: name: six
; MIR: fixedStack:
; MIR: offset: 6, size: 2
; MIR: offset: 3, size: 1
; MIR: offset: 0, size: 3
; MIR: liveins: $ubc, $ude, $uhl
define internal i24 @six(i24 %a,i24 %b,i24 %c,i24 %d,i8 %e,i16 %f) noinline {
 %small = zext i8 %e to i24
 %word = zext i16 %f to i24
 %x = add i24 %a,%b
 %y = add i24 %c,%d
 %z = add i24 %small,%word
 %xy = add i24 %x,%y
 %r = add i24 %xy,%z
 ret i24 %r
}

; CHECK-LABEL: define internal cc136 i64 @wide_return(
; OFF-LABEL: define internal i64 @wide_return(
define internal i64 @wide_return(i24 %a) noinline {
 %r = zext i24 %a to i64
 ret i64 %r
}

; CHECK-LABEL: define internal cc136 void @sret_first(
; CHECK-SAME: sret(%triple)
; OFF-LABEL: define internal void @sret_first(
; MIR-LABEL: name: sret_first
; MIR: liveins: $ubc, $ude, $uhl
; MIR: COPY $uhl
; MIR: RET24{{.*}}implicit $uhl
define internal void @sret_first(ptr sret(%triple) %dst,i24 %a,i24 %b,i24 %c) noinline {
 store i24 %a,ptr %dst,align 1
 %p1 = getelementptr %triple,ptr %dst,i24 0,i32 1
 %p2 = getelementptr %triple,ptr %dst,i24 0,i32 2
 store i24 %b,ptr %p1,align 1
 store i24 %c,ptr %p2,align 1
 ret void
}

; sret's location does not change its value or return pointer tracking.
; CHECK-LABEL: define internal cc136 void @sret_second(
; CHECK-SAME: sret(%triple)
; OFF-LABEL: define internal void @sret_second(
; MIR-LABEL: name: sret_second
; MIR: liveins: $ubc, $ude, $uhl
; MIR: RET24{{.*}}implicit $uhl
define internal void @sret_second(i24 %a,ptr sret(%triple) %dst,i24 %b,i24 %c) noinline {
 store i24 %a,ptr %dst,align 1
 %p1 = getelementptr %triple,ptr %dst,i24 0,i32 1
 %p2 = getelementptr %triple,ptr %dst,i24 0,i32 2
 store i24 %b,ptr %p1,align 1
 store i24 %c,ptr %p2,align 1
 ret void
}

; Byval and all other special ABI flags retain their guards.
; CHECK-LABEL: define internal void @byval(
define internal void @byval(ptr byval(%triple) %dst) noinline {
 ret void
}

; CHECK-LABEL: define i64 @caller(
; CHECK: call cc136 i24 @six(
; CHECK: call cc136 void @sret_first(
; CHECK: call cc136 void @sret_second(
; CHECK: call cc136 i64 @wide_return(
; OFF-LABEL: define i64 @caller(
; OFF-NOT: cc136
; OFF: ret i64
define i64 @caller(i24 %a,ptr %dst) {
 %x = call i24 @six(i24 %a,i24 1,i24 2,i24 3,i8 4,i16 5)
 call void @sret_first(ptr sret(%triple) %dst,i24 %a,i24 1,i24 2)
 call void @sret_second(i24 %a,ptr sret(%triple) %dst,i24 1,i24 2)
 call void @byval(ptr byval(%triple) %dst)
 %r = call i64 @wide_return(i24 %x)
 ret i64 %r
}
