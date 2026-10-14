; NOTE: Do not autogenerate
; RUN: opt -mtriple=ez80 -passes='cgscc(inline)' -inline-threshold=0 -S %s | FileCheck %s
; RUN: opt -mtriple=ez80 -passes='cgscc(inline)' -inline-threshold=0 -z80-target-cost-model=false -S %s | FileCheck %s --check-prefix=OFF

; C argument and return-address stack traffic is expensive relative to ADD.
; The speed policy can inline this leaf at threshold zero; size callers retain
; the generic penalty. This tests the policy without globally raising thresholds.

define void @leaf(ptr %p, i24 %x) {
entry:
  %a = add i24 %x, 5
  store i24 %a, ptr %p, align 1
  %p1 = getelementptr i24, ptr %p, i24 1
  %b = add i24 %x, 6
  store i24 %b, ptr %p1, align 1
  %p2 = getelementptr i24, ptr %p, i24 2
  %c = add i24 %x, 7
  store i24 %c, ptr %p2, align 1
  %p3 = getelementptr i24, ptr %p, i24 3
  %d = add i24 %x, 8
  store i24 %d, ptr %p3, align 1
  %p4 = getelementptr i24, ptr %p, i24 4
  %e = add i24 %x, 9
  store i24 %e, ptr %p4, align 1
  ret void
}

; CHECK-LABEL: define void @speed(
; CHECK-NOT: call void @leaf
; CHECK: store i24
; CHECK: ret void
; OFF-LABEL: define void @speed(
; OFF: call void @leaf
define void @speed(ptr %p, i24 %x) {
  call void @leaf(ptr %p, i24 %x)
  ret void
}

; CHECK-LABEL: define void @size(
; CHECK: call void @leaf
; OFF-LABEL: define void @size(
; OFF: call void @leaf
define void @size(ptr %p, i24 %x) minsize optsize {
  call void @leaf(ptr %p, i24 %x)
  ret void
}

define fastcc void @fast_leaf(ptr %p, i24 %x) {
entry:
  %a = add i24 %x, 5
  store i24 %a, ptr %p, align 1
  %p1 = getelementptr i24, ptr %p, i24 1
  %b = add i24 %x, 6
  store i24 %b, ptr %p1, align 1
  %p2 = getelementptr i24, ptr %p, i24 2
  %c = add i24 %x, 7
  store i24 %c, ptr %p2, align 1
  %p3 = getelementptr i24, ptr %p, i24 3
  %d = add i24 %x, 8
  store i24 %d, ptr %p3, align 1
  %p4 = getelementptr i24, ptr %p, i24 4
  %e = add i24 %x, 9
  store i24 %e, ptr %p4, align 1
  ret void
}


; FastCC retains the public stack argument ABI on this target.
; CHECK-LABEL: define void @fast_speed(
; CHECK-NOT: call fastcc void @fast_leaf
; CHECK: store i24
; CHECK: ret void
; OFF-LABEL: define void @fast_speed(
; OFF: call fastcc void @fast_leaf
define void @fast_speed(ptr %p, i24 %x) {
  call fastcc void @fast_leaf(ptr %p, i24 %x)
  ret void
}

; CHECK-LABEL: define void @fast_size(
; CHECK: call fastcc void @fast_leaf
; OFF-LABEL: define void @fast_size(
; OFF: call fastcc void @fast_leaf
define void @fast_size(ptr %p, i24 %x) minsize optsize {
  call fastcc void @fast_leaf(ptr %p, i24 %x)
  ret void
}
