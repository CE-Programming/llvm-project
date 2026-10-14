# Proven-byte arithmetic

GlobalISel uses `MLT` when known bits prove that both operands of a 16- or
24-bit multiplication fit in unsigned bytes. For speed functions it now also
splits a proven unsigned byte times an unsigned word into two byte products:

```
a * b = a * (b & 255) + ((a * (b >> 8)) << 8)
```

This is exact at 24 bits and wraps at 16 bits. Signed inputs are eligible only
when their actual bit patterns satisfy those unsigned bounds; sign-extending
an arbitrary signed word to 24 bits fails that proof. Both operand orders work.
The second product's low/high bytes are extracted individually so that its
unused upper byte does not require a spill. Native short eZ80 and Z180 use the
16-bit form. Ordinary Z80 retains its library fallback.

Two products and byte setup exceed a multiply-call size budget. This byte/word
expansion therefore applies to speed functions; `optsize`, `minsize`, and O0
retain compact library calls. Existing byte/byte and constant-multiply rules
remain independent. `-mllvm -z80-wide-byte-mul=false` disables both widened-byte
hardware expansions.
