# Original counted strings and memory prefixes

Stage0237 selects `guest_strings.c/.h` and `guest_kernel_strings.inc` for fourteen
imports:268/269 memory/pattern prefix matching;289/290 ANSI/Unicode init;
270/271 comparisons;279/280 equality;272/273 copy;317 ANSI upper;
261/262 counted append;263 null-terminated Unicode append. Their existing8/12-byte
guest argument cleanup remains unchanged. Every adapter snapshots all argument
words before VM lookup. Only EAX is written; void routines clear it. Comparisons
return signed32 differences, equality0/1, appends NTSTATUS, prefix counts bytes.

The six old native init/conversion/equality/pattern helpers and kernel.h prototypes
are retired after caller audit. Only the excluded kernel_thunks resolver retains
the native symbols. The guest ANSI/Unicode conversion bridges still remain legacy
work; removing their unused native host-heap counterparts does not replace those
bridges. kernel_rtl.c still supplies critical sections, status mapping, exception
and formatting services. Create/free/case conversion, numeric parsing/formatting,
remaining charset services and the old raw wcslen helper remain replacement work.
Vendor/historical patch bytes remain for reproducible unwind and notices.

The shared provider treats descriptors as eight little-endian bytes: Length16,
MaximumLength16, Buffer32. Unit width is1 or2, never native wchar_t width. It uses
the established tsfp_guest_vm full-span/access resolver, rejecting null/wrapping/
unmapped spans and odd UTF-16 source lengths before writes. Source MaximumLength
is not a bound for counted reads: Length defines the claimed read span. Empty
counted inputs permit null buffers. Copy and append snapshot both descriptors,
validate the entire claimed source and actual destination write span, and publish
Length only after the body operation. Body overlap uses memmove, followed by
in-place uppercase when requested. Output body overlapping its descriptor is
rejected (including host pointer aliases). No allocator, native pointer record,
guest struct cast, MMIO resolution or new lifetime authority is introduced.
Mappings must remain stable for the full call; resolving is not a lifetime pin.

Initialization scans checked code units only up to the representable length.
ANSI clamps at65534 bytes/Maximum65535; Unicode at65532/Maximum65534, reserving its
two-byte terminator without16-bit wrap. Source0 initializes eight zero bytes.
Copy truncates to destination capacity and complete code units, preserves Maximum
and Buffer, and null source clears Length without changing the body. Append is
all-or-nothing, returning C0000023 for insufficient capacity and C000000D for
invalid spans/records. Unicode copy and nonempty append add a terminator when two
spare bytes exist. Empty counted append leaves the body alone; non-null empty
raw Unicode append may add a terminator. Null raw source is an unchanged success.
Comparisons include embedded NULs, compare unsigned units, then unit counts.
Invalid compare returns neutral0; equality independently returns false, rather
than treating invalid input as equal. Prefix matching stops at the first mismatch;
pattern comparison ignores the final0..3 bytes and returns a whole-DWORD byte count.

The thin adapter supplies standard libc toupper/towupper for case-insensitive
ordering and upper-copy. Case-sensitive UTF-16 handles all16-bit code units,
including isolated surrogates, without claiming to transcode them. This retains
the runtime's host C-locale case-policy boundary; no complete NT Unicode/OEM case
table or locale independence is claimed. Native oracle tests for insensitive/
upper cases use ASCII; sensitive vectors cover the full code-unit/byte range.
Using uppercase also fixes the old lowercase ordering around punctuation (z/[).
Malformed odd source lengths are rejected and odd destination capacities round
down for Unicode copy; arbitrary malformed NT-record quirks are not emulated.

API references (contracts only, not implementation sources):
[copy](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-rtlcopyunicodestring),
[counted append](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-rtlappendunicodestringtostring),
[raw append](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-rtlappendunicodetostring),
[compare](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-rtlcompareunicodestring),
[init](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-rtlinitunicodestring).
Actual Windows ntdll probes additionally verify large init limits and empty append
differences. No third-party implementation was copied.

`test_guest_strings.py` compiles real providers/adapters with fatal warnings.
14000 seeded independent Python byte-model requests exercise complete512-byte
before/after images; Windows adds6000 actual ntdll oracle requests. Native records
are constructed separately with host pointers, then results serialized back to
guest-width fields. No native struct is cast onto the guest descriptor. Literal
tests cover embedded NULs, empty/null records, unsigned ordering, max init lengths,
overlap, protected/short/wrapping mappings, output alias rejection, whole-word
pattern tails, unchanged failures, void/bool/status EAX and fixed argument reads
for every selected import. Six broken providers and five broken adapters must
compile before failing at runtime. Caller/selection audit checks all C/C++/header/
include sources, retired native names, single include/provider and ordinal sizes.
Fresh/historical upgrades assert the new provider and native helper retirement.

Windows Release/archive/generation and isolated Story acceptance are separate
from these asset-free contracts. Native Mac title gameplay remains unverified.
See STATUS for measured results; the overall replacement goal is incomplete.


Acceptance:8a846e75 runtime40997984438 and launcher40997984495 passed both host
native/GPU/upgrade/transition and package checks. A separate Windows icon-fixture
configure timeout passed on unchanged retry; no limits/assertions were weakened.
Windows Story passed movement/fire/pause/resume with reviewed fresh captures and
matching07fd92c5 build/game bytes.90 distinct selected local checks passed,26
protected files unchanged. See STATUS stage0237 completed acceptance for exact
scope/counts/provenance. PR#413 follows the externally merged#406. Native Mac
title gameplay and the broader replacement goal remain unfinished.
