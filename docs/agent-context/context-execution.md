# Original guest continuation checkpoints

This is an execution primitive for the unfinished context/SEH replacement.
It is not selected by production CMake, called by the game, or a replacement
for RtlCaptureContext265, RtlRaiseException302 or RtlUnwind312 yet. The record
codec and native unwind evidence are in [context records](context-records.md).
No new game acceptance is implied by these native unit checks.

## Execution and ownership contract

src/runtime/guest_context_execution.c/.h keeps private heap metadata for live
C setjmp checkpoints. Each host thread has a separate root chain, bounded to64.
The caller supplies stable host operations, a guest stack domain and a return
PC capability. Guest PCs are validated identifiers, never host function pointers.
Initial and transferred states must pass admission before publication. Transfers
copy the complete candidate before callbacks; rejected requests preserve live
state, ownership and root metadata.

The nearest root whose domain contains ESP and whose entry ESP is at least that
ESP is selected; the oldest root admits the full domain. The entire four-byte
return slot must fit, including near the top of the32-bit address space. Scope
ledgers for every abandoned root and the destination are rewound deepest first.
Cleanup failure still attempts remaining ledgers, then exits the destination
with CLEANUP_FAILED without applying the candidate or resuming abandoned code.
Successful transfer frees abandoned root metadata and longjmps the destination;
only that live checkpoint applies the new state before executing its PC.

A step returns exactly STEP_ERROR, STEP_YIELD or STEP_RETURN. Only RETURN to
the caller's stop PC completes a root. Yielding or transferring to that same PC
executes it normally. Initial entry may equal the stop PC and must execute.
A return elsewhere continues through admission and dispatch. Zero is reserved
as a return capability, not an executable PC. Unknown step outcomes fail.

Native frames must be C-longjmp safe; there is no C++/Objective-C destructor
unwinding. Explicit ledgers own abandonable resources. Admission, mark/rewind
and apply callbacks must not execute guest code or reenter this engine. Steps
must not retain automatic native scopes across ordinary returns. The same-stack
predicate gates native longjmp. A foreign-stack adapter must transport control
to the actual checkpoint stack and never return or resume abandoned frames;
returning from it aborts with a diagnostic. No production fiber adapter exists.
MSVC4324 is suppressed only for the private root object's required jmp_buf
alignment padding; the object is not packed or serialized.

## Verification and known gaps

Run tests/compiler/test_guest_context_execution.py. Its original compiled C
contract executes real nonlocal transfers, nested/current/ancestor roots,
separate resource ledgers, cleanup failure, full context payloads, bounds,
root limit, bad admissions and four native threads each doing100 transfers.
Windows additionally tests an actual two-fiber handoff and a returning adapter
in a separate child process. Portable thread checks use pthreads elsewhere;
no native Mac result for this new provider has been established locally.
Seven compiled incorrect providers must fail at runtime, covering selection,
state publication, cleanup mark, root depth and false return completion/outcomes.

Test-first evidence exposed two real prototype defects: only the destination's
ledger was rewound, and PC equality alone completed a root. Both are repaired
and their cases retained. logs/context-execution-return-cap-before.log records
the latter failing assertion; logs/context-execution-return-fixed.log records
72 passing focused codec/callback/lifetime/checkpoint tests26.73s on Windows.
These tests are registered with the dual-host native runtime workflow.

Still required: translated instruction-boundary continuation generation and
resolver, a real live register/selector/FP adapter, production lifetime/fiber
integration, then guest exception dispatch/unwind and edited-context resumption.
Current callbacks in the fixture are test machine steps, not translated title
instructions. Do not equate this primitive with complete SEH or whole-runtime
independence, and do not claim a production build tested it while it is unselected.

The opt-in [instruction continuation generator](context-continuations.md) now
provides decoded steps and exact-PC dispatch in native tests. The production
live-state/SEH/fiber integration remains open.
