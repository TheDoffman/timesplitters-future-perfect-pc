# Subsystem entry points

Story chooser centering/reentry/lobby host loss, Game Over Quit, guest camera/weapon lifetime, in-progress rejoin, guest departure and in-match abort routing, bomb/barricade effects, Online Back navigation/browser level details, co-op movie score-packet lifetime, guest reward pages/context, NPC arms, corpse, pickup motion, barrel and camouflage integration keeps the
shared source and main paths; see [details](systems-details.md).

Battle Mech mouse: [shared routing and guards](mounted-mouse.md#battle-mech-mouse-425).

Minigun motor intent and release damping: [shared policy](native-minigun.md).

Kernel: [objects and boot](kernel-objects.md).

Memory: [retired APIs](retired-memory-compatibility.md).

Guest storage: [allocation](independent.md#guest-allocation-ownership), [threads](thread-memory.md), [virtual memory](virtual-memory.md).

Image/state/ABI: [image](independent.md#native-image-loading), [execution](independent.md#guest-state-and-abi-reports).

Procedural wind: [ownership/time scopes](native-wind.md).

Local completed aim/animation diagnostics: [scope and limits](native-local-trace.md).

GPU state-cache acceptance: shared Vulkan/WARP pixel fixture and frontend
header configuration are in [runtime](runtime.md).

Native player contact: [clear-slide policy/trace](native-player-contact.md).
Live integer/vector register references: [four-site policy and tests](gpr-refs.md).
Script/audio clocks: [contracts](native-script-clocks.md).

<!-- packet:start -->
Tracked policy: `src/input/` owns controls, bindings and prompts;
`src/interface/` owns display, settings, menus and refresh configuration;
`src/gameplay/` owns simulation and camera policy;
`src/diagnostics/` owns shared debug fields, crashes and hang reporting;
`src/graphics/` for common shader/math policy; `src/native/` for host adapters.
For movement regressions, compare player-position displacement in game as well
as sampled stick values: the title attenuates diagonals after the binding
sampler. Windows mouse confinement is owned by `pc_controls.c`; native SDL
relative mode is owned by `src/native/input_sdl.c` and should recover from
focus for pending capture; frame-applied mouse disable releases it (SDL self-test).
Windows gameplay confinement now uses the tiny center rectangle from
`pc_mouse_capture.h`, rather than the entire client. Relative WM_INPUT counts
still drive aim; absolute pointer drift cannot reach the border. Initial capture,
move/resize and repair share the same foreground-only rectangle calculation.
Menu/settings/Escape/focus release remain owned by `pc_controls.c`; repair never
resets held input. This is a Win32 adapter fix (#381); SDL already uses relative
mode. The launcher-only trigger has not been independently established.
Patches connect these files to upstream; the generated guest files are not the
authoritative place to implement portable behavior.

Textures: `src/textures/`, runtime `src/d3d/` and HLE
texture hooks; offline tooling `tools/textures/`. Sanitized replacements now live
in `assets/textures` (DDS via Git LFS), audited by `tools/textures/build_textures.py` for optional delivery;
Private art stays local. Networking: `src/network/`, HLE/kernel patches and
`tools/network/online.py`; [Online UI](multiplayer-ui.md). Crash capture:
`src/diagnostics/`, runtime crash hooks and launcher diagnostics.
Audio: `src/hle/hle_dsound*`, `hle_xmv*`. [Beta audio](../development/positional-audio.md).
General buffering, lifecycle, format and device recovery are unconditional;
Enhanced Audio controls only the optional mix. Effects adapt their default
lead from 40 to 90 ms and refill immediately after skipping stale hitch data.
Explicit lead overrides stay fixed. See [recovery contracts/tests](audio-buffers.md).
`audio_output.h` owns `RECOMP_AUDIO_DEFAULT_GAIN` (0.5, about -6 dB).
XAudio applies it once to the master; SDL applies it once per stream. This
quiets the output at 100% without changing saved Music/Sound sliders, their
curves or relative mix. An explicit `RECOMP_AUDIO_GAIN` still overrides it.
Find the connecting patch by symbol/path using `tools/repo-context.py find`.

Consult `docs/development/shared-platform.md` for shared versus adapter responsibilities.
<!-- packet:end -->

Split-screen mouse look (#112): `pc_camera_mouse_primary_in_region` reads the
current player's local input slot at `player+4` from the fresh bounded camera
mapping. Both the Win32 and SDL `pc_controls_apply_mouse` adapters skip slots
other than zero before terminal, mounted-gun, or ordinary camera processing,
leaving the mouse packet for player one. The first player word is a game/network
ID and is not a local slot. The shared `tests/graphics/mouse_region.c` fixture,
run by `tests/gameplay/test_mounted_mouse.py`, checks both adapters and verifies
the deferred packet reaches player one. Mac gameplay remains unverified.

The #74 material continuation and validation limits are recorded in
[material validation](../textures/material-validation.md).

Hybrid production separates fixed macro layout from repainted microtexture.
Failed drafts stay held for specific UV, hardware, motif, tone or pattern defects.
Do not downsample defects away, infer approval from classification, or treat
whole-image brightness as proof of local fidelity. Native original references
are normally sufficient. Enlarged references and revised prompts have improved
some corrections, but those are asset-specific results, not a universal recipe.

Narrow source extraction only removes explicitly reviewed atlas padding within
the compiler's1%aspect tolerance; it cannot fix changed material geometry. Record
actual source dimensions, crop coordinates, resampling and output format. Keep
prior full artwork and prompt records for corrections. Confirm counts against
the original: mistaken prompt counts have occurred for grooves and grid cells.

Temporary outputs stay below256MiB. Retire only reproducible disposable reviews
or catalog-verified exported originals with resolved-parent and SHA checks;
preserve source art/provenance and never retry denied cleanup targets. One private
pack and small public LFS pointers avoid duplicate hydrated copies. Detailed
historical batch measurements, held reasons and storage audits remain in docs/project/STATUS.md
and private hybrid-N validation records; coverage is summarized in docs/textures/textures.md.
Scene testing remains deferred under the near-completion policy.

Character effects: the shared US1260C0 gate in `pc_native_numeric.h` admits
positive fractional game time for player/NPC effect updates. Its children already
consume the float delta; dropping subticks delayed attached-effect timers.
Original mode and zero-time pause retain their behavior. See runtime guide and
the original-dispatcher fixture for tested versus stubbed nested coverage.

Arcade HUD panels: `pc_display_ui.inc` gives both shared-panel callers
US12EC8B (RANK) and12EA9B (SCORE) the left anchor. Their original compositor
retains panel widths, spacing and sliding/fading; mismatched anchors otherwise
overlap them on widescreen, including Original60. Each full panel flushes before
placement changes and restores its enclosing placement. Other panel callers,
4:3 and split-screen retain their prior path. Shared Windows/native Mac code;
the UI fixture tests both callers and guards, while Mac scenes remain unverified.

Reload #337 and interaction #357 use advancing-time admission at their exact
US compare sites; see [input consumers and validation](interaction-input.md).
Keyboard reload buffering additionally uses `pc_reload_input.h`, owned by
`PcInput`, and US state snapshots in `pc_reload_guest.h`. Win32 and SDL publish
press/release events under input locks, including collapsed down/up events.
The shared sampler applies configured key roles before merging pad input.
A short Reload request lasts at
least 34 ms and expires after 150 ms if the weapon never enters reload state 7.
Already-held Fire is suppressed until release, reload completion or rejection;
a fresh Fire press can interrupt. Monotonic press order preserves Fire after
Reload within one poll, including events with identical clock timestamps.
Context expires after 100 ms without an eligible
local-player camera update; menu/capture/focus/controller transitions clear it.
Rejected reloads expire without altering ammo/animation logic.
Fire presses use the shared key-edge path and a
34 ms pulse to cross 60 Hz polls; held input stays live. Reload clears earlier
Fire pulses; later presses can interrupt. Context resets discard pulses.
Pad input stays separate.


`pc_model_transform_guest.inc` implements the US151E80 affine point fast path.
It checks disjoint/wrap-safe buffers and magnitudes at most2^33, then uses local
binary32 intermediates with explicit rounding, preserving final GPR/SSE state.
Overlap/nonfinite/large inputs use the unchanged generated body. Entry callbacks
precede all reads; no state persists across calls, no host adapter is needed.
`pc_model_transform_guest.c` includes the generated type header and builds once
for either host. Fallback NaN result payloads depend on host compiler allocation;
rejection itself preserves every guest bit and floating-point exception flag.

## Crash breadcrumbs and capture health

`pc_breadcrumbs.h` owns a 64-slot atomic numeric history shared by the Windows
`pc_crash.c` packet and Darwin `native/crash_posix.c` mapping. Producer calls
never allocate, wait or write files; helper snapshots omit interrupted slots.
`pc_display_camera` forwards scene/mode/menu/pause/network/cutscene values from
the existing safe one-second guest sampler, even with Discord presence disabled.
Capture/guest lifecycle and modal-depth events come from the host adapters.
The helper formats `breadcrumbs.txt` during a crash/hang; it is sampled context,
not a reconstructed call stack or guaranteed checkpoint/load-event history.
`pc_capture_health.h` validates the launcher session token and defines the small
heartbeat JSON. Each helper atomically replaces `capture-health.json` every two
seconds in its safe report directory. No valid token means no health file.
Windows packet schema is now3; a mismatched older helper cannot claim readiness.


Windows `pc_menu_theme.h` snapshots each native menu item's caption, tab-separated
shortcut and separator type before owner drawing. Command IDs stay untouched;
do not reintroduce positional label tables when adding PC Settings options.
The per-window metadata is freed on detach. Cocoa menus do not use this adapter.

Recorded-input wait/exit timing: `pc_native_demo_guest.inc` captures the incoming
raw interval around original `43220`, with nested TLS restoration. Numeric
multiplies `433E2/43473` use that interval instead of the original minimum-one
tick. State2 overwrites raw time while looping controllers; the scope prevents
later controllers from inheriting that overwrite. The executed exit site is
remembered across cleanup changing the mode. Exit/EOF retain game pause
but restore incoming raw whole/float time. Original60 keeps original behavior,
including its multi-controller multi-tick asymmetry.
When that path pauses both guest game-time fields, it also clears the published
game delta/ticks/exact units. Raw metadata and clock phases stay intact, so
procedural consumers cannot advance using the earlier pre-exit interval.

Recording state3 now serializes only on raw whole-tick frames. Scoped mode load
`433A0` skips that writer branch on intervening native frames; device polling
and lifecycle checks still run. Pending game ticks survive offset phases after
pause and a live return to Original60. After the original writer, the wrapper
corrects only its newly written records' game byte/start timestamp, preserving
live globals, input bytes and the14-byte format. Buffer identity/cursor bounds
guard those writes; `42535` clears host state on same-buffer initialization.
Unexpected level-clock rebases also clear pending history. An untouched
Original60 recording retains its original path.

`pc_native_demo_playback_guest.inc` schedules authored input groups with exact
raw units, retaining fractional phase across120/144 and a live60handoff. It calls
the original decoder on the selected group, then publishes fractional raw/game
intervals and matching `PcNativeStep` metadata. `pc_native_visual_rebase` preserves
the level clock's fractional offset. Recorded raw timestamps are interval-END
values; level timestamps are interval-START values. Zero-time polling keeps
prior held buttons and suppresses packet consumption. `42535` resets playback
as well as recording; identity changes invalidate history. Fresh bounded VM
reads precede decoding; unreadable/partial groups reach original EOF.

Old native zero-tick groups fold into the following positive group, including
pending game ticks. A leading zero activates that repair in Original60; ordinary
console recordings retain their Original60 decoder. Catch-up consumes at most
one authored group per update to preserve presses, repaying at most three ticks
of pending time on subsequent frames. Sustained sub60 playback can outrun that
budget; full-game/file playback, overload and native Mac acceptance remain open.

`PcNativeStep` now exposes exact `raw_units/raw_phase` alongside game units,
using `units_per_tick`. Both phases describe the interval's start. Raw units
advance during pause; reset/Original60 clear the metadata. Guest fields, clocks,
save formats and pacing are unchanged. Playback sampling must use exact raw
time rather than summing float deltas. Catch-up packets retain their original
multi-tick bytes; the round-trip fixture covers recorder/reader rate dependency.

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
Cached rows have zero read/image/view stages and retain existing request/cache state.
The legacy GL path also closes a trace with `path_rejected` for an oversized path.
Older `2230901` traces began after hashing and excluded cached calls.
See `docs/textures/textures.md` and the runtime tests for usage and failure/bound checks.

| Area | Start here (tracked root unless noted) | Useful checks/docs |
| --- | --- | --- |
| Bindings, input prompts | `src/input/pc_controls.c`, `pc_input.c`, `pc_bindings.inc`, `pc_prompts.c` | `tests/input/pc_controls.c`, `tests/input/pc_prompts.c`, `docs/players/windows-settings.md` |
| Mouse/menu handling | `src/interface/pc_menu_mouse_guest.inc`, `pc_menu_choices.inc`, `pc_menu_sliders.inc`, `pc_menu_team.inc` | `tests/input/test_menu_mouse.py`, `test_terminal_mouse.py`, `test_ui_layout.py` |
| Auto-look / camera / cutscene | `src/gameplay/pc_auto_look_guest.inc`, `pc_camera.h`, `pc_cutscene.h`, `pc_ui_layout.h` | `tests/gameplay/test_auto_look.py`, `test_cutscene_letterbox.py`, `test_camera_feed.py` |
| Display model and native UI | `src/interface/pc_display.c`, `pc_display_*.inc`, `pc_settings.c`, `src/native/` | `tests/graphics/test_pc_display_modes.py`, `test_debug_settings_model.py` |
| GPU clear / screen copies / sampling | `src/graphics/pc_clear_rhi.inc`, `src/graphics/`, runtime `src/d3d/` | `tests/graphics/test_rhi_menu_sampling.py`, `tests/rhi/`, `docs/development/shared-platform.md` |
| Texture loading and formats | `src/textures/texture_pack.c`, `texture_pack_format.h`, `texture_pack_io.h`, `texture_pack_stream.h`, `texture_mode.c` | `tests/textures/test_texture_runtime.py`, `tests/textures/texture_pack_tools.py` |
| Texture production and audit | `tools/textures/texture-worklist.py`, `tools/textures/texture-hybrid-plan.py` | Root AGENTS texture rules, `docs/textures/textures.md` |
| System Link / online / bots | `src/network/pc_network.c`, `pc_network_guest.inc`, `pc_network_bots*`, `pc_online.c`, `tools/network/online.py` | `tests/network/test_network_guest_dispatch.py`, `test_online.py`, `test_network_identity.py` |
| Discord presence | `src/pc_discord*` plus runtime integration patches | `tests/network/test_discord_presence.py`, `test_discord_macos.py` |
| Saves and profiles | Runtime save-routing patch; `tools/testing/testing_profile.py`, `tools/testing/test-native-mac.py` | `docs/development/testing-profile.md`, `tests/repository/test_testing_profile.py` |
| Crash/hang diagnostics | `src/diagnostics/pc_crash.c`, `pc_hang_policy.h`, `tools/release/crash-symbols.py` | `tests/diagnostics/test_crash_capture.py`, `test_native_crash.py`, `test_crash_symbols.py` |
| Guest lift/runtime correctness | Runtime `tools/recomp/`, manual hooks, `tools/build/recomp-generation.py` | `tests/compiler/test_recomp_float_joins.py`, `test_recomp_vehicle_joins.py` |
| Audio/movie startup | Runtime `src/hle/` sound and XMV files | `tests/media/test_audio_timing.py`, `test_xmv_stop.py` |

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
`pc_display_weapon_effect` in `src/interface/pc_display_ui.inc`. The integer rectangle
returning to US caller `0x001232A9` is the final filtered screen-copy composite
over the actor's projected bounds. In a full widescreen view it uses full-view
placement, with deferred draws flushed before restoring the surrounding UI
placement. Guest coordinates, UVs/tint and the five preceding blur passes stay
unchanged. The separate humanoid ghost composite retains its existing guarded
call site. Original 4:3 and split-view paths keep their existing behavior.
`tests/graphics/test_ghost_actor_overlay.py` covers placement, deferred batches, ABI,
both platform wrappers and a local supported-US-routine check; see build/test
for test prerequisites. Windows scene confirmation is recorded in `docs/project/STATUS.md`;
native Mac visual validation remains pending under MAC-036.

## State and data boundaries

The shared `graphics/present_metrics.h` recorder accepts optional
`TSFP_FRAME_PROFILE_DELAY_MS` (0..600000, default0; malformed values use0).
Only CSV rows are delayed from the first successful Present. Counters and the
last-success timestamp keep advancing, preserving skipped/failed gaps without
turning the startup delay into a false hitch. The 20,000-row cap is unchanged.
No delay environment lookup occurs without a profile path. Common RHI and native
GL share this behavior; native Mac execution remains pending (MAC-003).

`pc_auto_look_camera` keeps two fresh `PcGuestRegion` slots for its consecutive
current-player, player-record and control-setting accesses before the guest call.
It retains the global's region while visiting high-RAM player data, so settings
in the first region reuse validation. Bounded misses use `pc_guest_query_span`;
the complete 0x1504-byte player record remains validated. Windows checks its
pages in one bounded batch; Mach/full-region results may cover additional fields.
Read permissions cannot grant writes.
Snapshots are never consulted after guest execution or
retained for the next invocation. The original bit-3 save/restore and controller,
local-slot and menu rules are unchanged. `test_auto_look.py` checks host behavior
plus real multi-page records, split/read-only maps, expiry and uncached/persistent
mutations. Controller and menu paths retain their original preference behavior.

NPC facing: [degree angles and aim controls](systems-details.md#npc-facing).

Online bot presentation uses `pc_network_bot_motion.h` from the shared
`pc_network_bots_guest_frame` hook. Bots publish each simulation tick, suppressing
duplicate callbacks in that tick. Frozen guest skeletons need the full pose at
that cadence: atomic crossfade, overlay and death channels cannot be rebuilt
from the former three-tick (20 Hz at Original60) stream. Human replication stays
original. Guests consume validated snapshots once and refresh displayed bodies,
bounds and matching clip times every frame over50ms, retargeting from the current
display pose. Vitals, scores, shots, absence and spawn transitions stay atomic;
there is no guest AI or extrapolation. Gaps over250ms snap; match/host/pool/inactive
changes clear history. Windows QPC supplies monotonic time. Native networking
shares the policy through TSFP_NATIVE_NETWORK; see [Online](multiplayer-ui.md).
Only clip-block+8 (model DC/134) is interpolated within one clip/cycle. Other
animation channels stay atomic: US5CEF0 can promote the secondary clip during
zero-advance evaluation, so mixing crossfade state can select an invalid clip.

The bot score-network adapter admits configured AI at US return site67868,
using EDI's actor slot in the original round score-limit scan. The stock
controller-flag filter otherwise skips AI despite valid scores. Limit/team
calculation and end-of-round authority remain original. Invalid slots, humans,
disabled networking and native Mac retain fallback (MAC-052). The supported-US
round routine fixture checks threshold decisions; the Online runner can require
both peers' completed Results records.

Camera and perspective corrections use `pc_display_memory.h` with a stack-local
region after the original guest routine returns; no guest calls, allocations or
protection changes may occur while it is reused. Bounds, overflow, read-only and
guard/no-access rejection remain enforced. `pc_display_perspective_projection`
does not re-query its output: supported US `151CF0` writes all 16 floats without
guest callbacks before returning. Weapon/preview correction therefore needs no
VM query; decals still validate their camera and source matrix afresh, independent
of the output's guest-stack region. `pc_guest_span.h` supplies a fresh Windows
resident-page query for known-size accesses spanning at most four OS pages.
Every page must be valid/readable; writes require all pages writable. Failed,
cold, guarded or larger spans fall back to the full-region query. Camera/source pairs can
reuse that snapshot; different pages need fresh queries. Darwin retains its Mach
region query. Unbounded string scans still use the full-region API. This avoids
Windows scanning a large mapped allocation to find its end on each display guard.
The general `pc_display_menu.c::guest_writable` also delegates to this adapter,
with a fresh zeroed snapshot on every call, including calls around guest callbacks.
HUD, viewports, menus and network display helpers retain their requested bounds;
spans beyond four pages fall back to full-region validation. Darwin uses real
Mach protection for these calls instead of the compatibility stub's synthetic
4 KiB writable extent. This does not cache matrices, reduce updates or change
projection arithmetic. `projection_output.c` checks the real
US builder's output extent, arithmetic, canaries and stack; generated C stays local.

Discord's `pc_display_menu.c::discord_guest_read` also uses a fresh local bounded
span for each requested read, including individual string bytes. No permission
snapshot survives the callback. Decoder fields, one-second sampling cadence,
event-name cache and IPC submission remain unchanged. Windows resident reads
avoid full-allocation queries; cold/large reads retain fallback and Mac retains
Mach validation. `test_discord_presence.py::test_guest_read_memory` executes the
production callback with real protection changes, exact copied bytes, rejected
reads leaving output intact, fallback and retained/full-query negative controls.

The native SDL input self-test ends its synthetic terminal context before
testing independent mouse capture/fire behavior. The fractional-terminal case
otherwise leaves `camera_input.terminal_active` set after its mock guest is freed,
suppressing the next case's Fire button. This is test isolation inside
`tsfp_native_input_test`; ordinary frame/input behavior is unchanged.
Its Confirm checks use the fixture's two active keyboard slots (E and Space),
then verify that its retained third entry (Return) stays inactive. The keyboard
profile parser deliberately caps bindings at two; the test must not require a
third binding or change the shared parser to satisfy its fixture.

`config/*.default.*` is tracked default configuration; `config/pc-controls.json`
and `config/pc-display.ini` are personal. `private/game` is the single extracted
game-data copy; `private/saves` is personal; `private/test-fixtures/unlocked` is
the canonical local fixture and must remain unchanged. Test runners copy it into
bounded disposable test roots. `private/texture-remaster/pack` is the one active
6-GiB-capped art-production pack; source art and original game assets stay ignored.
Sanitized compiled DDS/JSON replacements are tracked in `assets/textures` and
published as optional packs; `tools/textures/texture-repository.py` stages them with LFS,
`tools/textures/build_textures.py` audits them. Leave pointers unhydrated for unrelated
source work. Optional pack publication must reject missing LFS content. Ordinary
game packages omit textures; existing installed packs and runtime fallback remain.

For installed layouts, use the launcher platform adapter instead of assuming
checkout names. Never include private data, generated guest code, credentials,
environment dumps or personal configuration in durable context. Store paths,
symbols, semantics and sanitized measured conclusions instead.

## Prompt text layout read scopes

`pc_prompt_guest.inc` scans successive chunks through `pc_prompts_read_region`;
`pc_prompts_read_span` validates the complete argument frame before dereferencing
it. The bounded thread-local `pc_text_read_scope.h` cache uses fresh single-page
Windows queries with full-region fallback; Darwin retains its Mach query.
It is valid only during synchronous text layout, with no mapping/protection
changes or event dispatch. Nested scopes retain intervals; the next outer scope
clears them. Scratch writes still use a fresh protection query.
Cached intervals cover only the OS-validated page/region. Descending reads reuse
them only when the requested minimum fits; crossing argument frames query again.
Strings continue at the next validated chunk, preserving cross-page text. No
interval extends beyond its bounds or survives the outer layout scope.
The US outlined-label wrapper (`sub_00041E30`) retains this same scope over
its four outline draws and foreground pass, then expires it at return.
Scope wrapping, label construction/drawing and nested prompt measurement use the shared
hooks in `pc_tutorial_ui.inc`, `pc_display_ui.inc` and `pc_prompt_guest.inc`.
`pc_display_menu.c` includes `pc_prompts.h` before the .inc callers to avoid
implicit declarations. Do not extend caches across menu ticks or frames.
Tests cover region boundaries/expiry and actual prompt-hook scratch ABI/fallbacks.
Issue #105 also requires the runtime late-frame pacing patch; caching alone
measured 30 FPS. See docs/project/STATUS.md for measured acceptance and Mac limits.

## Physical keyboard text entry (#136)

`src/input/pc_text_input.h` defines the shared bounded per-dialog text queue and edit
commands. Windows `pc_controls.c` supplies WM_CHAR/WM_UNICHAR; native Mac
`input_sdl.c` supplies SDL text events and starts/stops text input on its main
thread. Both adapters suppress keyboard menu bindings while a text owner is active
and discard queued text on focus loss or owner changes.

`src/interface/pc_menu_text.inc`, included by `pc_menu_mouse_guest.inc`, recognizes live
character-row callbacks (`0x0007DB20`) rather than one dialog descriptor. Create
Profile embeds this keyboard in descriptor `0x004E8D54`; testing only standalone
`0x004D8D78` misses it. The callback-row scan reuses one writable OS region only within
that synchronous traversal (no guest calls), checking every row's bounds and
querying again on a region change. It expires before dispatch, including nested
menus; no row identity or permission survives a tick. Large non-keyboard menus
therefore avoid a VM query per row. `test_menu_text.py` checks descending links,
query counts, permission expiry, partial/invalid rows and cycles; eager-query and
stale-region mutations must fail. Windows/Mac use their existing VM adapters.
Original insertion/edit/Done routines retain selection replacement, limits and
save behavior. Escape queues the owning menu's
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

## Mounted and CCTV gun mouse aim (#182, #385)

`src/input/pc_mounted.h` handles guest player mode `0x80` before ordinary player
camera updates in both `pc_controls.c` and `native/input_sdl.c`. The US guest
`sub_0025D520`/`sub_00258A20` aim a mounted gun through player entity data `+0xC14`;
ordinary player yaw/pitch do not turn it. The shared helper validates its entity
backpointer and reverse operator link at gun data `+0xD8`, uses local input port zero (not guest player ID),
and updates gun pitch/yaw with authored limits, sensitivity, inversion, FOV and
optional gun speed. Mouse displacement is consumed once without frame-time
multiplication. Invalid ownership, pause, focus loss and stale motion consume
input without falling through to hidden player angles. Terminal mode `0x2000`
and its left-click mapping retain `pc_terminal_mouse` behavior. Controller
input remains in the guest routines. The separate vehicle-seat mode `0x8000`
is not routed by this helper. `tests/gameplay/test_mounted_mouse.py` runs the guarded C
fixture on Windows/macOS; physical Mac validation is tracked under MAC-001.
The mounted helper receives the adapter's explicit native-timing selection.
Native mode accepts finite positive game-float time at `4E794C`; Original60
retains its whole-time gate at `4E7944`. Both consume/rebase mouse batches with
`pc_input_consume_motion`, preventing fresh packets from expiring against a
previous batch timestamp. Paused/invalid native time consumes without aiming.
The helper still handles only mounted-gun mode `0x80`, not vehicle-seat mode `0x8000`.
For #385 field provenance, bounded access and original-code regression coverage,
see [mounted-gun ownership](mounted-mouse.md). The AI target and instance name
must not authorize or exclude player mouse aim.
Host input self-tests route synthetic motion through `pc_controls_apply_mouse`
in Windows and SDL, with zero-whole positive time, paused stale whole time,
live return to Original60 and return to on-foot controls. Windows execution
passes; native SDL self-test execution remains a host validation requirement.

Both adapters now create a local `PcGuestRegion` after input replay/focus gating
and share it through terminal, mounted and ordinary camera `_in_region` helpers.
`pc_guest_memory_in_span` reuses bounds/permissions only during that host mouse
operation, which invokes no guest callbacks or allocation. Every subsequent
update queries afresh. `pc_camera_mouse_region` seeds the snapshot at the lower
of the player and FOV field using their known extents (0x118 and 4 bytes).
Misses use bounded resident-page queries on Windows, or the existing full-region
fallback/Mach query. Only an actual returned span may cover another field;
separate pages/maps get individual checks. Region changes and read-to-write checks retain OS validation;
read access cannot grant writes. Standalone helper entry points create their own
snapshot. This removes duplicate player queries without retaining memory facts
across frames or changing motion, capture, terminal clicks or timing policy.

## Player FOV through Zeppelin air lifts (#339)

`gameplay_fov_scale` in `src/interface/pc_display_menu.c` recognizes grounded (2), falling
(4) and air-lift (0x800) movement. Original US `sub_00225C70` selects 0x800 in
Zeppelin's four lift cylinders and returns to 4 outside them. Both player-camera
callers retain the selected FOV; the projection and CPU frustum scale together.
The scene, camera-owner, story and letterbox guards still apply, and the guest
weapon lens is not rewritten. The shared hook needs no platform adapter.
`test_cutscene_letterbox.py` covers lift/ascent/fall/landing, excluded cameras and
a regression mutation. With the supported local XBE it also executes the original
lift/state routines and all four region records through the production camera
hook at default/90-degree FOV and three aspect ratios. The separate
`test_native_timing.py` player-force fixture checks original exit/landing motion.

## Unified Video Options and native presets (PR #285)

See [Video Options contracts](video-options.md) for shared staging, persistence,
platform adapters and accepted/default frame-rate behavior.

## Issue #74 menu texture additions

The branch pack now contains 2,046 accepted replacements. The same keyed DDS
files serve Windows and the common native Mac renderer; loader contracts are
unchanged. Menu coverage is 574/750 keys, with 164 original character icons, eight
intentional functional originals and four unresolved replacements.
`config/textures/texture-originals.json` records excluded icon keys; the worklist rejects
installed replacements for those keys and hybrid planning retains the original
lane. Character model materials remain in scope. Source resampling and original-detail
recovery are distinguished from newly generated art in `docs/textures/main-menu-textures.md`.
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
uploader's clamped/rounded byte anchors, then scales acceleration/displacement
and exponentiates damping without repacking fractional values. The existing
bias+float+fractional opt-in gates enable it. `pc_native_surface_guest.h` copies
original parameters into the HLE state before temporary guest scaling; whole
steps retain original uploads. The HLE path copies bank 0 stages 3/5/7 and bank
1 stage 6 into ordered commands only for shader 0x4BBF10, caching duplicates and
clearing on scope exit. Defaults and original/catch-up behavior stay unchanged.
Shared rounded conversion and finite-input guards:
[coefficient contracts](surface-coefficients.md). Xbox MXCSR/wave parity is unverified.

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

## Frame-rate preset range

`pc_frame_rate_settings.h` and launcher `GameSettings` expose Original60 plus
120/180/240 Beta. Legacy144 sidecars load as120 without rewriting; direct144
environment timing remains for regression comparison. `pc_native_rate.h` accepts
180/240, and `pc_frame_rate_live.inc` publishes their immutable strings at the
next guest update. Save/apply rollback and the 60-Hz console clock are unchanged.
The overlay already labels every native target above60 Beta and displays actual
measured FPS rather than rounding it to the preset interval.

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
slots, defaulting to 1Ã¢â‚¬â€œ9/0. The separate 70-entry internal table bound covers
weapon IDs and variants; it does not add settings rows.

## Themed live keyboard and mouse editor

Controller selection preserves the launcher's keyboard binding map. Shared
`input_bindings.c` recognizes an explicitly selected pad with keyboard sources
and no pad sources, then merges pad-only defaults from that selected slot at
sampling time. Custom pad mappings (including unbound actions), entirely unbound
profiles and saved keyboard keys remain intact. Focus/modal gating precedes the
fallback; keyboard mode retains its existing first-free-pad sharing policy.

`pc_keyboard_menu.inc` is included by `pc_display_menu.c`, adding Keyboard and
Mouse to both main Settings and paused Options. It uses the original menu
builders, callbacks, highlights and footer navigation. The controller-style
bindings pages draw a keyboard/mouse diagram with primary/alternate columns;
`pc_keyboard_menu_model.h` partitions all 34 actions across short pages.
`PC_BINDING_SLOTS` is two in the shared model and native dialogs; old keyboard
profiles use their first two sources without modifying disk until saved.
Controller profiles keep their existing source capacity.

For gameplay stick axes, the shared binding sampler uses
`pc_input_radial_axis_magnitude` from `pc_input.c` on both left and right
sticks. It subtracts the configured deadzone once from the stick's radius,
then applies one gain to both components. Keyboard WASD still uses the existing
diagonal magnitude cap; stick halves bound to buttons retain their original
per-source threshold. This prevents a physical diagonal from losing speed in
movement or look while preserving custom bindings and both host adapters.

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
frame. The callback fixture in `tests/graphics/fov_settings.c` exercises both native
per-frame callbacks and requires a selectable capture instruction row.

## Host storage choices in Create Profile

See [save-folder selection](save-locations.md): shared routing and persistence,
original guest profile state machine, owner-scoped picker completion, Windows/Mac folder pickers,
launcher copy/backup/restore and validation. `pc_storage.inc` supplies cached
volume metadata for original storage-text replacements. Mac includes unistd.h
before getattrlist; production pc_prompts.c compilation checks this contract.

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
common-RHI fixture reuses basis/scratch resources and reconstructs the field
after each fractional update. The production owner/scheduler is described below.
Restoring coefficients belong to spatial eigenmodes; per-texel matrix powers
cannot replace the spatial transform. Preserve authored disturbances, smoothing
and byte storage. Historical float-only candidates and their rejected storage
comparisons remain in `docs/analysis/native-high-refresh-regressions.md`.
`graphics/surface_state_shader.h` encodes red scratch, green signed velocity plus
0.5, blue height and alpha height. Effect22 samples alpha cardinal differences;
constant alpha erases ripple slopes. The native final-view adapter packs this
height channel too.
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
Original1956A0 dispatches disturbances1953B0, noise1952D0, smoothing1950B0,
feedback195110, then output195280. Noise effect19/2BE30 uses pixel4BBD30 and
vertex4BBBF8: immutable A8L8 noise adds into green (ONE/ONE, RGB writes) before
smoothing. Fixtures retain these shaders at authored boundaries with controlled
UV offsets; queued splash geometry and runtime RNG/event delivery need live
acceptance.
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
requests return NULL, never a stale owner. The opt-in native render hook now uses this registry.
The ten 128-square float4 images contain 2.5 MiB of pixels per owner; this is not
a total VRAM or in-game performance measurement.
`TSFP_SURFACE_GPU_CAPTURE=1` runs the shared kernel on actual GPU views and
writes fractional/full-tick fields to a `.gpu` sidecar. Its per-sample allocation
and readback are diagnostic; they do not replace visible water or alter guest
RNG/events. The persistent gameplay owner is separate. See the coupled-operator
record in `docs/analysis/native-high-refresh-regressions.md` for capture details/limits.

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

Classic now defers new replacement requests in both adapters (`pack_checked=3`).
Binding in Enhanced starts the deferred request; until completion the original
remains visible. Returning to Classic retains cached replacements and permits
already submitted requests to finish. Hash/content invalidation, cancellation
and explicit original dumps still run. Deferred bindings in Classic perform no
replacement I/O, GPU allocation or repeated trace writes. A `deferred` trace
records the initial hash only; late activation has zero hash/source-byte fields.

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

`pc_surface_native.h` schedules the persistent cosmetic field from exact native
clock metadata. It begins from post-event/smoothing byte input on authored updates,
then evolves the remainder and intervening fractional frames. This places the
cosmetic field one authored tick behind the original completed feedback, retaining
original event/RNG ordering. It does not add input latency to gameplay. Duplicate
frames do not advance; missing intervals, changed scopes and new identities wait
for an authored update. Pause preserves phase. Original60 suspends the clocks and invalidates field
readiness while retaining bounded GPU owners; constructor/device reset releases
them. Returning to native waits for an authored boundary before reusing an owner. `pc_surface_native_hle.inc` resolves copied full texture identities,
uses the five-entry retained-image cache, packs alpha-height through
`pc_surface_gpu_pack`, and borrows that view only during the original final shader.
No per-frame readback or queue flush belongs to this path. Older fractional/float/
live experiments are mutually exclusive. Native Mac common-RHI execution remains
unverified and legacy GL uses original output. Bounded Windows Scotland captures at 120/144 produce fresh final images on
partial frames; sustained pacing, broader visual acceptance and Mac remain open.

`pc_surface_gpu_cache_suspend` invalidates every output without freeing images,
shaders or retained identities. The deferred `hle_d3d8_surface_native_suspend`
clears the borrowed view and clocks, avoiding shader/resource recreation across
an Original60 round trip. Original60 still performs no coupled updates. Full
scene/device cleanup remains distinct. The creation log identifies actual
new owners; unchanged identity reuse emits no allocation notice.

Native water defaults on for common-RHI 120/144 beta presets; Original60 and
legacy GL remain original. `pc_surface_native_policy` accepts an explicit
`TSFP_SURFACE_NATIVE_TEST=0` recovery override and excludes old experiments.
The HLE adapter latches allocation/shader failure until constructor/device reset,
so fallback does not retry expensive setup each frame. Cleanup is registered
before first allocation; empty resets do not alter RHI texture bindings.

## Character status-effect model growth

`pc_native_character_guest.h` normalizes only scale-constructor call120BBF:
subdivide the one-raw-tick factor at current game speed. Original rotation,
composition, cleanup and whole/catch-up arithmetic remain intact. The original
helper fixture covers60..240, slow time, pause, jitter and live switches.

`TSFP_CHARACTER_TRACE` selects126029->120AB0: matrices/deltas/identity/removal,
validated mappings, cross-frame/scene rejection,8192rows/8MiB cap. Mapping caches
never span guest execution. Disabled/failed/capped traces keep original callee.
CPU evidence only; rendering/pacing and physical Mac remain unverified.

## Temporal Uplink map display

The shared HLE hook copies the preclear map into the handheld texture. Native
GL uses src/native/gl_screen_copy.inc and preserves framebuffer/scissor state.
Guest geometry and controls remain unchanged.

Temporary UV matrices: [render epoch and arena lifetime](render-matrix-cache.md).

## Forced and damped material scrolling

`pc_native_material.h` splits127500 velocity/travel in raw game time:
12765Bforce,1276FD/127705drag,12773B/127791UV. Weak drag uses stable limits;
Original/whole/catch-up/unsupported bases keep old math. Force/clamp, wrapping,
resources/events, layout and pause stay original. Varying-force/rendered/Mac
parity remain open; see surface/cache tests and STATUS.

## Scripted object clocks

TREE1D88B0 admits fractional time at its gate; EAX is overwritten before use.
Original phase/pose, events, terminal clamp, layout and whole updates remain.
TRAP1D8AB0 now couples fractional kicks/travel and carried-tick contact lifetime.
Original whole/catch-up and pause remain exact. See [script clocks](native-script-clocks.md)
for the map, hook composition, rejected candidates and live/Mac acceptance limits.

Statistics: D2CB7 scopes D1FD0 to carried ticks. Time totals keep Original
cadence; distance stays per-frame. D2079 blocks paused stale ticks.
Impact sparks: see [native spark timing](native-impact-sparks.md).


[Devices](kernel-devices.md).


Project-owned I/O contracts: [sharing](kernel-sharing.md),
[request lifetime](request-lifetime.md), [driver calls](driver-call.md),
[transfer/queues/events/targets](transfer-completion.md),
[symbolic links](symbolic-links.md), [path routing](guest-paths.md) and
[host storage](host-storage.md).

Vehicle gunner mouse uses shared `src/input/pc_mounted.h` for mode `0x400`,
resolving the occupied seat and its gun on every update. Driver/passenger seats
retain their existing route. Vehicle limits and locked states differ from mode
`0x80`; see [mounted and vehicle mouse contracts](mounted-mouse.md). Both Windows
and SDL adapters consume this helper. Exact mission validation remains separate.

Tank keyboard/mouse policy shares `pc_tank.h` and the
1730E0 guest wrapper. See [tank scope and original aim route](mounted-mouse.md).

Kernel: [pool](kernel-pool.md), [time](kernel-time.md), [RTL](kernel-rtl.md), [numbers](guest-numbers.md), [crashes](crash-evidence.md).

Online uses the build interpreter: [paths](multiplayer-ui.md).

Assault objective brackets (US142D10) use a tagged proportional placement.
`PcUiWorldAnchor` takes the complete first outline draw's horizontal midpoint;
all following labels retain that anchor until the next placement boundary.
The common renderer and legacy GL fit uniformly around this projected target,
not screen centre. This preserves both alignment and glyph/box proportions.
Guest flush boundaries isolate markers and restore the parent HUD. The existing
wide/single-player guards preserve 4:3 and split-screen. `test_ui_layout.py`
checks actual viewport scaling, target position, reset and seven display shapes.

Bot corpses: [contracts](systems-details.md#online-bot-corpses).

Physics: [poses](systems-details.md#guest-ragdolls-and-physical-npc-drops).
