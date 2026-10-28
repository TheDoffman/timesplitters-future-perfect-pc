# Guest host mappings and lifetime

Patch `0213-independent-guest-host-maps.patch` replaces main address-space
construction in `src/kernel/xbox_memory_layout.c` and the Mac arena/MMIO helpers
in `src/platform/win32_compat.c`. Those are installed runtime paths. Authoritative
policy is in project `src/runtime/`; OS adapters are in `src/native/`.

## Providers and ownership

`shared_views.c/.h` owns one backing and caller-supplied view records. Capacity
and ranges are checked before mapping. Requested addresses must match exactly.
Failed cleanup retains ownership, including unpublished wrong-address mappings.
Stop closes a backing only after every view is gone; failed cleanup is retryable.
`shared_host.c` supplies page-file mappings on Windows and file-backed mmap on
POSIX. Ordinary POSIX mappings use hints, never MAP_FIXED. Stream tokens own
their FILE until close consumes it; an error does not authorize retrying a reused
file descriptor. Addresses and handles are not generation tokens.

`owned_arena.c/.h` owns placements inside a reserved address range. Host-page
rounding, containment and overlap checks precede placement. Only this backend
uses MAP_FIXED, within its owned reservation. Release restores inaccessible
pages without losing the reservation. Uncertain placement/restore poisons the
arena and retains the affected record; new placements stop. Whole-arena release
cannot discard published placements. Access is a read/write/execute bit mask.
Arena backing tokens encode descriptor plus one; zero denotes private storage.
They are distinct from the FILE tokens of ordinary shared backings.

`mmio_views.c/.h` owns protected guest views and separate writable aliases.
Lookup reads immutable published metadata with bounded arithmetic; it performs
no locking, allocation or host operation in a signal handler. Setup/reset and
direct memory users must be quiescent. Failed setup/reset retains all remaining
resources for retry. `guest_arena_service.c/.h` provides the shared Mac service:
one arena, 512 placement records and two MMIO entries. Compatibility unmap checks
this owner in the initial integration. The unused generic file-map registry/API
and query/statistics stubs are now [retired](retired-memory-compatibility.md).
Other compatibility services remain replacement work.

## Main guest layout

`guest_map_layout.c/.h` constructs RAM, 28 mirrors, physical storage, tiled/high
physical aliases, NV2A/MCPX/flash and kernel storage. RAM and physical backings
are separate. A private physical fallback never advertises an unrelated tiled
alias. The reported tiled extent is capped to the actual physical backing and
mapping. Kernel storage borrowed from physical memory is not separately freed.
Optional aliases and guest spans are checked for overflow and excluded overlaps.

`guest_map_host.c` supplies Windows placement or the Mac reserved 4 GiB arena.
Each layout requires its own zero-initialized host session; a failed second
layout cannot release another layout's arena. Initialization and rollback are
quiescent operations. Failed begin can still require finish; failed cleanup
retains its owner. Public pointers are withdrawn on unpublished rollback.

The runtime binding publishes the layout before starting device workers. Stop
returns 1 for full release, 0 for retained failed cleanup, or 2 for a published
layout retained until process exit. Publication cannot be cleared to force
cleanup, and a retained layout cannot be reinitialized. Ack notifier drain and
thread join have bounded waits; timeout retains handles. Even joining that one
worker does not prove all guest/device users stopped. Explicit whole-runtime
worker quiescence remains future work. Public pointers stay valid for published
layouts. The unused `xbox_GetMappingHandle` export is removed.

## Build and checks

Platform CMake creates `tsfp_guest_maps` once and links both platform variants
to it. This avoids duplicate providers and a platform/kernel dependency cycle.
The library includes seven C providers and exports the runtime include path.
The main binding verifies physical/tiled aliasing and restores probe bytes.

Standalone renderer projects must supply `TSFP_SHARED_SOURCE` before adding the
platform directory: the native Mac RHI CMake project resolves it from its project
root, and the Windows replay fixture passes its absolute `src` path at configure
time. They bypass the title's setup and cannot rely on that title's variables.
The replay regression compiles the real renderer/replay and provider dependencies.

`test_shared_views.py` covers real host mappings plus ownership, arena and MMIO
failure models. `test_guest_arena_adapters.py` executes actual compatibility
allocation/MMIO bindings; retired file-map calls now use project owners directly.
`test_guest_map_layout.py` covers construction, partial rollback,
publication and session isolation; `test_guest_layout_binding.py` extracts the
actual runtime binding and exercises aliases and both worker timeout paths.
Negative controls must compile before their runtime assertions reject them.
Existing allocation/thread/image tests retain their assertions and now refer
to project map/stop boundaries. All four new suites run in Windows/Mac CI.
The standalone native event and audio timing fixtures also link the five arena
service dependencies when compiling the compatibility source directly. Their
behavioral assertions are unchanged; native execution requires the Mac CI host.

Candidate Windows evidence: 47 tests, 37 compiled negative controls, three
affected existing tests and three CTest link/binding scenarios passed. These do
not establish installed full-suite, production build or gameplay acceptance.
See STATUS and MAC-116 for measured acceptance. Controlled POSIX tests here are
not native Mac execution. Native Mac title gameplay remains unverified.

Main construction is project-owned; worker lifetimes, other kernel/devices,
graphics/audio, title/delivery and inherited fixtures remain unfinished. Final
independence requires generation/build/test/package with both the old toolkit
checkout and pristine snapshot unavailable.
