# Translated instruction continuations

This opt-in generator API is a prerequisite for the unfinished context/SEH
replacement. Normal game generation still emits the existing fast C functions.
There is no production CLI flag, CMake selection, live architectural adapter or
raise/unwind binding for this cold path yet. Do not claim that the game executes
it. [Checkpoint ownership](context-execution.md) and [record/native evidence](context-records.md)
remain separate contracts.

## Generator and dispatch

tools/compiler/translation_continuation.py owns the new implementation;
FunctionTranslator.translate_context_function and translate_context_functions
expose it without mutating the normal translator's lifter. Existing decoded
function recovery is reused. Immutable metadata/helper discoveries are shared;
per-emission caches and diagnostics are fresh. The title-wide C++ helper scan
must not be repeated for every function. A test forbids that repeated scan.

Each tsfp_context_step_ADDRESS takes an admitted guest PC and next-PC pointer,
executes one decoded instruction, then returns STEP_YIELD or STEP_RETURN.
Every actual decoded instruction has a switch entry, including the consumer
between a compare/conditional pair; entering at Jcc never reruns CMP. GPRs use
canonical project cells/optional stable references; EBP directly aliases g_ebp
for the cold body. FPU/vector instructions use the existing project model.
No fast-entry/native title hook is replayed when entering mid-function.

CALL samples its register/memory operand before pushing its four-byte guest
return address, then yields. RET reads the actual guest return cell, applies
its immediate cleanup and emits RETURN. JMP yields without a return event.
Conditional branches/SETcc/CMOVcc consume persistent status, not ephemeral C
snapshots. Required known bits are checked before effects; unknown conditions
fail rather than using a newly initialized fallback. LOOP honors the address
size of its counter. Ordinary instructions reuse original project lifters.
Unsupported opcodes, live selector/system register operands, far/16-bit control
transfers and forced-return/manual-entry policy gaps reject explicitly.

emit_dispatch builds a constant host function table for disjoint decoded
ownership spans. Binary range lookup is followed by exact instruction membership;
interior bytes and decode holes are never admitted. Guest bytes cannot supply a
host function pointer. Empty/overlapping spans reject; opaque HLE/kernel entries
require a separate adapter. A default/NULL-next step fails without guest writes.
Generated C and metadata stay ignored/local when derived from the private title.
The new provider participates in the generation-source fingerprint.

## Evidence and outstanding integration

Run tests/compiler/test_translation_continuation.py. Actual compiled C checks
CALL[ESP] sampling, guest RET cleanup/event, canonical EBP/frame restoration,
entry at Jcc, all1024 flag/condition combinations, exact-PC/gap rejection,
separate function dispatch and a real checkpoint transfer to an edited PC.
Both stable-reference modes execute. Sixteen programs compare cold and normal
GPR/status/FP-stack/vector/RAM results; EBP restoration instead uses an explicit
architectural oracle because normal fast C publishes a frame cache which can
stay stale after local POP EBP. Five compiled incorrect continuation providers
must fail at runtime. These tests are registered in both-host CI; native Mac
results for this new generator remain pending. Windows initial212 focused
compiler/checkpoint/status tests pass31.89s; further checks are recorded in STATUS.

This is not an instruction-complete CPU or a complete live context adapter.
Existing x87 uses binary64, with no new80-bit/tag/MXCSR semantics. The caller
still owns valid guest mappings and resource lifetimes; this generator adds no
memory fault delivery. Manual/native capabilities, forced-return hooks, CRT
continuations, fiber transport and normal-frame EBP publication need explicit
production adapters. A supported-US title coverage scan is diagnostic evidence,
not game execution. Production exception replacement, bounded game acceptance,
Mac title execution and final upstream-unavailable generation/build/package
proof remain required before broader completion claims.

Final local checkpoint:214 distinct compiler/checkpoint/status/fingerprint checks
covered (212 broad plus2 generation), final42 checks10.79s. US metadata scan
15771 emitted/92 explicit rejects among15863 functions in28.063s. Generation
and Release succeed with all22 normal generated files unchanged. Fresh bounded
normal-path Story acceptance passes at96.771s; see STATUS for exact scope/hashes.
Preceding2a84 CI runtime41487615373/launcher41487615218 fully passed; this new
provider still needs its own CI and production adapters before game execution.

The following [canonical EBP stage](canonical-ebp.md) fixes normal fast-path live
EBP and context field/macro compatibility. Its tests explicitly activate the
reference capability; earlier USE/ANCHORS-only fixtures did not. The historical
normal EBP cache mismatch above must not describe regenerated canonical bodies.
