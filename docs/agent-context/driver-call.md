# Original driver-call composer

Stage0222 selects `guest_io_call.c/.h` and `guest_io_target.c/.h` in the shared
kernel library through0222-original-driver-call-composer.patch, after0221 and
before2090. `guest_io_request_peek` validates the same next slot as advance while
preserving guest bytes. Historical fixture inputs omit0222 with its prerequisites;
complete upgrades assert the two new providers appear once.

These are original maintained replacement components, not installed kernel
IofCallDriver/CompleteRequest routes. Native legacy bridges remain selected.
Completion/status/event/APC/thread routing, caller-owned packet adoption,
StartIo/next queues, cancellation, synchronous builders and backend acceptance
are still needed before that bridge replacement is complete. No toolkit
independence or native Mac title-gameplay claim follows from this stage.

## Byte layout and target selection

The generic codec interprets only the four-byte major/minor/flags/control prefix
and round-trips the remaining20 bytes verbatim. Dispatch consumes the operation
byte and24-byte slot stride without assigning per-operation fields or stamping
DeviceObject. This is the composer's preservation contract, not proof of every
Xbox IofCallDriver side effect.

Subsequent supported-US title evidence supersedes the initial uncertainty about
the+20 field. Original instruction46F080 obtains CurrentStackLocation from IRP+5c;
46F0AA writes the device address to slot+14(hex). Read/write routine46EEE1 uses
length+4, buffer word+8, and byte-offset low/high words+c/+10(hex), with flags+2
bit80 selecting direct versus UserBuffer-relative buffer interpretation. Thus
the device field exists in this title's ABI. Earlier descriptions calling it
invented were too strong. Kernel dispatch assignment semantics and the complete
transfer/build/completion pipeline still require integration acceptance.
These facts come from the supported US XBE hash in AGENTS, not a Windows layout;
no title implementation bytes or instruction listings are shipped.

Reference declarations:
[Xbox public driver/device/IRP types](https://github.com/XboxDev/nxdk/blob/8d8438186c5dcf1187672b686a91c4603be9e111/lib/xboxkrnl/xboxkrnl.h),
[Xbox emulator slot declaration](https://github.com/Cxbx-Reloaded/Cxbx-Reloaded-legacy/blob/96aabe72e238674a22a61695fb6f5295259edf62/src/core/kernel/common/types.h).
The latter asserts24 bytes but omits the device field observed in the title;
its incomplete fields/comments must not override title evidence. No per-stack
completion-routine field has been established.
No external implementation bodies are copied.

Target selection validates full72-byte device and68-byte driver spans, device
driver word+8, bounded next packet slot and all14 major-operation indices.
Driver dispatch entries start at+12. A nonzero routine is still only a value;
the caller must hold real device/request references, serialize request/table
mutation, preserve mappings, and resolve an allowlisted translated/manual entry.
Failed target selection preserves every input and the output value.

## Calls and lifetime

The composer consumes exactly one caller-held request client ref, including
pre-entry failure and native unwind. A cleanup guard and0221 request scope hold
request/device ownership. It prepares the callback before consuming the slot,
then publishes ownership immediately before guest entry. Normal rollback restores
only the consumed location/pointer and is allowed only before publication with
the expected metadata still intact. It never modifies any stack-slot payload.

Nested and later pending forwards share one operation pin; each callback owns a
separate pin. Device identity travels in the two stdcall arguments and host-owned
deduplicated leases. Normal return copies guest EAX only after balanced callback
and successful scope cleanup. Malformed published return reports UNCERTAIN and
retains pending ownership. Native guest jumps unwind scopes before their C frames
are discarded, retaining operations that can still legitimately complete.
Trusted lookup/mapping helpers return normally and preserve guest architecture.

## Acceptance

Maintained test_io_target.py covers all14 majors, exact spans, wrap and invalid
pointer/index/routine cases without writes. test_io_call.py and test_io_forward.py
execute real composed pending, inline, malformed, rejected-admission, native-jump,
nested and asynchronous-forward scenarios with compiled fault controls. Whole
slot snapshots reject changes to either the proposed device word or any other
opaque payload. The Windows case uses real request pool/registry/heap/device
ownership and VirtualFree's request/binding metadata during nested completion;
VirtualQuery confirms both pages are gone before the outer composer returns.

test_io_stack.py tests literal bytes, unaligned spans, rejected-output preservation
and all5120 opaque-byte variants. Existing request, pool, owner, scope/concurrency,
actual-header and callback checks remain required. The three new suites are
selected explicitly on both CI hosts. Build/archive selection and title regression
are separate from these service fixtures; measured results belong in STATUS and
the MAC-118 follow-up.
