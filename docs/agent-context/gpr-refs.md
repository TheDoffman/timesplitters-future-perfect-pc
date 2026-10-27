# Live guest register references

`0540-stable-gpr-refs.patch` extends the common generator/template protocol.
New generated files define `RECOMP_GPR_REFS_SUPPORTED`; each translated body
declares `RECOMP_DECLARE_GPRS(site)` after entry callbacks and optional fast
returns. Legacy/manual files retain their old aliases. Unresolved stubs use
the globals directly. The patch applies after `0410-model-transform-entry.patch`;
historical fixture stacks without that entry hook omit the dependent patch.
Regenerate guest C with the supported build scripts; never edit generated C.

Each function prefix defines `RECOMP_REGISTER_SITE` and reincludes the type
header. Its alias-selection section intentionally lives outside the type guard:
the preprocessor selects vector pointers for eligible sites and direct globals
for the rest, then undefines the site. This avoids growing conditional vector
expressions in unselected large bodies. The fixture checks global/selected/
global/selected transitions in preprocessed output, as well as runtime behavior.

The title policy in `src/gameplay/pc_native_numeric.h` enables references only for
the profiled model-node renderer18EA90, state setter21450, state flush214C0
and state preparation22BE0.
Other sites select their existing global aliases. The template default is off
for titles without this policy. EBP and the existing SEH frame bridge are
unchanged. This applies to Original and native modes without changing clocks.

An eligible function retains individual constant pointers to the same seven
integer and eight vector thread-local cells. It does not retain their values:
every operand still reads/writes
the actual cell, including partial-register operations. Nested guest/host calls,
callbacks and memory aliases therefore see updates immediately. References
last only for the current C invocation and are not shared across threads.
There is no actor/frame snapshot, deferred write-back or interpolation.

`test_gpr_refs.py` compiles real generated parent/child functions in legacy,
enabled, disabled and mixed parent/child modes. Four simultaneous threads
exercise integer/partial writes, vector/scalar lane operations, unaligned loads,
guest-stack/data aliasing, nested calls and
callbacks that inspect the real cells. Results and intermediate-state digests
match the legacy mode. Mutations redirecting EAX or XMM0 to a copied value fail.
The deployment selector also checks all four sites and neighboring exclusions.
Tests run on Windows/macOS; Windows uses events/threads and POSIX uses pthreads
only inside the fixture. Clang permits the generator's fall-through block labels
with `-Wno-unused-label`; other `-Wall -Wextra` warnings remain fatal. Positive
and copied-cell controls must compile before their runtime outcomes are checked.
No runtime OS adapter is added.

The integer-only21450 MSVC microbenchmark compares the then-deployed
macro/after-trace declaration against direct global aliases. Six alternating
20-million-call rounds measure ratios0.920..0.938 (median0.928), with a100,000-call
final memory/nine-register comparison. An earlier struct representation was
slower (ratios1.045..1.074) and is superseded. Holding live cell addresses is
not sufficient by itself to guarantee faster compiler output. These are helper
CPU measurements, not game FPS gains, full model equivalence or Mac performance.
Live measurements and broader acceptance belong in STATUS before claiming an
experience improvement.

The vector extension keeps pointers to the eight original RecompXmm unions;
partial writes retain their existing lane behavior and nested calls observe immediate
raw-cell updates. MMX/x87 aliases and their helpers are unchanged. A private
MSVC model code-generation comparison reduces static instruction lines from
3920/93 TLS-base reads (integer references) to3646/5 (integer plus vector).
Those figures describe the private conditional-expression prototype. The actual
preprocessor-scoped header measures3678 instruction lines/eight TLS-base reads;
its supported Windows build passes. Compiler-output counts are not dynamic cost
or live-game acceptance; see STATUS for current gameplay evidence.
See STATUS for current build/tests and unverified measurements.

The22BE0 extension is based on the complete unmodified generated translation
unit with normal compiler/policy/headers: the baseline target matches the
normal object outside typed relocations. Its selector-enabled code changes
8,774 to6,844 bytes and116 to93 static TLS-base loads. An isolated-body compile
does not reproduce the production inline context and is not the deployment
performance baseline. Private native text/string slices retain all instructions
and typed relocations; controlled out-of-line services compare memory, GPRs,
vectors, FPU/FP flags and callback/dispatch observations across four rounding
modes and six scenarios. Native timings are component evidence with mocked
services. Slices omit unwind metadata and must never enter the game build.
Normal-build, live-game and physical-host acceptance remain separate in STATUS.

Standalone translate_function callers may supply their own register globals and C prelude without the complete runtime header. The stable-register patch gates the reincluded header and live-reference declaration on RECOMP_GPR_REFS_SUPPORTED. Normal generated translation units still define that protocol and retain reference selection; standalone consumers retain direct globals. The inherited compiled INC/DEC, REP compare, sign-width and SAR tests exercise this compatibility, while test_gpr_refs.py checks the enabled protocol.
