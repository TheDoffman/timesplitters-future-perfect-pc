# Original critical sections and native RTL module retirement

Stage0238 excludes the whole legacy kernel_rtl.c from KERNEL_SOURCES. It selects
guest_critical.c, host_critical.c, guest_rtl_services.c and guest_kernel_rtl.c once;
guest_kernel_rtl.inc supplies eight imports:277/278/291/294/295/306 critical
initialize/enter/leave/region/try,301 status-error mapping and352 Rip diagnostics.
Cleanup remains4 bytes except Rip12. All arguments snapshot before service/VM
lookup; only EAX changes. Void operations clear it; try returns0/1 and status
mapping returns a32-bit error. Other original services: [strings](kernel-strings.md),
[calendar](kernel-time.md), [pool](kernel-pool.md).

All twelve old native declarations/exports retire: three critical functions,
SetCrtLockTable, status conversion, Rip, unused Unwind/RaiseException and four
unused printf wrappers. Only excluded kernel_rtl/kernel_thunks retain names.
Stage0239 now selects [original narrow formatting](guest-printf.md). Guest
exception/unwind paths and charset/create/free/numeric services elsewhere in
kernel_bridge.c remain replacement work; unused native wrapper retirement alone
did not replace those guest implementations. Snapshot and
historical patch bytes remain for reproducible migration and notices.

## Registry and host ownership

The original shared registry uses caller-owned buckets and dynamically constructed,
permanent host records. There is no4096/512-lock capacity limit or unprotected
overflow fallback. Allocation failure preserves the registry and returns an error.
Metadata is guarded; a native acquisition never waits while holding that guard.
Native recursive mutexes remain in stable, noncopyable storage. Every acquisition/
release carries an API pin until its native operation and metadata bookkeeping
finish. Owner tokens, outer acquisition sites and recursion depth update under the
guard. Wrong-thread/excess leave is rejected without invoking native unlock.
Depth/API exhaustion is checked, and stats saturate instead of wrapping.

Enter, try and leave share the same record. The retired bridge's separate TryEnter
table could acquire a different native mutex from ordinary enter/leave, violating
mutual exclusion. Native tests now exercise mixed entry methods across threads.
Initialize is idempotent and never resets an owned native lock. Distinct keys are
independent; explicit single diagnostic mode collapses nonzero keys to one record.
The bridge validates a mapped byte and uses its full host identity as key, so
different guest addresses aliasing the same storage share ownership.

This retains the runtime's opaque guest-field compatibility policy: no presumed
Win32 CRITICAL_SECTION is cast onto a guest record and no guest fields are updated.
The one-byte resolution validates identity only; it does not claim validation of
an Xbox20/24/28-byte layout, guest-visible recursion fields or a mapping lifetime
pin. Mappings must remain stable through the call. Region variants retain the
existing lock policy; kernel APC-disable accounting is not implemented here.

Stop refuses while any lock is owned or an API operation is in flight. It closes
lookup admission under the guard and detaches records; native disposal occurs
outside it. Failed disposal quarantines retained records and is never blindly
retried. Users must be joined before guard destruction. Concurrent reinit/stop,
abandoned-thread recovery and whole-runtime worker shutdown are not supplied.
The production singleton lasts for the process, as before.

host_critical.c uses Windows SRW metadata guards/recursive CRITICAL_SECTION and
native pthread guards/PTHREAD_MUTEX_RECURSIVE on Mac. There is no dependency on
the legacy POSIX critical-section emulation. Standard API references:
[Windows ownership](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-entercriticalsection),
[Windows release](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-leavecriticalsection),
[Apple pthread mutex attributes](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/pthread_mutexattr_settype.3.html).
No implementation source was copied.

## Integration, errors and diagnostics

The singleton and immutable diagnostic options initialize once using native
InitOnce/pthread_once. RECOMP_CS_MODE=single and RECOMP_CS_TRACE_CRT=all remain
available. RECOMP_CS_WATCH selects a guest address independently; traces use
bounded checked stack-word scans labeled candidates, not authoritative frames.
The unused CRT table-registration export retires; there was no active caller.
Options are read only at initialization, so ordinary lock operations do not read
environment strings. Contention reports use guarded ownership snapshots and a
bounded report budget; diagnostic values can become stale after snapshot release.

Invalid/null mapped identities return an invalid result. Wrong-owner release
preserves protection. Try failure returns false. Infrastructure failure in a void
critical operation exits with a diagnostic before unprotected guest execution can
continue; it never fabricates successful ownership. Native callback failures are
separate from contention. A deterministic subprocess checks the fatal exit path.

The original status provider preserves nineteen compatibility overrides using a
sorted numeric table. Unlisted statuses reach the native Windows ntdll mapping
through a standard OS function lookup; Mac retains317 with an unmapped diagnostic.
The historical C0000021->1455 override is deliberate compatibility, not a claim
that it equals every host NT mapping. Unknown fallback receives all32 input bits.
There is no racy mutable function-pointer cache.

Rip reads at most255/255/511 guest bytes into private buffers. Null/unmapped/
wrapping text becomes '?'; truncation ends in '~'. All reads resolve mapped bytes,
and logging uses a fixed format so guest percent sequences stay data. Guest bytes
are untouched. Debug builds retain native breakpoint behavior on each host.

## Verification routes

test_guest_critical.py compiles the actual providers/adapters with fatal warnings.
The controlled host model verifies recursion, first-owner site, foreign release,
identity width, hash collisions,10000 records, active acquisition/release pins,
factory/host failures, exhaustion, closed state and disposal quarantine. Seven
broken providers, five bad import adapters and four bad status/text providers must
compile and fail at runtime. Real Windows/pthread tests cover a blocked waiter,
mixed try/enter/leave, independent/single keys and30000 synchronized increments.
Actual kernel integration additionally checks mapped aliases, thread-safe setup,
native error fallback, diagnostics/watch/trace, unchanged guest memory and fatal
exit. Literal tests verify all nineteen overrides and9999 raw fallback inputs.

Eight-import fixtures guard exact argument reads, action routing, return values,
preserved registers and the fail-closed hook. Caller/selection audit checks native
names absent from active C/C++/headers/includes, removed separate TryEnter table,
single provider/include selection and all argument cleanup sizes. Fresh/historical
upgrades must exclude the whole legacy module and install original providers.
Windows archive audit must exclude the exact kernel_rtl object (not the distinct
guest_kernel_rtl object) and retire its twelve exports. Build/generation/package,
isolated Story and native Mac title acceptance remain separate; see STATUS.


Acceptance:46bc2046 passed runtime41002852446 and launcher41002852486 across both
native/GPU/upgrade/transition hosts and launcher packaging. Windows149 distinct
selected local checks and Release/built-ins/symbols/archive/generation passed;
Story movement/fire/pause/resume and fresh captures agree with a586004e runtime
bytes.26 protected files unchanged; exact counts/limits are in STATUS stage0238.
Latest maincc531ea9 is already included. Native Mac title gameplay and the overall
replacement remain unfinished; PR#413 remains draft/unmerged.
