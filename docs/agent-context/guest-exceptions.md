# Original registration-chain exception dispatcher

This stage implements original dispatch/unwind bodies in guest_exception.c/.h
and selects the provider in the shared kernel archive through0258. It does NOT
yet replace the bridge entry points for RtlCaptureContext265, raise302 or unwind312.
The game's inherited imports remain until live capture, native checkpoints,
guard capabilities and resource/foreign callback barriers are connected. Do not
equate an archived provider or its translated fixture with title execution.

## Guest records, guards and ownership

The caller supplies checked memory resolution, handler admission and balanced
cdecl invocation. Guest addresses remain identifiers, never host code pointers.
An80-byte exception record,716-byte CONTEXT,16-byte temporary registration and
four-byte dispatcher word use explicit little-endian access. Caller-owned
workspace is disjoint and remains live across callbacks; every recursive dispatch
needs separate workspace. Stack bounds come from the owner, not cloned TIB hints.
No native CONTEXT layout, host SEH implementation or private game data is used.

Initial validation checks ranges/overflow/mappings, context control+integer groups,
argument count, workspace aliasing and guard capability before any guest writes.
Once dispatch begins, record/context/handler effects are not rolled back on
failure. The host continuation output changes only for successful continuation
or unwind. A full caller snapshot is copied before callbacks; unselected CONTEXT
groups preserve that baseline rather than becoming zero. Handler-selected partial
or group-free valid contexts overlay only their selected groups at completion. The caller must validate/apply/transfer it; the walker performs no
native longjmp, architectural capture, second-chance exception or host resource
cleanup. Callback admission/resolution preserves live guest state.

Guard layout: next, admitted guard routine, protected registration, unwind mode.
The adapter must supply that routine's nested/collided behavior and its native
execution transport. During invocation the guard precedes the actual chain.
Raise retains the complete chain while searching; unwind removes registrations
after their handlers return, retains/skips the target and never forges a target
when a corrupt walk fails. A callback returning with an unexpected head fails
without resurrecting an abandoned registration. Mappings/request/ops/output and
native invocation lifetimes remain caller-owned.

Raise updates ExceptionAddress from caller EIP, propagates edited context between
handlers, handles ContinueExecution/Search/NestedException and rejects invalid
dispositions or noncontinuable continuation. Unwind ORs unwind/exit flags,
synthesizes STATUS_UNWIND when no record was supplied, publishes ReturnValue in
context EAX before handlers and follows validated collided-unwind recovery frames.
Nested flags clear AFTER the boundary handler. Measured i386 collided unwind
does not itself add flag64; caller flags remain preserved. TargetIp is not an
input to this i386 core, matching the measured native cases, not proof of every
Xbox behavior. Invalid chains/targets and failed invocations return distinct
failures; a later bridge must surface them rather than log-and-continue.

## Native and translated checks

test_guest_exception.py compiles the original provider under fatal warnings.
The fixture covers four ordinary raise routes,16 unwind combinations, target
retention, whole-chain versus popped-chain guards, zero-record/exit unwind,
mapping/alias/overflow/context rejections, noncontinuability, invalid handlers,
unhandled search, chain corruption, recursion, nested boundaries and collision.
Compiled deliberately incorrect providers must fail at runtime.

A separate integration uses a real translated cdecl handler, the original
callback provider, codec, decoded continuation dispatcher and native execution
checkpoint. The handler edits EIP/EAX; the checkpoint resumes translated code at
that PC and bypasses the original instruction. Both actual register-reference
modes run. This exposed callback_plan's bare esp member colliding with generated
register macros. Its member is now r_esp without layout/behavior changes; the
callback provider and mutation fixtures use the new spelling consistently.

Original native Win32 i386 oracles exercise actual RtlRaiseException (eight
ordinary/nested/context-edit cases) and RtlUnwind (24 ordinary/collided cases).
They own temporary stack FS registrations, restore their process state and use
the Windows SDK solely as an ABI/API oracle. /SAFESEH:NO and /GS- apply only to
those original test executables. Native evidence corrected two prototype errors:
clearing nested flags before rather than after the handler, and adding flag64
on collision. Modern Windows raise includes additional context groups; tests
require the measured control/integer contract without importing XSTATE flags
into the Xbox codec. These tests skip on Mac; portable contracts run on both.

Native probes are Windows evidence, not physical Xbox conformance. Full live
selector/FP state, bridge binding, native callback ownership/barriers and actual
game exception resumption remain required. See [execution](context-execution.md),
[production code export](continuation-generation.md) and [records](context-records.md).
The broader runtime/tooling/title/header/fixture replacement and final builds
with both upstream sources unavailable remain part of the active objective.

Final Windows acceptance:183 focused cases50.52s and149 full migration/context/
scope/gate cases901.12s pass; Release and original archived symbols verified.
Current generated title C remains unchanged. Fresh final160.977s bounded Story
movement/fire/pause-resume and three captures pass;26 protected files unchanged.
See STATUS for exact hashes, the retained earlier candidate, CI retry/compiler
fixture repair, and the still-inherited kernel entry points.

The [original execution-ownership adapter](execution-ownership.md) now binds
production kernel entry/return, resource-ledger abandonment and thread teardown.
Guest-context capture, actual302/312 binding and foreign native barriers remain.
