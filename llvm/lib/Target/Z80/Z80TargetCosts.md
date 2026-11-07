# Z80 IR cost model

The target previously inherited a generic model with 32-bit scalar registers,
eight scalar/vector registers and almost indistinguishable costs for native
addition, full-width multiplication and shifts. `Z80TTIImpl` describes 16-/24-bit
scalar registers, four available pairs (conservatively reserving IX), and no
vector registers. The arithmetic and memory tables currently specialize eZ80;
ordinary Z80 arithmetic retains the generic fallback.

For code-size queries, local instruction sequences are estimated in bytes.
For speed queries, the unit is four RAM cycles: a native `ADD HL,rr` has one
unit, or four CEmu RAM cycles. A two-byte `MLT` adds four internal cycles, so it
has three speed units before operand packing. Native short calculations inside
ADL code include their mode-prefix byte where lowering needs it. Ordinary
16-bit add/sub and short constant left shifts can use native ADL arithmetic
and truncate, so their estimates omit those prefixes when enabled. CEmu models RAM reads/fetches with four
cycles and writes with two. The table rounds each local sequence up to whole
ADD units: a three-byte PUSH/POP copy takes 26 cycles (seven units), and a native
CALL/RET dispatch takes 43 (eleven units), excluding the callee body. Actual
calculator flash fetches and peripheral accesses have different timings. These are relative transformation
costs, not whole-function performance predictions.

The model handles native additions/subtractions, constant shifts, hardware
unsigned-byte products, constant multiplication under the existing size/speed
budgets, scalar register-indirect loads/stores and direct C/FastCC calls (both use
the public stack ABI). Known constant bit masks follow the byte/zero-extension
lowering; mapped 32-/48-/64-bit helpers retain conservative dispatch costs
rather than generic unit prices. The target lowering switches are shared with
TTI so comparison builds cost the sequences they actually select. Constant
multiplication includes the memory traffic of preserving the original value
with PUSH/POP. Calls account for argument words promoted/split by the native C
ABI and return-address traffic. Unknown addresses, vectors and types outside
the table use existing generic costs. Spills, frame setup, exact indexed
addressing, register pressure and runtime helper bodies are not predicted.
Software arithmetic gets a conservative CALL/RET dispatch cost; its operand-
and runtime-dependent body can be substantially more expensive.

Inlining retains the existing size policy, including explicit global threshold
settings used by KhiCAS. Speed callers pay an additional local call penalty
for native C argument/return-address stack traffic, scaled in the inliner's
five-unit instruction convention. Cold calls, unsupported signatures, and
register calling conventions retain the generic penalty. This changes the
tradeoff at each call rather than multiplying all inlining thresholds.

`-mllvm -z80-target-cost-model=false` selects the previous model for comparisons.
Handwritten regressions distinguish size and speed costs, short and ADL
arithmetic, and speed-only inlining at a fixed threshold.
