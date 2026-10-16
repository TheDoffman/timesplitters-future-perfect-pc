# PC features awaiting Mac adaptation

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

### MAC-001 — Direct-camera mouse look

- **Status:** Awaiting validation
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
- **Mac implementation and evidence:** Published on `mac-port` as `149939f`: shared `src/pc_debug.c`, native `debug_macos.inc`, `gl_debug.inc`, SDL/renderer hooks and `patches/zzzzzzzzzzzzzz-native-debug.patch`. Native GPU text/state/isolation tests, real Cocoa Save/Cancel, configuration preservation/failure, log rotation/reset and synthetic F9 repeat/release checks pass. [Measured validation](STATUS.md#mac-debug-and-performance-tools-2026-10-15).

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
- **Existing Mac support:** Four independent 2D samplers and T0-T3 coordinates now reach the fixed vertex, GPU program and CPU fallback paths. Ordinary/projected 2D, linear coordinate scaling, passthrough and absent stages are implemented, with per-stage wrap/filter state. High-bit FVF coordinate-size flags are no longer mistaken for programmable shader handles. Dependent AR/GB lookups and DOTPRODUCT/DOT_ST now evaluate in stage order, with source selection, signed byte mappings and linear target scaling.
- **Mac adaptation needed:** Implement remaining cube/volume/reflection modes with MAC-006, depth-writing DOT_ZW, bump, clip/BRDF and other unsupported Xbox modes. The signed mappings match the PC renderer; HILO retains its unsigned fallback. Continue matched scene/material checks and frame-time profiling; the 120-present setting is a target, not a demonstrated sustained rate.
- **Mac acceptance checks:** Use distinguishable multi-stage GPU fixtures plus matched mission captures; verify fallback agreement, alpha/depth behavior and interpolation. Reproduce the reported disappearing world separately before attributing its cause.
- **Mac implementation and evidence:** Implementation included with this backlog revision on `mac-port`: `src/native/renderer_gl.c`, `gl_fragment.inc`, `gl_vertex.inc`, `gl_bridge.inc`, `multitexture_selftest.inc`, `vertex_selftest.inc` (all latter paths under `src/native/`), and `patches/zzzzzzzzzzz-shared-linear-formats.patch`. Real-GPU four-stage fixtures passed distinct coordinates, projected/linear inputs and FVF/GPU/CPU agreement. Reviewed first-mission captures show improved terrain, weapons and NPC surfaces. Additional uncommitted work over `149939f` adds dependent/dot 2D modes in `gl_fragment.inc`, updates diagnostics in `renderer_gl.c` and adds `dependent_texture_selftest.inc`. Thirty-six real-GPU cases cover AR/GB, chained/source selection, invalid source/stage handling, dot mappings and linear DOT_ST across fixed/GPU/CPU vertex paths; all pass. Complete fidelity and the reported world-disappearance case remain unverified. See [dependent texture validation](STATUS.md#mac-dependent-texture-modes-2026-10-15). [Validation and evidence](STATUS.md#mac-backlog-renderer-camera-and-prompts-2026-10-15).

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

- **Status:** Awaiting validation
- **PC behavior:** Display settings offers Apply/Keep/Revert with a 15-second automatic rollback, persisted resolution and widescreen camera/HUD handling.
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

## Status meanings

- **Queued:** PC feature recorded; Mac adaptation is pending.
- **In progress:** Mac implementation is underway.
- **Awaiting validation:** Implementation is present but relevant Mac checks remain.
- **Verified:** Mac acceptance checks passed, with evidence linked below.
- **Not applicable:** No corresponding Mac capability is needed; explain why.

If shared code already supplies the feature, record that fact and use awaiting
validation until Mac checks pass. Do not infer Mac support from Windows results.

### MAC-014 — Patch upgrades across Windows watermark and native overlay guards

- **Status:** Awaiting validation
- **PC behavior:** Release preparation accepts the complete shared patch stack
  over fresh sources or an existing Windows watermark build, without losing
  unrelated runtime edits. Conflicting code still fails before runtime writes.
- **PC source:** Uncommitted on `mac-port`: `tools/apply-patches.py` and
  `tests/test_patch_updates.py`; tested with `main` revision `5cf4866` patches.
- **PC validation:** Eleven patch-loader tests passed on Windows, plus fresh and
  existing-Windows fixtures using the complete main patch stack; repeated
  application made no changes. See `STATUS.md`, 2026-10-15 patch preparation.
- **Existing Mac support:** The same Python patch loader serves both hosts.
- **Mac adaptation needed:** No native adapter changes; validate the shared
  loader on macOS. This does not add the Windows watermark patch to this branch.
- **Mac acceptance checks:** Run patch-loader tests and apply/reapply the
  complete merged stack on disposable sources, preserving actual code conflicts.
- **Mac implementation and evidence:** Shared fix present locally; macOS checks
  and other-device synchronization remain unverified.

## Entry template

Copy this template into the queue for each new entry. Replace all placeholders.

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
