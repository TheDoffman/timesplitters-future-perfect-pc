# Original string creation and case services

Stage0245 selects guest_string_transform.c/.h, host_string_case.c/.h and
guest_kernel_string_transform.inc for seven imports:274 create,275/313 Unicode
lower/upper unit,296/316 byte lower/upper unit and276/314 counted Unicode lower/
upper. Additive0245-original-string-transform.patch follows the
[counted conversion/free](guest-text-codec.md) tail. It removes all seven inherited
bridge bodies and their now-unused unchecked bridge_guest_wcslen_bytes scanner.
Both providers are selected once. Historical pre-layout subsets omit this tail;
complete upgrades and reverse switches verify it through the current install audit.

## Records, bounds and ownership

Eight-byte LE guest records carry Length16/MaximumLength16/Buffer32. Creation
requires a real UTF-16 NUL within32767 units, with at most32766 non-NUL units.
It checks each scan unit, rejects guest arithmetic wrap and mapping holes, then
checks the complete source span and snapshots it into host storage before heap
allocation. Successful Length excludes NUL, MaximumLength includes the two-byte
NUL, and Buffer belongs to the real shared guest allocator. Oversize/unterminated/
invalid strings return BOOLEAN false without publishing a partial descriptor.

Case transformation reads exactly the source Length, including embedded NULs;
source MaximumLength is not a read bound. Odd lengths reject. Source snapshots
support body/descriptor aliases without native pointer records. The host mapper
is applied independently to each16-bit unit. Nonallocating output must fit the
whole counted body; insufficient capacity returns80000005 unchanged. Success
copies exactly Length bytes, NEVER appending NUL even if capacity is spare. This
corrects the inherited out-of-bounds NUL write at exact capacity. Caller Maximum/
Buffer remain unchanged. Allocating mode publishes MaximumLength=Length, with no
terminator allowance; empty conversion still owns the allocator's minimum block
while publishing Length/Maximum0. The largest even counted Length65534 fits.

The allocation flag is its low BOOLEAN byte. Existing destination ownership is
not implicitly freed. Encoding/case staging completes before guest allocation;
fresh-output validation failure restores the descriptor and releases ownership.
Normal allocator zeroing of unpublished free storage is not undone. Writable
output overlapping its descriptor, including canonical aliases, rejects. Only
actual body writes are checked for access; zero-byte case output resolves no body.
Callers own stable valid descriptors/source/mappings disjoint from free heap
storage. This is not a lifetime pin or a transaction over arbitrary dangling
objects. Invalid structure/spans return C000000D, allocation failure C0000017.
The [original free service](guest-text-codec.md) handles created/transformed
buffers; both it and the remaining legacy allocator adapters share the same arena.

## Host and ABI boundary

The leaf masks inputs to byte/UTF-16 width, then delegates to standard libc
tolower/toupper/towlower/towupper with the existing process locale. The provider
masks mapped output when storing a guest16-bit unit. No native wchar_t record is
cast onto guest memory. This retains the existing host character policy, including
its locale/NT NLS differences; complete Windows NT case-table equivalence is not
claimed or inferred from ASCII native references. Standard libraries remain
permitted dependencies; no implementation or character table was copied.

Create reads one checked8-byte frame; string case reads12; character case reads4.
Every import snapshots g_esp, obtains VM once, reads the entire frame before work,
and preserves ECX/EDX/g_esp. Only EAX changes: create BOOLEAN, string NTSTATUS,
character zero-extended masked result. Invalid frames produce false/zero or
C000000D for the string statuses. Dispatcher cleanup stays8/12/4 bytes, supported
by public Xbox interface declarations; these character imports are stdcall.

Primary contracts only:
[Microsoft creation](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-rtlcreateunicodestring),
[Microsoft case strings](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-rtldowncaseunicodestring),
[Xbox interface declarations](https://github.com/XboxDev/nxdk/blob/master/lib/xboxkrnl/xboxkrnl.h).
Actual standard ntdll probes establish capacity/status/no-NUL and maximum creation
behavior. They are behavioral evidence, not imported source or a dependency.

## Verification

tests/compiler/test_guest_string_transform.py compiles the real providers,
allocator and import include with fatal warnings.10432 independent seeded whole
1024-byte RAM/ownership cases exercise creation/case/scalars through both direct
services and real imports. They cover every fixed frame, null/wrapping/short/
readonly/hole/alias spans, empty and embedded-NUL data, low-byte BOOLEAN, existing
ownership, heap exhaustion, failed fresh publication and complete memory images.
Eight compile-first broken variants test source length, output size/length/status,
rollback, descriptor overlap, BOOLEAN routing and character mapping. Another
compiled bad creation bound must fail maximum-length literals at runtime.
Windows compares6000 actual native creation/ASCII case calls, constructing native
pointer records separately and normalizing only their returned fields/bytes.
Non-ASCII provider policy uses an independent explicit ASCII fixture mapper;
standard-host leaf coverage compares all262144 width/direction/unit combinations
to the real C library, including ignored upper input bits. Literal limits cover
32766/32767 creation, counted65534 case, zero-capacity allocated output, missing
callbacks and real create/transform/free ownership round trips. Installed selection,
old scanner/body removal and executed dispatcher argument sizes are separate.

Both CI hosts select this suite; portable Mac fixtures use undefined-behavior
sanitizer, and actual ntdll references skip there. Exact local build/migration/game
acceptance is in STATUS. Native Mac title gameplay, raw charset/exception/kernel/
graphics/audio/input/title/header/fixture work and final old-checkout/snapshot-
unavailable generation/build/package proof remain unfinished.
