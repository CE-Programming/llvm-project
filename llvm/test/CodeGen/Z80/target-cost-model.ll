; NOTE: Do not autogenerate
; RUN: opt -mtriple=ez80 -passes='print<cost-model>' -cost-kind=all -disable-output %s 2>&1 | FileCheck %s --check-prefix=ADL
; RUN: opt -mtriple=ez80 -passes='print<cost-model>' -cost-kind=all -z80-target-cost-model=false -disable-output %s 2>&1 | FileCheck %s --check-prefix=OFF
; RUN: opt -mtriple=ez80-code16 -passes='print<cost-model>' -cost-kind=all -disable-output %s 2>&1 | FileCheck %s --check-prefix=SHORT

; These are local register-resident instruction estimates. Size counts bytes;
; speed counts RAM cycle units, normalized to one native ADD. Runtime helper
; bodies, address setup and spills are deliberately outside these estimates.

declare i24 @callee(i24, i24)

define void @arithmetic(i8 %a, i8 %b, i16 %x, i16 %y, i24 %u, i24 %v, ptr %p) {
; ADL-LABEL: function 'arithmetic'
; ADL: costs of 1 for: %add24 = add i24
; ADL: costs of 2 for: %add16 = add i16
; ADL: costs of RThru:11 CodeSize:4 Lat:11 SizeLat:11 for: %mul24 = mul i24
; ADL: costs of RThru:5 CodeSize:4 Lat:5 SizeLat:5 for: %byte_product = mul i24
; ADL: costs of 4 for: %shift24 = shl i24
; ADL: costs of 6 for: %shift16 = shl i16
; ADL: costs of RThru:11 CodeSize:4 Lat:11 SizeLat:11 for: %divide = udiv i24
; ADL: costs of RThru:2 CodeSize:1 Lat:2 SizeLat:2 for: %load8 = load i8
; ADL: costs of RThru:5 CodeSize:3 Lat:5 SizeLat:5 for: %load16 = load i16
; ADL: costs of RThru:5 CodeSize:2 Lat:5 SizeLat:5 for: %load24 = load i24
; ADL: costs of RThru:4 CodeSize:2 Lat:4 SizeLat:4 for: store i24
; ADL: costs of RThru:25 CodeSize:8 Lat:25 SizeLat:25 for: %call = call i24
; OFF-LABEL: function 'arithmetic'
; OFF: costs of 1 for: %add24 = add i24
; OFF: costs of 1 for: %add16 = add i16
; SHORT-LABEL: function 'arithmetic'
; SHORT: costs of 1 for: %add16 = add i16
; SHORT: costs of 3 for: %shift16 = shl i16
; SHORT: costs of RThru:4 CodeSize:2 Lat:4 SizeLat:4 for: %load16 = load i16
  %add24 = add i24 %u, %v
  %add16 = add i16 %x, %y
  %mul24 = mul i24 %u, %v
  %ea = zext i8 %a to i24
  %eb = zext i8 %b to i24
  %byte_product = mul i24 %ea, %eb
  %shift24 = shl i24 %u, 4
  %shift16 = shl i16 %x, 3
  %divide = udiv i24 %u, %v
  %load8 = load i8, ptr %p, align 1
  %load16 = load i16, ptr %p, align 1
  %load24 = load i24, ptr %p, align 1
  store i24 %u, ptr %p, align 1
  %call = call i24 @callee(i24 %u, i24 %v)
  ret void
}

define i24 @multiply_speed(i24 %u) {
; ADL-LABEL: function 'multiply_speed'
; ADL: costs of RThru:21 CodeSize:16 Lat:21 SizeLat:21 for: %v = mul i24 %u, 255
  %v = mul i24 %u, 255
  ret i24 %v
}

define i24 @multiply_size(i24 %u) optsize minsize {
; ADL-LABEL: function 'multiply_size'
; ADL: costs of RThru:15 CodeSize:8 Lat:15 SizeLat:15 for: %v = mul i24 %u, 255
  %v = mul i24 %u, 255
  ret i24 %v
}
