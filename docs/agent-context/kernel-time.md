# Original absolute-time conversion

Stage0236 selects `src/runtime/guest_time.c` once in xbox_kernel and replaces the
304/305 adapters with `guest_kernel_time.inc`. Both ordinals retain two guest
words/eight argument bytes. The native two xbox_RtlTime functions and their
kernel.h declarations are retired. The now-unused POSIX SystemTimeToFileTime and
FileTimeToSystemTime definitions/declarations are also removed. kernel_rtl.c is
still compiled for its other RTL services; this is not whole-module retirement.
Only the excluded legacy kernel_thunks resolver retains the two native names.
The pristine snapshot and historical patch inputs remain for reproducible unwind.

The original codec counts Gregorian days since1601 using ordinary year/leap/month
arithmetic. It encodes 16 little-endian field bytes to eight absolute100ns bytes;
decode uses bounded year search and month subtraction. No host calendar, libc
time_t, timezone, locale, allocation or external algorithm implementation is used.
TIME_FIELDS order is year/month/day/hour/minute/second/millisecond/weekday, each
16bits. Input weekday is ignored. Encode validates1601..30827, real month/day,
hour0..23, minute/second0..59 and millisecond0..999. Decode accepts0..INT64_MAX,
discards sub-millisecond ticks and computes Sunday0 weekday (epochMonday1).
INT64_MAX reaches30828-09-14 02:48:05.477 Thursday; encoding that year is rejected.
Pure-code failure preserves the destination. Null, short, unaligned and overlapping
buffers are explicit contracts; inputs are consumed before candidate publication.

These ranges/failure rules preserve the Windows compatibility surface previously
used by this runtime, not every native NT quirk. API references:
[SystemTimeToFileTime](https://learn.microsoft.com/en-us/windows/win32/api/timezoneapi/nf-timezoneapi-systemtimetofiletime),
[SYSTEMTIME](https://learn.microsoft.com/en-us/windows/win32/api/minwinbase/ns-minwinbase-systemtime),
[FileTimeToSystemTime](https://learn.microsoft.com/en-us/windows/win32/api/timezoneapi/nf-timezoneapi-filetimetosystemtime).
The old POSIX shim normalized invalid dates and rejected pre1970 decodes; both
hosts now share the validated1601-based policy. This deliberately removes that
host discrepancy. No claim of full Xbox clock, timer, locale or scheduling support.

Adapters snapshot both argument words before VM lookup and resolve complete16/8
byte spans with read/write intent. Null/wrapping/unmapped/insufficient-access spans
preserve guest output; encode returnsEAX0. Successful encode returnsEAX1. A mapped
invalid absolute time zeroes the16-byte fields, retaining the old Windows wrapper
behavior. The void decode clearsEAX. Mapping/access must remain stable throughout
the call, as required by guest_virtual_memory.h; resolving is not a lifetime pin.
No native host struct casts or pointer-width guest records are used.

`tests/compiler/test_guest_time.py` compiles real code with fatal warnings. Python
datetime supplies every day in the400-year Gregorian cycle1601..2000 and3000
seeded dates through9999, including sub-millisecond truncation. Windows adds12000
encode and12000 decode cases against actual kernel32 APIs. Guard/overlap/failure
checks and fixed epoch/INT64_MAX literals run on both hosts. Five compiled broken
codecs and five broken bridges must fail at runtime, after successful compilation.
The mapped fixture checks full spans, access flags, two-word argument reads,
failure preservation, invalid-time zeroing, overlap and EAX. Selected-source caller
audit checks the removed functions, single include/provider, unchanged ordinals
and excluded legacy resolver. Fresh/historical upgrades must install the new
provider and remove both native bodies and obsolete POSIX helpers.

Run the maintained test through the repository Python environment, then Release
and isolated Story validation. Windows archive/export checks and generation
fingerprint/capture review remain separate acceptance. Native Mac title gameplay
is not established by portable contracts or public CI. See STATUS for evidence.


Acceptance: code/checker734df44c passed runtime40994285965 and launcher40994285970,
including both native/upgrade/transition hosts, GPU and package checks. Windows
Story with final runtime bytes passed movement/fire/pause/resume and fresh capture
review;100 distinct local checks passed across runs. The Windows source-checker
UTF-8 repair changes tests only. Source/archive/generated/game hashes and26
protected files agree; exact metrics/limits are in STATUS stage0236 completed
acceptance. Native Mac title and broader replacement remain unfinished.
