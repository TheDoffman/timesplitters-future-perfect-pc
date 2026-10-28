# Guest handles and object lifetime

Project providers in `src/runtime/guest_objects.c`, `guest_object_scope.c` and
`guest_object_services.c` replace the old bridge handle arrays through additive
`patches/compiler/0215-independent-kernel-objects.patch`. The original adapters
`guest_kernel_objects.inc` and `guest_kernel_threads.inc` are included by the
kernel bridge. CMake links the same providers on Windows and macOS.

## Ownership and ABI

Generation-bearing 32-bit tokens identify shared objects, independently of host
handle bits. Duplicates share object identity; separately adopted native refs
never collapse merely because POSIX returns equal pointers. Exhausted token
generations retire slots. Object refs and scoped leases outlive handle closure.
Native close and body disposal occur outside the metadata lock. A successful
host close remains recorded through a failed guest cleanup; uncertain ownership
is accounted for and never blindly retried. Initialization requires quiescence.

Guest outputs resolve as complete writable spans before creation or ownership
changes. Unaligned words are serialized explicitly. Type checks use the kernel's
distinct object-type exports. ObReference returns an owned guest reference;
fastcall Obf reference/dereference use ECX and preserve zero stack cleanup.
Publication selects one body; unused candidate allocations are released. Only
thread/manual-event signal probes may wait with zero timeout, avoiding consumption
of auto-event/semaphore/mutant state. Single-process duplicate supports close-
source/same-access options; permissions and handle attributes are not modeled.

## Threads and operation scopes

Creators reserve metadata, descriptor, stack and TIB before native startup. A
publication gate prevents guest entry until the parent handle, worker reference
and original full thread object are associated. Current-thread pseudo handles
and parent handles share that object. Suspended startup remains suspended. IDs
come from guest thread storage at object+0x12c. Explicit exit and ordinary return
release scopes/root refs and stacks. TIB/object storage retains its established
guest-memory lifetime; an interior object pointer is never freed as a standalone
allocation. Main and auxiliary threads retain their own identities. USB exit
releases auxiliary ownership; the existing timer worker runs until process exit.

Every thunk call owns a borrow scope across blocking host calls and callbacks.
Duplicate token borrows reuse leases; pseudo caches include identity keys. Native
guest setjmp/longjmp hooks unwind only abandoned scopes before register transfer.
Audio event completion owns an explicit scope outside thunk dispatch. Published
object pointers resolve through the same owner instead of embedded-event fallback.
The obsolete unread debug-handle setter/getter/storage has been removed.

## Layout facts and remaining work

The Xbox thread body has a 0x110-byte KTHREAD prefix, consistent with its thread
ID at ETHREAD+0x12c. Dispatcher Size is measured in guest words: thread0x44,
event4, mutant8, semaphore5 and timer10. These are ABI constants, not imported
implementations. Reference layouts: [Xbox public kernel declarations](https://github.com/mborgerson/xbox-includes/blob/master/include/xboxkrnl.h);
the word-size convention is also visible in [ReactOS queue initialization](https://doxygen.reactos.org/db/d6c/ntoskrnl_2ke_2queue_8c_source.html).
No external implementation bodies are incorporated into these providers.

This stage does not replace native kernel_ob.c, named objects, full file/process
body serialization, embedded Ke synchronization, scheduling, device workers or
whole-runtime shutdown. Thread template fields beyond the dispatcher retain the
project thread-memory contract. Pointer/permission mappings must remain stable
through a service call. There is no live-owner reinitialization/drain API.

## Checks

`tests/compiler/test_guest_objects.py` compiles original owner/scope/service
fixtures, real Windows wait/thread adapters and native guest continuations.
Negative controls must compile before failing behavior. The Windows binding
fixture runs both bounded memory and real project allocation/thread-memory/image
providers, covering descriptor/host creation failure, publication, normal and
explicit exit, suspended startup, auxiliary cleanup and scoped event signaling.
Private TIBs persist while guest stacks release. Shared portable contracts remain
selected on both CI hosts; the actual Windows binding test skips on Mac. Native
Mac compilation, title gameplay and active-hook foreign-fiber ownership require
separate acceptance. See MAC-118 and STATUS for measured production evidence.

The standalone `tests/media/audio_stream_timing.c` fixture supplies the scoped
`xbox_bridge_signal_event` boundary, rather than the retired raw-handle resolver.
Its completion scenario checks the exact token, status/size publication before
signaling, no signal before consumption and no repeat on a later tick. This is
an audio adapter contract; real event ownership remains tested by the kernel
binding fixture. No production source changes are needed for this fixture repair.


## Original boot and logging (Windows verified; CI pending)

`src/runtime/guest_kernel_boot.c` implements lifecycle and serialized diagnostic
output. `0216-independent-kernel-boot.patch` links this provider in place of
kernel_thunks.c and removes the unused native pointer-table/resolver declarations.
The guest bridge retains actual import dispatch. Guest-lock initialization/entry,
lifted entry and HAL timers preserve their startup order. Log severity clamps,
quiet mode and file-open stderr fallback are shared behavior. Initialization and
shutdown require quiescence; this is not a worker join or drain implementation.
Native object-manager and device code remains inherited at this boundary.

`tests/compiler/test_kernel_boot.py` tests startup order, repeated active init,
stopped shutdown, real host threads writing400 complete unique log lines,
filter/quiet/fallback, six compiled negative controls, and isolated compilation
against actual runtime headers. Synthetic contract headers remain test-only.
Windows maintained-path12passed and context/backlog21passed; Release build and
built-in checks passed. The generated kernel project selects original boot and
its rebuilt archive has no raw native resolver/table. Fresh Windows gameplay
passed as recorded below; Mac execution and CI for this boot change are pending. MAC-118 remains
Awaiting validation; no new gameplay result is implied by fixture compilation.

The boot fixture counts both acquisition and release through wrappers forwarding
into real host critical sections. Every one of403 accepted lines must use both;
mutations removing either operation are rejected. This complements the400-line
threaded uniqueness/completeness check, rather than inferring lock use from
scheduler-dependent line interleaving alone. Quiescent shutdown remains required.


Windows boot replacement gameplay acceptance: the fresh isolated Story run
reached scene4/mission1, moved11.046 forward and5.884 backward, fired ammo8to6,
froze ticks through40 pause samples and advanced through60 resume samples.
The final checkpoint was102.869s within a240s cap. Gameplay, pause menu and
resumed captures were reviewed. The rebuilt executable SHA-256 is
`da6ad5f39521a8b2040630354e3e7bd3e95ec8795d951541db83036dabdcd33a`.
All26 protected files retained hash, size and modification time. This was a
minimized synthetic-input Classic run with movies skipped; physical input,
audio listening, full campaign and native Mac gameplay remain unverified.
Local snapshot/tracking/migration checks passed52 tests in1755.05s.
Boot source is part of PR #393; CI for the new boot revision remains pending.
