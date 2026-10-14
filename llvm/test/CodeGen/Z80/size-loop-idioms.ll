; NOTE: Do not autogenerate
; RUN: opt -mtriple=ez80 -passes="default<Oz>" -verify-each -S %s | FileCheck %s --check-prefix=OPT
; RUN: opt -mtriple=ez80 -passes="default<Oz>" -z80-size-loop-idioms=false -verify-each -S %s | FileCheck %s --check-prefix=OFF
; RUN: opt -mtriple=ez80 -passes="default<Os>" -verify-each -S %s | FileCheck %s --check-prefix=OPT
target triple = "ez80"

; OPT-LABEL: define{{.*}} @copy(
; OPT: call void @llvm.memcpy
; OFF-LABEL: define{{.*}} @copy(
; OFF-NOT: @llvm.mem
; OFF: store i8
define void @copy(ptr noalias %d, ptr noalias %s, i24 %n, i8 %v) minsize optsize {
entry:
  br label %header
header:
  %i = phi i24 [0, %entry], [%next, %body]
  %test = icmp ult i24 %i, %n
  br i1 %test, label %body, label %exit
body:
  %dp = getelementptr inbounds i8, ptr %d, i24 %i
  %sp = getelementptr inbounds i8, ptr %s, i24 %i
  %x = load i8, ptr %sp, align 1
  store i8 %x, ptr %dp, align 1
  %next = add i24 %i, 1
  br label %header
exit:
  ret void
}

; OPT-LABEL: define{{.*}} @fill(
; OPT-NOT: @llvm.mem
; OPT: store i8
; OFF-LABEL: define{{.*}} @fill(
; OFF-NOT: @llvm.mem
; OFF: store i8
define void @fill(ptr %d, ptr %s, i24 %n, i8 %v) minsize optsize {
entry:
  br label %header
header:
  %i = phi i24 [0, %entry], [%next, %body]
  %test = icmp ult i24 %i, %n
  br i1 %test, label %body, label %exit
body:
  %dp = getelementptr inbounds i8, ptr %d, i24 %i
  store i8 %v, ptr %dp, align 1
  %next = add i24 %i, 1
  br label %header
exit:
  ret void
}

; OPT-LABEL: define{{.*}} @may_alias(
; OPT-NOT: @llvm.mem
; OPT: store i8
; OFF-LABEL: define{{.*}} @may_alias(
; OFF-NOT: @llvm.mem
; OFF: store i8
define void @may_alias(ptr %d, ptr %s, i24 %n, i8 %v) minsize optsize {
entry:
  br label %header
header:
  %i = phi i24 [0, %entry], [%next, %body]
  %test = icmp ult i24 %i, %n
  br i1 %test, label %body, label %exit
body:
  %dp = getelementptr inbounds i8, ptr %d, i24 %i
  %sp = getelementptr inbounds i8, ptr %s, i24 %i
  %x = load i8, ptr %sp, align 1
  store i8 %x, ptr %dp, align 1
  %next = add i24 %i, 1
  br label %header
exit:
  ret void
}

; OPT-LABEL: define{{.*}} @volatile_copy(
; OPT-NOT: @llvm.mem
; OPT: store volatile i8
; OFF-LABEL: define{{.*}} @volatile_copy(
; OFF-NOT: @llvm.mem
; OFF: store volatile i8
define void @volatile_copy(ptr noalias %d, ptr noalias %s, i24 %n, i8 %v) minsize optsize {
entry:
  br label %header
header:
  %i = phi i24 [0, %entry], [%next, %body]
  %test = icmp ult i24 %i, %n
  br i1 %test, label %body, label %exit
body:
  %dp = getelementptr inbounds i8, ptr %d, i24 %i
  %sp = getelementptr inbounds i8, ptr %s, i24 %i
  %x = load volatile i8, ptr %sp, align 1
  store volatile i8 %x, ptr %dp, align 1
  %next = add i24 %i, 1
  br label %header
exit:
  ret void
}

; OPT-LABEL: define{{.*}} @transformed_bytes(
; OPT-NOT: @llvm.mem
; OPT: store i8
; OFF-LABEL: define{{.*}} @transformed_bytes(
; OFF-NOT: @llvm.mem
; OFF: store i8
define void @transformed_bytes(ptr noalias %d, ptr noalias %s, i24 %n, i8 %v) minsize optsize {
entry:
  br label %header
header:
  %i = phi i24 [0, %entry], [%next, %body]
  %test = icmp ult i24 %i, %n
  br i1 %test, label %body, label %exit
body:
  %dp = getelementptr inbounds i8, ptr %d, i24 %i
  %sp = getelementptr inbounds i8, ptr %s, i24 %i
  %x = load i8, ptr %sp, align 1
  %y = xor i8 %x, %v
  store i8 %y, ptr %dp, align 1
  %next = add i24 %i, 1
  br label %header
exit:
  ret void
}

; OPT-LABEL: define{{.*}} @strided_copy(
; OPT-NOT: @llvm.mem
; OPT: store i8
; OFF-LABEL: define{{.*}} @strided_copy(
; OFF-NOT: @llvm.mem
; OFF: store i8
define void @strided_copy(ptr noalias %d, ptr noalias %s, i24 %n, i8 %v) minsize optsize {
entry:
  br label %header
header:
  %i = phi i24 [0, %entry], [%next, %body]
  %test = icmp ult i24 %i, %n
  br i1 %test, label %body, label %exit
body:
  %j = shl i24 %i, 1
  %dp = getelementptr inbounds i8, ptr %d, i24 %j
  %sp = getelementptr inbounds i8, ptr %s, i24 %j
  %x = load i8, ptr %sp, align 1
  store i8 %x, ptr %dp, align 1
  %next = add i24 %i, 1
  br label %header
exit:
  ret void
}
