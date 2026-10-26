# Native player contact investigation

The pre-fix Venice route blocks native180/240 where Original60 continues.
Paired traces and the complete original C54A0 fixture reproduce a valid clear
slide subsequently discarded by the final motion guard. See STATUS for the
measured evidence and live acceptance state.

`pc_native_walk_guest.h` scopes only native positive game deltas below one tick.
Completed C4700 returns at C5781/C58B0 classify the last slide attempt; a later
blocked result replaces an earlier clear result. Only a clear final slide makes
the original C596C branch choose its existing small-motion guard instead of the
large corner penalty. The normal comparison otherwise uses scratch globals after
the no-hit slide/floor query. Existing guards already multiply delta twice; do
not scale them again. Geometry sweeps, response positions, returned flags and
threshold constants stay original. Original/whole/catch-up/paused paths retain
the old branch. A call-local scope restores its parent and retains no actor data
between frames. Broader geometry/NPC and physical Mac acceptance remain separate.

`pc_native_numeric.h` selects an optional shared diagnostic at US call C5AD6
(C5A90 -> C54A0). The root context's first word is the actor slot, populated by
C4560; only slot0 roots and their nested C4700 sweeps are captured. Audited sweep
call sites are C55E6/C5668/C577C/C58AB, C5259/C5283 and recursive C4B68.
`pc_native_timing_guest.inc` includes the policy/diagnostic bridge; no OS-specific
feature logic or new adapter is needed. Nonfractional calls stay direct when
tracing is disabled. Production fractional calls perform no diagnostic mapping
queries or file writes without an explicit trace request.

Set `TSFP_WALK_TRACE` to an isolated CSV path to enable it. Defaults are scene46
and inclusive level ticks2000..2300. Optional `TSFP_WALK_TRACE_SCENE` is an
unsigned decimal; `TSFP_WALK_TRACE_TICKS` is `first,last`. Invalid selections
disable capture with a diagnostic. Bounds:8192 pairs,16MiB, nested depth below8.
Writes flush every64 records; normal process shutdown flushes the tail. This is
CPU diagnostic evidence, excluded from clean performance measurements.

Each CSV row contains a completed pair with id/parent/depth, routine, before/
after ESP and returned EAX. Children finish before their parent, so id order is
not row order. `layout1` has90 semicolon-separated hexadecimal words:45 before,
then45 after. Per-phase offsets are:

| Words | Payload |
| --- | --- |
| 0..5 | frame, scene, level ticks, raw/game float bits, caller |
| 6..15 | ten entry argument words (sweep's tenth is zero) |
| 16..23 | eight context words |
| 24..26 /27..29 | old/new position bits |
| 30 /31..36 | optional result readability flag /six result words |
| 37..44 | global words at732F70..732F8C |

The optional result is sweep argument7 only; root arguments have a different
signature. These are raw fields, not inferred geometry identities. Original
routines reuse argument stack slots, so after-state reads retain entry pointer
anchors instead of following clobbered arguments. Mapping snapshots are reset
before and after every guest call. Mismatched frame/scene/tick or stack balance,
unreadable required spans and write failures reject records without changing
guest execution. The tracer never changes positions, collision results or clocks.

`tests/native_walk_trace.c` exercises the production bridge and numeric dispatch
with controlled services: disabled/neighbor sites, nested pairs, returned EAX/ESP,
argument reuse, slot/scene/tick filters, invalid reads/options and output bounds.
It does not execute real collision geometry. `native_walk_policy.c` covers
classification, interval guards, blocked replacement and nested scope restoration.
`native_walk_contact.c` executes full original C54A0 with recorded primary/slide
service boundaries. Sixteen Original/native/clear/blocked/scoped cases reproduce
the old rewind and retain only the eligible clear slide. Removing the root scope
or final branch hook fails. This is controlled geometry-service evidence, not a
complete map replay. Existing `test_native_timing.py` registration runs these on
both CI hosts; private-XBE cases skip without assets and Mac scenes remain open.
