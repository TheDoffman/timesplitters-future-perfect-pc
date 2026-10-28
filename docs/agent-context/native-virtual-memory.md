# Native Nt host mappings and reservation ownership

`src/native/native_vm_host.c` is the native NT mapping boundary.
`src/runtime/native_reservations.c` owns reservation/page-state metadata on hosts
without NT virtual memory. Its headers and the portable query record are under
`src/runtime/`. Patch `0212-independent-native-vm-host.patch` replaces the three
native host callbacks in `kernel_memory.c` and routes native Mm protection through
the same boundary. Guest Nt/Mm ownership, ordinal registration and guest argument
cleanup do not change. `host_virtual_memory.c` still owns native argument/status
and output-publication policy, including ignored ZeroBits and unchanged sizes.

## Host boundary

Windows delegates allocation and release flags to VirtualAlloc/VirtualFree and
queries real MEMORY_BASIC_INFORMATION fields. The adapter retains pointer/size
widths, allocation protection and PartitionId. Native ABI serialization starts
from explicitly zeroed bytes and copies only after a complete successful query.
The existing wrapper bounds partial output copies and leaves outputs unchanged
on failure. Mac uses its existing native structure without a PartitionId member.

Darwin reserves anonymous inaccessible mappings with mmap hints, never MAP_FIXED.
A different address is released and reported as failure; an exact hint cannot
overwrite someone else's mapping. Ordinary reserve, commit, combined reserve/
commit, decommit and release are implemented. Other allocation/free flag families
are rejected on this host, not silently treated as supported. Nonzero ranges
round outward to actual host pages; zero-sized allocation fails. COMMIT with a
null hint owns a new reservation. This is not the main guest arena or its aliases.

The owner has 4096 records and one state byte per host page, allocated before
mapping. It records exact base/length, initial allocation protection and fresh,
committed or discarded page states. Capacity, metadata allocation and range
overflow failures happen before host mapping. Ownership and callbacks serialize;
callbacks must not reenter the owner. Metadata lives for the process, initialized
once with pthread_once. Kernel shutdown still does not prove worker quiescence.
Matching releases reclaim mappings; process exit reclaims any survivors. The
standalone quiescent stop operation can retry failed releases.

RELEASE requires the exact owned base and zero size; it unmaps the recorded
length. Foreign, interior and already-released pointers cannot reach munmap.
Failed release retains the record. Addresses are not generation tokens: callers
must not keep stale pointers after release/address reuse.

## Commitment, protection and failures

Decommit revokes access without dropping the address reservation. MADV_FREE is
best-effort physical reclamation; success does not promise immediate reclamation.
Discarded contents are explicitly zeroed on recommit, even when advice failed or
the OS retained every byte. Recommitting already committed pages preserves their
contents. The Darwin implementation applies requested protection to those pages.
Zero-size decommit at the exact base covers the whole reservation; an interior
zero-size request fails. Windows retains its OS semantics for all these calls.

Native Mm protection uses this owner too: reserved or mixed reserved/committed
spans cannot become accessible through the protection-only route. Wholly
committed spans can change access; foreign mapped spans retain ordinary host-page
protection behavior. Partial overlap with a reservation is rejected. Guest Mm
protection remains on its separately validated guest mappings.

OS protection failure is not assumed transactional. A dirty-page recommit first
obtains write access, clears contents, then applies requested protection. If that
last step fails, successful no-access rollback keeps the pages reserved. If
access is uncertain, the entire reservation is quarantined: further commit,
protect and query operations fail, but exact release remains possible. Earlier
successful subspans of a multi-state commit stay recorded if a later subspan
fails. A failed new allocation whose cleanup also fails retains quarantined
ownership instead of forgetting the mapping. None of these failures publishes a
successful allocation to the caller.

Direct accesses and external mapping changes must be quiescent while operations
run. Do not unmap/remap a reservation or bypass this boundary to change its access.
The metadata lock does not pin arbitrary CPU loads or stop guest/device workers.

## Queries and checks

Owned queries intersect real host-region boundaries with contiguous commitment
state and report the recorded allocation base/protection. Reserved pages report
reserved state with protection zero. A mapped foreign address uses a real host
snapshot but leaves unknown allocation origin/protection/type zero; it is not
invented ownership. An unmapped Darwin address or quarantined owner fails instead
of fabricating a committed writable page. These are snapshots, not lifetime pins.

`test_native_reservations.py` covers 4/16 KiB page states, ranges, retained failed
release, partial failures, capacity, metadata allocation failure and concurrency.
`test_native_vm_host.py` exercises real host calls and controlled POSIX APIs,
including retained discarded contents, rollback, quarantine and exact collisions.
`test_native_vm_adapters.py` executes actual callbacks and native exports with
wide addresses/sizes, complete native records, padding and failure outputs.
Fifteen compiled negative controls must compile before failing their runtime
assertions. Existing VM/Mm fixtures retain their assertions and gain the new host
dependencies plus a reserved-page protection test. Native Mac execution, build
and title gameplay are separate acceptance; see STATUS and MAC-115.

API references: Microsoft's [VirtualAlloc](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc)
and [VirtualFree](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualfree),
and Apple's [mmap](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/mmap.2.html)
and [madvise](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/madvise.2.html)
manuals. These provide OS contracts, not copied implementations.

[Main guest mappings/aliases](guest-host-mappings.md) now have project providers.
Worker quiescence and process-lifetime teardown, other kernel/device services,
title/delivery integration and inherited fixtures remain. Final independence
requires generation/build/test/package and provenance checks with both the old
toolkit checkout and pristine snapshot unavailable.
