; RUN: llc -mtriple=ez80 < %s

declare i24 @llvm.abs.i24(i24, i1)
define i24 @abs.i24(i24) {
  call i24 @llvm.abs.i24(i24 %0, i1 false)
  ret i24 %2
}

declare i24 @llvm.smax.i24(i24, i24)
define i24 @smax.i24(i24, i24) {
  call i24 @llvm.smax.i24(i24 %0, i24 %1)
  ret i24 %3
}

declare i24 @llvm.smin.i24(i24, i24)
define i24 @smin.i24(i24, i24) {
  call i24 @llvm.smin.i24(i24 %0, i24 %1)
  ret i24 %3
}

declare i24 @llvm.umax.i24(i24, i24)
define i24 @umax.i24(i24, i24) {
  call i24 @llvm.umax.i24(i24 %0, i24 %1)
  ret i24 %3
}

declare i24 @llvm.umin.i24(i24, i24)
define i24 @umin.i24(i24, i24) {
  call i24 @llvm.umin.i24(i24 %0, i24 %1)
  ret i24 %3
}

declare void @llvm.memcpy.p0i8.p0i8.i24(i8*, i8*, i24, i1)
define void @memcpy.p0i8.p0i8.i24(i8*, i8*, i24) {
  call void @llvm.memcpy.p0i8.p0i8.i24(i8* %0, i8* %1, i24 %2, i1 false)
  ret void
}

declare void @llvm.memcpy.inline.p0i8.p0i8.i24(i8*, i8*, i24, i1)
define void @memcpy.inline.p0i8.p0i8.i24(i8*, i8*) {
  call void @llvm.memcpy.inline.p0i8.p0i8.i24(i8* %0, i8* %1, i24 32, i1 false)
  ret void
}

declare void @llvm.memmove.p0i8.p0i8.i24(i8*, i8*, i24, i1)
define void @memmove.p0i8.p0i8.i24(i8*, i8*, i24) {
  call void @llvm.memmove.p0i8.p0i8.i24(i8* %0, i8* %1, i24 %2, i1 false)
  ret void
}

declare void @llvm.memset.p0i8.i24(i8*, i8, i24, i1)
define void @memset.p0i8.i24(i8*, i8, i24) {
  call void @llvm.memset.p0i8.i24(i8* %0, i8 %1, i24 %2, i1 false)
  ret void
}

declare i24 @llvm.bitreverse.i24(i24)
define i24 @bitreverse.i24(i24) {
  call i24 @llvm.bitreverse.i24(i24 %0)
  ret i24 %2
}

declare i24 @llvm.ctpop.i24(i24)
define i24 @ctpop.i24(i24) {
  call i24 @llvm.ctpop.i24(i24 %0)
  ret i24 %2
}

declare i24 @llvm.ctlz.i24(i24)
define i24 @ctlz.i24(i24) {
  call i24 @llvm.ctlz.i24(i24 %0)
  ret i24 %2
}

declare i24 @llvm.cttz.i24(i24)
define i24 @cttz.i24(i24) {
  call i24 @llvm.cttz.i24(i24 %0)
  ret i24 %2
}

declare i24 @llvm.fshl.i24(i24, i24, i24)
define i24 @fshl.i24(i24, i24, i24) {
  call i24 @llvm.fshl.i24(i24 %0, i24 %1, i24 %2)
  ret i24 %4
}

declare i24 @llvm.fshr.i24(i24, i24, i24)
define i24 @fshr.i24(i24, i24, i24) {
  call i24 @llvm.fshr.i24(i24 %0, i24 %1, i24 %2)
  ret i24 %4
}

declare {i24, i1} @llvm.sadd.with.overflow.i24(i24, i24)
define i1 @sadd.with.overflow.i24(i24, i24) {
  call {i24, i1} @llvm.sadd.with.overflow.i24(i24 %0, i24 %1)
  extractvalue {i24, i1} %3, 1
  ret i1 %4
}

declare {i24, i1} @llvm.uadd.with.overflow.i24(i24, i24)
define i1 @uadd.with.overflow.i24(i24, i24) {
  call {i24, i1} @llvm.uadd.with.overflow.i24(i24 %0, i24 %1)
  extractvalue {i24, i1} %3, 1
  ret i1 %4
}

declare {i24, i1} @llvm.ssub.with.overflow.i24(i24, i24)
define i1 @ssub.with.overflow.i24(i24, i24) {
  call {i24, i1} @llvm.ssub.with.overflow.i24(i24 %0, i24 %1)
  extractvalue {i24, i1} %3, 1
  ret i1 %4
}

declare {i24, i1} @llvm.usub.with.overflow.i24(i24, i24)
define i1 @usub.with.overflow.i24(i24, i24) {
  call {i24, i1} @llvm.usub.with.overflow.i24(i24 %0, i24 %1)
  extractvalue {i24, i1} %3, 1
  ret i1 %4
}

declare i24 @llvm.sadd.sat.i24(i24, i24)
define i24 @sadd.sat.i24(i24, i24) {
  call i24 @llvm.sadd.sat.i24(i24 %0, i24 %1)
  ret i24 %3
}

declare i24 @llvm.uadd.sat.i24(i24, i24)
define i24 @uadd.sat.i24(i24, i24) {
  call i24 @llvm.uadd.sat.i24(i24 %0, i24 %1)
  ret i24 %3
}

declare i24 @llvm.ssub.sat.i24(i24, i24)
define i24 @ssub.sat.i24(i24, i24) {
  call i24 @llvm.ssub.sat.i24(i24 %0, i24 %1)
  ret i24 %3
}

declare i24 @llvm.usub.sat.i24(i24, i24)
define i24 @usub.sat.i24(i24, i24) {
  call i24 @llvm.usub.sat.i24(i24 %0, i24 %1)
  ret i24 %3
}

declare i24 @llvm.smul.sat.i24(i24, i24)
define i24 @smul.sat.i24(i24, i24) {
  call i24 @llvm.smul.sat.i24(i24 %0, i24 %1)
  ret i24 %3
}

declare i24 @llvm.umul.sat.i24(i24, i24)
define i24 @umul.sat.i24(i24, i24) {
  call i24 @llvm.umul.sat.i24(i24 %0, i24 %1)
  ret i24 %3
}
