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
