; RUN: llc -mtriple=ez80 < %s | FileCheck %s

define internal i8 @"private$byte"() noinline {
  ret i8 7
}

define i8 @public_byte() {
  %r = call i8 @"private$byte"()
  ret i8 %r
}
