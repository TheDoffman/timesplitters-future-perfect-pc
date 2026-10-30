# Independent runtime replacement

Current kernel work has original providers for [pool allocation](../agent-context/kernel-pool.md),
[counted strings](../agent-context/kernel-strings.md),
[critical sections, status and diagnostics](../agent-context/kernel-rtl.md), and
[narrow guest formatting](../agent-context/guest-printf.md) through patch0239.
The complete old native RTL module is excluded from the build. Guest exceptions,
charset conversion, string allocation/freeing and numeric conversions remain,
along with other kernel, graphics, audio, input, title and fixture work.

Stage0239 passed the Windows Release build, selected local checks, both-host CI
and bounded isolated Scotland movement/fire/pause/resume checks. Exact source, CI, build and gameplay evidence is recorded by stage in
[STATUS](../project/STATUS.md). Earlier acceptance paragraphs below describe
those stages, not current whole-project or cross-platform gameplay acceptance.
The runtime checkout and bundled snapshot are still production dependencies;
final builds/generation/packaging with both unavailable have not passed yet.

Unused POSIX file-map/query/statistics APIs and their registry are retired in
patch 0214 after production callers moved to project providers. Native alias
fixtures retain their assertions through those providers; real Windows APIs
remain in host adapters. See [audit and limits](../agent-context/retired-memory-compatibility.md)
and MAC-117. Windows build, isolated Story and all 998 compiler checks passed;
all 73 migration/repository and 21 final documentation checks passed.
Exact-commit CI remains pending.

Main guest maps, physical aliases and Mac arena/MMIO ownership now have original
providers through patch 0213. Failed setup retains cleanup ownership, and
published mappings live through process exit until whole-runtime worker
quiescence can be proved. See [contracts and validation scope](../agent-context/guest-host-mappings.md)
and MAC-116. Windows build, 998 compiler tests and isolated Story checks passed;
73 migration/repository checks also passed. All 17 CI checks at `f0633f99` passed.

Native Nt host callbacks and native Mm protection now use patch 0212. Original
Darwin reservations replace leaking size-zero release and fabricated queries;
Windows retains real OS calls. See [ownership, commitment and failure limits](../agent-context/native-virtual-memory.md).

All 25 guest Mm bridges and five remaining native statistics/physical/hint
functions now use project policy through patch 0211. Complete guest output
validation and initialized host snapshots replace unsafe statistics access.
See [contracts and retained advisory limits](../agent-context/guest-memory-services.md).

Native Mm allocation/release, stacks, protection and region queries now use
project ownership and OS page adapters through patch 0209. Guest Mm/statistics
continue in 0211, main host mappings in 0213; other services remain work. See
[contracts](../agent-context/native-memory.md); full independence is not complete.

Guest Nt virtual-memory calls and native pointer-width allocate/free/query now
use project services through additive patch 0208. Existing flat guest VM policy
is explicit, complete output spans are validated before side effects, and exact
reservations can be released. Native host operations continue in 0212; main
guest mappings continue in 0213 and other kernel services still require
replacement. See [contracts](../agent-context/virtual-memory.md)
and MAC-112; this is not whole-runtime independence.

Main/private thread records and guest stack ownership now use
`src/runtime/guest_thread_memory.c` through additive patch 0207. Published
identity storage retains the guest-memory lifetime; host scheduling and object
reference counting remain separate work. See [thread contracts](../agent-context/thread-memory.md)
and MAC-111 for validation scope.

Guest heap/reservation/physical allocation now has project ownership in
`src/runtime/guest_allocation.c`, installed by additive patch0206. The bridge
validates fixed/ranged requests before zeroing. Host mappings and TIB/thread
construction now have project providers; other kernel services remain inherited. See
[allocation contracts](../agent-context/independent.md#guest-allocation-ownership)
and MAC-110 for measured acceptance and limits.

The native XBE loader now has project inspection/materialization and bootstrap
validation in `src/runtime/guest_image.c`, installed by additive patch 0205.
It removes inherited image parsing/copying and title decoding while keeping
host mappings and TIB construction on the subsequent project providers;
kernel/device services remain transitional. See
[image contracts](../agent-context/independent.md#native-image-loading) and MAC-109.

Guest architectural storage and optional ABI diagnostics now have project
providers in `src/runtime/guest_state.c` and `guest_abi_diagnostics.c`, installed
by `0204-independent-guest-state.patch`. Memory mapping, loading and thread/TIB
storage now have subsequent project providers; host scheduling remains transitional. See
[state and reporting contracts](../agent-context/independent.md#guest-state-and-abi-reports)
and MAC-108 for behavior, tests and acceptance boundaries.

Native nonlocal jumps now have a project continuation provider, installed by
`0203-independent-nonlocal-jumps.patch`. Direct native and emitted-call contracts
are separate from title regression checks because the supported-US translation
currently detects neither CRT helper. See [service boundaries](../agent-context/independent.md#native-nonlocal-jumps)
and MAC-107 for acceptance, including the Windows foreign-stack adapter and
pending native Mac validation.

The project now owns `src/runtime/recomp_types.h`, used directly by default
generation. Previously replaced execution primitives remain; the remaining
header service interfaces and target-admission support have original code and
contract checks. An additive patch supplies the same compatibility header to the
transitional runtime. Declared service bodies, title integration and final
toolkit-unavailable production acceptance are still incomplete (MAC-105).
The optional feedback header, recording service and guard-counter updates now
use project-owned implementations in `src/runtime/tsfp_branch_feedback.h` and
`guest_branch_feedback.c`, through additive patch `0202-independent-branch-feedback.patch`.
Generation publishes the supplemental header too. See [recorder contracts and
validation boundaries](../agent-context/independent.md#guest-branch-feedback-recorder)
and MAC-106; the larger runtime-service replacement remains incomplete.

The objective is to replace the code taken from `ts3FPrecomp` with this
project's implementations. Standard dependencies such as SDL, FFmpeg,
Capstone, the compiler and host graphics SDKs remain allowed. Copying upstream
files into this repository does not meet that objective. The bundled snapshot from PR #369 supplies interim offline availability;
it does not count as original replacement code.

The vector-support stage (`0197-independent-vector-support.patch`) removes the
obsolete inherited MMX/SSE helper bodies now superseded by the project emitters.
The live model-transform constructor has an original bit-preserving body.
The scalar-support stage (`0198-independent-scalar-support.patch`) also retires
obsolete numeric helpers and emits MOVSX/MOVZX directly from project code.
Existing rounding/classification assertions test real instruction emission;
direction and extension contracts have independent compiled oracles. FP environment, diagnostics, storage, kernel/device services
and title integration remain. Acceptance is tracked in MAC-103 and MAC-104.

**The replacement is not complete.** The current game still executes using the
pinned runtime and its patches. Disc import and executable analysis now have
independent implementations, as do the disassembler's image view and instruction
classification, traversal, direct-edge reachability, table/body recovery,
function discovery, labels, cross-references, analysis orchestration/output/cache,
the library-symbol adapter, generation driver, function classifier, manual-source
scanner, generation output support, translator instruction/block decoding,
executable layout configuration, HLE symbol planning/thunk emission,
indirect-branch feedback, unresolved-address diagnostics, C operand emission,
basic data-transfer/stack lowering, scalar integer arithmetic/logic, shifts/rotates,
multiply/divide lowering, non-LOCK bit tests/scans/byte swapping, and
exchange/compare-exchange lowering, comparison snapshots and implicit sign
extension, string-instruction lowering, condition expressions, SSE/MMX/x87
emission, control transfers, instruction/block lifting and lifter context,
helper recognition, naming, batch/output orchestration, translator entry discovery
and complete C function assembly and command-line orchestration.
Do not present the port as an original implementation of the complete Xbox
runtime or recompiler.

## Active goal and completion gates

The user requested completion of the remaining replacement as an active goal
on 2026-10-26. Work proceeds through the milestones below without requiring a
new request for each component. Keep each accepted stage reviewable in Git;
the goal does not authorize merging into main. Host validation remains separate
from implementation status.

1. **Own the tooling.** Finish instruction traversal, table/prologue recovery,
   function discovery, labels/cross-references, analysis caches/output, symbol
   integration and x86-to-C translation. Retain permitted Capstone; verify
   integer/flags, floating point, indirect calls and ABI behavior.
2. **Own the guest execution foundation.** Replace memory, executable loading,
   dispatch, kernel objects/imports, threads/TLS, timing, filesystems and saves.
3. **Own the device services.** Replace graphics/GPU commands, audio/movies,
   input/devices and networking around permitted host libraries/adapters.
4. **Own the title and delivery integration.** Replace title entry/hooks/build
   targets, migrate patches into original source, and update packaging/tests.
5. **Prove independence.** Audit executable and shipped source dependencies,
   fixtures, headers and historical copied text. Generate/build/test/package
   with the old toolkit and vendored snapshot unavailable. Retain notices for
   any third-party material still shipped. Verify isolated game behavior and
   record unavailable host checks accurately before assessing completion.

The goal stays active while required implementation or acceptance work remains.
Finishing a component, passing a build or exhausting a work session is not
completion of the whole goal. The table below and docs/project/STATUS.md record progress.

## Replacement sequence and acceptance

| Area | Current replacement status | Evidence required before removing the old implementation |
| --- | --- | --- |
| Disc catalog and extraction | Implemented in `tools/compiler/disc_image.py` and `tools/build/import-game.py`; Windows Extract uses it, same Python CLI available on Mac | Format fixtures, bounds/cycles/names, output preservation, private supported-XBE fixture; full original ISO extraction on both hosts remains unverified |
| Executable parser and analysis model | Implemented in `tools/compiler/xbe_image.py`; production generation uses the compatibility bridge | Synthetic valid/malformed images, verified-US metadata equality, regeneration and bounded gameplay; native Mac validation pending |
| Disassembler input and instruction model | Project-owned `recomp_image.py` and `x86_decode.py`; production consumer uses them with permitted Capstone | Mapping/operand fixtures, cache invalidation, full regeneration and bounded gameplay; native Mac pending |
| Instruction traversal and reachability | Project-owned `x86_flow.py`; production engine inherits scan, realignment, reachability and queries | Synthetic edges/cycles/overlaps/limits, existing consumer tests, full regeneration and bounded gameplay; native Mac pending |
| Table recovery and body/entry probes | Project-owned `x86_recovery.py`; production engine is now a compatibility import | Original table/shape/bounds fixtures, existing consumer tests, full regeneration/build and isolated gameplay; native Mac pending |
| Function discovery, labels and references | Project-owned `x86_functions.py` and `recomp_symbols.py`; production uses compatibility imports | Original discovery/metadata fixtures, existing consumer tests, semantic function/xref comparison, full regeneration/build and isolated gameplay; native Mac pending |
| Analysis orchestration, configuration, cache and output | Project-owned `analysis_runner.py`, `analysis_config.py`, `analysis_cache.py`, `analysis_output.py`; compatibility imports serve production | Standalone XBE-to-JSON/assembly CLI, content-verified cache, seed/section/publication fixtures, generation/build and isolated gameplay; native Mac pending |
| Library-symbol adapter and generation driver | Project-owned `xdk_catalog.py` and `compile_pipeline.py`; production compatibility entries use them | Protocol/seed/routing/failure fixtures, actual symbol and generated-code comparisons, Windows build and isolated gameplay; native Mac pending |
| Function classification | Project-owned `function_catalog.py`, `function_evidence.py`, `function_categories.py`; production calls the standalone catalog | Bounded operands/xrefs, source/context/graph hypotheses, measured pointer-entry blocks, no guessed CRT names or function extents; generated executable-code comparison, build and isolated gameplay; native Mac pending |
| Manual-source scanning and generation output | Project-owned `c_source_scan.py` and `generation_output.py`; production uses compatibility entries | Lexical/preprocessor/declaration fixtures, standalone consumer imports, unchanged-output timestamps and failure preservation; regeneration/build/game checks; native Mac pending |
| Translator instruction/operand and block model | Project-owned `translation_decode.py`; production compatibility imports use it | Original operand/segment/bounds/CFG/restart/overlap fixtures, existing translator consumers, full regeneration/build and isolated gameplay; native Mac pending |
| Translator layout and HLE symbol bindings | Project-owned `translation_layout.py` and `translation_hooks.py`; production modules alias them | Metadata/bounds/replacement fixtures, lexical markers, unique bindings, return/register ABI checks, native generated-thunk execution, regeneration/build and isolated gameplay; native Mac pending |
| Branch feedback and unresolved diagnostics | Project-owned `branch_feedback.py` and `unresolved_report.py`; compatibility CLIs use them | Cumulative observations, saturation, overlaps, independent seeds, real section/backing metadata, standalone CLI and production consumers; generation/build/game validation in docs/project/STATUS.md, native Mac pending |
| C operand emission | Project-owned `translation_operands.py`; lifter imports replace its formatter bodies and tables | Native subregister/FS/address-width/floating-stack and production memory-macro tests, standalone copy, existing consumers, regeneration/build/game; native Mac pending |
| Basic data-transfer and stack instructions | Project-owned `translation_moves.py` supplies MOV, MOVZX, MOVSX, LEA, PUSH and POP | Native extension/hook/stack-alias/segment/trace contracts, standalone copies, existing consumers, generation/build/game; native Mac pending |
| Scalar integer arithmetic and logic | Project-owned `translation_integer.py` supplies ADD/SUB/AND/OR/XOR/ADC/SBB/INC/DEC/NEG/NOT | Native result/carry/snapshot and decoded-sequence contracts, standalone copies, consumers, generation/build/game; complete flag lowering remains separate |
| Shifts and rotates | Project-owned `translation_bits.py` supplies SHL/SHR/SAR/SHLD/SHRD/ROL/ROR/RCL/RCR | Native bit-step and Windows x64 hardware value/carry comparisons, no-op/carry routing, consumers and generation/build/game; complete flag provenance remains separate |
| Multiply and divide | Project-owned `translation_product.py` supplies MUL/IMUL/DIV/IDIV; shared `guest_integer_fault.c` stops invalid division before result writes | Width/alias/overflow and native x86 comparisons, decoded flag consumers, host fault adapter, generation/build/game; guest SEH and native Mac acceptance remain separate |
| Bit tests, scans and byte swaps | Project-owned `translation_bitops.py` supplies non-LOCK BT/BTS/BTR/BTC, BSF/BSR and BSWAP | Native width/offset/alias and x86 references, captured flag consumers, generation/build/game; atomic forms and full flag provenance remain separate |
| Exchange and compare-exchange | Project-owned `translation_exchange.py` and original template atomics supply XCHG/XADD/CMPXCHG | Compiled values/flags, x86 oracles, aliases, decoded consumers and contention; generation/build/game acceptance in STATUS.md, native Mac pending |
| Comparison snapshots and implicit sign extension | Project-owned `translation_compare.py` supplies CMP/TEST, self-test normalization and CBW/CWD/CWDE/CDQ | Compiled width/carry/memory-hook/state fixtures and decoded consumers; measured host acceptance in STATUS.md and MAC-087 |
| String instructions | Project-owned `translation_strings.py` replaces string and direction emission | Original native/decoded contracts and Windows generation/build/game acceptance passed; native Mac pending (MAC-088) |
| Condition expressions | Project-owned `translation_conditions.py` for captured flags and Jcc/SETcc/CMOVcc/LAHF | Native and mathematical oracles, decoded consumers; Windows acceptance below, native Mac pending (MAC-089) |
| SSE/SSE2 emission | Project-owned `translation_sse.py` emits lane operations and bit-preserving moves; existing XMM ABI remains | Portable/native oracles, decoded consumers, generation/build/game acceptance; native Mac title pending (MAC-090) |
| MMX emission | Project-owned `translation_mmx.py` emits packed integer operations without inherited MMX helpers | Mathematical/native/boundary/decoded checks, full generation/build/game; native Mac pending MAC-091 |
| x87 emission | Project-owned stack/operation emitter; existing binary64 stack ABI remains | Rounding/bounds/native fixtures and Windows title acceptance; native Mac pending MAC-092 |
| Control transfers and block lifting | Project-owned call/branch/table handling, dispatch and block condition propagation | Compiled/decoded/negative-control checks and Windows title acceptance; native Mac pending MAC-093/094 |
| Lifter context, helpers and naming | Project-owned class/registry, bounded helper recognition and SDK-derived name data | Standalone/bounds/compiler checks and deterministic generation; native Mac pending MAC-095 |
| Batch translation and dispatch output | Project-owned database loading, declarations, split publication, stubs and dispatch | Compiled search/allocation/override tests and production output comparison; native Mac pending MAC-096 |
| Translator context and entry discovery | Project-owned context, callback tables, split CFG ownership and switch-arm recovery | Bounded graph fixtures, compiled recovered function and generation comparison; native Mac pending MAC-097 |
| x86-to-C function orchestration | Project-owned function assembly, typed joins, stack captures and x87 accessors | Compiled hooks/stack/FP tests, private negative controls and production output comparison; native Mac pending MAC-098 |
| Translation command line | Project-owned selection, manual/HLE planning, generation and hook refresh; pipeline invokes it directly | Toolkit-free subprocess, compiled emitted output and failure-preservation tests; native Mac pending MAC-099 |
| Guest memory, executable loader and kernel | Still upstream | Guest address-space ownership, protection, imports, threads/TLS, synchronization, filesystem and saves; boot and isolated save/load on Windows and Mac |
| Graphics and GPU command handling | Still upstream, with project adapters/patches | Own resource/state/command model, texture formats, shaders, draw/clear/present paths; asset-free GPU tests and representative captured/live scenes on both hosts |
| Audio and movies | Still upstream, with project integration | Own Xbox-facing buffer/voice/clock services and movie adapter around permitted libraries; decode, mixing, resampling, looping, synchronization and pause/resume |
| Input, devices and networking | Mixed project and upstream code | Own Xbox-facing controller/device/network services using shared PC input and permitted SDL; hotplug, multiplayer and timing contracts |
| Title startup, hooks and builds | Still upstream title target plus project patches | Own entry/dispatch integration and title configuration, migrated patches, native builds and bounded gameplay; generated game code stays private |
| Final removal and source audit | Not started | Build/generate/test/package with the old checkout and snapshot unavailable; remove obsolete patch context and copied fixtures; audit runtime source, tests, headers and packages, retaining notices for any code still shipped |

This order keeps the current port usable while replacements gain evidence. A
component is finished only when a production entry point uses the replacement,
tests verify behavior, and that component can operate with the old source absent.
Whole-project independence requires all applicable rows, not merely successful
compilation, renamed symbols or removal of attribution links.

## Disc importer: implementation and provenance

The new Python reader was authored for this project from published binary-format
descriptions. It imports only Python standard-library modules. It does not import,
copy or patch `tools.xiso`; it runs from a directory containing just the two new
Python files. The constants describe the file format, not an upstream code API.

References:

- [XboxDevWiki volume descriptor](https://xboxdevwiki.net/XDVDFS).
- [Xbox-Linux filesystem layout](https://multimedia.cx/xdvdfs.html).
- [XISO Studio format reference](https://purelogiccode.github.io/XISOStudio/XDVDFS-Technical-Documentation.html).

Validation is in `tests/compiler/test_disc_image.py`. The Windows driver test constructs
a disposable project with no external checkout or virtual environment. Public
tests use original synthetic bytes. The optional private-XBE fixture packages
only the user's existing supported executable and synthetic payloads into a
temporary image; this is not evidence of a full retail-disc extraction.

The original ISO recorded in this Windows checkout is no longer present. Keep
the existing `private/game`; do not create another full extraction merely to
fill that evidence gap. The importer has a read-only verification command for
when an original ISO is available.

## Current commands

### Library symbols and generation orchestration

```sh
python tools/compiler/xdk_catalog.py private/game/default.xbe --cli /path/to/XbSymbolDatabaseCLI --extra /path/to/title-symbols.json
python tools/compiler/compile_pipeline.py private/game/default.xbe --runtime external/ts3FPrecomp --work-dir private/pipeline --project external/ts3FPrecomp/titles/futureperfect --game-name "TimeSplitters: Future Perfect" --trace-all-entries
python -m pytest tests/compiler/test_xdk_catalog.py tests/compiler/test_compile_pipeline.py -q
```

`xdk_catalog.py` owns parsing the scanner's public line protocol, exact CLI
selection, supplemental-record validation and merging, and atomic JSON output.
[XbSymbolDatabase](https://github.com/Cxbx-Reloaded/XbSymbolDatabase) remains a
separately MIT-licensed dependency for signature matching, credited to its
contributors; its source and license remain required. It is not claimed as
project-owned code. The adapter was authored against its CLI emitter protocol
in `src/cli/main.c`, not by retaining the old wrapper implementation.

`compile_pipeline.py` owns stage selection, per-title seed lookup, output paths,
manual-source exclusion, diagnostics and subprocess execution. Parsing, analysis,
classification and lifting all call project scripts directly. The selected runtime
tree still supplies title data and hook sources; the shared C types header is
owned in `src/runtime/recomp_types.h`. A custom `--json` path is passed to disassembly rather
than relying on a sibling file. Missing requested seed files and invalid images
fail before execution; dry runs can describe a missing image without writing.
The driver stops on a failed stage and reports where to resume.

Production compatibility entries preserve Windows/Mac build-driver calls and
runtime dependency locations. Both new sources and the adapters enter the
regeneration hash. Existing supplemental symbol/seed data and upstream patch
history still require the final provenance audit. Native Mac acceptance is
pending under MAC-072.

### Complete executable analysis

```sh
python tools/compiler/analysis_runner.py private/game/default.xbe -o private/pipeline/disasm --force
python -m pytest tests/compiler/test_analysis_runner.py tests/compiler/test_analysis_cache.py -q
```

The runner uses only project analysis modules, the separately credited ABI table,
Python and permitted Capstone. It can operate without the toolkit directory.
Production `tools.disasm` entry points are compatibility imports supplied by
`0110-independent-pipeline.patch`. This does not yet remove the generation/runtime
checkout dependency or upstream text in historical patches and test fixtures.

Configuration derives addresses from the selected executable. Optional metadata
must agree with it. Explicit extra sections are treated as code by decoding and
entry probes; missing/empty requested sections fail before output publication.
JSON retains the function/reference/label/string/summary interchange contract;
human assembly listings use a project format and retain full instruction bytes.

Cache receipts verify input, implementation, normalized options and output bytes.
Observed seed status participates in the key. `--force` recomputes;
`--stats-only` does not write outputs. Files are individually replaced atomically;
failed publication invalidates the receipt, so mixed results cannot become a
cache hit. Native Mac validation is pending under MAC-071.

### Disassembler input and instruction model

`tools/compiler/x86_flow.py` now owns the next layer: linear scanning across valid runs,
bounded realignment at an explicit address, finite reachability over cached
direct edges, overlap/range queries and direct/indirect call-reference queries.
Calls retain fall-through; unconditional jumps and returns stop it. Missing
decodes and unknown indirect jump targets end a path, without inventing code.
Realignment preserves overlapping streams and stops at a known successor or
terminator. A zero/negative instruction limit performs no decode.

The production engine inherits those methods through
`0080-independent-disasm-traversal.patch`, after the input patch. The subsequent
`0090-independent-disasm-z-recovery.patch` replaces the remaining engine methods with
`x86_recovery.CodeRecovery`. The later `0100-independent-functions.patch` replaces
function-boundary discovery as described below.
The traversal source is included in both cache fingerprints. Its independent
tests are in `tests/compiler/test_x86_flow.py`; the consumer integration test asserts
that production traversal methods originate in the project module.

`tools/compiler/x86_recovery.py` owns bounded switch-table measurement, removal of
instructions overlapping proven table bytes, case/continuation realignment,
and read-only code-shape probes. Measurement handles displacement aliases,
negative indices and up to three unused leading slots. Backward extension is
bounded by the owning dispatch and a local target cluster so plausible pointer
bytes in an epilogue do not become table entries. Slots and destinations stay
in the same executable section; file-backed reads reject virtual zero fill.

Block-tail and extent queries share one boundary scan. Returning-body evidence
may continue through a measured switch arm; unknown indirect jumps do not prove
a return. These are lexical probes, not proofs of all-path control flow: internal
forward branches keep the lexical run, and only measured switch dispatches move
the probe cursor. General body evidence also accepts a tail jump. All probes
have byte/instruction bounds and preserve the cached instruction map. Exact
operand shapes recognize constant accessors, virtual dispatch and common entry
prologues; saved-register evidence reports pops before their corresponding save.

Original fixtures are in `tests/compiler/test_x86_recovery.py`. The production consumer
assertions verify recovery method origins. The standalone test executes recovery
without toolkit imports. Existing upstream consumer tests remain a compatibility
check and must be replaced/removed before the final fixture/provenance audit.

`tools/compiler/x86_functions.py` owns candidate discovery, body measurement, shared-tail
entries and the call graph. Ordered interval snapshots distinguish gaps from
owned bodies. Direct calls, installed callbacks, entry/prologue/padding evidence,
immediate references and corroborated pointer tables nominate entries. Separate
aliases keep callable tails without splitting their owning function. Bounded
tail rounds and a finite alias worklist propagate discovered targets. Body
measurement follows decoded extents and known branch/table destinations; it
stops at missing instructions or the next function/section boundary.

`tools/compiler/recomp_symbols.py` owns labels, string candidates and indexed references.
A label's evidence priority controls replacement. Duplicate names retain their
other address mappings when one address is renamed. Reference queries return copied list
containers, so list edits do not change the indices. Kernel references update the kernel
index on insertion. The legacy `data_read` schema denotes an absolute memory
operand, not a claim about read versus write semantics. Printable ASCII string
limits apply to the whole null-terminated run; overlong runs do not yield false
suffix labels. The supported-US comparison removed one 255-byte suffix of a
512-byte run, retaining all function and reference records semantically.

`0100-independent-functions.patch` replaces the former function/label/reference module
bodies with compatibility exports. Original tests are in `test_x86_functions.py`
and `test_recomp_symbols.py`; the standalone fixture and production class-origin
assertions cover integration. Configuration, analysis orchestration/cache/output,
symbol identification and translation are still upstream and remain goal work.

`tools/compiler/recomp_image.py` builds a read-only image from the independent XBE parser,
without requiring cached JSON or the external toolkit. Explicit or exactly named
sibling analysis JSON must match that executable's layout and imports; unrelated
JSON is never selected by a broad search. Headers use the actual header size.
Exact reads stay within a single file-backed region, rejecting virtual zero fill
and cross-region reads. Code probes accept a shorter window at a section boundary
instead of reading unrelated bytes that happen to follow it in the file.

`tools/compiler/x86_decode.py` supplies the instruction model, Capstone decoder setup and
operand classification used by production disassembly. It follows the
[Capstone Python detail/operand API](https://www.capstone-engine.org/lang_python.html)
with the existing pinned standard dependency. Direct transfers, absolute memory
references, address-like immediates and indexed table candidates retain the
consumer interface. A candidate is not proof of a function or table; recovery,
analysis orchestration, symbol identification and translation remain upstream.

`patches/compiler/0070-independent-disasm-input.patch` removes the old loader/model/classifier
bodies and connects the new implementations. It adapts code-probe reads and adds
the independent dependencies to the disassembly cache key. The build generation
fingerprint includes both the new source and the remaining disassembler source.
This prevents a changed reader/classifier from reusing stale analysis or C.
Deletion text in patches still records historical upstream code.

Tests: `python -m pytest tests/compiler/test_recomp_input.py -q`. Synthetic fixtures check
data reads, probe windows, stale JSON and x86 targets, including dynamic operands
that must not be treated as static addresses. A temporary directory containing
only the project modules and export data proves the input layer runs without the
toolkit. When the patched checkout is available, the test also runs its existing
97 disassembler tests against the new input layer. This stage does not replace
the full disassembler or recompiler.

### Executable analysis and disc import

Executable analysis uses the same project-owned reader on Windows and Mac:

```sh
python tools/compiler/xbe_image.py private/game/default.xbe --json private/game/default_analysis.json
python -m pytest tests/compiler/test_xbe_image.py tests/compiler/test_recomp_generation.py -q
```

The reader was authored from [Caustik's XBE format description](https://xbdev.net/fileformats/xbe/NonOfficialXBEFileFormat.pdf).
It validates file/virtual section extents, retail/debug entry and thunk decoding,
certificate metadata, library versions and TLS initialization ranges. Analysis
omits certificate secrets. It preserves the attributes and JSON consumed by the
current disassembler/recompiler; the recompiler remains an upstream implementation.
The supported title is the pinned US executable; synthetic debug images are not
evidence that other titles or beta executables work.

`tools/compiler/xbe_kernel_exports.def` is the unmodified CC0 export declaration table
from [XboxDev/nxdk at 03acd1415f7ccc3127bcfb629224aacdd95d783c](https://github.com/XboxDev/nxdk/blob/03acd1415f7ccc3127bcfb629224aacdd95d783c/lib/xboxkrnl/xboxkrnl.exe.def),
credited to Stefan Schmidt in its SPDX header. This standard ABI data is retained
as an allowed third-party dependency; it is not ts3FPrecomp parser code.

`patches/compiler/0210-independent-xbe-reader.patch` removes the installed upstream parser body
and routes its old module entry point to this reader. The two ordinal audit
consumers use the new table reader too. Build drivers set `TSFP_PROJECT_ROOT`
to the source checkout, including Mac builds sharing another checkout's ignored
dependencies. Direct toolkit commands can discover the containing project, or
must set that variable when dependencies are elsewhere. There is no fallback to
the old parser. The patch's deletion records remain historical upstream text.
The independent source, export data and bridge enter the generation fingerprint.
Windows automatically reruns all four stages when that fingerprint changes;
Mac continues to require `--generate` for stale generation.

Python 3.12 or newer is sufficient; Git, the runtime checkout and third-party
Python packages are unnecessary for disc import:

```sh
python tools/build/import-game.py --iso /path/to/game.iso --verify-only
python tools/build/import-game.py --iso /path/to/game.iso --output private/game
```

Windows also supports extraction before Setup:

```powershell
.\tsfp.ps1 Extract -Python 'C:\path\to\python.exe' -Iso 'C:\path\to\game.iso'
```

The executable must match the existing US SHA-256 pin, and `pak`/`xmv` must be
directories. Image cataloging and existing-file checks happen before writes.
Identical existing files are reused; conflicting files and path aliases stop
the operation. Unrelated files are retained. Completed files remain after a
handled interruption and can be reused on retry; incomplete temporary files
from handled failures are removed. Forced process termination can leave a
temporary file, which is never mistaken for a completed game file.

The destination must support hard links (ordinary NTFS/APFS project locations
do). Publication is exclusive, so a concurrent destination file is not replaced.
Unsupported filesystems fail without replacing existing files. Source images
are read-only, payload copies are bounded and verified, and imports check free
space. Personal settings and saves are outside the output path.

Run the focused checks with `python -m pytest tests/compiler/test_disc_image.py -q`.
CI runs them on Windows and macOS. Build/generation still need the existing
toolkit until subsequent replacements are completed.

## Function catalog: implementation and provenance

`function_catalog.py` coordinates project instruction/reference evidence and
category inference. It imports permitted Capstone through the own decoder and
uses the validated project image model. The generation driver calls it directly;
`0060-independent-classification.patch` replaces eleven upstream module bodies with
compatibility entries or retirement notices. Original tests include a copied
standalone catalog operating on a synthetic executable without toolkit imports.
Historical patch deletion text still awaits the final provenance audit.

Function ranges remain the analyzer's measured ranges. Decoded operand evidence
cannot mistake immediate payload bytes for instructions; xrefs in gaps have no
owner. Pointer arrays need three entries, two distinct targets, a known function
and bounded body probes for other targets. Only actual 32-bit immediate stores
support constructor hypotheses. Unlisted targets receive measured entry-block
records in a separate artifact, never estimated 32-byte function bodies.

Source paths preserve full module identity. Whole-word string evidence and calls
into actual named SDK sections give weak subsystem hints; ties remain unknown.
Graph inference requires two distinct unanimous neighbors in bounded snapshot
rounds. Scalar initializers require complete short SSE bodies with data reads
and writes. Numeric operation observations explicitly do not assert CRT names:
old prefix labels included a cosine body called a float-to-integer conversion
and a left-shift body called a right-shift helper. Correcting those diagnostic
labels must not exclude or substitute the original executable functions.

The catalog emits identified functions, source modules, an empty CRT-identity
list, pointer arrays, entry blocks and summary JSON. Inputs and output aliases
are validated before publication; files are atomically replaced and the driver
proceeds only on successful exit. There is no classifier cache. Own sources and
compatibility entries participate in generation fingerprints. Labels, scores
and array membership remain heuristic, not proof of source/library identity or
complete C++ recovery. docs/project/STATUS.md records actual host validation; MAC-073 keeps
native Mac generation and gameplay acceptance separate.

## Manual-source inspection and generated output

`c_source_scan.py` is a project-authored lexical scanner using only the standard
library. Comments and quoted literals cannot fabricate function bodies or
references. Token inspection distinguishes declarations from definitions by the
opening body brace. Literal conditional branches, nested directives, else/elif
and spliced lines are handled; unknown macro conditions retain possible branches.
This is not a complete C preprocessor: configuration-specific conclusions need
preprocessed input. Macro text contributes references without becoming a C body.
The current title preserves 138 definitions and 137 generated-body wrappers;
four references present only in comments/text are no longer reported.

`generation_output.py` owns translation summaries, optional inferred-ABI header
reports and atomic generated-file publication. The signature report does not
replace the executable translator's register-state ABI. Header inputs validate
names/counts and sanitize comment text. Unchanged decoded text retains existing
bytes and timestamps, including existing CRLF; new files use host-native text
newlines. Temporary writes/replacements preserve previous output on failures.

`0380-independent-generation-support.patch` replaces the manual scanner/output
modules and translator's text-writer body after existing flag-join patch context.
Standalone importlib and direct translator imports initialize own module paths
through the parser bridge. Original tests cover lexical decoys, preprocessor
branches, source wrappers, file failures/timestamps and signatures; existing
camera/crane/native-vehicle consumers exercise the integration. Both modules
enter generation fingerprints and shared CI. Historical deleted upstream text
still awaits final removal. docs/project/STATUS.md records measured Windows validation;
MAC-074 tracks native Mac acceptance without inferring it from shared source.

## Translator instruction and block model

`translation_decode.py` was authored for this project around permitted Capstone,
using the project decoder factory. It owns translator-facing operands (including
segment overrides, widths and signed displacements), instruction properties,
bounded linear and worklist decoding, and basic blocks. Linear restart anchors
partition supplied bytes so an instruction cannot straddle a known restart.
CFG traversal follows explicit branches and fall-through within the supplied
window; it does not infer indirect destinations or traverse callee bodies.

Blocks connect only decoded leaders. Missing byte ranges cannot create a
fall-through edge, and unreferenced bytes after a return do not establish a
new block. Independently decoded overlapping entries split at their
convergence instead of duplicating a shared suffix. Original synthetic fixtures
cover these behaviors, and copied modules run without toolkit imports. The
production compatibility patch replaces the complete old decoder body; own
source and bridge hashes invalidate generation. Historical patch deletion text
still awaits final provenance removal.

A broader existing-consumer run exposed a command-builder compatibility gap:
older callers omit the optional `json` attribute. The helper now accepts that
shape and preserves explicitly supplied output paths; the CLI resolves relative
paths before changing subprocess directories. Original tests verify both
contracts. docs/project/STATUS.md records actual generated-output/build/game comparisons,
nested consumer results/skips and limits. MAC-075 tracks native Mac acceptance;
core x86-to-C lowering and translator orchestration remain upstream and are
not claimed as replaced.

## Translator layout and HLE symbol bindings

`translation_layout.py` starts with an empty layout instead of a captured game's
fallback addresses. The project XBE reader supplies sections, entry and thunk
addresses. New layouts validate before replacing the previous snapshot; failed
reads preserve prior state. Missing data/text ranges and zero entry/thunk values
are explicitly reset. Virtual classification and file-backed reads remain
distinct, so BSS cannot read unrelated file bytes. Compatibility module identity
keeps existing mutable-state consumers and their imported helpers consistent.

`translation_hooks.py` owns runtime-hook marker discovery, symbol selection,
unique name/address binding, original-body retention and guest thunk generation.
It reuses the project lexical scanner: comments/literals and known inactive
branches cannot fabricate markers; unknown macro branches remain possible.
Exact duplicate symbols are one candidate, while ambiguous names, multiple
LTCG variants and conflicting implemented names at one address are refused.
Title-written bodies retain precedence. Return cleanup is consensus among
decoded near returns in the supplied span; it cannot establish code/data
separation or an external tail-call convention. Register-position name parsing
is strict, and emitted frames must fit the current runtime's 24-argument limit.

Original tests cover these rules, standalone copied modules, failed reads
and layout replacement. Native C tests execute generated plain/register thunks,
stack restoration, imports and original pointers with the production runtime's
ABI macros. Those macros and HLE service implementations remain upstream;
replacing the planning tool does not replace graphics/audio/device services.
`0130-independent-translation-bindings.patch` removes both old tool module bodies;
historical deletion text still awaits final provenance removal. Shared CI and
generation hashes include the own modules. docs/project/STATUS.md records actual regeneration,
build and game evidence; MAC-076 tracks pending native Mac acceptance.

Running the broader translator consumer suite on CI exposed stale C fixtures:
fast-entry defaults, result temporaries and indirect-dispatch declarations had
not followed the production ABI. `0710-translation-consumer-fixtures.patch`
repairs that scaffolding, accepts equivalent SAR sign-extension parentheses
while retaining compiled numeric checks, and applies the Windows-only onexit
header-collision negative control on its actual host. This does not alter
production generation. These inherited fixtures remain part of the final
replacement/provenance work; passing them does not make them project originals.


## Branch feedback and unresolved diagnostics

`tools/compiler/branch_feedback.py` owns cumulative target flags and per-site target sets.
Truncated dump lines are ignored; stored JSON is validated before publication.
Saturation persists, and site-only observations survive missing target dumps.
Repeated merges are idempotent. Files publish atomically one at a time, not as a
multi-file transaction; rerun a merge after an I/O interruption to finish the
pair. The tools are single-writer commands, not concurrent ledger services.

```sh
python tools/compiler/branch_feedback.py merge /path/to/icall_targets.dump
python tools/compiler/branch_feedback.py --functions private/pipeline/disasm/functions.json seeds --xbe private/game/default.xbe --out private/pipeline/observed-seeds.json
python tools/compiler/unresolved_report.py private/game/default.xbe --functions private/pipeline/disasm/functions.json --unresolved private/pipeline/recomp/unresolved_symbols.txt --out private/pipeline/unresolved-report.json
python -m pytest tests/compiler/test_branch_feedback.py tests/compiler/test_unresolved_report.py -q
```

Default ledgers are `private/pipeline/recomp/icall_targets.json` and
`icall_sites.json`. Existing legacy or other-title ledgers are never silently
imported: select `--db` before the subcommand and `merge --sites-db` explicitly.
The generation driver already reads the per-work-directory site ledger.
Seed export preserves observations and filters recorded interiors only with
explicit `--functions`; aliases at known starts remain eligible. Optional XBE
probing uses the own decoder/recovery implementation; alignment alone is only
a filter, not proof of executable code. Exporting seeds does not automatically
add them to the normal title configuration.

The unresolved reporter requires the selected executable and function map.
It reports real section flags, file backing, headers and unmapped addresses,
plus all recorded overlapping bodies and entry/end relationships. Empty inputs
are valid. A recorded end does not establish underestimated size; no guessed
function extent or addable-function file is emitted. Its versioned diagnostic
JSON replaces the old heuristic report schema. Input hashes identify evidence,
not a verified association between a user-selected map and executable.
Compatibility entries support module and direct-script invocation; standalone
copies run without the old toolkit. Runtime recording, indirect dispatch,
core translation and the final provenance cleanup remain separate work.


## Register and memory operand emission

`translation_operands.py` owns C expressions for guest general/subregisters,
integer immediates, FS-relative addresses, memory access and the existing
floating-stack storage. It generates subregister relationships from the register
model and preserves unaffected bits on writes. Native fixtures execute 1,000
input pairs across all 16 subregister forms, address arithmetic and floating-stack
wraparound. A separate fixture compiles the actual production XBOX_PTR and
integer/signed/float memory macros with emitted accesses and an isolated buffer.

The decoder now retains address width. For an address-size override, the offset
wraps to 16 bits before adding FS; normal guest addresses wrap through the
runtime's 32-bit pointer macro. Absolute negative displacements emit unsigned
32-bit literals, including high device addresses. This changes their C spelling,
not the runtime pointer result. Unsupported registers, sizes and malformed
address forms fail before C emission instead of producing unknown C tokens or
silently selecting a narrower memory accessor.

Architectural x87 register width is reported separately from the runtime's
existing double storage; this stage does not establish 80-bit arithmetic fidelity.
General 64-bit integer operands and 80-bit memory floating conversion are not
silently lowered to 32-bit accesses. Segment/system register reads retain the
existing zero/no-op policy; actual privileged-register emulation is separate.
The compatibility patch removes the old helper bodies and tables. Opcode/flag
handlers, naming/export data and remaining translator orchestration are still
upstream. Tests are in `tests/compiler/test_translation_operands.py`, registered on both
CI hosts; measured generation/build/game acceptance is in docs/project/STATUS.md.


## Data-transfer and stack instruction lowering

`translation_moves.py` owns MOV/MOVZX/MOVSX/LEA/PUSH/POP C emission. Shared
project load/store/after-store/push policy hooks and frame publication remain
explicit. Extension validates operand widths; invalid vector-to-integer MOVSX
fails before emission. The inherited test now requires that error instead of
an invalid assignment with a warning comment. No compiled valid-case checks
are disabled.

LEA computes an offset without adding a segment base. PUSH reads its source
before changing ESP. POP resolves memory destinations after the increment;
POP ESP then replaces ESP with the popped value, while POP SP preserves the
high bits after increment. Word-sized stack operations retain prefix/register
width information. Segment selectors keep the existing unmodeled policy.
Trace names use UTF-8 byte escapes, including quotes, control bytes and non-BMP
characters, without assuming JSON escapes are valid C string escapes.

Original native fixtures compile emitted operations with production macros,
checking title-hook effects/order, frame publication, all byte sign-extension
values, word memory sign extension, narrow register preservation, LEA with FS,
stack aliases/addresses, word immediates and trace bytes. Copies of the two own
modules run without the toolkit. The complete patch loader applies the body
replacement after older title instrumentation so recorded stacks can unwind
and replay. Fingerprints and both CI hosts include the source and tests.
XCHG, arithmetic/flags, control transfers, SIMD/FPU handlers, orchestration and
runtime ABI macros remain separate parts of the replacement goal.


## Scalar integer lowering

`translation_integer.py` owns the scalar ADD/SUB/AND/OR/XOR/ADC/SBB/INC/DEC/NEG/NOT
handlers. It validates general registers, operand widths and encodable operand
kinds, masks sampled inputs before carry/borrow calculations and publishes the
computed result to the existing snapshot interface. Memory results are not read
again merely to obtain flags. The existing byte-decrement title hook is retained.
INC/DEC leave carry alone, NOT leaves snapshots alone, and NEG uses unsigned
subtraction so INT_MIN does not invoke signed-overflow behavior in generated C.

Original compiled fixtures cover every byte input pair and carry, seeded and
boundary word/dword inputs, memory access counts and hook order. Decoded
NEG/ADC/SBB/PUSH/CMP/ADD/SUB sequences execute through production block lowering.
Two inherited expression assertions are updated for the new temporary variables;
negative checks for carry preservation remain active. Copied modules run without
the toolkit. The source participates in generation hashes and Windows/Mac CI.

The compatibility patch removes six old handler bodies after all historical title
instrumentation and the move replacement. Historical upgrade fixtures defer it
until the complete stack. This stage retains the existing flag snapshot ABI;
conditional lowering, complete AF/PF/OF behavior, other instruction families and
runtime services still require replacement. Generation/build/game evidence and
native Mac gaps are recorded separately in docs/project/STATUS.md and MAC-080.


## Shift and rotate lowering

`translation_bits.py` owns SHL/SHR/SAR/SHLD/SHRD/ROL/ROR/RCL/RCR emission.
Count masking precedes width handling, shifts publish computed results rather
than rereading the destination, and rotations preserve result snapshots. Carry
rotates use a width-plus-one ring without relying on the old runtime helper.
Zero counts leave emitted flag state unchanged. Word double shifts at count 16
are implemented; larger counts leave state unchanged as explicit policy for an
architecturally undefined result. Logical shifts beyond operand width use zero
carry; undefined flags are not claimed to reproduce a particular x86 core.

Original native tests compare all byte values and all 256 count encodings, plus
word/dword boundary and seeded values, with single-bit reference calculations.
Windows x64 additionally assembles actual x86 instructions with ML64 and compares
results and architecturally defined carry. These are value/carry tests, not a
claim of complete flag emulation. Copied modules run without the toolkit.

Known constant no-ops retain the preceding flag interpretation in block lowering
and carry discovery. Rotate/double-shift carry branches request and consume the
published carry. Carry rotates still declare carry at constant zero because their
guarded generated C references it. Dynamic zero-count conditions across setters,
overflow/parity/auxiliary flags and complete conditional lowering still require
replacement. Four inherited expression fixtures follow the new emitted forms;
compiled behavioral checks remain active. Hashes and both-host CI include the
module. docs/project/STATUS.md and MAC-081 separate implementation from host acceptance.


## Multiply and divide lowering

`translation_product.py` replaces the two multiply/divide handler bodies.
MUL/IMUL implement byte, word and dword implicit pairs and the explicit word/
dword IMUL forms. Signed products fit a widened intermediate before truncation;
CF and OF agree on whether the result fits. Other multiplication flags remain
undefined. Consumer routing supports carry/overflow branches and SET/CMOV forms.

DIV/IDIV sample the divisor before any result writes, including EAX/EDX aliases
and memory addressed through them. Quotients must fit the destination width.
Signed division uses unsigned magnitudes and sign restoration, including the
64-bit minimum divided by minus one, without invoking signed C overflow. Byte
results share AX; word results preserve the upper halves of EAX/EDX. Undefined
division flags retain the existing snapshots without asserting hardware values.

Zero and overflow faults preserve result registers, publish the local frame and
call `src/runtime/guest_integer_fault.c`. The adapter logs guest site/reason and triggers
the existing fatal host diagnostics (Windows noncontinuable integer-divide
exception; POSIX SIGFPE with abort fallback). It does **not** implement guest SEH
recovery; that remains execution-foundation work. Both title CMake host paths
link the same adapter. Original compiled tests check portable arithmetic,
Windows x86 instructions, aliases, memory-read counts, fault state, full decoded
consumer functions and the actual host adapter. Local/CI/native Mac acceptance
is recorded separately in docs/project/STATUS.md and MAC-084.

The preceding bit-stage CI exposed a stale inherited SAR statement-count/cast
assertion, skipped locally without Clang/GCC. The product-stage compatibility
patch retains the compiled width/count sweeps and negative controls, accepts the
whole emitted block, and routes native compilation through the project helper
so MSVC runs the same fixture. No production arithmetic change is involved.


## Bit tests, scans and byte swapping

`translation_bitops.py` owns non-LOCK BT/BTS/BTR/BTC, BSF/BSR and BSWAP.
Inputs and memory addresses are sampled before writes. Register/immediate indices
wrap at operand width; memory/register indices select a signed bit string using
unsigned arithmetic, including negative offsets and wrapping guest addresses.
BT and modifying forms publish the tested bit once to CF. Carry branches,
SET and CMOV consume that snapshot even when intervening moves overwrite inputs.
Scans publish ZF from the sampled source and preserve the destination on zero as
an explicit policy for an architecturally undefined result. BSWAP accepts dwords.

The specification basis is Intel SDM volume 2A (BT through BTS, BSF/BSR, BSWAP):
https://cdrdv2-public.intel.com/812383/253666-sdm-vol-2a.pdf.
Original native fixtures cover all word values, seeded dwords, aliases, signed
memory offsets, immediate wrapping and decoded consumers. Windows ML64 references
compare defined register/memory results and flags with actual x86 instructions.
Copied modules execute without the toolkit. Inherited textual fixtures follow the
new emission while preserving behavioral checks. Generation fingerprints and
Windows/macOS CI include the module; see docs/project/STATUS.md and MAC-083 for acceptance.

The additive compatibility patch follows product lowering and retains unrelated
instruction dispatch. LOCK-prefixed operations remain in the existing atomic
handler for a separate replacement stage. Complete flag provenance, preserved
flags across different setters, guest memory faults and other instruction/runtime
families remain separate work; this stage does not establish full independence.


## Exchange and compare-exchange

`translation_exchange.py` owns XCHG, XADD and CMPXCHG, including LOCK spellings
on memory. Original byte/word/dword atomic helpers in the runtime template use
MSVC intrinsics or Clang/GCC sequentially consistent operations. Existing
RECOMP_ATOMIC_ADD32/CAS32 names now alias these implementations. This removes
the previous helper bodies without replacing the rest of the upstream header.
Memory XCHG is implicitly atomic. Unlocked memory XADD/CMPXCHG preserve the
existing stronger host-atomic policy rather than modelling every guest race.

Operands and effective addresses are sampled before writes. XADD writes its
source before its destination, including overlapping subregisters; CMPXCHG
compares the original accumulator and only replaces it on failure. Captured
result, signed inputs and carry drive conditional branches, SET and CMOV;
zero-flag joins use the result snapshot. Other instruction flag provenance and
a complete architectural flags register remain separate work.

Original fixtures compare all byte input pairs and seeded word/dword values
with portable references and Windows x86 instructions, then exercise aliasing,
mapped-address sampling, decoded consumers and four-thread contention. ADD
return-value sums, CAS increments and exchange-protected counters check the
actual helper bodies. Host atomics follow their aligned-RAM contracts; guest
MMIO, alignment/page-crossing faults, exception delivery and other LOCK families
remain separate foundation work. The source basis is Intel SDM instruction
semantics and standard compiler atomics, not copied lowering implementation:
https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
and https://learn.microsoft.com/en-us/cpp/intrinsics/interlockedexchangeadd-intrinsic-functions.
See STATUS.md and MAC-086 for measured Windows and pending native Mac acceptance.


## Comparison snapshots and implicit sign extension

`translation_compare.py` replaces CMP/TEST operand capture, self-test
normalization and CBW/CWDE/CDQ emission, and adds CWD. Operands are sampled
at their actual width, with signed/unsigned snapshots and carry exposed through
the existing consumer ABI. Direct dword-global CMP retains the title load hook;
other widths, indirect and segmented accesses retain normal memory reads.
Register self-tests normalize to compare against zero for existing flag joins.
Unsigned masks implement implicit sign extension without signed shifts. The
previously unhandled CWD form preserves upper EDX; all four forms preserve flags.

Original compiled tests cover all byte comparison pairs, every word input for
extension, seeded wider values, memory-read counts, hook scope and preserved
state. Full decoded consumers test operand overwrite, carry into SBB, normalized
joins and operand-size prefixes; Windows x86 oracles also compare extension
values and preserved flags. Inherited translator fixtures stay enabled.
The semantics basis is Intel SDM volumes 2A/2B:
https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html.
This stage replaced snapshot/emission bodies. Fused CMP/TEST branches already
consume captured operands without rereads. Condition expressions are replaced
in the later stage below; complete EFLAGS and guest faults remain work. See STATUS.md and MAC-087 for validation.


## MMX emission

MMX emission is owned by `translation_mmx.py` through the final
`independent-translation-mmx.patch`. It captures register bits and addresses,
uses unsigned masks and widened signed arithmetic for packed operations, and
guards full 64-bit shift counts. Saturation, packing, multiply-add wrap, masks,
all shuffle selectors and float conversions have independent compiled references
and Windows native MMX oracles. Low-unpack memory sources read only four bytes;
other packed sources read eight. Integer flags survive owned MMX instructions.
The emitter calls no inherited MMX operation helpers. Register declarations and
execution primitives are now in the project header; guest FP environment and
x87 alias/tag service ownership remain separate work. EMMS retains the explicit separate-storage behavior.
Standalone, boundary, decoded and hardware fixtures cover the new module.
Native Mac title acceptance is pending MAC-091.

The architectural reference is Intel SDM volume 2B, MMX packed operations,
including the m32 low-unpack operand forms. Tests retain their independent
mathematical and hardware expectations; the old helper is not the oracle.

## SSE emission

SSE emission is owned by `translation_sse.py` through the final
`independent-translation-sse.patch`. It samples vector registers and memory
addresses before writes, uses integer lanes for bit-preserving moves, and
preserves unaffected scalar lanes. Double operations use 64-bit lanes; packed
roots/reciprocals update every lane. Float-to-integer conversion checks bounds
before casting and distinguishes rounded from truncating instructions. Shared
numeric-policy hooks are retained. SSE compares publish captured floating
inputs; other SSE operations preserve integer flag provenance. Memory width,
all shuffle controls, conversions/rounding, NaN predicates, masks and decoded
consumers are covered by compiled references and Windows native SSE oracles.
Register declarations and memory-access primitives now use the project header;
guest address-space mapping and FP services remain transitional. Guest MXCSR,
floating exception status, alignment faults and
processor-specific reciprocal estimates are not claimed; exact division keeps
the existing reciprocal policy. Native Mac title acceptance is pending MAC-090.

## Condition-code generation

Condition evaluation is owned by `translation_conditions.py` through the final
`independent-translation-conditions.patch`. Jcc, SETcc, CMOVcc aliases and LAHF
consume captured comparison/result values. Arithmetic signed conditions use
SF xor OF; LAHF now includes defined auxiliary carry. ADC/SBB retain the source
and original left operand in the snapshot ABI. LAHF requests carry storage;
parity/overflow SETcc and CMOVcc dispatch use the shared alias map. Integer
native x86 and independent mathematical oracles cover all byte pairs and wide
edges, with operand overwrites; decoded consumers and NaN/float tests compile.
Unknown flags still return no expression. Complete EFLAGS, mixed setters,
zero-count shifts, caller flags and guest fault delivery remain work.
The architectural reference is Intel SDM volume 1, EFLAGS and condition codes,
and volume 2 instruction definitions (linked above). See STATUS.md and MAC-089
for acceptance evidence and host limits.

## String-instruction lowering

String lowering is owned by `translation_strings.py` through the final
`independent-translation-strings.patch`. MOVS/STOS/LODS and CMPS/SCAS use
byte/word/dword elements, direction-aware indices, address-size-specific count
and index updates, and FS source offsets. Bulk copy uses widened guest-span
bounds and disjointness; overlaps use ordered element reads/writes. Compare
operands are read once. Repeated compares retain the ZF/CF consumer ABI and
incoming zero-count preservation; single compares publish ordinary CMP
snapshots. The translator declares their required storage even without a
conditional consumer. SIMD MOVSD/CMPSD stay on their existing route.
Native compiled fixtures cover overlap, counts, directions, widths, FS and
16-bit wrap; Windows x86 oracles validate memory/register/ZF/CF results. Decoded
consumers and inherited carry/zero-preload negative controls run with both host
compilers. Complete repeat EFLAGS, memory protection/MMIO and resumable faults
remain execution-foundation work. See STATUS.md and MAC-088 for acceptance.

Source basis: Intel SDM, MOVS/STOS/LODS, CMPS/SCAS and REP instruction
semantics: https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html.


## x87 emission

`translation_x87.py` supplies original emission through the additive
`independent-translation-x87.patch`. Stack loads capture values before changing
TOP; stores and pop forms preserve operand order. Guest control-word RC selects
rounding, and integer stores check representable bounds before casting. Only
FILD32 retains the existing title conversion hook. External 80-bit memory forms
convert to/from the binary64 stack; FSCALE bounds finite exponents before the
host integer cast and defines infinite-scale cases. Compiled independent and
native x86 checks cover rounding, integer indefinite results, stack aliases,
status/compare bits, memory bounds and decoded production consumers.
The runtime stack storage, tags, exception state/delivery, precision control and
80-bit intermediates remain foundation work. Libm transcendental results and
result-only remainder handling preserve the current approximation, with no
hardware bit-exact claim. Unknown forms retain the standard diagnostic marker.
Source reference: [Intel SDM volume 2A, x87 instruction definitions](https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-2a-manual.pdf).
See STATUS.md and MAC-092 for acceptance and host limits.


## Instruction and block lifting

The original `tools/compiler/translation_lifting.py` replaces instruction dispatch,
basic-block condition propagation, comparison/branch fusion, result snapshots,
unsupported diagnostics and conditional-write fallbacks. The additive lifting
patch delegates nine functions without changing unrelated function bodies.
This module imports no upstream toolkit. Function-level orchestration, helper
recognition, runtime flag storage and the remaining C runtime are still separate
replacement work. Existing partial EFLAGS policies remain; this stage does not
claim full architectural flag emulation. See the owning context guide, STATUS.md
and MAC-094 for test contracts and host acceptance.


## Lifter context and host identifiers

The original `translation_context.py`, `translation_helpers.py` and
`translation_names.py` replace the remaining lifter class/setup, helper
recognition, naming and policy exports. The old module is now an import bridge.
The lifter's Windows API inventory is regenerated directly from SDK import
libraries by `generate_host_symbols.py`; it no longer reads bundled name data.
The checked-in identifier list and manifest record all 453 source-library
hashes, SDK version and output digest. Only factual identifiers are included.
Helper detection remains bounded heuristic matching; it does not supply guest
exception unwinding. Function translation/orchestration and runtime/title code
remain separate replacement work. See STATUS.md and MAC-095 for acceptance.


## Batch translation and output publication

The original `tools/compiler/translation_batch.py` replaces the complete old batch class
and certificate-title helper. It assembles metadata, selects categories/manual
implementations, publishes declarations/chunks/stubs and generates original
binary/optional-flat address dispatch. The later function and canonical-header
stages replace its injected translator and default header. Tests compile
the emitted C for wrapper/manual selection, stack cleanup, lookup misses/edges,
allocation failure and an empty table. Publication is atomic per file and
removes only obsolete outputs owned by the selected prefix. See the owning
[guide](../agent-context/independent.md), STATUS.md and MAC-096 for contracts and
host evidence. Historical patches and shipped fixtures remain audit work.


## Translator context and late entry discovery

The original `tools/compiler/translation_discovery.py` replaces sixteen FunctionTranslator
methods for context setup, bounded byte/table reads, callback recovery, split
CFG ownership, switch-arm entries, frame metadata and unresolved return cleanup.
The installed class binds these original methods while retaining its remaining
C emission/flag-join implementation. Synthetic graphs and a compiled recovered
function verify behavior, including rejection of mapped holes and incomplete
arms. Bounds and entry-flag heuristics do not establish complete symbolic
execution or architectural EFLAGS semantics. See STATUS.md and MAC-097 for
production generation/build/game evidence and native Mac validation limits.

## Complete function assembly

`tools/compiler/translation_function.py` replaces the remaining translator implementation.
It assembles decoded/lifted blocks, carries known flag descriptions across
compatible predecessors, emits required condition snapshots and local storage,
and preserves entry/exit/frame hooks. The old path is a module alias, preserving
production callers and diagnostic substitution without inherited implementation.
Original circular x87 accessor output evaluates push inputs before updating the
index. Failed indirect calls capture stack state above callee-cleaned arguments
or below caller-cleaned arguments while preserving frame saves.

Bounded three-pass flag propagation and eight-pass decoding resynchronization
retain the existing coverage; neither proves complete CPU/ABI behavior. The
remaining runtime services, title integration, historical patches and inherited
fixtures remain separate work. A standalone test copies only project modules
and emits a function with the toolkit absent; whole-game independence still
requires the remaining runtime and final provenance/removal gates.

## Original translation command line

`translation_cli.py` owns option parsing, input resolution, category selection,
single/header/batch modes, manual pin/wrapper planning, HLE integration and hook
refresh. `compile_pipeline.py` invokes it directly with an explicit runtime data
root. The compatibility `tools.recomp` entry only bootstraps and aliases the own
module; no implementation remains there. Runtime headers, title data and hook
sources are still dependencies. The historical snapshot and patch text remain
until final provenance/removal work; standalone tooling is not whole-game independence.

The default category set includes every observed game category plus unknown,
CRT and data initializers. Trace/force-return/SEH options and indirect-call
feedback retain per-run routing. Split output keeps manual names, wrapper entry
versus generated-body names, hook variables and actually emitted original bodies.
Hook cleanup reads a complete mapped function span. Refresh defers hooks whose
ordinary bodies already exist, recognizes manually provided wrappers, and refuses
missing dispatch definitions or a missing types header before writing either
output. It masks comments/strings, literal-disabled blocks and continued
preprocessor directives when reading generated definitions. Each output uses
atomic content-aware publication; the pair is not a filesystem transaction.

The legacy analysis-summary check compares basenames, not executable content;
the normal analysis cache separately checks content hashes. Invalid supplied
summary JSON now fails instead of silently bypassing that check. Conflicting
selection/output modes and negative limits are rejected; explicit mode flags
are otherwise retained. `--types-header` and `--hle-impl` support independently
supplied inputs. Original synthetic and compiled fixtures cover selection,
manual/hook dispatch, kept bodies, safe refresh, per-output feedback and a full
split-generation subprocess with only copied project modules. See STATUS.md
and MAC-099 for measured production acceptance and host limits.


## Guest register and scalar-memory primitives

The additive `independent-guest-abi.patch` replaces the register interface,
scalar RAM lvalues, partial-register writes and stack primitives with project
implementations. Existing symbol names retain binary/source compatibility with
runtime callbacks. This does not replace kernel storage, memory mapping, MMIO,
indirect dispatch or all remaining header arithmetic helpers. The existing
double-backed x87 stack and separate MMX storage remain explicit limitations.

Address conversion wraps guest arithmetic before applying host relocation.
Typed volatile lvalues explicitly support unaligned scalar aliasing on the
supported little-endian GCC/Clang/MSVC hosts. Partial writes sample each supplied
expression once; PUSH reads its source before adjusting the stack and POP updates
the stack before applying its destination. Standalone and production compiled
fixtures test these contracts, with independently broken variants and cross-file
thread-local callbacks. See `docs/agent-context/independent.md`, STATUS.md and
MAC-100 for ownership and measured Windows acceptance; native Mac title
validation and complete upstream removal remain pending.
