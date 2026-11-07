; NOTE: Do not autogenerate
; RUN: llc -O2 -mtriple=z80 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -O2 -mtriple=ez80-code16 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -O2 -mtriple=ez80 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -O0 -mtriple=z80 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -O0 -mtriple=ez80-code16 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -O0 -mtriple=ez80 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -O0 -mtriple=ez80 -stop-after=legalizer -verify-machineinstrs < %s | FileCheck %s --check-prefix=MIR
; RUN: llc -O2 -mtriple=ez80 -stop-after=legalizer -verify-machineinstrs < %s | FileCheck %s --check-prefix=MIR
;
; Ordered/unordered predicates must inspect NaNs instead of returning constants.
; Strict numerical predicates also canonicalize signed zero for the ROM helper.
; Explicit nnan+nsz permits the existing compact comparison path. Each flag
; alone must retain the handling required for the other floating-point case.
; MIR checks tie operand classification and zero canonicalization to the
; returned boolean; an unused classification call must not satisfy the test.

; CHECK-LABEL: unordered32:
; CHECK: call __lcmpu
; CHECK: call __lcmpu
; CHECK: ret
; MIR-LABEL: name: unordered32
; MIR: CALL24 &_land,
; MIR: CALL24 &_lcmpu,
; MIR: %[[LNAN:[0-9]+]]:r8(s1) = SetCC 3, implicit $f
; MIR: %[[LANY:[0-9]+]]:_(s8) = G_ANYEXT %[[LNAN]](s1)
; MIR: %[[LCLASS:[0-9]+]]:_(s8) = G_OR %{{[0-9]+}}, %[[LANY]]
; MIR: CALL24 &_land,
; MIR: CALL24 &_lcmpu,
; MIR: %[[RNAN:[0-9]+]]:r8(s1) = SetCC 3, implicit $f
; MIR: %[[RANY:[0-9]+]]:_(s8) = G_ANYEXT %[[RNAN]](s1)
; MIR: %[[RCLASS:[0-9]+]]:_(s8) = G_OR %{{[0-9]+}}, %[[RANY]]
; MIR: %[[UNORDERED:[0-9]+]]:_(s8) = G_OR %[[LCLASS]], %[[RCLASS]]
; MIR: $a = COPY %[[UNORDERED]](s8)
; MIR-NEXT: RET24 {{.*}}implicit $a{{$}}
define i1 @unordered32(float %a, float %b) {
  %r = fcmp uno float %a, %b
  ret i1 %r
}

; CHECK-LABEL: ordered64:
; CHECK: call __llcmpu
; CHECK: call __llcmpu
; CHECK: ret
; MIR-LABEL: name: ordered64
; MIR: CALL24 &_lland,
; MIR: CALL24 &_llcmpu,
; MIR: %[[LNAN:[0-9]+]]:r8(s1) = SetCC 3, implicit $f
; MIR: %[[LANY:[0-9]+]]:_(s8) = G_ANYEXT %[[LNAN]](s1)
; MIR: %[[LCLASS:[0-9]+]]:_(s8) = G_OR %{{[0-9]+}}, %[[LANY]]
; MIR: CALL24 &_lland,
; MIR: CALL24 &_llcmpu,
; MIR: %[[RNAN:[0-9]+]]:r8(s1) = SetCC 3, implicit $f
; MIR: %[[RANY:[0-9]+]]:_(s8) = G_ANYEXT %[[RNAN]](s1)
; MIR: %[[RCLASS:[0-9]+]]:_(s8) = G_OR %{{[0-9]+}}, %[[RANY]]
; MIR: %[[UNORDERED:[0-9]+]]:_(s8) = G_OR %[[LCLASS]], %[[RCLASS]]
; MIR: %[[ONE:[0-9]+]]:_(s8) = G_CONSTANT i8 1
; MIR: %[[ORDERED:[0-9]+]]:_(s8) = G_XOR %[[UNORDERED]], %[[ONE]]
; MIR: $a = COPY %[[ORDERED]](s8)
; MIR-NEXT: RET24 {{.*}}implicit $a{{$}}
define i1 @ordered64(double %a, double %b) {
  %r = fcmp ord double %a, %b
  ret i1 %r
}

; CHECK-LABEL: less32:
; CHECK: call __lcmpu
; CHECK: call __lcmpu
; CHECK: call __lcmpzero
; CHECK: call __lcmpzero
; CHECK: call __fcmp
; CHECK: and a,
; CHECK: ret
; MIR-LABEL: name: less32
; MIR: %[[LLOW:[0-9]+]]:_(s24) = G_LOAD
; MIR: %[[LHIGH:[0-9]+]]:_(s8) = G_LOAD
; MIR: %[[RLOW:[0-9]+]]:_(s24) = G_LOAD
; MIR: %[[RHIGH:[0-9]+]]:_(s8) = G_LOAD
; MIR: %[[MASKLOW:[0-9]+]]:_(s24) = G_CONSTANT i24 -1
; MIR: %[[MASKHIGH:[0-9]+]]:_(s8) = G_CONSTANT i8 127
; MIR: CALL24 &_land,
; MIR: CALL24 &_lcmpu,
; MIR: %[[LNAN:[0-9]+]]:r8(s1) = SetCC 3, implicit $f
; MIR: %[[LANY:[0-9]+]]:_(s8) = G_ANYEXT %[[LNAN]](s1)
; MIR: %[[LCLASS:[0-9]+]]:_(s8) = G_OR %{{[0-9]+}}, %[[LANY]]
; MIR: CALL24 &_land,
; MIR: CALL24 &_lcmpu,
; MIR: %[[RNAN:[0-9]+]]:r8(s1) = SetCC 3, implicit $f
; MIR: %[[RANY:[0-9]+]]:_(s8) = G_ANYEXT %[[RNAN]](s1)
; MIR: %[[RCLASS:[0-9]+]]:_(s8) = G_OR %{{[0-9]+}}, %[[RANY]]
; MIR: %[[UNORDERED:[0-9]+]]:_(s8) = G_OR %[[LCLASS]], %[[RCLASS]]
; MIR: %[[ZEROLOW:[0-9]+]]:_(s24) = G_CONSTANT i24 0
; MIR: %[[ZEROHIGH:[0-9]+]]:_(s8) = G_CONSTANT i8 0
; MIR: $uhl = COPY %[[LLOW]](s24)
; MIR: $e = COPY %[[LHIGH]](s8)
; MIR: $ubc = COPY %[[MASKLOW]](s24)
; MIR: $a = COPY %[[MASKHIGH]](s8)
; MIR: CALL24 &_land,
; MIR: %[[LABSLOW:[0-9]+]]:_(s24) = COPY $uhl
; MIR: %[[LABSHIGH:[0-9]+]]:_(s8) = COPY $e
; MIR: $uhl = COPY %[[LABSLOW]](s24)
; MIR: $e = COPY %[[LABSHIGH]](s8)
; MIR: CALL24 &_lcmpzero,
; MIR: %[[LISZERO:[0-9]+]]:r8(s1) = SetCC 1, implicit $f
; MIR: %[[LCANONLOW:[0-9]+]]:_(s24) = G_SELECT %[[LISZERO]](s1), %[[ZEROLOW]], %[[LLOW]]
; MIR: %[[LCANONHIGH:[0-9]+]]:_(s8) = G_SELECT %[[LISZERO]](s1), %[[ZEROHIGH]], %[[LHIGH]]
; MIR: $uhl = COPY %[[RLOW]](s24)
; MIR: $e = COPY %[[RHIGH]](s8)
; MIR: $ubc = COPY %[[MASKLOW]](s24)
; MIR: $a = COPY %[[MASKHIGH]](s8)
; MIR: CALL24 &_land,
; MIR: %[[RABSLOW:[0-9]+]]:_(s24) = COPY $uhl
; MIR: %[[RABSHIGH:[0-9]+]]:_(s8) = COPY $e
; MIR: $uhl = COPY %[[RABSLOW]](s24)
; MIR: $e = COPY %[[RABSHIGH]](s8)
; MIR: CALL24 &_lcmpzero,
; MIR: %[[RISZERO:[0-9]+]]:r8(s1) = SetCC 1, implicit $f
; MIR: %[[RCANONLOW:[0-9]+]]:_(s24) = G_SELECT %[[RISZERO]](s1), %[[ZEROLOW]], %[[RLOW]]
; MIR: %[[RCANONHIGH:[0-9]+]]:_(s8) = G_SELECT %[[RISZERO]](s1), %[[ZEROHIGH]], %[[RHIGH]]
; MIR: $uhl = COPY %[[LCANONLOW]](s24)
; MIR: $e = COPY %[[LCANONHIGH]](s8)
; MIR: $ubc = COPY %[[RCANONLOW]](s24)
; MIR: $a = COPY %[[RCANONHIGH]](s8)
; MIR: CALL24 &_fcmp,
; MIR: %[[NUMPRED:[0-9]+]]:r8(s1) = SetCC 7, implicit $f
; MIR: ADJCALLSTACKUP24
; MIR: %[[ONE:[0-9]+]]:_(s8) = G_CONSTANT i8 1
; MIR: %[[ORDERED:[0-9]+]]:_(s8) = G_XOR %[[UNORDERED]], %[[ONE]]
; MIR: %[[NUMERIC:[0-9]+]]:_(s8) = G_ANYEXT %[[NUMPRED]](s1)
; MIR: %[[RESULT:[0-9]+]]:_(s8) = G_AND %[[NUMERIC]], %[[ORDERED]]
; MIR: $a = COPY %[[RESULT]](s8)
; MIR-NEXT: RET24 {{.*}}implicit $a{{$}}
define i1 @less32(float %a, float %b) {
  %r = fcmp olt float %a, %b
  ret i1 %r
}

; CHECK-LABEL: unordered_less64:
; CHECK: call __llcmpu
; CHECK: call __llcmpu
; CHECK: call __llcmpzero
; CHECK: call __llcmpzero
; CHECK: call __dcmp
; CHECK: or a,
; CHECK: ret
; MIR-LABEL: name: unordered_less64
; MIR: CALL24 &_lland,
; MIR: CALL24 &_llcmpu,
; MIR: %[[LNAN:[0-9]+]]:r8(s1) = SetCC 3, implicit $f
; MIR: %[[LANY:[0-9]+]]:_(s8) = G_ANYEXT %[[LNAN]](s1)
; MIR: %[[LCLASS:[0-9]+]]:_(s8) = G_OR %{{[0-9]+}}, %[[LANY]]
; MIR: CALL24 &_lland,
; MIR: CALL24 &_llcmpu,
; MIR: %[[RNAN:[0-9]+]]:r8(s1) = SetCC 3, implicit $f
; MIR: %[[RANY:[0-9]+]]:_(s8) = G_ANYEXT %[[RNAN]](s1)
; MIR: %[[RCLASS:[0-9]+]]:_(s8) = G_OR %{{[0-9]+}}, %[[RANY]]
; MIR: %[[UNORDERED:[0-9]+]]:_(s8) = G_OR %[[LCLASS]], %[[RCLASS]]
; MIR: CALL24 &_llcmpzero,
; MIR: CALL24 &_llcmpzero,
; MIR: CALL24 &_dcmp,
; MIR: %[[NUMPRED:[0-9]+]]:r8(s1) = SetCC 7, implicit $f
; MIR: ADJCALLSTACKUP24
; MIR: %[[NUMERIC:[0-9]+]]:_(s8) = G_ANYEXT %[[NUMPRED]](s1)
; MIR: %[[RESULT:[0-9]+]]:_(s8) = G_OR %[[NUMERIC]], %[[UNORDERED]]
; MIR: $a = COPY %[[RESULT]](s8)
; MIR-NEXT: RET24 {{.*}}implicit $a{{$}}
define i1 @unordered_less64(double %a, double %b) {
  %r = fcmp ult double %a, %b
  ret i1 %r
}

; CHECK-LABEL: finite_fast32:
; CHECK-NOT: call __lcmpu
; CHECK-NOT: call __lcmpzero
; CHECK: call __fcmp
; CHECK-NOT: call __lcmpu
; CHECK-NOT: call __lcmpzero
; CHECK: ret
; MIR-LABEL: name: finite_fast32
; MIR-NOT: CALL24 &_lcmpu,
; MIR-NOT: CALL24 &_lcmpzero,
; MIR-NOT: G_SELECT
; MIR: CALL24 &_fcmp,
; MIR: %[[NUMPRED:[0-9]+]]:r8(s1) = SetCC 7, implicit $f
; MIR: %[[NUMERIC:[0-9]+]]:_(s8) = G_ANYEXT %[[NUMPRED]](s1)
; MIR: $a = COPY %[[NUMERIC]](s8)
; MIR-NEXT: RET24 {{.*}}implicit $a{{$}}
define i1 @finite_fast32(float %a, float %b) {
  %r = fcmp nnan nsz olt float %a, %b
  ret i1 %r
}

; CHECK-LABEL: finite_strict_zero32:
; CHECK-NOT: call __lcmpu
; CHECK: call __lcmpzero
; CHECK-NOT: call __lcmpu
; CHECK: call __lcmpzero
; CHECK-NOT: call __lcmpu
; CHECK: call __fcmp
; CHECK-NOT: call __lcmpu
; CHECK: ret
; MIR-LABEL: name: finite_strict_zero32
; MIR: CALL24 &_lcmpzero,
; MIR: CALL24 &_lcmpzero,
; MIR: CALL24 &_fcmp,
; MIR: %[[NUMPRED:[0-9]+]]:r8(s1) = SetCC 7, implicit $f
; MIR: %[[NUMERIC:[0-9]+]]:_(s8) = G_ANYEXT %[[NUMPRED]](s1)
; MIR: $a = COPY %[[NUMERIC]](s8)
; MIR-NEXT: RET24 {{.*}}implicit $a{{$}}
define i1 @finite_strict_zero32(float %a, float %b) {
  %r = fcmp nnan olt float %a, %b
  ret i1 %r
}

; CHECK-LABEL: nan_fast_zero32:
; CHECK-NOT: call __lcmpzero
; CHECK: call __lcmpu
; CHECK-NOT: call __lcmpzero
; CHECK: call __lcmpu
; CHECK-NOT: call __lcmpzero
; CHECK: call __fcmp
; CHECK-NOT: call __lcmpzero
; CHECK: and a,
; CHECK-NOT: call __lcmpzero
; CHECK: ret
; MIR-LABEL: name: nan_fast_zero32
; MIR: CALL24 &_land,
; MIR: CALL24 &_lcmpu,
; MIR: %[[LNAN:[0-9]+]]:r8(s1) = SetCC 3, implicit $f
; MIR: %[[LANY:[0-9]+]]:_(s8) = G_ANYEXT %[[LNAN]](s1)
; MIR: %[[LCLASS:[0-9]+]]:_(s8) = G_OR %{{[0-9]+}}, %[[LANY]]
; MIR: CALL24 &_land,
; MIR: CALL24 &_lcmpu,
; MIR: %[[RNAN:[0-9]+]]:r8(s1) = SetCC 3, implicit $f
; MIR: %[[RANY:[0-9]+]]:_(s8) = G_ANYEXT %[[RNAN]](s1)
; MIR: %[[RCLASS:[0-9]+]]:_(s8) = G_OR %{{[0-9]+}}, %[[RANY]]
; MIR: %[[UNORDERED:[0-9]+]]:_(s8) = G_OR %[[LCLASS]], %[[RCLASS]]
; MIR: CALL24 &_fcmp,
; MIR: %[[NUMPRED:[0-9]+]]:r8(s1) = SetCC 7, implicit $f
; MIR: ADJCALLSTACKUP24
; MIR: %[[ONE:[0-9]+]]:_(s8) = G_CONSTANT i8 1
; MIR: %[[ORDERED:[0-9]+]]:_(s8) = G_XOR %[[UNORDERED]], %[[ONE]]
; MIR: %[[NUMERIC:[0-9]+]]:_(s8) = G_ANYEXT %[[NUMPRED]](s1)
; MIR: %[[RESULT:[0-9]+]]:_(s8) = G_AND %[[NUMERIC]], %[[ORDERED]]
; MIR: $a = COPY %[[RESULT]](s8)
; MIR-NEXT: RET24 {{.*}}implicit $a{{$}}
define i1 @nan_fast_zero32(float %a, float %b) {
  %r = fcmp nsz olt float %a, %b
  ret i1 %r
}
