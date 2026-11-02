# Production continuation export

This stage exports original instruction continuations through the common compiler
pipeline. It prepares actual title code for the unfinished native context/SEH
adapter. Generating or compiling these functions does not establish that gameplay
uses them; RtlRaiseException302 and RtlUnwind312 remain inherited.

## Shared generation command

tools/compiler/compile_pipeline.py adds --context-continuations whenever split
generation is selected. Windows Generate and native Mac generation use this
shared command builder. Direct translation_cli.py users can opt in with the same
flag and a positive --split; header/single-function/category-list/HLE-refresh
modes reject that combination before writing output. Existing normal function
generation is unchanged. HLE-only refresh does not remove continuation files.

continuation_output.py stages recomp_context_NNNN.c chunks (100 functions each),
recomp_context_dispatch.c, two original context headers and a manifest in the
selected generated directory. Production location is the maintained runtime
title's src/recomp/gen directory; CONFIGURE_DEPENDS already selects its C files.
Generated title material remains ignored/private. The two header sources and
both new generator modules participate in the generation source fingerprint.

Unsupported bodies are explicitly reported and excluded. Pure manual entries
remain opaque native capabilities; kept original bodies are eligible for decoded
continuation generation. Wrapper lifetime/posthook restoration is not implemented
by this exporter. The manifest reports selection, bodies, exact/shared entries,
opaque entries and rejected reasons; counts establish generation coverage only.

## Exact PCs and execution

continuation_index.py validates each instruction's bounded bytes and decoded
metadata. Different recovered functions may share tails or even overlap through
alternate instruction streams. Every overlapping byte must agree; a shared PC
must also have identical decoded operands/targets. Lowest function entry owns a
shared PC. Each add validates before committing registry changes. An inconsistent
title registry is fatal rather than silently excluding a conflicting function.

Generated dispatch uses a sorted exact-PC table and a separate immutable host
function table. Binary search admits only actual instruction starts, never ranges,
interior bytes or guessed host pointers. The existing one-instruction API remains
available. Production run functions instead follow local decoded flow until a
CALL, RET or transfer outside their body. CALL samples its target before pushing
the actual return PC; RET alone emits RETURN. A branch reaching a value equal to
a caller return address is still a branch. Restored Jcc consumes saved flags and
the maintained RECOMP_FLOAT_BRANCH_AT policy, without repeating a preceding CMP.

## Publication and receipts

All decode/emission/registry checks finish in a staging directory before replacing
previous outputs. Individual replacements are atomic, timestamps remain stable
for unchanged text, stale cleanup is restricted to owned continuation C names,
and symlink/escaping targets reject before publication. This is not a multi-file
filesystem transaction: an I/O interruption during replacement can leave a mixed
set. The manifest is published last and recomp-generation.py validates every
listed file and the complete owned chunk set before accepting a generation
receipt. Missing, modified, unlisted or unsafe output invalidates it. Text SHA256
normalizes native newlines, matching the existing publication helper.

## Verification and remaining boundary

test_continuation_index.py compiles shared/overlapping dispatch, local loops,
cross-function CALL/RET and restored branch policies. test_continuation_output.py
exercises real CLI generation and separately compiled chunks in both actual GPR
reference modes, idempotence, generation failure preservation, stale cleanup and
manual/kept selection. test_recomp_generation.py rejects partial/modified output
receipts and unsafe manifest identities. These join both-host runtime CI.
Initial95 focused checks pass17.44s on Windows; full title generation/build/game
results belong in STATUS. Neither native Mac title execution nor production
continuation execution is established by those synthetic checks.

Native state mapping, resource barriers/ownership, wrapper lifetimes and real
raise/unwind remain required before selecting the checkpoint execution core in
the production kernel. See [context execution](context-execution.md),
[records](context-records.md) and [canonical EBP](canonical-ebp.md). Broader runtime,
graphics/audio/devices/title/header/fixture provenance removal and final builds
with both upstream sources unavailable remain part of the full active objective.

Production Windows acceptance:15746 bodies/1198926 PCs/62486 shared,158 chunks,
92 explicit rejects/25 opaque; normal generation unchanged. Release and all159
new compiled units pass. Fresh101.212s Story movement/fire/pause-resume passes;
source/run/executable and26 protected files verified. See STATUS for hashes and
Windows-only synthetic scope. Native context execution remains unselected.
