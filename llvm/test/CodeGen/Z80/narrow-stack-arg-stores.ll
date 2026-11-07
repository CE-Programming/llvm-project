; NOTE: Do not autogenerate
; RUN: llc -mtriple=ez80 -O0 -stop-after=irtranslator -verify-machineinstrs %s -o - | FileCheck %s --check-prefixes=MIR,ADL
; RUN: llc -mtriple=ez80-code16 -O0 -stop-after=irtranslator -verify-machineinstrs %s -o - | FileCheck %s --check-prefixes=MIR,SHORT
; RUN: llc -mtriple=z80 -O0 -stop-after=irtranslator -verify-machineinstrs %s -o - | FileCheck %s --check-prefixes=MIR,SHORT
; RUN: llc -mtriple=ez80 -O2 -verify-machineinstrs %s -o /dev/null
; RUN: llc -mtriple=ez80-code16 -O2 -verify-machineinstrs %s -o /dev/null
; RUN: llc -mtriple=z80 -O2 -verify-machineinstrs %s -o /dev/null
;
; The leading byval argument prevents replacing all outgoing stores with PUSH.
; Native stack slots are 24/16 bits, but the stores carry only the argument
; bytes. Their memory operands must describe those bytes, not the slot size.
;
; MIR-LABEL: name: call_narrow
; MIR: [[BYTE:%[0-9]+]]:_(s8) = G_TRUNC
; ADL: [[WORD:%[0-9]+]]:_(s16) = G_TRUNC
; SHORT: [[WORD:%[0-9]+]]:_(s16) = G_LOAD
; MIR: [[FLAG:%[0-9]+]]:_(s1) = G_TRUNC
; MIR: G_MEMCPY
; MIR: G_STORE [[BYTE]](s8), {{.*}} :: (store (s8) into stack +
; MIR: G_STORE [[WORD]](s16), {{.*}} :: (store (s16) into stack +
; MIR: [[EXT:%[0-9]+]]:_(s8) = G_ANYEXT [[FLAG]](s1)
; MIR: G_STORE [[EXT]](s8), {{.*}} :: (store (s8) into stack +
;
declare void @narrow(ptr byval(i8) align 1, i8, i16, i1)

define void @call_narrow(ptr %p, i8 %byte, i16 %word, i1 %flag) {
  call void @narrow(ptr byval(i8) align 1 %p, i8 %byte, i16 %word, i1 %flag)
  ret void
}
