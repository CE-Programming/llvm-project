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

## Constant unsigned division and remainder

For a dividend proven to be in `0..255`, the target preserves constant unsigned
division before generic combines turn it into full-width multiply/shift calls.
An unsigned divisor of one or greater than 255 gives a direct input or constant
zero. Power-of-two divisors use byte shifts or masks. Signed division/remainder
is unchanged, and unknown dividend ranges, zero divisors and variable divisors
retain the previous lowering.

For other divisors, the legalizer finds an unsigned-byte multiplier `m` and
shift `s >= 8` such that:

```
q = (n * m) >> s
r = n - q * divisor
```

The formula is accepted only when it is exact for **all 256 byte dividends**.
The quotient uses the high byte of an `MLT` result and byte right shifts; the
remainder uses a second byte multiply and subtraction. The result is explicitly
zero-extended to its requested 16- or 24-bit width. This finds reciprocals for
192 non-power-of-two divisors in `1..255`. Divisors requiring a correction or a
ninth reciprocal bit (including 7, 11 and 31) retain fallback.

For example, division by three uses multiplier 171 and shift nine. Dividing a
24-bit value whose range is known to fit in a byte no longer needs the generic
48-bit multiply and shift libcalls for this operation.

The size estimate includes byte setup, multiplier, MLT, extraction, result
extension, byte shifts, mode prefixes, and a second MLT/subtraction for remainder.
Size functions use a native constant-load plus call budget (six bytes in short
mode, eight in ADL); speed functions permit four times that budget to avoid the
runtime division loop. O0 uses the size budget. Final register allocation can
change actual size. `-mllvm -z80-byte-constant-div=false` disables the new
preservation and lowering; existing generic MLT combines can still apply.

Handwritten regression tests cover 8-, 16- and 24-bit results, both multiply
operand orders, full-width result definitions, range and signedness guards,
short eZ80/Z180/ordinary Z80, size policy, comparison switches and machine
verification through legalization. Mathematical verification of all byte/word
input pairs and all accepted byte reciprocal formulas complements emulator
validation of the final emitted code; it does not replace it.
