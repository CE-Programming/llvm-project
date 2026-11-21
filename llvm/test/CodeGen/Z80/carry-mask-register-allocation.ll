; NOTE: Do not autogenerate
; RUN: llc -mtriple=ez80 -O2 -verify-machineinstrs < %s | FileCheck %s --implicit-check-not="sbc a, {{(b|c|d|e|h|l|ixh|ixl|iyh|iyl)}}"
;
; Materializing an i8 carry mask requires SBC A, A. Keep the explicit source
; fixed to A even when another value is live in A, so register allocation
; preserves that value instead of changing the source to another byte register.

@a = external global i24, align 1
@b = external global i24, align 1
@c = external global i24, align 1

declare i8 @g()

define i8 @carry_mask_register_allocation() {
; CHECK-LABEL: _carry_mask_register_allocation:
; CHECK:       ld b, a
; CHECK-NEXT:  sbc a, a
; CHECK:       sbc a, a
; CHECK:       ret
entry:
  %r = call i8 @g()
  %c.value = load i24, ptr @c, align 1
  %c.zero = icmp eq i24 %c.value, 0
  br i1 %c.zero, label %compare, label %exit

compare:
  %a.value = load i24, ptr @a, align 1
  %a.lt = icmp ult i24 %a.value, 9
  %b.value = load i24, ptr @b, align 1
  %b.lt = icmp ult i24 %b.value, 9
  %both.lt = and i1 %a.lt, %b.lt
  %r.wide = zext i8 %r to i24
  %class = select i1 %both.lt, i24 1, i24 2
  %equal = icmp eq i24 %class, %r.wide
  %result = zext i1 %equal to i8
  br label %exit

exit:
  %return = phi i8 [ %result, %compare ], [ %r, %entry ]
  ret i8 %return
}
