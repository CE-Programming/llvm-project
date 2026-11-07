; NOTE: Do not autogenerate
; RUN: opt -mtriple=ez80 -passes=z80-internal-regargs -verify-each -S %s | FileCheck %s
; RUN: opt -mtriple=ez80 -passes=z80-internal-regargs -z80-internal-regargs=false -verify-each -S %s | FileCheck %s --check-prefix=OFF
;
; Every use must be an exact direct call and every ABI-affecting attribute must
; be ordinary. Already optimized fastcc functions are eligible too.
target triple = "ez80"

@escaped = global ptr @address_taken
@aliased = alias i24 (i24), ptr @alias_target

declare void @consume(i24)
declare void @consume_ptr(ptr)
declare i24 @external(i24)
declare i24 @tail_target(i24)
declare i32 @personality(...)

; CHECK-LABEL: define internal cc136 i24 @sum(
; OFF-LABEL: define internal i24 @sum(
define internal i24 @sum(i24 %a, i24 %b, i8 %c) noinline {
  %z = zext i8 %c to i24
  %s = add i24 %a, %b
  %r = add i24 %s, %z
  ret i24 %r
}

; CHECK-LABEL: define internal cc136 i8 @read_byte(
define internal i8 @read_byte(ptr %p) noinline {
  %v = load i8, ptr %p
  ret i8 %v
}

; CHECK-LABEL: define internal cc136 i24 @previous_fast(
; OFF-LABEL: define internal fastcc i24 @previous_fast(
define internal fastcc i24 @previous_fast(i24 %x) noinline {
  ret i24 %x
}

; CHECK-LABEL: define internal cc136 i24 @recursive(
; CHECK: call cc136 i24 @recursive(
define internal i24 @recursive(i24 %x) noinline {
  %z = icmp eq i24 %x, 0
  br i1 %z, label %done, label %recur
recur:
  %n = sub i24 %x, 1
  %v = call i24 @recursive(i24 %n)
  ret i24 %v
done:
  ret i24 0
}

; CHECK-LABEL: define i24 @entry(
; CHECK: call cc136 i24 @sum(
; CHECK: call cc136 i8 @read_byte(
; CHECK: call cc136 i24 @previous_fast(
; CHECK: call cc136 i24 @recursive(
; CHECK: call i24 @external(
; OFF-LABEL: define i24 @entry(
; OFF-NOT: cc136
; OFF: ret i24
define i24 @entry(i24 %x, ptr %p, i8 %c) {
  %a = call i24 @sum(i24 %x, i24 3, i8 %c)
  %b = call i8 @read_byte(ptr %p)
  %d = call fastcc i24 @previous_fast(i24 %a)
  %e = call i24 @recursive(i24 %d)
  %f = call i24 @external(i24 %e)
  ret i24 %f
}

; CHECK-LABEL: define i24 @public(
define i24 @public(i24 %x) { ret i24 %x }
; CHECK-LABEL: define linkonce_odr i24 @linkonce(
define linkonce_odr i24 @linkonce(i24 %x) { ret i24 %x }
; CHECK-LABEL: define internal i24 @address_taken(
define internal i24 @address_taken(i24 %x) { ret i24 %x }
; CHECK-LABEL: define internal i24 @alias_target(
define internal i24 @alias_target(i24 %x) { ret i24 %x }
; CHECK-LABEL: define internal i24 @callback(
define internal i24 @callback(i24 %x) { ret i24 %x }
; CHECK-LABEL: define internal i24 @varargs(
define internal i24 @varargs(i24 %x, ...) { ret i24 %x }
; CHECK-LABEL: define internal i32 @wide(
define internal i32 @wide(i32 %x) { ret i32 %x }
; CHECK-LABEL: define internal cc136 i24 @four(
define internal i24 @four(i24 %a, i24 %b, i24 %c, i24 %d) { ret i24 %a }
; CHECK-LABEL: define internal i24 @noargs(
define internal i24 @noargs() { ret i24 0 }
; CHECK-LABEL: define internal i24 @byval(
define internal i24 @byval(ptr byval(i24) %p) { ret i24 0 }
; CHECK-LABEL: define internal i24 @inreg(
define internal i24 @inreg(i24 inreg %x) { ret i24 %x }
; CHECK-LABEL: define internal i24 @call_inreg(
define internal i24 @call_inreg(i24 %x) { ret i24 %x }
; CHECK-LABEL: define internal i24 @interrupt(
define internal i24 @interrupt(i24 %x) "interrupt"="Generic" { ret i24 %x }
; CHECK-LABEL: define internal i24 @none(
define internal i24 @none(i24 %x) noinline optnone { ret i24 %x }
; CHECK-LABEL: define internal i24 @tail_callee(
define internal i24 @tail_callee(i24 %x) { ret i24 %x }
; CHECK-LABEL: define internal i24 @tail_caller(
define internal i24 @tail_caller(i24 %x) {
  %r = musttail call i24 @tail_target(i24 %x)
  ret i24 %r
}
; CHECK-LABEL: define internal i24 @invoked(
define internal i24 @invoked(i24 %x) { ret i24 %x }
; CHECK-LABEL: define internal i24 @bundled(
define internal i24 @bundled(i24 %x) { ret i24 %x }
; CHECK-LABEL: define internal i24 @with_as(
define internal i24 @with_as(ptr addrspace(1) %p) { ret i24 0 }
; CHECK-LABEL: define internal void @self_pointer(
define internal void @self_pointer(ptr %callback) {
  call void @self_pointer(ptr @self_pointer)
  ret void
}
; CHECK-LABEL: define internal i24 @cross_mode(
define internal i24 @cross_mode(i24 %x) { ret i24 %x }

; CHECK-LABEL: define i24 @musttail_entry(
; CHECK: musttail call i24 @tail_callee(
define i24 @musttail_entry(i24 %x) {
  %r = musttail call i24 @tail_callee(i24 %x)
  ret i24 %r
}

define void @negative_calls(i24 %x, i32 %w, ptr %p, ptr addrspace(1) %other) personality ptr @personality {
  call i24 @public(i24 %x)
  call i24 @linkonce(i24 %x)
  call i24 @address_taken(i24 %x)
  call i24 @alias_target(i24 %x)
  call i24 @callback(i24 %x)
  call void @consume_ptr(ptr @callback)
  call i24 (i24, ...) @varargs(i24 %x, i24 %x)
  call i32 @wide(i32 %w)
  call i24 @four(i24 %x, i24 %x, i24 %x, i24 %x)
  call i24 @noargs()
  call i24 @byval(ptr byval(i24) %p)
  call i24 @inreg(i24 inreg %x)
  call i24 @call_inreg(i24 inreg %x)
  call i24 @interrupt(i24 %x)
  call i24 @none(i24 %x)
  call i24 @tail_caller(i24 %x)
  call i24 @bundled(i24 %x) [ "deopt"(i24 %x) ]
  call i24 @with_as(ptr addrspace(1) %other)
  invoke i24 @invoked(i24 %x) to label %done unwind label %catch
catch:
  %exc = landingpad { ptr, i24 } cleanup
  ret void
done:
  ret void
}

define i24 @short_mode_caller(i24 %x) "target-features"="-24bit-mode" {
  %r = call i24 @cross_mode(i24 %x)
  ret i24 %r
}
