# Independent tooling contracts

## Guest allocation ownership

`src/runtime/guest_allocation.c` / `guest_allocation.h` own heap, virtual
reservation and physical allocation metadata, with no toolkit headers or
host mapping calls. Live intervals are sorted by address; unowned gaps supply
lowest aligned fit. Heap and virtual reservations share ownership, preventing
their overlap when mappings extend above physical RAM. Freed records compact
immediately; no tombstones, untracked overflow allocations or split entries.

Heap requests have minimum size 16/alignment 4 and zero their returned size.
Reservations begin above physical RAM, default to 4096 alignment and neither
zero nor expose free/query support. Physical requests own page-rounded spans
but zero/query requested bytes; zero-size requests still own a page. Explicit
pins retain their requested base and reject live overlap before zeroing.
The synthetic kernel page is protected, the ordinary arena skips the low 1 MiB,
and the GPU instance tail stays reserved. Inclusive range bounds constrain
requested bytes; page padding may exceed the high bound within the usable
window. Invalid alignment, overflow, exhaustion and metadata-capacity failure
leave ownership and memory unchanged.

Physical iteration returns live allocations in address order; the APU consumer
keys its doorbell cache by address. Physical extent returns the highest owned
end minus origin, at least the ordinary arena floor. It must not become a sum
of live bytes: the GPU uses that extent to resolve physical surfaces. Queries
return remaining requested bytes from an interior address, excluding padding,
protected ranges and reservations. Exact-base releases do not alter contents.

Host atomics serialize metadata and allocation zeroing. Initialization requires
quiescent callers and rejects invalid/overlapping host storage without changing
the prior context. Stop disables requests before teardown; this does not own
or audit all guest-thread and host-map lifetimes. The provider changes allocation
placement/slack policy and rejects formerly overlapping or untracked claims;
it does not promise inherited address sequences or complete Xbox VM semantics.

Additive `0206-independent-guest-allocations.patch` removes inherited allocator
bodies, retains ABI adapters, starts ownership before device workers, and stops
it before mapping teardown. The kernel bridge replaces unchecked pin zeroing
with `xbox_ContiguousAllocAt`, rejects reversed/impossible physical ranges and
uses the bounded allocator for ordinary range requests. Shared kernel CMake
builds the same provider on both hosts. TIB/threads, host maps/MMIO, other kernel
services and the larger runtime remain transitional.

`tests/compiler/test_guest_allocation.py` compiles the provider, proposed/current
ABI adapters and bridge with MSVC /W4 /WX or Clang -Wall -Wextra -Werror.
Independent byte occupancy and exact snapshots cover 2,100 randomized operations;
guarded high-address/lifecycle probes test invalid configuration preservation,
range limits, protected pages, exhaustion, padding and restart. Four native
threads execute 8,000 allocation/write/query/release cycles; five compiled
negative controls must fail. Existing APU discovery and GPU address resolution
functions also execute against the provider: address-keyed audio history survives
reindexing, and GPU physical coverage includes gaps below the highest owner.
Those inherited consumer bodies are extracted into temporary test storage,
not shipped as new fixtures. These probes are separate from full patch
migrations, builds and isolated gameplay; measured acceptance is in STATUS
and MAC-110. Native Mac title execution remains unverified here.

## Native image loading

`src/runtime/guest_image.c` / `guest_image.h` own XBE byte inspection and
materialization. They use explicit little-endian reads with no toolkit headers
or host calls. `tsfp_image_inspect` validates header/image bounds, up to 4096
section descriptors, disjoint virtual/file payloads, a unique retail/debug
entry decoding, up to 378 terminated import slots, certificate and TLS data.
Only initialized section bytes can supply entry/import/TLS initialization data;
virtual tails are zero-filled, raw alignment padding is clipped, and other
memory holes are left untouched. Optional names/debug/library metadata and
cryptographic signatures are not authenticated by this runtime reader.

`tsfp_image_load` revalidates before writing any destination bytes. Failure
preserves memory and output metadata; overlapping source/destination/metadata
buffers are rejected. Input bytes must remain immutable throughout the call.
Successful publication supplies section/code bounds, thunk count, title/region
and TLS fields. Title conversion handles surrogate pairs, substitutes U+FFFD
for unmatched surrogates and trims trailing spaces/tabs within 40 UTF-16 units.

`tsfp_image_bootstrap` checks the transitional low-service layout before host
mapping: shift above 0x700000 rounded to 64 KiB, stack end 0xf80000 plus shift
below the guest limit, and aligned TLS initialization plus four bytes fitting
the existing 0x200-byte template reservation. The runtime still owns actual
host mappings/mirrors/MMIO, TIB construction and thread scheduling. Rejecting
an oversized template replaces the old unchecked overwrite; it does not add
support for arbitrary TLS sizes or callbacks.

`0205-independent-image-loader.patch` retires the old header/section/thunk/
certificate parsing/copy and title conversion, calls preflight before mapping,
loads via the provider, and uses validated TLS fields for existing TIB setup.
Load failure after mapping invokes existing shutdown before returning false.
Metadata and low-shift globals are assigned from the new image, not accumulated.
This is not a replacement or full lifecycle audit of host-memory services.

`tests/compiler/test_guest_image.py` compiles the real provider with unaligned
input, guarded destinations and unchanged-on-error checks. Original synthetic
images cover retail/debug decoding, BSS, raw padding, reordered sections, more
than 64 sections, title encoding, malformed/overflowing/overlapping extents,
import capacity/termination and bootstrap limits. The independent Python reader
checks metadata and full image bytes; three compiled mutations must fail.
Provider probes use MSVC /W4 /WX and Clang -Wall -Wextra -Werror; the import
stride is explicitly 64-bit and the integration initializes bootstrap outputs.
Complete memory buffers are compared directly before raising a bounded
first-mismatch/length diagnostic. Pytest's CI-enabled full sequence formatting
stalled on intentionally incorrect repetitive buffers, despite the surrounding
expected-failure check. The diagnostic repair preserves every byte comparison
and all three compiled mutation assertions; it changes no runtime code.
The private supported-US test verifies the executable hash before comparing all
header/section bytes. Baseline evidence compared all 64 MiB with the inherited
loader in temporary ignored storage; none of those extracted bodies are shipped.
Installed-kernel routing, full patch migrations, builds and isolated gameplay
are separate acceptance gates. See STATUS.md and MAC-109 for measured host scope.

## Guest state and ABI reports

`src/runtime/guest_state.c` owns architectural GPR, XMM/MMX, x87, direction,
SEH/frame and refusal-context cells declared by the canonical header. They are
per native thread; callbacks and fibers on that thread share them. This does
not implement a guest-thread scheduler or fiber save/restore. New native
threads receive zero cells except FS bootstrap 0x1000, x87 control 0x037f and
comparison status 0x4000. The loader still supplies actual spawned-thread TIBs.
The force-return flag and volatile call history/counters retain process scope;
history remains best effort, not a coherent atomic crash snapshot. Memory
mapping, code/image bounds and kernel thread objects remain separate services.

`guest_abi_diagnostics.c` supplies the optional RECOMP_ABI_CHECK hooks. Ordinary
and known-cleanup calls independently report the first 32/64 distinct target
addresses, including zero. A shared host-atomic lock serializes registration
and report output across these hooks. Diagnostics include register changes,
the modular signed stack delta and four recent ring entries, and never change
guest cells. Signed deltas are computed without signed-overflow arithmetic.
The ordinary unsigned lower-bound and exact cleanup comparisons retain the
canonical call macros' policy. Messages retain the ABI tag and target identity;
register, delta and history lines are now consistent between both forms.
This is ordinary diagnostic logging, not reentrant or signal-safe reporting.

`0204-independent-guest-state.patch` removes inherited storage/report bodies
from memory layout and links both project providers after continuations. It
leaves memory/loader services, canonical header and generated C unchanged.
`test_guest_services.py` compiles cross-unit/thread/default/storage contracts,
both diagnostic modes, capacity/duplicate/concurrent calls, wraparound and
actual production macros with checks on/off. Seven compiled negative controls
cover default cells, TLS, report limits, duplicate suppression and ring indexing.
The installed-routing test also checks the current loader's bootstrap constant.
Both CI hosts select these tests. STATUS.md and MAC-108 distinguish acceptance
from implementation; title gameplay exercises the storage even when ABI
diagnostics are disabled.

## Native nonlocal jumps

`src/runtime/guest_nonlocal_jump.c` owns the continuation registry behind
`recomp_setjmp_slot`, `recomp_guest_longjmp` and the foreign-stack callback.
Generated `setjmp` stays in its caller's live native frame. Per-thread arrays
keep 32 stable native buffers; repeated guest keys rearm in place, overflow
replaces only the newest slot, and jumping to an older slot retires later ones.
An unknown key returns zero before reading guest memory. Project guest-cell
accessors restore EBP/SEH-EBP, EBX, EDI, ESI and saved ESP plus four. Zero results
become one; all other result bits are preserved on the supported 32-bit-int hosts.

Windows compares an integer stack tag with the current stack limits. A foreign
handler must transfer control to the owning stack; a missing or returning handler
now stops with a diagnostic rather than attempting an invalid native unwind.
Configure that process-wide handler before guest threads start or while they
are quiescent. The POSIX path retains the existing same-stack-only assumption;
native Mac fiber switching is not claimed. Tags are not dereferenced, and are
captured in the binding frame without returning a local address from a helper.

`0203-independent-nonlocal-jumps.patch` removes the inherited implementation and
adds the own source to `xbox_kernel`. It uses `TSFP_SHARED_SOURCE` supplied by the
final layout patch. Earlier patch payloads stay immutable. The canonical header
and generated function bodies do not change. `test_nonlocal_jump_runtime.py`
executes the real provider, six compiled negative controls, actual emitted calls
with the canonical header, concurrent same-key use and Windows fiber/error paths.
The supported-US generation currently detects neither CRT helper: a title replay
is a regression check, not evidence that these service calls were exercised.
Production integration and host acceptance are tracked in MAC-107.

## Guest branch-feedback recorder

`src/runtime/tsfp_branch_feedback.h` and `guest_branch_feedback.c` own optional
target/site observation and guard counters. The 8 MiB guest window uses atomic
flag union; a bounded chained table retains six distinct targets per site,
marks saturation and counts dropped observations when full. Thread-local
last-pair caching avoids a shared racy cache. Disabled macros discard arguments;
enabled macros evaluate addresses once and leave out-of-window flags lazy.

Target/site v1 dumps remain compatible with the own Python merge parser.
`RECOMP_ICALL_FEEDBACK_PATH` in the environment overrides the compiled default.
Explicit initialization registers one exit callback. No title initialization or
normal-exit dump is inferred from this API; forced test termination does not
prove dumping. File output is best effort, not transactional or signal-safe.
A busy snapshot lock or allocation failure preserves the previous site dump;
recording releases the lock before file I/O. Prior write and close errors are
reported. Concurrent dumps are not a globally consistent snapshot.

`0202-independent-branch-feedback.patch` follows the consumer-fixture repair,
replaces the old C/header bodies with compatibility bridges and mirrors the
canonical types header. Layout routing remains last. Default split generation
and hook refresh publish both own headers and validate inputs before writing;
both headers are fingerprinted. Explicit custom types headers remain the
caller's contract. Reserved output names also reject case-insensitive aliases.

`test_branch_feedback_runtime.py` compiles the real provider for boundaries,
evaluation, eight recording threads, capacity/saturation, idempotent exit,
disabled mode, failure/recovery and negative controls. It also executes the
inherited recorder round-trip with the project native compiler adapter.
Toolkit-free synthetic generation compiles/executes with feedback on and off;
installed runtime and title acceptance remain separate checks (MAC-106).
Both bounded host runners use `testing_profile.feedback_dump_path` to validate
the target and companion paths and override inherited output settings with paths
inside the disposable test directory, including native unlocked-profile runs.

The double-address negative control explicitly discards its first value with
`(void)`, retaining both evaluations. It must compile with Clang `-Werror` before
the behavioral oracle rejects it; a warning failure is not a passing control.

## Canonical guest header

`src/runtime/recomp_types.h` is the project-owned interface. It retains the
previously replaced register/memory, atomic, call-frame, dispatch, vector and
direction primitives and adds original service declarations, diagnostic aliases
and guest-width executable-target admission. The argument to admission is read
once; zero upper bounds and synthetic kernel addresses retain their acceptance.
Services declared here still include transitional implementations; this is not
ownership of the loader, clock or diagnostic service bodies. Native nonlocal
jumps use the own provider described above.
Optional feedback uses the project provider described above. The header-service
fixture supplies a test provider for routing/counters; the recorder suite
separately exercises the real implementation and its compatibility bridge.

The batch generator and CLI read this header by default, even with no toolkit
header present. Explicit `--types-header` remains supported. The generation
fingerprint hashes the canonical header. `0199-independent-runtime-header.patch`
installs an identical compatibility template after scalar support and before
source-layout routing; tests prevent divergence. Do not edit the snapshot.
`test_runtime_header.py` executes generated synthetic guest code with the toolkit
absent, checks missing-header output preservation, service calls and interval
boundaries, and rejects five compiling mutations. Mac title acceptance is pending
MAC-105. Existing ABI/call-frame/dispatch/vector regressions remain enabled.

`0201-runtime-header-consumer-fixture.patch` follows the header replacement and
precedes layout routing. The inherited guarded-call oracle must extract the
target-admission function with its public macro. Macro-only extraction failed
on both CI hosts. `test_runtime_header.py` runs that same oracle through the
project's native compiler adapter, including MSVC-only machines, and verifies
that removing the helper still fails compilation/linking. Game code is unchanged.

## Scalar numeric support

`0198-independent-scalar-support.patch` follows vector support and precedes
layout routing. MOVSX/MOVZX now mask their source directly in
`translation_moves.py`; signed extension flips and subtracts the sign bit using
unsigned arithmetic. The source is read once, partial destinations retain their
upper bits, and no SX/ZX runtime macro is needed. String direction uses an
original unsigned displacement macro which samples DF before the argument.
The unused comparison, parity, rotate, carry and x87 numeric helper bodies are
removed. The rounding fixture retains its numeric assertions while executing
production-emitted FRNDINT/FIST; classification drops unused helper extraction.

`test_scalar_support.py` covers all byte/word values and register/partial aliases,
memory read counts and FS addressing, direction argument side effects, actual
rounding emission, and seven compiled negative controls. Existing x87 hardware,
classification and extension regression sweeps remain enabled. Audits must scan
generated C with `--no-ignore` and inspect dynamically assembled helper names.
Generation predicts exactly 7,894 extension-expression replacements across 16
chunks; all other generated bytes except the types header must remain identical.
FP environment and runtime services remain work; header ownership is described above.

## Vector runtime support

`0197-independent-vector-support.patch` follows call frames and precedes the
repository-layout patch. It removes the unused inherited MMX/SSE operation layer;
the independent emitters already generate those operations directly. The only
remaining consumer API, `XMM_SCALAR`, is project-owned: it copies a float's bits
to lane zero and clears all upper lanes for the model-transform fast path.
`test_vector_support.py` checks production, generated-code and isolated headers
against raw bit patterns, including signed zeros, subnormals and NaN payloads;
both mutation controls must compile and then fail. Existing SSE/MMX native
oracles and model-transform comparisons remain enabled. The canonical header is described above; FP environment and runtime services
remain separate replacement work (MAC-103).

The bundled snapshot supplies the remaining pinned runtime while project-owned tooling replaces supported stages. Existing third-party credits and the remaining replacement work stay explicit.

## Generation and validation

After main's responsibility-folder migration, all branch-only translation tools,
fixtures and patches also live in their compiler groups. Dynamic standalone-test
copies select `tools/compiler`; production entry points resolve their siblings.
The full replacement order is retained, with main's source-layout routing patch
last. Existing patch payloads and ledger basenames are unchanged. The grouped
XBE bridge establishes the compiler import path for compatibility modules.

For MSVC compiler heap exhaustion during generated-C compilation, a process-local
`CL_MPCount=2` limits the existing /MP workers while keeping normal Release
optimization and the standard build/test driver. CMake's project parallelism
alone does not cap /MP. Record the failed attempt and successful retry separately.

Generation hashes cover own translation/diagnostic tools and bridges. Native
integer/bit/product fixtures cover widths, carry, snapshots and division faults, with ML64 shift/rotate
references on Windows; moves cover hooks/stack aliases. Title patches precede
own move/integer/bit/product bodies. Shared CI runs original and inherited consumers;
scope and host limits are in `docs/development/independent-runtime.md`.

`tools/compiler/xbe_image.py` owns executable analysis; `test_xbe_image.py` validates
synthetic bounds/decoding and optional private-US metadata. Build drivers pass
`TSFP_PROJECT_ROOT` to the compatibility bridge, including shared Mac deps.
The generation fingerprint includes reader, export data and bridge. Windows
reruns the full pipeline on staleness; Mac requires `--generate`. See
`docs/development/independent-runtime.md` for provenance and remaining upstream components.

`tools/build/import-game.py` / `disc_image.py` are the project-owned stdlib disc
importer. `tsfp.ps1 Extract -Python ...` runs before Setup without entering
the external checkout; Mac uses the same CLI. Existing files must match before
any copy. `tests/compiler/test_disc_image.py` covers format failures, preservation,
private supported-XBE input and a toolkit-free PowerShell fixture, on both CI
hosts where applicable. This replaces import only; generation/runtime are still
upstream. See `docs/development/independent-runtime.md` for the remaining replacement work.

## Runtime and lowering ownership

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

Condition evaluation is owned by `translation_conditions.py` through the final
`independent-translation-conditions.patch`. Jcc, SETcc, CMOVcc aliases and LAHF
consume captured comparison/result values. Arithmetic signed conditions use
SF xor OF; LAHF now includes defined auxiliary carry. ADC/SBB retain the source
and original left operand in the snapshot ABI. LAHF requests carry storage;
parity/overflow SETcc and CMOVcc dispatch use the shared alias map. Integer
native x86 and independent mathematical oracles cover all byte pairs and wide
edges, with operand overwrites; decoded consumers and NaN/float tests compile.
Private join and timing fixtures retain their independent behavioral oracles;
actor-voice negative controls locate the original JNE target and wrap its
current captured condition instead of depending on a legacy macro spelling.
`independent-condition-fixture.patch` updates the inherited logical-condition
spelling assertion and skips only compiler-dependent sweeps when a compiler is
absent. Original sweeps/negative controls also run through the project MSVC
harness, preventing the Windows compiler probe from hiding this fixture.
Unknown flags still return no expression. Complete EFLAGS, mixed setters,
zero-count shifts, caller flags and guest fault delivery remain work.

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

`independent-translation-compare.patch` follows exchange. `translation_compare.py`
owns CMP/TEST snapshots and register-self-test normalization shared by fused
and ordinary consumers, retaining the existing snapshot ABI and direct 32-bit
global-load hook. Each operand is read once per snapshot. CBW/CWD/CWDE/CDQ use
unsigned masks and preserve unaffected registers/flags; CWD, previously unhandled, fills only DX.
Original compiled fixtures cover all byte comparison pairs, exhaustive word
extension, seeded wider inputs, memory hooks/read counts and decoded consumers.
Windows x86 oracles also compare implicit-extension values and flag preservation.
Original-guest fixtures built by `test_recomp_float_joins.compile_and_run` link
the shared `guest_integer_fault.c` adapter on both hosts; MSVC outputs each
source object into the fixture directory. This keeps DIV/IDIV callees resolvable.
The full inherited translator suite remains enabled. Complete EFLAGS provenance and fault delivery remain separate work.
Fused CMP/TEST branches already consume sampled operands, without rereads. Generation hashes and both-host CI include this module; see
MAC-087 and STATUS.md for host acceptance.

`independent-translation-exchange.patch` follows bitops, removing XCHG and
XADD/CMPXCHG bodies in favor of `translation_exchange.py`. Byte/word/dword
operands and addresses are sampled before writes; XADD writes its source first
for same-register aliases. Memory exchange is implicitly atomic; unlocked
XADD/CMPXCHG retain the prior stronger atomic-memory policy. Original helpers
in the runtime template use MSVC intrinsics or Clang/GCC sequentially consistent
atomics at all three widths; old 32-bit ABI names are aliases only. The header
and module enter existing generation fingerprints. Captured result/operand/
carry state supplies zero, sign, parity, overflow and ordered conditions, plus
zero-flag joins. Zero predicates use logical negation so CMOV wrapping stays
Clang-clean under -Werror. Full architectural flags and other LOCK families remain work.
Fixtures execute decoded consumers, native x86 references and four-thread ADD,
CAS and exchange-lock contention. Aligned mapped RAM follows host contracts;
MMIO, alignment/page-crossing faults and guest exception delivery are not
implemented here. See MAC-086 and STATUS.md for host acceptance.

`0140-independent-translation-bitops.patch` follows product lowering, replacing
non-LOCK bit tests/modifications, scans and byte swaps with `translation_bitops.py`.
BT forces CF publication too; carry and scan-zero consumers use sampled flags.
Signed word/dword memory indices use unsigned floor-division arithmetic; zero
scan results retain destinations as explicit undefined-result policy. Unrelated
dispatch and existing LOCK handlers remain intact. Native fixtures include x86
memory/register oracles and decoded SET/CMOV/branch consumers; historical text
fixtures follow the new emission. Hashes, loader order and both-host CI include
this stage. Full flag provenance, atomic replacement and native Mac gameplay
remain separate work. Windows bit-stage game acceptance now passed; see MAC-083
and docs/project/STATUS.md. Patch-index hashes must use LF patch bytes as stored in Git;
normalize a stale local CRLF copy before refreshing the index.

`0200-independent-translation-product.patch` follows bit lowering and removes the
multiply/divide method bodies in favor of `translation_product.py`. Inputs are
sampled before writes; all integer widths retain the implicit register pairs.
Multiplication publishes CF/OF together, with overflow branch/SET/CMOV consumers.
Division checks zero and quotient range before result writes, using unsigned
magnitudes for signed division. Faults publish the local EBP and call the own
fatal adapter; this does not implement guest SEH. CMake links that adapter on
both hosts. Historical snapshots defer this patch with the other own handlers.
The SAR consumer fixture accepts the complete emitted block and uses the shared
native compiler adapter; its compiled sweeps and negative controls remain.


The active independent-replacement goal is in `docs/development/independent-runtime.md`.
`0150-independent-translation-bits.patch` follows integer lowering and removes seven
shift/rotate handlers in favor of `translation_bits.py`. Counts are masked to
five bits, operations retain operand width and zero counts preserve emitted
state. Carry rotates use an explicit width-plus-one ring. Double shifts accept
the defined word count of 16; larger word counts explicitly leave state alone.
Known constant no-ops preserve predecessor flag interpretation in block lowering
and carry discovery. Rotate/double-shift carry consumers request and read `_cf`.
Carry rotates declare `_cf` even at constant zero: their guarded C still names it.
Native fixtures compare values/carry with bit-step references and, on Windows
x64, ML64 hardware instructions. Dynamic zero-count flag interpretation and
complete overflow/other flag conditions remain separate replacement work.
`0170-independent-translation-integer.patch` follows moves and replaces scalar
ADD/SUB/AND/OR/XOR/ADC/SBB/INC/DEC/NEG/NOT handlers with `translation_integer.py`.
Inputs are sampled and masked at operand width before carry/borrow calculation;
result snapshots use the computed value instead of rereading memory. INC/DEC
preserve carry, NOT preserves snapshots, and NEG uses unsigned subtraction.
The byte-decrement title hook remains explicit. Native result/carry and decoded
sequence fixtures cover existing block consumers; two inherited text fixtures
track the new expressions, including negative carry-preservation checks.
Historical snapshots omit this replacement until title hooks are installed.
Flag-condition lowering, remaining instruction families and runtime ABI remain
separate work; this module does not provide complete x86 flag semantics.
`0180-independent-translation-moves.patch` replaces MOV/MOVZX/MOVSX/LEA/PUSH/POP
bodies with `translation_moves.py`. The loader applies it after historical title
instrumentation; the own implementation retains load/store/after-store/push
policy hooks and frame publication. LEA ignores segment bases. Stack lowering
retains operand-size overrides, resolves POP memory after ESP increments and
handles POP ESP/SP aliasing explicitly. Trace names use UTF-8 byte escapes.
The inherited impossible MOVSX-vector fixture now requires a generation error.
Native fixtures compile emitted moves against production ABI macros; standalone
copies and installed consumers check the boundary. XCHG and other opcode/flag
families remain separate replacement work.
Historical checkpoint fixtures exclude the new move replacement until upgrading
to the complete title instrumentation stack; upgrade and repeated-build checks
still exercise the replacement. The context index asserts its final loader order.
`0190-independent-translation-operands.patch` removes the lifter's operand formatter
bodies/tables and imports `translation_operands.py`. Own expressions handle
subregister preservation, signed/unsigned/float memory, FS, absolute address
normalization and runtime floating-stack access. Decoder operands now retain
address width: 16-bit offsets wrap before FS is added. Invalid forms fail before
C emission; system/segment register reads remain documented zero/no-op policy.
Native fixtures execute expressions and actual runtime pointer/width macros;
standalone copies and installed translator consumers exercise the boundary.
This replaces expressions, not opcode handlers, flag lowering or the ABI macros.
`0050-independent-branch-diagnostics.patch` replaces feedback and unresolved-report
bodies with `branch_feedback.py` and `unresolved_report.py`. Feedback unions
observations before separate seed filtering, retains saturation and accepts
site-only crash dumps. Strict ledgers and path checks precede per-file atomic
publication; the pair is not a transactional database. Defaults now use project
private/pipeline; old ledgers require explicit paths, never silent title mixing.
Function bounds filter seeds only when explicitly selected. The diagnostic
report requires an XBE and explicit metadata, shows all overlapping recorded
bodies, real sections/backing and known entries/ends, and invents no function
extent or library identity. Input hashes identify the files inspected; they do
not prove a selected function map belongs to that XBE. Compatibility CLIs,
standalone fixtures, generation hashes and shared CI cover both tools.
`0130-independent-translation-bindings.patch` aliases translator config/HLE modules
to `translation_layout.py` and `translation_hooks.py`. Module identity preserves
mutable legacy configuration for imported helpers. Layout starts empty, reads
own XBE metadata and validates replacement before publication; missing ranges
and zero addresses cannot retain an earlier image. Hook markers use the lexical
scanner, with known disabled branches excluded and unknown branches retained.
Planning rejects ambiguous names/addresses and invalid register ABI frames;
the current runtime frame holds at most 24 arguments. Return cleanup is consensus
over decoded near returns in a supplied span, not a proof of code/data separation
or tail-call ABI. Own thunk emission uses the existing guest ABI macros; native
fixtures compile and execute the generated glue with those production macros.
Both sources enter generation hashes and shared CI. Later stages own lowering
and translator orchestration; runtime services/macros remain replacement work.
`0710-translation-consumer-fixtures.patch` updates isolated C scaffolding for
current fast-entry/result/dispatch ABI, retains executable SAR assertions, and
limits the onexit header-collision negative control to Windows. It changes only
tests; the generator fingerprint deliberately excludes test files.
`0090-independent-disasm-z-recovery.patch` follows traversal, replacing the remaining
engine with a bridge to `x86_recovery.CodeRecovery`. It owns table measurement,
realignment, read-only body/entry probes and block boundaries. Both fingerprints
include recovery; `test_x86_recovery.py` covers ownership, bounds and shapes.
`0100-independent-functions.patch` replaces function detection, labels and references
with `x86_functions.py` and `recomp_symbols.py`. Candidates split ordinary
bodies; alternate entries preserve shared tails. Interval snapshots bound each
evidence pass. Function/call-graph and metadata fixtures cover this boundary;
both source fingerprints include the modules.
`0110-independent-pipeline.patch` replaces config/cache/output/CLI orchestration with
`analysis_config`, `analysis_cache`, `analysis_output` and `analysis_runner`.
The standalone runner accepts XBE, optional verified metadata, section and seed
options. Receipts hash inputs/options/source and all outputs, including observed
seed status; failed publication invalidates receipts. Explicit extra sections
become code for both decoding and probes. JSON retains the generation contract;
assembly listings are human output. Original runner/cache tests cover this layer.
`0120-independent-symbol-driver.patch` routes the symbol CLI to `xdk_catalog.py`
and generation orchestration to `compile_pipeline.py`. The adapter consumes
separately licensed XbSymbolDatabase protocol output; it owns CLI discovery,
supplement validation/merging and atomic publication. The driver calls the own
parser/analyzer directly and preserves title seed and per-title output routing.
Custom metadata paths reach disassembly. The bridge also exposes the bounded
identifier-only helper to importlib consumers; real runs validate the full XBE.
Both enter the generation fingerprint.
`0060-independent-classification.patch` replaces all eleven function-classifier
module bodies. `function_catalog.py` now coordinates `function_evidence.py`
and `function_categories.py` directly from the project generation driver.
Decoded operands and bounded xref ownership supply evidence; source identifiers,
actual SDK section calls and unanimous graph neighbors supply diagnostic labels.
Pointer arrays and measured entry blocks are separate outputs, never guessed
function records. Numeric instructions do not establish CRT symbol identity.
The catalog preserves measured function bounds; all three sources and runtime
bridges enter generation fingerprints. Original catalog/evidence/category tests
include copied-module execution without the runtime. Translation remains upstream.

`0380-independent-generation-support.patch` follows the flag-join patches so their
translator context remains replayable. It routes manual-source inspection to
`c_source_scan.py`, reports/declarations and generated-text publication to
`generation_output.py`. Standalone importlib consumers bootstrap through the
own parser bridge. Scanner tokenization masks comments/literals, recognizes
real body braces and resolves literal preprocessor branches; unknown conditions
retain possible branches and require preprocessed input for configuration proof.
Atomic changed-file publication preserves existing output on failure and keeps
unchanged timestamps/newlines. Original scanner/output and existing title-wrapper
tests cover these contracts; both own modules enter generation fingerprints.

`0160-independent-translation-decode.patch` replaces the translator's instruction and
operand model, bounded linear/CFG decoding and basic-block construction with
`translation_decode.py`. It uses the project Capstone factory, preserves segment
and width details, and splits linear streams at explicit restart anchors. CFG
walks terminate at known boundaries; block edges only name decoded leaders.
Overlapping entries converge on shared suffix blocks, and gaps cannot create
fall-through. `test_translation_decode.py` includes standalone copied modules
and the existing translator consumers. Generation hashes include this module;
core lowering/configuration/HLE/translation orchestration remain upstream.
The command builder accepts legacy options without `json` and verbatim explicit
output paths; CLI entry resolves relative output paths before subprocess cwd changes.

`0080-independent-disasm-traversal.patch` follows the input patch and removes the
engine's scan/realignment/reachability/query methods in favor of inherited
`x86_flow.InstructionTraversal`. The subsequent recovery patch replaces the table/prologue methods. Both cache fingerprints include the new module. Synthetic
`test_x86_flow.py` covers edges, holes, limits and overlaps; input tests assert
the production method origin and run the existing consumer suite.

`0070-independent-disasm-input.patch` replaces the disassembler loader with imports
from project `recomp_image.py`, and its instruction model/classifier/Capstone
creation with `x86_decode.py`. The project detector now owns function discovery; the base classes own traversal,
candidate registration and recovery. Probe reads use `read_code_window`;
exact data reads remain strict. The analysis cache includes all independent
input source hashes, and the generation fingerprint includes disassembler
source too. `test_recomp_input.py` exercises the real patched consumer suite.

`0210-independent-xbe-reader.patch` replaces the installed Python XBE parser with a
bridge to project-owned `tools/compiler/xbe_image.py` (source root selected by the build
drivers' `TSFP_PROJECT_ROOT`, ancestor lookup for ordinary direct commands).
It also redirects two ordinal audits to the separately credited nxdk CC0 ABI
table. No original parser body runs; other generation/runtime stages remain
upstream. Reader/table/bridge changes invalidate the generator fingerprint;
Windows rebuilds all stages. Parser fixtures and patch transition tests cover
this boundary; game regeneration and isolated gameplay provide title evidence.


SSE numeric-policy timing fixtures now target captured `_simd_a`/`_simd_b`
values and `_simd_integer`; their missing-conversion and matrix fault controls
remain executed against original game routines.

## Inherited SAR sweeps

The shift/rotate patch keeps the inherited compiled SAR sweeps and their broken-expression controls. The fixture compiles the complete emitted statement sequence, including declarations and guards. Its pinned 0x80-to-0xC0 case checks the executed result rather than requiring one signed-cast spelling. Width, upper-register preservation and masked-count sweeps remain enabled on both CI hosts.


## Original x87 emission

`translation_x87.py`, installed by `independent-translation-x87.patch` after
MMX, owns stack loads/stores, arithmetic, comparisons, control/status stores
and libm mappings. Captures precede TOP changes; rounding uses guest RC and
integer conversions check range before casting. Only FILD32 uses the title's
`RECOMP_I32_TO_F64` observation hook. External 80-bit loads/stores convert to
and from the current binary64 stack. Scaling bounds its integer exponent.
Unknown x87 forms now use the standard unimplemented diagnostic.
`test_translation_x87.py` covers copied-module imports, stack aliases/wrap,
mathematical conversion/compare references, memory boundaries and supplemental
Windows native x87 oracles. Inherited structural fixtures follow new statements;
their runtime helper checks remain until foundation replacement. Both CI hosts
select the original tests; fingerprints include the new module.
This replaces emission, not stack storage, tags, exceptions, full precision
control or 80-bit intermediates. Libm transcendental approximation and result-only
remainder behavior preserve the existing model; they are not exact x87 claims.
The timing FILD fixture checks captured address 0x4E7944, a single memory read,
and conversion-hook-before-push ordering; its other behavioral cases remain.
Generation/build/game acceptance is recorded separately in STATUS.md/MAC-092.

The x87 classification fixture exports `classification_source()` and includes
production `SET_LO16`. The original x87 suite compiles and executes that exact
fixture through the host compiler adapter, including MSVC, so the inherited
GCC/Clang-only skip cannot hide missing declarations on Windows. The additive
`independent-x87-fixture.patch` changes test setup only; expected status patterns,
TOP/sign coverage and stack preservation checks remain intact.

## Original control-transfer emission

`translation_control.py`, installed after x87 by
`independent-translation-control.patch`, owns direct/indirect calls, returns,
tails, conditional targets, LOOP counter widths, bounded jump-table scanning
and callee cleanup lookup. Ten Lifter methods and `_emit_cond_goto` delegate to
this module; function orchestration and runtime ABI helpers still need
replacement. Instruction/block orchestration is now project-owned as below. The existing native setjmp/longjmp interface is retained.

Indirect target capture precedes the guest return-address push. Keep each
indirect call in one complete emitted statement: the existing recovery postpass
places the stack snapshot before that statement and adjacent argument pushes.
Fallback branches also retain the `_flags /*` marker used by the existing join
probe to save predecessor zero flags. The decoded mixed-known/unknown join
fixture executes the known path and rejects a removed predecessor snapshot.
Guard misses still use the safe dispatcher. Conditional/manual/traced tails
apply the configured forced-return value on their taken exit. CX loops preserve
upper ECX; incomplete callee bodies cannot establish exact cleanup. Backward
table scans remain possible when fewer than four image bytes follow the table
end; opcode displacement words are excluded and local arms retained.

The original control suite compiles guest-stack/frame/guard/failure cases,
returns and tails, native nonlocal jumps and decoded production translations.
It also checks standalone imports, bounded table reads, malformed/short bodies
and the actual stack-recovery postpass. The installed-binding contract checks
production routing separately from subprocess injection. Both CI hosts select
these tests; generation fingerprints include the module. Generation/build and
isolated title acceptance are recorded in STATUS.md and MAC-093.

The string fixture caches its six immutable memory patterns once, then restores
both complete buffers before every case. It retains the full width/repetition/
address-size/segment/direction/count/overlap/flag matrix and whole-buffer and
register assertions. This removes repeated pattern synthesis from a Windows CI
fixture that exceeded its existing 90-second execution limit; the timeout and
runtime emitter are unchanged. Local timing is recorded separately in STATUS.md.


## Original instruction and block lifting

`translation_lifting.py` owns decoded instruction routing, captured-condition
propagation within blocks, adjacent CMP/TEST-branch fusion, result snapshots,
SETcc/CMOVcc fallbacks and unsupported-instruction diagnostics. Nine remaining
functions delegate through `independent-translation-lifting.patch`, after the
control patch. It imports only project modules and permitted standard libraries.
The instruction registry resolves string/SIMD overloads by operand register
family. The block state describes saved comparison operands/results; it is not
full EFLAGS. Existing call, SAHF, unknown-instruction and partial-flag policies
remain explicit. Counter branches do not consume comparison flags. A fused
branch still publishes the comparison for later consumers. Repeated comparisons
preload incoming ZF for a zero count; NEG can carry across preserving operations
to ADC/SBB. Unknown producers and POPFD clear the tracked interpretation.

`test_translation_lifting.py` checks decoded routing, installed bindings,
toolkit-free imports, state boundaries, compiled guest sequences, snapshot
widths and escaped diagnostics. Its negative control removes the incoming-ZF
preload and must fail the same executable. Existing translator join/postpasses
and title hooks continue to run and remain separate replacement work. Both CI
hosts select the suite; fingerprints include the module. STATUS.md and MAC-094
record acceptance and host limits.


## Original lifter context, helper recognition and naming

`translation_context.py` now owns the complete lifter class, handler registry,
per-instance state and compatibility export surface. The final additive
`independent-translation-context.patch` replaces the old lifter module with an
import bridge. `translation_helpers.py` recognizes bounded guest SEH/EH, C++
throw-record references and VC nonlocal-jump markers using project image
mapping. Full spans must be mapped and backed by image bytes. Recognition is
heuristic, not a proof of helper semantics or host exception support. Explicit
SEH overrides retain auto-detection of the other helper; zero is a valid address.

`translation_names.py` resolves wrapper/function/label precedence and records
referenced callees. It guards C identifiers and host name collisions using
project-owned keyword/library policy and an SDK-derived host-export inventory.
`generate_host_symbols.py` reads dumpbin /exports from Windows SDK um/x64 import
libraries. `translation_host_symbols.txt` contains 23,947 factual API identifiers;
the JSON manifest records SDK 10.0.26100.0, 453 input hashes and the output hash.
Neither file is copied from the bundled toolkit. LF attributes keep the exact
inventory hash stable on both hosts. Regeneration explicitly takes --dumpbin,
--libraries, --output and --manifest; ordinary builds use the checked-in list.

`test_translation_context.py` covers bounded/truncated/unmapped helper evidence,
metadata forms, multiple helpers/overrides, context isolation, name precedence,
SDK inventory provenance, standalone imports and compiled reserved identifiers
with a failing unmangled-name control. Fingerprints include all new runtime
modules and inventory files; both CI hosts select the tests. Function-level
translation, runtime storage and remaining shipped fixtures still need original
replacements. Acceptance and host limitations are in STATUS.md and MAC-095.


## Original batch translation and output publication

`translation_batch.py` owns database loading, helper overrides, category/manual
selection, generated declarations/chunks/unresolved stubs and address dispatch.
The final `independent-translation-batch.patch` reduces the installed batch class
to a subclass supplying FunctionTranslator and the runtime data root.
The later function and canonical-header stages replace both injected inputs.
Standalone batch imports need only project modules and the standard library;
title data and runtime service bodies still prevent whole-game independence.

Split output validates names, positive chunk size and required runtime types
before publication. Each file uses the original atomic content-aware writer;
this is not a transaction across all outputs. Only stale numbered chunks under
the selected prefix and obsolete unresolved stubs are removed. Wrapper dispatch,
manual replacements, kept-original bodies and unresolved caller-before-stack-pop
ordering remain explicit. Translation failures remain counted empty stubs; a
supported-title acceptance must have zero such failures. Category outputs now
include the category even with an explicit prefix, avoiding overwrite collisions.

Original generated dispatch uses sorted binary lookup with an optional flat
allocation. Allocation failure preserves binary lookup; a 64-bit span represents
the full guest address range. Empty dispatch is valid ISO C. Initialization is
startup-only, not a new concurrency guarantee. `test_translation_batch.py`
compiles emitted outputs and checks selection, stub stack behavior, both search
paths, allocation failure, empty/extreme addresses, repeat publication, stale
cleanup and standalone imports. Both CI hosts select these tests; generation
fingerprints include the module. STATUS.md and MAC-096 record measured acceptance.


## Original translator context and entry discovery

`translation_discovery.py` owns function-context initialization and the late
translator discovery pass (distinct from the earlier whole-image analyzer).
`independent-translation-discovery.patch` replaces sixteen methods and injects
two shared read/decode helpers into the remaining FunctionTranslator. Its
constructor uses the original project decoder/lifter and retains database
identity for batch-added entries; trace/override maps are instance-local.

Callback bounds are inferred from immediate/register moves and comparisons in
functions containing register calls. Explicit destination/subregister writes
and volatile registers across calls invalidate constants. This bounded heuristic
is not arbitrary symbolic execution. Tables reject incomplete mapped words and
unsupported gaps. Function reads validate the whole mapped physical span.
Strong entries separate sections; indexed-jump/conditional bridges can reclaim
reachable weak seeds. CFG decoding iterates until table destinations stabilize.
Standalone switch arms require closed decoded paths and the existing entry-flag
prefix screen; it does not prove whole-graph EFLAGS liveness. Return-cleanup
recognition remains a bounded straight-line heuristic and stops at calls/jumps.

`test_translation_discovery.py` uses synthetic bytes/metadata, section holes,
alias clobbers, table sentinels/bias/duplicates, reachable/unreachable weak seeds,
partial/flag-dependent arms, known call targets and return cleanup. One installed
consumer test compiles and executes a recovered split function's table arms.
Standalone import/constructor checks exclude the toolkit. Both CI hosts select
the suite, and generation fingerprints include the module. The subsequent
function stage owns emission and joins; runtime storage and old fixtures remain.
STATUS.md and MAC-097 record acceptance and platform limits.


Windows CI follow-up: the crash-helper heartbeat fixture must poll through
FileNotFoundError/PermissionError while the helper atomically replaces its JSON.
`wait_for_armed_health` keeps the original absolute eight-second deadline; JSON
corruption and other I/O errors still fail. Portable deterministic tests prove
transient recovery, malformed-data rejection and persistent-denial timeout.
Native helper death, report, dump and heartbeat assertions remain unchanged.
This test-only repair does not change the helper, launcher or game behavior.

## Complete function emission

`translation_function.py` owns C function assembly, carry-storage selection,
typed flag joins, bounded block prepasses, x87 accessor output and indirect-call
failure stack captures. `independent-translation-function.patch` follows discovery;
the old translator path aliases this module so diagnostic/negative-control
monkeypatches still reach production. Batch output injects the runtime root.
Entry traces precede fast-entry checks, which precede register references;
explicit traces and forced returns retain their bypass rules. Local EBP and
shared frame inheritance retain the runtime ABI. Flag convergence keeps its
three-pass bound and decoder resynchronization its eight-pass bound. These are
migration contracts, not full EFLAGS/ABI proofs. Stack recovery retains the
first restored-register push as a frame save and recognizes nearby caller cleanup.
The x87 push captures its argument once before changing the circular index.
`tests/compiler/test_translation_function.py` compiles hook ordering, failure stack
recovery and x87 accessors; it also checks typed joins, diagnostic counts,
escaped metadata and toolkit-free function generation. Private pickup/vehicle
negative controls and inherited consumers remain selected. Compare production
function bodies after accounting only for reviewed comment, redundant final
null-statement and accessor-definition changes; do not equate those with an
unconditional byte-identical result. See STATUS.md and MAC-098 for acceptance.

## Original translation command line

`translation_cli.py` owns option parsing, input resolution, category selection,
single/header/batch modes, manual pin/wrapper planning, HLE integration and hook
refresh. `compile_pipeline.py` invokes it directly with an explicit runtime data
root. The compatibility `tools.recomp` entry only bootstraps and aliases the own
module; no implementation remains there. The default header is project-owned;
title data and hook sources are still transitional dependencies. The historical snapshot and patch text remain
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

## Guest register and scalar-memory interface

`independent-guest-abi.patch` replaces the shared register declarations, lane
layouts, scalar RAM lvalues, partial-register writes and PUSH32/POP32 primitives
in the effective runtime header. The original implementation is in this additive
patch; the snapshot and historical patch stack remain intact. Macro names and
TLS cell symbols preserve the generated-code/callback ABI. Dispatch and call
frames are described below; GPR-reference policy, remaining operation helpers
and kernel-owned storage are separate work.
The x87 stack stays double-backed and MMX storage remains independent of x87.

Supported hosts are little-endian. GCC/Clang scalar aliases explicitly permit
unaligned aliasing; MSVC uses unaligned-qualified volatile access. Address
arithmetic wraps to 32 bits before host relocation. These RAM lvalues preserve
access ordering, not atomicity, MMIO routing or guest exception delivery.
Partial writes evaluate cell and source expressions once. PUSH samples its source
before decrementing ESP. POP loads the old top, increments ESP, then applies its
destination, including an ESP alias or an ESP-relative memory destination.

`test_guest_abi.py` compiles the actual header and an extracted project-only
interface in two translation units. It covers widths/unaligned addresses, signed
and float representations, relocation/wrap, every low-word value, side-effect
counts, stack aliasing and thread isolation. Four compiled negative variants
lose wrapping, preserved bits, POP order or TLS and must fail at runtime.
`test_translation_moves.py` includes the new primitive-support block when
extracting production macros; the operand fixture shares that extraction. The
inherited x87 classification fixture includes the partial-write helper. Their
original generated-code assertions are retained.
Both runtime CI hosts select these tests; native Mac gameplay remains pending.


### Main-branch filename reconciliation

The independence branch retains its full late replacement list after adopting
main's numbered names for older patches. Branch-only patches from before that
cleanup retain their existing names; newly added patches follow patches/README.
The combined renamed ledger preserves all 149 effective runtime file hashes.
Historical fixtures keep their old-stage memberships and then upgrade to the
entire current stack. CI reconciliation does not establish new title behavior.

## Indirect guest-call dispatch

`0195-independent-guest-dispatch.patch` replaces the ICALL, SAFE, SAFE_AT and
ITAIL implementation with shared project lookup/invocation helpers and lazy
recovery macros. It follows the guest ABI patch. Manual entries retain priority
over generated entries and kernel bridges. Calls record the truncated target
before range validation; kernel synthetic addresses and an unloaded code range
remain admitted. Only valid calls observe their site. Tail jumps use the same
resolver without call-ring/range admission and retain refusal form 2.

On success the callee's register/stack results survive. On failure diagnostics
see the original frame before recovery. SAFE arguments remain unevaluated on
success; invalid targets evaluate the saved stack for logging and again for
recovery, preserving diagnostic side effects. Disabled feedback does not evaluate
the site expression. Call checks and HLE frames are described below. Trace
storage, feedback providers, diagnostic loggers and nonlocal-jump services
remain separate dependencies.
The canonical-header stage above completes header ownership, not service ownership.

`test_guest_dispatch.py` compiles the production header and extracted dispatch
with ABI checks and feedback independently on/off. Each configuration runs
4,096 state/event cases covering resolver precedence, range endpoints, target
truncation, trace overflow/timing, lazy arguments, diagnostic stack changes,
callee register/stack results and tail failures. Four compiled negative controls
lose lookup priority, recovery timing, recording or tail classification and must
fail. The existing guarded-call fixture now extracts the whole helper interface
without weakening its generated-call assertions. Both runtime CI hosts select
the tests. Production generation, build and game evidence is recorded in STATUS.

The generated table defines `RECOMP_DISPATCH_H` before including the runtime
types and supplies its public pointer typedef later. Dispatch helpers declare
an independent compatible pointer type and lookup prototypes, so either include
order works. A regression compiles the actual batch-generated table with the
production header and executes a lookup; isolated macro fixtures alone did not
cover this translation-unit boundary.

## Call checks and HLE argument frames

`0196-independent-call-frames.patch` replaces the callee-state check macros and
LTCG argument-frame helpers after the dispatch patch. Diagnostic builds capture
EBX/ESI/EDI/ESP before evaluating the callee expression. Ordinary calls retain
the unsigned lower-bound stack check; known-pop calls retain exact cleanup.
Target-address expressions are evaluated only for a diagnostic, cleanup keeps
its short-circuit/double-evaluation behavior, and disabled checks evaluate
neither expression. Logger side effects remain visible; logger implementations
and register storage are separate dependencies.

HLE frames capture the return address, original argument base and entry stack
before cleanup/target expressions run. At most 24 supplied words are copied into
a scratch guest frame; the return address is written first. Calls preserve HLE
register results and restore entry ESP plus captured cleanup, including wrapping
and nested calls. Uninitialized argument slots remain the caller's responsibility.
The compatibility macros keep the public `a` array and original call order.

`test_guest_call_frames.py` compiles production and extracted project blocks,
with diagnostics on/off. Each fixture executes 448 call-state/expression cases
and 24 HLE frame cases, including nested calls, large argument counts and wrapped
cleanup. Five compile-first mutations must fail. The existing hook fixture
extracts the full frame block, retaining its generated-thunk assertions; dispatch
extraction no longer depends on the removed HLE comment. Both CI hosts run these
checks. Native title acceptance and its limits remain in STATUS and MAC-102.
