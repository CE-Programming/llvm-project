# Internal register arguments after LTO

The target pass `z80-internal-regargs` selects calling convention `cc136` for
eligible internal functions at the end of optimized full LTO and ThinLTO. The
public C ABI and the existing `fastcc` lowering stay unchanged.

`cc136` passes the first three native scalar arguments in HL, DE, BC (the full
24-bit register pairs in ADL mode). Smaller integer arguments are promoted as
in the C convention, with `signext`/`zeroext` respected. Return values, IX
preservation, flags clobbers and caller stack cleanup follow the C convention.
The lowering has ordinary stack fallback, but the automatic pass only selects
functions whose arguments all fit the three registers.

Eligibility requires a defined function with local linkage, C or `fastcc`,
one to three scalar arguments (i1/i8/i16, also i24 in ADL mode, or an ordinary
address-space-zero pointer), and a supported scalar or void return. Every use
must be a direct call with the exact function type and matching convention.
The pass rejects aliases, escaped function pointers, invokes, operand bundles,
varargs, interrupt/naked/optnone/returns-twice functions, musttail callers or
bodies, ABI-altering parameter attributes, and per-function mode overrides.
It changes the definition and every known call together. External library/OS
calls retain their original conventions.

No automatic changes are made in ordinary optimization or LTO prelink. To
compare against the former stack convention, pass:

```
-mllvm -z80-internal-regargs=false
```

Some CE projects implement LTO externally using `llvm-link` followed by
`clang -S -x ir`. Since that pipeline is ordinary optimization from LLVM's
perspective, opt in explicitly when compiling the merged module:

```
-mllvm -z80-internal-regargs-on-merged-ir=true
```

The master disable switch takes precedence over this opt-in. An explicit
`opt -mtriple=ez80 -passes=z80-internal-regargs` invocation is also available
for controlled IR/codegen comparisons. Register arguments can trade frame
traffic for register pressure; measure whole-program size and runtime for
applications with large callees or many values live across calls.
