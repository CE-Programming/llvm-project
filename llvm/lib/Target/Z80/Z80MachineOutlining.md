# eZ80 machine outlining

The ADL backend supports LLVM's late MachineOutliner. At `-Oz` it shares repeated
straight-line instruction sequences, replacing each occurrence with a native
four-byte CALL and appending one native RET to the shared body. The call has no
ordinary C ABI clobber mask: it preserves all registers/flags except those changed
by the original sequence. LLVM propagates the sequence's exposed register uses
and definitions to the call. The helper RET explicitly keeps register results
and live-through state live for the target's subsequent address/peephole passes.

This policy is enabled by default only for `minsize` functions, as emitted by
`-Oz`. `-Os`, `-O2`, and `-O3` functions are not outlined by default. The default
minimum sequence size is 16 bytes. Both the minimum and LLVM's profitability
calculation must pass; repeated two-instruction fragments are not automatically
accepted. Costing uses actual instruction bytes, including mode/index prefixes.

## Tuning

Pass these through Clang's `-mllvm` option at final LTO/code generation:

* `-z80-machine-outliner-min-sequence-bytes=16`: default size/performance policy.
* `-z80-machine-outliner-min-sequence-bytes=24` or `=32`: fewer, longer sequences.
* `-z80-machine-outliner-min-sequence-bytes=0`: any profitable sequence; aggressive.
* `-outliner-benefit-threshold=N`: require at least N total bytes of estimated gain
  from a shared sequence across its static call sites (LLVM default: 1).
* `-enable-machine-outliner=never`: disable the pass.
* `-enable-machine-outliner=always`: also consider non-minsize functions, while
  retaining the target's safety guards and minimum sequence size.

Minimum sequence length is a better direct bound on per-occurrence overhead than
static benefit: a tiny sequence repeated many times can save many static bytes
but still be expensive when executed in a hot loop. Source profile information
can additionally drive LLVM's conservative profile-guided outlining mode; this
has not been measured on KhiCAS.

## Safety limits

Only native ADL functions without unwind/frame-move information or named sections
are eligible. Interrupt/naked functions and, by default, linkonce_odr functions are
excluded. LLVM also respects the IR `"nooutline"` function attribute.

Candidates cannot contain calls, branches, returns, inline assembly, bundles,
unexpanded pseudos, CFI, prologue/epilogue instructions, unmodeled side effects,
shadow-register exchanges, or accesses to SP, PC, R, I or MB. Descriptor implicit
operands are checked as well as actual operands. LLVM rejects function-local
block, constant-pool and jump-table references. Consequently the additional
three-byte return address cannot change SP-relative argument/frame accesses
inside a shared body. IX/IY-based frame accesses remain valid, since CALL/RET
preserve IX/IY. Outlining adds three bytes of temporary stack use.

Short-mode instructions retain their original encoding and upper-byte effects;
this pass does not cancel or replace short exchanges or copies.

## Controlled KhiCAS measurements

Pinned French merged IR (SHA256
`08d30d59cf71f435df54ae8f8dc4e844ca2bc44db8a18fdf527b264b7282b77d`), tuned
`-Oz -fmerge-functions -inline-threshold=-10`, private register args enabled,
and identical v15 runtime/packaging:

The corrected combined compiler also includes post-merge private argument
conversion and native tagged equality. The same-compiler disabled row isolates
outlining's size effect from those two changes.

| Compiler / minimum sequence | Raw app bytes | Savings vs 4d3200781844 | AppVars |
| --- | ---: | ---: | ---: |
| Previous branch, disabled | 2,513,200 | 0 | 39 |
| Current compiler, disabled | 2,501,720 | 11,480 | 39 |
| Current compiler, 0 bytes | 2,190,936 | 322,264 | 34 |
| Current compiler, 16 bytes | 2,458,136 | 55,064 | 38 |
| Current compiler, 24 bytes | 2,477,008 | 36,192 | 38 |

With the existing KhiCAS Makefile flags (without the explicit merged-IR private
ABI opt-in), the default 16-byte compiler build is 2,465,689 raw bytes / 38
AppVars. No KhiCAS feature or packaging change is required.

38 AppVars require a raw app of at most 2,478,813 bytes: 65,232 bytes per segment,
including a three-byte overall length trailer. Segment capacity and app features
were unchanged. Counts of outlined calls in assembly are static sites, not
execution frequency or a speed measurement.

The corrected 16-byte build passed all ten Matrix CE assertion suites and all
36 libtexce rendering CRC suites, both with and without the explicit merged-IR
private ABI opt-in. All nine CAS evaluator and wrapper calls returned for each
of the 0/16/24-byte builds; all 27 output regions matched the baseline pixels.
This is a smoke workload, not a frequency-weighted performance score or a full
KhiCAS correctness suite.

Twelve emitted MIR functions executed with 65,536 randomized states each,
with and without outlining: 1,572,864 executions preserved all observable
AF/BC/DE/HL/IX/IY register values, both stack views, fenced data and stack memory,
including short-exchange upper-byte behavior, live DE/F small-add outputs, live
A byte-mask outputs and live-through LEA temporaries. In this RAM fixture
environment one CALL/RET pair added 43 CPU cycles (e.g. 101 -> 144 and 117 ->
160); flash execution and whole-app overhead are separate measurements.

The full late-pass regressions explicitly retain observable outputs on the
shared helper's RET. A bare RET previously allowed address optimization to turn
`LD DE,17; ADD IY,DE` into LEA, changing caller-observable DE/F, and byte peepholes
to discard A results. The return use set includes original live-ins and every
physical definition; copied kill/dead flags are cleared to match that set.

Measured in CEmu on the authorized TI-83 Premium CE OS 5.3.1 ROM, the combined
16/24-byte builds reduced core evaluation cycles by 0.84-5.75% across the nine
smoke cases relative to 4d3200781844. The aggressive 0-byte build increased core
evaluation cycles by 2.12-8.66% and `do_eval` wrapper cycles by 9.04-22.20%.
The native comparison/private-call improvements offset conservative outlining's
cost; outlining itself is a size optimization, not a speed optimization.
Timing spans guest evaluator entry through its original return PC/SP, excludes
keyboard waits, and retains the real CEmu scheduler/interrupt behavior. Repeated
factor/gcd/powmod measurements were deterministic; hardware timing is unmeasured.

A same-compiler run with outlining disabled isolates the conservative policies:
core evaluator overhead is approximately 0-0.13% in these nine cases. The
aggressive policy adds 4.31-11.44% core evaluator cycles relative to the same
compiler without outlining. Read-only IRQ instrumentation confirmed that a few
negative conservative deltas (notably gcd) came from one fewer timer interrupt,
not intrinsic acceleration of the outlined code. Every measured interval was
repeatable, with no NMI/trap, and all 36 changed-app output comparisons (including
the disabled build) matched the baseline. The nine-case evidence does not imply
that hot loops in other workloads have the same overhead.
