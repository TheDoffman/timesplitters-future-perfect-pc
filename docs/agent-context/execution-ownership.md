# Original kernel execution ownership

guest_execution_state.c/.h owns per-host-thread lock and active-execution depth.
guest_execution_scope.c/.h attaches exact state restoration to the original object
ledger.0259-original-execution-ownership.patch replaces the inherited lock/lifted
function bodies and binds kernel dispatch to these scopes in production. This
is distinct from guest_context_execution.c's still-unselected PC checkpoints.
RtlCaptureContext265/raise302/unwind312 remain inherited; this ownership change
prepares their native integration rather than enabling the new exception core.

## State and host binding

One stable ops domain supplies recursive host lock/unlock and active/inactive
notifications. Its callbacks return normally, do not execute guest code or reenter
the provider, and do not report partial failure. The private TLS state records
logical nesting; only zero/nonzero active transitions notify the platform, so
diagnostics and parking count active host threads rather than nested entries.
Capture is metadata-only and does not bind a previously unused domain. A thread
binds its first ownership operation; another domain rejects while that binding
lives. Snapshots carry domain and TLS-owner identity and are caller-owned native
metadata, never guest bytes or host-PC capabilities. They expire at thread exit.

Exact restoration first leaves active accounting before changing lock depth,
releases/acquires the required levels, then reactivates after acquiring the lock.
It does not add a child's levels to the parent's saved levels. Idempotent empty
leave/exit does not make counters negative. Lock drop/restore retain the public
depth-token ABI; full suspend captures state and drops both kinds of ownership.
Invalid owner/domain/count/acquisition requests reject before host effects.

guest_kernel_execution.inc, included by the patched xbox_memory_layout.c, supplies
the shared kernel domain and existing xbox_GuestLock/GuestLifted exports. It uses
standard critical-section/atomic APIs and delegates the remaining platform's
parking, optional Windows CPU-affinity choice, configuration and diagnostic
storage. Those retained host policies still need provenance replacement; this
does not claim that the whole concurrency or platform backend is independent.

## Normal return, abandonment and exit

Kernel dispatch enters a tsfp_guest_execution_scope instead of manually dropping
the lock and decrementing a global count. It snapshots ownership, arms cleanup
before suspending, and restores on normal leave after leases release. Async
device pumping still happens after restoration. Other object scopes keep their
existing cleanup slot; each provider owns its own nested frame.

bridge_object_restore now calls tsfp_guest_execution_scope_unwind. It validates
the mark before effects, suspends current execution, drains all deeper resource
scopes and defers saved execution states. Only after every lease/cleanup callback
has run does it restore the oldest abandoned execution snapshot once. Missing
marks change nothing. Cleanup errors still drain remaining scopes and attempt
restoration, then report failure; the nonlocal caller must not resume after a
failed cleanup. Raw object-ledger unwind is not the abandonment API for these
execution frames. The existing guest CRT lifetime hook therefore restores both
resource and CPU ownership before its real native longjmp.

Thread teardown uses the separate execution-scope abort mode: drain while
suspended and stay inactive/unlocked, without reacquiring a saved guest lock just
to exit. bridge_object_thread_exit uses it; normal worker exit removes the former
early partial exit, and PsTerminateSystemThread cleans scopes/state before
freeing the guest stack and calling ExitThread. Empty exit is safe for auxiliary
workers too. Thread drain/join and process shutdown remain separate contracts.

This is host-thread ownership, not guest TIB/fiber identity or native destructor
unwinding. It neither validates arbitrary host frames nor permits longjmp across
opaque OS/C++/Objective-C callbacks. Actual SEH integration still needs those
barriers, live register/selector/FP capture and admitted checkpoint transport.

## Checks and isolated gameplay routes

test_execution_ownership.py compiles a256-transition matrix, ordered lease cleanup,
duplicate/stale/foreign-owner rejection, disabled acquisition, real guest CRT
longjmp, cleanup failure and four native threads doing64 restoration cycles each.
Thread-exit tests require no reactivation or acquisition even with saved nesting.
Seven compiled incorrect providers must fail at runtime. The Windows object/
thread binding fixture now uses the real state provider and native recursive
critical section, checks zero ownership before stack release, and retains its
publication/suspended/explicit-exit/failed-creation cases and negative providers.
Initial84 ownership/object/device/boot cases pass41.59s on Windows. New portable
checks are registered for both CI hosts; new-stage Mac results remain pending.

stability_transitions_game.py adds --guest-lock and --watch-guest-execution for
fresh disposable Windows tests. Options are applied after prepare_run strips
parent diagnostic environment variables, and actual requested modes are recorded
in provenance. Defaults retain the shipping lock-off policy. Check both off/on
with the usual bounded Story movement/fire/pause route; do not use personal
settings/saves. Build/game/provenance and migration results belong in STATUS.
Patch0259 follows0258; the maintained tail is57 and all five historical omission
sets include the new dependency. See [exceptions](guest-exceptions.md),
[checkpoint transport](context-execution.md) and [lifetime scopes](kernel-objects.md).

Final Windows acceptance:281 broad109.32s plus149 full migrations/context/gates
905.18s pass; Release and actual provider bindings/current source/archive audit
pass. Sameb4bb executable passes fresh99.352s lock-off and95.011s lock-on Story
movement/fire/pause runs; counted concurrency peaks2(off)/1(on), on overlaps0.
Six captures reviewed, provenance matches and26 protected files unchanged. See
STATUS for exact hashes and Windows-only scope. Previous36 CI is green on both
hosts; new-stage CI, native Mac title and actual SEH import binding remain open.
