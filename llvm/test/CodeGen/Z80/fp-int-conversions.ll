; NOTE: Do not autogenerate
; RUN: llc -mtriple=z80 -O0 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=z80 -O2 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=ez80 -mattr=-adl -O0 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=ez80 -mattr=-adl -O2 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=ez80 -O0 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=ez80 -O2 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=ez80 -O0 -stop-after=legalizer -verify-machineinstrs < %s | FileCheck %s --check-prefix=MIR
; RUN: llc -mtriple=ez80 -O2 -stop-after=legalizer -verify-machineinstrs < %s | FileCheck %s --check-prefix=MIR
;
; Non-power-of-two integer inputs/results must use a supported libcall width.
; Floating-point-to-integer conversions truncate the widened result back to the requested width.
; Keep the existing 24-, 32- and 64-bit paths covered alongside i48.
; MIR checks pin extension signedness and the 64-bit helper arguments, plus the
; two 24-bit result limbs returned after discarding the helper's upper 16 bits.

; CHECK-LABEL: s24_to_float:
; CHECK: call __ltof
; CHECK: ret
define float @s24_to_float(i24 %x) {
  %r = sitofp i24 %x to float
  ret float %r
}

; CHECK-LABEL: s24_to_double:
; CHECK: call __ltod
; CHECK: ret
define double @s24_to_double(i24 %x) {
  %r = sitofp i24 %x to double
  ret double %r
}

; CHECK-LABEL: u24_to_float:
; CHECK: call __ultof
; CHECK: ret
define float @u24_to_float(i24 %x) {
  %r = uitofp i24 %x to float
  ret float %r
}

; CHECK-LABEL: u24_to_double:
; CHECK: call __ultod
; CHECK: ret
define double @u24_to_double(i24 %x) {
  %r = uitofp i24 %x to double
  ret double %r
}

; CHECK-LABEL: s32_to_float:
; CHECK: call __ltof
; CHECK: ret
define float @s32_to_float(i32 %x) {
  %r = sitofp i32 %x to float
  ret float %r
}

; CHECK-LABEL: s32_to_double:
; CHECK: call __ltod
; CHECK: ret
define double @s32_to_double(i32 %x) {
  %r = sitofp i32 %x to double
  ret double %r
}

; CHECK-LABEL: u32_to_float:
; CHECK: call __ultof
; CHECK: ret
define float @u32_to_float(i32 %x) {
  %r = uitofp i32 %x to float
  ret float %r
}

; CHECK-LABEL: u32_to_double:
; CHECK: call __ultod
; CHECK: ret
define double @u32_to_double(i32 %x) {
  %r = uitofp i32 %x to double
  ret double %r
}

; MIR-LABEL: name: s48_to_float
; MIR: %[[LOW:[0-9]+]]:_(s24) = G_LOAD
; MIR: %[[MID:[0-9]+]]:_(s24) = G_LOAD
; MIR: %[[INPUT:[0-9]+]]:_(s48) = G_MERGE_VALUES %[[LOW]](s24), %[[MID]](s24)
; MIR: %[[WIDE:[0-9]+]]:_(s64) = G_SEXT %[[INPUT]](s48)
; MIR: %[[LO:[0-9]+]]:_(s24) = G_EXTRACT %[[WIDE]](s64), 0
; MIR: %[[MI:[0-9]+]]:_(s24) = G_EXTRACT %[[WIDE]](s64), 24
; MIR: %[[HI:[0-9]+]]:_(s16) = G_EXTRACT %[[WIDE]](s64), 48
; MIR: $uhl = COPY %[[LO]](s24)
; MIR: $ude = COPY %[[MI]](s24)
; MIR: $bc = COPY %[[HI]](s16)
; MIR: CALL24 &_lltof,
; MIR: %[[OUT0:[0-9]+]]:_(s24) = COPY $uhl
; MIR: %[[OUT1:[0-9]+]]:_(s8) = COPY $e
; MIR: ADJCALLSTACKUP24
; MIR: $uhl = COPY %[[OUT0]](s24)
; MIR: $e = COPY %[[OUT1]](s8)
; MIR-NEXT: RET24 {{.*}}implicit $uhl, implicit $e{{$}}
; CHECK-LABEL: s48_to_float:
; CHECK: call __lltof
; CHECK: ret
define float @s48_to_float(i48 %x) {
  %r = sitofp i48 %x to float
  ret float %r
}

; MIR-LABEL: name: s48_to_double
; MIR: %[[LOW:[0-9]+]]:_(s24) = G_LOAD
; MIR: %[[MID:[0-9]+]]:_(s24) = G_LOAD
; MIR: %[[INPUT:[0-9]+]]:_(s48) = G_MERGE_VALUES %[[LOW]](s24), %[[MID]](s24)
; MIR: %[[WIDE:[0-9]+]]:_(s64) = G_SEXT %[[INPUT]](s48)
; MIR: %[[LO:[0-9]+]]:_(s24) = G_EXTRACT %[[WIDE]](s64), 0
; MIR: %[[MI:[0-9]+]]:_(s24) = G_EXTRACT %[[WIDE]](s64), 24
; MIR: %[[HI:[0-9]+]]:_(s16) = G_EXTRACT %[[WIDE]](s64), 48
; MIR: $uhl = COPY %[[LO]](s24)
; MIR: $ude = COPY %[[MI]](s24)
; MIR: $bc = COPY %[[HI]](s16)
; MIR: CALL24 &_lltod,
; MIR: %[[OUT0:[0-9]+]]:_(s24) = COPY $uhl
; MIR: %[[OUT1:[0-9]+]]:_(s24) = COPY $ude
; MIR: %[[OUT2:[0-9]+]]:_(s16) = COPY $bc
; MIR: ADJCALLSTACKUP24
; MIR: $uhl = COPY %[[OUT0]](s24)
; MIR: $ude = COPY %[[OUT1]](s24)
; MIR: $bc = COPY %[[OUT2]](s16)
; MIR-NEXT: RET24 {{.*}}implicit $uhl, implicit $ude, implicit $bc{{$}}
; CHECK-LABEL: s48_to_double:
; CHECK: call __lltod
; CHECK: ret
define double @s48_to_double(i48 %x) {
  %r = sitofp i48 %x to double
  ret double %r
}

; MIR-LABEL: name: u48_to_float
; MIR: %[[LOW:[0-9]+]]:_(s24) = G_LOAD
; MIR: %[[MID:[0-9]+]]:_(s24) = G_LOAD
; MIR: %[[ZERO:[0-9]+]]:_(s24) = G_CONSTANT i24 0
; MIR: %[[HIGHZERO:[0-9]+]]:_(s16) = G_EXTRACT %[[ZERO]](s24), 0
; MIR: %[[PART0:[0-9]+]]:_(s64) = G_INSERT %{{[0-9]+}}, %[[LOW]](s24), 0
; MIR: %[[PART1:[0-9]+]]:_(s64) = G_INSERT %[[PART0]], %[[MID]](s24), 24
; MIR: %[[WIDE:[0-9]+]]:_(s64) = G_INSERT %[[PART1]], %[[HIGHZERO]](s16), 48
; MIR: %[[LO:[0-9]+]]:_(s24) = G_EXTRACT %[[WIDE]](s64), 0
; MIR: %[[MI:[0-9]+]]:_(s24) = G_EXTRACT %[[WIDE]](s64), 24
; MIR: %[[HI:[0-9]+]]:_(s16) = G_EXTRACT %[[WIDE]](s64), 48
; MIR: $uhl = COPY %[[LO]](s24)
; MIR: $ude = COPY %[[MI]](s24)
; MIR: $bc = COPY %[[HI]](s16)
; MIR: CALL24 &_ulltof,
; MIR: %[[OUT0:[0-9]+]]:_(s24) = COPY $uhl
; MIR: %[[OUT1:[0-9]+]]:_(s8) = COPY $e
; MIR: ADJCALLSTACKUP24
; MIR: $uhl = COPY %[[OUT0]](s24)
; MIR: $e = COPY %[[OUT1]](s8)
; MIR-NEXT: RET24 {{.*}}implicit $uhl, implicit $e{{$}}
; CHECK-LABEL: u48_to_float:
; CHECK: call __ulltof
; CHECK: ret
define float @u48_to_float(i48 %x) {
  %r = uitofp i48 %x to float
  ret float %r
}

; MIR-LABEL: name: u48_to_double
; MIR: %[[LOW:[0-9]+]]:_(s24) = G_LOAD
; MIR: %[[MID:[0-9]+]]:_(s24) = G_LOAD
; MIR: %[[ZERO:[0-9]+]]:_(s24) = G_CONSTANT i24 0
; MIR: %[[HIGHZERO:[0-9]+]]:_(s16) = G_EXTRACT %[[ZERO]](s24), 0
; MIR: %[[PART0:[0-9]+]]:_(s64) = G_INSERT %{{[0-9]+}}, %[[LOW]](s24), 0
; MIR: %[[PART1:[0-9]+]]:_(s64) = G_INSERT %[[PART0]], %[[MID]](s24), 24
; MIR: %[[WIDE:[0-9]+]]:_(s64) = G_INSERT %[[PART1]], %[[HIGHZERO]](s16), 48
; MIR: %[[LO:[0-9]+]]:_(s24) = G_EXTRACT %[[WIDE]](s64), 0
; MIR: %[[MI:[0-9]+]]:_(s24) = G_EXTRACT %[[WIDE]](s64), 24
; MIR: %[[HI:[0-9]+]]:_(s16) = G_EXTRACT %[[WIDE]](s64), 48
; MIR: $uhl = COPY %[[LO]](s24)
; MIR: $ude = COPY %[[MI]](s24)
; MIR: $bc = COPY %[[HI]](s16)
; MIR: CALL24 &_ulltod,
; MIR: %[[OUT0:[0-9]+]]:_(s24) = COPY $uhl
; MIR: %[[OUT1:[0-9]+]]:_(s24) = COPY $ude
; MIR: %[[OUT2:[0-9]+]]:_(s16) = COPY $bc
; MIR: ADJCALLSTACKUP24
; MIR: $uhl = COPY %[[OUT0]](s24)
; MIR: $ude = COPY %[[OUT1]](s24)
; MIR: $bc = COPY %[[OUT2]](s16)
; MIR-NEXT: RET24 {{.*}}implicit $uhl, implicit $ude, implicit $bc{{$}}
; CHECK-LABEL: u48_to_double:
; CHECK: call __ulltod
; CHECK: ret
define double @u48_to_double(i48 %x) {
  %r = uitofp i48 %x to double
  ret double %r
}

; CHECK-LABEL: s64_to_float:
; CHECK: call __lltof
; CHECK: ret
define float @s64_to_float(i64 %x) {
  %r = sitofp i64 %x to float
  ret float %r
}

; CHECK-LABEL: s64_to_double:
; CHECK: call __lltod
; CHECK: ret
define double @s64_to_double(i64 %x) {
  %r = sitofp i64 %x to double
  ret double %r
}

; CHECK-LABEL: u64_to_float:
; CHECK: call __ulltof
; CHECK: ret
define float @u64_to_float(i64 %x) {
  %r = uitofp i64 %x to float
  ret float %r
}

; CHECK-LABEL: u64_to_double:
; CHECK: call __ulltod
; CHECK: ret
define double @u64_to_double(i64 %x) {
  %r = uitofp i64 %x to double
  ret double %r
}

; CHECK-LABEL: float_to_s24:
; CHECK: call __ftol
; CHECK: ret
define i24 @float_to_s24(float %x) {
  %r = fptosi float %x to i24
  ret i24 %r
}

; CHECK-LABEL: double_to_s24:
; CHECK: call __dtol
; CHECK: ret
define i24 @double_to_s24(double %x) {
  %r = fptosi double %x to i24
  ret i24 %r
}

; CHECK-LABEL: float_to_u24:
; CHECK: call __ftoul
; CHECK: ret
define i24 @float_to_u24(float %x) {
  %r = fptoui float %x to i24
  ret i24 %r
}

; CHECK-LABEL: double_to_u24:
; CHECK: call __dtoul
; CHECK: ret
define i24 @double_to_u24(double %x) {
  %r = fptoui double %x to i24
  ret i24 %r
}

; CHECK-LABEL: float_to_s32:
; CHECK: call __ftol
; CHECK: ret
define i32 @float_to_s32(float %x) {
  %r = fptosi float %x to i32
  ret i32 %r
}

; CHECK-LABEL: double_to_s32:
; CHECK: call __dtol
; CHECK: ret
define i32 @double_to_s32(double %x) {
  %r = fptosi double %x to i32
  ret i32 %r
}

; CHECK-LABEL: float_to_u32:
; CHECK: call __ftoul
; CHECK: ret
define i32 @float_to_u32(float %x) {
  %r = fptoui float %x to i32
  ret i32 %r
}

; CHECK-LABEL: double_to_u32:
; CHECK: call __dtoul
; CHECK: ret
define i32 @double_to_u32(double %x) {
  %r = fptoui double %x to i32
  ret i32 %r
}

; MIR-LABEL: name: float_to_s48
; MIR: %[[ARG0:[0-9]+]]:_(s24) = G_LOAD
; MIR: %[[ARG1:[0-9]+]]:_(s8) = G_LOAD
; MIR: $uhl = COPY %[[ARG0]](s24)
; MIR: $e = COPY %[[ARG1]](s8)
; MIR: CALL24 &_ftoll,
; MIR-NOT: = COPY $bc
; MIR: %[[LO:[0-9]+]]:_(s24) = COPY $uhl
; MIR-NOT: = COPY $bc
; MIR: %[[HI:[0-9]+]]:_(s24) = COPY $ude
; MIR-NOT: = COPY $bc
; MIR: ADJCALLSTACKUP24
; MIR: $uhl = COPY %[[LO]](s24)
; MIR: $ude = COPY %[[HI]](s24)
; MIR-NEXT: RET24 {{.*}}implicit $uhl, implicit $ude{{$}}
; CHECK-LABEL: float_to_s48:
; CHECK: call __ftoll
; CHECK: ret
define i48 @float_to_s48(float %x) {
  %r = fptosi float %x to i48
  ret i48 %r
}

; MIR-LABEL: name: double_to_s48
; MIR: %[[ARG0:[0-9]+]]:_(s24) = G_LOAD
; MIR: %[[ARG1:[0-9]+]]:_(s24) = G_LOAD
; MIR: %[[ARG2:[0-9]+]]:_(s16) = G_LOAD
; MIR: $uhl = COPY %[[ARG0]](s24)
; MIR: $ude = COPY %[[ARG1]](s24)
; MIR: $bc = COPY %[[ARG2]](s16)
; MIR: CALL24 &_dtoll,
; MIR-NOT: = COPY $bc
; MIR: %[[LO:[0-9]+]]:_(s24) = COPY $uhl
; MIR-NOT: = COPY $bc
; MIR: %[[HI:[0-9]+]]:_(s24) = COPY $ude
; MIR-NOT: = COPY $bc
; MIR: ADJCALLSTACKUP24
; MIR: $uhl = COPY %[[LO]](s24)
; MIR: $ude = COPY %[[HI]](s24)
; MIR-NEXT: RET24 {{.*}}implicit $uhl, implicit $ude{{$}}
; CHECK-LABEL: double_to_s48:
; CHECK: call __dtoll
; CHECK: ret
define i48 @double_to_s48(double %x) {
  %r = fptosi double %x to i48
  ret i48 %r
}

; MIR-LABEL: name: float_to_u48
; MIR: %[[ARG0:[0-9]+]]:_(s24) = G_LOAD
; MIR: %[[ARG1:[0-9]+]]:_(s8) = G_LOAD
; MIR: $uhl = COPY %[[ARG0]](s24)
; MIR: $e = COPY %[[ARG1]](s8)
; MIR: CALL24 &_ftoull,
; MIR-NOT: = COPY $bc
; MIR: %[[LO:[0-9]+]]:_(s24) = COPY $uhl
; MIR-NOT: = COPY $bc
; MIR: %[[HI:[0-9]+]]:_(s24) = COPY $ude
; MIR-NOT: = COPY $bc
; MIR: ADJCALLSTACKUP24
; MIR: $uhl = COPY %[[LO]](s24)
; MIR: $ude = COPY %[[HI]](s24)
; MIR-NEXT: RET24 {{.*}}implicit $uhl, implicit $ude{{$}}
; CHECK-LABEL: float_to_u48:
; CHECK: call __ftoull
; CHECK: ret
define i48 @float_to_u48(float %x) {
  %r = fptoui float %x to i48
  ret i48 %r
}

; MIR-LABEL: name: double_to_u48
; MIR: %[[ARG0:[0-9]+]]:_(s24) = G_LOAD
; MIR: %[[ARG1:[0-9]+]]:_(s24) = G_LOAD
; MIR: %[[ARG2:[0-9]+]]:_(s16) = G_LOAD
; MIR: $uhl = COPY %[[ARG0]](s24)
; MIR: $ude = COPY %[[ARG1]](s24)
; MIR: $bc = COPY %[[ARG2]](s16)
; MIR: CALL24 &_dtoull,
; MIR-NOT: = COPY $bc
; MIR: %[[LO:[0-9]+]]:_(s24) = COPY $uhl
; MIR-NOT: = COPY $bc
; MIR: %[[HI:[0-9]+]]:_(s24) = COPY $ude
; MIR-NOT: = COPY $bc
; MIR: ADJCALLSTACKUP24
; MIR: $uhl = COPY %[[LO]](s24)
; MIR: $ude = COPY %[[HI]](s24)
; MIR-NEXT: RET24 {{.*}}implicit $uhl, implicit $ude{{$}}
; CHECK-LABEL: double_to_u48:
; CHECK: call __dtoull
; CHECK: ret
define i48 @double_to_u48(double %x) {
  %r = fptoui double %x to i48
  ret i48 %r
}

; CHECK-LABEL: float_to_s64:
; CHECK: call __ftoll
; CHECK: ret
define i64 @float_to_s64(float %x) {
  %r = fptosi float %x to i64
  ret i64 %r
}

; CHECK-LABEL: double_to_s64:
; CHECK: call __dtoll
; CHECK: ret
define i64 @double_to_s64(double %x) {
  %r = fptosi double %x to i64
  ret i64 %r
}

; CHECK-LABEL: float_to_u64:
; CHECK: call __ftoull
; CHECK: ret
define i64 @float_to_u64(float %x) {
  %r = fptoui float %x to i64
  ret i64 %r
}

; CHECK-LABEL: double_to_u64:
; CHECK: call __dtoull
; CHECK: ret
define i64 @double_to_u64(double %x) {
  %r = fptoui double %x to i64
  ret i64 %r
}
