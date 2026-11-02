# Original RTL memory and bit operations

Stage0243 replaces seven inherited imports with guest_memory_ops.c/.h and
guest_kernel_memory_ops.inc. Additive0243-original-memory-operations.patch
selects one shared provider after0242 and the source-layout migration. Historical
pre-layout subsets omit0243 too; full upgrades and reverse switches exercise it.
No native RTL module is restored, and no third-party implementation is copied.
Standard libc supplies bulk byte filling and overlap-safe host byte movement.

## Services and guest ABI

Ordinals284 fill,285 DWORD fill and298 move retain12-byte stdcall frames;
320 zero and297 generic-mask mapping retain8-byte frames. The adapter snapshots
g_esp, obtains the VM once, resolves the complete read frame once and decodes its
little-endian words before any output. EDX, ECX and g_esp are preserved. Void
operations clear EAX, including invalid frames/spans; they have no NTSTATUS result.
Invalid/wrapping/unmapped/readonly spans produce no partial writes. This is an
explicit defensive boundary, not guest access-violation or SEH delivery.

Fill/zero validate the full actual write span before writing. Byte fills use the
low pattern byte. DWORD fills repeat four little-endian pattern bytes, permitting
unaligned destinations and ignoring a final0..3 bytes. Zero actual write count
resolves nothing and succeeds even for null/unmapped addresses. Move validates
both full spans, then uses memmove, supporting overlap in either direction and
canonical host aliases of guest addresses. No allocations, guest native-pointer
casts or MMIO occur. Caller owns stable mappings/access/data throughout; span
resolution does not pin storage or coordinate guest data races.

Byte swaps307/318 now take their input from ECX with zero dispatcher cleanup,
correcting inherited stack-word/4-byte cleanup. Both return zero-extended results
in EAX and preserve ECX/EDX/g_esp/memory without touching the VM, including when
stack storage is null or unmapped. Swap16 uses only the low16 input bits. The
actual Windows x86 exports are also fastcall: an initially wrong stdcall native
probe was caught by its stack check. Public Xbox SDK declarations independently
confirm the register ABI; no supported-US import of these APIs is claimed.

Generic-mask mapping snapshots a four-byte access mask and complete16-byte
little-endian GenericRead/Write/Execute/All record before publishing. It expands
flags in read/write/execute/all order against accumulated rights, then clears
all four generic bits. This also defines native-observed ordered behavior when a
mapping itself contains generic flags. Output may alias its mapping record or
frame, including high guest aliases, because arguments/table are snapshotted.
Native-oracle overlapping-record cases are excluded: the provider's explicit
snapshot policy is tested independently rather than inferred from native aliases.
This replaces the inherited no-op implementation.

Contract references only:
[Microsoft fill](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-rtlfillmemory),
[Microsoft overlap-safe move](https://learn.microsoft.com/en-us/windows/win32/devnotes/rtlmovememory),
[generic mappings](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-generic_mapping),
[public Xbox fastcall declarations](https://github.com/XboxDev/nxdk/blob/master/lib/xboxkrnl/xboxkrnl.h).
No SDK implementation is used or added as a dependency.

## Checks

tests/compiler/test_guest_memory_ops.py compiles real provider/import bodies with
fatal warnings and compares14007 independent Python whole512-byte RAM models,
checking both direct and import outputs. Deterministic cases cover both overlap
directions, partial DWORD tails, ordered mask expansion and record aliases.
Random cases include overflow, holes, readonly outputs, high aliases, frame/output
aliases, all byte values and guard bytes. The fixture additionally checks null VM,
invalid width, every stdcall malformed frame and fastcall execution with invalid
stacks/missing VM. Every request checks registers, complete-frame behavior and
lookup counts. Binary streams use explicit Windows binary mode.

Nine deliberately broken providers/adapters must compile before failing vectors
or literal frame checks. Actual installed dispatcher argument sizes execute under
a separate C probe, in addition to provider selection/old-body retirement checks.
Windows also calls actual x86 ntdll exports for4305 valid bounded vectors, checking
pre/post stack pointers, scalar values and whole memory images; native pointer
arguments stay separate from guest addresses. Mac selects portable model/import
cases with undefined-behavior sanitizer; the Windows-only oracle skips there.

The US XBE imports none of these seven routines, so fixtures establish direct
service behavior while bounded Story tests establish integration stability.
Release/archive/current-generation, packaging and native Mac title acceptance
are separate; see STATUS for actually measured hashes, counts and limits.
Charset, string ownership, exception/unwind and other kernel/graphics/audio/input/
title/fixture replacement and toolkit/snapshot-unavailable proof remain open.


Local acceptance on083d13c8 plus0243:69 focused and35 migration/line-ending
cases passed (104 distinct). Release, utilities/built-in tests, archive/current-
generation inspection and isolated Scotland movement/fire/pause/resume passed on
executable206c6a4b. All generated game bytes are unchanged. Exact hashes,
measurements and limits are in STATUS; new-stage CI/native Mac title verification
remain pending. Prior083d runtime41412095577 and launcher41412095536 passed.
