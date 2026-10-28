# Guest thread storage

`src/runtime/guest_thread_memory.c` and its header own main TIB/object/PRCB/TLS
construction, private TIB/TLS/object allocation, eight heap-backed guest stacks
and the sixteen fixed worker-stack slots. Additive
`patches/compiler/0207-independent-thread-memory.patch` removes the inherited
construction bodies, identity counter, stack counter and fixed-slot array.
The shared kernel links the same provider on Windows and macOS.

## Layout and initialization

The provider consumes the already loaded image's validated `tsfp_image_info`
and active project allocator. `tsfp_image_bootstrap` supplies the low-service
shift and TLS size, retaining the supported placement contract. The main TIB
is at 0x1000. Relative to the shift, the main object is at 0x760000, PRCB at
0x761000, TLS at 0x770000 and per-thread data at 0x770200. The main stack spans
0x780000 through 0xf80000; its initial top is end minus 16. Heap ownership must
start at or above that end. Initialization validates all bounds before writes,
zeros the main records, copies initialized TLS and fills its zero tail. The
title still owns writing its TLS index. Without TLS, object+0x28 retains the
fallback data address at shifted 0x700000.

The loader initializes the allocator, then thread storage, before starting
device workers. It checks the provider's shift/TLS result against image
preflight and installs main FS/ESP. Shutdown disables thread storage before
allocation ownership and host mappings. Init/reinit requires quiescent callers
and a new guest-memory lifetime; this does not join or stop host workers.

## Private records and compatibility

Each private TIB request copies the current main TIB, TLS and object into one
zeroed heap allocation. It rebinds SEH, TLS end, TLS slot zero, TIB->object and
the object's ID. Constructors serialize ownership and identity assignment.
Allocation failure leaves no partial object or consumed ID; IDs stop on
32-bit exhaustion rather than wrap to an existing identity. No-TLS images
retain the existing zero-return convention.

Other template bytes are intentionally preserved, including the TIB's self
and stack-limit fields and the object's data pointer. This retains current
title behavior; it does not establish complete Xbox per-thread semantics.
The source is the mutable main template, not an immutable copy of the XBE.
Constructor serialization does not synchronize arbitrary guest writes to that
template; callers retain their existing guest-execution synchronization policy.

Published TIB/TLS/object allocations remain live for the guest-memory lifetime.
The guest can retain identity pointers, and this layer has no object reference
counting that would prove them dead on host-thread exit. Handle/pseudo identity now shares the full thread object through the
[object owner](kernel-objects.md). Publication, failure cleanup and ordinary/explicit
exit are owned there; scheduling and host-worker stop/join remain replacement work.

## Stack ownership and callers

Heap stacks are 512 KiB, page aligned, with top at end minus 16. Eight may be
live concurrently. Releases match an owned top; an unrelated heap allocation
cannot be freed through this API. Freed slots can be reused. The pointer ABI
does not carry a generation token, so callers must not release a stale top
after its address has been reused.

Fixed worker stacks keep the existing sixteen-index ABI for 256 KiB slices in
the lower half of the main-stack region. A serialized mask claims the lowest
free slot. Acquisition preserves its bytes; only its owner may release it.
This index ABI likewise cannot distinguish a stale release after reuse.

`xbox_memory_layout.c` supplies thin ABI adapters. `kernel_thread.c` retains
host thread services, but no longer owns slots or an unread watchdog handle.
Both guest thread-creation bridges now use the original publication adapter
in `guest_kernel_threads.inc`; descriptor/TIB/native creation failure releases
unpublished ownership. The timer TIB failure still releases its fixed slot.
The object-owner guide covers startup, ordinary return and explicit termination.

USB `ohci.c` also uses these APIs: its controller thread gets a private TIB;
each interrupt acquires a fixed slice and releases it after the callback.
Timer/USB scheduling and device behavior remain separate, inherited services.

## Evidence and checks

`tests/compiler/test_guest_thread_memory.py` compiles the provider with fatal
warnings, original synthetic byte-layout fixtures and native host threads.
It checks full RAM preservation outside bootstrap writes, no-TLS/shifted/TLS
boundary cases, private field copying/rebinding, ID exhaustion, failure
atomicity, exact stack ownership, slot exhaustion/reuse and stopped/reinitialized
states. Four native threads create 400 distinct identities while cycling heap
and fixed stacks. Five broken variants must compile and then fail their runtime
contracts. Actual initialization and storage ABI adapters retain stack/timer probes.
`test_guest_objects.py` executes the current host-create/exit adapters with these
real providers on Windows; native Mac binding acceptance remains separate.

The optional private-US test verifies the required XBE hash, loads its bytes
through the project loader and checks bootstrap changes against an independent
byte-layout oracle across the fixture's 40 MiB RAM. It verifies the supported
20-byte TLS layout; it is not a production game run. Remote CI without that
private asset skips only that case. Both CI hosts select the suite. Production
build/game and migration acceptance is recorded separately in STATUS and MAC-111.
