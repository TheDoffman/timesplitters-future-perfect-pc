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

## PC Settings bar visibility

PC Settings bar: `pc_settings_attach/toggle_menu` in `src/interface/pc_settings.c` restore/save top-level `pc_menu_visible` in the active input JSON via `pc_bindings.inc`. Missing preference starts hidden; F1 and Hide Menu Bar persist immediately through the existing atomic writer, preserving bindings and other JSON. Window sizing uses actual menu visibility. Native Mac uses its system menu and F1 pointer capture, so this Windows bar preference does not alter that host.

PC Settings bar persistence is covered by the existing Windows `tsfp_pc_settings_test`: absent/hidden/shown, reattachment, fresh-process restoration, read-only failure and preservation through binding saves. The standard Build action runs it on isolated test JSON.

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
