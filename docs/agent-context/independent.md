# Independent tooling contracts

The bundled snapshot supplies the remaining pinned runtime while project-owned tooling replaces supported stages. Existing third-party credits and the remaining replacement work stay explicit.

## Generation and validation

Generation hashes cover own translation/diagnostic tools and bridges. Native
integer/bit/product fixtures cover widths, carry, snapshots and division faults, with ML64 shift/rotate
references on Windows; moves cover hooks/stack aliases. Title patches precede
own move/integer/bit/product bodies. Shared CI runs original and inherited consumers;
scope and host limits are in `docs/independent-runtime.md`.

`tools/xbe_image.py` owns executable analysis; `test_xbe_image.py` validates
synthetic bounds/decoding and optional private-US metadata. Build drivers pass
`TSFP_PROJECT_ROOT` to the compatibility bridge, including shared Mac deps.
The generation fingerprint includes reader, export data and bridge. Windows
reruns the full pipeline on staleness; Mac requires `--generate`. See
`docs/independent-runtime.md` for provenance and remaining upstream components.

`tools/import-game.py` / `disc_image.py` are the project-owned stdlib disc
importer. `tsfp.ps1 Extract -Python ...` runs before Setup without entering
the external checkout; Mac uses the same CLI. Existing files must match before
any copy. `tests/test_disc_image.py` covers format failures, preservation,
private supported-XBE input and a toolkit-free PowerShell fixture, on both CI
hosts where applicable. This replaces import only; generation/runtime are still
upstream. See `docs/independent-runtime.md` for the remaining replacement work.

## Runtime and lowering ownership

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
implemented here. See MAC-085 and STATUS.md for host acceptance.

`independent-translation-bitops.patch` follows product lowering, replacing
non-LOCK bit tests/modifications, scans and byte swaps with `translation_bitops.py`.
BT forces CF publication too; carry and scan-zero consumers use sampled flags.
Signed word/dword memory indices use unsigned floor-division arithmetic; zero
scan results retain destinations as explicit undefined-result policy. Unrelated
dispatch and existing LOCK handlers remain intact. Native fixtures include x86
memory/register oracles and decoded SET/CMOV/branch consumers; historical text
fixtures follow the new emission. Hashes, loader order and both-host CI include
this stage. Full flag provenance, atomic replacement and native Mac gameplay
remain separate work. Windows bit-stage game acceptance now passed; see MAC-083
and STATUS.md. Patch-index hashes must use LF patch bytes as stored in Git;
normalize a stale local CRLF copy before refreshing the index.

`independent-translation-product.patch` follows bit lowering and removes the
multiply/divide method bodies in favor of `translation_product.py`. Inputs are
sampled before writes; all integer widths retain the implicit register pairs.
Multiplication publishes CF/OF together, with overflow branch/SET/CMOV consumers.
Division checks zero and quotient range before result writes, using unsigned
magnitudes for signed division. Faults publish the local EBP and call the own
fatal adapter; this does not implement guest SEH. CMake links that adapter on
both hosts. Historical snapshots defer this patch with the other own handlers.
The SAR consumer fixture accepts the complete emitted block and uses the shared
native compiler adapter; its compiled sweeps and negative controls remain.


The active independent-replacement goal is in `docs/independent-runtime.md`.
`independent-translation-bits.patch` follows integer lowering and removes seven
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
`independent-translation-integer.patch` follows moves and replaces scalar
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
`independent-translation-moves.patch` replaces MOV/MOVZX/MOVSX/LEA/PUSH/POP
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
`independent-translation-operands.patch` removes the lifter's operand formatter
bodies/tables and imports `translation_operands.py`. Own expressions handle
subregister preservation, signed/unsigned/float memory, FS, absolute address
normalization and runtime floating-stack access. Decoder operands now retain
address width: 16-bit offsets wrap before FS is added. Invalid forms fail before
C emission; system/segment register reads remain documented zero/no-op policy.
Native fixtures execute expressions and actual runtime pointer/width macros;
standalone copies and installed translator consumers exercise the boundary.
This replaces expressions, not opcode handlers, flag lowering or the ABI macros.
`independent-branch-diagnostics.patch` replaces feedback and unresolved-report
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
`independent-translation-bindings.patch` aliases translator config/HLE modules
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
Both sources enter generation hashes and shared CI. Runtime services/macros,
core lowering and translator orchestration still require replacement.
`zzzz-translation-consumer-fixtures.patch` updates isolated C scaffolding for
current fast-entry/result/dispatch ABI, retains executable SAR assertions, and
limits the onexit header-collision negative control to Windows. It changes only
tests; the generator fingerprint deliberately excludes test files.
`independent-disasm-z-recovery.patch` follows traversal, replacing the remaining
engine with a bridge to `x86_recovery.CodeRecovery`. It owns table measurement,
realignment, read-only body/entry probes and block boundaries. Both fingerprints
include recovery; `test_x86_recovery.py` covers ownership, bounds and shapes.
`independent-functions.patch` replaces function detection, labels and references
with `x86_functions.py` and `recomp_symbols.py`. Candidates split ordinary
bodies; alternate entries preserve shared tails. Interval snapshots bound each
evidence pass. Function/call-graph and metadata fixtures cover this boundary;
both source fingerprints include the modules.
`independent-pipeline.patch` replaces config/cache/output/CLI orchestration with
`analysis_config`, `analysis_cache`, `analysis_output` and `analysis_runner`.
The standalone runner accepts XBE, optional verified metadata, section and seed
options. Receipts hash inputs/options/source and all outputs, including observed
seed status; failed publication invalidates receipts. Explicit extra sections
become code for both decoding and probes. JSON retains the generation contract;
assembly listings are human output. Original runner/cache tests cover this layer.
`independent-symbol-driver.patch` routes the symbol CLI to `xdk_catalog.py`
and generation orchestration to `compile_pipeline.py`. The adapter consumes
separately licensed XbSymbolDatabase protocol output; it owns CLI discovery,
supplement validation/merging and atomic publication. The driver calls the own
parser/analyzer directly and preserves title seed and per-title output routing.
Custom metadata paths reach disassembly. The bridge also exposes the bounded
identifier-only helper to importlib consumers; real runs validate the full XBE.
Both enter the generation fingerprint.
`independent-classification.patch` replaces all eleven function-classifier
module bodies. `function_catalog.py` now coordinates `function_evidence.py`
and `function_categories.py` directly from the project generation driver.
Decoded operands and bounded xref ownership supply evidence; source identifiers,
actual SDK section calls and unanimous graph neighbors supply diagnostic labels.
Pointer arrays and measured entry blocks are separate outputs, never guessed
function records. Numeric instructions do not establish CRT symbol identity.
The catalog preserves measured function bounds; all three sources and runtime
bridges enter generation fingerprints. Original catalog/evidence/category tests
include copied-module execution without the runtime. Translation remains upstream.

`z-independent-generation-support.patch` follows the flag-join patches so their
translator context remains replayable. It routes manual-source inspection to
`c_source_scan.py`, reports/declarations and generated-text publication to
`generation_output.py`. Standalone importlib consumers bootstrap through the
own parser bridge. Scanner tokenization masks comments/literals, recognizes
real body braces and resolves literal preprocessor branches; unknown conditions
retain possible branches and require preprocessed input for configuration proof.
Atomic changed-file publication preserves existing output on failure and keeps
unchanged timestamps/newlines. Original scanner/output and existing title-wrapper
tests cover these contracts; both own modules enter generation fingerprints.

`independent-translation-decode.patch` replaces the translator's instruction and
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

`independent-disasm-traversal.patch` follows the input patch and removes the
engine's scan/realignment/reachability/query methods in favor of inherited
`x86_flow.InstructionTraversal`. The subsequent recovery patch replaces the table/prologue methods. Both cache fingerprints include the new module. Synthetic
`test_x86_flow.py` covers edges, holes, limits and overlaps; input tests assert
the production method origin and run the existing consumer suite.

`independent-disasm-input.patch` replaces the disassembler loader with imports
from project `recomp_image.py`, and its instruction model/classifier/Capstone
creation with `x86_decode.py`. The project detector now owns function discovery; the base classes own traversal,
candidate registration and recovery. Probe reads use `read_code_window`;
exact data reads remain strict. The analysis cache includes all independent
input source hashes, and the generation fingerprint includes disassembler
source too. `test_recomp_input.py` exercises the real patched consumer suite.

`independent-xbe-reader.patch` replaces the installed Python XBE parser with a
bridge to project-owned `tools/xbe_image.py` (source root selected by the build
drivers' `TSFP_PROJECT_ROOT`, ancestor lookup for ordinary direct commands).
It also redirects two ordinal audits to the separately credited nxdk CC0 ABI
table. No original parser body runs; other generation/runtime stages remain
upstream. Reader/table/bridge changes invalidate the generator fingerprint;
Windows rebuilds all stages. Parser fixtures and patch transition tests cover
this boundary; game regeneration and isolated gameplay provide title evidence.


## Inherited SAR sweeps

The shift/rotate patch keeps the inherited compiled SAR sweeps and their broken-expression controls. The fixture compiles the complete emitted statement sequence, including declarations and guards. Its pinned 0x80-to-0xC0 case checks the executed result rather than requiring one signed-cast spelling. Width, upper-register preservation and masked-count sweeps remain enabled on both CI hosts.
