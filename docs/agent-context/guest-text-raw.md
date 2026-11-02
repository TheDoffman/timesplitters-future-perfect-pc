# Original raw charset services

Stage0246 selects guest_text_raw.c/.h and guest_kernel_text_raw.inc for299/300
multibyte-to-Unicode conversion/size,310/311 Unicode-to-multibyte conversion/size
and315 uppercase Unicode-to-multibyte conversion. It composes the original
[counted codec leaf](guest-text-codec.md) and [case mapper](guest-string-transform.md)
without inheriting their descriptor/terminator/16-bit-size policy. Additive
0246-original-raw-codec.patch follows0245 in the effective tail, removes all five
old bridge bodies and links the shared provider once. Historical pre-layout
subsets omit it; complete forward/reverse upgrades assert current selection.

## Explicit lengths and publication

Raw counts/capacities are32-bit byte values, not counted-string USHORT fields.
The provider snapshots the complete effective source into aligned host storage.
ANSI input reads every explicit byte, including NUL. Unicode input reads complete
16-bit LE units and ignores an incomplete final byte; an odd declared length
therefore does not require access to that unused byte. Source size0 accepts a
null buffer. No native wchar_t record or pointer is cast onto guest memory.

Size operations require a writable four-byte count and publish full encoded
length. Destination/capacity are ignored. N operations accept a null count;
otherwise it is checked before conversion. Output is a capacity-bounded prefix,
with no automatic NUL and no padding: a NUL only appears when part of the input.
Unicode output truncates at complete16-bit units; it can end between the units
of a surrogate pair. ANSI prefixes retain codec character boundaries (including
UTF-8/GB18030 surrogate pairs and DBCS sequences). The existing host ANSI-prefix
adapter supplies those boundaries. Zero-byte output resolves no body, even for
nonzero declared capacity. All counts use explicit little-endian writes.

Plain N truncation returns success; uppercase N returns80000005 when full output
was not written. Public Microsoft uppercase documentation describes success, but
actual native probes on this Windows ACP1252 host return80000005 for truncation;
this stage follows that measured distinction. Windows raw/code-page tests prove
their stated scope, not every Xbox/NT version's NLS behavior. Unicode size/ordinary
conversion agree on ignored odd bytes. Empty queries/conversions succeed.

Source/output and source/count aliases are safe through staging, including fixed
argument-frame storage. Actual output and count spans must not overlap, including
canonical host aliases; otherwise C000000D leaves both unchanged. Null/wrapping/
unmapped/readonly effective spans reject before publication. In particular, the
provider validates the full effective source even if output capacity is0. Mapping,
access, data and callback state must stay stable throughout; resolves do not pin
or trigger MMIO. No guest heap owner is created or released. Host staging can
fail C0000017; codec query/write/prefix failures or inconsistent written sizes
return C0000001 with guest output/count unchanged. Missing operation/callback/
required count/upper mapper returns C000000D. A zero-input/no-output operation
can succeed without a VM resolver; imports still require their checked frame.

## Shared host policy and ABI

Standard host conversion uses the existing captured ACP Win32 NLS adapter on
Windows and the existing single-byte/Latin-1 fallback on native Mac. Uppercase
maps complete Unicode units with the existing libc leaf before encoding. Its
locale/NT table differences remain explicit; complete native NLS case equivalence
is not claimed. Reusing these project-owned leaves adds no toolkit compatibility
shim, copied code or tables. Standard libraries/OS services remain permitted.

Conversion imports read checked20-byte fixed frames; size imports read12 bytes.
Each snapshots g_esp, obtains VM once, reads all words before service work and
preserves ECX/EDX/g_esp. EAX carries NTSTATUS; malformed frames return C000000D
before output. Dispatcher cleanup remains20/12 bytes and is executed in fixtures.

Primary interface contracts only:
[Microsoft raw conversion](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-rtlmultibytetounicoden),
[Microsoft uppercase variant](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-rtlupcaseunicodetomultibyten),
[Microsoft size query](https://learn.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-rtlunicodetomultibytesize).
Actual ntdll calls establish measured truncation/status/odd-length behavior; no
native implementation source is imported.

## Verification

tests/compiler/test_guest_text_raw.py compiles real provider/host leaves/imports
with fatal warnings.12450 independent seeded full1024-byte RAM requests run through
both direct and actual import paths. Tests include empty/embedded NUL, odd units,
zero/partial/full/huge capacity, optional/required counts, readonly/hole/wrapping/
canonical aliases and frame/source/output/count aliases. Query/write/prefix and
short/long encoded-count failures must preserve the entire guest image. Ten broken
providers/adapters must compile before failing runtime contracts; overlap helpers
remain referenced in deliberately broken variants for Clang fatal warnings.

Windows compares7500 actual native raw API calls on this host's ACP. Plain paths
use all bytes/random UTF-16 units; uppercase references use ASCII to respect the
existing host-case policy. A first native fixture accidentally crossed its own
readonly guard; corrected reference buffers now remain entirely writable, without
weakening provider bounds or skipping cases. CP1252/932/65001 cases independently
check full sizes, Unicode code-unit prefixes and ANSI logical-character prefixes.
Literal compiled limits verify65538-byte output queries,65534-byte bounded Unicode
writes without NUL,65537-byte odd input handling, optional/no-op/null services and
no guest allocator. Installed single selection, old-body absence and actual
20/12-byte dispatcher cleanup are separate. Both CI hosts select this suite;
Mac portable fixtures use undefined-behavior sanitizer; four Windows-only native/
code-page tests skip. Exact build/migration/game results are in STATUS.

Native Mac title gameplay, exception/context and other kernel/graphics/audio/
input/title/header/fixture work, unused compatibility/header retirement and final
old-checkout/snapshot-unavailable generation/build/package proof remain unfinished.
