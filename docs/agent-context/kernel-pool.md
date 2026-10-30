# Original pool imports and retired host-heap surface

Stage0235 selects guest_kernel_pool.inc in kernel_bridge.c through0235 after0234
before2090. The four original bridge bodies use the existing guest allocation
provider directly through xbox_GetGuestVirtualMemory: heap_allocate with16-byte
alignment, exact-base heap_release and allocation_remaining. KERNEL_SOURCES no
longer compiles kernel_pool.c, and kernel.h no longer declares its four unused
native xbox_ExAllocatePool/WithTag/FreePool/QueryPoolBlockSize functions.
No new allocator/parallel ownership implementation is introduced.

Ordinals14/15/17/23 retain4/8/4/4 guest argument bytes. Tagged allocation snapshots
both words; tags remain diagnostic data without ownership authority. Returns are
guest-width addresses, never native heap pointers. Current compatibility behavior
is preserved: zero/minimum requests own16 zeroed bytes, alignment16, failed
allocation returns0 without writes, release accepts only exact heap bases, null/
unowned/interior/physical/reservation releases leave ownership unchanged, and void
release clears EAX. Query preserves the shared provider's remaining-byte policy
for heap/physical interiors and excludes reservations/padding. It does not claim
full NT pool tag/accounting or native process-heap ABI compatibility. Provider
lifetime/locking/mapping contracts remain [allocation ownership](independent.md).

Caller audit covers installed runtime source/headers, selected title/generated
C/includes and project sources. The native four functions have no production
caller: only the excluded old kernel_pool definitions and excluded legacy
kernel_thunks resolver retain them. The real guest resolver uses original bridges.
The legacy resolver was already unselected and is not a supported parallel kernel
route. Historical files/vendor snapshot/patch bytes remain for reproducible unwind;
retirement is from compilation/declarations, not a full source provenance cleanup.
The ordinary xbox_Heap ABI remains for active native runtime consumers and already
delegates to the same original allocation provider.

Maintained test_kernel_pool compiles actual original import adapters against the
real allocation provider, guarded argument access and an independently configured
RAM/physical window. Literal checks cover unaligned arena floor->16-byte aligned
address, minimum zeroing/guards, tag width, remaining size, failed/overflow/capacity
preservation, invalid/interior/high-address release, live reservation/physical
ownership protection, reuse and inactive lifetime. Six compiled bad adapters must
fail at runtime. Its caller audit verifies original include selection, old body/
header absence, ordinal cleanup and no remaining active-source native references.
The existing independent randomized occupancy/lifecycle/concurrency/allocation
consumer suites remain required. Native archive audit excludes kernel_pool and
kernel_thunks members and verifies the retired exports are absent. Windows game
acceptance and native Mac validation are separate from these portable contracts.
