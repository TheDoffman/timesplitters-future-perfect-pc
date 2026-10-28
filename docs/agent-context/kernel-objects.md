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


Mac fixture compilation builds inherited win32_compat.c separately with the
existing host diagnostic policy and undefined-behavior instrumentation. Strict
-Wall/-Wextra/-Werror still applies to the original boot source, fixture and
owned providers. Applying those added warnings to the dependency exposed unused
parameters in its pre-existing unfinished Darwin SecureZeroMemory branch; this
fixture repair does not implement that legacy routine or change production code.
The real-header probe keeps strict warnings on the original source, but marks
the inherited kernel-header directory as a system dependency: its old nested
comment otherwise fails Clang -Wcomment. Compiler/type errors remain errors.
Boot CI completed atfd63094f; subsequent37f9042d also passed all16 checks.
New request-stage CI remains pending.


## Guest next-packet ABI prerequisite

`patches/compiler/0217-guest-next-packet-abi.patch` corrects ordinal81 to one
32-bit guest argument and four cleanup bytes. The transitional native stub
still has a second desktop-style argument; the bridge passes a constant false
instead of reading a nonexistent guest argument. Six call sites in the
SHA-256-verified supported US executable, decoded from mapped function entries,
push only DeviceObject and immediately restore caller registers after return.
The prior eight-byte cleanup consumed that saved caller state. Public interface:
[Xbox nxdk declaration](https://github.com/XboxDev/nxdk/blob/master/lib/xboxkrnl/xboxkrnl.h).
No executable bytes/instructions are included in the synthetic fixture.

`tests/compiler/test_io_packet_abi.py` compiles the installed bridge adapter and
actual ordinal-cleanup function with a synthetic caller stack, checking register
restoration and that only one guest argument is read. Both compiled fault
controls must fail behavior. CI selects this test on Windows and native Mac.
This is a prerequisite repair for original device/request dispatch, not a full
queue implementation: IoStartNextPacket still invokes the transitional host
stub. Request storage was subsequently promoted through0218; the verified-disc
and event-body prototypes remain uninstalled.
Windows Release and built-in checks passed. Isolated Story acceptance reached
scene4/mission1, moved11.175 forward and6.185 backward, fired ammo8to6,
paused40 samples and resumed60, with captures reviewed. Last checkpoint99.207s
within240s cap; all26 protected files unchanged. Executable SHA-256
`21c2ba6bbfbc05eb78594d3b79d75099dcd7bf05025c0dc7d1d2099b8b5f55da`.
All22 generated files unchanged. Minimized synthetic-input Classic/movie-skipped
run; ordinal81 execution frequency, physical input, audio and Mac title gameplay
are not established. Compiled ABI/context/backlog24 checks and tracking16 checks
passed; full snapshot/migration and new-head Mac CI remain pending.


## Original request storage and initialization

`src/runtime/guest_io_request.c`, `guest_io_owner.c`, `guest_io_registry.c` and
`guest_io_pool.c` now supply request storage/lifetime through
`patches/compiler/0218-independent-kernel-requests.patch`. The original
`guest_kernel_requests.inc` replaces guest IoAllocateIrp59, IoFreeIrp72 and
IoInitializeIrp73; the old native initializer body/declaration are removed.
Startup initializes the pool once before guest threads. The actual guest heap,
full writable-span resolver, metadata allocator and two host critical sections
are adapters to the shared providers. No new OS policy is duplicated.

Guest request header0x68 and stack24 use explicit little-endian bytes; type6,
stack count/current location and current stack address initialize before
publication. The stack codec preserves64-bit offsets and unaligned spans.
Allocation validates count0..127 and bounds, owns one registry reference and
returns a guest address. Host phases/refcounts never come from mutable guest
bytes. Lookup retains under the registry lock; removal detaches before releasing
outside locks. Dispatch and pending operations own internal references, so
inline completion cannot destroy storage still used by a dispatch callback.
One completion publishes status/information before notifying. Final disposal
may reclaim the owner control block; no provider accesses it afterward.

The pool bounds live/in-flight/failed-cleanup metadata. Failures roll back;
definitely retained frees permit one explicit retry, uncertain frees remain
accounted without blind retry. Production heap-release failure is uncertain.
IoFreeIrp accepts only registered requests. Registered initialization accepts
only READY packets and cannot exceed their original allocation; caller-owned
storage validates the entire writable span. Reset/dispatch serialize on the
owner lock. Caller-owned memory/mappings remain stable during calls. Shutdown
admission/drain is a component API: workers/API callers must be joined and
pending operations completed before pool storage can be destroyed. The kernel
adapter currently lives for the guest-memory/process lifetime, with no full
worker shutdown or live reinitialization claim.

Xbox IoAllocateIrp consumes one argument/four cleanup bytes. IoStartPacket83
consumes three arguments/twelve bytes: a mapped supported-title caller agrees
with the public nxdk declaration. Its transitional host stub receives NULL for
its obsolete fourth compatibility argument. Actual bridge/cleanup fixtures
check caller preservation, both NULL/non-NULL optional keys and compiled faults.
The supported title does not directly import59/72/73; absence of direct imports
does not establish every possible callback path. New allocation/reset behavior
is verified through native contracts, separately from bounded title gameplay.

Maintained contracts: test_io_stack.py, test_io_request.py, test_io_owner.py,
test_io_registry.py, test_io_pool.py, test_io_pool_windows.py and
test_kernel_requests.py in tests/compiler. They include literal byte oracles,
compiled faults, actual project heap, reclaimable Windows metadata mappings,
four Windows workers1004 allocations/releases, pending lifetime across stop,
reentrant callbacks and actual runtime-header compilation. Portable contracts
are selected on both CI hosts; actual Windows fixtures skip on Mac. Windows maintained-path36 checks, Release/built-in checks and fresh isolated
Story acceptance passed. New-head Mac compilation/execution remains pending.

Remaining work: device creation/body/type/namespace ownership, queued StartIo
and next-packet dispatch, guest callbacks and nonlocal-exit scopes, full request
completion/cancellation/thread association, synchronous builders and actual
supported-disc configuration. IoBuildSynchronousFsdRequest still uses the
transitional native route and returns NULL. The verified-disc and explicit
object-body event prototypes remain uninstalled. They are not proof of a working
driver stack, full request ABI, Mac gameplay or complete toolkit independence.


Stage0218 Windows acceptance: all36 request/packet ABI checks and21 context/
backlog checks passed; package/path/symbol checks7passed plus2subtests. Release
build and built-in checks passed; generated project selects all five providers
once and the kernel archive has no native xbox_IoInitializeIrp symbol. All22
generated files are unchanged. Fresh isolated Story acceptance reached
scene4/mission1, moved11.175 forward/6.185 backward, fired ammo8to6, froze40
pause samples and advanced60 resume samples. Captures reviewed; final checkpoint
100.457s within240s cap. Executable SHA-256
`d260852719f82abe650bc26a4f8df7e53af7790a46e6364163548776417bf0a9`.
All26 protected files unchanged. Minimized synthetic-input Classic/movie-skipped
run; no physical input/audio/campaign/Mac title claim. Direct supported imports
do not exercise59/72/73, so native contracts establish their new behavior while
this game test establishes bounded regression coverage. Full new-stage local
snapshot/migration and new-head CI remain pending. No full independence claim.


Request-stage CI capacity repair: exactfb4aeb2a Windows native job123069441536
was cancelled at the30-minute job boundary while the native suite continued to
advance near its end, with no assertion failure reported. GitHub annotated
maximum execution time30m0s; dependent final gates failed because the selected
host job was cancelled. Mac native and both patch-upgrade jobs succeeded.
The native matrix now grants Windows45 minutes while Mac retains30. Every test,
per-test timeout, assertion, dependency pin and final gate remains selected.
No runtime source/build/game input changed; the tested d2608527 executable and
its existing Windows gameplay evidence remain the current product evidence.
New-head complete Windows CI remains required; this is not a passing result.
