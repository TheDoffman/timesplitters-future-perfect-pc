# Original request construction and completion components

Stage0223 selects `guest_io_transfer.c/.h`, `guest_io_build.c/.h` and
`guest_io_complete.c/.h` through0223-original-transfer-completion.patch, after0222
and before2090. `guest_io_pool_construct` prepares a fresh private owner before
registry publication; the existing allocate API delegates with no initializer.
Historical inputs omit0223 with its prerequisites; complete upgrades assert all
three providers are present once.

These are shared original replacement components in the kernel library. They do
not yet reroute imported synchronous builders, IofCallDriver/CompleteRequest,
StartIo/next queues or caller-owned packet adoption. Real guest event/APC/thread
adapters and backend acceptance remain required. Full toolkit replacement and
native Mac title-gameplay acceptance remain unfinished.

## Transfer and packet facts

The generic0222 slot codec preserves all20 non-prefix bytes. The separate transfer
view interprets only major2/3 read/write slots, whose fields are established by
the local supported-US title; see [instruction facts](driver-call.md).

| Guest field | Offset/width | Interpretation |
| --- | --- | --- |
| Transfer length | slot+4/4 | Byte length |
| Transfer buffer word | slot+8/4 | Absolute address if flags+2 bit80; otherwise relative to IRP.UserBuffer |
| Transfer byte offset | slot+12/8 | Low/high guest words; preserve all bits |
| Transfer device | slot+20/4 | Guest address data, not host ownership authority |
| IRP status/information | +10/+14 hex,4 each | Exact32-bit result words |
| IRP UserIosb/UserEvent | +1c/+20 hex,4 each | Guest pointers; target spans/lifetime need backend ownership |
| IRP UserBuffer | +30 hex/4 | Buffer base |
| IRP SegmentArray/LockedBufferLength | +34/+38 hex,4 each | Raw values requiring backend validation |
| IRP Thread/OriginalFileObject | +50/+60 hex,4 each | Guest identities requiring real references |

The [pinned nxdk declaration](https://github.com/XboxDev/nxdk/blob/8d8438186c5dcf1187672b686a91c4603be9e111/lib/xboxkrnl/xboxkrnl.h)
has five DriverContext words, so Thread is+50hex and ListEntry+54hex. The earlier
four-word assumption would incorrectly place Thread at+4chex. Public declarations
and title observations are interface facts; no foreign implementations, private
bytes or title instruction listings are incorporated. Flags, pending/cancel bytes
and the two overlay words are snapshotted without inventing APC interpretation
from raw overlay bits.

The transfer address calculator rejects32-bit address/range wrap and nonempty
null buffers; empty-null is a value. The packet constructor encodes the transfer
to private temporary storage, validates shape/spans, then initializes and fills
the packet. Rejection leaves the entire destination untouched. Stack counts1..127
are supported. Device/status/event range checks are not mapping/permission checks;
callers own stable disjoint spans and referenced-object lifetimes. Raw bytes can
be unaligned. No event or completion is fabricated by byte construction.

## Private construction and delivery ownership

Pool initialization callbacks run outside pool/registry locks, before publication.
They return normally, execute no guest code, preserve control refs/READY phase,
and may construct bytes or transfer an owned attachment. Failed preparation or
concurrent stop disposes the creator and its attachment without publishing a
partial request. Existing known-retained versus uncertain allocation cleanup
accounting remains in effect. Stop/drain still requires caller quiescence.

The completion coordinator consumes one caller-held client ref on every nonnull
path. It snapshots the owned packet under the owner lock and supplies the host
owner's address separately from guest data. Backend prepare validates/pins full
writable IOSB and notification targets before completion admission. A successful
claim writes the two IOSB words before notify, outside metadata locks. The backend
must provide release ordering and actual event/thread/APC routing; accepted async
work owns its own continuing references.

Release runs once after successful prepare with PRIVATE, DONE or UNCERTAIN
disposition. Only DONE may retire a constructor-owned registry ref. Failed or
nonlocally abandoned notification retains an UNCERTAIN operation, never invented
success or an unsafe free. Backend references must be accounted/quarantined on
uncertainty, never blindly retried. Cleanup failure is reported even after known
delivery completion; the caller's client ref is still released. Unexpected child
scopes and native guest jumps unwind before C frames disappear. Ops/context and
mappings stay valid through these callbacks; guest packet mutation is serialized.

## Acceptance routes

Maintained test_io_transfer/test_io_build cover literal bytes, all256 operation
tags, all127 stack counts, unaligned guards, invalid-span/output preservation,
buffer flags and wrap. A missing-last-byte fault required a nonzero high device
byte in its independent oracle; zero-filled expected data had hidden that omission.

test_io_construct uses actual heap/pool/registry ownership to reject early
publication, preparation failure and stop during initialization, including
attachment disposal and pending lifetime. test_io_complete covers exact IOSB
ordering, rejected preparation/full-span validation, competing completion,
malformed packets, failed cleanup, nested scopes and native jumps with persistent
uncertainty accounting. Fault controls must compile before runtime rejection.

test_io_pipeline_windows constructs privately, runs nested translated-entry
callbacks, publishes IOSB before a real Windows event, retires registration and
VirtualFree's both request/binding metadata only after callbacks drain.
test_io_title_layout decodes12 original field-access facts with exact US hash
validation; absence skips, a wrong revision does not substitute. It ships no XBE.
All six suites are selected on both native CI hosts; the Windows OS-event case
and private title presence remain distinct from portable contracts and Mac title
execution. Measured build/archive/game/CI acceptance belongs in STATUS and MAC-118.

## Host-owned request queue

Stage0224 adds guest_io_queue.c/.h through0224-original-request-queue.patch,
selected once after0223. This is an original portable FIFO component, not yet
an IoStartPacket/StartNext bridge or guest StartIo implementation. Existing kernel
imports remain selected. Key ordering, cancellation, device lifecycle, guest
CurrentIrp/Busy/list mirrors and callback entry adapters still need integration.

A queue owns a separate client ref per waiting/current request. Push borrows a
caller ref and accepts ACTIVE/PENDING operations only. Duplicate host identities,
closed admission, capacity exhaustion and retain overflow reject without transfer.
Next compares the expected current identity, atomically advances FIFO state and
returns an additional caller-owned ref. Output must be disjoint host storage.
Completed packets may remain queued; selecting one does not authorize guest entry.
Callers serialize cancellation/selection policy and supply independent callback
scopes, device leases and allowlisted guest dispatch before execution.

Storage is bounded caller-owned host memory, not authority derived from guest
links. Locks order queue then owner; disposal runs outside both locks and may
reenter the queue. Stop closes admission and drains waiting refs outside the lock,
retaining current until explicit next(expected). It never fabricates cancellation
or completion: pending/uncertain operation refs remain independently accounted.
Callers join users before destroying queue/storage. Helpers return normally and
execute no guest code; no queue metadata may reside on an abandoned guest C stack.

The maintained test_io_queue fixture covers FIFO wrap, identity mismatch, duplicate
admission, close/drain, retain overflow, completed waiting owners, reentrant
cleanup and a separately pinned callback surviving inline completion and next.
Eight native host threads submit distinct owners; each is selected exactly once.
Seven deliberate bad variants must compile, then fail their runtime contracts.
Shared CI runs the fixture on both native hosts; Windows acceptance does not prove
Mac title gameplay or a completed queue/backend adapter. Evidence is in STATUS.

## StartIo callback composition

Stage0225 selects guest_io_startio.c/.h through0225 after0224. tsfp_io_startio
consumes one caller-held client ref, pins the request-owned device, validates its
full72-byte body and driver68-byte table, then selects DriverStartIo at offset0.
Only an allowlisted translated/manual entry is permitted. Its void stdcall gets
(Device,Request); guest EAX is ignored. No stack location or packet payload is
advanced or written. A forward scope adds exactly one callback pin to an existing
ACTIVE/PENDING operation; READY/COMPLETE/UNCERTAIN admission rejects. Prepared
pre-entry rejection rolls back only that callback ref, preserving concurrent
completion. Normal returns restore guest architecture. Malformed/native-abandoned
execution unwinds child scopes and retains pending operation ownership.

Queue selection and publication of CurrentIrp/Busy/list state remain the backend
adapter's responsibility. The composer performs no queue transition, supplies no
key/cancel policy and does not itself install kernel StartPacket/Next routes.
Caller owns queue/device serialization and stable mappings, with no host metadata
locks held across guest entry. Factory/ops/scopes outlive callbacks and cleanup.

The portable test_io_startio case exercises17 active/pending/READY/completed,
preflight/admission rejection, complete-during-admission, inline-complete/next,
malformed return, child scope and actual native longjmp scenarios with real FIFO,
request-owned device leases and metadata disposal. Six fault variants compile
before runtime rejection (wrong routine/argument order, initial instead of forward
scope, lost client ref, missing abandonment and accidental slot advance).
The separate Windows case runs translated driver -> StartIo -> inline completion,
then VirtualFree's both request and binding metadata before the outer call returns;
VirtualQuery confirms both pages are unmapped. This is real host lifetime evidence,
not installed guest queue/event/APC/backend or native Mac title acceptance.

## Device queue mirrors

Stage0226 selects guest_io_queue_wire.c/.h through0226 after0225. Queue admission
now claims an exclusive host waiting_queue tag and one client ref under the owner
lock. A packet may be current above and queued below, but never waiting in two
lists. Taking an entry clears Inserted before releasing membership under the same
owner lock. Publishers must never touch detached/current entry bytes: a lower
queue may already own them. Metadata lock ordering remains queue -> owner.

Atomic submit either starts an idle empty queue (separate current and returned
caller refs) or appends behind current. READY, duplicate, closed, full, membership
or ref-overflow rejection preserves output/state. Idle waiting work created by
push must be selected with next(NULL) before submit; it cannot be overtaken.
Stop/next release queue clients outside locks. Failed matching-tag detach retains
accounted waiting refs; private host metadata must remain intact. Completion/cancel
policy and shutdown joining remain external responsibilities.

A quiescent empty queue may bind a caller-pinned72-byte device body to a trusted,
non-failing publisher. It validates DeviceObject/queue header/ranges before any
write and reconstructs CurrentIrp+10hex, Busy+2b, head links+2c/+30 from host state.
IRP queue entry links+3c/+40 and Inserted+48 are established by the pinned public
Xbox declaration (five DriverContext words, Thread+50). It never follows guest
links, and preserves SortKey+44, fifth context word+4c, Thread+50, payload and
other device bytes. List-entry addresses are host-owned request.address+3c.
Publisher runs under queue lock, touches only retained waiting entries/device
bytes, takes no other locks and executes no guest code. Stable disjoint mappings,
real device leases and publisher/context lifetime are caller contracts.

The maintained test_io_queue_wire suite covers atomic start/FIFO wrap, unaligned
literal links, corrupt guest links, byte preservation, exclusive membership and
upper-current/lower-waiting transfer without an old publisher clobbering new links.
Eight compiled bad variants must fail at runtime. test_io_startio now binds these
actual mirrors and checks them before callback/after inline complete-next across
its portable17 scenarios. Shared CI selects both suites on Windows/Mac. This is
not yet a persistent per-device registry/lease lifecycle, keyed scheduling,
cancellation, kernel import route or full storage/event/APC backend. Existing
kernel queue bridges remain selected until those adapters are integrated.

## Persistent device queue lifetime

Stage0227 selects guest_io_scheduler.c/.h through0227 after0226. A bounded
caller-owned record/entry array hosts persistent queues, mirrors and real device
leases. One metadata lock protects registry and queues, ordered before request
locks. PREPARING reserves capacity without exposing a half-built queue; competing
preparation rejects rather than claiming success. Pinning and final lease release
run outside metadata locks. Slot generations never wrap/reuse exhausted values.

Each noncopyable API ticket owns a separate record reference and validates slot
state/generation before use. The registry's device lease survives idle/current/
waiting work until explicit close, empty queue and all API refs drain. Device
pointers cannot be reused while that lease is held. Call close for deletion,
even when idle; callers must own legal device use and coordinate factory teardown.
Closed live records remain accessible with create=0 to retire a known current.
Submit pins matching request-owned device bindings before admission, independently
protecting pending operations after queue drain. Failed admission may retain this
pin in the request attachment until final disposal; it never invents completion.

Close marks record and queue admission closed under one lock, then drains waiters
outside locks while retaining current and active API refs. Global stop closes all
admission in its initial locked sweep, including preparing creators, before drain.
Reference exhaustion can prevent a complete drain; ownership remains accounted
and caller shutdown must join users and complete/cancel pending work. Final device
lease release runs outside locks; failure quarantines the record and lease without
retry/reuse. Normal object-provider cleanup uncertainty remains accounted by that
provider. Manager/context/storage outlive every pending/quarantined resource.

Thread-affine queue scopes release API tickets before native C-stack abandonment.
Scheduler StartIo consumes the selected caller ref and uses the matching factory,
request device binding, live record and shared callback composer; it holds no
metadata locks over guest execution. Inline complete/next may empty a closed
queue, but its device lease remains until the outer API scope returns/unwinds.
Normal end remains required; callbacks and guest mutations are externally serialized.

Maintained test_io_scheduler covers real device heap ownership, scoped inline
completion/deletion, actual native longjmp, closed waiters retaining pending
operation/device references, rejected lookup, capacity/generation guards, bad-body
preparation, stop-during-preparation and submission at the close/stop interleaving.
A controlled lease-release failure preserves/quarantines the actual lease; no
manager retry occurs. Only fixture recovery releases the deliberately retained
lease after checking accounting. Eight compiled bad variants must fail at runtime.
Both native CI hosts select it; host contracts do not establish Mac title gameplay.
Kernel import routing, caller-owned request adoption, keyed/cancel policies and
actual synchronous/async storage/event/APC backend integration remain unfinished.
Existing kernel queue bridges still use legacy implementations.

## Caller-owned request adoption

Stage0228 extends the already selected request/owner/pool providers, so no new
runtime patch or CMake source is needed. request_validate reads the complete
initialized layout (including final consumed slot), expected type/size/count/
location/current-pointer bounds, preserving all bytes/output on rejection.
owner_adopt creates fresh READY host control around exact caller storage without
initializing/changing that storage or inferring an operation from guest status.
Caller owns stable, quiescent mappings until every host ref/callback drains.

pool_adopt reserves bounded metadata, claims the exact range before resolving or
reading candidate bytes, validates type6 and exact declared size, optionally
prepares host metadata before registry publication, then releases the creator ref.
Preparation returns normally, executes no guest code, preserves packet/control
state and may transfer an attachment. Failure/removal/stop releases attachment and
metadata only; caller guest storage is never sent to the pool guest deallocator.
This is not a guest virtual-memory pin; callers must not free/reuse pending storage.

A protected all-node range ledger covers owned and borrowed packets, including
detached pending owners and retained/uncertain cleanup. It blocks duplicate or
partial-overlap adoption even after the registry entry disappears. Owned allocation
uses the same ledger. A unique allocator can reuse actually consumed owned storage
while an old deallocate callback has not yet returned/unlinked metadata; only a
new allocator-owned node can claim through that owned cleanup window. Borrowed
adoption still waits for final claim retirement. Failed cleanup keeps its claim;
known retry semantics and unknown no-retry accounting remain unchanged. Registry
-> owner/pool lock ordering is preserved; allocator/metadata callbacks run outside
locks. Range retirement removes the exact node rather than resetting the ledger.

Maintained test_io_adopt covers128 stack counts/all8256 locations, literal unaligned
packet preservation, invalid output/control preservation, READY admission despite
advanced guest state, duplicate/overlap rejection before reads, detached/pending
lifetime, failed preparation/stop, retained owned cleanup and actual Windows host
metadata unmapping while caller storage remains. A deterministic deallocation
handoff verifies owned reuse without permitting premature borrowed adoption.
The existing Windows four-worker1004-allocation regression must continue to pass.
Seven bad variants compile before runtime rejection. Its Windows pipeline adopts
caller-built bytes, runs nested translated drivers, delivers IOSB before a real
event, removes registration and unmaps request/binding metadata while leaving
caller heap ownership/data intact until the caller explicitly frees it.

Shared CI selects adoption contracts on both hosts (Windows event/unmap remains a
separate host case). This does not yet install IoInitialize/CallDriver/Complete
kernel routing or implement event/APC/storage/key/cancel backend policy. Existing
IoInitialize caller-storage behavior remains unchanged until full bridge integration.


Stage0227 Mac CI follow-up:952b7f3c runtime40945393065 failed only the deliberate
missing-unlock fault, whose non-recursive pthread lock blocked until30s timeout;
2009 other checks passed. The fixture now checks thread-local lock depth before
acquiring either native mutex, so the bad variant reaches runtime assertion on
both hosts. Fatal warnings and timeout limits remain unchanged. This is a fixture
control repair, not production lock-policy evidence or a passing old-head rerun.
