# Constant register-pair multiplication

The GlobalISel legalizer expands nonnegative constant multiplication for native
16-bit arithmetic and eZ80 ADL 24-bit arithmetic. It uses binary Horner form:
shift the accumulator by one and add the original operand when the next
constant bit is set. This requires at most the original value and accumulator.
`G_SHL` selects direct register doubling (`add hl,hl`); generic self-adds can
introduce unnecessary register copies through distinct operand classes.

Arithmetic wraps at the result width, so the lowering handles signed as well
as unsigned inputs. It does not propagate overflow flags to intermediate
operations. Both operand orders are supported. Proven unsigned-byte products
still prefer hardware `MLT` on targets that support it. Unknown operands,
negative constants and expensive sequences retain the runtime fallback.

The cost estimate counts two bytes for preserving the original value, each
ADD and the short-mode prefixes needed for 16-bit arithmetic in ADL mode.
Size functions (`optsize`, including `minsize`) use the constant-load plus
runtime-call budget: six bytes in native short mode and eight in ADL mode.
Speed functions permit twice that budget. O0 uses the size budget; legalization
therefore also handles multiply by three without depending on an optimizing
combiner. The existing pre-legalizer multiply-by-three rule remains intact.
Final register allocation and spills can affect actual size.

Examples for unrestricted inputs in ADL mode:

- 24-bit times 3, 5, 7, 9, 10, 15, 17 or 33: inline at both size and speed modes.
- 24-bit times 31, 65, 127, 255 or 257: inline in speed mode, fallback in size mode.
- 16-bit times 3, 5 or 6: inline at both modes.
- 16-bit times 7 or 9: inline in speed mode, fallback in size mode.

`-mllvm -z80-constant-mul=false` disables this legalization expansion for
comparisons. It does not disable the existing 24-bit times-three combine or
the independent `MLT` optimization.

Regression tests cover both operand orders, size/speed budgets, O0, native
short eZ80 and ordinary Z80, live inputs, negative/large constants, variable
multiplication and machine verification through legalization.
