# Final eZ80 peepholes

The pattern selection in `Z80MachinePeepholeOptimization.cpp` is inspired by
[ZERICO2005/ez80_optimizer](https://github.com/ZERICO2005/ez80_optimizer), revision
`151a275889300bd448a1c124d58080e9b3d51a0e` (`src/asm_pattern.h`).
The implementation uses LLVM machine instructions and physical-register liveness;
it does not invoke the upstream text rewriter or depend on its executable.

The pass runs after pseudo expansion and address folding, before branch
selection. It supports eZ80 only, respects `optnone`, and is omitted at `-O0`.
`-mllvm -z80-machine-peephole-opt=false` disables it for comparisons.

Supported patterns:

- Fold LEA followed by an increment/decrement of the same register, within the
  signed displacement range and with matching operand widths.
- Remove two cancelling 24-bit DE/HL exchanges. Short exchanges clear upper
  bytes and are deliberately retained, even when their short values cancel.
- Fold final PUSH/POP DE/HL copies into an exchange only when the pushed source
  is marked killed and the source and all its aliases are dead. The kill proof
  is required because index-half copy expansion can hide live carrier values
  behind undef exchange operands. Remove a 24-bit self-copy.
  Exclude frame setup/destroy and
  explicit memory operands, including volatile stack accesses.
- Use RLCA/RRCA for an accumulator rotate immediately followed by SBC A,A.
  Only carry is observed between the rotate and SBC, which overwrites all flags.
- Use RLCA/RRCA to extract A's sign before SBC HL,HL, restoring A and preserving
  the final result and flags. Recognize matching LD L,A / RLC L instructions.
- Remove BIT 0,A or OR A,A after AND A,1 or SBC A,A when the sole flag observer
  is a Z/NZ branch and flags are dead on every successor. Other condition codes
  and live flags prevent the fold; the producer's dead flag marker is cleared.

The following upstream rewrites are not imported:

- Two EX DE,HL instructions must not become one: two full-width exchanges are
  an identity, while one swaps the values. They become zero instructions here.
- Boolean-test removals cannot be unconditional: BIT, AND and SBC have different
  effects on parity, carry and other flags. Only the checked Z/NZ use is folded.
- Runtime-call canonicalizations/inlining need an independently verified library
  ABI and flag/register contract. In particular, fabs and fabsf cannot generally
  be substituted solely from their names.
- Mixed-width arithmetic and register loads need upper-byte checks; textual
  pattern matching provides no register liveness or width proof.

Upstream's pattern data is distributed under the following license:

MIT License

Copyright (c) 2025 ZERICO2005

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
