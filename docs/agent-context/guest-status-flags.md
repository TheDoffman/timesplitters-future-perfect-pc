# Original persistent arithmetic status

Stage0248 adds original persistent arithmetic value/known-mask storage and
compiler producers/consumers. Windows124 focused checks and55 migration/context
checks pass; fresh production generation, Release/built-ins/build audit and a
clean bounded Story run pass after the remainder fix below. This is a foundation
for saved CPU context, not completed guest CONTEXT or exception delivery.
Preceding9003b35d CI passed. Stage0248 runtime41444560160 later failed two
identifier/template checks on both hosts; [stage0249](guest-shift-flags.md)
repairs them. Launcher41444560191 passed; native Mac title remains unverified.

## Storage and contracts

src/runtime/guest_status_flags.h owns pure arithmetic helpers. guest_state.c
owns the TLS cell, initially reserved bit1 known/set. recomp_types.h exposes it
under TSFP_RECORD_STATUS_FLAGS. Additive0248 selects that definition in the shared
title CMake before subdirectories for Windows and native Mac. Standalone fixtures
without the feature retain the earlier ABI. A cached value is authoritative only
where its known mask says so; invalidation must not invent missing state.

ADD/SUB/ADC/SBB/INC/DEC/NEG/CMP publish arithmetic bits at8/16/32-bit width.
INC/DEC preserve carry. Logic/TEST define SZP and clear CF/OF while making AF
unknown. Normalized TEST register,register uses the tested value, not the zero
operand of its legacy CMP-shaped descriptor. ADC/SBB use persistent known CF or
the legacy fallback; an unknown input carry leaves output flags unknown.
STC/CLC/CMC, SAHF and LAHF have explicit policies. CMC keeps unknown carry unknown;
SAHF preserves OF; LAHF forces bit1 and clears reserved bits3/5. Conditional
helpers use known required bits for all16 x86 predicates, otherwise a supplied
legacy predicate. Jcc, SETcc, CMOVcc and LOOPE/LOOPNE consume that interface.

translation_integer.py and translation_compare.py emit producers;
translation_lifting.py routes consumers and invalidations; translation_function.py
enables recording for production functions and seeds local carry. Direct known
translated calls retain callee flags. Opaque calls/tails and fast entry invalidate
unmodeled status. [Shift/rotate producers](guest-shift-flags.md) now track defined status and
dynamic zero counts. Product/bit/floating compare/string producer families remain
incomplete and conservatively invalidate affected knowledge. Legacy block snapshots
remain fallback compatibility, not a proof of complete cross-block EFLAGS.

guest_callback_call.c, pc_display_menu.c and pc_acoustic_guest.inc include status
in their existing modeled-register scopes. guest_nonlocal_jump.c invalidates
arithmetic knowledge on a successful transfer because the guest CRT jump buffer
does not contain EFLAGS; it must not restore fabricated flags. Ordinary callback
returns restore the captured status; nonlocal transfers bypass that restoration.

## Checks and remaining work

test_guest_status_flags.py executes independent mathematical vectors and
compile-first mutations. Its real Windows x86 oracle exhausts8-bit ADD/SUB and
both carry inputs of ADC/SBB and checks wider cases. It separately tests all16
conditions against every6-bit value and known-mask combination with both
fallbacks (131072 requests). Native x86 skips on other hosts.
test_status_flags_translation.py compiles actual production translations against
canonical recomp_types.h, guest_status_flags.h and guest_state.c with the feature
enabled: arithmetic, normalized TEST, branch/set, SAHF/LAHF, loops, constant/dynamic
shift handling, callee-to-caller carry and fast-entry invalidation.
test_guest_callback_call.py runs both ABIs, malformed cleanup and real native
nonlocal transfer, including a compile-first broken status-restore control.

The final focused suite passed113 tests in20.93s, including adjacent signed
branching, unknown carry and byte/word ADC preserving upper register bits.
Generation translated15838/15863 functions with0 failures (25 exclusions);
Release/built-ins passed. The build audit confirms the ABI definition, archive
state symbol, canonical generated-header text (exact expected Windows CRLF),
current generation and all16 translation units updated. Executable SHA256:
35270399b17ce401d9fefd461fbd921dfe2b4bca64877bc2420c37259d5dba0a.
The first Story run reached the boat intro then stopped swapping frames at~105s,
scene30/mission1/level_ticks787. Controller exited1; runner exited0 after stop.
This is NOT a gameplay pass. First report/state/images are retained under
logs/status-flags-attempt1-*; a second bounded CPU-sampled run is diagnostic only.
Full migration/context suites subsequently passed55 checks.
Both new suites are selected by the shared Windows/Mac native CI job. UI/acoustic
preservation changes compile in production; playable/game acceptance is pending.
Full saved context, PUSHFD/POPFD fidelity, all flag producers and real exception
dispatch/unwind remain unfinished. Do not replace those services with partial
records or claim the larger toolkit replacement complete.

## Boat-intro regression diagnosis and repair

The CPU-sampled second attempt reproduced the stall. The main thread accumulated
samples in sub_003C9DE8 and fmod: FPREM/FNSTSW/SAHF/JP back to FPREM. The original
project x87 emitter completed libm remainder but left the C2 condition bit stale.
The new SAHF/JP consumer exposed that missing producer, rather than causing a
slow render loop. Both failed runs are retained and neither is accepted.

translation_x87.py now obtains low quotient bits from remquo, uses the absolute
quotient (confirmed by a real x86 probe), adjusts nearest quotient to truncation
for FPREM, publishes C0/C1/C3 and clears completed C2. Other status bits remain
unchanged. FPREM1 retains nearest-even remainder. This preserves the existing
binary64 guest model; it does not implement extended intermediates or guest FPU
exception delivery. Invalid-operand quotient/status values remain outside the
native valid-input comparison.

[Intel's architecture manual](https://cdrdv2-public.intel.com/868140/253666-089-sdm-vol-2a.pdf)
documents reduction completion and quotient condition bits. Added ML64 hardware
checks compare1248 valid signed/zero/subnormal/huge-quotient double pairs against
both operations, including all quotient condition bits and preserved state. A
compiled bad-C2 control must fail. Production translations execute both complete
FPREM/FNSTSW/SAHF/JP loops from deliberately stale C2 state.31 focused x87 and
production translation checks pass6.33s. Full migration/context55 pass1093.01s.
The first35270399 executable predates the repair and remains rejected. Both
failed runs and their CPU samples are retained; the repaired run is separate.

After the remainder repair, the combined suite passed124 checks in22.16s;
context/index and whitespace checks pass. Regeneration completed again with
15838/15863 functions,0 failures,6,582,633 lines (91.16s lift). The repaired
Release/built-ins and archive/generation audit pass; failed executable rejected.

## Accepted Windows production run

Repaired executable SHA256:
8c2c57575b3d7d17ce1b9f8a62fff60aa5411bfb12407717cafd63a8557e6c68.
Runner21355 and controller35157 both exited0. Fresh scene4/mission1 initialized
with input enabled and8 rounds; movement11.1745, back6.1925, fire8-to6 rounds.
Pause39 samples frozen, resume58 advancing; last checkpoint117.595s below240s.
Fresh start/fire/pause captures were reviewed. Profile signatures/fixture valid;
all26 protected files retain hashes/sizes/mtimes; no run lock/dumps/partition
images. Build input hashes and run executable provenance match exactly.
Evidence logs/status-flags-build.json, status-flags-game-check.json,
status-flags-captures, status-flags-accepted-{game,controller}.log. This is a
minimized synthetic Original60/movie-skipped Story check, not physical-input,
audio-listening, full-campaign or native Mac gameplay acceptance. Remaining
unmodeled flags, context/SEH and wider replacement/dependency audit remain open.
