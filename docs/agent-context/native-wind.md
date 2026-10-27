# Procedural wind simulation

The US `1DDF10` wind callback adds forcing using the live fractional delta,
then calls the coupled `AB050` angle/velocity map once per carried whole tick.
Converting only its loop gate to a boolean changes damping, restoring and
contact behavior. `pc_native_wind.h` subdivides the coupled map instead, with
an authored restoring segment held to its next exact tick boundary. Contact
projection and wrap rebasing belong to that same interval. Original and
whole/catch-up calls still execute the original oscillator and loop count.

`0460-native-wind-history.patch` wraps the original `A5CE0` registration and
`1DDF10` callback, retaining both generated bodies. Each `_gen` declaration must be on its own
`extern void ...;` line so `manual_scan` recognizes both wrappers. Only registration slot3
with callback `1DDF10` creates/resets a wind owner; another slot3 callback or
null removes it. `pc_native_wind_owners.h` stores solver history per owner,
data pointer and guest generation/handle. Memory/scene changes discard owner
entries; skipped advancing intervals and identity/frequency changes invalidate
history. Pause without advancing game time preserves it. Allocation occurs at
registration only; failure leaves authored stepping available. No pose, vertex,
guest pointer contents or render state is cached, and saves are unchanged.

The normal frame parent `1CE2E0` calls level-clock updater `68830` before
active-list selection `A0130` and callback dispatch `A7670`. The wind helper
uses the end guest level tick at `7DE318` minus the current whole count as its
start tick, together with the published `PcNativeStep` game start phase and
exact game interval. Both wind loop reads (`1DDF7B`, `1DDF96`) are selected
at the numeric macro boundary, keeping other consumers independent of the
published clock. Fractional admission requires an exact match with live game
delta and whole count. A local zero delta freezes; unmatched scopes keep the
original loop count. Callback entry observation clears incompatible history
even when the oscillator is skipped. The two axis proposals/history commit
together; failed proposals discard both histories before authored fallback.

The implementation is shared C for Windows and native Mac. The public `test_native_wind.py` checks independent authored low-angle steps,
exact clock guards, ownership and manual wrapper scanning without game assets.
It also runs `native_wind_original.c` when the verified local XBE is available:
22 original routines/constants are injected at test time, never tracked. That
check covers two-owner data/matrix equality, pause/resume, unmatched timing
scopes and whole fallback, with frozen-matrix/missing-registration controls.
The original second-pass wind controller/phase updater and callback setter now
execute too; the fixture initializes the console refresh word to 60 and requires
finite phase/scales and phase agreement with game elapsed time. A frozen-provider
control fails. A three-game-second Original60 comparison covers 840 schedules
(7 angles, 6 velocities, 4 carried phases, 5 rates) under the original changing wind
inputs; maximum angle outside the existing sample window is 0.217468262 degrees,
below the unchanged 0.25-degree limit. Contact/settling may legitimately leave
matrices identical; builder/clock/independent-point checks still execute.
Both CI hosts and `tsfp.ps1 -Action Test` run the module; hosts without the
XBE skip the original-code check. These do not replace live host/visual acceptance.

Local original-code fixtures execute the constructor, registration/removal,
wind sampler/provider, matrix/point routines, level-clock updater and original
frame selection/dispatch. Other weather-particle and model/world services remain
controlled; entity+19C secondary rendering is not accepted by these CPU checks. With controlled world/model services and synthetic host
timestamps, 9,864 two-owner solo/interleaved data/matrix comparisons pass;
371 paused updates freeze data/matrices/history and resume the same trace.
Wrong clock-domain, wrong tick-boundary and shared-owner controls fail.
There are 47 byte-equal unmatched-scope fallbacks and 360 Original/native
whole-fallback owner comparisons. Prior nonlinear comparisons cover 5,040
cases within the recorded quarter-degree angle window; these are bounded
behavioral checks, not exact whole-game equivalence.

Shared source passes these CPU checks and normal Windows Build/bundled gates
on source e9ea817a (executable 70a33689). The selected Temple floor-rocket
regression passes at native240; it does not exercise wind-scene acceptance.
Real wind scene/ownership transitions, host pacing, live GPU output, performance
cost and other guest timing overrides remain unverified.
Unmatched timing scopes retain authored stepping and therefore do not yet
prove fresh fractional motion in those scopes. Physical Mac validation is
also outstanding. Keep these limits explicit in STATUS and the PR.
