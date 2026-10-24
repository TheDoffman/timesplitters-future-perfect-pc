# Subsystem entry points

<!-- packet:start -->
Root tracked policy: `src/pc_input*`, `pc_controls*`, `pc_bindings.inc` for input;
`pc_display*`, `pc_settings*`, `pc_refresh*` for display/configuration;
`pc_menu_*`, `pc_ui_layout.h`, `pc_cutscene.h`, `pc_camera.h` for menus/camera;
`pc_debug_options.def` for the shared debug fields/keys/labels/capabilities;
`src/graphics/` for common shader/math policy; `src/native/` for host adapters.
Patches connect these files to upstream; the generated guest files are not the
authoritative place to implement portable behavior.

Textures: root `src/texture_pack*`, `texture_mode*`, runtime `src/d3d/` and HLE
texture hooks; offline tooling `tools/texture-*`. Sanitized replacements now live
in `assets/textures` (DDS via Git LFS), audited by `tools/build_textures.py` for optional delivery;
private source art and original assets stay local. Networking: `src/pc_network*`,
`pc_online*`, runtime HLE/kernel patches and `tools/online.py`. Crash capture:
`src/pc_crash*`, `pc_hang_policy.h`, runtime crash hooks and launcher diagnostics.
Audio/movie timing is runtime `src/hle/hle_dsound*`, `hle_xmv*`, not launcher code.
Find the connecting patch by symbol/path using `tools/repo-context.py find`.
Consult `docs/shared-platform.md` for shared versus adapter responsibilities.
<!-- packet:end -->

Native vehicle/physics candidate (`fix/native-vehicle-physics`):
`pc_native_numeric.h` opens vehicle gate `0x17E6C3`, scales linear adjustment
`0x17E6EC` by the smaller live raw/game interval capped at one tick, and retains
the rest timer's authored raw/game cadence at byte load `0x176673`. The same
policy now applies at rigid-body memory DEC `0x171F83`, preventing resting
bodies from sleeping 2/2.4 times too soon at 120/144.
Rigid-clock loads `0x171FB9/0x17208E/0x172119` select `ceil(game_delta)` solver
substeps; multiply `0x172099` uses the live interval instead of a fifteen-frame
average. Original 60 remains on its existing arithmetic. On native-to-original
handoff, `pc_native_timing_guest.inc` rebases that old history at the first
advancing interval, deferring across pause. Routine tests establish elapsed-time
and timer behavior. Original body integration now has force/drag/rotation and
controlled support-contact checks at all three rates. The original entity
publisher and bounds transform, sleep/wake and vehicle component integration
now run in that fixture. Real broadphase geometry, model/render
notification and Mac gameplay remain unverified. The scoped
RNG helper in `pc_native_physics_guest.h` now keeps additional primary solves
from advancing the shared sequence through contact/constraint shuffle sites;
controlled three-contact tests verify its cadence. `17164F` also prevents a
rounding-induced ninth secondary solve. These results do not certify arbitrary
contact geometry or vehicle handling. A subsequent Windows build runs actual
Siberia jeep driving at all presets, but matched throttle replays expose lower
travel at native rates; see STATUS for measurements and limits. The playable
combined crowd build was restored while this discrepancy is investigated.
The original driver dispatcher `17DA00` independently retained a whole-tick
gate at `17DA03`, dropping throttle on intervening native updates. Its numeric
policy now uses positive game time, matching the vehicle consumer's cadence.
The original trigger producer `172EE0` checks forward, reverse, braking and
release on fractional frames; seat weapons and live handling remain unverified.
The rebuilt live replay now matches initial acceleration within 0.1% across
the three presets, but later terrain contact differs. Do not infer collision
parity or measured 144 FPS from that corrected throttle window.
The subsequent `166899` correction keeps character contact velocity fresh
on fractional updates. Original contact math and the body callback forwarding
path verify the fix. In the controlled co-moving support case, body motion
was already correct; the previously stale normal/vector notification changes.
Character movement and world geometry remain fixture inputs, not live coverage.
The object-spawn gate `172AB5` now retains animation-derived initial motion
on fractional events. The original history copy and spawn/setter path test
linear velocity and angular momentum, including a rotated owner transform,
an offset model vertex, pause, absent history and allocation failure. This is
shared numeric policy; live visual/physics and native Mac acceptance are pending.
Windows candidate `3e1ee87` builds and passes its built-in checks with both
contact and spawn fixes; it is retained separately from normal playable `4adb6b0`.
Actor/vehicle impact divide `17CBD6` now preserves the native 60 conversion
factor: actor entity velocity is already normalized by the post-update.
Original mode and contact dispatch cadence remain unchanged. Original
publication, event queue, contact construction, impulse selection and actual
suspension component changes are covered by `native_vehicle_actor_impact.c`.
This establishes response to controlled individual contacts, not repeated
contact dynamics or live driving acceptance.
## Texture-upload timing diagnostic

`texture_pack_trace.h` records bounded opt-in CPU wall spans when
`TSFP_TEXTURE_UPLOAD_TRACE` names a CSV. The common-RHI loader and legacy GL
adapter share it, including the traced reader in `texture_pack_io.h`.
The disabled collector performs no clock/file work. Rows identify key, backend,
mode, dimensions, outcome and monotonic nanosecond boundaries for source-key
hashing (including `cached` calls), open, transfer/allocation,
validation, image creation and (common RHI) view creation. This is CPU elapsed
time, not GPU completion. Reading/validation/fallback and live texture switching
are unchanged. Logging occurs after the measured end, can perturb subsequent
frames and is capped at 8,192 rows/below 4 MiB; do not use it as clean pacing data.
Cached rows have zero read/image/view stages and retain the same loaded resources.
The legacy GL path also closes a trace with `path_rejected` for an oversized path.
Older `2230901` traces began after hashing and excluded cached calls.
See `docs/textures.md` and the runtime tests for usage and failure/bound checks.

| Area | Start here (tracked root unless noted) | Useful checks/docs |
| --- | --- | --- |
| Bindings, input prompts | `src/pc_controls.c`, `pc_input.c`, `pc_bindings.inc`, `pc_prompts.c` | `tests/pc_controls.c`, `tests/pc_prompts.c`, `docs/windows-settings.md` |
| Mouse/menu handling | `src/pc_menu_mouse_guest.inc`, `pc_menu_choices.inc`, `pc_menu_sliders.inc`, `pc_menu_team.inc` | `tests/test_menu_mouse.py`, `test_terminal_mouse.py`, `test_ui_layout.py` |
| Auto-look / camera / cutscene | `src/pc_auto_look_guest.inc`, `pc_camera.h`, `pc_cutscene.h`, `pc_ui_layout.h` | `tests/test_auto_look.py`, `test_cutscene_letterbox.py`, `test_camera_feed.py` |
| Display model and native UI | `src/pc_display.c`, `pc_display_*.inc`, `pc_settings.c`, `src/native/` | `tests/test_pc_display_modes.py`, `test_debug_settings_model.py` |
| GPU clear / screen copies / sampling | `src/pc_clear_rhi.inc`, `src/graphics/`, runtime `src/d3d/` | `tests/test_rhi_menu_sampling.py`, `tests/rhi/`, `docs/shared-platform.md` |
| Texture loading and formats | `src/texture_pack.c`, `texture_pack_format.h`, `texture_pack_io.h`, `texture_pack_stream.h`, `texture_mode.c` | `tests/test_texture_runtime.py`, `tests/texture_pack_tools.py` |
| Texture production and audit | `tools/texture-worklist.py`, `tools/texture-hybrid-plan.py` | Root AGENTS texture rules, `docs/textures.md` |
| System Link / online / bots | `src/pc_network.c`, `pc_network_guest.inc`, `pc_network_bots*`, `pc_online.c`, `tools/online.py` | `tests/test_network_guest_dispatch.py`, `test_online.py`, `test_network_identity.py` |
| Discord presence | `src/pc_discord*` plus runtime integration patches | `tests/test_discord_presence.py`, `test_discord_macos.py` |
| Saves and profiles | Runtime save-routing patch; `tools/testing_profile.py`, `tools/test-native-mac.py` | `docs/testing-profile.md`, `tests/test_testing_profile.py` |
| Crash/hang diagnostics | `src/pc_crash.c`, `pc_hang_policy.h`, `tools/crash-symbols.py` | `tests/test_crash_capture.py`, `test_native_crash.py`, `test_crash_symbols.py` |
| Guest lift/runtime correctness | Runtime `tools/recomp/`, manual hooks, `tools/recomp-generation.py` | `tests/test_recomp_float_joins.py`, `test_recomp_vehicle_joins.py` |
| Audio/movie startup | Runtime `src/hle/` sound and XMV files | `tests/test_audio_timing.py`, `test_xmv_stop.py` |

Names in this table are entry points, not a complete call graph. Read the
applicable adapter plus one relevant test. Tests may encode historical migration
or compatibility behavior, so confirm current defaults in root instructions and
effective source. Preserve unknown/unsupported saved settings across hosts.

The shared title header `pc_gpu_ack.h` selects the audited PFB flush store through
the generated `RECOMP_AFTER_STORE32` hook. It is included by the existing title
policy header for both Windows and Mac, independent of native timing activation.
The runtime validates the request and wakes its existing acknowledgement worker;
game input, clock policy, frame target and saved settings are unchanged. See the
runtime guide for register guards and the required regeneration boundary.

Ghost Gun living-actor reveal alignment (#181/#241) is handled by
`pc_display_weapon_effect` in `src/pc_display_ui.inc`. The integer rectangle
returning to US caller `0x001232A9` is the final filtered screen-copy composite
over the actor's projected bounds. In a full widescreen view it uses full-view
placement, with deferred draws flushed before restoring the surrounding UI
placement. Guest coordinates, UVs/tint and the five preceding blur passes stay
unchanged. The separate humanoid ghost composite retains its existing guarded
call site. Original 4:3 and split-view paths keep their existing behavior.
`tests/test_ghost_actor_overlay.py` covers placement, deferred batches, ABI,
both platform wrappers and a local supported-US-routine check; see build/test
for test prerequisites. Windows scene confirmation is recorded in `STATUS.md`;
native Mac visual validation remains pending under MAC-036.

## State and data boundaries

Online bot presentation (#222) uses `pc_network_bot_motion.h` from the
`pc_network_bots_guest_frame` hook. The transport still consumes each validated
host snapshot once. The guest caches it and updates the displayed body, spatial
bounds and matching animation frame times every frame over a 50 ms span, retargeting
from the current display pose when arrivals vary. Discrete vitals, scores, shots,
absence and spawn transitions retain host values; no guest AI or extrapolation
is enabled. A gap over 250 ms snaps to the next received host state. Match,
host, actor-pool and inactive-state changes clear presentation history. The
Windows QPC adapter supplies monotonic seconds independently of guest game time.
Human controller replication and the host's three-tick send interval are unchanged.
Native Mac fallback remains original until its network adapter exists (MAC-052).
Only clip-block `+8` (model `DC`/`134`) is interpolated within the same clip/cycle;
the other animation channels and crossfade controls stay atomic. US `5CEF0`
can promote the secondary clip during zero-advance evaluation, so mixing fade
duration/elapsed with another snapshot's clip identity can select an invalid clip.

Camera and perspective corrections use `pc_display_memory.h` to retain one
OS-validated writable region only after their original guest routine returns.
The stack-local snapshot expires when the correction returns; no guest calls,
allocations or protection changes may occur while it is reused. Bounds,
overflow, read-only and guard/no-access rejection remain enforced. Other
display/menu validation still uses its existing checks. This avoids repeating
the VM query for camera, output and source matrices within the same region;
it does not cache matrices, reduce updates or change projection arithmetic.
Decal corrections validate the camera and its source matrix before the output
because the original callers build their temporary output on the guest stack.
Separate camera/stack regions therefore need two queries, not three.

The native SDL input self-test ends its synthetic terminal context before
testing independent mouse capture/fire behavior. The fractional-terminal case
otherwise leaves `camera_input.terminal_active` set after its mock guest is freed,
suppressing the next case's Fire button. This is test isolation inside
`tsfp_native_input_test`; ordinary frame/input behavior is unchanged.

`config/*.default.*` is tracked default configuration; `config/pc-controls.json`
and `config/pc-display.ini` are personal. `private/game` is the single extracted
game-data copy; `private/saves` is personal; `private/test-fixtures/unlocked` is
the canonical local fixture and must remain unchanged. Test runners copy it into
bounded disposable test roots. `private/texture-remaster/pack` is the one active
6-GiB-capped art-production pack; source art and original game assets stay ignored.
Sanitized compiled DDS/JSON replacements are tracked in `assets/textures` and
published as optional packs; `tools/texture-repository.py` stages them with LFS,
`tools/build_textures.py` audits them. Leave pointers unhydrated for unrelated
source work. Optional pack publication must reject missing LFS content. Ordinary
game packages omit textures; existing installed packs and runtime fallback remain.

For installed layouts, use the launcher platform adapter instead of assuming
checkout names. Never include private data, generated guest code, credentials,
environment dumps or personal configuration in durable context. Store paths,
symbols, semantics and sanitized measured conclusions instead.

## Prompt text layout read scopes

`pc_prompt_guest.inc` reads through `pc_prompts_read_region`; the bounded
thread-local `pc_text_read_scope.h` cache uses existing OS VM query adapters.
It is valid only during synchronous text layout, with no mapping/protection
changes or event dispatch. Nested scopes retain intervals; the next outer scope
clears them. Scratch writes still use a fresh protection query.
The cached interval covers the full OS-validated region, so descending guest
stack reads and earlier labels in the same mapping reuse one query. It never
extends beyond that region or survives the outer layout scope.
The US outlined-label wrapper (`sub_00041E30`) retains this same scope over
its four outline draws and foreground pass, then expires it at return.
Scope wrapping, label construction/drawing and nested prompt measurement use the shared
hooks in `pc_tutorial_ui.inc`, `pc_display_ui.inc` and `pc_prompt_guest.inc`.
`pc_display_menu.c` includes `pc_prompts.h` before the .inc callers to avoid
implicit declarations. Do not extend caches across menu ticks or frames.
Tests cover region boundaries/expiry and actual prompt-hook scratch ABI/fallbacks.
Issue #105 also requires the runtime late-frame pacing patch; caching alone
measured 30 FPS. See STATUS.md for measured acceptance and Mac limits.

## Physical keyboard text entry (#136)

`src/pc_text_input.h` defines the shared bounded per-dialog text queue and edit
commands. Windows `pc_controls.c` supplies WM_CHAR/WM_UNICHAR; native Mac
`input_sdl.c` supplies SDL text events and starts/stops text input on its main
thread. Both adapters suppress keyboard menu bindings while a text owner is active
and discard queued text on focus loss or owner changes.

`src/pc_menu_text.inc`, included by `pc_menu_mouse_guest.inc`, recognizes live
character-row callbacks (`0x0007DB20`) rather than one dialog descriptor. Create
Profile embeds this keyboard in descriptor `0x004E8D54`; testing only standalone
`0x004D8D78` misses it. The original guest insertion/edit/Done routines retain
selection replacement, limits and save behavior. Escape queues the owning menu's
Back action, preserving embedded-menu callbacks. Numeric descriptor `0x004D94D4`
is excluded. Supported input follows the original Latin-1 tables; paste and
arbitrary Unicode are outside this change. See MAC-059 and the #136 STATUS entry
for physical Windows confirmation and pending Mac/dialog-matrix validation.

## Automatic display size

`pc_display_current()` is the selected/persisted mode: `0,0` retains console
resolution, `1,1` means Auto, and other valid dimensions are manual. New missing
settings default to Auto; existing settings are preserved. Render/camera/UI code
must use `pc_display_effective()` or the shape/scale helpers, never interpret
the Auto sentinel as physical pixels. The shared catalog appends Auto without
renumbering existing ratio groups. Guest Display settings and Cocoa expose it.

On Windows Auto reads the fullscreen window's nearest monitor desktop mode,
or its non-minimized client pixels in windowed mode. The SDL main thread
publishes drawable pixels via `pc_display_host_size`; the render thread reads
an atomic snapshot. Frame completion checks at most four times per second;
GPU allocation failure retains the last effective size and retries later.
Unsupported/empty sizes retain the last valid output (guest output before a window
exists); current supported limits remain landscape 640x480 through 7680x4320,
no wider than 4:1. Manual selection disables this tracking. Desktop resolution
and refresh rate are never changed.

## Crane viewport edge clipping (#221)

`pc_ui_vignette_clip` in `pc_ui_layout.h` converts the crane/remote-view
vignette's full 0,0..639,479 clip to host-exclusive 640,480 bounds only for
STRETCH tag `0x00137010`. Shared RHI `d3d8_GetScissor` and legacy GL
`native_apply_scissor` apply this policy before output scaling. Stored guest
clips, other tags and partial rectangles remain unchanged. The title helper
scope supplies the tag only for complete single-player non-4:3 views.

## Mounted and CCTV gun mouse aim (#182)

`src/pc_mounted.h` handles guest player mode `0x80` before ordinary player
camera updates in both `pc_controls.c` and `native/input_sdl.c`. The US guest
`sub_0025D520`/`sub_00258A20` aim an AGUN through player entity data `+0xC14`;
ordinary player yaw/pitch do not turn it. The shared helper validates its AGUN
tag and reverse owner link, uses local input port zero (not guest player ID),
and updates gun pitch/yaw with authored limits, sensitivity, inversion, FOV and
optional gun speed. Mouse displacement is consumed once without frame-time
multiplication. Invalid ownership, pause, focus loss and stale motion consume
input without falling through to hidden player angles. Terminal mode `0x2000`
and its left-click mapping retain `pc_terminal_mouse` behavior. Controller
input remains in the guest routines. The separate vehicle-seat mode `0x8000`
is not routed by this helper. `tests/test_mounted_mouse.py` runs the guarded C
fixture on Windows/macOS; physical Mac validation is tracked under MAC-001.
The mounted helper receives the adapter's explicit native-timing selection.
Native mode accepts finite positive game-float time at `4E794C`; Original60
retains its whole-time gate at `4E7944`. Both consume/rebase mouse batches with
`pc_input_consume_motion`, preventing fresh packets from expiring against a
previous batch timestamp. Paused/invalid native time consumes without aiming.
The helper still handles only AGUN mode `0x80`, not vehicle-seat mode `0x8000`.
Host input self-tests route synthetic motion through `pc_controls_apply_mouse`
in Windows and SDL, with zero-whole positive time, paused stale whole time,
live return to Original60 and return to on-foot controls. Windows execution
passes; native SDL self-test execution remains a host validation requirement.

## Unified Video Options and native presets (PR #285)

`pc_display_menu.c` stages resolution (including Auto), aspect, vertical FOV,
texture style, FPS and subtitles behind one dirty-state Apply. Auto uses the
`1,1` saved sentinel and is labeled Auto, never displayed as a pixel size.
Controls follows Video Options; conditional frontend Cheats is last. Audio
retains its frontend/pause-specific alignment and common highlights are rounded.
`pc_frame_rate_settings.h` persists the beta presets; live requests enter
`pc_frame_rate_live.inc` and commit at the guest clock boundary.

The merge retains main's nested text-read scope and fresh scratch-write checks
rather than the older per-hook stack-query optimization. The real-protection
fixture in `tests/pc_prompts.c` retains boundary/ABI checks; query reduction is
measured inside the cache by `tests/text_read_scope.c`, because reads now execute
in a separately compiled translation unit. Mounted mouse aim remains ahead of
ordinary camera input in both adapters; fractional terminal handling is retained.
Native high-refresh validation is MAC-060, distinct from crane MAC-058 and
text input MAC-059.

## Issue #74 menu texture additions

The branch pack now contains 2,046 accepted replacements. The same keyed DDS
files serve Windows and the common native Mac renderer; loader contracts are
unchanged. Menu coverage is 574/750 keys, with 164 original character icons, eight
intentional functional originals and four unresolved replacements.
`config/texture-originals.json` records excluded icon keys; the worklist rejects
installed replacements for those keys and hybrid planning retains the original
lane. Character model materials remain in scope. Source resampling and original-detail
recovery are distinguished from newly generated art in `docs/main-menu-textures.md`.
Offline acceptance does not certify mapped scene appearance or Mac GPU behavior.

The original-icon policy also includes historical public DDS/JSON hashes.
`build_textures.ensure_pack` rejects reintroduced icon keys and emits these
identities in `BUILD.json` under `textures.retired` for both game packagers.
The shared launcher retires only matching installed pairs transactionally;
custom artwork stays protected. See the launcher guide for recovery behavior.

## Native movement boost resource

The US walking routine `sub_00258F50` gates a movement boost on mode byte
`0x7DE456 == 0x11`, selected player `0x7DE498`, input bit `0x800`, and positive
entity+0x1D0 resource. The fixed subtraction at `0x259060` is now scoped through
`pc_native_boost_drain` in `pc_native_numeric.h`. Native frames split one active
original-update charge by `max(0, min(1, raw_delta, game_delta))`: slow motion
retains the original zero-whole-tick gate's reduced consumption; fast/catch-up
updates still consume at most one charge. Original mode and other sites retain
exact subtraction. No constant table, boost speed, threshold or stored clock
is changed. The complete US routine fixture uses a controlled flat collision
floor; it does not establish real-map geometry, input acceleration or network
transport parity. The game rule-name table at 0x4D1F90 + index*0x20, decoded
through the existing Discord string-table path, identifies index 17 as Escort.
A bounded frontend lookup confirms the name; live boost activation remains
unverified.

Escort recovery takes a separate route: active-match `sub_00068830` calls
`sub_000681C0`, then `sub_000654E0`. Its original game-clock delay is more than
five seconds; eligible match updates include zero-whole-game-tick frames.
`pc_native_boost_recover` therefore scales only addition site `0x6552D` by
`max(0,min(1,raw_delta))`. The original dispatcher gates pause; its selected-actor
write and mirror, threshold overshoot and health-dependent clamp are preserved.
The full three-routine fixture controls external services and validates those
boundaries separately from the walking drain's active-game-update budget.

## Experimental water coefficient precision

`pc_surface_coefficients` in `pc_surface_bias.h` preserves the current guest
uploader's clamped/truncated byte anchors, then scales acceleration/displacement
and exponentiates damping without repacking fractional values. The existing
bias+float+fractional opt-in gates enable it. `pc_native_surface_guest.h` copies
original parameters into the HLE state before temporary guest scaling; whole
steps retain original uploads. The HLE path copies bank 0 stages 3/5/7 and bank
1 stage 6 into ordered commands only for shader 0x4BBF10, caching duplicates and
clearing on scope exit. Defaults and original/catch-up behavior stay unchanged.
The constant-uploader fixture documents the current lifter's truncation; this
change does not claim original Xbox MXCSR rounding fidelity or full wave parity.

The opt-in precision policy now selects full-float format 0x4B for registered
feedback intermediates. Final byte textures, guest headers, rate gating and
normal-play defaults are unchanged. Common-RHI promotion/demotion preserves
live GPU contents and capture bytes; the transaction still flushes before copy,
rebinds only on success and retains the old resource on failure. This removes
measured half-float accumulation error but does not prove original wave parity.

Optional game packages use `build_textures.optional_identity` to retain only the
small `textures.retired` migration, without any DDS/JSON payload or LFS access.
This preserves the original-icon fix using the existing shared launcher consumer;
ordinary missing texture files never cause removal of an installed pack.

## Direct keyboard inventory slots

`pc_weapon_slots.def` appends host-only actions after the 24 Xbox controls;
`pc_weapon_slots.h` owns press edges and expiring requests. `pc_bindings.inc`
loads/saves them through the normal binding editor and migrates only missing
slots. Stock White=1 and Black=2 move to free Z/X keys; existing custom and
explicitly unbound actions remain intact. No migration writes until Save.
Both Win32 and SDL gate requests on focus, mouse capture, menus and text input.

The additive keyboard-weapon-slots patch wraps US `sub_001BF140`, the original
player weapon-input routine. `pc_weapon_slots_guest.inc` runs on that guest
thread after ordinary input, using the ordered table at `0x4FC888`, the original
variant filter `sub_001BAA70`, usability `sub_001BA410` and selection
`sub_001BA130`, then refreshes the weapon HUD with `sub_00142820`. It deduplicates
weapon groups, checks local input slot zero rather than network player ID, and
never grants inventory. Empty slots do nothing. Settings expose exactly ten bindable
slots, defaulting to 1–9/0. The separate 70-entry internal table bound covers
weapon IDs and variants; it does not add settings rows.

## Themed live keyboard and mouse editor

`pc_keyboard_menu.inc` is included by `pc_display_menu.c`, adding Keyboard and
Mouse to both main Settings and paused Options. It uses the original menu
builders, callbacks, highlights and footer navigation. The controller-style
bindings pages draw a keyboard/mouse diagram with primary/alternate columns;
`pc_keyboard_menu_model.h` partitions all 34 actions across short pages.
`PC_BINDING_SLOTS` is two in the shared model and native dialogs; old keyboard
profiles use their first two sources without modifying disk until saved.
Controller profiles keep their existing source capacity.

`pc_binding_capture.h` handles opening-input release, capture, final-input
release, Escape cancellation, focus loss and timeout. `pc_bindings.inc` owns
its lock/state; Windows and SDL supply raw physical keys and wheel pulses, latching press
events so quick taps are retained between polling ticks.
While capturing, controller polling and menu pointer dispatch are suppressed,
so E/F/Y and mouse clicks bind without activating menu commands. Assignments
use the existing atomic settings save/conflict API. Mouse sensitivity previews
live, debounces writes, flushes on exit, and restores previous values on error.

Capture starts once in the new page update. Native descriptor +12 is a per-frame
callback, not a destructor; installing capture cleanup there cancels it every
frame. The callback fixture in `tests/fov_settings.c` exercises both native
per-frame callbacks and requires a selectable capture instruction row.

## Host storage choices in Create Profile

`pc_storage_menu.inc` injects mounted host drive rows at native storage-button
site 001944C3, directly in Select Storage Device. Rows sort by drive letter on Windows and case-insensitive volume name on Mac;
Rows show only volume names and drive letters, with no game/save role suffix.
Native menu sizing is widened before its
title at 0019440B. Labels are ASCII-safe for the guest font. Windows lists mounted
drive letters; Mac lists the startup volume and /Volumes mounts. Neither lists
unmounted partitions. The native 00192770 update and 00191470 icon adapter keep
host indices away from the console device table. Selection delegates to original
00192E60 with console index zero, preserving the save/overwrite state machine.
After a drive change, synchronous 00024EC0/00190F70 enumeration rebuilds the native
profile list (74F9FC) before duplicate-name validation against the new location.

`pc_storage.inc` supplies actual volume metadata, cached five seconds for original
storage-text replacements. `pc_storage_routes.inc` validates a writable save
folder, prepares UserData/45410066 for the native free-space query, and atomically
saves save-location.txt beside RECOMP_INPUT_CONFIG. Changes immediately redirect
future UDATA opens through the registered kernel adapter; failed persistence
rolls that route back. Existing handles, caches and partition devices stay put.
Startup also honors the preference; profiles are neither moved nor deleted.
An unavailable configured location fails explicitly instead of silently
creating a new profile elsewhere. TSFP_SAVE_LOCATION_LOCK protects isolated test
sessions from changing real drive preferences. Windows resolves junctions and
volume identities; drive metadata retains executable-volume identity internally.
The Mac metadata adapter includes unistd.h before calling getattrlist; sys/attr.h
provides attribute structures but not that function declaration. The production
pc_prompts.c compile contract checks this include order in Mac CI.

## Experimental coupled water operator

`pc_surface_capture.h` defines the copied original-feedback snapshot and validates
the guest resource's input/first-pass/output indices and texture records.
`pc_surface_capture_guest.h` validates mapped memory, calls the original feedback
once, and submits eligible Scotland samples. `pc_surface_capture_hle.inc` reads
the existing RHI images at that point in the deferred command stream. It writes
at most 16 records to the explicitly requested `TSFP_SURFACE_CAPTURE` file,
normalizes BGRA to RGBA, and stops on unsupported identity/storage or I/O failure.
No image state is replaced. The older fractional/float experiments disable the
probe. Each TSWATER1 record contains 23 little-endian metadata words plus three
128-square RGBA8 images (input, first pass, second pass). GPU readback stalls
make these diagnostic runs unsuitable for performance claims. Common RHI is
supported by the source; legacy GL intentionally performs no capture.

`graphics/surface_coupled.h` computes a fractional power of the coupled affine
velocity/displacement step, anchored to the existing one-tick coefficients.
`graphics/surface_coupled_shader.h` supplies one shader body with vector-type
adapters. `graphics/surface_transform_shader.h` supplies separable transforms with an
explicit axis extent. `graphics/surface_basis.h` owns clamped DCT-II and wrapped
real Hartley weights and the corresponding diagonal-stencil eigenvalues, bounded
to 128 texels per axis. The captured game path uses 128x128 **wrap**, so the
earlier clamped DCT result is only a diagnostic. Texture-load/vector adapters
remain separate from shared spectral policy. The current
common-RHI fixture reuses its basis and scratch resources and reconstructs the
world field on the GPU after every fractional update. These are experimental helpers exercised by CPU/common-RHI fixtures;
the game does not call them and no normal-play or legacy-GL policy changes.
The matrix's restoring coefficient belongs to a spatial eigenmode, not an
independent world texel. A runtime integration needs spatial transforms and the
original disturbance/smoothing/storage rules. Float32 equivalence alone does
not preserve the original byte-feedback appearance. A separate Original60 GPU
reference now includes cardinal smoothing before feedback; the storage gap
persists there too. This changes validation coverage, not runtime ownership.
`graphics/surface_state_shader.h` converts reconstructed signed velocity/height
to the original texture channels: red scratch, green velocity plus 0.5, blue
height and **alpha height**. Original effect 22 samples alpha cardinal differences
for its ripple shading input; constant alpha would erase those slopes. This
shared encoding is currently exercised only by the GPU fixture, not game hooks.
`pc_surface_coupled_forced_step` stores the fractional response to a held forcing
input in each row's fourth component; `TSFP_SURFACE_FORCED_SHADER` applies that
input in the same spectral basis. The rounding fixture refreshes the measured
first-pass residual at authored boundaries. `surface_gradient_shader.h` supplies
the unclipped linear gradient: subtracting this from actual byte red retains
both clipping and rounding. Subtracting the already-clipped float shader output
loses the clipping contribution and fails strong initial impulses.
`surface_feedback_residual` also includes the second pass's signed acceleration
clamp after adding the fourth diagonal neighbor. `surface_limit_boundary`
applies final velocity bounds and the height cap before damping at authored
boundaries; it does not clip the internal state after every fractional piece.
`surface_rounding_shader.h` canonicalizes nearby values with original shader
arithmetic evaluated at that boundary, only within 32 float epsilons. It does
not predict byte thresholds: a hardware probe found an RTX transition away
from the ideal half-byte value. The earlier threshold-distance condition missed
that transition. Larger disagreements remain untouched and detectable; fractional
frames never use this authored-boundary correction.
The fixture requires byte initial/persistent state. No runtime scheduler/resource
owner calls this policy yet. The fixture covers mild/strong initial fields,
near-limit signed velocities/heights and
mixed-rate steps, including nonzero-phase switches; actual disturbance shaders,
full scene saturation coverage and live game scheduling remain acceptance work.
The original dispatcher `1956A0` calls queued disturbances `1953B0`, texture
noise `1952D0`, smoothing `1950B0`, feedback `195110`, then final output `195280`.
Effect 19 (`2BE30`) uses pixel definition `4BBD30` and vertex program `4BBBF8`.
Its captured noise draw adds the immutable A8L8 texture into green with ONE/ONE
blend and RGB writes, before smoothing. The candidate fixture now executes
those unchanged shaders at authored boundaries with controlled UV offsets;
queued splash geometry and actual runtime RNG/event delivery remain unverified.
`graphics/surface_interval.h` splits exact host-side intervals into at most four
pieces (the existing maximum three-tick catch-up can cross four partial pieces).
Each piece marks authored start/end boundaries; zero elapsed time produces none.
It consumes `PcNativeStep` metadata without rounding float deltas or resetting
phase on rate changes. The GPU fixture uses this producer/splitter, including
a pause during motion. It is not yet a resource owner or an enabled game hook;
new resources and guest-local time overrides require explicit synchronization.
`graphics/surface_mode.h` packs each spatial mode's immutable eigenvalue and
fixed-point data into two float4s. `surface_evolve_shader.h` advances that mode
using current state, held residual and a delta/scale uniform. Unlike the earlier
fixture's per-delta coefficient-image cache, varying elapsed time requires only
a 16-byte buffer update and no coefficient-image allocation/upload. Real,
repeated and complex roots share this representation; CPU validation covers all
three and authored GPU trajectories use the complex branch.
`graphics/surface_gpu.h` now owns the coupled kernel's RHI images/views, shaders,
buffer and raster/depth/blend states. Create it on the render thread with fixed
coefficients/extent; begin from full single-mip float state/residual views, then
advance by each positive fractional piece. Zero time preserves output. Reset
invalidates the interval and the output accessor until a new begin; destroy before the device. Inputs
with incompatible storage/shape or hazardous internal aliases are rejected.
Creation failure unwinds every owned handle. It saves/restores output targets
and all viewports, including an empty list, and unbinds temporary textures and
its step buffer. It may leave other draw state changed under the RHI contract.
The owner executes the GPU fixture trajectories and the opt-in live GPU probe.
`pc_surface_gpu_begin_bytes` initializes from completed original input/first-pass
sampled byte views, retaining first-pass clipping/rounding and second-pass
acceleration saturation. It requires the authored center (128/255) and rejects
aliases or incompatible storage before drawing. Its unpack/residual shaders and
temporary residual image are owner-managed; ordinary float input remains supported.
Original disturbances/smoothing/byte boundaries remain caller-owned.
`graphics/surface_gpu_cache.h` provides a zero-initialized, render-thread registry
bounded to the constructor's five resources. It keys owners by resource ID,
generation, coefficients and four retained RHI image identities. Unchanged draws
reuse their owner without allocation; replacement is transactional and a full
registry returns NULL without evicting live entries. Clear before scene/device
reset and advance generation for in-place reinitialization. Invalid or failed
requests return NULL, never a stale owner. This registry runs in the fixtures;
guest/HLE reset hooks, local time scopes and actual event ordering still need wiring.
The nine 128-square float4 images contain 2.25 MiB of pixels per owner; this is not
a total VRAM or in-game performance measurement.
`TSFP_SURFACE_GPU_CAPTURE=1` extends the bounded capture with shared-kernel
execution on the renderer thread. Each sample initializes from its actual GPU
views, advances 5/12 + 5/12 + 1/6, and writes the first-piece/full-tick float
fields to a `.gpu` sidecar. It creates/releases the owner within that callback,
so no diagnostic resources survive scene/device teardown. This allocation and
readback-heavy probe is not the eventual persistent gameplay path and does not
replace visible water or alter guest event/RNG order.
See the coupled-operator
section in `docs/native-high-refresh-regressions.md` for evidence and limits.
## Background replacement texture reads

`texture_pack_stream.h` owns a shared single-reader queue for common RHI and
legacy GL. The worker reads/validates DDS data without touching texture owners or
GPU resources. Submit/cancel/pump run on the renderer thread; release and content
changes cancel pending owners before reuse. A full 64-slot queue retries when the
texture is next bound. Two completed payloads and one read bound CPU staging to
72 MiB; the existing 512-MiB GPU budget and DDS/alpha rules still apply.

Original views remain usable during loading. Each Present pumps at most one
nonempty payload into GPU resources; F10 switches cached views immediately. The
GL adapter restores the previous texture binding after upload. Shutdown joins
the reader; normal cancellation never waits for disk. `TSFP_TEXTURE_STREAMING=0`
selects the synchronous diagnostic fallback. Per-texture messages now require
`TSFP_TEXTURE_PACK_TRACE=1`; explicit original dumping remains synchronous.

The common renderer pumps from present_resolve, shared by Swap and Present.
The initial Present-only prototype never delivered replacements in this title;
its apparent pacing gains are invalid evidence. Actual DDS upload records and
rendered Enhanced scenes are required alongside queue unit tests.

The in-game Video Options Apply path calls the explicit tsfp_texture_set_4k
setter. Unlike the host toggle, that setter does not emit a mode log line.
Reopening Video Options reads the effective mode; a pending row label alone is
not evidence that the change was applied. Texture selection is session-only.

Integration #308/#311: upload timing follows the queued request by value. Only
the renderer opens/writes/closes the bounded CSV; the worker stamps read times
in its exclusively owned record and publishes it under the queue lock. Completed
records measure hash, worker read/validation and renderer image/view creation.
Total duration includes queue/frame scheduling waits, not GPU completion.
Cancellation discards the record with the payload; a full queue logs its failed
attempt and starts a new record on retry. Disabled timing takes no clocks/files.
