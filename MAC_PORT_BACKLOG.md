# PC features awaiting Mac adaptation

### MAC-026 — Spirit skull and eye rendering (#17)

- **Status:** Awaiting validation
- **PC behavior:** The US spirit renderer uses clockwise culling for rigid
  skull nodes while keeping skinned trails two sided. Its two eye-billboard
  batches use unbiased clip-space positions and no fog, preserving their
  red texture and depth testing. Overrides are restored after each node/batch.
- **Source:** Spirit rendering commit `70d8e7f` on
  `17-ghosts-in-mansion-of-madness-and-what-lies-below-are-nonvisible`;
  `src/pc_ghost_render_guest.inc`, the `spirit-rendering.patch` at the end of
  the shared patch stack, and `tests/ghost_rendering.c`.
- **Shared Mac support:** Guest wrappers and HLE render-state forwarding are
  shared with Windows. No shader hash/string rewriting, geometry fingerprint,
  Windows-only API, or diagnostic switch is required. PR #84 CI run
  `39008025033` passes the macOS ARM64 source-only spirit and patch-transition
  tests after harness clock fix `57324bc`; native gameplay remains unverified.
- **Remaining:** Build on macOS and verify GPU and interpreted-vertex paths.
  No native execution was available on this Windows host. Confirm the native
  unwritten-fog fallback observes the scoped fog-disable state.
- **Acceptance:** Check Mansion entrance-hall spirits from several angles;
  preserve solid faces, wispy trails, red eyes and wall occlusion. Check
  ordinary particles/fog immediately afterward and other ghost encounters in
  both affected levels. Run `tests/test_ghost_rendering.py` and patch-transition
  suites. See `STATUS.md` for measured Windows evidence and remaining limits.

### MAC-025 — Reusable unlocked testing profile

- **Status:** Queued
- **PC behavior:** Local Test Lab fixture unlocks story/Arcade/characters and
  challenges without changing fresh collectible/pickup progress. Disposable
  copies use isolated configurations and the existing startup-video switch.
- **Source:** Commit `0c69834`, [PR #80](https://github.com/TheDoffman/timesplitters-future-perfect-pc/pull/80)
  on `test-profile-fast-start`; `tools/testing_profile.py`,
  `tests/test_testing_profile.py`, `docs/testing-profile.md` and `AGENTS.md`.
- **Shared Mac support:** The US save layout and runtime HMAC key are shared;
  fixture generation/preparation uses portable Python. Native execution unverified.
- **Remaining:** Integrate fixture cloning and `RECOMP_XMV_PLAY=0` with the bounded
  native runner. The new play/Recycle Bin wrapper is Windows only. No gameplay or
  existing native launcher behavior changed; no Mac adapter was modified.
- **Acceptance:** Load the profile in native menus, verify late story levels,
  Arcade character selection and challenge availability, retain fresh pickup
  data, isolate both display configs/controls and recycle only test disks.
  Windows evidence is recorded in `STATUS.md`; other-device sync is unverified.

This is the handoff list for Windows PC features to bring to native Mac later.
Add or update an entry when implementing a new PC feature, before reporting the
PC work complete. Also record behavioral fixes that need Mac adaptation. Mac
work is deferred until requested; it does not block completion of the PC feature.

Keep one stable ID per feature (`MAC-001`, `MAC-002`, and so on). Update existing
entries rather than duplicating them, and retain completed entries as history.
Include commit/PR references when available; label local changes uncommitted
until those references exist. Keep paths relative to the repository.

## Audit scope and next work

Audited on 2026-10-15 against local `mac-port` commit
`93dc422139bae4acb64d1e24c528cf3869a4ce20`. All PC source references below refer to
that snapshot, not a claim about newer Windows-device work. `external/ts3FPrecomp`
references identify the pinned upstream `d90c929` plus the shared local patches;
keep future changes reproducible through that patch stack. The other computer
was inaccessible, so its revision, features and synchronization are unverified.

The initial audit was a source/documentation comparison. The subsequent Mac implementation and bounded tests are recorded in
[Mac backlog renderer, camera and prompts](STATUS.md#mac-backlog-renderer-camera-and-prompts-2026-10-15).
Windows evidence remains historical where linked; implementation alone does not
establish fidelity. Earlier Mac evidence remains in
[Native Mac settings and graphics adapters](STATUS.md#native-mac-settings-and-graphics-adapters-2026-10-15).

Next work: finish the remaining camera/prompt and resolution acceptance checks,
then reproduce the reported world disappearance with a short controlled route.
Four-stage 2D rendering improved reviewed first-mission captures, but does not
establish the cause of every missing-world symptom. Prioritize remaining
MAC-005/006/007 paths from scene evidence, and movies. Debug tools and binding
capture are implemented with remaining physical-input and live-window validation
below. MAC-011 needs the actual texture pack on this device; packaging follows
runtime readiness. No full campaign playthrough is required.

## Queue

| ID | Feature | Status |
| --- | --- | --- |
| MAC-025 | Reusable unlocked testing profile | Queued |
| MAC-001 | Direct-camera mouse look | Awaiting validation |
| MAC-002 | Keyboard/mouse prompts and device switching | Awaiting validation |
| MAC-003 | Debug settings and on-screen performance information | Awaiting validation |
| MAC-004 | Capture-to-bind and conflict reassignment | Awaiting validation |
| MAC-005 | Multiple texture stages and shader fidelity | In progress |
| MAC-006 | Cube and volume texture resources | Queued |
| MAC-007 | Screen-copy effects and image surfaces | In progress |
| MAC-008 | Scissor clipping | Verified |
| MAC-009 | Intro and in-game movie playback | Queued |
| MAC-010 | In-game resolution preview and rollback | Awaiting validation |
| MAC-011 | Real texture-pack gameplay coverage | Awaiting validation |
| MAC-012 | Standalone Mac runtime distribution | Queued |
| MAC-013 | System Link and Online multiplayer | Queued |
| MAC-014 | Patch upgrades and branch-switch transitions | Awaiting validation |
| MAC-016 | Shared pickup-range comparison fix (issue #6) | Awaiting validation |
| MAC-018 | Story audio timing, queue retry and ADPCM resume (issue #8) | Awaiting validation |
| MAC-019 | Framebuffer refresh between effects (issue #9) | Awaiting validation |
| MAC-021 | Published-release launcher and pre-game settings | Queued |
| MAC-022 | Configurable gameplay FOV (issue #29) | Awaiting validation |
| MAC-023 | Story Objectives health/shield (issue #11) | Awaiting validation |
| MAC-024 | Resolution catalog and proportional UI (issues #27/#31) | Awaiting validation |
| MAC-026 | Character picker and weapon proportions (issue #22) | Awaiting validation |
| MAC-027 | Arcade RANK widget left placement | Awaiting validation |

### MAC-027 — Arcade RANK widget left placement

- **Status:** Awaiting validation
- **PC behavior:** In widescreen single-player Arcade, anchor the complete RANK
  widget (caption, ordinal and frame) to the left inset, matching the status icons
  below it. Flush preceding/following sprite batches at the placement boundary.
  Preserve 4:3, narrower outputs and the existing split-screen layout.
- **Source:** Commit `3e747a5`, [PR #83](https://github.com/TheDoffman/timesplitters-future-perfect-pc/pull/83), on
  `22-character-selection-screen-is-stretched-when-using-higher-resolutions`;
  `src/pc_display_ui.inc`,
  `patches/zzzzzzzzzzzzzzzzzzzzzzz-arcade-rank-anchor.patch`,
  `tests/ui_layout.c` and `tests/test_ui_layout.py`.
- **Shared Mac support:** The existing native LEFT placement adapter and shared
  widget scope handle this hook under `TSFP_NATIVE_DISPLAY`; no new adapter is
  required. Native build/gameplay validation remains pending.
- **Remaining:** Validate Arcade RANK alignment in the native renderer and keep
  the label, number and frame together during rank changes. Other-device
  synchronization is unverified.
- **Acceptance:** Compare 4:3, 16:9 and ultrawide with the original left inset,
  status icon alignment and proportional size; check changing ranks and confirm
  split-screen and unrelated HUD groups keep their existing placement. See
  [Windows validation](STATUS.md#arcade-rank-widget-placement-2026-10-17).

### MAC-026 — Character picker and weapon proportions (issue #22)

- **Status:** Awaiting validation
- **PC behavior:** Correct the horizontal aperture of newly built character
  picker and first-person weapon projections for the selected output aspect.
  Keep each lens, zoom, depth and the original 4:3 behavior; do not reuse or
  repeatedly scale the preceding character's projection during model changes.
- **Source:** Commit `6485069`, [PR #83](https://github.com/TheDoffman/timesplitters-future-perfect-pc/pull/83), on
  `22-character-selection-screen-is-stretched-when-using-higher-resolutions`;
  `src/pc_display_menu.c`, `src/pc_display_menu.h`,
  `patches/zzzzzzzzzzz-model-projection.patch`, `tests/test_decal_projection.py`.
- **Shared Mac support:** The perspective hook is already enabled by
  `TSFP_NATIVE_DISPLAY`; this correction uses the shared aspect model. Both
  platform defines pass the source wrapper tests. No new OS/graphics adapter is
  required; native execution and visual fidelity are unverified.
- **Remaining:** Build and visually validate on Apple Silicon. Other-device
  synchronization is unverified; Windows captures do not establish Mac support.
- **Acceptance:** Cycle character models at 4:3, 16:9 and ultrawide without
  changing their proportions during loading; verify first-person weapons,
  aiming/zoom, split-screen, and unchanged world/decal/cinematic projections.
  See [Windows evidence](STATUS.md#issue-22-model-projections-2026-10-16).

### MAC-001 — Direct-camera mouse look

- **Issue #7 follow-up (2026-10-16):** `src/pc_terminal.h`
  on `7-breaking-and-entering-camera-turret-not-working` routes relative mouse
  counts to the active terminal's logical pointer in
  mode `0x2000`, preserving its 4–251 bounds and normal aiming on exit.
  Left-click emits Confirm only in terminal menus; release is required when
  entering/leaving with a held button. Shared context is in `src/pc_input.c`
  and the sampling hook in `patches/zzzzzzzzzzzzzzzzz-terminal-mouse-select.patch`.
  Both `src/pc_controls.c` and `src/native/input_sdl.c` call this shared helper;
  no additional Mac adapter is required. `tests/test_terminal_mouse.py` is
  included in Windows/macOS CI. Windows build and physical terminal checks
  passed, including user-confirmed left-click selection and return to ordinary
  aiming/firing; results are recorded in [Issue #7 validation](STATUS.md#terminal-mouse-routing-issue-7--2026-10-16).
  Native Mac compilation and physical-pointer validation remain pending:
  check terminal selection/clicks, scripted/paused states and ordinary aiming
  after exit using isolated saves/settings.

- **Status:** Awaiting validation
- **Issue #18 follow-up (2026-10-16, commit 8dee97f):** `src/pc_camera.h` no longer
  requires guest player ID zero. Both Windows `src/pc_controls.c` and native
  `src/native/input_sdl.c` already call this shared helper, so no additional
  Mac adapter is required. Windows regression `tests/pc_controls.c` reproduces
  the former rejection for ID 1 and checks fields for IDs 0–15; Release build
  and build-script checks passed. Mac compilation/execution remains pending.
  Acceptance: verify the native camera adapter with a nonzero guest ID as well
  as ID zero, retaining FOV, focus/capture and single-consumption behavior.
  Native multiplayer still depends on MAC-013; Windows checks do not establish
  native multiplayer support. [Measured scope](STATUS.md#nonzero-player-mouse-look-issue-18--2026-10-16).
- **PC behavior:** PC mouse counts update camera angles directly, with zoom/FOV scaling and no controller deadzone or turn-speed curve.
- **PC source:** Audited revision above; `src/pc_controls.c`, `src/pc_input.c`, `patches/pc-controls.patch`, `tests/pc_controls.c`.
- **PC validation:** Input regression and deterministic mission replay, including zoom scaling, are recorded in [Windows validation](STATUS.md#validation); physical mouse fidelity remains a separate check.
- **Existing Mac support:** SDL relative counts now feed the shared `pc_input_look` camera model through the guest wrapper. Controller right-stick input stays independent; configured sensitivity, inversion and FOV scaling are shared with Windows.
- **Mac adaptation needed:** Implementation is present. Finish live gameplay checks across pause/settings/focus transitions and physical mouse use. Native adapter tests now cover equal counts batched or applied per event, both pitch limits and stale motion suppression.
- **Mac acceptance checks:** Equal counts give equal angles across polling rates; zoom scaling, inversion and pitch limits work; pause/settings/focus changes cause no jump or unwanted shot. Use isolated settings and a short mission check.
- **Mac implementation and evidence:** Implementation included with this backlog revision on `mac-port`: `src/native/input_sdl.c`, `src/pc_camera.h`, `src/pc_guest_memory.h`, `src/pc_controls.c`, `patches/zzzzzzzzzzzzz-native-camera.patch`. Native fixtures passed sensitivity, inversion, FOV, capture/release, focus and no-stick-injection checks. A bounded first-mission run logged 8/-2 counts changing yaw/pitch by -1.6/+0.4 degrees at sensitivity 0.2 and FOV 55. These are synthetic SDL events, not physical-device validation. Further native adapter checks for batched/per-event counts, pitch limits and stale motion passed; see [capture and camera validation](STATUS.md#mac-binding-capture-and-camera-checks-2026-10-15). [Validation and evidence](STATUS.md#mac-backlog-renderer-camera-and-prompts-2026-10-15).

### MAC-002 — Keyboard/mouse prompts and device switching

- **Status:** Awaiting validation
- **PC behavior:** Formatted menu/HUD hints show current bindings, [Unbound] or [Mouse], and switch back to controller icons on deliberate controller activity.
- **PC source:** Audited revision above; `src/pc_prompts.c`, `src/pc_prompt_guest.inc`, `patches/pc-prompts.patch`, `tests/pc_prompts.c`.
- **PC validation:** Windows wiring and regression coverage are present; no new Windows execution was performed for this audit.
- **Existing Mac support:** The shared formatter and guest text wrappers now compile on Mac. SDL keyboard/mouse and player-1 controller activity update the shared prompt model. Menu captures show current keyboard labels; saved binding changes update formatter output.
- **Mac adaptation needed:** Finish representative live gameplay hint checks and physical keyboard/controller switching. Original artwork and literal tutorial text are unchanged; a first-mission tutorial still says START, which is not a formatted prompt token.
- **Mac acceptance checks:** Menu and gameplay hints reflect saved bindings immediately; meaningful controller input switches icons; releases, held buttons and drift do not cause flicker. Preserve original artwork-based diagrams.
- **Mac implementation and evidence:** Implementation included with this backlog revision on `mac-port`: `patches/zzzzzzzzzzzz-native-prompts.patch`, `src/pc_prompt_guest.inc`, `src/pc_guest_memory.h`, `src/native/input_sdl.c`, `src/native/prompt_selftest.inc`, `src/native/settings_selftest.inc`. Passed custom/unbound/mouse labels, deliberate pad activity, held/release/drift suppression, SDL virtual-pad integration, live saved binding and guarded guest ABI tests. Reviewed main/paused Display menus show [E] Select and [F] Back. [Validation and evidence](STATUS.md#mac-backlog-renderer-camera-and-prompts-2026-10-15).

### MAC-003 — Debug settings and on-screen performance information

- **Status:** Awaiting validation
- **PC behavior:** F12 opens debug settings; F9 toggles FPS. Options show guest/shown FPS, frame intervals, issue counters and recent logs, with bounded optional performance logging.
- **PC source:** Audited revision above; `src/pc_debug.c`, `src/pc_debug.h`, `patches/pc-debug.patch`, `tests/pc_debug.c`, `tests/pc_debug_game.py`.
- **PC validation:** Windows implementation and test scripts exist; current Windows runtime behavior was not retested during this audit.
- **Existing Mac support:** F9 toggles a host-only OpenGL text overlay; F12 and the Game Settings menu open a Cocoa debug panel. Both use the shared statistics, options, configuration and diagnostic classifier. POSIX log reading handles truncation/replacement and runs on the main thread.
- **Mac adaptation needed:** Implementation is present. Finish visual Cocoa layout, physical F9/F12 and live fullscreen panel/focus checks. A synthetic shortcut smoke run does not establish physical event delivery.
- **Mac acceptance checks:** Toggle in windowed/fullscreen modes; distinguish guest and interpolated presents; save/cancel and issue-counter reset work; log polling stays off the render thread; preserve unrelated JSON.
- **Mac implementation and evidence:** Uncommitted on `mac-port` over `e83e074`: shared `src/pc_debug.c`, native `debug_macos.inc`, `gl_debug.inc`, SDL/renderer hooks and `patches/zzzzzzzzzzzzzz-native-debug.patch`. Native GPU text/state/isolation tests, real Cocoa Save/Cancel, configuration preservation/failure, log rotation/reset and synthetic F9 repeat/release checks pass. [Measured validation](STATUS.md#mac-debug-and-performance-tools-2026-10-15).

### MAC-004 — Capture-to-bind and conflict reassignment

- **Status:** Awaiting validation
- **PC behavior:** The Windows editor captures a pressed key, mouse button or wheel direction and offers to move a conflicting binding from another action.
- **PC source:** Audited revision above; `src/pc_settings.c`, `src/pc_bindings.inc`, `tests/pc_settings.c`, `tests/pc_settings_game.py`.
- **PC validation:** Capture, conflict reassignment, Cancel and live persistence were checked in [PC controls menu](STATUS.md#pc-controls-menu-2026-10-15).
- **Existing Mac support:** Each of the four Cocoa binding slots now has a Record action. Capture accepts keyboard/modifier keys, five mouse buttons and wheel directions; Escape cancels, with separate Use Escape and Unbind buttons. Explicit conflict confirmation moves the binding using the same assignment/reserved-key rules as Windows. Punctuation and keypad keys now work through the SDL mapping; unsupported saved keys remain visible and preserved.
- **Mac adaptation needed:** Finish visual/editor interaction checks and physical keyboard/mouse capture on Mac, including non-US layouts. Native responder and modal-dialog tests passed; they do not establish physical macOS event delivery.
- **Mac acceptance checks:** Capture keyboard, five mouse buttons and wheel directions without leaking gameplay input; test conflict move/cancel, reserved keys, save/reopen and controls from another device using isolated files.
- **Mac implementation and evidence:** Implementation included with this backlog revision on `mac-port`, tested against `30653e3`: `src/native/binding_capture.inc`, `src/native/settings_macos.m`, `src/native/input_sdl.c`, `src/native/settings_selftest.inc`, and shared `src/pc_bindings.inc`, `src/pc_settings.c`, `src/pc_settings.h`. Cocoa responder/modal, shared move/cancel, save/reload, live punctuation/keypad input, backup and failure tests pass. [Validation](STATUS.md#mac-binding-capture-and-camera-checks-2026-10-15).

### MAC-005 — Multiple texture stages and shader fidelity

- **Status:** In progress
- **PC behavior:** The Windows renderer evaluates multiple texture inputs and Xbox texture/combiner modes for layered materials and effects.
- **PC source:** Audited revision above; `external/ts3FPrecomp/src/d3d/d3d8_combiners.c`; native counterparts `src/native/gl_fragment.inc`, `src/native/gl_vertex.inc`, `src/native/gl_bridge.inc`.
- **PC validation:** Windows shader implementation exists; scene-by-scene material fidelity is not established by this audit.
- **Existing Mac support:** Four independent 2D samplers and T0-T3 coordinates now reach the fixed vertex, GPU program and CPU fallback paths. Ordinary/projected 2D, linear coordinate scaling, passthrough and absent stages are implemented, with per-stage wrap/filter state. High-bit FVF coordinate-size flags are no longer mistaken for programmable shader handles.
- **Mac adaptation needed:** Implement remaining dependent/dot-product and other Xbox texture modes, plus cube/volume integration with MAC-006. Continue matched scene/material checks and frame-time profiling; the 120-present setting is a target, not a demonstrated sustained rate.
- **Mac acceptance checks:** Use distinguishable multi-stage GPU fixtures plus matched mission captures; verify fallback agreement, alpha/depth behavior and interpolation. Reproduce the reported disappearing world separately before attributing its cause.
- **Mac implementation and evidence:** Implementation included with this backlog revision on `mac-port`: `src/native/renderer_gl.c`, `gl_fragment.inc`, `gl_vertex.inc`, `gl_bridge.inc`, `multitexture_selftest.inc`, `vertex_selftest.inc` (all latter paths under `src/native/`), and `patches/zzzzzzzzzzz-shared-linear-formats.patch`. Real-GPU four-stage fixtures passed distinct coordinates, projected/linear inputs and FVF/GPU/CPU agreement. Reviewed first-mission captures show improved terrain, weapons and NPC surfaces. Complete fidelity and the reported world-disappearance case remain unverified. [Validation and evidence](STATUS.md#mac-backlog-renderer-camera-and-prompts-2026-10-15).

### MAC-006 — Cube and volume texture resources

- **Status:** Queued
- **PC behavior:** Windows has resource factories and sampling paths for cube/volume textures.
- **PC source:** Audited revision above; `external/ts3FPrecomp/src/d3d/d3d8_device.c`, `external/ts3FPrecomp/src/d3d/d3d8_combiners.c`; `src/native/renderer_gl.c`, `src/native/gl_bridge.inc`.
- **PC validation:** Factories are implemented on the Windows path; usage and fidelity in particular game scenes need targeted evidence.
- **Existing Mac support:** Native cube/volume creation returns `E_NOTIMPL`; cube metadata/level queries return false.
- **Mac adaptation needed:** Add resource lifetime, face/slice/mip uploads and sampler integration. Coordinate with MAC-005; establish actual title usage before prioritizing expensive format work.
- **Mac acceptance checks:** Synthetic face/slice/mip checks and resource release pass on the GPU; validate a game scene if one uses these resources. Unsupported formats must fail explicitly.
- **Mac implementation and evidence:** Remaining work pending. Existing coverage:
  [native adapter validation](STATUS.md#native-mac-settings-and-graphics-adapters-2026-10-15).

### MAC-007 — Screen-copy effects and image surfaces

- **Status:** In progress
- **PC behavior:** Windows can copy the full backbuffer or a rectangle into a texture for later draws.
- **Issue #21 channel-clear follow-up (this revision):** The lobby uses destination
  alpha as a temporary text mask. `src/pc_clear_rhi.inc` and
  `patches/zzzzzzzzzzzzzzzzz-channel-clears.patch` preserve partial colour clears,
  including clearing alpha without erasing the finished RGB image. Capture
  version 8 records the channel flags and still accepts older captures.
  `tests/pc_clear.c` checks all 15 masks on the Windows GPU; portable mapping
  and capture tests are in `tests/test_channel_clears.py`. The shared mapping
  also feeds Mac, so `src/native/renderer_gl.c` now applies the corresponding
  GL colour mask and restores the existing render state. No additional native
  adapter is needed; native build/GPU validation is pending. Acceptance:
  repeatedly cycle lobby information pages at default and enlarged resolutions;
  verify readable text without white fill or stale page masks, unchanged
  backgrounds, and preserved clipping, depth/stencil and offscreen targets.
  [Windows validation and remaining checks](STATUS.md#issue-21-lobby-text-masks-2026-10-16).
- **PC source:** Audited revision above; `external/ts3FPrecomp/src/d3d/d3d8_screencopy.c`, `external/ts3FPrecomp/src/d3d/d3d8_device.c`; `src/native/renderer_gl.c`.
- **PC validation:** Windows copy implementation exists; not every effect has a recorded visual comparison.
- **Existing Mac support:** Full and rectangular current-scene copies now support 32-bit ARGB/XRGB and their linear formats. Copies preserve clipping, offsets, orientation and framebuffer state, including enlarged scenes. Image surfaces support validated full/subrectangle locks, BGRA writes and read-only locks.
- **Mac adaptation needed:** Establish required title formats and extend explicitly unsupported ones where used. Validate a specific in-game effect that exercises copies; successful GPU fixtures alone do not establish every effect.
- **Mac acceptance checks:** GPU tests verify full and partial copy pixels, offsets and bounds at original/1080p sizes; review a title effect using the path and confirm no stale frame, black drawable or feedback loop.
- **Mac implementation and evidence:** Implementation included with this backlog revision on `mac-port`: `src/native/gl_screen_copy.inc`, `src/native/renderer_gl.c`, `src/native/screen_copy_selftest.inc`. GPU tests passed full/partial copies, negative destination clipping, preserved surrounding pixels and GL state at original/1080p targets; image-surface write/read, double-lock and invalid-rectangle tests passed. Other formats return E_NOTIMPL. [Validation and evidence](STATUS.md#mac-backlog-renderer-camera-and-prompts-2026-10-15).

### MAC-008 — Scissor clipping

- **Status:** Verified
- **PC behavior:** Windows applies the supported guest scissor rectangle to restrict drawing.
- **PC source:** Audited revision above; `external/ts3FPrecomp/src/d3d/d3d8_device.c`, `external/ts3FPrecomp/src/d3d/d3d8_states.c`; `src/native/renderer_gl.c`.
- **PC validation:** Windows applies its supported rectangle; its own source notes limits on multiple/exclusive rectangles, so do not assume complete Xbox parity.
- **Existing Mac support:** The first inclusive guest scissor rectangle is applied with GL Y conversion, target scaling and the existing HUD transform. Disable restores the full draw region; clears and screen-copy operations preserve the drawing state.
- **Mac adaptation needed:** None for the audited Windows first-inclusive-rectangle contract. Multiple rectangles and exclusive clipping remain unsupported, matching the documented Windows limitation; they are not claimed as full Xbox parity.
- **Mac acceptance checks:** GPU fixtures prove pixels outside the scissor remain unchanged at original and enlarged resolutions; cover disable/reset and viewport changes, and document unsupported rectangle modes.
- **Mac implementation and evidence:** Implementation included with this backlog revision on `mac-port`: `src/native/renderer_gl.c`, `src/native/screen_copy_selftest.inc`. Real-GPU fixtures passed inside/outside pixels, disable and clear behavior at original and 1920x1080 targets. Existing offscreen, interpolation, depth and drawable regressions also passed. Verification is limited to the supported rectangle contract. [Validation and evidence](STATUS.md#mac-backlog-renderer-camera-and-prompts-2026-10-15).

### MAC-009 — Intro and in-game movie playback

- **Status:** Queued
- **PC behavior:** Windows uses FFmpeg to decode XMV movies, draws their frames and submits movie PCM audio.
- **PC source:** Audited revision above; `external/ts3FPrecomp/src/d3d/d3d8_movie.c`, `tools/fetch-video.ps1`, `tools/build-native-mac.py`, `src/native/gl_bridge.inc`.
- **PC validation:** [Windows validation](STATUS.md#validation) records EA/Free Radical intro decoding and PCM submissions; this is not exhaustive movie/audio fidelity.
- **Existing Mac support:** Native movie draw/clear hooks are empty; native bring-up skipped intros. SDL game audio output already exists.
- **Mac adaptation needed:** Integrate host-built movie dependencies using shared version policy, native decoded-frame presentation and synchronized audio, with cleanup and skip handling.
- **Mac acceptance checks:** Bounded isolated runs show intro picture and audible synchronized sound, allow skip, and reach menus; test an in-game movie transition without leaking buffers or overwriting saves.
- **Mac implementation and evidence:** Remaining work pending. Existing coverage:
  [native adapter validation](STATUS.md#native-mac-settings-and-graphics-adapters-2026-10-15).

### MAC-010 — In-game resolution preview and rollback

- **Issue #28 release follow-up:** Windows game v0.2.0-alpha.4 and launcher
  v1.2.0 use primary-display modes, the console default, lower modes down to
  640×480, refresh-rate deduplication and preservation of saved custom modes.
  `src/pc_display_menu.c` uses the game's native arrow choice widget. Sources:
  `src/pc_display.c`, `tools/launcher/DisplayModes.cs`, `SettingsPage.cs`;
  implementation `e3e99c1`, integrated on `release/resolution-v0.2.0-alpha.4`.
  SDL discovery otherwise retains its existing behavior. Mac mode discovery,
  native arrow spacing, lower modes and Apply/Keep/Revert/timeout still need
  native verification; Windows results do not verify Mac support.

- **Status:** Awaiting validation
- **PC behavior:** Display settings offers Apply/Keep/Revert with a 15-second automatic rollback, persisted resolution and widescreen camera/HUD handling.
- **Issue #19 decal projection follow-up (`19-floating-decals`):** `src/pc_display_menu.c`,
  `src/pc_display_menu.h` and `patches/zzzzzzzzzz-decal-projection.patch` make
  both depth-biased decal passes use the active camera's horizontal and vertical
  aperture, preserving their depth bias. This is shared guest code; the existing
  Mac build includes the same manual wrappers and display source, so no native
  adapter is required. Mac rendering remains unverified. Acceptance: in Arcade,
  shoot a wall and strafe/look around at default, 4:3, 16:9 and ultrawide modes;
  impacts must stay attached and HUD proportions must remain unchanged. Also
  check cinematic camera zoom and split-screen cameras with isolated settings.
- **PC source:** Audited revision above; `src/pc_display.c`, `src/pc_display_menu.c`, `src/pc_display_renderer.inc`, `tests/pc_display_game.py`, `tests/pc_display_gameplay.py`.
- **PC validation:** [Display settings and texture preview](STATUS.md#display-settings-and-texture-preview--october-15) records Windows apply/save, timeout rollback, 4K and ultrawide render-target checks.
- **Existing Mac support:** Mac shares the menu/model and has working resolution targets, Cocoa save/reload and fullscreen. The in-game timeout interaction was not exercised end-to-end.
- **Mac adaptation needed:** Finish Keep/save/reopen, explicit Revert and representative aspect/HUD checks in the actual menu. Automatic timeout and return to first-mission gameplay have now been exercised.
- **Mac acceptance checks:** From actual game menus: apply, keep, reopen, revert and let a preview expire; test paused Options and return to gameplay. Confirm saved mode and render targets agree, using isolated configs.
- **Mac implementation and evidence:** Existing implementation unchanged. In the final bounded run, paused Options opened Display Settings, previewed 2560x1440, displayed the countdown and automatically restored 1920x1080 after 15 seconds. The isolated saved mode remained 1920x1080, and the run returned to visible first-mission gameplay. Apply/Keep/reopen and explicit Revert remain pending. [Validation and evidence](STATUS.md#mac-backlog-renderer-camera-and-prompts-2026-10-15).

### MAC-011 — Real texture-pack gameplay coverage

- **Status:** Awaiting validation
- **PC behavior:** Windows loads compiled DDS replacements with matching keys/mips and bounded residency.
- **Windows build fix (2026-10-15):** Uncommitted on `mac-port`,
  `src/texture_pack.c` restores the local `FILE *f` declaration used by
  `dump_original`. MSVC syntax compilation passes; no Mac adapter change is
  needed because the native renderer uses `src/native/gl_texture_pack.inc`.
  Existing Mac texture coverage remains awaiting validation as described below.
- **PC source:** Audited revision above; `src/texture_pack.c`, `src/texture_pack_format.h`, `tools/texture-pack.py`, `tools/play-textures.ps1`; `src/native/gl_texture_pack.inc`.
- **PC validation:** [Display settings and texture preview](STATUS.md#display-settings-and-texture-preview--october-15) records four of eleven replacements loaded in the first mission; seven still need scene-specific checks even on Windows.
- **Existing Mac support:** Native replacement loading and synthetic real-GPU sampling already pass; the launcher accepts `--texture-pack`. Actual installed-art coverage has not been established on Mac.
- **Mac adaptation needed:** The real `private/texture-remaster/pack` directory is absent on this Mac (checked 2026-10-15); the user confirmed the textures have not yet been transferred. Obtain its existing location/copy before testing matching scenes and mapping replacement keys to captures. Do not recreate the loader or label all artwork verified.
- **Mac acceptance checks:** Compare original/replacement mission captures, confirm expected key hits, mip behavior, eviction/fallback and the 512 MiB residency limit. Keep the 6 GiB pack budget and isolated saves; no duplicate extract.
- **Mac implementation and evidence:** Synthetic DDS/GPU replacement and corrupt-fallback regressions passed again in this batch. Actual installed-art coverage remains pending because the real pack is unavailable on this device. [Validation and evidence](STATUS.md#mac-backlog-renderer-camera-and-prompts-2026-10-15).

### MAC-012 — Standalone Mac runtime distribution

- **Release metadata follow-up (2026-10-16, branch `fix/game-release-history-titles`):**
  `tools/publish-release.py` compares changes with the latest published game
  tag, excluding launcher releases, and creates short `TSFP v…` titles.
  `docs/releasing.md` specifies matching `Launcher v…` titles. No Mac adapter
  is needed for simplifying notes to PR/contributor changes and installation
  instructions, without redundant headings, source lists or build sections,
  or for GitHub metadata selection; the shared Python regression suite
  covers the behavior. macOS execution remains unverified.

- **Windows CI harness follow-up (PR #65, 2026-10-16):**
  `tests/launcher_settings.cs` gives cold WPF UI initialization a bounded
  60-second worker budget and preserves named assertion/timeout diagnostics.
  No Mac adaptation is needed: this is the Windows-only launcher test harness,
  not shared runtime behavior. Hosted Mac patch checks passed on the preceding
  PR revision; native Mac functionality is not inferred from that result.
  [Validation](STATUS.md#launcher-settings-ci-startup-budget--2026-10-16).

- **Release-test compiler discovery follow-up (2026-10-16, branch
  `fix-release-msvc-tests`):** `tests/native_compiler.py` supplies one compiler
  environment to audio, framebuffer, cutscene, and lifted-float regressions.
  Windows discovers standalone Build Tools as well as Visual Studio and loads
  x64 headers/libraries without a developer shell. The POSIX path still uses
  `CC` or `cc` and inherits its environment. No Mac adapter is needed for this
  Windows discovery fix; simulated POSIX checks are separate from pending
  execution on macOS. Acceptance: run `tests/test_native_compiler.py` and the
  affected native regressions on Mac. See
  [compiler validation](STATUS.md#release-regression-compiler-discovery--2026-10-16).

- **Status:** Queued
- **PC behavior:** Windows has a public runtime package with launcher, dependencies, licenses, checksums/build manifest and game-import workflow.
- **PC source:** Audited revision above; `tools/package-release.py`, `tools/release/Import-game.ps1`, `tools/release/ImportGame.cs`, `tests/release_import.py`; `tools/play-native-mac.py`.
- **PC validation:** Windows packaging/import tooling exists; this audit did not assemble or validate a new release.
- **Existing Mac support:** Mac has a checkout-based interactive launcher and `BUILD.json`, but the release packager targets Windows executables/DLLs.
- **Mac adaptation needed:** Add a relocatable native runtime package and supported local-data import/check path, with host dependencies, licenses and accurate provenance. Plan signing/distribution requirements when this work is requested.
- **Mac acceptance checks:** Launch from a clean path without compiler/build tree, verify architecture/dependencies and hashes, reject unsupported XBE, preserve existing saves/settings, and exclude game data/personal files from public artifacts.
- **Mac implementation and evidence:** Remaining work pending. Existing coverage:
  [native adapter validation](STATUS.md#native-mac-settings-and-graphics-adapters-2026-10-15).

## Shared uncertainties, not confirmed Mac-only omissions

Campaign completion, checkpoint/save-load round trips, physical-controller play,
multiplayer/online, vehicle/MapMaker controls and exhaustive audio/rendering
fidelity lack complete evidence across the project. They should not be described
as proven PC features missing only on Mac. Track focused validation in `STATUS.md`.
The reported world disappearance remains unresolved until reproduced and checked;
none of the renderer entries above is a confirmed diagnosis of that symptom.

### MAC-013 — System Link and Online multiplayer

- **Status:** Queued
- **Temporary diagnostics experiment (uncommitted, 2026-10-16):** Windows
  `Play-Quiet.cmd` / `TSFP_DEBUG_QUIET=1` routes stdout/stderr to `NUL` and skips
  Debug log polling and performance-log writes, retaining FPS measurement and
  saved options. Sources: `src/pc_debug_output.h`, `src/pc_debug.c`,
  `tools/play.py`, `patches/zzzzzzzzzzzzzzzzz-pc-debug-quiet.patch`,
  `tests/pc_debug.c`. Native defaults are unchanged. Mac adaptation is queued:
  native startup would need equivalent stream suppression; shared Debug checks
  alone do not verify a full native quiet run. Acceptance: isolated normal/quiet
  comparison with unchanged personal settings, no quiet diagnostic writes, FPS
  still measured, and reporting restored on a normal launch.
- **Issue #20 follow-up (2026-10-16, uncommitted):** Active Windows PC matches
  open the movement-send gate once per guest tick instead of waiting seven
  ticks. A subsequent candidate bounds full movement-state refresh to three
  match ticks (20 Hz), including unchanged held keys and mouse-only turning.
  This retains the original packet encoder; it does not remove host frame
  stalls. The existing network epoch and simulation/heartbeat/timeout clocks
  are preserved. Sources: `src/pc_network_guest.inc`,
  `patches/zzzzzzzzzzzzzzz-pc-network-movement.patch`,
  `patches/zzzzzzzzzzzzzzzz-pc-network-state-refresh.patch`,
  `tests/network_guest_dispatch.c`, `tests/test_patch_updates.py` on
  `20-player-desync-when-strafing-multiplayer` over `c8f8b31`.
  The native fallback still delegates to the original guest pump; portable
  boundary tests exercise it on Windows, not on a Mac host. No native transport
  is added here. When porting the transport, reuse this guest send policy,
  regenerate/rebuild the wrapper, and verify rapid remote strafe reversals in
  both directions, mouse-only turns and recovery from bounded frame stalls,
  with unchanged 60 Hz guest and timeout clocks. Native
  execution and other-device synchronization remain unverified.
  [Measured Windows scope](STATUS.md#multiplayer-strafe-updates-issue-20--2026-10-16).
- **PC behavior:** System Link uses the native UDP/XNet bridge; Online offers
  host/join discovery and routes compatible peers through the directory bridge.
- **PC source:** Main through `16fcea5` (PRs #4 and #5);
  `src/pc_network.c`, `src/pc_network_guest.inc`, `src/pc_online.c`,
  `src/pc_display_menu.c`, `patches/zz-pc-network.patch`,
  `patches/zzz-pc-online.patch`, and `tools/online*.py`.
- **PC validation:** Main records local two-instance lobby/match loading in
  `docs/multiplayer.md`; cross-internet routing/full-match validation is pending.
- **Existing Mac support:** The shared menu/display and original guest XNet
  paths are retained. Windows host UI/transport calls are guarded during the
  main/mac-port conflict resolution; no native Mac multiplayer adapter is linked.
- **Mac adaptation needed:** Native socket/identity transport, directory host/join
  UI, launch/cleanup integration, and platform-appropriate dependencies.
- **Mac acceptance checks:** Build and launch on Apple Silicon; verify existing
  display/settings menus, then test isolated two-instance discovery, joining,
  gameplay, shutdown and user-setting preservation before cross-network tests.
- **Mac implementation and evidence:** Deferred. Conflict resolution preserves
  the Mac build boundary; host-independent dispatch and Windows checks are
  recorded in `STATUS.md`. Native Mac execution remains unverified for this merge.

### MAC-016 — Shared pickup-range comparison fix (issue #6)

- **Status:** Awaiting validation
- **Issue #24 follow-up (2026-10-16, this revision):** The same existing
  `patches/recomp-float-joins.patch` from `837692d` also fixes the controller
  sensitivity slider's shared clamp at guest address `0x002EABEB`.
  `tests/controller_sensitivity.c` and `tests/test_recomp_float_joins.py` execute
  the original slider callback with its XBE constants, covering 0.05 steps,
  endpoints, no input and guest register preservation on all four profiles.
  A test restoring the older generator behavior reproduces the endpoint jump.
  No new platform adapter or runtime patch is needed. On Mac, regenerate/rebuild
  and run this shared regression with Clang, then check the slider in the menu
  with isolated settings. Native validation and other-device synchronization
  remain pending. [Windows evidence](STATUS.md#controller-sensitivity-slider-issue-24--2026-10-16).
- **PC behavior:** Restore the original horizontal-distance rejection for pickups
  by preserving SSE comparison snapshots across control-flow joins. The old
  translation ignored this rejection; pickup radii and input bindings are unchanged.
- **PC source:** Focused `fix-pickup-range` change (this commit): `patches/recomp-float-joins.patch`,
  `tools/recomp-generation.py`, `tsfp.ps1`, `tests/test_recomp_float_joins.py`,
  `tests/pickup_collision.c`.
- **PC validation:** The original US pickup tick reproduces the out-of-range grant
  with the pinned translator and passes isolated range/height/ABI cases with the
  patch. Build and gameplay evidence is recorded in
  [STATUS.md](STATUS.md#pickup-range-comparison-fix--october-16-2026).
- **Existing Mac support:** The same patch stack and generator are used by both
  builds. `tools/build-native-mac.py` now rejects stale generator hashes and records
  them after `--generate`.
- **Mac adaptation needed:** No platform adapter is required for this math fix.
  Transfer the patch through Git, regenerate the game C and rebuild on Apple Silicon.
- **Mac acceptance checks:** Run the shared regression with the supported US XBE
  and Clang; verify near/far pickup behavior while turning and strafing in a bounded
  isolated native gameplay run. Body-collision symptoms need separate confirmation.
- **Mac implementation and evidence:** Shared implementation is present; native
  compilation/gameplay and cross-device synchronization remain unverified.

## Status meanings

- **Queued:** PC feature recorded; Mac adaptation is pending.
- **In progress:** Mac implementation is underway.
- **Awaiting validation:** Implementation is present but relevant Mac checks remain.
- **Verified:** Mac acceptance checks passed, with evidence linked below.
- **Not applicable:** No corresponding Mac capability is needed; explain why.

If shared code already supplies the feature, record that fact and use awaiting
validation until Mac checks pass. Do not infer Mac support from Windows results.

### MAC-014 — Patch upgrades across Windows watermark and native overlay guards

- **CI efficiency follow-up (branch `ci/faster-runtime-checks`):**
  `.github/workflows/runtime-patches.yml` and `tools/ci-runtime-scope.py` select
  affected suites for PRs and main pushes; manual, patch/pin and workflow changes
  retain full Windows/macOS coverage. `tests/test_patch_updates.py` shares only
  immutable pristine upstream bytes, with independent writable checkouts for
  every transition scenario. No native Mac adapter is needed for CI scheduling
  or fixture construction. `tools/apply-patches.py` eliminates a redundant dry-run
  process per staged patch, preserving full-stack validation before runtime
  writes; shared multi-file conflict/rollback tests cover this change.
  macOS execution of this revision remains unverified.
  Acceptance: full transition suite and affected scope tests pass on both hosted
  platforms. See [CI validation](STATUS.md#runtime-ci-efficiency).

- **Status:** Awaiting validation
- **PC behavior:** Release preparation accepts the complete shared patch stack
  over fresh sources or an existing Windows watermark build, without losing
  unrelated runtime edits. Conflicting code still fails before runtime writes.
  Branch switches can now unwind the recognized texture-toggle patch when
  building a branch that does not enable it.
  Automatic tracking stores the applied stack in runtime Git metadata so
  removed, renamed and edited patches migrate without a manual legacy archive.
- **PC source:** Original watermark fix is integrated in `main`. Branch-switch
  repair is commit `979a16c`. Automatic tracking is on
  `fix/automatic-patch-tracking` (this commit), based on `979a16c`:
  `tools/apply-patches.py`, `tests/test_patch_tracking.py`, `tsfp.ps1`, and
  `.github/workflows/runtime-patches.yml`. The legacy archive remains a
  migration aid for runtimes without a record.
- **PC validation:** Eleven patch-loader tests passed on Windows, plus fresh and
  existing-Windows fixtures using the complete main patch stack; repeated
  application made no changes. See `STATUS.md`, 2026-10-15 patch preparation.
  Branch-switch repair: 14 patch-loader tests and the complete Windows Release
  build/native checks passed; see the October 16 runtime branch-switch record.
  Automatic tracking: 27 patch checks and 16 shared-tool checks passed, with
  one Windows symlink-privilege skip. Compilation and remaining native checks
  passed; the existing foreground-window prompt assertion failed twice.
  See the automatic tracking entry in `STATUS.md` for the scope and evidence.
- **Existing Mac support:** The same Python patch loader serves both hosts.
- **Mac adaptation needed:** No native adapter changes; validate the shared
  loader on macOS. Legacy patches are never enabled by their archive location.
- **Mac acceptance checks:** Run patch-loader tests and apply/reapply the
  complete merged stack on disposable sources, preserving actual code conflicts.
  Include texture-to-main-to-texture transitions and unrelated-edit preservation.
  Cross-platform CI now covers source-only transitions, record corruption,
  changed/removed patches and rollback. Native Mac build validation is separate.
- **Mac implementation and evidence:** Shared fix present locally; macOS checks
  and other-device synchronization remain unverified.

### MAC-015 — Release test privileges and disposable cleanup

- **Release follow-up:** `tools/publish-release.py` recognizes only pytest's
  `*current` sibling symlinks in `build/release-tests`, validates all targets
  before unlinking, and retains the normal recursive-cleanup guards. Native
  fixtures cover containment and target preservation. This affects Windows
  publication; a Mac publisher still needs its own native adapter/validation.

- **Status:** Awaiting validation
- **PC behavior:** Native-tool symlink tests skip only Windows privilege error
  1314; other errors still fail. Release test/staging cleanup removes validated
  disposable directories without shell Recycle Bin support, preserves archives,
  and does not mask the original test failure if cleanup also fails.
- **PC source:** Uncommitted on `fix/release-cleanup` over `2ef7536`:
  `tests/test_native_mac_tools.py`, `tests/test_publish_release.py`,
  `tools/recycle-release-work.ps1`, `tools/publish-release.py`.
- **PC validation:** 22 passed, three skipped; see `STATUS.md` release cleanup.
- **Existing Mac support:** Shared symlink assertions remain enabled on macOS;
  native Mac cleanup is unchanged.
- **Mac adaptation needed:** No PowerShell adapter needed for native Mac.
  Validate the shared Python test change on macOS before marking verified.
- **Mac acceptance checks:** Run native-tool Python tests with real symlinks;
  cleanup and capture must still reject redirects and preserve target files.
- **Mac implementation and evidence:** macOS checks and other-device
  synchronization remain unverified.

### MAC-018 — Story audio timing, queue retry and ADPCM resume (issue #8)

- **Status:** Awaiting validation
- **PC behavior:** Stream packets complete against the device's reported
  position even after an underrun; submission, flush and pause preserve the
  correct timeline. Refused buffer PCM is retried, and ADPCM resume preserves
  partial-block offsets without stepping beyond packet data. Stream volume also
  includes XDK headroom attenuation, previously omitted for stereo streams.
  Final stream release retires the host queue; immediate and zero-time async
  FlushEx return packets correctly during story/level transitions.
- **PC source:** Audio fix prepared on `fix/issue-8-audio-timing`, published through
  the user-requested `8-audio-issues` branch; original validation base is main
  `4189924`: `patches/dsound-audio-timing.patch`,
  `patches/dsound-stream-lifecycle.patch`, `tools/recomp-generation.py`,
  `tests/test_recomp_generation.py`, `tests/audio_stream_timing.c`,
  `tests/audio_buffer_timing.c`, `tests/test_audio_timing.py`, `tsfp.ps1`, and
  `.github/workflows/runtime-patches.yml`.
- **PC validation:** Eight of thirteen deterministic scenarios fail on the
  original pinned runtime; all thirteen pass after the fix. A further headroom
  regression reproduced the omitted attenuation and passes with its correction.
  The first fourteen checks and full headroom Build passed, but the user
  still heard distortion. Live capture then isolated corrupted stale music
  packets; three additional lifecycle regressions failed before the new hooks
  and passed afterward. All 45 current regression checks and the full Windows
  Build pass. The user confirmed the first-mission opening sounds clean; eight
  captured music packets had valid headers and no clipped decoded samples.
  Rebuild results and measured probe evidence,
  including limitations, is recorded in `STATUS.md` under issue #8.
- **Existing Mac support:** Both adapters consume the patched shared DirectSound
  code. Buffer retry and ADPCM copying apply to Mac. SDL currently reports no
  hardware position, so its existing wall-clock fallback remains in use.
- **Mac adaptation needed:** Validate shared changes on macOS; device-driven
  stream completion additionally needs trustworthy SDL playback-position
  accounting. Native adapter work is deferred; no audio API was changed here.
- **Mac acceptance checks:** Run source-only audio and patch suites, validate
  pause/resume, repeated level starts, starvation and device loss with isolated
  settings, and check audible story dialogue/music and A/V synchronization.
- **Mac implementation and evidence:** Shared fixes are present locally; native
  Mac compilation/listening and other-device synchronization remain unverified.

### MAC-019 — Framebuffer refresh between effects (issue #9)

- **Status:** Awaiting validation
- **PC behavior:** Khallos Express's final glow samples the scene including
  the train and weapon, instead of overlaying an earlier background-only copy.
  Consecutive effect taps retain a stable snapshot; rebinding after other
  textures refreshes it within the same frame.
- **PC source:** `patches/zzzzzzz-framebuffer-sampling-passes.patch`, modifying
  shared `src/hle/hle_d3d8_texture.c`; `tests/framebuffer_sampling.c`,
  `tests/test_framebuffer_sampling.py`, `tests/test_patch_updates.py`,
  `tsfp.ps1` and `.github/workflows/runtime-patches.yml`. Delivered on branch
  `9-khallos-express-level-is-transparent`, source base `800687d`.
- **PC validation:** Windows build/native checks and 29 patch/cache checks pass.
  A mutation check detects the stale-copy bug; the user confirmed solid walls
  after moving/turning in Khallos Express. Evidence and limits are in
  [STATUS.md](STATUS.md#khallos-express-see-through-scenery-issue-9--2026-10-16).
- **Existing Mac support:** Native OpenGL already implements screen copying in
  `src/native/gl_screen_copy.inc`; both hosts consume this shared HLE cache.
  No separate platform implementation is expected for the refresh rule.
- **Remaining adaptation/acceptance:** Build on macOS and run the new native
  cache regression, GPU screen-copy checks and an isolated Khallos Express
  scene at 60 and interpolated 120 FPS. Check solid walls/weapon, windows and
  glow, plus a first-mission effects smoke test. Native rendering and other-device
  synchronization are unverified; Windows results do not establish Mac support.

### MAC-020 — Adaptive cutscene letterboxing (issue #33)

- **Status:** Awaiting validation
- **PC behavior:** Story and scripted in-game cutscene bars adapt to the selected
  display aspect ratio. The original 4:3 cinematic aperture is the reference;
  wider displays have thinner bars, with side bars when wider than that aperture.
  The cinematic camera and its CPU culling adapt together to retain composition
  and avoid revealing outside authored backdrops. Animated entry/exit, fades,
  subtitles and scope overlays remain.
- **PC source:** Branch `33-remove-the-letter-boxes-in-cut-scenes`, based
  on `5c9effd`: `src/pc_cutscene.h`, `src/pc_display_menu.c`,
  `patches/zzzzzzzzzzzzzzz-cutscene-letterbox.patch`,
  `tests/cutscene_letterbox.c`, `tests/test_cutscene_letterbox.py`.
- **PC validation:** Full Windows Release build/native checks, 27 patch transition
  checks and four letterbox tests passed. Original US cinematic routines execute
  in disposable memory and compare bars, other rectangles, subtitle calls and
  guest ABI across resolutions and animation phases. Mutations detect missing
  bar and camera corrections. The user confirmed the opening space scene looks
  correct in an isolated 1280x720 run. Live ultrawide/other scenes remain unverified;
  see [STATUS.md](STATUS.md#adaptive-cutscene-letterboxing-issue-33--2026-10-16).
- **Existing Mac support:** The same manual guest wrapper is enabled by
  `TSFP_NATIVE_DISPLAY`; both hosts use the shared display mode and bar-height
  helper. Original guest coordinates still pass through each renderer's normal
  scaling. No OpenGL-specific implementation is required by the sizing rule.
- **Remaining adaptation/acceptance:** Run the source-only regression and native
  build on macOS, then check story and scripted cutscenes at 4:3, 16:9, 16:10 and
  ultrawide output, including resolution changes, fades, subtitles, scopes and
  60/120 FPS. Mac compilation, GPU execution and other-device sync are unverified.

## Entry template

Copy this template into the queue for each new entry. Replace all placeholders.

### MAC-021 — Published-release launcher and pre-game settings

- **Status:** Queued
- **Preserve launcher artwork (2026-10-16, commit `f1011f8`,
  [PR #75](https://github.com/TheDoffman/timesplitters-future-perfect-pc/pull/75),
  prepared for launcher v1.2.7):** `Core.cs` preserves verified independent
  launcher binaries/manifest when a game package bundles the same launcher
  version; damaged equal versions may repair, and newer versions still install.
  `build-launcher.ps1 -RequireArtwork` and `package-release.py` reject missing
  release artwork while allowing developer/CI fallbacks. Native Mac has no
  corresponding game-package launcher updater; adapt version/manifest ownership
  checks in its future release adapter, then test equal/newer/damaged versions,
  rollback, required artwork and preserved user data with disposable fixtures.
  Existing shared runtime is unchanged; Mac execution remains unverified. See
  [Windows evidence](STATUS.md#launcher-artwork-lost-after-game-update-2026-10-16).
- **Launcher v1.2.6 release compatibility (2026-10-16,
  [PR #73](https://github.com/TheDoffman/timesplitters-future-perfect-pc/pull/73) on
  `release/launcher-v1.2.6`):** `GameSettings.cs` and `SettingsPage.cs` filter
  resolution presets and reject custom heights above 2160 for packaged games
  through alpha.4 before writing settings. Newer/unpackaged games keep 8K modes.
  Source copy cleanup is merged in PR #71. Mac has no release-launcher adapter;
  reuse installed-version capability filtering in that future adapter and verify
  legacy/new game bounds, atomic rejection and preserved settings with disposable
  fixtures. Existing native Mac runtime is unchanged; Mac execution is unverified.
- **Neutral interface copy (2026-10-16, commit `b9a05b9` on
  `cleanup/readme-launcher-copy`):** README and Windows launcher slogans/quips
  are removed. `Launcher.xaml`, `Launcher.cs`, `Bootstrap.cs` and `SettingsPage.cs`
  use functional labels/status messages, including Home/Game/Updates. The README
  screenshot and launcher usage guide match the labels. No shared game logic or
  native Mac adapter changed, so no Mac runtime adaptation is needed for this
  copy-only edit. The future Mac release launcher should use the same neutral
  wording; existing native Mac UI copy is not audited or verified here. Evidence:
  [Windows copy validation](STATUS.md#readme-and-launcher-copy-cleanup-2026-10-16).
- **Game version cleanup (2026-10-16, commit `d1f4e42` on
  `fix/game-update-cleanup`, [PR #69](https://github.com/TheDoffman/timesplitters-future-perfect-pc/pull/69),
  prepared for launcher v1.2.5):** `tools/launcher/Core.cs` removes retired game
  backups and obsolete/incomplete game staging after a window was observed and
  the game exited normally, then all managed runtime hashes verified. Confirmation
  is tied to the BUILD.json hash; later startup retries cannot confirm a new
  version. `Launcher.cs` uses the existing window observation/exit path, with
  background cleanup and busy-state protection. Updates transactionally remove
  unchanged files owned by an old manifest but absent from the incoming package.
  Modified/untracked files, personal settings, independent launchers, saves,
  extracted data, texture packs and ISOs are protected. ISO files inside managed
  cleanup folders prevent deletion. Retired snapshots are renamed before removal
  to avoid offering a partial rollback. `tests/launcher_updates.cs` covers these
  boundaries, retries, junctions and restoration. Mac has no corresponding
  published-release launcher; implementation remains queued. Reuse these retention
  and ownership rules in its future adapter, then verify POSIX deletion/locking,
  manifest validation, clean-exit observation and protected data with isolated
  fixtures. See [Windows game cleanup evidence](STATUS.md#game-version-cleanup-2026-10-16).
- **Launcher update cleanup (2026-10-16, commit `dc019bf` on
  `fix/launcher-update-cleanup`, [PR #68](https://github.com/TheDoffman/timesplitters-future-perfect-pc/pull/68),
  prepared for v1.2.4):** `tools/launcher/SelfUpdate.cs` removes the
  prior launcher backup, obsolete/incomplete staging and unused helper EXEs
  after the normal main window loads and the installed file hashes verify.
  `Launcher.cs` invokes cleanup under the installation mutex; render previews
  do not invoke it. Failed preparation also cleans its own staging. Active
  transactions, newer/unknown pending releases, linked paths, game rollback and
  personal files are protected; locked files retry on the next startup.
  `tests/launcher_updates.cs` covers repeated upgrades, deferred cleanup,
  preservation, junction refusal and preparation failures. Mac has no equivalent
  published-release launcher implementation; adaptation remains queued. When
  implementing it, retain these success/recovery boundaries and test POSIX
  executable replacement/deletion independently. Windows results do not verify
  Mac integration. See [cleanup validation](STATUS.md#launcher-update-cleanup-2026-10-16).
- **Startup argument handling (2026-10-16, release preparation on
  `fix/launcher-bootstrap-arguments`):** A resolution-preview command passed an
  application-only flag to the startup EXE, producing `Unknown bootstrap
  arguments`. The Windows v1.2.3 local fix in `tools/launcher/Bootstrap.cs`
  delegates the three application preview flags, preserves output paths/exit
  status, and logs bounded escaped startup arguments. Tests are in
  `tests/launcher_updates.cs`; normal startup and updater command routes remain
  separate. These WPF commands have no existing native Mac counterpart. When
  adapting MAC-021, test native argument routing, safe preview isolation and
  diagnostics independently; Windows results do not verify Mac integration.
  Evidence: [Bootstrap diagnostics](STATUS.md#bootstrap-argument-diagnostics-2026-10-16).
- **Embedded settings (2026-10-16, commit `5eea0ca` on `feat/launcher-inline-settings`):**
  Windows launcher v1.2.2 replaces the separate settings window with an
  embedded `SettingsPage` in `Launcher.xaml`/`Launcher.cs`. Both tabs, binding
  capture and confirmations keep the sidebar visible. Home navigation keeps
  drafts; Cancel discards them; Save keeps the page open. Build references and
  isolated WPF checks are in `tools/build-launcher.ps1` and
  `tests/launcher_settings.cs`. Native Mac pre-game UI remains queued; its
  existing in-game Cocoa settings are unchanged. Reuse shared configuration
  semantics when adapting navigation, focus, inline capture/confirmation and
  save/cancel; verify sidebar visibility and resize/keyboard accessibility on
  Mac. Windows evidence: [Embedded launcher settings](STATUS.md#embedded-launcher-settings-2026-10-16).
- **Startup skipping (2026-10-16, commit `bad108c` on `feat/launcher-skip-startup`):**
  Windows launcher v1.2.1 adds an off-by-default Startup checkbox under Graphics
  & resolution. It preserves `launcher.skip_startup_splashes` in the existing
  controls JSON and explicitly sets `RECOMP_XMV_PLAY=0/1` for each game launch;
  opening logos and the idle attract movie are skipped, while rendered story
  cutscenes use a separate system. Sources: `tools/launcher/GameSettings.cs`,
  `tools/launcher/SettingsPage.cs` (formerly `SettingsWindow.cs`),
  `tools/playable-launch.ps1`.
  Existing Mac HLE has no host XMV player, so this does not establish Mac movie
  playback or a new native checkbox. When adding Mac movie playback/pre-game UI,
  honor the same saved boolean and switch, preserve unknown JSON members, and
  verify enabled/disabled launches with isolated paths. Windows evidence:
  [Launcher startup skipping](STATUS.md#launcher-startup-skipping-2026-10-16).
- **PC behavior:** Launcher v1.2.0 retains verified release-only game updates,
  independent self-updates, import, rollback and minimize/restore; it adds
  pre-game bindings, graphics, resolution and fullscreen editing with backups,
  external-edit protection and transactional saves. The banner, blue/silver theme
  and icon remain embedded. Release source: `tools/launcher`, `tools/build-launcher.ps1`,
  `tools/package-release.py`, branch `release/resolution-v0.2.0-alpha.4`.
- **Mac support/adaptation:** Existing in-game Cocoa settings remain separate.
  Add native release discovery, import, update/rollback and pre-game UI adapters;
  reuse shared binding/display/config rules. Do not ship Windows binaries on Mac.
- **Acceptance:** Safe archive paths/checksums, stable/preview channels, failed
  update recovery, isolated settings save/cancel, conflicts, file preservation,
  primary display modes, and actual runtime consumption of all selected settings.
  Windows results do not verify Mac adapters or rendering.

### MAC-NNN — Feature name

- **Status:** Queued
- **PC behavior:** What the user can do and how it should behave.
- **PC source:** Branch, commit/PR (or uncommitted), and relevant source/patch paths.
- **PC validation:** Checks performed and any remaining Windows gaps.
- **Existing Mac support:** Shared behavior already available, or unknown.
- **Mac adaptation needed:** Adapter, build, UI, rendering or configuration work;
  include dependencies and settings compatibility where relevant.
- **Mac acceptance checks:** Concrete behavior to verify with isolated tests.
- **Mac implementation and evidence:** Commit/files, host checks and a link to
  the relevant `STATUS.md` validation record; pending until performed.

### MAC-022 — Configurable gameplay FOV (issue #29)

- **Issue #66 follow-up (2026-10-16, commit `df3bc2e`,
  [PR #72](https://github.com/TheDoffman/timesplitters-future-perfect-pc/pull/72)):**
  `66-bug-fov-returns-to-default-when-falling-from-a-ledge` updates
  `src/pc_display_menu.c` to keep the selected FOV in falling state 4 as well
  as grounded state 2. The shared camera hook needs no Mac-specific adapter.
  `tests/cutscene_letterbox.c` covers both call sites through a fall/landing,
  default/90-degree FOV and excluded cameras. Windows regressions and build
  checks passed; the user confirmed stable falling/landing FOV on Windows.
  Native Mac compilation and ledge/landing playtesting remain pending.
  Check that FOV stays steady while falling, zoom remains relative,
  and cutscene/terminal lenses are unchanged. See
  [Issue #66 evidence](STATUS.md#falling-gameplay-fov-issue-66--2026-10-16).

- **PC behavior:** Vertical FOV slider with numeric value, 40–100°, default/reset
  55°, in the guest Display settings menu and Windows launcher graphics settings.
  Camera projection and CPU frustum change together; weapon zoom remains relative.
  Story/letterboxed cameras, terminals and menu framing retain their authored lens.
- **Source:** issue #29 implementation on `29-fov-support-in-video-settings`, based on main
  `ef17e2a`; `src/pc_display.c`, `src/pc_display.h`, `src/pc_display_menu.c`,
  `tools/launcher/GameSettings.cs`, `tools/launcher/SettingsPage.cs`.
- **Shared Mac support:** the existing shared camera/menu hooks and config model
  use the same behavior. Config accepts legacy V1 (55°) and V2 with explicit FOV;
  existing Cocoa resolution saves retain the loaded FOV through the shared writer.
  No native Mac execution has been performed for this change.
- **Remaining:** validate native compilation, menu slider drawing/input, gameplay
  and zoom on Mac. Adding a slider to the separate Cocoa settings window is
  deferred; Windows launcher UI does not constitute a Mac launcher implementation.
- **Acceptance:** default framing unchanged; 40/55/75/100° at 4:3, 16:9 and
  ultrawide; zoom retains relative magnification; cinematics/terminals unchanged;
  configuration round trips and resolution timeout reverts both choices. See
  [Windows FOV evidence](STATUS.md#configurable-gameplay-fov-issue-29--2026-10-16).

Launcher v1.2.3 release compatibility (MAC-021/MAC-022): `GameSettings.cs` and
`SettingsPage.cs` disable custom FOV for packaged game versions through alpha.4
to retain V1 display compatibility. Windows model/UI regression coverage is in
`tests/launcher_settings.cs`; Mac launcher capability gating remains queued.

### MAC-023 — Story Objectives health/shield (issue #11)

- **Status:** Awaiting validation
- **PC behavior:** Story quick Objectives (Tab/Back) displays the original
  health/shield indicators while retaining quick-menu input and pause behavior.
  Indicators automatically fit the current viewport with a 4% edge inset at
  every output resolution, including split-screen. The Objectives panel uses
  the same fit ratio for its width and text measurement to reserve clearance
  from the indicators; original font size and wrapping remain in use.
- **Source:** [PR #55](https://github.com/TheDoffman/timesplitters-future-perfect-pc/pull/55),
  branch `11-objectives-tab-not-showing-health-and-shield` (issue #11);
  `patches/zzzzzzzzzzzzzzzzz-story-objectives-health.patch`,
  `src/pc_objectives.h`, `tests/test_objectives_health.py`,
  `tests/objectives_health.c`, `tests/objectives_layout.c`.
- **Mac support:** Shared guest wrapper with no operating-system or renderer
  API dependency; no additional adapter is expected. Native Mac compilation and
  gameplay are not yet verified.
- **Remaining checks:** Apply the full stack, run the regression on native Mac,
  then check Story quick Objectives at full/partial health and shield, normal
  pause-menu entry/exit, controller Back and keyboard input, live resolution
  changes, portrait/ultrawide displays, edge insets, panel-border clearance,
  long/multiple objectives with wrapping/scrolling, and split-screen.
  Other game modes must keep their existing menu behavior.
- **PC validation:** Final Windows build and native checks passed; seven
  focused regressions passed. The user confirmed the viewport fit and corrected
  panel spacing on 2026-10-16. Live coverage of all display shapes remains
  unverified; shared numerical tests cover those dimensions.
- **Evidence:** [Windows scope](STATUS.md#story-objectives-health-and-shield-issue-11--2026-10-16).

### MAC-024 — Resolution catalog and proportional UI (issues #27/#31)

- **Status:** Awaiting validation
- **PC behavior:** Common render presets and six aspect groups, up to 7680×4320,
  shared by the guest menu and Windows launcher. Full-canvas menu backgrounds,
  proportional widgets, and single-player edge placement for ammo, radar, status
  and zoom controls. Health/armor curves fit their full mesh inside the viewport,
  with separate left/right anchors and a centred damage-direction marker. These
  title hooks and sizing rules are shared; Mac playtesting remains pending.
  Tutorial cards measure assigned key labels, reserve a padded shared column,
  grow within the viewport and wrap descriptions without overlap. Single-player
  tutorial text/glyphs use 90% size before measurement; other HUD/menu font scales
  are restored afterward. Controller columns and split-screen layout are preserved. These hooks use shared
  font/prompt logic; native compile and gameplay validation are pending.
  Full-canvas textured shading effects now span the Windows output instead of
  leaving a centred 4:3 tint rectangle. The `fullscreen-effects` runtime patch
  uses shared `pc_ui_full_canvas_quad` geometry classification, preserves clipped
  panels, and corrects the original vertex count used for fan/quad placement.
  Native OpenGL already allows full-width AUTO draws to span the output; no new
  adapter change is required for this captured pass, but visual validation remains.
  Mission date/title and tutorial cards also use scoped left-edge placement on
  wide single-player displays, preserving their original insets, animations and
  proportions. This uses shared title hooks (`story-caption` patch and
  `src/pc_tutorial_ui.inc`) and the existing native LEFT adapter; Mac visual
  checks remain pending.
  Preview, timeout rollback and saved custom modes remain. The subsequent
  `objectives-ui-integration` patch combines MAC-023 sizing with side placement
  in one health renderer wrapper, preserving its split-screen and panel fit.
- **Source:** Feature commit `2d69459` on
  `27-add-aspect-ratio-169-and-more-resolutions`, integrated with main `9e08fa3`.
  `src/pc_display*`, `src/pc_ui_layout.h`, the `ui-layout` and
  `ui-narrow`/`tutorial-layout` runtime patches, `src/pc_tutorial_ui.inc`, and
  `tools/launcher/DisplayModes.cs`/`SettingsPage.cs`.
- **Shared Mac support:** The catalog, config bounds, guest picker and title HUD
  hooks are shared. Native OpenGL uses the shared side-anchor rule and fits narrow
  widgets vertically, including their scissor rectangles. Cocoa consumes the same
  capacity and catalog without duplicating Original. Native execution is unverified.
- **Remaining:** Build and validate on Apple Silicon; add the new aspect filter
  to the separate Cocoa settings window when adapting its UI. The Windows launcher
  is not a Mac launcher implementation. Existing split-screen layout is preserved.
- **Acceptance:** 4:3, 5:4, 16:9, 16:10, 21:9 and 32:9 menu captures with readable,
  unclipped, proportional text; full backgrounds; ammo/radar corners; centred
  crosshairs and scope aim; zoom controls aligned; pause/return; no leaked placement
  between batches; original/custom settings, save/cancel and timeout rollback.
  Check the first-mission canyon shading for a rectangular tint boundary at wide
  ratios, then check the weapon tutorial with default and longer rebound keys,
  wrapped descriptions, and keyboard/controller switching. Verify supported GPU
  sizes and allocation failure recovery. See issue #27/#31 in
  `STATUS.md` for Windows evidence; do not infer Mac runtime verification from it.
