# Controller deadzones (#529)

Shared `input_bindings.c` patch2950 extends the final2940 sampler. Each input
port has raw `move_deadzone` and `aim_deadzone` values0..32767; absent/invalid
values inherit the existing `deadzone` (default7849). Never rewrite legacy
values just by opening a menu or saving unrelated launcher settings. Keyboard,
triggers and stick halves assigned to buttons retain their existing policy.
Physical axes mapped to movement/look use the destination axis's setting.

`pc_input_radial_axis_magnitude` removes the selected radial threshold. Only
explicitly tuned pad axes then call `pc_input_compensate_stick_deadzone` to
invert the original US18FF70 decoder's10% per-axis threshold (constant475D24).
Zero remains zero. The decoder's integer quantization and subsequent native
sensitivity/aim curve remain; this is not a new acceleration curve. Untouched
legacy axes retain both original thresholds. Avoid a second compensating layer
in platform adapters. Windows, SDL Mac and Linux use the same source.

`pc_settings_deadzone` reads a locked port snapshot; `_save` validates, writes
only the selected JSON member through the existing atomic writer, then publishes
under the bindings lock. Failed writes leave live values unchanged. Sampling
uses the existing per-port Controller snapshot, with no per-poll filesystem IO.
`controls_deadzone_build/poll` creates native percentage selectors252/253 in the
Controller tab above the native footer divider (before Turn Speed), mapped through the selected profile's local port. Every value
owns guest text storage. Changes save live independently of native Accept/Cancel;
opening/rebuilding preserves unrounded raw legacy values. Main and pause share it.

Launcher GameSettings preserves raw values; SettingsPage exposes two percentage
sliders per player in Controls / Controller deadzones, with live percent
readouts, 0..100 bounds and 1% steps. Each player has a collapsible section;
only Player1 starts open. Expansion neither saves nor changes the draft. Only edited sliders update the model; Save uses the existing transaction/conflict checks.
Launcher settings apply on the next game launch, not through live IPC.

Validation routes: test_controller_profile.py checks actual parser/sampler,
legacy fallback, invalid fields, separate aim/movement and both input adapters.
test_fov_settings.py checks native row selection/live state. Original US decoder
sweep in native_analog_input.c checks compensation and untouched legacy behavior;
its private-XBE test skips without the supported image. launcher_shared_ui.cs
checks percentage UI, per-player persistence and unrelated saves. Physical
controller/Steam Deck drift and aiming feel require hardware acceptance.
