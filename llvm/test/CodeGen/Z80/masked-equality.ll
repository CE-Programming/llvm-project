; NOTE: Do not autogenerate
; RUN: llc -mtriple=ez80 -O2 -o - %s | FileCheck %s --check-prefix=EZ80
; RUN: llc -mtriple=ez80 -O2 -stop-after=legalizer -verify-machineinstrs -o - %s | FileCheck %s --check-prefix=MIR
; RUN: llc -mtriple=ez80 -O2 -stop-after=instruction-select -verify-machineinstrs -o /dev/null %s
; RUN: llc -mtriple=z80 -O2 -stop-after=z80-prelegalizer-combiner -verify-machineinstrs -o - %s | FileCheck %s --check-prefix=SHORT
; RUN: llc -mtriple=ez80 -O2 -z80-masked-equality=false -o - %s | FileCheck %s --check-prefix=OFF
;
; Tagged equality observes only bits 0..22. A wrapping difference is either
; 0 or 0x800000 exactly when those bits are equal. Doubling then testing Z
; must accept both, and must ignore input carry and every upper tag bit.

@target = global i24 0

; SHORT-LABEL: name: eq24
; SHORT-NOT: G_SUB
; SHORT: G_XOR
; SHORT: G_AND
; SHORT: G_ICMP
; EZ80-LABEL: _eq24:
; EZ80-NOT: call __ixor
; EZ80-NOT: call __ishru_1
; EZ80: or a, a
; EZ80-NEXT: sbc hl,
; EZ80-NEXT: or a, a
; EZ80-NEXT: adc hl, hl
; EZ80: ret
; MIR-LABEL: name: eq24
; MIR: G_SUB
; MIR: G_SHL
; MIR: G_ICMP intpred(eq)
; OFF-LABEL: _eq24:
; OFF: call __ixor
define i1 @eq24(i24 %a, i24 %b) minsize optsize {
 %x = xor i24 %a, %b
 %m = and i24 %x, 8388607
 %c = icmp eq i24 %m, 0
 ret i1 %c
}

; EZ80-LABEL: _ne24:
; EZ80-NOT: call __ixor
; EZ80-NOT: call __ishru_1
; EZ80: or a, a
; EZ80-NEXT: sbc hl,
; EZ80-NEXT: or a, a
; EZ80-NEXT: adc hl, hl
; EZ80: ret
; MIR-LABEL: name: ne24
; MIR: G_SUB
; MIR: G_SHL
; MIR: G_ICMP intpred(ne)
; OFF-LABEL: _ne24:
; OFF: call __ixor
define i1 @ne24(i24 %a, i24 %b) minsize optsize {
 %x = xor i24 %a, %b
 %m = and i24 %x, 8388607
 %c = icmp ne i24 %m, 0
 ret i1 %c
}

; EZ80-LABEL: _commuted24:
; EZ80-NOT: call __ixor
; EZ80-NOT: call __ishru_1
; EZ80: or a, a
; EZ80-NEXT: sbc hl,
; EZ80-NEXT: or a, a
; EZ80-NEXT: adc hl, hl
; EZ80: ret
; MIR-LABEL: name: commuted24
; MIR: G_SUB
; MIR: G_SHL
; MIR: G_ICMP intpred(eq)
; OFF-LABEL: _commuted24:
; OFF: call __ixor
define i1 @commuted24(i24 %a, i24 %b) minsize optsize {
 %x = xor i24 %b, %a
 %m = and i24 8388607, %x
 %c = icmp eq i24 0, %m
 ret i1 %c
}

; EZ80-LABEL: _global24:
; EZ80-NOT: call __ixor
; EZ80-NOT: call __ishru_1
; EZ80: or a, a
; EZ80-NEXT: sbc hl,
; EZ80-NEXT: or a, a
; EZ80-NEXT: adc hl, hl
; EZ80: ret
; MIR-LABEL: name: global24
; MIR: G_SUB
; MIR: G_SHL
; MIR: G_ICMP intpred(eq)
; OFF-LABEL: _global24:
; OFF: call __ixor
define i1 @global24(i24 %a) minsize optsize {
 %b = ptrtoint ptr @target to i24
 %x = xor i24 %a, %b
 %m = and i24 %x, 8388607
 %c = icmp eq i24 %m, 0
 ret i1 %c
}

; EZ80-LABEL: _live_input24:
; EZ80-NOT: call __ixor
; EZ80-NOT: call __ishru_1
; EZ80: or a, a
; EZ80-NEXT: sbc hl,
; EZ80-NEXT: or a, a
; EZ80-NEXT: adc hl, hl
; EZ80: ret
; MIR-LABEL: name: live_input24
; MIR: G_SUB
; MIR: G_SHL
; MIR: G_ICMP intpred(eq)
; OFF-LABEL: _live_input24:
; OFF: call __ixor
define i1 @live_input24(i24 %a, i24 %b, ptr %out) minsize optsize {
 %x = xor i24 %a, %b
 %m = and i24 %x, 8388607
 %c = icmp eq i24 %m, 0
 store i24 %a, ptr %out, align 1
 %next = getelementptr i8, ptr %out, i24 3
 store i24 %b, ptr %next, align 1
 ret i1 %c
}

; EZ80-LABEL: _other_mask:
; EZ80-NOT: adc hl, hl
; EZ80: ret
; MIR-LABEL: name: other_mask
; MIR-NOT: G_SUB
; MIR: RET
define i1 @other_mask(i24 %a, i24 %b) minsize optsize {
 %x = xor i24 %a, %b
 %m = and i24 %x, 4194303
 %c = icmp eq i24 %m, 0
 ret i1 %c
}

; EZ80-LABEL: _disjoint_mask:
; EZ80-NOT: adc hl, hl
; EZ80: ret
; MIR-LABEL: name: disjoint_mask
; MIR-NOT: G_SUB
; MIR: RET
define i1 @disjoint_mask(i24 %a, i24 %b) minsize optsize {
 %x = xor i24 %a, %b
 %m = and i24 %x, 8388605
 %c = icmp eq i24 %m, 0
 ret i1 %c
}

; EZ80-LABEL: _ordered:
; EZ80-NOT: adc hl, hl
; EZ80: ret
; MIR-LABEL: name: ordered
; MIR-NOT: G_SUB
; MIR: RET
define i1 @ordered(i24 %a, i24 %b) minsize optsize {
 %x = xor i24 %a, %b
 %m = and i24 %x, 8388607
 %c = icmp ugt i24 %m, 0
 ret i1 %c
}

; EZ80-LABEL: _shared_xor:
; EZ80-NOT: adc hl, hl
; EZ80: ret
; MIR-LABEL: name: shared_xor
; MIR-NOT: G_SUB
; MIR: RET
define i1 @shared_xor(i24 %a, i24 %b, ptr %out) minsize optsize {
 %x = xor i24 %a, %b
 store i24 %x, ptr %out, align 1
 %m = and i24 %x, 8388607
 %c = icmp eq i24 %m, 0
 ret i1 %c
}

; EZ80-LABEL: _shared_mask:
; EZ80-NOT: adc hl, hl
; EZ80: ret
; MIR-LABEL: name: shared_mask
; MIR-NOT: G_SUB
; MIR: RET
define i1 @shared_mask(i24 %a, i24 %b, ptr %out) minsize optsize {
 %x = xor i24 %a, %b
 %m = and i24 %x, 8388607
 store i24 %m, ptr %out, align 1
 %c = icmp eq i24 %m, 0
 ret i1 %c
}

; EZ80-LABEL: _word16:
; EZ80-NOT: adc hl, hl
; EZ80: ret
; MIR-LABEL: name: word16
; MIR-NOT: G_SUB
; MIR: RET
define i1 @word16(i16 %a, i16 %b) minsize optsize {
 %x = xor i16 %a, %b
 %m = and i16 %x, 32767
 %c = icmp eq i16 %m, 0
 ret i1 %c
}

; These live operands are also doubled in a successor. Instruction selection
; visits that block first and gives both inputs the accumulator register class;
; the equality subtraction must copy its RHS instead of changing that class.
; EZ80-LABEL: _tagged_order:
; EZ80-NOT: call __ixor
; EZ80: or a, a
; EZ80-NEXT: sbc hl,
; EZ80-NEXT: or a, a
; EZ80-NEXT: adc hl, hl
; EZ80: ret
; MIR-LABEL: name: tagged_order
; MIR: G_SUB
; MIR: G_SHL
; OFF-LABEL: _tagged_order:
; OFF: call __ixor

define i1 @tagged_order(i24 %a, i24 %b) minsize optsize {
 %x = xor i24 %a, %b
 %m = and i24 %x, 8388607
 %c = icmp eq i24 %m, 0
 br i1 %c, label %equal, label %order
order:
 %aa = and i24 %a, 8388607
 %bb = and i24 %b, 8388607
 %less = icmp ult i24 %aa, %bb
 ret i1 %less
equal:
 ret i1 false
}
