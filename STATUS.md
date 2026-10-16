# Measured progress

Tested on Windows and Apple Silicon macOS on October 14-15, 2026. This is an experimental port bring-up;
full gameplay and campaign completion have not been verified.




## Windows launcher v1.1.2 release preparation - October 16, 2026

The launcher waits for the installed game's actual window before minimizing.
It restores its previous normal/maximized state on exit. Early startup failure
leaves the launcher visible; returning to it during play does not re-minimize it.
Executable-path matching ignores windows from other installations. The banner,
blue/silver theme, icon and release-only updating remain intact.

Windows x64/.NET Framework 4.8 WPF build, three synthetic lifecycle checks,
two packaging tests, embedded artwork and compiled ZIP validation passed.
The broader updater suite was attempted but real game processes are running,
so its safety guard stopped installation tests. The unchanged updater passed
71 regressions in v1.1.1; this is prior evidence, not a fresh v1.1.2 full-suite pass.
No game was started/stopped for this release; personal files were preserved.
Lifecycle test source is included; Mac lifecycle adaptation remains MAC-014.

The source tag excludes unrelated uncommitted runtime/texture work and preserves
the checked-out branch, HEAD and real Git index.


## Windows launcher v1.1.1 release preparation - October 16, 2026

This release embeds the generated Cortez/time-portal banner and original title
lettering, with blue steel panels, silver text and icy blue highlights. A compact
silver TS/blue portal icon replaces the miniature wide title logo. Both launcher
executables contain the multi-resolution icon; no extra runtime image is needed.
The banner is an unchanged 2032 x 774 generated PNG. Raw artwork remains private;
builds without it retain the vector/text fallback. Icon source is reproducible.

Windows x64/.NET Framework 4.8 WPF: 71 isolated launcher regressions and two
packaging tests passed. The compiled ZIP verifies and installs in isolation.
Native startup and icon appearance were inspected, and all seven icon sizes
were reviewed. No game run was needed; game files and personal controls were
preserved. Mac icon/UI adaptation remains queued as MAC-014.

The source tag excludes unrelated uncommitted texture changes. Publication
preserves the checked-out branch, HEAD and real Git index.


## Launcher v1.1.0 release preparation

- Release tag: `launcher-v1.1.0`. This source snapshot includes the launcher,
  its build and packaging tools, isolated tests and the MAC-014 handoff.
- The Windows launcher-only ZIP, checksum and `launcher-update.json` are the
  publication assets. They contain no game data or personal files.
- 71 launcher regressions, two packaging tests and native bootstrap-to-main
  startup passed. Launcher and game update channels remain independent.
- The source snapshot excludes unrelated uncommitted texture work. Release
  publication does not merge branches or change the checked-out branch/index.

## Launcher self-update before opening — October 15, 2026

- Launcher v1.1.0 now starts through a separate 64 KiB WPF bootstrap. A themed
  splash checks published launcher-only assets before opening the main UI, with
  an eight-second check deadline, a two-minute download limit and an Open
  installed launcher action. Closing the splash cancels startup.
- Launcher-only versions, packages and manifests are independent of game
  releases. The build generates the versioned ZIP, SHA-256 sidecar and
  launcher-update.json. The ZIP contains the bootstrap, Bin/TimeSplitters.App.exe
  and Support/Launcher/manifest.json. Full game packages also include them.
- A separate self-update helper waits for the bootstrap to close and verifies
  staged hashes before replacement. Self-update recovery/backups are separate
  from the game's recovery/backups. Game update/rollback preserves a newer
  independently installed launcher. Neither path changes personal data/settings.
- Game release discovery now filters the published release list, so a newer
  launcher-only GitHub Release cannot hide the latest matching game package.
- Windows x64 .NET Framework 4.8/WPF: 71 isolated launcher regressions and two
  synthetic packaging tests passed. New checks cover independent versions,
  channel filtering, metadata agreement, an OS request ignoring cancellation,
  continue-offline cancellation, narrow extraction, changed staged files,
  separate-process replacement, failed/interrupted self-updates, independent
  game rollback preservation, and the generated compiled launcher ZIP itself.
- Native startup displayed the splash, closed its bootstrap process and opened
  the main application's normal window with launcher v1.1.0 in the footer.
  Main UI/accessibility and both offscreen renders passed. No game was launched
  or extracted, and no personal saves/settings were used.
- Live launcher checks found no launcher-only release in either channel. The
  generated assets still need publication to enable a real remote self-update;
  no release was published and no Git changes were committed/pushed. Remote
  download/update paths were exercised with controlled synthetic responses.
- Source remains local/uncommitted in the existing Windows checkout at f847832.
  See tools/launcher/Bootstrap.cs, SelfUpdate.cs, LauncherVersion.cs,
  docs/windows-launcher.md and the MAC-014 handoff. Mac execution is unverified.

## Windows GitHub Release launcher — October 15, 2026

- Added a native Windows x64 WPF launcher with a TimeSplitters temporal-scanner
  theme, local US Xbox ISO/folder import, game launch, release notes, logs and
  save-folder access. Users need no Git, Python installation or build tools.
- Stable checks only published non-prereleases; Preview is an explicit opt-in.
  Startup checks are automatic; installing requires a click. Branch pushes do
  not trigger updates. Downloads verify GitHub's SHA-256 or the release sidecar,
  then every file in BUILD.json. An external helper replaces the launcher itself.
- Safe extraction, protected user directories, per-installation locking, game
  process checks, backup-before-write installation, interrupted-install recovery
  and previous-release restore are implemented. Existing controls/display,
  saves, data and optional textures are retained. One rollback copy is kept.
- Validation on Windows x64: 45 isolated C# regressions and two synthetic
  packaging tests passed. They cover release selection/version ordering,
  checksums, unsafe/duplicate/protected ZIP paths, rollback and injected failures,
  crash recovery, settings preservation, a separate updater process with spaced
  paths, malformed ISO rejection and checksum-sidecar fallback.
- Live GitHub discovery reported no Stable release and Preview v0.2.0-alpha.2.
  Its 96,663,763-byte ZIP passed SHA-256 and all 1,272 manifest file checks, then
  installed in isolation through the helper without executing downloaded code.
  The temporary runtime and archive were removed; no game data was copied.
- The actual WPF window and accessible controls were inspected. Channel switching
  discovered the live Preview release; ISO picker open/cancel passed. An
  illustrative installed-state render is in build/launcher/preview.png. No game
  launch, new real ISO extraction, campaign test or Mac validation was performed.
- Source is local/uncommitted, based on the existing Windows texture checkout;
  no branch switch, commit, push or public release was made. Existing texture
  work remains in place. Packaging builds/includes the launcher, while the
  checkout's developer Play.cmd remains intact. Mac adaptation is MAC-014.


## Texture branch integration of Mac PR #10 — October 15

- Integrated merged main `fc25ef3` into the texture branch, retaining F10 cached
  Classic/4K switching, shared DDS reading and the Mac camera/menu/input work.
  Restored the DDS-dump file handle declaration missing from the shared-I/O
  refactor. Existing patch files remain intact; the upgrade fixture skips both
  the texture-toggle and later native patches until the full-stack update.
- Forty-seven shared/Python/compiler checks passed (45 host checks plus both
  compiled guest-network dispatch variants). The Mac Trash allocation check
  was skipped; other native Mac execution remains unavailable on this host.
- Compiled the affected Windows controls, prompt/menu sources and real texture
  loader against counted RHI stubs. All four loader modes passed cached toggles,
  missing/invalid replacements, content changes, original preservation and
  cleanup. The prompt executable passed its F10/menu and guest text assertions,
  but its foreground-controller check could not complete: diagnostics showed
  the test window did not own foreground focus. No product behavior was changed
  to bypass this guard. Full game rebuild and Mac runtime validation are pending.
- Uncommitted texture artwork/tooling work is kept separate from this merge.
  No game launches, personal-save writes or settings changes were required.

## Live Classic / 4K texture toggle — October 15

- F10 switches the cached original/replacement texture views without restarting
  or reloading a level. PC Settings > 4K Textures also switches the mode and
  shows its checkmark. Keyboard repeats are ignored; fullscreen and hidden-menu
  shortcuts work. F10 no longer also cycles the runtime's diagnostic frame cap.
- Play.cmd starts Classic with the installed local pack available; Play-Textures
  starts 4K. Packaged launchers discover an optional Textures folder beside Play.
  This session choice is independent of render resolution. Missing, invalid and
  memory-budget-limited replacements retain Classic; original assets stay intact.
- Windows Release build and all build-time native checks passed. Counted RHI
  loader tests cover cached switches, unchanged original data, missing/invalid
  DDS files, screen copies/render targets, content changes, allocation failures
  and release. Seven fresh/upgrade/idempotence patch-stack tests passed.
- One brief actual-runtime startup smoke check passed F10, held repeats, hidden
  menu, fullscreen, menu selection/checkmarks, one replacement load, clean exit,
  and personal controls/display settings preservation. Evidence is in
  logs/texture-toggle-validation.json and logs/texture-toggle-game.err.
- Full gameplay scene/artwork validation remains pending under the existing
  overhaul cadence. The current partial pack and 512 MiB resident GPU budget
  are unchanged. Disposable validation disk images were recycled after the run.
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
