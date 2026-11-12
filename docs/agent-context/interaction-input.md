# Native interaction input

Issue #357 reports Scotland the Brave's first interior portcullis levers
failing above 60 FPS with keyboard and controller. The shared original US
interaction selector is `A0D90`; its `A0DE8` direct comparison against the
whole game-tick count is only an admission gate. It does not use that count
as an elapsed-time operand later in the routine.

`pc_native_numeric.h:pc_native_global_load` admits this exact comparison when
native game time is positive. Original mode and other instructions retain
their original values. Zero game time rejects even a stale nonzero whole
count. No global clock, input mask, polling cadence or binding is rewritten.

The selector reads held input through `42760` and pressed input through
`427B0`. Native input decoding `18FF70` publishes digital edges on raw ticks;
pause can shift those ticks relative to game ticks. An edge published on a
zero-whole game frame is otherwise discarded before target selection. Holding
the button is not equivalent: the first activation uses the pressed edge,
while the held path has a separate original repeat cooldown.

Original target availability, proximity, facing, obstruction, player-alive,
occupied-object and cooldown branches remain intact. Target selection ends
in original activation services such as `B8C60`; the fix does not activate a
specific lever by address or bypass its level script.

Reload #337 uses the same principle at `1C2F09` in `1C2E80`: raw input edges
must reach the reload/mode dispatcher on advancing frames. Its eligibility,
binding lookup, animation and ammo logic remain original. The generic
generated CMP hook is documented in runtime.md.

## Checks and limits

`tests/gameplay/native_interaction_input.c`, selected by
`tests/gameplay/test_native_timing.py:test_original_fractional_dispatch`, lifts
the supported private US XBE's complete interaction selector, original input
decoder and held/pressed getters. Geometry/audio/activation services are
controlled boundaries. It exercises 72 Original/native schedules at
60/120/144/180/240 FPS with twelve pause offsets, checks one activation per
published press, pause with a stale whole count, preserved registers/stack,
unchanged clocks, and obstruction/player-alive guards. Reverting only A0DE8's
hook must compile and fail with `interaction edge lost`.

The asset-free `native_reload_gate.c` contract also checks this exact site,
neighboring instructions, raw-clock exclusion, Original fallback and positive
versus paused game time through the production policy. Both test paths are
already selected by the shared runtime workflow's `test_native_timing.py`.
The original-code test skips without the verified private XBE.

Windows command: `.venv/Scripts/python.exe -m pytest
tests/gameplay/test_native_timing.py -k "interaction_input or reload_compare_gate
or reload_input or analog_input" -q --tb=short`.

These CPU checks do not prove the authored portcullis script, collision geometry
or physical controller delivery in a live mission. Test Scotland the Brave at
the first interior upper-floor levers, including pause/resume and Original60.
Native Mac gameplay remains pending under MAC-121.

## Stationary turret entry

Time to Split's first mounted gun uses `B4DF0`, rather than the generic
`A0D90` target pool. Its `B4E5E` load tests whole game ticks only as a pause
gate before distance/facing checks and the `B4B60` mount call. The same
positive-float admission policy now covers this exact load. It preserves
Original mode, idle-gun state, on-foot mode, proximity, facing, input mask and
mount transition. It does not promote the gun update's elapsed-time operands.

`native_turret_entry.c` lifts the complete original entry routine, decoder and
pressed getter. It checks72 schedules at60/120/144/180/240 with12 pause offsets,
one mount request per published press, unchanged clocks/ABI, pause with stale
whole ticks and occupied/on-foot/distance/facing rejection. Geometry, floor,
messages, player enumeration and the final mount service are controlled
boundaries. Removing only the B4E5E hook reproduces `turret entry edge lost`.
Add `or turret_entry` to the Windows pytest selector above. The asset-free
`native_reload_gate.c` also covers this site, neighboring sites and raw-clock
exclusion. Live mounting acceptance remains separate from these CPU tests.

## Gameplay hint opacity

`1405B0` marks eligible gameplay hints with a positive fade velocity. The
original `140630` update consumes that marker once, then restores negative
decay. Script/input hint refreshes can arrive at raw tick boundaries; consuming the
marker on every rendered frame makes hints dim at high native rates.
`pc_native_numeric.h` skips only `140630` on zero-raw-tick native intervals,
and scales its `140652` time multiplication to the consumed raw count while
preserving the game/raw speed ratio. No global clock is overwritten. The
original18-slot/player indexing, colors, show/hide eligibility, clamping and
fade-out stay intact; hint placement/rendering still runs normally. Original
mode retains its unmodified routine.

`native_interaction_hints.c` lifts both complete routines and tests full opacity,
fade-out, zero game time, slow motion and split-screen isolation across
60/120/144/180/240 and8 raw phases, with refresh before/after the update. Removing either hook reproduces the faded
hint assertion. Select it with `-k interaction_hints` in `test_native_timing.py`.
Live opacity acceptance is separate; user confirmed the turret entry correction
works at240 FPS before this follow-up.

## PC Settings bar visibility

Developer Tools replaces the Windows bar with the shared F1 overlay in
`src/interface/pc_developer.c`. `pc_settings_attach` keeps the old command
registry hidden; `pc_settings_toggle_menu` now toggles the overlay. The old
`pc_menu_visible` JSON is retained and survives binding saves, but no longer
attaches a toolbar. Windows and SDL F1 now open the same overlay and release
the pointer for its exclusive keyboard/mouse input. Flight reads configured
movement and Interact/Crouch keys without writing controls. No-clip retains
firing/weapon keys; free-cam exclusively owns character input. Closing latches held keys
until release. Original menus receive empty pointer/text/key input while open.

The Windows settings, prompts and debug linked checks retain legacy JSON and
hidden-command coverage while checking F1 repeat handling, overlay toggling and
unchanged dialog behavior. Shared Developer Tools fixtures cover its settings,
binding capture/conflict and numeric editing routes. Live acceptance remains
separate; see STATUS.

## Fresh weapon clicks (#434)

US `1C2C00` checks fresh and held eligibility separately through `1B7ED0`.
Its call to `1C2AF0` returns to `1C2E6D`; argument four distinguishes a fresh
press from held fire. The request's whole-game-tick gate can discard an already
eligible fresh edge after a pause offsets raw/game phases. The remaining host
pulse then uses the slower held-fire cooldown; increasing its duration is not
a substitute for delivering the fresh request.

`pc_native_player_trigger_guest` admits exactly this fresh request on positive
fractional native time, temporarily providing one whole tick for the request
and restoring it on return. The float interval is unchanged. Held requests,
`1C3100` continuations, Original mode and zero-time calls are not promoted.
No cooldown, ammo, binding, animation-readiness or host pulse is bypassed.
Patch2180 wraps the original request after the late native timing patches.

`native_player_trigger.c` executes the original eligibility/request routines,
original Xbox decoder, shared timing model and host Fire pulse buffer. It
checks680 schedules at60/120/180/240,1/20ms clicks,84/100ms spacing,17 input
phases and pause offsets. Device magnitudes and the final weapon-state
transition are controlled boundaries: this counts admitted firing requests,
not projectiles or damage. The old request gate loses eligible clicks; direct
phase tests detect it. `native_player_trigger_scope.c` checks64 asset-free
scope/restoration cases on both CI hosts. Both use `test_native_timing.py`.
Live ammunition evidence and throughput limitations belong in STATUS.
