# Virtual-memory call boundaries

Native Mm now uses [project ownership and real host pages](native-memory.md).
The Nt adapter limitations below remain distinct from that newer Mm route.

`src/runtime/guest_virtual_memory.c` owns the four guest Nt virtual-memory
operations. `host_virtual_memory.c` owns native allocate/free/query policy.
Their headers require no toolkit headers. Additive
`patches/compiler/0208-independent-virtual-memory.patch` removes the inherited
implementations from `kernel_bridge.c` and `kernel_memory.c`, leaving adapters
and existing thunk/stack-cleanup registrations. Both routes remain live;
native addresses and `size_t` must never be truncated into guest words.

## Guest calls and ownership

The guest service snapshots unaligned little-endian input words, validates
whole output spans, then publishes results. Allocation requires both base and
size output pointers; null/malformed outputs cannot orphan an allocation.
Invalid buffers leave allocation records and caller memory unchanged. The
resolver must keep mappings/access stable through the call; this is not a
guest-write lock or protection-race solution.

Allocation reuses `guest_allocation.c`, including its minimum sizes and zeroing.
Zero bytes retain status `0xc0000045`. A nonzero hint without RESERVE retains
unchanged success. Otherwise try the full heap request, then for pure RESERVE
try reservation storage and repeatedly halve oversized requests until a heap
request fits or the existing 64 KiB loop threshold is reached. The returned
size is the request actually accepted. ZeroBits and allocation protection
hints retain their no-effect policy. These are compatibility rules, not a
complete reserve/commit or demand-paging implementation.

`tsfp_virtual_release` removes an exact heap or reservation owner under the
existing allocator lock; physical/protected records cannot be released through
it. It adds reservation release without a second ownership table. Guest RELEASE
validates optional size output before reclaiming storage and clears outputs.
Unowned bases retain historical success/clear behavior. DECOMMIT and legacy
mode retain storage. The bridge reads `RECOMP_NTFREE_LEGACY` per call using the
existing nonempty/first-character-not-zero rule; the old unsynchronized cached
environment flag is removed. Environment configuration should remain stable
while the game runs.

Query writes exactly seven 32-bit words (28 bytes). It retains page-based flat
RAM reporting: page >= code_begin and below 64 MiB reports COMMIT/READWRITE/
PRIVATE through the RAM end; otherwise it reports a free 4 KiB page. The free
record retains Protect=4 while AllocationProtect/Type are zero. This policy
does not report individual heap ownership, extended mappings or real per-page
protection. Native query records are a different ABI.

Protection validates base/size words, the entire target mapping and optional
old-protection output before calling the host. The output cannot overlap any
affected host page, including a 16 KiB page on Apple Silicon, because making it
read-only would prevent safe result publication. Mapped-only resolution allows
restoring access to PAGE_NOACCESS storage. The host callback must fail without
partial protection changes. Caller buffers/mappings must remain stable; memory
protection changes are not serialized with arbitrary guest execution.

## Host mapping adapters and remaining work

The layout adapter admits only registered RAM, contiguous, tiled, mirror and
synthetic-kernel views. MMIO and unrelated process mappings are excluded.
Read/write buffer validation walks actual OS regions via project
`pc_guest_memory.h`, including adjacent regions with different permissions.
Darwin uses its real Mach VM query, avoiding the compatibility VirtualQuery
stub. Mapped-only checks use the known live views and do not read target bytes.
Mapping setup/teardown and page protection remain transitional host services.

Native allocate/free use pointer-width callbacks. Failed calls leave outputs
unchanged; RELEASE clears the base but retains the size. Native queries use a
zero-initialized host-record scratch buffer, require a complete host query,
copy only the caller's allowed length and publish returned length on success.
The actual native MEMORY_BASIC_INFORMATION layout remains at the adapter.
The seven existing native access modes and default READWRITE conversion remain.
Native host allocation/query still use the existing platform APIs; Darwin's
native query placeholder and unaligned mprotect behavior remain limitations
to replace with the host mapping services. Guest buffer checks do not rely on
that placeholder. Mm APIs, statistics, device mapping and other kernel services
remain separate replacement work; this stage does not complete independence.

## Tests and evidence

`test_guest_virtual_memory.py` and its original C fixture cover snapshot and
publication order, overlapping/unaligned outputs, allocation clamp/failure,
invalid/read-only buffers, reservation release/reuse, legacy/decommit behavior,
query boundaries, protection callback failure and 4/16 KiB page overlap.
`test_host_virtual_memory.py` covers pointer/size values above 32 bits, partial
record copies, output preservation, scratch padding and 65,536 protection flags.
Nine compiled negative controls must fail at runtime.

`test_virtual_memory_adapters.py` compiles actual installed/proposed functions
with controlled host APIs, checking guest/native route separation, registered
views, cross-region access and status/result publication. Both Win32 and POSIX
page-size branches execute with controlled host APIs. Additional Windows probes
use real VirtualAlloc/VirtualProtect/VirtualQuery for read-only/no-access pages,
unmapped guest spans, native query layouts, decommit and release. Other hosts
skip only those two Windows probes. These checks run in CI on both hosts;
native Mac gameplay remains separate. Production build/game acceptance and
remaining limits are recorded in STATUS and MAC-112.
