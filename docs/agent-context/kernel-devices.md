# Project device and counted-name ownership

Stage0219 selects project providers with `patches/compiler/0219-independent-kernel-devices.patch`.
Authoritative code is `src/runtime/guest_device_*.c/.h`, `guest_callback_call.c/.h`,
`guest_object_names.c/.h`, `guest_name_service.c/.h`, and the four
`guest_kernel_device_host.inc`, `guest_kernel_devices.inc`,
`guest_kernel_device_events.inc`, `guest_kernel_names.inc` adapters.
Object owner/service/scope extensions stay in their existing maintained files.
The vendor snapshot remains an input; this stage does not prove independence.

## Creation and names

Devices own initialized guest heap storage and one creator reference, with no
fabricated native handle. Counted ANSI names copy their bytes and own separate
references; typed lookup gives the guest another reference to the existing body.
Removing a name or deleting a creator does not invalidate leases or guest refs.
Rebinding a name cannot let an older deletion remove its new owner's binding.
The full eight-byte descriptor, counted bytes and unaligned output spans are
validated. ASCII folding is optional; no Unicode/path/directory/symlink parsing
or nonzero parse context is claimed. Other policies need explicit providers.
Device type identity is distinct at export70; file type is71. Identity is opaque,
not a complete serialized Xbox OBJECT_TYPE. Existing kind values stay stable.

The explicit little-endian device prefix is72 bytes; extension is at+0x18 and
starts after that prefix. Queue and event headers have their own byte/word size
units. Constructor flags, initial references/stack size and extension limits are
project policy, not proof of all Xbox defaults. The public32-bit layout model
and original guarded fixtures cover offsets without native host struct copies.

## Deletion, callbacks and embedded locks

Deletion claims the creator once, marks the body and removes only its matching
name. Once ordinary refs/leases drain, exposed devices queue value tickets for
driver finalization. Callback storage stays owned until acknowledgement. Failed
publication never invokes guest deletion. Known-retained heap cleanup may retry;
uncertain disposition is counted and never blindly retried. Generation-bearing
tickets reject stale body-address reuse. Completed tickets cannot create new
pins; existing callback pins delay READY cleanup until released. Uncertain
callbacks remain quarantined even after those pins drain.

The bounded pump executes translated/manual callbacks only after the bridge
restores its guest lock/lifted state. No metadata lock crosses guest entry.
Normal calls validate stack spans, exact one-argument stdcall return cleanup and
restore project register state. Native guest jumps intentionally keep continuation
target registers. Scope hooks release abandoned event pins, quarantine the active
ticket and clear pump reentrancy state before the C stack is discarded.
These are normal-stack/known native-continuation contracts, not arbitrary foreign
fiber shutdown, callback recovery or a complete cancellation/worker-join service.

An embedded DeviceLock is recognized only at body+0x34. Ordinary owner leases or
an active matching callback ticket pin the parent; embedded storage is never
freed separately. Set/reset/try-wait share aligned atomic guest signal bytes.
Wait/signal wrappers pin through existing scheduling; a recognized device that
cannot be pinned fails instead of falling back to raw guest memory. Multi-wait
validates the complete unaligned pointer array, accepts1..64 entries and uses one
bounded batch cleanup scope. Partial failure and native unwind release all pins.
Other providers retain their existing routes. Wait/pulse scheduling, APCs and
full event fairness remain transitional. Owned reset/boost update guest signal
bytes; the existing wake-generation service remains the scheduling boundary.

The boost API's optional second argument is a thread-pointer reference, not an
increment. Thread-priority handoff remains part of the transitional scheduler;
this stage does not claim to implement it. Interface facts: [nxdk declarations](https://github.com/XboxDev/nxdk/blob/master/lib/xboxkrnl/xboxkrnl.h).

## Selection and acceptance

Shared CMake adds the same ten new providers on both hosts and omits kernel_ob.c.
The obsolete native object-manager/device create/delete declarations/bodies are
not selected; the actual guest ordinal bridge remains. Init runs after object
initialization and before guest threads. Device callbacks enter through the
bounded pump after guest execution state is restored.

Maintained tests are `tests/compiler/test_guest_object_names.py`,
`test_guest_body_only.py`, `test_guest_device_names.py`, `test_guest_device_body.py`,
`test_guest_device_factory.py`, `test_guest_callback_call.py`,
`test_kernel_device_host.py`, `test_kernel_device_headers.py`, and
`test_kernel_device_abi.py`, plus existing object/scope contracts. CI selects all
of these. The real Windows host fixture skips on Mac; portable checks and the
actual-header probe remain selected on both hosts. Source tests, compiled fault
controls, host binding, full build and bounded title gameplay are separate evidence.

The host fixture uses real Windows locks, actual runtime headers, the original
project heap and synthetic RAM/names/driver callbacks. It proves namespace/type,
creation failures, reference survival, callback/lock scopes and real native jump
cleanup. Its wait adapters are observation shims; it does not prove wait timing,
physical input, audio listening or title gameplay. STATUS and MAC-118 hold actual
revision/host results. Windows Release and bounded isolated Story acceptance passed on the dirty3b11d5b1 base.
Full current-stage snapshot/tracking/migration passed52 in1037.38s; new-head remote CI remains pending.
No physical CdRom name is published until the verified original backend/request
pipeline is installed. Native synchronous builders, packet queues and broader
kernel/file/driver/scheduling replacement remain unfinished.


Merged-main verification at96eb84a1: regenerated Windows Release/built-in checks
and fresh isolated Story movement/firing/pause/resume passed. Three generated
files changed for main Unlock hooks; current generator fingerprint and exact
source/archive/executable/game hash audit passed. All26 protected files unchanged.
The old unchanged22-file result applies only before that main integration.
Fresh merged migration and new-head CI remain pending; see STATUS and MAC-118.
