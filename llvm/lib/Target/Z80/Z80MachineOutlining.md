# eZ80 machine outlining

The ADL backend supports LLVM's late MachineOutliner. At `-Oz` it shares repeated
straight-line instruction sequences, replacing each occurrence with a native
four-byte CALL and appending one native RET to the shared body. The call has no
ordinary C ABI clobber mask: it preserves all registers/flags except those changed
by the original sequence. LLVM propagates the sequence's exposed register uses
and definitions to the call.

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
are eligible. Interrupt functions and, by default, linkonce_odr functions are
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

| Minimum sequence bytes | Raw app bytes | Savings vs 4d3200781844 | AppVars |
| --- | ---: | ---: | ---: |
| Disabled | 2,513,200 | 0 | 39 |
| 0 | 2,205,409 | 307,791 | 34 |
| 16 | 2,471,615 | 41,585 | 38 |
| 24 | 2,489,947 | 23,253 | 39 |
| 32 | 2,497,298 | 15,902 | 39 |

38 AppVars require a raw app of at most 2,478,813 bytes: 65,232 bytes per segment,
including a three-byte overall length trailer. Segment capacity and app features
were unchanged. Counts of outlined calls in assembly are static sites, not
execution frequency or a speed measurement.

The 16-byte build passed nine direct-install CAS checks with pixel-identical
screenshots, all ten Matrix CE assertion suites, and all 36 libtexce rendering
CRC suites. Six emitted MIR fixtures executed with 65,536 randomized states each,
with and without outlining: 786,432 executions preserved all observable
AF/BC/DE/HL/IX/IY register values, both stack views, and fenced memory, including
short-exchange upper-byte behavior. In this RAM fixture environment one CALL/RET
pair added 43 CPU cycles (e.g. 93 -> 136 and 137 -> 180); flash execution and
whole-app overhead must be measured separately. This is a size optimization,
not a speed optimization.
