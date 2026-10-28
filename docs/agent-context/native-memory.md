# Native Mm allocation and host pages

`src/runtime/host_blocks.c` owns native allocation records. Its header contains
no toolkit types and never stores guest addresses. `src/native/host_pages.c`
adapts Windows virtual memory and POSIX anonymous mappings; its shared interface
is `src/runtime/host_pages.h`. Additive patch
`patches/compiler/0209-independent-native-memory.patch` replaces thirteen native
Mm allocation, release, stack, protection and query bodies with these services.
It runs after 0208 and before the final repository-layout patch.

## Ownership and lifetime

Each record stores the raw host mapping, mapped length, aligned public pointer,
requested length and service kind. Metadata is published before validating a
backend result so failed cleanup can retain ownership for retry. A free must
match the exact public pointer and service kind. Interior, foreign, wrong-kind
and already-freed pointers never reach the host release callback. Host release
failure preserves the record. This replaces the inherited VirtualQuery guess
between aligned malloc and VirtualAlloc ownership, which was unsafe with Mac's
placeholder query. Addresses are not generation tokens: callers must not retain
stale pointers after free, because the host can reuse an address.

The native adapter uses a process-lifetime table of 4096 records, initialized
once with Win32 InitOnceExecuteOnce or pthread_once. Admission fails cleanly at
capacity. Allocation and ownership changes serialize on the provider's lock;
host callbacks must not reenter the same owner. Kernel shutdown does not join
guest/device workers, so it does not free their potentially live native blocks.
Matching releases reclaim allocations; process exit reclaims any remaining
storage. The standalone provider has a quiescent stop/retry API, but the runtime
does not claim that kernel shutdown supplies that quiescence.

System, IO and kernel-stack blocks have separate kinds. Native GPU instance
buffers share contiguous ownership and can be freed with the contiguous call.
Kernel-stack creation returns base plus requested size; deletion uses the low
limit and retains the ignored top argument. Free sizes remain ignored. Native
IO mapping retains its read/write scratch-buffer behavior, not physical MMIO.
Physical bounds in the native contiguous API remain ignored; the guest route
uses the existing guest physical allocator and is not replaced by this table.

Zero-byte native allocations now consistently fail. Invalid nonzero alignment
retains page-aligned fallback; valid power-of-two alignment is always honored.
Unlike the inherited aligned-malloc route, extended allocations honor protection
at every alignment. A failed aligned allocation no longer returns a less-aligned
fallback. These are deliberate ownership/access fixes, not just relocation.

## Page boundary

The backend maps zeroed anonymous pages, checks page granularity, and releases
the raw mapping. Interior protection spans round outward to actual host pages,
including 16 KiB on Apple Silicon. Callers must own those pages and keep mappings
stable through each operation. Host errors propagate; the API does not promise
transactional rollback of arbitrary POSIX protection changes.

Queries include inaccessible mappings. Windows uses VirtualQuery; Darwin uses
mach_vm_region and releases its returned object port on every path. A hole must
not be mistaken for the next region returned by Mach. Failed queries leave
output unchanged. Query results are snapshots, not lifetime pins. The native Mm
size call retains OS-region size, which can split after protection changes; it
does not become the guest allocator's logical-size query. Windows modifiers are
retained in query output; Darwin R/W/X bits map to NT base protections. Native
[Nt mappings](native-virtual-memory.md) now have their own boundary. Mm protection
uses it too, preventing protection-only access to uncommitted Nt reservations.

## Checks and remaining work

`test_host_blocks.py` covers alignment, zeroing, ownership kinds, retained failed
release, capacity, overflow, lifecycle and concurrent allocation. Four compiled
negative controls must compile and fail at runtime. `test_host_pages.py` runs
real host mappings/protection/release plus controlled 4/16 KiB POSIX/Mach cases;
four compiled negative controls check protection rounding, query containment,
object-port release and mapping-failure translation.

`test_native_mm_adapters.py` extracts actual installed bodies and initialization
helpers, then executes concurrent first use, mixed allocation kinds, foreign and
interior release rejection, stack convention, capacity and real protection.
All three suites are selected in Windows/Mac CI. Controlled Mac tests on Windows
do not prove native Mac execution. Build/game acceptance belongs in STATUS and
MAC-113, and cannot be inferred from these asset-free checks.

Existing Nt adapter fixtures now emit the protection helper before its callers,
matching the production source order. Their original ABI/result checks remain.

[Guest Mm/statistics and hints](guest-memory-services.md) now have project
providers, as do [main guest maps](guest-host-mappings.md). Remaining replacement
includes worker quiescence and host-map lifetimes, kernel/devices,
title/delivery integration and inherited fixtures. The old toolkit snapshot is
still part of production preparation. Full independence requires the final
generation/build/test/package audit with both snapshot and old checkout absent.
