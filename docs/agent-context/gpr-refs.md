# Live guest register references

`zz-stable-gpr-refs.patch` extends the common generator/template protocol.
New generated files define `RECOMP_GPR_REFS_SUPPORTED`; each translated body
declares `RECOMP_DECLARE_GPRS(site)` after entry callbacks and optional fast
returns. Legacy/manual files retain their old aliases. Unresolved stubs use
the globals directly. The patch applies after `zz-model-transform-entry.patch`;
historical fixture stacks without that entry hook omit the dependent patch.
Regenerate guest C with the supported build scripts; never edit generated C.

The title policy in `src/pc_native_numeric.h` enables references only for
the profiled model-node renderer18EA90, state setter21450 and state flush214C0.
Other sites select their existing global aliases. The template default is off
for titles without this policy. EBP and the existing SEH frame bridge are
unchanged. This applies to Original and native modes without changing clocks.

An eligible function retains seven individual constant pointers to the same thread-local
GPR cells. It does not retain their values: every operand still reads/writes
the actual cell, including partial-register operations. Nested guest/host calls,
callbacks and memory aliases therefore see updates immediately. References
last only for the current C invocation and are not shared across threads.
There is no actor/frame snapshot, deferred write-back or interpolation.

`test_gpr_refs.py` compiles real generated parent/child functions in legacy,
enabled, disabled and mixed parent/child modes. Four simultaneous threads
exercise integer/partial writes, guest-stack/data aliasing, nested calls and
callbacks that inspect the real cells. Results and intermediate-state digests
match the legacy mode. A mutation redirecting EAX to a copied value fails.
The deployment selector also checks all three sites and neighboring exclusions.
Tests run on Windows/macOS; Windows uses events/threads and POSIX uses pthreads
only inside the fixture. Clang permits the generator's fall-through block labels
with `-Wno-unused-label`; other `-Wall -Wextra` warnings remain fatal. Positive
and copied-cell controls must compile before their runtime outcomes are checked.
No runtime OS adapter is added.

The ignored original21450 MSVC microbenchmark compares the actual deployed
macro/after-trace declaration against direct global aliases. Six alternating
20-million-call rounds measure ratios0.920..0.938 (median0.928), with a100,000-call
final memory/nine-register comparison. An earlier struct representation was
slower (ratios1.045..1.074) and is superseded. Holding live cell addresses is
not sufficient by itself to guarantee faster compiler output. These are helper
CPU measurements, not game FPS gains, full model equivalence or Mac performance.
Live measurements and broader acceptance belong in STATUS before claiming an
experience improvement.
