# Scalar eZ80 optimizations

The target specializes native scalar operations in GlobalISel and performs
register-aware folds after pseudo expansion. These transformations complement
`Z80ByteArithmetic.md`, `Z80InternalRegArgs.md`, and `Z80TargetCosts.md`.

## Constant bitwise operations

The low and high bytes of a register pair are directly addressable. A wide
AND/OR/XOR that changes only one of those bytes extracts it, performs the byte
operation, and inserts the result into the original full-width value. All
untouched bytes are retained, including the hidden upper ADL byte. An AND with
a byte or word mask uses explicit zero extension to clear higher result bytes.
Other upper-byte masks retain their runtime helper.

For the frequent 24-bit mask `0x7fffff`, `(x << 1) >>u 1` clears the top bit.
The left shift is a native self-add and the right shift uses `__ishru_1`.
Both shifts operate at 24 bits, so the identity is exact for every input.
The caller sequence occupies five bytes instead of the eight-byte mask-load
plus generic AND call. It depends on the existing one-bit CRT helper.

Constant byte ALU operands use immediate instructions. Single-bit AND/OR use
RES/SET, which preserve flags and can avoid accumulator traffic. The final
pass also folds `LD A,r; RES/SET bit,A; LD r,A` when all aliases of A are dead.
Index half registers are excluded from that fold because RES/SET cannot name
them. `-z80-constant-bitwise=false` and `-z80-byte-immediates=false` independently
disable the legalization and byte-selection rules.

## Word arithmetic in ADL mode

Ordinary 16-bit ADD/SUB can operate on arbitrary extensions to 24 bits and then
read only the low 16 bits. Arithmetic modulo 65536 is independent of the upper
input bytes. The native operation avoids the short-mode prefix. G_ADD/G_SUB
results do not expose carry, signed overflow, or other machine flags; opcodes
that return carry/overflow retain their separate 16-bit lowering. Existing
INC/DEC and size-oriented negation rules remain independent.

One-bit left shifts also use the existing wider self-add/truncation path.
Identical ADD operands select the self-add instruction without allocating a
second pair. `-z80-native-word-arith=false` disables the new width choices;
existing multi-bit shift rules remain available.

The final pass folds `PUSH HL; POP rr; ADD HL,rr` into `ADD HL,HL` only when the
copy's destination and every alias are dead afterward. Widths must match;
frame setup/destroy, bundled instructions, and explicit memory operands are
excluded. Both ADD forms produce identical results and flags. The same-width
short form is safe because it retains the original final ADD's upper-byte
effect. Other existing short exchange/copy folds keep their width safeguards.

## Verification and policy

Handwritten LLVM tests cover ADL, native short mode, comparison switches,
carry-producing operations, full-width result definitions, alias liveness,
frame and volatile guards. Runtime tests use CEmu with randomized incoming
registers and upper argument bytes, returned values, live operands/stores,
write fences, IX preservation, and balanced stacks. Tests of a transformation's
mathematical identity supplement execution of the final compiled code.

RAM fixture cycle counts depend on memory placement and the chosen CRT. They
do not establish whole-application speed. Some speed-oriented byte arithmetic
expansions grow modestly; size-oriented functions keep stricter budgets.
Private register calls can also increase register pressure. Matched LTO
application builds and calculator regression suites should accompany changes
in any of these policies.
