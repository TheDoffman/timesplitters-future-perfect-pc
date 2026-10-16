# Measured progress

Tested on Windows and Apple Silicon macOS on October 14-15, 2026. This is an experimental port bring-up;
full gameplay and campaign completion have not been verified.

## Mac branch update before publishing — October 15

- Incorporated the newer Mac camera/prompt/multistage-renderer commit `6e48241`
  into the prepared main compatibility merge, preserving both lines of work.
  Shared prompt wrappers use the new guarded guest-memory helpers while still
  allowing the Online text rewrite in controller mode. Network backlog moved to
  `MAC-013` so existing audited `MAC-001` through `MAC-012` keep their identities.
- Windows/shared validation passed: 47 Python/compiler checks, with five
  Mac-specific checks skipped. Recompiled the shared controls, prompt model,
  guest prompt regression and display-menu sources with MSVC x64; linked the
  prompt regression against the existing input/SDL libraries and passed its
  controller/keyboard switching, guest ABI and Online-label checks.
- A full Windows game build and native Apple Silicon execution of this combined
  revision remain pending; prior evidence is retained below. No game or personal
  settings/saves were used for this merge.

## Mac/main merge compatibility — October 15

- Reconciled `mac-port` at `93dc422` with main at `16fcea5` for PR #10.
  The shared display menu retains POSIX memory-query compatibility and the
  Windows Online/System Link entries without linking the Windows host adapter
  into native Mac. Native network wrappers delegate to their original guest
  functions until a Mac transport is implemented; see backlog `MAC-013`.
- Kept existing runtime patch files intact. The updater applies the existing
  network patches before native boot, retaining its adjacent context around the
  SDK block and reversing the same order. Regression tests cover fresh installs,
  repeated builds, older Windows/Mac stacks, CRLF context and local-edit refusal.
- On Windows, 47 selected shared/native-tool/network tests passed; five Mac
  compiler/configuration/Trash checks were skipped. Compiled the affected shared
  display-menu source with MSVC x64, and compiled/executed guest-ABI tests for
  both the host-network route and native fallback without host-network symbols.
- Native Apple Silicon build, GPU/menu and multiplayer execution are not verified
  by this merge check. No game launches or personal settings/save changes were
  required; the existing Windows texture branch and overhaul files were preserved.

## Shared Windows and Mac feature policy/input model — October 15

- Added tracked shared-development rules to `AGENTS.md`: common behavior has one
  implementation wired into both builds; native API work stays in adapters, and
  unimplemented or untested platform support must be recorded explicitly.
  Git synchronization and rebuilding are still required between devices/branches.
- Found and corrected a concrete gap: the Windows diagonal-movement speed fix
  was behind `TSFP_PC_CONTROLS`, so the Mac binding path skipped it. The new
  `zzzzzzz-shared-input-model.patch` compiles the same `src/pc_input.c` on Mac
  and invokes its normalization from the shared binding parser. Windows's
  existing implementation/build path is unchanged. The Mac SDL regression now
  checks W+D speed normalization and release back to full straight-line speed.
- Native manifests now include shared source hashes. README documents the shared
  paths and remaining differences: Windows settings UI, direct-camera mouse path,
  display/texture-pack hooks and separate graphics APIs do not become portable
  merely by compiling the input model. No complete feature-parity claim is made.
- Rebuilt on macOS 27.0.1 ARM64 / Apple M5 Pro, AppleClang 21, pinned SDL 3.4.16
  and OpenGL 4.1 Metal -91.7. Supported US XBE and upstream `d90c929` verified.
  Source base `ea1979a` plus the prior interpolation repair and this change.
  Three native CTests, real-GPU/SDL checks (including diagonal input) and 25
  tool/patch tests pass; two Windows-only tests skipped. No mission run was
  required for this shared binding change. Windows execution and the other
  computer's synchronization remain unverified.
- Evidence: `logs/shared-pc-mac-build.log`, `logs/shared-pc-mac-tests.log`,
  `logs/shared-pc-mac-validation.json` and `build/macos/BUILD.json`.
  Changes remain local and uncommitted on `mac-port`; personal saves/settings
  were untouched, and disposable native regression disks were recycled.

## Native interpolation mesh-layout repair — October 15

- Investigated the user's missing first-mission terrain/character screenshots.
  Found a shared replay-state bug: vertex declarations changed during a guest
  frame, but interpolation retained neither those changes nor the initial
  declarations. Replaying with the final layout could misread vertices or skip
  a draw entirely. Immediate-mode effects temporarily use a sixteen-float4
  layout, then return to their normal stream layout.
- Added `zzzzzz-interpolation-layouts.patch` after the existing patch stack.
  It snapshots active declarations at frame start and copies successful layout
  changes into the replay in order, preserving the final guest state. The native
  executable was rebuilt; temporary culling/depth/CPU diagnostic edits were
  removed. No generated game C was edited.
- Before the fix, a moving first-level guest/midpoint pair showed fire/smoke
  disappearing in the midpoint; logs also reported a skipped vertex declaration.
  After the fix, both frames retain those effects, terrain and the nearby soldier.
  Reviewed further movement/turn captures retain terrain. **The exact complete
  terrain disappearance in the user's photograph was not reproduced, so its
  full resolution remains unverified.** No full-level playthrough was attempted.
- The final 145-second isolated 120-FPS run ended normally at its time limit,
  with no native fault, sampled GL error or skipped-declaration warning.
  Movement/turning used synthetic SDL keys. In seconds 115–135 after the first
  host present, the stationary sample measured **120.00 host FPS**: 1,200 guest
  plus 1,200 interpolated presents (60.00 FPS each); p95 11.47 ms, worst 13.34 ms.
  The moving/capture sample at seconds 95–110 measured 116.81 host FPS
  (59.67 guest + 57.14 interpolated). These are presents, not display scanout.
- Validation: 25 portable/native-tool tests passed, 2 Windows-only tests skipped;
  all three native CTests and real-GPU/SDL regressions passed. The new regression
  compiles the actual shared recorder with a mock renderer and checks layouts at
  draw boundaries, copied payloads, rejected changes and final restoration. It
  fails when declaration recording is deliberately removed. Complete patch-stack
  checks and the read-only native launcher prerequisite check pass. Native Windows
  execution remains untested for this shared recorder change.
- Build base `ea1979a` plus the new patch, upstream `d90c929`, supported US XBE
  verified. Host: macOS 27.0.1 ARM64, Apple M5 Pro, AppleClang 21, SDL 3.4.16,
  OpenGL 4.1 Metal -91.7. Evidence and full hashes are in
  `logs/render-layout-validation.json`, `logs/render-layout-fixed.*`,
  `logs/render-layout-build.log` and `build/macos/BUILD.json`. Selected before/after
  PNGs are `private/captures/native-mac/render-layout-*.png`; redundant diagnostic
  PNGs were removed. Isolated controls were restored and disposable test disks
  recycled. Personal saves/settings were untouched. Changes are local and
  uncommitted on `mac-port`; the other computer's synchronization is unverified.

## CrossOver integration removed — October 15

- Removed the compatibility launcher, Python implementation, dedicated tests
  and setup instructions. The native launcher owns its game-data/settings
  validation directly; it has no dependency on the removed launcher. Shared
  `AGENTS.md` now documents native Mac and Windows workflows.
- Moved the dedicated TimeSplitters compatibility-test bottle and obsolete
  Python caches to Trash. The bottle contained 309.5 MiB of physical allocation
  (305.2 MiB logical); recycling preserves recovery and does not free that space
  until Trash is emptied. The shared application, Windows build, native binary,
  original game data, personal settings and saves were preserved.
- On macOS 27.0.1 ARM64 with Python 3.12, 24 portable tests pass and 2
  Windows-only checks are skipped. Native launcher `--check`, shell syntax and
  diff whitespace checks pass. No game rebuild or gameplay run was needed:
  runtime source/patches are unchanged from native build `024d9be` (AppleClang
  21, SDL 3.4.16, OpenGL 4.1 Metal -91.7). Source base is `024d9be` plus this
  launcher/documentation removal. Evidence: `logs/remove-crossover-tests.log`,
  `logs/remove-crossover-cleanup.json` and `logs/remove-crossover-validation.json`.
- Changes remain local on `mac-port`; synchronization on the other computer
  is unverified. Historical validation logs remain as recovery records.

## Native Mac mouse-look — October 15

- Clicking inside the focused native game window now enables relative
  mouse-look. The capture click is consumed; subsequent clicks use the shared
  fire binding. F1 remains an alternative toggle. Escape and focus loss release
  capture, clear pending look samples and release held mouse buttons. Disabling
  mouse-look also releases capture on the main thread.
- Corrected the vertical sign after a first-level check showed upward mouse
  movement looking down with the default profile. Native input still feeds the
  original right-stick look path, so profile look speed/inversion also applies;
  it does not yet use the Windows direct-camera mouse path.
- The first 150-second isolated run reviewed horizontal/vertical turning,
  capture without an ammunition decrease, and pause/resume/recapture, and exposed
  that vertical-direction bug. The final 135-second run reviewed horizontal
  turning and upward motion looking up after the correction. Additional live
  input and focus changes interrupted its later scripted sequence: the final
  down/pause/resume/fire sequence is **not** counted as a controlled pass.
  Both runs reached their time limit without a native fault or sampled GL error.
  Mouse actions used macOS event delivery; navigation used synthetic SDL keys.
  This does not establish a completed human hardware playtest.
- Real SDL-window regression checks pass for click capture, swallowed capture
  click, subsequent fire, immediate neutralization on release, F1, focus loss,
  disabled mouse-look, sensitivity and inversion. Three native CTests and GPU
  regressions pass; portable checks report 19 passed, 2 Windows-only skipped,
  and 7 subtests. The read-only native launcher check passes. Windows was not run.
- Build base `9b00846` plus these native source changes; pinned runtime
  `d90c929` and supported US XBE verified. Apple M5 Pro, macOS 27.0.1 ARM64,
  AppleClang 21, SDL 3.4.16, OpenGL 4.1 Metal -91.7. Full source/binary hashes,
  test routing and limitations are in `logs/native-mouse-validation.json` and
  `build/macos/BUILD.json`; evidence includes `logs/mac-mouse-*` and selected
  `private/captures/native-mac/mouse-*.png`. Personal saves/configuration were
  absent and remain absent; disposable test disks were recycled. Changes are
  local on `mac-port`; the other computer's synchronization is unverified.

## Native Mac up-to-120 FPS mode — October 15

- Native launches now default to **2x render interpolation**, retaining the
  60 Hz console clock. `--fps 60` / `--fps 120` override only that launch;
  existing `frame_interp = 1` preferences remain respected. Windows behavior
  and personal saves/settings are unchanged.
- Compiled the decoded NV2A vertex programs into cached OpenGL vertex shaders,
  retaining the portable CPU interpreter as fallback/reference. Native game
  C now uses `-O2` with debug symbols and vectorization disabled for the scalar
  MMIO fault handler. Changes to pinned upstream sources are reproduced by
  `zzzzz-mac-high-refresh.patch`; generated game C was not edited.
- Connected the shared draw replay/matrix interpolation with preservation of
  the completed guest image/depth. Cleared intermediate depth separately after
  visual comparison exposed missing fire/smoke, and added a real-GPU regression
  for depth isolation and restoration. Late cinematic frames consume an already
  delivered vblank rather than waiting an extra tick for interpolation.
- The final **165-second** isolated run's stationary first-level canyon window
  (120–145 seconds after the first present) measured **120.00 host FPS**:
  **1,500 guest + 1,500 interpolated presents**, each **60.00 FPS**. Median host
  interval **10.02 ms**, p95 **13.47 ms**, p99 **15.61 ms**, worst **16.82 ms**;
  none exceeded 33.33 ms. Captures were enabled, with explicit paired readback
  outside the measurement window. These are completed host-present intervals,
  not monitor scanout measurements; spacing remains uneven rather than locked
  to 8.33 ms. Background focus during this window is recorded in the evidence;
  a separate foreground input check is also retained.
- A separate 110-second foreground run measured **119.90 host FPS** (60.00
  guest + 59.90 interpolated) in seconds 80–90: median 9.81 ms, p95 11.76 ms,
  worst 16.00 ms. Reviewed fresh captures of movement, the pause menu, resumed
  gameplay and ammunition decreasing after firing. Keyboard events were synthetic
  SDL input; firing used macOS mouse-event delivery, not a human playtest. The
  paired moving frames retain fire/smoke and show different geometry pixels.
- Host: Apple M5 Pro (16 GPU cores), macOS 27.0.1/26A434 ARM64, built-in display
  reporting 120 Hz; scene 640x480, AppleClang 21, pinned SDL 3.4.16,
  OpenGL 4.1 Metal -91.7 / GLSL 4.10. Build base `83491ef` plus this change,
  pinned runtime `d90c929`, and the supported US XBE; full hashes/tool versions
  are in `build/macos/BUILD.json` and `logs/native-120-fps-validation.json`.
- GPU/CPU transform-feedback comparisons cover MAC/ILU operations, paired
  reads, output/temporary aliasing and relative constants. All GPU/input tests
  and three native CTests pass. Portable checks: **19 passed, 2 Windows-only
  checks skipped, 7 subtests**; read-only native launcher check passes. No native
  fault or sampled GL error in the final timed benchmark.
- This is an up-to-120 target, not a campaign-wide guarantee. Cinematics,
  transitions, unmatched draws and insufficient rendering time can hold frames;
  some particles remain at the guest rate. Full-level completion, save/load,
  physical-controller play and complete rendering fidelity remain unverified.
  Changes are local on `mac-port`; the other computer has not been verified.
  Evidence: `logs/mac-120-verified.*`, `logs/mac-120-input-*`,
  `logs/mac-120-build.log`, `logs/mac-120-portable-tests.log`, and selected
  `private/captures/native-mac/native-120-*.png` / `interpolation-*.png`.

## Earlier 120/144 FPS feasibility check (before implementation) — October 15

- Tested `frame_cap = 0` only in the isolated runtime settings, keeping the
  console vblank clock at 60 Hz. The log confirmed the flip gate was off.
  In the same stationary canyon window (120–145 seconds), 1,500 host-present
  intervals still measured **60.00 FPS**, median **16.57 ms**, p95 **18.99 ms**,
  and worst **31.63 ms**. Removing that gate is not a high-refresh solution for
  this build. The 150-second run had no native fault or sampled GL error.
- The current Apple M5 Pro's only reported display is its built-in Color LCD,
  configured at **120.00 Hz**. Displaying 144 distinct frames per second would
  need an appropriate higher-refresh display; 120 is the relevant built-in target.
- Source inspection confirms numeric caps are divisions of the console vblank
  clock, and at that revision the native Mac interpolation entry points were unimplemented.
  Increasing the console clock may change game speed and was not used as a
  substitute for rendering support. The follow-up CPU sample placed 1,193 of
  2,633 main-thread samples in the CPU vertex interpreter. GPU vertex shaders
  and render-only high-refresh presentation were still development work at that
  point. The later 120 FPS implementation is recorded above; 144 is not a target.
- Restored the isolated runtime settings exactly and recycled disposable test
  images. Personal saves/settings were not used. No runtime source or binary
  changed; only findings/documentation were updated. Evidence:
  `logs/native-higher-fps-validation.json`, `logs/mac-perf-uncapped.csv`,
  `logs/mac-perf-uncapped-boot.json`, and `logs/mac-perf-uncapped-sample.txt`.

## Native frame-time investigation and upload fix — October 15

- Confirmed the reported slowdown on `mac-port`. Measured host-present intervals
  in the stationary first-level canyon, 120–145 seconds after the first present,
  excluding menus, loading, the opening animation and CPU sampling. Three bounded
  150-second runs used the same isolated profile and menu input sequence.

  | Run | Average FPS | Median frame | 95th percentile | Worst frame |
  | --- | ---: | ---: | ---: | ---: |
  | Before, rotating captures enabled | 14.63 | 68.34 ms | 71.61 ms | 80.61 ms |
  | Before, captures disabled | 14.85 | 67.28 ms | 70.48 ms | 74.60 ms |
  | After, rotating captures enabled | 60.00 | 16.44 ms | 19.09 ms | 33.80 ms |

- Captures were not the main cause. CPU sampling placed 1,442 of 1,942 main-thread
  samples in `glBufferSubData`, including driver waits for earlier GPU work.
  The renderer reused offset zero in one transient vertex/index buffer on every
  draw. Replacing each buffer's storage with `glBufferData` lets the driver retain
  earlier uploads while the GPU finishes them. The follow-up sample has no
  `glBufferSubData` calls. CPU vertex interpretation remains a performance limit.
- Compiler settings stayed unchanged to isolate this fix: Debug game code and
  the runtime libraries' existing `-O2`. `BUILD.json` now records the build type
  and complete configure command. The result is specific to this scene and Mac;
  it does not establish 60 FPS throughout the campaign or monitor scanout pacing.
- Added opt-in `--profile-frames` (20,000 CSV rows maximum, no GPU readback) and
  a frame-statistics tool that selects a known gameplay interval. The corrected
  1,500-frame measurement had one interval over 33.33 ms and none over 50 ms.
- Added real-GPU coverage for 128 queued, changing vertex/index uploads before
  readback, including nonindexed draws and both 16/32-bit indices. All graphics,
  input and three native CTests pass; portable tests: **17 passed, 2 Windows-only
  checks skipped, 7 subtests**. The read-only native launcher check passes.
  The final timed run reached at least 7,800 guest frames and host presents,
  with no native fault or sampled GL error. Reviewed the world/weapons/HUD capture.
- Test partition images were recycled; personal saves/settings were not used.
  Evidence: `logs/native-frame-performance-validation.json`,
  `logs/mac-perf-*.csv`, `logs/mac-perf-*-sample.txt`,
  `logs/mac-frame-fixed-build.log`, `logs/mac-frame-portable-tests.log`, and
  `private/captures/native-mac/native-frame-performance.png`.

## Focused native combat smoke test — October 15

- Continued on `mac-port` with the same native binary and isolated test profile.
  Traversed the initial canyon beyond the spawn area, reached the first enemy
  encounter, fired both handguns, observed ammunition consumption and enemies
  falling, and saw the accompanying marines advance. The world remained visible.
  This was an automated smoke test, not a human playthrough.
- Used temporary I/J/K/L aim and O fire bindings in the isolated test configuration
  for reproducible SDL input. Restored the fixture afterward. Personal saves and
  settings were not used or created. During inspection, waited for a fresh
  rotating frame; stale captures had made some turns appear inconsistent.
- The user narrowed the scope before full-level completion. The test was stopped
  intentionally with native exit 0; the runner reports an early `process_exit`
  rather than a passed timed probe. No new gameplay/rendering blocker was confirmed
  in this coverage. The 477-second session reached at least 8,400 guest frames
  and host presents, with no native fault or sampled GL error.
  Full-level completion, checkpoint reload, campaign save/load,
  audio fidelity and physical-controller play remain unverified.
- Extended the runner's explicit duration ceiling to 30 minutes, retaining its
  15-second default, isolated paths, two rotating captures and 8 MiB log limit.
  Portable regressions: **16 passed, 2 Windows-only checks skipped, 7 subtests**.
  No renderer/runtime source changed, so the native binary was not rebuilt.
- Evidence: `logs/native-combat-validation.json` and
  `private/captures/native-mac/first-combat-smoke.png`. Disposable partition images
  were recycled after the run.

## Visible native first-level gameplay — October 15

- Fixed the black world on `mac-port`. A per-draw GPU trace showed the world
  rendered correctly, then a later clear erased it. The game switches to a
  **160×120 offscreen texture** for that pass; the Mac backend previously returned
  no surface from `GetSurfaceLevel` and ignored `SetRenderTarget`, so the clear
  landed on the visible back buffer.
- Added real texture surfaces, OpenGL framebuffers and depth/stencil attachments,
  reference-counted target ownership, a persistent device depth surface, target
  viewport reset, rectangle/viewport clear clipping, back-buffer readback while
  an offscreen target is bound, and correct texture orientation on sampling.
  Failed/incomplete target changes preserve the prior target. These changes are
  confined to the native Mac backend; shared upstream patches and generated
  game C are unchanged.
- Reviewed actual first-level captures after the opening animation: canyon,
  enemies, dual pistols, crosshair and ammunition HUD stay visible. Forward
  input changed the viewpoint; firing reduced both pistols from **16 to 15**.
  Captured OS mouse deltas turned the view from the canyon toward a rock wall.
  Pause/resume returned to the visible world. These are bounded checks using
  SDL test input and macOS CoreGraphics events, not a human full-level playthrough.
- Added GPU regressions for offscreen clear isolation, top/bottom sampling,
  rectangle clears and preserving the old target after an incomplete attachment.
  All GPU/input checks and three native CTests pass. Portable checks:
  **16 passed, 2 Windows-only checks skipped, 7 subtests passed**. Windows
  build/runtime checks remain unrun on this host.
- Added `Play-Native-Mac.command` for interactive play with the native binary.
  It verifies ARM64/XBE/build hashes, preserves personal settings, strips stale
  test overrides and bounds logs to 8 MiB total. Its read-only readiness check
  passes. The interactive launcher was not used for automated game testing;
  personal save/settings paths remain untouched.
- The 120-second exact-binary gameplay probe completed at least **2,700**
  guest frames and host presents, with no native fault or sampled GL error.
  Its last render-target summary recorded **16,206 switches, zero failures**.
  Disposable test images were recycled; selected captures stay under `private/`.
- Evidence: `logs/mac-world-build-and-tests.log`, `logs/mac-world-portable-tests.log`,
  `logs/native-world-validation.json`, `logs/native-fixed-world-draw.txt`, and
  `private/captures/native-mac/world-playtest.png` / `world-resumed.png`.
- Remaining limits: full-level completion, campaign save/load, audio fidelity,
  physical controllers and complete renderer fidelity are unverified. The
  vertex programs still execute on the CPU; general multitexturing, cube/depth
  textures and screen-copy effects need further work. This is a first-level
  gameplay milestone, not a finished Mac release.

## Native Mac controls and first story-level load — October 15

- Added a Mac-only SDL event bridge to the shared JSON binding reader through
  `zzzz-mac-input.patch`. Keyboard/mouse state is published atomically for guest
  worker polls. Main-thread Cocoa events handle focus loss, mouse capture and
  window close. A new launch raises the game window once; it does not steal
  focus back during play. Windows control code and generated game C are unchanged.
- Confirmed the real menu flow: player selection, isolated profile creation,
  storage selection, main menu, Story, **2401: Time to Split**, difficulty,
  briefing and loading. Up/down changed selection. The isolated profile survived
  a new process and was selected again. This is profile persistence only, not
  campaign save/load validation.
- SDL event injection exercised the binding-to-XInput path. Separately, macOS
  CoreGraphics keyboard events delivered through the OS opened the in-level
  pause menu with Escape, moved its selection with Down and resumed with Escape.
  This verifies OS event delivery, not a human keyboard/controller playtest.
- The first level rendered its opening 3D scene and reached an ammunition HUD,
  but the gameplay world became black. No full playability, movement/collision,
  combat or rendering-fidelity claim is made. Renderer gaps remain the immediate
  blocker after menu navigation and level startup.
- F1 toggles relative mouse capture; Escape/focus loss releases it. The shared
  mouse enabled/sensitivity/invert settings are read at startup. The first free
  SDL controller shares keyboard player 1 without taking pads assigned to other
  players. Physical mouse look and hardware-controller play remain unverified.
- Native regression checks pass for key press/release, arrow/WASD bindings,
  focus neutralization, mouse buttons/deltas/settings and SDL virtual-controller
  hotplug/sharing. All three CTests and the real-GPU regression pass. Portable
  checks: **13 passed, 2 Windows-only checks skipped, 7 subtests passed**.
  Windows build/runtime tests were not run on this Mac.
- Repeated bounded 120-second menu/level probes completed without a native fault
  or sampled GL error. The final focused 100-second run logged at least 1,800
  guest frames and host presents and again reached the level. Window-close
  handling now exits cleanly at the probe's
  time limit (exit 0), without the former forced kill. Each probe uses
  `private/test-saves/native-mac`; known disposable partition images are recycled.
  Personal saves and settings remain unused and unchanged.
- Evidence: `logs/mac-input-build-and-tests.log`, `logs/mac-input-patch-tests.log`,
  `logs/native-input-validation.json`, `logs/native-level-summary.json`,
  `logs/native-boot-summary.json`,
  `private/captures/native-mac/menu-navigation.png` and
  `private/captures/native-mac/native-pause-navigation.png`,
  `private/captures/native-mac/first-native-level.png` and
  `private/captures/native-mac/native-gameplay-hud.png`.

## First visible native Mac menu — October 15

- On `mac-port`, connected the existing shared HLE graphics hooks to an Apple
  Silicon SDL3/Cocoa OpenGL 4.1 backend. The driver reports **GL 4.1 Metal - 91.7,
  GLSL 4.10**. The executable is native ARM64 Mach-O, with no Wine/Rosetta process.
- Visually reviewed actual GPU back-buffer captures: the TimeSplitters logo,
  animated menu background, **1 PLAYER / 2 PLAYER / 3 PLAYER / 4 PLAYER**, and
  **A Select** are readable. Repeated bounded runs reached this menu without a
  native fault. The final 45-second probe logged at least 2,100 guest frames
  and host presents, with no sampled GL errors; termination at the limit needed
  SIGKILL after the three-second grace period. This establishes the first visible
  menu; no input was injected,
  and menu navigation or gameplay is not claimed.
- Reused the shared vertex declarations, guest state mapping and texture cache.
  Added CPU execution of NV2A vertex programs with screen-space conversion,
  GLSL register-combiner evaluation, quad triangulation, Xbox texture unswizzling,
  DXT and A8 font texture uploads, and GPU readback. The loading worker exposed
  a missing current-context fault; a recursive lock now hands the context between
  guest threads. Cocoa window creation/event pumping remain on the main thread.
- Added a separate reproducible `zzz-mac-renderer.patch`; prior patches and
  generated game code remain unchanged. The Mac backend is `src/native/renderer_gl.c`;
  the existing Windows and Linux backend source selection remains intact.
  Shared portable format helpers compile from the pinned upstream source.
- Real GPU regression checks pass for quad coverage, A8 unswizzling and alpha
  blending, a synthetic NV2A vertex program, per-stage combiner constants,
  readback, and worker-thread context handoff. The three native CTest suites
  pass. Portable patch/launcher/isolation checks: 13 passed, 2 Windows-only
  PowerShell checks skipped, 7 subtests passed. The Windows executable was not rebuilt or tested.
- The probe supports `--capture`, with two rotating BMPs, isolated save/settings
  paths, bounded run/log size and validated partition-image recycling. Its JSON
  separates guest frames, sampled successful host presents, sampled GL errors,
  and capture hashes; visual menu recognition is recorded separately. Personal
  saves/settings remain unused. Project physical allocation remains about 8.5 GiB,
  with a 258 MiB native build and about 2 MiB of selected/rotating captures.
- Evidence: `private/captures/native-mac/first-native-menu.png`,
  `logs/native-boot-summary.json`, `logs/native-renderer-validation.json`,
  `logs/mac-renderer-build-and-tests.log`, and `logs/mac-renderer-patch-tests.log`.
- Remaining renderer gaps include general multi-texturing, offscreen render
  targets, screen-copy effects, movie playback and full 3D fidelity. Native
  controls/navigation, audible quality/latency, PC display/texture features,
  gameplay, fullscreen and save/load are not verified. Vertex programs currently
  execute on the CPU; a menu result is not a performance claim for gameplay.

## First native Apple Silicon execution — October 15

- Built `build/macos/futureperfect_recomp` as a native ARM64 Mach-O executable
  on macOS 27.0.1 with Apple Clang 21, Python 3.12 and the shared pinned runtime,
  Python dependencies and SDL submodule. No Wine, Rosetta or Windows process
  participates in the native runs. The verified US XBE was regenerated locally.
- A bounded 30-second run loaded all 23 XBE sections, resolved all 151 kernel
  imports, initialized the game's graphics device successfully, loaded game
  assets, ran worker threads and completed at least 1,800 guest frame calls.
  No native fault was recorded. The runner stopped it at the time limit;
  SIGTERM did not exit within the three-second grace period, so it used SIGKILL.
  This is **game-code execution without a connected host renderer**: no menu
  image, game window, gameplay, physical controls or save/load was verified.
- The first attempt exposed missing related address mappings. The Mac now
  reserves one 4 GiB virtual range and maps only its own unused pages; all 28
  low-RAM aliases, contiguous memory, device apertures and tiled alias mapped.
  It does not weaken macOS protections or overwrite unrelated host mappings.
- Implemented native relative/absolute and periodic waitable timers, and an
  ARM64 signal handler for supported scalar MMIO loads/stores. Protected NV2A
  interrupt writes and AC97 reset writes have real-signal regression coverage;
  unsupported instructions stop with diagnostics instead of being skipped.
  Read/write aliases let handlers update device backing without opening the
  protected guest page to concurrent writes. APU accesses use the existing
  device model, whose DSP implementation remains incomplete upstream.
- Implemented POSIX sparse partition images with the existing retail geometry,
  partition-device path routing, volume-directory opens and relative file opens.
  Existing image contents are preserved. These fixed the guest's early request
  to return to the Xbox dashboard. Isolated title metadata was created under
  `private/test-saves/native-mac`; personal saves were not used.
- Added an SDL3 PCM output backend and reused the portable controller-binding
  path. SDL audio output opened and PCM submissions occurred. Audible quality,
  latency and physical input remain unverified; audio uses the existing
  wall-clock fallback rather than claiming a hardware playback cursor. Movies
  are skipped because the native movie/renderer path is not connected.
- The initial graphics probe exposed an additional upstream settings fallback
  at `~/.config/xboxrecomp/titles/45410066.conf`. Its newly created default was
  moved into the isolated test directory. The probe now explicitly routes
  `RECOMP_DISPLAY_CONFIG`, `TSFP_DISPLAY_CONFIG`, input, saves and XDG roots;
  regression tests cover stale override removal and protection of personal paths.
- Three native CTest suites pass: mapping/threads/events/timers, ARM64 MMIO,
  and save/partition routing. Portable checks: 12 passed, 2 Windows-only
  PowerShell tests skipped, 7 subtests passed. Fresh application, upgrades,
  CRLF inputs, conflicts and repeat application of the complete patch stack
  pass. The actual Windows executable was not rebuilt or tested on this Mac.
- The shared rules now distinguish the retained Windows compatibility build
  from one active native build in `build/macos`. Native build provenance is
  recorded in its ignored `BUILD.json`; runtime changes are reproducible from
  `patches/zz-mac-native-boot.patch`, applied after the existing feature stack.
- Native tests recycled their known disposable partition images. The game
  probe's six images had 5,243,406,848 logical bytes but only 135,168 allocated
  bytes; the images are no longer in the project. Metadata/configuration and
  diagnostic logs remain. Physical project allocation is approximately 8.5 GiB,
  including the original ISO, the single game extract, dependencies and builds;
  `build/macos` is approximately 256 MiB.
- Evidence: `logs/native-boot.out`, `logs/native-boot.err`,
  `logs/native-boot-summary.json`, `logs/mac-native-build-and-tests.log`,
  and `logs/mac-native-patch-tests.log`. Earlier feasibility notes below are
  historical and are superseded by this native execution result.

### Next native milestone

The first-menu renderer connection is complete (see the newer result above).
Validate physical input and menu navigation, then extend rendering support for
an in-game scene. Movie presentation, PC settings/display/texture features and
save/load remain separate milestones. The earlier execution-only observations
in this section describe the state before the renderer connection.

## Native Mac feasibility and ISO preparation — October 15

- Work is on `mac-port`; the user requires changes from this chat to stay on
  that branch unless explicitly directed to `main`.
- Validated the supplied 4,047,372,288-byte ISO's embedded executable against
  the supported US SHA-256. Moved the original into ignored `private/discs`
  and extracted one copy: 412 files, 4,046,110,016 bytes, in `private/game`.
  Extraction paths/sizes were checked and the extracted executable rehashed.
  `private/disc.json` and `logs/iso-verification.json` record the checks.
- Game data is now available locally; no Mac gameplay validation was performed
  in this ISO preparation pass.
- Checked out the exact pinned runtime revision under `external/ts3FPrecomp`.
  Found a reproducible patch-installation failure on its CRLF CMake source.
  Made patch context whitespace handling explicit, avoiding reliance on the
  Windows machine's Git settings. The complete patch stack now applies and a
  second pass changes zero files. Conflict protection and CRLF tests pass.
- Added `mac-platform.patch`: the upstream Apple anonymous-mapping function
  returned descriptor 0 instead of creating a backing file. The native test
  failed before this fix and passes afterward. The patch uses an owned file
  descriptor, leaves no named backing file, and is restricted to `__APPLE__`.
- Compiled and ran an actual ARM64 Mach-O test with Apple Clang 21 on macOS
  27.0.1. It verifies 64 MiB aliased mappings, bidirectional data coherence,
  non-destructive collision rejection, a worker thread and event signaling.
  This runs natively. The initial low-address-linker
  experiment was terminated before useful output; the passing test uses the
  normal executable layout and OS-selected addresses. It does not establish
  that the full Xbox address-space mapping is ready.
- The native path-routing source passes a Clang syntax check. The memory-layout
  and kernel-bridge sources still fail on Windows process-affinity/timer APIs.
  Further source review found Windows-only title startup/graphics hooks,
  missing POSIX HLE audio/input implementations, an incomplete OpenGL renderer,
  a Windows-wired Vulkan backend, and Linux `.so` names in the non-Windows
  FFmpeg loader. Native game boot, rendering and gameplay are **not built or
  verified**. No platform support claim is inferred from the upstream README.
- Portable checks: 8 passed, 2 Windows-only PowerShell tests skipped, 7 subtests
  passed, using Python 3.12 and pinned pytest 9.1.1. Native evidence is in
  `logs/native-platform-before.*`, `logs/native-platform-after.*`, and
  `logs/native-kernel-compile-summary.json`. Personal saves and settings were
  not used. No disposable virtual disk images were created.

## Display settings and texture preview — October 15

- The working build now adds **Display settings** through the US game's own
  button builder in main Settings and paused Options. Resolution selection,
  Apply, Keep and Revert all use guest menu widgets and navigation.
- 1080p, 1440p, 4K and monitor-reported modes are offered. Rendering targets and
  depth buffers resize live; a 15-second unconfirmed preview returns to the old
  resolution. Confirmed settings persist beside the PC input configuration.
- The US camera wrapper updates the perspective matrix and horizontal CPU
  frustum together. The HUD retains its proportions in a centered 4:3 region.
  Front-end menus retain the original layout. Fullscreen remains Alt+Enter.
- The actual-menu replay verified 1080p apply/save, 1440p preview, timeout
  rollback, and return to the original menu. Native tests cover resolution
  bounds, monitor-mode deduplication, failed allocation handling at the API
  boundary, aspect calculations and persistence. Original game files, saves
  and keyboard/mouse configuration passed 424 protected-file hash comparisons.
- First-mission runs rendered 3840×2160 and 3440×1440 scene buffers and reached
  paused Options without a recorded crash or unresolved indirect call. The
  resolution checks use GPU render targets on the available display; a physical
  ultrawide monitor, split screen, and a full campaign are not validated.
- The expanded texture preview contains 11 replacements, approximately 44 MiB,
  covering selected terrain, masonry, timber, concrete, floor tiles, metal panels
  and one weapon atlas. Four replacements loaded in a 174-second opening-mission
  run at 1920×1080; gameplay and paused Options were reached without a recorded
  crash or unresolved call. The remaining seven need scene-specific runtime checks.
- DDS files have complete mip chains: two 4096×4096, eight 2048×2048, and one
  2048×1024. Square source art is 1254×1254; stone masonry is 1774×887. These are
  resampled outputs, not native 4K generated art. A character draft shifted facial
  and ear UV details and was kept out of the active pack.
- The coverage audit tracks 5,547 supported texture keys: 11 installed, one draft
  requiring correction/review, and 5,535 awaiting art. Another 753 cataloged
  payloads use unsupported formats. Embedded textures may remain undiscovered.
  This is not a completed game remaster. Eight offline tool tests pass; installed
  DDS hashes, dimensions and mip chains pass the audit. All 427 protected game,
  save and settings files matched their pre-test hashes; temporary disks and
  frame captures were recycled. Exact new prompts and real source sizes are saved.
- Pack compilation enforces the user's 6 GiB cap including metadata. Runtime
  replacement residency remains capped at 512 MiB. The supported-asset size plan
  is approximately 2.5 GiB before source art and unsupported-format additions;
  the plan is not completed artwork or a guarantee of final full-game size.
- Runtime changes are reproducible from the pinned upstream revision using
  `pc-controls.patch`, `save-routing.patch`, and `texture-pack.patch`; the last
  patch includes the renderer wiring shared by display and texture features.
  Fresh application in setup order and repeat-build checks pass.

## Completed

- Online source-build integration: a new network-menu entry, HTTPS browser with
  certificate fingerprint checking, public/private rooms, compatible-only
  listing/joining, peer credentials and leases, and a virtual-address bridge
  using the game host's forwarded UDP port. Guests establish outbound paths;
  the host relays guest-to-guest traffic. Two local instances used Online and
  an HTTPS directory to join a 2/16-player lobby and enter the same Shrink match.
  Cross-internet routing and full-match stability remain unverified. Campaign
  co-op remains a separate future effort.
  Automatic hosting now detects public IPv4 over HTTPS, starts/stops a local
  directory with the bridge, reuses its certificate and offers a copyable
  invitation containing the directory address, fingerprint and join code.

- Experimental PC System Link: native framed UDP, strict build/content identity
  checks, default-protocol discovery sockets, and XNADDR/key mapping. Two isolated
  game instances discovered a match, joined a 2/16-player lobby, and loaded into
  the same Thief match. This is a same-PC loopback test, not a cross-network test
  or a completed-match validation. See [multiplayer status](docs/multiplayer.md)
  for the later Online integration and remaining cross-network validation.

- Read the supplied desktop Xbox ISO without modifying it; extracted 412 files.
- Confirmed the US executable, title ID `45410066`, entry point `0038024D`,
  XDK 5849 and the expected SHA-256 documented in `README.md`.
- Identified 379 Xbox library symbols, including six upstream manual symbols.
- Generated 15,838 C function bodies with zero reported translation failures,
  totaling 2,507,843 lines across 16 chunks plus support files.
- Built `build/windows/Release/futureperfect_recomp.exe` with Visual Studio
  2022 / MSVC 19.44.35224 for Windows x64.
- Compiled and loaded FFmpeg 8.1 movie support.
- Fixed native profile device paths that bypassed `RECOMP_SAVE_DIR`, and
  preserved the fix in `patches/save-routing.patch`. Ten native path regression
  checks pass, including asset routing and switching between save directories.
- Added player-1 keyboard/mouse controls: normalized WASD movement, Windows raw
  relative mouse motion, direct camera-angle updates, mouse buttons and wheel,
  editable sensitivity/invert-Y settings, focus-loss clearing and cursor
  capture/release. The launcher loads `config/pc-controls.json`.
- Preserved the host integration and camera wrapper in `patches/pc-controls.patch`;
  the generated original `sub_000D2B80` body remains available as `_gen`.

## Validation

- Upstream Python suite: 547 passed, 41 skipped, 87 subtests passed.
- Upstream unittest discovery: 289 tests passed, including 26 skipped.
  This overlaps the pytest suite and is not an additional independent total.
- CPU differential conformance: 5,741 instruction vectors and 211 function
  vectors, zero mismatches.
- First 35-second startup test: decoded the EA and Free Radical intro movies
  (88 and 81 pictures), submitted PCM movie sound, reached the rendered
  player-count menu, and remained running until the test stopped it.
  No crash or unresolved indirect call was logged. Frame captures were
  inspected, including the player-count menu.
- Second 35-second input test: scripted A presses reached profile name entry;
  the captured name-entry screen was inspected.
- A subsequent 70-second test reached Story difficulty selection. It also
  exposed profile contamination through the original runtime's save-path bug.
  Earlier test profiles are preserved in `private/pre-fix-test-saves`.
- After fixing save routing, a fresh 100-second run created its profile under
  the requested test save directory, reached the first mission briefing, loaded
  `2401: Time to Split`, and rendered its opening cutscene and the landing scene.
  No crash or unresolved indirect call was logged; 3,578 function entries were
  observed. These scenes were inspected in captured frames.
- A 260-second fresh-save run reached first-person gameplay in the first mission
  and remained running until the test's time limit, with no logged crash or
  unresolved indirect call. It reached 3,855 instrumented functions.
  Scripted movement/look input changed the viewpoint, and trigger input reduced
  the displayed dual-weapon ammunition from 16 rounds per weapon to 14 and 13.
  Level geometry, textures, weapons, crosshair and ammunition display were
  inspected in the captured frames. This is a short gameplay check, not a
  completed mission or campaign playthrough.
- The final incremental build and native save-path tests also passed through
  Windows PowerShell 5.1, the same shell used by `Play.cmd`.
- Native PC input tests cover input clearing on focus/capture changes, consuming
  a recapture click, wheel press/release sequencing, equal mouse counts across
  different polling intervals, FOV scaling, vertical limits, inverted Y,
  normalized diagonal movement, and the actual bindings/guest-camera adapter.
- A 155-second run driven by the PC input model navigated menus with Enter,
  reached first-person gameplay, moved with W, applied raw-count camera changes,
  fired/reloaded, and switched weapons with the wheel. It ended at the test
  limit without a logged crash or unresolved indirect call. This used the
  adapter's deterministic event replay, not physical desktop input injection.
- A fresh-checkout fixture verifies that both local patches apply in Setup's
  order and reproduce all seven modified upstream files.
- A second 150-second run used the corrected aim/crouch bindings and reached
  3,874 instrumented functions with no logged crash or unresolved indirect call.
  Right-mouse aiming reduced the FOV from 55 to 30.25; a 20-count mouse event
  then changed yaw by 1.1 degrees, confirming the expected 0.55 zoom multiplier.
- The final build, both native test suites, and a six-second launcher startup
  test passed through Windows PowerShell 5.1 after the cursor activation fix.

Test output is in `logs/toolchain-tests.log`, `logs/pytest.log`,
`logs/conformance.log`, `logs/build.log`, and the timestamped smoke-test logs.
`logs/gameplay-smoke-summary.log` records the 260-second run;
`logs/final-build-check.log` records the final build and path checks.
PC input work is recorded in `logs/pc-final-bindings-build.log`,
`logs/pc-gameplay-summary.log`, and the newer `logs/pc-final-gameplay-summary.log`.
The final delivered build checks are `logs/pc-release-validation.log` and
`logs/pc-launcher-smoke.log`.
Original graphics captured by tests remain local in `private/captures`.

## Remaining work

- Verify mission progression, combat against enemies, control fidelity,
  profile reloads/checkpoints and sustained play beyond the short first-mission
  test. File creation and save isolation have been tested; a complete save/load
  gameplay round trip has not.
- Investigate translation coverage where execution requires it: the generator
  reports 50 unresolved direct-call targets emitted as diagnostic stubs, plus
  730 unsupported instruction occurrences emitted as comments/no-ops across
  30 mnemonics. Zero translation failures does not imply equivalent behavior.
- Check rendering and audio fidelity against the original game. Captured menu
  frames and host audio submissions are narrower evidence than a full playtest.
- Keyboard/mouse support currently targets player 1's first-person gameplay.
  Vehicle aiming, split-screen, MapMaker mouse navigation and extended hardware
  playtests remain unverified. Focus/capture behavior has automated state and
  activation-policy checks; no physical mouse/Alt-Tab playtest was performed.
- Aim/crouch toggle behavior follows the original game's profile. Mouse
  sensitivity/inversion is controlled by the PC configuration, independently
  of controller turn speed. The supplied PC profile selects keyboard input;
  controller input requires a controller binding configuration.
- Multiplayer and online behavior have not been tested here.

The generated C is a machine-code translation, not recovered original source.
Runtime code is pinned upstream; no claim of a completed decompilation or a
finished PC release is made.

## Interactive launcher correction (2026-10-15)

Normal play now launches through `tools/play.py`, which waits for the game and
writes its output directly to `logs/play-*.out` and `logs/play-*.err` using native
file handles. Interactive runs disable the generated code's default 400,000-line
function trace; diagnostic profiling remains available through SmokeTest.
The previous PowerShell GUI launch could return before the game, and inherited
or redirected output could stall startup. The corrected normal launch produced
captured startup logos and advancing swaps, with keyboard/mouse configuration
loaded. The user also confirmed that the logos appeared. Main-menu and gameplay
interaction for this hardware playtest is still to be tried.

## PC controls menu (2026-10-15)

Added a PC Settings → Keyboard & Mouse entry to the native game window's menu
bar, with F2 access in windowed and fullscreen modes. The owned settings dialog
lists all 24 Xbox actions with four keyboard/mouse binding slots each, mouse
sensitivity, inversion, mouse enable/disable, defaults, and Cancel. Capture
supports keys, five mouse buttons, and wheel directions; conflicting assignments
can be moved after confirmation. F1/F2/F9–F11 and Windows keys are reserved.
Gameplay input is blocked while editing, but simulation is not paused: pause
the mission first.

Saving applies bindings and mouse options immediately. Polls copy an immutable
binding snapshot under an SRW lock; editor input cannot become gameplay input.
Configuration changes preserve other ports and unrelated JSON members. Writes
use a temporary sibling file and atomic replacement, retaining the previous
configuration as `.bak`; failed writes retain the active bindings. Build checks
use a separate default fixture so user-customized bindings do not affect tests.

Validation: native input and save-path suites passed, as did the new settings
suite for live sampling, persistence and fresh-process reload, backup, unrelated
JSON retention, read-only failure, invalid values/reserved keys, defaults, native
dialog Cancel, key/wheel capture, and conflict reassignment. The UI was rendered
and visually inspected. A bounded real-game integration check verified both
entry points, F2 in fullscreen, input ownership/return, rendering after closing,
and that Cancel left the user's file unchanged. Both patches reproduced all
seven modified upstream files from pristine pinned sources. Evidence is in
`logs/pc-settings-release-validation.log` and `logs/pc-settings-game-tests.log`.

## In-game menu integration reverted (2026-10-15)

At the user's request, removed both the original-menu entry and the subsequent
game-rendered editing pages. Restored the original Xbox Settings/Options menus.
Keyboard/mouse gameplay controls, the separate F2 / PC Settings editor, and
user bindings/saves remain. Integration source is archived under
`private/rollback-in-game-menu`; generated code and the executable are rebuilt
without the guest menu wrapper or callback hooks.

Rollback validation: regeneration and Release build succeeded; save-path, PC
input/camera, and settings suites passed. A real-game UI check verified F2,
the PC Settings menu, and F2 in fullscreen; closing the editor returned input
and rendering to the game. The user configuration hash is unchanged. Logs:
`logs/revert-menu-build.log`, `logs/revert-menu-settings-ui.log`.

## Native Mac settings and graphics adapters (2026-10-15)

Local, uncommitted work on `mac-port` over `ea1979a4b6279045d0dc9c7c19472cb75e8a2216`.
Main and the other device were not changed; Windows build and cross-device
synchronization remain unverified. The native executable and every source hash
match `build/macos/BUILD.json` (executable SHA-256
`b6344c2b09bd415905d8675c05947a99f2063a069f5a361764d7954dce47b8ac`).
Host: Apple Silicon macOS 27.0.1, Apple Clang 21.0.0, SDL 3.4.16,
OpenGL 4.1 Metal 91.7 / GLSL 4.10. Shared upstream and supported XBE pins unchanged.

- F2 and the Mac Game Settings menu open a Cocoa Keyboard & Mouse / Graphics
  editor. Bindings, validation, defaults, unknown JSON preservation and display
  state use the PC implementation; POSIX atomic writes retain a controls backup.
  Sensitivity/inversion and bindings apply live. Conflicting keys are rejected
  with the two action names; Mac does not yet offer Windows' move-binding dialog.
  Closing the editor releases its input and raises the SDL game window.
- Native render targets support the shared resolution list, high-resolution
  scenes, widened projection/CPU frustum, and screen-position HUD classification.
  F11 toggles fullscreen; fullscreen is a session preference. The original
  Settings/Options menu's existing Display Settings integration is compiled on
  Mac too. Its 15-second preview logic is shared; the page's timeout interaction
  has not yet been exercised end-to-end on Mac.
- OpenGL replacements use the same texture keys and DDS loader/acceptance rules
  as Windows: full mip chains, BC1/2/3, aspect/no-downscale checks, 512 MiB resident
  replacement budget, and original-texture fallback. Launch with `--texture-pack`.
  Synthetic replacements were sampled on the real GPU and released correctly;
  this is not verification of a complete remaster pack.
- Integration found and fixed nested native settings initialization blocking
  input startup, and Cocoa presenting black while an offscreen FBO was bound.
  Both guest and interpolated swaps now bind the window drawable during swap.
  The GPU regression checks the drawable as well as guest readback.

Validation: complete patch-stack fresh/repeated/historical-upgrade tests;
28 Python tests passed, 2 Windows-only tests skipped; three native CTests passed;
real-GPU renderer/vertex/texture/resolution/offscreen/interpolation tests,
SDL input and shared settings tests passed. Settings checks cover live binding
sampling, exact backup, unrelated members/other controllers, invalid/reserved
values, symlink/read-only refusal and preserving active state on failed writes.
Standalone shared DDS-format tests and the interactive launcher's read-only
prerequisite check passed. Native MSVC/Direct3D tests cannot run on this host.

A bounded 130.15-second isolated game run at 1920x1080 with the 120-present target
reached the first mission and performed short movement checks. Reviewed both
rotating guest captures and an actual macOS window screenshot showing the world,
weapons, NPC, fire and HUD; full campaign completion was not attempted. At least
6,900 completed guest frames and 10,011 host presents were logged, with no sampled
GL errors or native fault. The 90–110-second movement interval measured 86.84
host presents/s: 56.17 guest + 30.66 interpolated, p95 19.32 ms, p99 31.48 ms.
Captures were enabled; these are host intervals, not display scanout or a broad
performance benchmark. The 60 Hz guest clock and personal FPS preferences remain
unchanged. The earlier pre-fix 1080p run established offscreen rendering only;
use the final run for on-screen evidence.

Separate bounded UI checks visually verified windowed/fullscreen menus,
resolution save/reload at 1080p, F2, controls save/reopen, Cancel, and keyboard
navigation after closing Settings. All tests used isolated saves/settings;
personal controls retain SHA-256
`cf44293910a77fb8f397ced933a1cfd5098e1949c93db9cc0571ed438b39f64a` and the absent
personal display file remains absent. Disposable partition images were recycled.
Evidence: `logs/mac-features-build.log`, `logs/mac-features-pytest.log`,
`logs/mac-features-final.json`, `logs/mac-features-ui.json`,
`logs/mac-features-final-frame-times.csv` and selected captures under
`private/captures/native-mac/mac-features-*` / `mac-settings-*`.

Remaining: general multi-texturing, movies/screen-copy effects and exhaustive
HUD placement/rendering fidelity; native direct-camera mouse behavior and PC
prompt/debug features are not claimed as equivalent. This work does not establish
that the previously reported world-disappearance case is completely resolved.


## Mac backlog renderer, camera and prompts (2026-10-15)

Validation performed before commit on `mac-port`, based on
`93dc422139bae4acb64d1e24c528cf3869a4ce20` plus the changes accompanying this
record. The other computer was not accessible for verification. Pinned upstream remains `d90c92988a9f99d0580d2ca3f7efd6ec3a7b7845`.
Host: macOS 27.0.1 arm64, Apple M5 Pro, Apple Clang 21.0.0, SDL 3.4.16,
OpenGL 4.1 Metal 91.7 / GLSL 4.10. The supported US XBE hash was checked:
`3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc`.
The complete patch stack was applied and wrappers regenerated through
`tools/build-native-mac.py --generate`; no generated C was hand-edited.
The final build manifest's recorded source hashes match the current sources.
Executable SHA-256:
`8f50deaa32247b94266b725fc6a00911b81236858743932be8e14edbfdf918e3`.

Implemented and checked in this batch:

- MAC-001: SDL relative counts now use the shared direct-camera model and verified
  US guest fields, preserving sensitivity, inversion, zoom scaling and pitch
  limits without adding mouse input to the controller stick. Native fixtures
  checked sensitivity/inversion/FOV, no stick injection, capture-click suppression,
  release and focus behavior. A live first-mission wrapper logged synthetic
  8/-2 count events producing -1.6/+0.4 degree yaw/pitch changes at sensitivity
  0.2 and FOV 55. Equal-count/polling-rate checks, complete live transition checks
  and physical-device testing remain pending on Mac.
- MAC-002: Mac compiles the shared prompt formatter and guest wrappers, and SDL
  activity selects the active prompt device. Real Mac VM region/protection checks
  replace the compatibility VirtualQuery stub for guest text/camera access;
  Windows retains its VirtualQuery implementation. Native fixtures passed
  custom/unbound/mouse labels, held/release/drift suppression, virtual-controller
  switching, live saved binding changes, guarded memory and guest stack/ABI
  restoration. Reviewed menus show [E] Select and [F] Back. Literal tutorial
  text still says START; artwork and non-token strings are not rewritten.
  Representative gameplay hints and physical-device switching remain unverified.
- MAC-005: all four 2D texture inputs and coordinates now reach fixed vertex,
  GPU vertex-program and CPU fallback paths. Tests cover distinct stages/UVs,
  projected coordinates, linear texture scaling and GPU/CPU agreement. FVF
  coordinate-size flags no longer accidentally select the programmable path.
  Reviewed first-mission captures show improved terrain, weapons, NPCs and
  debris. Dependent/dot texture modes, cube/volume resources and complete
  material fidelity remain unfinished.
- MAC-007: current-scene full/rectangular copies and image surfaces support
  32-bit ARGB/XRGB and linear variants. Real-GPU fixtures passed full/partial
  pixels, clipped offsets, orientation, preserved surrounding pixels/FBO state,
  and image-surface full/subrect read/write, double-lock and invalid-rect checks.
  Original and 1920x1080 scene sizes passed. Other formats explicitly return
  E_NOTIMPL; an actual title effect using the copy path remains to be checked.
- MAC-008: the first inclusive scissor rectangle now clips draws with target
  scaling, GL origin conversion and the existing HUD transform. GPU fixtures
  verified inside/outside pixels, disabling and clear behavior at original/1080p
  targets. Multiple/exclusive rectangles remain unsupported, matching the
  audited Windows limitation. This limited contract is marked Verified.
- MAC-010: the actual paused Options menu previewed 2560x1440, displayed the
  15-second countdown and automatically restored 1920x1080. The isolated saved
  mode stayed 1920x1080 and the game returned to visible first-mission gameplay.
  Keep/save/reopen, explicit Revert and broader aspect/HUD checks remain pending.

Validation commands/results:

- `python3 tools/build-native-mac.py --test`: all three native CTests passed,
  plus real-GPU renderer/multitexture/scissor/copy/image-surface/vertex/texture-pack/
  resolution/offscreen/interpolation/drawable tests and SDL input, prompt and
  settings regressions. Evidence: `logs/mac-backlog-build.log`.
- `.venv/bin/python -m pytest -q tests/test_patch_updates.py tests/test_native_interp_declarations.py tests/test_native_config_files.py tests/test_native_mac_tools.py tests/test_native_mac_launcher.py`:
  26 passed, two Windows-only checks skipped. Fresh, repeated and historical
  patch-upgrade coverage passed. Evidence: `logs/mac-backlog-python.log`.
- Final isolated runner: `--seconds 270 --capture --test-keys --profile-frames
  --fps 120`; 270.12 seconds, normal bounded exit, at least 13,800 completed guest
  frames and 15,530 successful host presents, no sampled GL errors/native fault.
  Captures were reviewed separately; frame counters alone do not establish
  gameplay. The final capture shows terrain, weapons, HUD and the opening
  tutorial. The run included menus, cinematics, resolution preview and brief
  synthetic input; it was not a full-level playthrough or physical-device test.
  Evidence: `logs/mac-backlog-final-summary.json`, `.err`, `-frames.csv`.

The final whole-run timing sample includes loading/cinematics and the higher
resolution preview: 58.33 host presents/s, comprising 51.69 guest and 6.64
interpolated presents/s, p95 25.56 ms and p99 37.18 ms. These are host CPU
intervals, not display scanout or a matched performance comparison. The 120
presentation setting and 60 Hz guest clock remain intact; sustained 120 FPS is
not established. Future renderer work needs scene-specific profiling as well as
fidelity checks.

Selected reviewed images in `private/captures/native-mac`:
`backlog-render-opening.png` (earlier renderer),
`backlog-multitexture-landing.png`, `backlog-multitexture-gameplay.png`,
`backlog-keyboard-prompts.png`, `backlog-paused-display.png`,
`backlog-resolution-countdown.png`, `backlog-final-landing.png`, and
`backlog-final-gameplay.png`. These are guest readbacks, not new macOS window
screenshots. Earlier cinematic border/outside-viewport artifacts and the user's
reported world-disappearance route are not claimed resolved by this batch.

One intermediate probe lost window focus and was stopped; it is not completion
evidence. The isolated SDL input file now accepts `motion`, `button` and `focus`
commands as well as key presses/releases. Synthetic focus allowed reliable menu
checks without changing normal interactive focus handling. This does not test
physical macOS event delivery. Runtime/input/display/save paths stayed isolated;
personal controls retain SHA-256
`cf44293910a77fb8f397ced933a1cfd5098e1949c93db9cc0571ed438b39f64a`, and the personal
display file remains absent. Known disposable partition images were recycled;
none remain in either native test directory.

MAC-003/004/006/009/012 remain queued. MAC-011 needs the real texture pack: its
local directory is absent, and the user confirmed it has not yet been transferred.
Synthetic pack tests passed again, but they do not verify actual artwork coverage.
Windows MSVC/Direct3D runtime checks are unavailable on this Mac; changes to the
shared headers/wrappers must still be checked on Windows. See
[MAC_PORT_BACKLOG.md](MAC_PORT_BACKLOG.md) for per-entry remaining acceptance work.


## Mac binding capture and camera checks (2026-10-15)

Validation performed before commit on `mac-port` based on `30653e3` (the checkout
advanced to the shared merge revision before edits began). Shared root rules are
unchanged. No work was moved to main; the other computer was not verified.
Host remains macOS 27.0.1 arm64 / Apple M5 Pro, Clang 21.0.0, SDL 3.4.16,
OpenGL 4.1 Metal 91.7. Upstream remains pinned to `d90c929`; the complete patch
stack was applied and generated wrappers rebuilt after the merge. The supported
US XBE SHA-256 was verified before generation:
`3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc`.

MAC-004 now has a Record button for each Cocoa binding slot. The capture box
accepts keys, modifiers, five mouse buttons and both wheel directions. Escape
cancels; Use Escape and Unbind provide explicit alternatives. Reserved and
unsupported inputs show feedback. A conflicting capture offers Cancel or Move
binding, with Cancel as the default. All edits remain drafts until Save; a
binding change does not discard pending sensitivity/inversion edits. Existing
unsupported saved keys remain selected and labeled unavailable on Mac.

Cocoa hardware keys and SDL gameplay input use the same portable virtual-key
mapping. Added punctuation/OEM keys, keypad digits/operators, lock keys and
system-key recognition. Windows and Mac share `pc_settings_key_allowed` and
`pc_settings_assign`; the latter removes the chosen key from old slots only
after explicit conflict approval. The Windows editor retains its existing
confirmation UI and uses the shared assignment function. Its native runtime
checks remain unavailable on this host.

Validation:

- `python3 tools/build-native-mac.py --generate --test`, followed by the final
  incremental `--test`: three CTests and all native GPU, input, prompt, settings
  and Cocoa capture checks passed. Final executable SHA-256:
  `c9dee18525ec447939bd1ecec5a6bcd0de431489fbb82d1db266bcb432d050ea`.
  `build/macos/BUILD.json` source hashes match the final sources.
- Cocoa responder fixtures cover key/modifier press/release, repeat suppression,
  five mouse buttons, both wheel directions, unsupported/reserved rejection and
  Escape cancellation. Actual modal capture dialogs test initial keyboard focus,
  Return through NSWindow event dispatch, Use Escape, Unbind and Cancel. These
  are application-generated tests, not physical macOS input validation.
- Shared assignment tests cover conflict refusal without mutation, explicit move,
  removing old occurrences, reserved/invalid rejection and unbinding. Disposable
  settings save/reload tests verify punctuation/keypad input reaches the correct
  guest sticks, live prompts update, unrelated JSON and other controllers remain
  intact, backups match, and failed read-only writes preserve active settings.
- MAC-001 native guest-camera fixtures now compare one batch of counts with 100
  individual updates, both pitch limits and stale motion suppression. Existing
  zoom, inversion, focus, capture-click and no-stick-injection checks still pass.
  This extends synthetic adapter coverage; physical mouse and complete live
  pause/settings transition checks remain pending.
- Relevant Python suites: 28 passed, two Windows-only checks skipped, including
  patch fresh/repeated/historical-upgrade, interpolation declarations, config
  preservation, isolated runner and launcher tests. `git diff --check` passed
  with the project's CRLF whitespace convention.

Evidence: `logs/mac-capture-build.log`, `logs/mac-capture-python.log`. A 90-second
isolated probe reached a reviewed rendered menu and dispatched F2 into Settings.
The automation surface did not expose the unbundled game's Cocoa window, so no
visual claim is made for the new editor layout. The runner force-stopped the
open modal after its timeout (93.17 s including shutdown grace, exit -9); no
native fault or sampled GL error was recorded. This probe used the build before
the final unsupported-key label adjustment and is not normal-shutdown evidence.
Its summary/error log are `logs/mac-capture-game-summary.json` and
`logs/mac-capture-game.err`; reviewed guest menu: `private/captures/native-mac/mac-binding-menu.png`.

The pre-build disk check showed 224 GiB available and 158 MiB physically allocated
for `build/macos`. All checks used the existing isolated native paths. Personal
controls retain SHA-256
`cf44293910a77fb8f397ced933a1cfd5098e1949c93db9cc0571ed438b39f64a`; the personal
display file remains absent. Disposable partition images were recycled through
the shared cleanup helper. MAC-004 is Awaiting validation for visual/editor and
physical-device checks (including non-US keyboards). The remaining rendering,
debug, movies, multiplayer and packaging entries stay in the backlog. Actual
texture-pack validation still awaits the user's asset transfer.

A final 35.16-second isolated menu probe used the final executable above and
exited normally at its bound (exit 0), with at least 1800 completed guest frames
and 1993 successful host presents, no sampled GL errors or native fault.
Evidence: `logs/mac-capture-final-summary.json` and `logs/mac-capture-final.err`.
No gameplay or physical-input claim is made from this short startup check.


## Mac debug and performance tools (2026-10-15)

Published the preceding binding/camera work as `e83e074` to `origin/mac-port`.
The debug adaptation below is local and uncommitted over that revision. No main
merge was performed; the Windows computer's checkout remains unverified.

MAC-003 now uses shared `pc_debug.c` statistics, options, formatting and log
classification. Native adapters provide F9, an F12/Cocoa panel, CoreText/OpenGL
text rendering, POSIX log reading and Finder log-folder access. Save & Close
uses the existing atomic controls-JSON writer and backup behavior. Cancel
preserves active values; Clear counters skips old log text. The log reader
handles incomplete lines, truncation and inode replacement, rejects non-regular
files and consumes at most 64 KiB per main-thread poll. Performance samples are
optional and emitted no more than twice per second. Both launch paths override
inherited log paths to their own bounded stderr files.

The overlay is drawn after the scene blit and stays out of guest captures,
screen copies and interpolation history. The Game count is guest presentations;
Shown includes successful interpolated host presents. The panel also displays
their difference. Average guest interval is derived from these counts; neither
counter measures monitor scanout or changes the 60 Hz guest clock.

Validation on macOS 27.0.1 arm64 / Apple M5 Pro, AppleClang 21.0.0, SDL 3.4.16,
OpenGL 4.1 Metal 91.7 / GLSL 4.10, upstream `d90c929` and the complete patch stack:

- Supported US XBE SHA-256 verified before building and the bounded run:
  `3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc`.
- Final `python3 tools/build-native-mac.py --test` passed all three CTests plus
  real-GPU, input, prompts, binding/settings and debug suites. Debug checks cover
  log rotation/truncation/partial lines, classifier exclusion of its own samples,
  counter reset, invalid samples, Cocoa modal Save/Cancel, read-only failure,
  unrelated JSON/other-controller preservation and exact backups. SDL F9 tests
  verify key repeat and release do not toggle a second time.
- Real-GPU overlay checks verify visible text pixels, disabled overlay, restored
  framebuffer/scissor state and unchanged guest scene. Reviewing the rendered
  fixture caught upside-down CoreGraphics text; the blit now corrects row
  orientation. Final fixture `build/macos/path-tests/debug-overlay.png` is upright
  and readable. Its 60/120 numbers are controlled test values, not measured FPS.
- Relevant Python checks: 28 passed, two Windows-only tests skipped. Launcher
  `--check` passed, source hashes match `build/macos/BUILD.json`, and
  `git diff --check` passed. Final executable SHA-256:
  `40f27d687a9a8bfe5ea1538c74a64171e5e161c55929aa122b1f39ba3f82e290`.
- A 45.14-second isolated run completed at its time limit with exit 0, no native
  fault and no sampled GL errors. It dispatched synthetic F9 and F11 toggles,
  including F9 between fullscreen requests. Its profiler recorded 2,237 guest
  and 852 interpolated presents, 68.78 total presents/s over loading/title
  rendering and transitions (49.81 guest + 18.97 interpolated/s), with p95
  interval 25.38 ms. The debug logger produced 88 bounded samples and kept guest
  and shown rates distinct. This is a shortcut/integration smoke test, not a
  steady gameplay or 120 FPS performance certification.
- That integration run used executable `c699b3e482218f050a2749511bd4055c860c3b9faceeb232612a44d6d751223d`,
  before the orientation correction and added F9 regression assertion; final
  GPU/SDL suites above validate those subsequent changes. No additional
  full-level or campaign run was performed.

Evidence: `logs/mac-debug-build.log`, `logs/mac-debug-python.log`,
`logs/mac-debug-native-boot-summary.json`, `logs/mac-debug-native-boot.err`,
`logs/mac-debug-native-frame-times.csv`, and the small rendered fixture above.
The test controls were restored; personal controls retained SHA-256
`cf44293910a77fb8f397ced933a1cfd5098e1949c93db9cc0571ed438b39f64a`;
personal display settings remained absent, and personal saves were unused.
Both isolated test roots have no remaining disposable partition images.

MAC-003 remains Awaiting validation: physical F9/F12 input, visual Cocoa layout,
Finder action and live fullscreen panel/focus behavior need review. Synthetic
SDL events and application-driven modal tests do not establish physical macOS
event delivery. Windows MSVC/runtime checks remain unavailable on this host.
MAC-011 still awaits the user's real texture pack; no artwork was substituted.

## Mac dependent texture modes (2026-10-15)

Pushed the debug adaptation to GitHub on `mac-port` as `149939f` and verified the
local and remote-tracking revisions match. The graphics changes below remain
local and uncommitted over that revision. No main merge or other-device checkout
synchronization was performed.

MAC-005 now evaluates dependent AR/GB texture lookups and DOTPRODUCT/DOT_ST
in stage order instead of returning zero for those modes. Source-stage selection
and invalid-source fallback match the PC renderer. Signed byte mappings 1-3
match its formulas; mapping 0 and the PC's HILO fallback remain unsigned.
Dependent reads retain linear texture scaling, orientation and existing sampler
state. Unsupported stages return zero. Non-combiner draws still sample only the
first texture. Cube/volume/reflection, depth-writing DOT_ZW, bump and other
advanced modes are not included in this change.

Files: `src/native/gl_fragment.inc`, `renderer_gl.c`,
`multitexture_selftest.inc`, and `dependent_texture_selftest.inc`. The bounded
per-draw trace now includes texture modes, dot mapping and source registers for
both GPU and CPU vertex paths, making future scene-specific gaps inspectable.
No generated game C, dependencies or Windows rendering source were changed.

Validation on macOS 27.0.1 arm64 / Apple M5 Pro, AppleClang 21.0.0, SDL 3.4.16,
OpenGL 4.1 Metal 91.7, upstream `d90c929` and the complete local patch stack:

- Supported US XBE SHA-256 verified before builds/probes:
  `3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc`.
  Free space before builds was 223 GiB; no asset copies/downloads were needed.
- Final `python3 tools/build-native-mac.py --test` passed the three platform
  CTests and native renderer, input, prompts, settings/capture and debug suites.
  Thirty-six new real-GPU cases exercise fixed vertices, GPU vertex programs
  and the CPU vertex fallback. Distinct quadrant colors test AR versus GB,
  chained stage inputs, invalid source fallback, illegal stage placement,
  signed mappings 0-3, unsigned HILO fallback and linear DOT_ST coordinates.
  Existing alpha, scissor, screen-copy, enlarged-target, depth/interpolation and
  resource regressions also pass. A mistaken chained-color expectation was
  corrected during fixture development; the final suite passes all cases.
- Source hashes match `build/macos/BUILD.json`; launcher `--check` and
  `git diff --check` pass. Final executable SHA-256:
  `bf612a1531ee9e919baf39b43c49fb4aff29eaaa2f3451db3dc4906488842784`.
- Bounded isolated probes of 150.18 s and 120.12 s reached the briefing and
  mission opening respectively, then exited at their time limits with code 0.
  The isolated existing-profile overwrite prompt required explicit navigation;
  personal saves were never used. A final 300.19 s run rendered the mission
  opening (including cockpit/terrain imagery) and its pause menu, then exited
  cleanly with no native fault or sampled GL errors. It logged at least 14,100
  completed guest frames and 21,376 successful host presents.
- The final run's bounded profiler reached its 20,000-row limit at 277.94 s:
  13,261 guest and 6,739 interpolated presents, 71.96 total presents/s over the
  recorded menu/loading/cinematic interval, p95 26.35 ms. This includes a traced
  frame with per-draw readbacks, so it is not a steady gameplay benchmark or a
  sustained 120 FPS result. Guest simulation timing was unchanged.
- Runtime probes used executable
  `6ed302ec40ab05df7afae79ba80c7cf2e59f7e38b63479b8b72bf69912a5bf82`.
  The final rebuild only moved diagnostic mode fields from CPU-vertex-only
  output into the common trace; shaders are unchanged and the full native suite
  passed afterward. The captured GPU-path trace predates that correction, so
  it does not prove these new modes were used by the reviewed mission imagery.

Evidence: `logs/mac-dependent-build.log`, `logs/mac-dependent-menu-summary.json`,
`logs/mac-dependent-intro-summary.json`, `logs/mac-dependent-native-boot-summary.json`,
`logs/mac-dependent-native-boot.err`, `logs/mac-dependent-native-frame-times.csv`,
and `private/captures/native-mac/graphics-review.png` (final pause-menu capture).
Both test roots have no remaining disposable partition images. Personal controls
retained SHA-256 `cf44293910a77fb8f397ced933a1cfd5098e1949c93db9cc0571ed438b39f64a`;
personal display settings remained absent and personal saves were untouched.

MAC-005 remains In progress. These runs establish startup/mission-opening and
pause-menu regression evidence, not a sustained gameplay segment, matched PC
material fidelity, completion of the level or resolution of the reported
world-disappearance route. Physical input and Windows execution were not tested.
Cube/volume resources (MAC-006), further screen-copy/title-effect coverage
(MAC-007), movies and the real texture pack remain separate backlog work.

## 2026-10-15 — Windows release patch preparation

Local, uncommitted change on `mac-port` base `e83e074`; release patch inputs
were read from local `main` revision `5cf4866`. Host: Windows 11 x64
10.0.26300, Python 3.14.2, Git 2.47.0.windows.2. The native boot overlay guard
invalidated the Windows watermark patch's original surrounding context.
The loader now requires one adjacent context line for that specific patch
when native boot is present; added/removed code must still match.

`python -m pytest tests/test_patch_updates.py -q`: 11 passed. Added regression
cases cover fresh application, an existing watermark, repeated application,
and rejection of conflicting watermark code without writes. Separate temporary
source-only fixtures using all patches from main reproduced the original error
before the fix and passed afterward for fresh upstream sources and a copy of
the existing Windows runtime (34 and 28 files updated respectively); a second
application changed nothing. The actual runtime was not modified. No game
build, packaging, publishing, or gameplay test was needed or performed for
this Python loader fix. No game data or personal settings were used.

MAC-014 tracks pending macOS validation. Changes have not been committed,
pushed, or merged to main; synchronization with the other device is unverified.

## 2026-10-15 — Windows texture-dump compilation fix

Reported during release preparation on main `38f1a2f`. Fix is local and
uncommitted on `mac-port` base `724b1a1`: restore the missing `FILE *f`
declaration in `src/texture_pack.c::dump_original`. No runtime patch or generated
game source changes are required. MAC-011 records the platform impact.

On Windows x64, the installed VS 2022 BuildTools MSVC compiler accepted the
corrected translation unit with `/Zs /TC` and the Windows renderer include paths
and feature defines. The existing `tsfp_texture_format_test.exe` passed DDS
validation checks; that executable was not rebuilt and does not exercise the
dump function. Texture-tool Python tests could not collect because Pillow is
not installed in this virtual environment. No dependency pins were changed.
No full game build, packaging, game launch or macOS validation was performed;
personal settings and saves were unused. Other-device synchronization remains
unverified. This fix must reach main before release preparation can use it.

Patch validation: `python -m pytest tests/test_patch_updates.py -q` passed all
11 tests on Python 3.14.2. `git diff --check` passed.
