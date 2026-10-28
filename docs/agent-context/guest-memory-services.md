# Guest Mm services and memory statistics

`src/runtime/guest_memory_services.c` owns supported guest Mm policy using the
existing `guest_allocation.c` owner and `guest_virtual_memory.h` span resolver.
`memory_statistics.c` owns the nine-word report. `src/native/host_memory_stats.c`
supplies real host availability; all public interfaces live in `src/runtime/`.
Additive `patches/compiler/0211-independent-guest-memory-services.patch` replaces
all 25 Mm bridge bodies and the five remaining native statistics/physical/hint
function bodies. Ordinal registrations and guest argument-cleanup entries remain
unchanged. This does not retire the rest of the kernel or its data exports.

## Allocation and physical address policy

All guest ownership stays in the existing allocator; no second guest table is
introduced. Contiguous requests use the physical arena with 4096 alignment,
including its zero-byte page ownership. System requests reject zero and use
4096-aligned heap storage. Debug allocation rejects zero and uses 16 alignment.
IO scratch buffers use 4096 alignment and retain the heap minimum for zero size.
Guest kernel stacks use 16 alignment and return base plus requested length;
zero-size stacks retain their minimum heap block. Deletion uses the low limit.
Native zero-size allocation policy from 0209 is intentionally separate.

Contiguous free first tries physical ownership, then retains the historical
heap fallback. Other frees use heap ownership; ignored free sizes/tags remain
ignored. Queries use the allocator's remaining requested-byte count, including
its existing physical/heap behavior, not the native OS-region-size query.
Extended contiguous allocation retains the project's 0206 narrow-range fixed
placement rule and inclusive bounded-placement rule, with the same collision,
alignment, protected-range and overflow rejection. Allocation protection hints
do not change already mapped guest storage; IO physical/protection hints do not
create host physical mappings. Redundant bridge zeroing is removed because the
allocator already owns zeroing.

Physical translation subtracts the contiguous window base only inside that
window; other guest addresses retain identity. The native physical-address
adapter retains its full-width host pointer identity instead.

GPU instance queries retain fixed tail addressing: UINT32_MAX selects the
configured default, and zero returns the representable window-end address.
Oversized requests and invalid optional padding words now fail without writes.
The entire padding word is validated and written little-endian even unaligned.
These queries do not allocate or guarantee exclusive ownership: the default
tail is reserved at initialization, but a larger bounded claim may overlap the
ordinary arena as before. Do not describe this as dynamic GPU reservation.

## Access and output publication

`xbox_GetGuestMemoryServices` reuses the project's VM context and adds physical
window constants, query/protection callbacks and host availability. Complete
guest spans must resolve before host protection or guest writes. No inaccessible
guest bytes are read while resolving mapped-only protection targets, so restoring
access works. The 0209 page adapter rounds to real host-page boundaries and
normalizes the existing native protection modes. Callers still must keep mappings
and access stable through each operation; no worker join or protection transaction
is implied. Unknown spans cannot become arbitrary host protection requests.

Null protection queries retain zero; other unresolved queries return NOACCESS.
The guest void protection bridge still returns zero after host success or failure.
MmGlobalData remains a self-referential placeholder, now published only after its
complete four-byte guest output is validated. It is not a kernel MM database.

Statistics validates all 36 guest output bytes before querying the host. Results
are snapshotted and published as nine unaligned little-endian words; native
records instead copy nine native 32-bit words, guarded by a compile-time size
assertion. Invalid output and host failure leave caller storage unchanged.
Availability stays 64-bit until bounded: total=16384 guest pages; available
host pages above that report8192, otherwise the actual quotient by4096. The
remaining six usage fields remain zero. Huge host counters no longer wrap before
the bound. This retains title compatibility, not precise guest free accounting.

Windows initializes MEMORYSTATUSEX before GlobalMemoryStatusEx. Darwin queries
[host_statistics64](https://developer.apple.com/documentation/kernel/1502863-host_statistics64)
and estimates availability as free plus inactive pages times host page size.
Only the returned prefix containing those counters is required, allowing older
record revisions; the host port is released on success and failure. Field and
revision definitions come from the OS SDK's
[statistics](https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/mach/vm_statistics.h)
and [host-info interfaces](https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/mach/host_info.h).
No OS implementation code is copied. This estimate is not a promise of immediate
reclaimability. The previous Darwin compatibility function read uninitialized
fields; these Mm routes no longer call it.

Debug/advisory compatibility is explicit: IsAddressValid still returns1,
DbgQueryAvailablePages returns0x10000, WriteCheck echoes its address, and
persist/locking/release-address hints have no effect. They are not proof of
accessible memory, available storage, pinning or a reboot lifetime.

## Validation and remaining work

Four suites cover original statistics policy, controlled Windows/Darwin and real
host snapshots, synthetic guest ownership/output/access cases, and all actual
proposed/installed bridge/context/native bodies. Sixteen compiled negative
controls must compile before their runtime checks reject them. The earlier
allocation fixture keeps all its placement/ownership assertions and now supplies
the Mm context and dependencies required by the extracted bridge. Both hosts run
the new suites in CI; controlled Darwin cases on Windows are not native Mac
execution. STATUS and MAC-114 record actual stage acceptance.

Main guest map creation/lifetimes, native Nt platform compatibility, remaining
kernel/device services, title/delivery integration and inherited fixtures remain
replacement work. Final independence still requires generation/build/test/package
and provenance checks with the old toolkit checkout and snapshot unavailable.
