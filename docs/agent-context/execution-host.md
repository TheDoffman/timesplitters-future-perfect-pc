# Original guest host scheduling policy

Stage0260 replaces the remaining execution-host policy block in the patched
xbox_memory_layout.c with guest_execution_host.inc, followed by the existing
owned guest_kernel_execution.inc. The inherited guest lock bootstrap, parking,
concurrency counters/report, optional Windows affinity and environment-switch
bodies are removed by0260-original-execution-host-policy.patch after0259.
No generator input or architectural header changes in this stage. The context
execution core remains unselected; actual265/302/312 imports are still inherited.

## Initialization and ownership

One standard platform InitOnce initializes the recursive critical section and
freezes the three diagnostic/scheduling switches before any guest operation.
No racy sentinel caches are used. Dispatch parking allocates its manual-reset
wake event only when enabled; allocation failure disables that optional policy
and reports it. Process-lifetime storage requires guest-thread quiescence before
process exit. The compatibility layer itself remains a separate provenance task.

The own state provider remains authoritative for per-thread recursive lock and
active depth. Only zero/nonzero transitions call the host policy, so the atomic
counters count active host threads rather than nested guest calls. Park admission
increments before reading the atomic owner; an owner waits for existing active
threads to drain. The event is only a wake hint; ownership is always rechecked.
Same-owner entry is allowed. Contended entry waits at most100ms; drain/acquisition
waits are bounded50ms and explicitly best-effort. There is no whole-instruction
preemption or claim of strict single-CPU semantics when the bound expires.

Dispatch owners are acquired with compare/exchange, not overwritten. TLS nesting
keeps an inner release from clearing the outer gate. A request that times out
without ownership cannot release another thread's gate. Normal release verifies
its own owner identity. Existing kernel_hal IRQL/DPC locking remains separate;
this stage does not replace every thread scheduler or its termination semantics.

Environment false words are exactly0/off/no/false, case-insensitive ASCII.
An empty value means enabled and absence uses the caller default. Shipping guest
lock, dispatch parking and counters remain off. Isolated runners set their own
requested modes; personal settings and saves are not configuration inputs here.

## Windows affinity

The optional RECOMP_GUEST_ONE_CPU setting remains Windows-only.1 selects the
highest eligible processor index in the fastest efficiency class, independent
of record enumeration order. Without usable CPU records it selects the highest
allowed bit. Values2through65 select index value-minus2 in the allowed mask;
out-of-range, overflow, malformed and unavailable choices disable pinning.
All shifts are bounded. Group-zero-only CPU enumeration matches this adapter's
existing group-local mask scope; it is not a new multi-group scheduling feature.

The original guest_cpu_choice.h policy uses only standard C. The Windows-only
guest_execution_cpu.inc uses SDK SYSTEM_CPU_SET_INFORMATION named fields,
walks Size within the returned allocation and stops at malformed/truncated tails.
It selects from preceding usable records, or uses the allowed-mask fallback if
none are usable or the query failed/returned an oversized length. It never reads
unfilled query bytes or maintains private field offsets. Standard API reference:
[Microsoft CPU-set records](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-system_cpu_set_information).
Host affinity applies once to each entering guest thread; host render/audio
threads are not independently pinned by this policy. Native Mac keeps the common
lock/parking/counters and omits the optional Windows affinity adapter.

## Verification and stage status

Run tests/compiler/test_execution_host_policy.py. Portable deterministic fixtures
exercise recursive ownership, suspend/restore, frozen switches, owner nesting,
foreign contention/release, event failure, drain/admission completion and timeouts,
and simultaneous activity accounting. A separate SDK-shaped fixture deliberately
changes field offsets and tests unsorted CPU classes, group filtering, all64 mask
bits, malformed settings/records and OS query failures. Nine compiled wrong
providers must fail at runtime; compile failure is not accepted as rejection.
The Windows-only native fixture uses actual threads, critical sections, InitOnce,
events and affinity APIs, with four workers doing100 suspend/restore cycles.
All12 checks pass locally4.73s; Windows evidence only. Shared checks are registered
on both CI hosts; native Mac results for this stage are pending.

The own-stage tail58 and all five historical omission sets include0260 after0259.
Integration with main623acff4 adds SMAA2470 after0260, making the combined tail59.
Production apply60626 and Release49959 exit0, including built-ins/crash symbols.
The actual source/archive audit confirms own policies and unchanged generated
C/current receipt.113 focused cases pass249.21s (the12 new checks are a subset).
Fresh98.728s default-off and99.193s enabled-lock/parking/automatic-affinity Story
runs pass movement/fire/pause; all1680 source-input and executable/runner hashes
match, six captures reviewed and26 protected files unchanged. Enabled startup
reports mask0x8000; counted concurrency peak1/overlaps0 versus2/126 off. These
observations do not prove all legacy callbacks or actual title IRQL routes.
The177 full migrations/repository/context/gates pass1172.30s on the original
handle25162 (terminal0):290 distinct checks including the113 focused cases.
Prior exactd36 CI runtime41527543804 (all11 jobs) and standalone41527543813
(all5) are fully green.
New-stage CI and native Mac title gameplay remain required. Detailed source,
failed-invocation records and final acceptance scope are in STATUS.
See [execution ownership](execution-ownership.md) and [context records](context-records.md).
The full runtime/tooling/title/header/fixture replacement and generation/build/
package with both upstream sources unavailable remain part of the active goal.

Isolated Windows runner flags --dispatch-lock and --guest-one-cpu are explicit
per-run settings, applied after parent-environment stripping and recorded in
provenance alongside lock/counter modes. Use off/default and on/automatic-CPU
Story routes; fresh native title acceptance is still pending.

Merged main623acff4 source is committed/pushed as0477fdb1. Merged Release,
117 quick metadata checks and15 host/SMAA cases pass; fresh99.172/99.267s off/on
Story runs pass with current source/executable provenance, six captures reviewed
and26 protected files unchanged. Full177 merged migrations83784 pass907.81s (terminal0); the117 quick
checks are a subset, giving192 distinct merged checks with the15 host/SMAA cases.
Exact-head CI runtime41532493664/launcher41532493218 remains to be concluded. See STATUS for distinct
pre-merge versus merged executable evidence; do not reuse fe70 gameplay as0477
proof. Actual265/302/312 integration and full independence remain unfinished.

0477 runtime41532493664 fails one Mac negative-fixture compile: removing the
same-owner exemption left me set-but-unused. The mutation now consumes me and
must still fail its runtime owner-entry timing assertion. Production policy and
accepted build/game source are unchanged.2759 other Mac cases pass; Windows
native/upgrades and standalone launcher CI pass. New-head CI remains required.
