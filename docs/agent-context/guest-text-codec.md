# Original counted string conversion and ownership

Stage0244 selects guest_text_codec.c/.h, host_text_codec.c/.h and
guest_kernel_text_codec.inc for260 ANSI-to-Unicode,308 Unicode-to-ANSI,
286 ANSI free and287 Unicode free. Additive0244-original-counted-codec.patch
removes all four inherited bridge bodies and links both providers once. It
follows the [arithmetic/EOL](guest-integer-division.md) and
[memory/bit](guest-memory-ops.md) stages in the effective tail. Historical
pre-layout subsets omit it; complete forward/reverse upgrades install it.
No native RTL module or toolkit compatibility implementation is selected.

## Guest records, conversion and allocation

Both records are eight little-endian bytes: Length16/MaximumLength16/Buffer32.
Source Length defines the complete read span, including embedded NULs; source
MaximumLength is not a read bound. Zero Length accepts a null source buffer.
Unicode Length must be even. The source is copied into aligned host storage
before encoding, and output is staged separately. Source/destination body aliases
therefore work without exposing native string records or wchar_t width to guest
memory. Output body overlapping its descriptor, including canonical host aliases,
is rejected before output publication. Null/wrapping/unmapped/readonly records
or actual output spans return C000000D without partial output. Mappings/access/
data/ownership must stay stable throughout; resolving does not pin lifetimes or
trigger MMIO. Caller-owned records and source must be disjoint from free heap
storage during allocation, as with the existing guest allocator contract.

ANSI-to-Unicode requires the full converted body plus a two-byte terminator.
Insufficient caller capacity returns80000005 without descriptor/body changes.
Unicode-to-ANSI requires one terminator byte: at capacity0 it returns80000005
unchanged; otherwise it may publish the largest encoding-valid prefix plus NUL,
set Length to that prefix and return80000005. Successful Length excludes NUL.
Caller MaximumLength and Buffer remain unchanged in nonallocating mode. Only
actual written bytes are resolved; declaring a larger capacity does not pad it.
An encoded body plus its terminator exceeding65535 returns C00000F0; malformed
codec output/query/prefix returns C0000001; host/guest allocation failure returns
C0000017. Structural failure is distinct from a legitimate bounded ANSI prefix.

The allocation flag is a BOOLEAN byte, not arbitrary upper stack bits. Allocation
uses the original tsfp_heap_allocate with16-byte alignment and existing minimum/
zeroing/observer policy. Published MaximumLength is exactly encoded bytes plus
terminator; the guest pointer is an actual heap owner. Existing destination Buffer
is replaced without implicitly freeing its prior ownership, matching the API's
caller responsibility. Fresh-buffer validation failure releases the new owner
and preserves the destination descriptor. Normal allocator zeroing of fresh/
unpublished storage is not rolled back; no claim of restoring freed padding is
made. Encoding and host staging complete before guest ownership is requested.

## Host charset boundary and free behavior

The thin Windows leaf captures GetACP and delegates character mapping to standard
MultiByteToWideChar/WideCharToMultiByte. Prefix sizing uses bounded source-prefix
queries, retaining whole DBCS characters and complete UTF-16 surrogate pairs for
UTF-8/GB18030 output. The shared provider owns records, bounds, status, allocation
and publication. No old toolkit conversion shim is called. The native Mac leaf
preserves the existing single-byte/Latin-1 fallback: bytes widen directly and
non-byte UTF-16 units become '?'. This existing platform encoding boundary is
explicit; complete NT Unicode/OEM/NLS equivalence or new Mac code-page support is
not claimed. Standard OS character tables remain permitted host dependencies.

Free validates a writable descriptor and exact live guest heap ownership. Null
record/buffer is an unchanged success, including existing Length/Maximum fields
for a null buffer. Unowned/interior buffers are rejected unchanged; descriptor
storage must outlive the body and cannot overlap it. Successful release clears
all eight descriptor bytes. The body is not unmapped or zeroed on release.
No native host-heap pointer is returned, queried or freed by guest services.

Conversion imports read complete12-byte fixed frames; free imports read4 bytes.
Each snapshots g_esp and obtains the VM once, before service work. Conversion
returns NTSTATUS in EAX; void frees clear EAX even for invalid frames. EDX/ECX/
g_esp are retained; dispatcher cleanup remains12/4 bytes. Missing/wrapping/
truncated conversion frames return C000000D before ownership or buffer changes.

API contracts only:
[Microsoft ANSI-to-Unicode](https://learn.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-rtlansistringtounicodestring),
[Microsoft Unicode-to-ANSI](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-rtlunicodestringtoansistring).
No third-party implementation was copied. Real native API probes establish the
ordinary capacity/empty-string/status behavior, not arbitrary malformed NT quirks.

## Verification routes

tests/compiler/test_guest_text_codec.py compiles real providers, host adapter,
allocator and import include with fatal warnings.10224 seeded independent Python
whole1024-byte RAM/ownership models run through direct services and actual imports.
Cases cover empty/NUL-containing/full-byte strings, UTF-16 units, odd lengths,
read-only/hole/wrapping/high aliases, descriptor/body aliases, BOOLEAN high bits,
heap exhaustion, unpublished allocation rollback, existing ownership preservation,
free/interior/self-alias guards and codec query/write/prefix failures. Metadata
counts/addresses/sizes and the complete memory image must agree. Ten broken
providers/adapters must compile before failing these contracts; warning failures
are not accepted negative controls.

Windows additionally compares6000 actual ntdll counted conversions, constructing
native pointer records separately and mapping only their results to guest width.
Allocated native buffers are released with the native matching API; guest padding
follows the project's zeroing policy. Three independent Windows code-page cases
1252/932/65001 check all capacity boundaries through direct/import paths against
standard OS conversions, with logical-character enumeration as a prefix oracle.
Portable max-length literals check32766/32767 ANSI source boundaries, maximum
Unicode length, output-query overflow, null service contracts and a live allocated
round trip/free with the real shared allocator. Installed provider selection,
old-body removal and actual dispatcher argument sizes are checked separately.

Native Mac runs portable Latin-1/model/ownership cases with undefined-behavior
sanitizer; the four Windows-only reference cases skip. Build/archive/generation,
packaging and bounded game checks remain separate. Unlike the previous unused
helper stages, the supported US import table includes260/308. Actual build/game/
CI evidence belongs in STATUS; native Mac title and whole-runtime/snapshot-
unavailable production acceptance remain unfinished. Create/case/raw charset,
exception/unwind, other kernel/graphics/audio/input/title/fixture work remains.
