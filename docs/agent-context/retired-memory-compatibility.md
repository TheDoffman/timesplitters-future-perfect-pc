# Retired compatibility memory APIs

Patch `0214-retire-compatibility-memory.patch` removes the unused POSIX
CreateFileMapping/MapView/UnmapView implementation and its view registry, mapping
object branch, size field and declarations. It also removes the unused POSIX
VirtualQuery and GlobalMemoryStatusEx implementations and declarations. This
retirement follows replacement of their production callers by project mapping,
native virtual-memory and host-statistics providers.

## Caller and ABI boundary

The caller audit covered installed runtime source/headers, title source and
generated C/includes, plus project source, tools and fixtures. No production
POSIX caller remained. The surviving occurrences were compatibility definitions,
obsolete documentation or tests. Real Windows API calls remain in the original
project host adapters and Windows crash/network code. Those use Windows headers,
not the retired compatibility layer. Windows mapping flags in negative-control
tests likewise target the real OS adapter and remain intentional.

`MEMORY_BASIC_INFORMATION` remains in the compatibility header: native Nt ABI
serialization in `kernel_memory.c` still requires that host record layout. Its
presence does not retain the removed fake query implementation. The unused
`MEMORYSTATUSEX` compatibility record is removed; host statistics use project
policy and actual host APIs. `sys/stat.h` remains available for live POSIX file
size operations; unused sysinfo/sysctl includes are removed.

The old registry could silently omit mappings when full and discarded ownership
before unmap success. The old query fabricated a writable committed page, while
the Darwin statistics stub could use uninitialized fields. These are source
findings, not newly observed gameplay failures. Live production paths already
use the project providers; recreating those unused APIs would add unnecessary
duplicate ownership. The pristine snapshot and historical patch inputs remain
unchanged for reproducibility and migration.

## Fixtures follow the live providers

`tests/gameplay/native_platform.c` now checks shared backing through
`shared_views`, and native arena aliases through `guest_arena_service`. It keeps
stdin-descriptor preservation, first/last-byte alias visibility, occupied-address
collision, exact arena placement, VirtualAlloc collision, unmap/remap contents,
backing release and arena release assertions. Its event/thread and other native
checks are unchanged. Existing platform CMake and standalone compiler commands
already supply the provider library, sources and include directories.

`test_guest_arena_adapters.py` extracts the still-live VirtualAlloc/Free and
arena/MMIO forwarders. Shared descriptor-zero placements exercise the project
arena service directly. Failed cleanup, poisoning, page rounding, access masks,
admission, immutable MMIO lookup and reset assertions remain. The obsolete
registry-consumption mutation becomes an ownership-bypass mutation in the live
VirtualFree branch. All five negative controls must compile before failing;
the successful baseline must retain every ownership assertion.

The native fixture's normal mapping prefix compiled and ran on Windows against
real host providers during candidate validation. That check translates its stdin
probe to the Windows CRT and excludes the Mac arena branch. It does not prove
Mac event/arena execution. Native Mac CI and title gameplay remain separate
acceptance. See STATUS and MAC-117 for exact measured scope.

## Remaining work

Allocation/protection compatibility, kernel and device services remain under
replacement. This patch does not remove their APIs or imply worker quiescence.
Published main maps retain the [guest mapping lifetime](guest-host-mappings.md).
The native memory/host-statistics providers retain their own contracts. Original
mapping and image-loading descriptions replace obsolete direct API examples in
the installed runtime documentation; they do not claim arbitrary-title support.

Whole-runtime independence still requires generation, build, test and packaging
with both the old toolkit checkout and pristine snapshot unavailable. Retiring
unused routines does not authorize removing historical notices or deleting the
snapshot before that final audit.
