# PC features awaiting Mac adaptation

This tracks platform exceptions and native validation for shared PC/Mac features.
Ordinary feature logic now belongs in shared source by default; follow
[the shared implementation contract](../development/shared-platform.md). Add/update an
entry when a platform adapter or host validation remains, including behavioral
fixes. Explicitly scoped PC-only work and genuine host dependencies may be queued,
with a reason and acceptance checks. Shared source alone does not verify Mac.

Keep one stable ID per feature (`MAC-001`, `MAC-002`, and so on). Update existing
entries rather than duplicating them, and retain completed entries as history.
Include commit/PR references when available; label local changes uncommitted
until those references exist. Keep paths relative to the repository.

The 2026-10-27 repository organization change renames patch paths and updates
their references without changing payloads, order or platform behavior; no Mac
adapter work is required. Windows validation and native Mac execution limits
are recorded in [STATUS](STATUS.md#repository-organization-2026-10-27).

The broader responsibility-folder change on `cleanup/repository-folders`
updates shared source, test, compiler, build and package paths, including the
native Mac adapters. Final patch 2090 supplies common CMake/compiler routing;
no new platform feature or duplicate adapter is introduced. Windows builds
pass; initial organized-build Story gameplay passed, while final-build gameplay
remains unverified under the recorded host conditions. Native Mac execution
remains pending;
see [measured scope](STATUS.md#repository-responsibility-folders-2026-10-27).
No feature-specific backlog ID is added for this path-only maintenance.

## Integration and ID reconciliation (2026-10-19)

The integration branch `codex/shared-platform-baseline` combines main `5131d29`
with the unmerged native stack ending at `f562e14`. Historical checks below
apply to their stated revisions. The merged native build, regression suite and
bounded first-mission run pass; see [integration evidence](STATUS.md#shared-platform-integration-baseline-2026-10-19).
Windows build/runtime validation remains pending.

Independent branches reused IDs. Current IDs below disambiguate those features;
historical STATUS records retain their original identifiers and dates.

| Feature | Historical ID(s) | Current ID |
| --- | --- | --- |
| Spirit skull and eyes | MAC-026 on main, MAC-032 on native stack | MAC-049 |
| Native build restoration | MAC-033 on native stack | MAC-050 |
| Native build watermark | MAC-034 on native stack | MAC-051 |
| Network bots | MAC-028, then MAC-033 on main | MAC-052 |
| Discord Rich Presence | MAC-048 on main | MAC-053 |
| Fractional presentation / monitor refresh | MAC-049 in PR #188 | MAC-054 |
| Frontend Controls Accept persistence | MAC-050 in PR #188 | MAC-055 |

`codex/renderer-main-integration` depends on the unmerged native renderer stack
at `a0a89f5` and integrates main `4d5b3f0` (PR #188). The two new IDs above
preserve the existing spirit and native-build entries. Historical PC validation
records keep their original IDs. Combined-host acceptance is recorded separately
in [STATUS](STATUS.md#renderer-and-pc-presentation-integration-2026-10-19).

Main's MAC-032 menu backgrounds, MAC-033 transition fades, MAC-034 issue-form
handoff and MAC-048 vehicle joins retain their IDs. MAC-026 remains proportions.

## Audit scope and next work

**Historical native baseline (2026-10-17):** The focused native branch stack was based on main
`d9c145f` (alpha.6). This release-only update preserves the runtime source
reviewed below. The rebuilt native regression suite passes; historical gameplay
evidence keeps its original revision. [Refresh evidence](STATUS.md#alpha6-branch-stack-refresh-2026-10-17).

Reviewed on 2026-10-17 against fetched `main` / `origin/main`
`0dc9fea49041bdca985d372a8e5513d6cac514ac`; no pull was necessary. Compared with
previously reviewed main `d83ca5e`: 133 commits including merges, 134 changed
tracked files. Current game VERSION is `0.2.0-alpha.5`; launcher source is
v1.2.12. Reviewed commit history, changed-file inventory, feature implementations,
native adapter changes, test/build wiring and the full handoff list. This is a
source/parity audit, not complete line-by-line correctness certification.

At the initial audit, baseline Mac evidence dated from October 15–16 rather
than this main revision. The focused native work below now adds current-source
build, GPU/settings/input checks and bounded menu/first-mission evidence.
Most recent PC runtime features already have entries; shared code is not proof
of native rendering/input. This audit adds the missing build-readiness and
watermark handoffs, distinguishes the Cocoa latency-setting regression, restores
omitted queue entries/status, and resolves the duplicate MAC-026 ID as MAC-049.
IDs remain stable otherwise; MAC-017 is unassigned and is not a missing feature.

The initial audit was against `93dc422` on 2026-10-15. Historical implementation
and bounded checks remain in [renderer/camera/prompts](STATUS.md#mac-backlog-renderer-camera-and-prompts-2026-10-15)
and [native settings/graphics](STATUS.md#native-mac-settings-and-graphics-adapters-2026-10-15).
Upstream references mean pinned `d90c929` plus the complete local patch stack.
The other device's checkout, ignored files and current build remain unverified.

Recommended implementation sequence (each task starts on a focused branch from
updated main unless its dependency is explicitly unmerged):

1. **Build baseline:** MAC-050 and MAC-003 setting preservation. Review the
   unmerged MAC-005 implementation at `5cba184` rather than losing or duplicating
   it. Run shared patch/generation and native smoke checks before visual triage.
2. **Repeatable scene access:** MAC-025 Test Lab fixture support in the isolated
   native runner, with immutable fixture and disposable copies. This enables
   later-level checks without a full campaign playthrough.
3. **Shared fixes and native validation:** camera/terminal (MAC-001), pickups
   and sensitivity (MAC-016), audio (MAC-018), screen-copy/clear/refresh effects
   (MAC-007/019), decals/resolution/FOV/cinematic framing/UI/spirits
   (MAC-010/020/022/023/024/026/027/028/032). Finish the Cocoa FOV slider and
   aspect filter within MAC-022/024. Keep GPU, listening and physical-input
   evidence separate from portable helper checks.
4. **Native interaction adapters:** menu hover/click/wheel/footer/tab handling
   (MAC-030), confirmed desktop exit (MAC-031), remaining binding/prompt/debug
   checks (MAC-002/003/004), and accurate build watermark (MAC-051).
5. **Larger runtime gaps:** remaining dependent/cube/volume texture paths
   (MAC-005/006), movies and idle-demo stop integration (MAC-009/029), native
   audio playback-position accounting (MAC-018), then real texture-pack scenes
   when the existing pack is transferred (MAC-011). Prioritize graphics paths
   with scene evidence; no fix is yet a confirmed diagnosis of world disappearance.
6. **Distribution and multiplayer:** standalone runtime (MAC-012), staged
   release-launcher delivery (MAC-021), and native transport/Online integration
   (MAC-013). These require separate host adapters and acceptance routes.

Shared patch/cleanup checks (MAC-014/015) accompany relevant stages. Existing
scissor verification (MAC-008) is historical and limited to its supported
rectangle contract; rerun affected GPU checks after renderer changes.

### Coverage of work since the previous review

| PC additions / implementation commits | Mac handoff and current boundary |
| --- | --- |
| Terminal pointer/click `a60deb5` | MAC-001: SDL adapter already updated; physical terminal test pending |
| Decal alignment `a28a8b6`; model proportions `6485069` | MAC-010/026: shared hooks; native scene validation pending |
| FOV `bf31b24`, falling `df3bc2e`, frontend exclusion `ad2f09f` | MAC-022/028: shared camera/menu; Cocoa slider implemented/tested; gameplay matrix pending |
| Objectives `5da8cf6`; aspect/HUD/tutorial layout `2d69459`; rank `3e747a5` | MAC-023/024/027: shared rules plus native viewport/scissor changes; Cocoa aspect filter tested; full visual matrix pending |
| Channel clears `f17026b`; spirits `70d8e7f` | MAC-007/032: shared/native clear wiring and shared spirit wrappers; GPU checks pending |
| Idle movie stop `c797b65` | MAC-029: shared stop/symbol corrections; movie player still MAC-009 |
| Frontend/pause mouse and desktop exit `e18baa9` | MAC-030/031: native pointer and quit adapters implemented; acceptance evidence in their entries |
| Test Lab `0c69834` | MAC-025: portable save preparation; native runner integrated; late Story selection checked, Arcade/challenge checks pending |
| Patch/CI efficiency `4c5848d`; test linkage/portability fixes | MAC-014 and affected entries: shared tooling; source tests do not boot the game |
| Launcher v1.2.1–v1.2.12: startup skip, embedded settings, capability gates, cleanup/artwork, repair/diagnostics/reporting, resumable downloads, release news and tabs | MAC-021: detailed commit references and acceptance below; native launcher queued |
| Release comparison `3ed4223`, packaging/version/provenance and runtime-settings preservation tests | MAC-012/021: common release rules; Windows package/UI checks do not verify Mac distribution |
| Guided issue forms `4d70d96`, automation `6c5ee41`, simplified requests, release-choice refreshes; copy/line-ending fixes | Repository/GitHub behavior requires no Mac game adapter. Future native diagnostic reporting uses MAC-021; no live GitHub automation or Mac UI verification claimed |

[Measured local review and portable checks](STATUS.md#mac-current-main-backlog-audit-2026-10-17).

## Status meanings

- **Queued:** PC feature recorded; Mac adaptation is pending.
- **In progress:** Mac implementation is underway.
- **Awaiting validation:** Implementation is present but relevant Mac checks remain.
- **Verified:** Mac acceptance checks passed, with evidence linked below.
- **Not applicable:** No corresponding Mac capability is needed; explain why.

If shared code already supplies the feature, record that fact and use awaiting
validation until Mac checks pass. Do not infer Mac support from Windows results.

## Queue

| ID | Feature | Status |
| --- | --- | --- |
| MAC-001 | Direct-camera mouse look | Awaiting validation |
| MAC-002 | Keyboard/mouse prompts and device switching | Awaiting validation |
| MAC-003 | Debug settings and on-screen performance information | Awaiting validation |
| MAC-004 | Capture-to-bind and conflict reassignment | Awaiting validation |
| MAC-005 | Multiple texture stages and shader fidelity | In progress |
| MAC-006 | Cube and volume texture resources | In progress |
| MAC-007 | Screen-copy effects and image surfaces | In progress |
| MAC-008 | Scissor clipping | Verified |
| MAC-009 | Intro and in-game movie playback | Queued |
| MAC-010 | In-game resolution preview and rollback | Awaiting validation |
| MAC-011 | Real texture-pack gameplay coverage | Awaiting validation |
| MAC-012 | Standalone Mac runtime distribution | Verified |
| MAC-013 | System Link and Online multiplayer | Queued |
| MAC-014 | Patch upgrades across Windows watermark and native overlay guards | Verified |
| MAC-015 | Release test privileges and disposable cleanup | Awaiting validation |
| MAC-016 | Shared pickup-range comparison fix (issue #6) | Awaiting validation |
| MAC-018 | Story audio timing, queue retry and ADPCM resume (issue #8) | Awaiting validation |
| MAC-019 | Framebuffer refresh between effects (issue #9) | Awaiting validation |
| MAC-020 | Adaptive cutscene letterboxing (issue #33) | Awaiting validation |
| MAC-021 | Published-release launcher and pre-game settings | Awaiting validation |
| MAC-022 | Configurable gameplay FOV (issue #29) | Awaiting validation |
| MAC-023 | Story Objectives health/shield (issue #11) | Awaiting validation |
| MAC-024 | Resolution catalog and proportional UI (issues #27/#31) | Awaiting validation |
| MAC-025 | Reusable unlocked testing profile | Awaiting validation |
| MAC-026 | Character picker and weapon proportions (issue #22) | Awaiting validation |
| MAC-027 | Arcade RANK widget left placement | Awaiting validation |
| MAC-028 | Preserve frontend camera FOV (#82) | Awaiting validation |
| MAC-029 | Skippable idle demo video (#50) | Awaiting validation |
| MAC-030 | Menu mouse selection (#86) | Awaiting validation |
| MAC-031 | Quit to Desktop menu entries | Awaiting validation |
| MAC-032 | Responsive menu backgrounds and scope masks | Awaiting validation |
| MAC-033 | Full-display transition fades | Awaiting validation |
| MAC-034 | Plain improvement form handoff | Not applicable |
| MAC-035 | Cinematic width fill (issue #30 investigation) | Awaiting validation |
| MAC-036 | Ghost Gun full-body visibility (issue #111) | Queued |
| MAC-037 | Fresh-install launch without PowerShell (issue #128) | Not applicable |
| MAC-039 | Recoverable launcher startup security interruptions | Not applicable |
| MAC-040 | Dedicated game icon | Queued |
| MAC-041 | Published game version picker and banner replacement | Queued |
| MAC-042 | Formatted release notes and browser links | Queued |
| MAC-043 | Automatic mouse Auto Lookahead override | Awaiting validation |
| MAC-044 | Game crash capture and launcher reports | Awaiting validation |
| MAC-045 | Terminal camera depth clears (issue #47) | Awaiting validation |
| MAC-046 | Update transaction rename contention | Queued |
| MAC-047 | Main-menu image and lettering fidelity | Awaiting validation |
| MAC-048 | Companion vehicle zero-flag joins (#100) | Awaiting validation |
| MAC-049 | Spirit skull and eye rendering (#17) | Awaiting validation |
| MAC-050 | Restore a reproducible native build from current main | Verified |
| MAC-051 | Version and build-identity watermark | Awaiting validation |
| MAC-052 | Experimental network bot setup (issue #25) | Queued |
| MAC-053 | Discord Rich Presence (issue #63) | Awaiting validation |
| MAC-054 | Fractional presentation targets and Windows transform setup (issue #167) | Awaiting validation |
| MAC-055 | Persist accepted frontend Controls before quitting Settings | Awaiting validation |
| MAC-056 | Texture-camera composition alignment (issue #49) | Awaiting validation |
| MAC-057 | Optional texture pack delivery through the launcher | Awaiting validation |
| MAC-058 | Crane and remote-view vignette sizing (issue #221) | Awaiting validation |
| MAC-059 | Physical keyboard entry in original text dialogs (issue #136) | Awaiting validation |
| MAC-060 | Native fractional updates at 60/120/144 FPS | Awaiting validation |
| MAC-061 | Host save-drive metadata and inline drive selection | Awaiting validation |
| MAC-062 | Stable and named game release channels | Awaiting validation |
| MAC-063 | Projected enemy health bars (issue #102) | Awaiting validation |
| MAC-064 | Self-contained runtime source | Awaiting validation |
| MAC-065 | Independent game-disc import | Awaiting validation |
| MAC-066 | Independent executable analysis | Awaiting validation |
| MAC-067 | Independent disassembler input | Awaiting validation |
| MAC-068 | Independent instruction traversal | Awaiting validation |
| MAC-069 | Independent table and body recovery | Awaiting validation |
| MAC-070 | Independent function discovery and metadata | Awaiting validation |
| MAC-071 | Independent analysis runner and publication | Awaiting validation |
| MAC-072 | Independent symbol adapter and pipeline driver | Awaiting validation |
| MAC-073 | Independent function catalog | Awaiting validation |
| MAC-074 | Independent generation support | Awaiting validation |
| MAC-075 | Independent translator decoding | Awaiting validation |
| MAC-076 | Independent translation layout and hook planning | Awaiting validation |
| MAC-077 | Independent branch feedback and unresolved diagnostics | Awaiting validation |
| MAC-078 | Independent C operand emission | Awaiting validation |
| MAC-079 | Independent data-transfer and stack lowering | Awaiting validation |
| MAC-080 | Independent scalar integer lowering | Awaiting validation |
| MAC-081 | Independent shift and rotate lowering | Awaiting validation |
| MAC-082 | Projected enemy shield effects (issue #358) | Awaiting validation |
| MAC-083 | Independent bit tests, scans and byte swaps | Awaiting validation |
| MAC-084 | Independent multiply and divide lowering | Awaiting validation |
| MAC-085 | Shared impact spark timing (issue #371) | Awaiting validation |
| MAC-086 | Independent exchange lowering and atomic helpers | Awaiting validation |
| MAC-087 | Independent comparison snapshots and sign extension | Awaiting validation |
| MAC-088 | Independent string-instruction lowering | Awaiting validation |
| MAC-089 | Independent condition-code generation | Awaiting validation |
| MAC-090 | Independent SSE emission | Awaiting validation |
| MAC-091 | Independent MMX emission | Awaiting validation |
| MAC-092 | Independent x87 emission | Awaiting validation |
| MAC-093 | Independent control-transfer emission | Awaiting validation |
| MAC-094 | Independent instruction and block lifting | Awaiting validation |
| MAC-095 | Independent lifter context and helper recognition | Awaiting validation |
| MAC-096 | Independent batch translation and dispatch output | Awaiting validation |
| MAC-097 | Independent translator context and entry discovery | Awaiting validation |
| MAC-098 | Independent complete function emission | Awaiting validation |
| MAC-099 | Independent translation command line | Awaiting validation |
| MAC-100 | Project guest register and memory primitives | Awaiting validation |
| MAC-101 | Project indirect guest-call dispatch | Awaiting validation |
| MAC-102 | Project call checks and HLE argument frames | Awaiting validation |
| MAC-103 | Project vector constructor and obsolete SIMD helper retirement | Awaiting validation |
| MAC-104 | Direct scalar extension and numeric helper retirement | Awaiting validation |
| MAC-105 | Canonical project guest execution header | Awaiting validation |
| MAC-106 | Project branch-feedback provider and generated header | Awaiting validation |
| MAC-107 | Project native continuation service | Awaiting validation |
| MAC-108 | Project guest state and ABI diagnostics | Awaiting validation |
| MAC-109 | Project native image loading | Awaiting validation |
| MAC-110 | Project guest allocation ownership | Awaiting validation |
| MAC-111 | Project guest thread storage | Awaiting validation |
| MAC-112 | Project virtual-memory services | Awaiting validation |
| MAC-113 | Project native memory ownership | Awaiting validation |
| MAC-114 | Project guest memory services | Awaiting validation |

### MAC-048 - Companion vehicle zero-flag joins (#100)

- **Status:** Awaiting validation
- **Behavior:** Correct the reversed stay/dismount decision that makes Captain
  Ash repeatedly leave and re-enter the jeep without an enemy target. Preserve
  the original driver, vehicle-mode and combat checks.
- **Source:** `100-captain-ash-buggin-out-in-the-car`;
  `patches/compiler/0310-recomp-zf-join-polarity.patch`, `tests/compiler/test_recomp_vehicle_joins.py`
  and `tests/gameplay/vehicle_companion.c`. Windows Test and runtime CI include the tests.
- **Mac support:** Shared Python recompiler and patch stack; no OS adapter is
  required. Existing generation fingerprinting invalidates old generated C.
  Windows results do not establish native macOS gameplay correctness.
- **Remaining:** Regenerate/rebuild on native macOS, run source-only ZF tests and
  the supported-XBE mounted-AI regression, then validate the encounter.
- **Acceptance:** Ash remains seated with a player driving away from enemies,
  still fights and can dismount appropriately. Check Harry Tipper separately
  before claiming that encounter verified. See `docs/project/STATUS.md` for Windows evidence.

### MAC-047 — Main-menu image and lettering fidelity

- **Status:** Awaiting validation
- **PC behavior:** Windows frontend textures use clamped Catmull-Rom
  reconstruction during magnification. Verified main-menu row draws additionally
  reconstruct A8 glyph coverage without changing glyph placement or measurement.
  Native-size/minified samples, point filtering, framebuffer copies, render
  targets and gameplay retain their existing paths. `TSFP_MENU_FILTER=linear`
  restores previous sampling for an individual process.
- **Source:** Commit titled `Improve main-menu image and lettering fidelity` on
  `fix/main-menu-image-fidelity`;
  `src/interface/pc_menu_sampling.h`, `src/interface/pc_display.c`, `src/interface/pc_display_ui.inc`,
  the `menu-sampling.patch`, and `tests/graphics/menu_sampling_gpu.c`.
- **Mac support:** The frontend/glyph scope and per-present reset are shared.
  The native OpenGL renderer does not use the Windows HLSL helper, so its
  current sampling is retained. Native execution is unverified.
- **Remaining:** Adapt reconstruction and glyph coverage to the native GLSL
  sampling paths, retaining wrap/clamp, alpha, texture dimensions and minification.
  Reuse the shared frontend/row scope; exclude framebuffer and depth effects.
- **Acceptance:** GPU comparison at native size and during minification is
  unchanged; magnified menu background, logo, glyphs and icons sharpen without
  ringing, atlas bleed, opacity drift or changes to gameplay. Check 1080p and
  ultrawide captures, font measurements, fades and menu/gameplay transitions.
  Windows evidence is recorded in `docs/project/STATUS.md`; it is not native Mac validation.

- **Shared implementation (2026-10-19):** `codex/shared-menu-rendering`, based
  on unmerged integration `bee4ed9`, moves the reconstruction and glyph coverage
  algorithm into `src/graphics/menu_sampling_shader.h`. Windows HLSL and native
  GLSL consume the same body; adapters map texture access and intrinsic syntax.
  Texture eligibility is also shared. Mac binding tests cover glyph scope and
  point/render-target/screen-copy/dependent exclusions. An asset-free OpenGL
  test runs the same pixel contract as Windows D3D11 WARP; CI selects it on both
  hosts. Native full regressions and menu/mission-opening captures pass.
  Windows execution after extraction, the aspect/resolution matrix, font
  measurements and post-change gameplay transitions remain pending.
  [Measured evidence](STATUS.md#shared-menu-shader-2026-10-19).

### MAC-045 - Terminal camera depth clears (issue #47)

- **Status:** Awaiting validation
- **PC behavior:** The terminal camera's smaller depth attachment receives clears
  issued to the same guest depth image on the main target. This addresses the
  blank What Lies Below camera feed without clearing on every target switch.
- **Source:** `47-what-lies-below-camera-visual-bug` branch; `terminal-depth-clear` additive runtime patch,
  `tests/graphics/terminal_depth.c`, `tests/graphics/test_terminal_depth.py`, and build/CI wiring.
- **Mac support:** The fix uses shared HLE clear/cache logic and existing
  `host_Clear` / depth-surface APIs; no new OS or D3D11-only call is introduced.
  The native GL renderer already provides those APIs. Windows regression/build
  evidence is recorded in docs/project/STATUS.md; native execution remains unverified.
- **Remaining:** Run the native compiler regression and exercise the terminal
  cameras on Apple Silicon. No separate implementation is currently expected.
- **Acceptance:** Containment Area A and Laboratory Entrance feeds show their
  rooms over multiple frames; changing cameras, opening doors and leaving the
  terminal work. Colour-only clears and repeated target binds preserve depth;
  independent guest images do not share cached depth. Recheck humanoid ghosts
  and ordinary scene occlusion on the existing shared-depth path.

### MAC-046 - Update transaction rename contention

- **Status:** Queued
- **PC behavior:** Retry Windows access/sharing/lock failures when promoting a
  completed game, repair or launcher transaction to rollback storage. Retries
  only the rename, bounded to five seconds; permanent failures still restore
  previous files. Recovery failures retain both original and recovery errors.
- **Source:** `tools/launcher/Core.cs`, `tests/launcher/launcher_updates.cs`; commit titled
  `Retry blocked launcher transaction commits` on `fix/launcher-transaction-lock`.
- **Mac support:** Native Mac does not use this Windows C# updater. No existing
  Mac behavior changes; release-updater parity remains queued under MAC-021.
- **Remaining:** When implementing a native updater, use native filesystem error
  handling and bounded contention recovery with preserved transaction journals.
- **Acceptance:** A transient lock completes without replaying writes; a
  persistent lock fails with recovery instructions; unrelated errors fail
  immediately; recovery remains possible after unlocking and personal files
  remain untouched. Windows checks do not establish Mac execution. See docs/project/STATUS.md.

### MAC-044 - Game crash capture and launcher reports

- **Status:** Awaiting validation
- **Launcher release preparation:** v1.2.16 packages the Windows report UI and
  unfinished-session recovery from merged PR #160. Native crash/hang capture
  still requires a separate game update. The version bump adds no Mac adapter;
  the remaining work and acceptance checks below are unchanged.
- **PC behavior:** Windows unhandled-exception capture through a prestarted helper;
  bounded minidump, cached Xbox registers, executable hash and log tail. Launcher
  detects new reports, opens Diagnostics and offers local open/export actions.
  Matching executable/PDB archives are separate from player packages. The same
  helper now saves a suspected-hang report after 60 seconds without frame
  progress (180 seconds before the first frame), with modal/background/minimized
  suppression and sleep/resume grace. One report per stall; frame recovery
  rearms it. The game is left running and the launcher offers live open/export.
  Launcher-started sessions now persist an atomic completion record, infer
  abandoned sessions after checking PID/start-time identity, and create local
  unexpected-exit summaries when no matching native fatal report exists.
- **Source:** [PR #160](https://github.com/TheDoffman/timesplitters-future-perfect-pc/pull/160),
  implementation commit `20058fa` on `feature/windows-crash-logger`; `src/pc_crash.*`,
  `tools/build/crash_helper.c`, the `windows-crash-logger` patch,
  `tools/launcher/CrashReports.cs`, `Launcher.cs`, `MaintenancePage.cs`,
  `tools/release/crash-symbols.py`, `tools/testing/test-windows-crash.py`, `src/diagnostics/pc_hang_policy.h`,
  `src/diagnostics/pc_debug.c`, `src/input/pc_controls.c`, the `windows-hang-watchdog` patch,
  `tools/testing/test-windows-hang.py` and `tests/diagnostics/pc_hang_policy.c`.
  Session follow-up `3222e52` in the same PR: `tools/launcher/GameSessions.cs`,
  `tests/launcher/launcher_sessions.cs`, and launcher report/lifecycle integration.
  Review follow-up in this PR: concurrent fatal threads wait for capture,
  report/export temporary names fit valid Windows paths, selected build
  configurations retain their own symbols, and source-only CI covers these cases.
- **Native implementation (2026-10-19, `356e5a3` on `codex/mac-launcher-parity`):**
  `src/native/crash_posix.*`, native main/input hooks and the separate
  `native-launcher-services` patch implement signal-safe shared-memory notification
  to a prestarted reporter, ARM64/Xbox context, symbolication, bounded native
  samples and log tails. The helper reuses `pc_hang_policy.h` with independent
  foreground observation, modal suppression, sleep grace and recovery rearming.
  `CrashReports.cs` and native Diagnostics share discovery/export/retention;
  `GameSessions.cs` shares durable session recovery. Native builds retain dSYMs
  and UUID/hash provenance. Native context replaces the OS-specific minidump.
- **Evidence/remaining:** Actual crash and clean game exits, report export,
  launcher restoration, synthetic hang/recovery/focus/modal/normal tests and
  linked-path rejection pass. See [STATUS](STATUS.md#native-launcher-parity-completion-2026-10-19).
  Clean-machine symbol/report delivery and a Windows host rerun remain unverified.
- **Acceptance:** Isolated main/worker crash resolves against the exact Mach-O and
  dSYM; expected MMIO signals and normal exit create no crash reports; bounded
  storage, missing reporter/unwritable destination, log privacy, and native UI.
  Validate deliberate freeze/recovery/repeated stalls and a later crash, exact
  thread symbols, no false reports in responsive/paused/modal/background or
  sleep/resume cases, and export while the game stays alive.
  Confirm clean/failed launches do not flag, an interrupted launcher does not
  flag a still-running game, PID reuse cannot hide an abandoned run, and recovery
  creates one bounded report with an unknown cause/code where unobserved.
  Include simultaneous fatal faults and native filesystem path limits in adapter
  validation; retain the symbols for the actual selected build configuration.
  Windows checks do not establish native Mac behavior. See
  [Windows design and checks](../players/windows-crash-logger.md); measured Windows
  results are recorded in `docs/project/STATUS.md`.

- **Breadcrumb/health follow-up (2026-10-24):** Both capture adapters now use
  `pc_breadcrumbs.h` for bounded numeric history and `pc_capture_health.h` for
  session-scoped helper heartbeats. Both launcher adapters pass the session token;
  the shared Diagnostics model checks storage, freshness and process identity.
  Shared UI and Darwin capture tests cover these paths in CI. Windows native
  build/report validation is local; native Mac gameplay remains unverified.
  No separate duplicated gameplay policy or new deferred Mac adapter is added.

### MAC-043 — Automatic mouse Auto Lookahead override

- **Status:** Awaiting validation
- **Behavior:** Keyboard/mouse with mouse aiming enabled suppresses pitch
  recentering for local input slot zero. Controls displays OFF without arrows
  in both frontend and pause menus. The saved controller preference, other
  players, disabled-mouse mode, and other settings retain their original behavior.
- **Source:** `Disable Auto Lookahead for keyboard and mouse` on
  `fix/mouse-auto-look-ahead`,
  `src/gameplay/pc_auto_look_guest.inc`, the `mouse-auto-look-ahead` runtime patch,
  `tests/gameplay/auto_look.c` and `tests/gameplay/test_auto_look.py`.
- **Mac support:** Shared guest wrappers use existing native prompt-device and
  mouse-enable APIs. No new OS adapter is required; Mac execution is unverified.
- **Shared optimization (2026-10-24):** `fix/native-water-rounding` reuses two
  local permission slots for the pre-call player/settings accesses. Reuse never
  crosses guest execution or input updates. Windows host and controlled map/
  mutation tests pass; shared source needs no new Mac adapter. Native scene
  acceptance below remains pending.
- **Remaining and acceptance:** Run the native regression and build on macOS;
  verify a tilted view stays put while walking with keyboard/mouse, OFF is
  displayed in frontend and paused Controls, controller input restores the
  stored ON/OFF value, and Accept/Cancel and another local player are unaffected.
  Windows evidence belongs in docs/project/STATUS.md; it does not establish Mac validation.

### MAC-042 — Formatted release notes and browser links

- **Status:** Queued
- **PC behavior:** Game versions and What's new use the same selectable native
  Markdown view for headings, emphasis, lists, quotes, code and labeled links.
  HTTP(S) links open through the existing launcher browser action only after
  activation; images/HTML/XAML are inert, other URI schemes are not navigable,
  and existing note limits/known-issue separation remain in force.
- **Source:** `fef30c9` on `fix/launcher-markdown-notes`,
  `tools/launcher/ReleaseNotesMarkdown.cs`, `GameVersionsPage.cs`,
  `WhatsNewPage.cs`, `Launcher.cs`, `tools/build/build-launcher.ps1`,
  `tests/launcher/launcher_news.cs` and `tests/launcher/launcher_versions.cs`.
- **Mac support:** No native release-notes page exists yet; Windows WPF types
  are not compiled into native Mac builds. Existing Mac launch/build paths are unchanged.
- **Remaining:** Reuse these display rules in the future native release center
  and version picker, with a native selectable renderer and explicit safe-link action.
- **Acceptance:** Check original published notes, labeled PR links, selection/copy,
  browser activation and failures, inert images/markup/other URI schemes,
  bounded long notes and scrolling. Windows rendering does not verify Mac UI.
  See [Windows evidence](STATUS.md#formatted-launcher-release-notes-2026-10-18).

### MAC-040 — Dedicated game icon

- **Status:** Queued
- **Publisher dependency follow-up (2026-10-19, this revision on
  `fix/release-python-dependencies`):**
  `tools/release/publish-release.py` refreshes pinned Python requirements before Windows
  builds/tests so older environments include Pillow for icon validation.
  `tests/launcher/test_publish_release.py` checks ordering and installation failure.
  No Mac adapter is applicable to this Windows publishing orchestration;
  native Mac packaging and icon validation remain unverified and queued above.
- **PC behavior:** New Windows game executables embed a seven-size transparent
  ICO. Large sizes use the official TSFP title in a blue/silver temporal frame,
  with the original Future Perfect lettering enlarged on a separate line.
  Small sizes use compact series-inspired serif TSFP initials, with the same monogram
  available at every size without private art. The initials are centered and
  scaled radially to stay inside the rings with clear space around their outline.
  The launcher retains its TS
  icon. SDL's Windows backend uses the executable's first icon for its window
  class; game packages carry it inside the sealed runtime executable.
- **Source:** Icon feature commit `c2db36b` and current-main integration
  `272f930` in [PR #126](https://github.com/TheDoffman/timesplitters-future-perfect-pc/pull/126);
  `tools/launcher/IconBuilder.cs`, `GameMonogram.cs`, `GameMonogram.NOTICE.txt`,
  `tools/build/build-game-icon.ps1`, `tsfp.ps1`,
  `patches/platform/1150-game-icon.patch`,
  `tools/release/publish-release.py` and `tests/repository/test_game_icon.py`.
- **Mac support:** Existing native build and runtime paths are unchanged. ICO
  resources and RC compilation are Windows-only. No Mac app bundle/Dock icon
  has been implemented or verified by this change.
- **Remaining:** Adapt the mark to native macOS app-bundle icon assets and
  distribution once native packaging exists. Preserve original wordmark art
  and its provenance; use the compact initials where the wide title is unreadable.
- **Acceptance:** Verify native app/Dock/Finder and window icons at Retina and
  small sizes, transparent edges, bundle/update packaging and source-only fallback.
  Windows resource tests do not prove native icon integration. See the
  [Windows evidence](STATUS.md#windows-game-icon-for-pr-126-2026-10-18).

### MAC-041 — Published game version picker and banner replacement

- **Status:** Queued
- **PC behavior:** Home's Updates card opens a Game versions page inside the
  launcher with the sidebar visible. Paginated GitHub metadata lists compatible
  stable and preview Windows packages. Explicit confirmation permits a downgrade
  or reinstall through the checksum-verified resumable transactional updater.
  Retry stays pinned to the chosen release. Saves, existing settings, imported
  game data, the ISO and the independent launcher are preserved. Normal channel
  updates remain newer-only. The local private banner uses supplied 1600 x 1200
  key art unchanged, fully fitted on the right of the header. The original game
  wordmark and a blue/silver panel fill the remaining width without cropping or
  redrawing the photo; original assets remain ignored.
- **Source:** [PR #126](https://github.com/TheDoffman/timesplitters-future-perfect-pc/pull/126),
  feature commit `04eaf1c` on `feature/launcher-game-versions`;
  `tools/launcher/GameVersionsPage.cs`, `ReleaseClient.cs`, `Launcher.cs`,
  `Core.cs`, `Bootstrap.cs`, `Launcher.xaml`; `tools/build/build-launcher.ps1`;
  `tests/launcher/launcher_versions.cs` and `tests/launcher/launcher_updates.cs`.
- **Mac support:** No Mac release-browser/transactional-updater adapter exists.
  Native Mac launch/build scripts are unchanged. Private artwork is local and
  its availability on another device is unverified.
- **Remaining:** Add a native release picker once signed/verified Mac runtime
  packages and an updater exist. Reuse version comparison, repository checks,
  exact release selection and personal-data preservation rules. Adapt the banner
  viewport if a corresponding Mac UI is added; retain local asset provenance.
- **Acceptance:** Browse more than one API page including launcher-only pages;
  filter drafts and incompatible/unverified packages; test older, equal, newer,
  missing and unversioned installs; cancellation/resume/retry, request limits,
  corrupt ZIPs and interrupted install recovery. Keep the launcher, settings,
  saves, ISO and data unchanged; block running-game installs; verify normal
  update channels, keyboard navigation and minimum-size layout on Mac. Fit the
  entire supplied image proportionally beside the wordmark without cropping,
  distortion or a generated redraw at minimum and normal window sizes.
- **PC evidence:** [Version picker and banner checks](STATUS.md#launcher-game-version-picker-and-supplied-banner-2026-10-17).
  [Full artwork layout](STATUS.md#complete-original-launcher-banner-artwork-2026-10-18)
  supersedes the initial cropped banner viewport.

### MAC-039 — Recoverable launcher startup security interruptions

- **Status:** Not applicable
- **Scope:** Windows bootstrap only.
- **PC behavior:** A failed application handoff keeps startup open with Retry,
  Open launcher folder and View logs. Win32 cancellation 1223 explains a possible
  SmartScreen/security prompt without claiming the user clicked Cancel. Access
  denied, missing files and other failures have distinct guidance. Retry opens
  the installed application without downloading again; success or user close
  exits normally. Native error codes are logged. Security settings and file
  blocking are not changed automatically; binaries remain unsigned.
- **Source:** Feature commit `a9b79e7` on `fix/launcher-startup-recovery`, based
  on main `e6c68bd`. `tools/launcher/Bootstrap.cs`, `DownloadTransfer.cs`,
  `tools/build/build-launcher.ps1` and `tests/launcher/launcher_startup.cs`.
- **Existing Mac support / remaining adaptation:** Native Mac startup does not
  use this WPF bootstrap or Windows shell/SmartScreen. Existing native scripts
  and adapters are unchanged. A future native GUI should explain its own OS
  launch errors; no Windows dialog or registry behavior should be copied.
- **Acceptance:** Windows synthetic cancellation, access denial, missing files,
  retry, close and folder/log failure tests pass with isolated fixtures. Verify
  guidance/action layout and no personal-file changes. Actual SmartScreen prompt
  visibility and other-device behavior require manual validation; Mac execution
  is not claimed. See [startup recovery evidence](STATUS.md#launcher-startup-security-recovery-2026-10-18).

### MAC-037 — Fresh-install launch without PowerShell (issue #128)

- **Status:** Not applicable
- **Scope:** Windows launcher only.
- **PC behavior:** Play Game starts the runtime directly even before graphics
  settings have been saved, avoiding the reported ConstrainedLanguage script
  failure after a successful ISO import. Settings, saves and logs retain their
  portable paths; data verification remains required.
- **Source:** `Fix fresh-install launch and explain merged fixes on issues`
  on `128-fix-constrained-language-launch`;
  `tools/launcher/Launcher.cs`, `GameSettings.cs`, and launcher settings tests.
- **Existing Mac support / remaining adaptation:** Native Mac launchers do not
  execute the Windows WPF launch path or `Launch.ps1`; no adapter is needed.
  No native Mac code was changed or executed for this fix.
- **Acceptance checks:** Windows fresh/configured startup, exit status, redirected
  logs and settings preservation pass with synthetic executables, including a
  constrained PowerShell test host. See issue #128 in `docs/project/STATUS.md` for scope.

### MAC-036 — Ghost Gun full-body visibility (issue #111)

- **Living-character alignment follow-up (#181, 2026-10-20, this commit):**
  `src/interface/pc_display_ui.inc` now places the US `sub_00122F10` final actor reveal
  composite in world-screen coordinates rather than the centered HUD viewport.
  This covers Past Cortez and other living actors viewed with the Ghost Gun;
  humanoid ghost rendering and the native blur/tint are unchanged. Both
  `TSFP_PC_CONTROLS` and `TSFP_NATIVE_DISPLAY` use the existing primitive wrapper.
  `tests/graphics/test_ghost_actor_overlay.py` is registered in Windows/macOS CI, with
  asset-free regression/mutation checks and an optional original-XBE fixture.
  Windows build/native tests and local fixtures pass; no Mac adapter change is
  needed for the common renderer. The user confirmed the overlay now stays on
  Cortez in the isolated Windows test build, launched at 1920x1080. Mac execution
  and visual checks at other ratios remain pending. Check the reveal against
  the body while panning at 4:3, 16:9 and ultrawide, preserving the blur and
  normal HUD placement afterward.
  [Validation limits](STATUS.md#ghost-gun-living-character-alignment-issue-181-2026-10-20).

- **Status:** Queued
- **Fixture follow-up (2026-10-19, this revision on `fix/ghost-replay-crash`):**
  `tests/graphics/ghost_targets.py` now uses a valid power-of-two 64x64 swizzled target;
  the former 64x48 fixture caused out-of-bounds unswizzle reads and intermittent
  Windows publishing crashes. `tests/graphics/test_ghost_targets.py` validates storage
  and CI runs it. No runtime/Mac adapter change is needed for the fixture fix;
  native GL acceptance remains queued and unverified. See the
  [Windows evidence](STATUS.md#ghost-replay-fixture-access-violation-2026-10-19).
- **PC behavior:** Full-body ghosts retain their native red translucent effect
  when revealed with the Ghost Gun. Offscreen colour targets share scene depth
  and stencil; self-sampling colour passes read a snapshot taken before each
  draw instead of an input rejected by the graphics API.
- **Source:** Commit `Fix Ghost Gun humanoid rendering and target feedback` on
  `111-bug-ghosts-are-still-not-visible-with-ghost-gun`; runtime patch
  `1130-ghost-target-feedback.patch`,
  `tests/graphics/ghost_targets.py`, `tsfp.ps1`. Separate from the floating-skull fix #17.
- **Shared Mac support:** HLE retains the automatic depth surface for same-size
  full-frame targets when the native adapter can bind it. Mismatched native
  dimensions retain the previous fallback. The Windows D3D11/Vulkan RHI adapter
  promotes colour images to match scene depth; native OpenGL does not use it.
- **Remaining:** Implement native GL feedback snapshots and matching physical
  colour/depth sizes; preserve logical UVs, viewport/scissor scaling and stencil.
  Windows visual results do not establish Mac support. See `docs/project/STATUS.md`.
- **Acceptance:** Verify What Lies Below humanoid ghosts with the Ghost Gun and
  Mansion's TV-room Deadwina. Preserve occlusion by walls, depth writes across
  target switches, red tint/blur, multiple ghosts, resolution changes and
  supersampling. Reproduce the synthetic occlusion/feedback checks in native GL.

### MAC-035 — Cinematic width fill (issue #30 investigation)

- **Status:** Awaiting validation
- **PC behavior:** Story and scripted cinematic mattes no longer become
  vertical side bars on ultrawide displays. The camera crops the vertical
  aperture proportionally and retains the authored horizontal lens. The
  letterboxed `attract.xmv` fills the display width proportionally, excluding
  its encoded side padding on wide displays. Startup logos follow the same
  width-fill policy with proportional vertical cropping. Timing is unchanged.
- **Source:** Branch `30-cutscenes-playing-at-lower-fps`;
  `src/gameplay/pc_cutscene.h`, `src/interface/pc_display_menu.c`, `cinematic-movie-fill.patch`,
  `cinematic-movie-inset.patch`,
  and `tests/graphics/test_cutscene_letterbox.py` / `tests/graphics/cutscene_letterbox.c`.
- **Shared Mac support:** In-engine camera/matte calculations use the existing
  native display adapter. The movie rectangle helper is portable. Native XMV
  playback remains unavailable; the existing Windows playback path selects
  the known movies' presentation modes. This does not enable movies on Mac.
- **Remaining:** Native build and visual validation for real-time scenes;
  connect the same movie policy when native movie playback is implemented.
  Windows measurements are recorded separately in `docs/project/STATUS.md`.
- **Acceptance:** At 4:3, 16:9, 16:10, 21:9 and 32:9, verify story openings,
  endings and animated in-game cinematic transitions. Preserve proportions,
  subtitles, fade coverage and gameplay FOV after the scene. At ultrawide,
  verify the first mission's space backdrop still covers the view and no
  vertical matte appears. Check the recorded reel and startup logos' proportional
  width fill, movie skip, audio sync and unchanged guest clock. Source-only tests
  cover aspect math and original US cinematic call paths; they do not prove
  every scene's performance or native movie support.

### MAC-034 — Plain improvement form handoff

- **Status:** Not applicable
- **PC behavior:** Request enhancement opens the idea-only GitHub form without
  copying or sending diagnostics, including when no report has been refreshed.
  Bug reports retain their optional diagnostic handoff. Bug version/platform
  fields are now empty required text inputs: GitHub accepts a blank dropdown
  option as selected. Published versions remain an automatically updated reference.
- **Source:** `fix/issue-form-empty-selections`;
  `.github/ISSUE_TEMPLATE`, `tools/launcher/DiagnosticIssueDraft.cs`,
  `tools/launcher/Launcher.cs`, and their reporting regression checks.
- **Shared Mac support:** The GitHub forms apply to browser users on every OS.
  The changed WPF launcher is Windows-only; there is no native Mac counterpart
  to adapt. No native game code or Mac settings/saves are changed.
- **Remaining:** No Mac implementation work. Native Mac execution is not claimed.
- **Acceptance:** The improvement form has only its required idea field; the
  launcher opens it without diagnostic query parameters or clipboard writes.
  Bug forms retain automatic version references without prefilling their required
  answers. A blank-looking dropdown alone does not establish enforced validation.

### MAC-033 — Full-display transition fades

- **Status:** Awaiting validation
- **PC behavior:** Frontend and game transition fades, menu canvas tints, story
  fades/tints and blackouts cover the whole output at non-4:3 ratios. Loading-picture
  fades and their separate top/bottom borders also span the output. The loading
  label uses proportional UI scale and a left-edge inset, with its native font
  and animated dots preserved. Each draw
  keeps its original colour, opacity and timing. Scoped placement flushes sprite batches before
  and after the fade; ordinary UI, cinematic mattes and partial viewports retain
  their existing placement.
- **Source:** `fix/responsive-transition-overlay`, integrated on main base
  `4ad4c17`; `src/interface/pc_display_ui.inc`, `src/interface/pc_display_menu.h`,
  `src/interface/pc_tutorial_ui.inc`, `transition-overlay.patch`,
  `tests/graphics/transition_overlay.c` and `tests/graphics/tutorial_layout.c`.
- **Shared Mac support:** The same rectangle hook and renderer placement ABI
  compile through `TSFP_NATIVE_DISPLAY`; no OS-specific adapter is introduced.
- **Remaining:** Native build/regression and visual validation. Windows evidence
  and the verified Story navigation, mission-loading and replay checks are recorded
  in `docs/project/STATUS.md`.
- **Acceptance:** Check fade-out and fade-in between frontend screens, entering
  and leaving missions, and story transitions at 4:3, 5:4, 16:9, 16:10,
  and ultrawide sizes. No bright uncovered edges may remain at full
  blackout. Check intermediate opacity, subsequent menu proportions, cinematic
  mattes and split-screen player viewports. Verify loading text remains readable,
  proportional and aligned to the left edge when resizing.

### MAC-032 — Responsive menu backgrounds and scope masks

- **Status:** Awaiting validation
- **PC behavior:** The frontend footer's black background and decorative strips
  fill widescreen/ultrawide output; the animated blue sheet also fills the
  picture, with its original motion. Labels and mouse targets keep their existing
  proportions. Background changes preserve original 4:3/narrow layouts and
  partial viewports. Main-menu construction/measurement and row drawing share a 25%
  smaller glyph scale, keeping the panel below the logo and scrolling description.
  Objectives top/bottom bars and blue trim fill wide screens. The circular scope's
  outer masks reach display edges while its aperture remains proportional.
  Conventional and futuristic sniper groups explicitly cover the host area
  outside the original canvas, even when the guest omits its own outer masks.
  The Sci-Fi Sniper uses its destination-alpha tint instead of opaque black
  margins. Shared aim shapes scale around their projected aim point, zoom
  gauges use explicit edge anchors, and full-view crosshairs/borders stretch.
  Lock-on box halves and orbiting pieces share their target anchor. Special
  scope rings stay fitted while their gradients/composite cover the output.
  Issue #152 also extends the title reveal's two outer black 3D panels with
  the display aspect ratio, preserving the proportional logo and animated wipe.
  The related white-flash correction on the #152 branch scopes the gameplay white
  wash and additive flash to the full output, preserving intensity and timing.
  It uses `pc_display_flash_overlay`, `white-flash-overlay.patch`, and the
  shared transition hook; coverage is in `tests/graphics/flash_overlay.c` and
  `tests/graphics/transition_overlay.c`. The wrapper also enables `TSFP_NATIVE_DISPLAY`.
  Check explosion flashes at 4:3, widescreen, ultrawide and portrait on Mac;
  confirm partial/split views and later HUD elements retain their placement.
  Windows unit/build evidence does not establish Mac visual correctness.
- **Source:** [Feature branch](https://github.com/TheDoffman/timesplitters-future-perfect-pc/tree/fix/responsive-menu-footer);
  `src/interface/pc_display_ui.inc`, `src/interface/pc_display_menu.h`,
  `menu-footer-background.patch`, `menu-backdrop.patch`,
  `main-menu-font.patch`, `objectives-scope-coverage.patch`,
  `sniper-scope-coverage.patch`, `weapon-aim-overlays.patch`,
  `special-weapon-overlays.patch`, `tests/graphics/weapon_effects.c`, `src/interface/pc_ui_layout.h`, and `tests/graphics/ui_layout.c`.
- **Shared Mac support:** The same title hooks and scoped STRETCH placement are
  compiled for `TSFP_NATIVE_DISPLAY`; the native renderer already supports this
  placement. The blue sheet uses the shared guest projection setter and restores
  it before subsequent layers. No new operating-system adapter is needed.
  The #152 title-matte hook also uses `TSFP_NATIVE_DISPLAY`; it adjusts guest
  vertices directly, without adding renderer placement state or an OS adapter.
  Its shared coverage is in `tests/graphics/title_matte.c` and `test_ui_layout.py`;
  native startup-animation visual validation remains required.
- **Remaining:** Run shared regression tests and native visual validation.
  Windows evidence is in the responsive frontend backgrounds and Objectives
  bars/scope masks entries in `docs/project/STATUS.md`, including the live 3440x1440
  conventional and Sci-Fi Sniper checks. This does not establish Mac rendering
  or pointer behavior.
- **Acceptance:** At 4:3, 16:9, 16:10 and ultrawide, check the main and Story
  footer backgrounds, both decorative halves, proportional prompt labels and
  menu transitions; verify no background clipping. Check the smaller main-menu
  rows fit below the header and retain matching mouse targets; other screens
  keep their existing font sizes. Check Objectives bars and trim at both edges,
  and circular scope masks/reticles at 4:3, 5:4, 16:9, 16:10 and ultrawide.
  Verify displaced reticles remain over their aim point and zoom indicators
  stay proportional. Check lock-on indicators on targets near the screen edges,
  including lock acquisition/rotation, and special scope gradients during
  activation/deactivation. Check the Sci-Fi tint has no hard black side margins.
  Split-screen viewports must retain their original layout.
  Watch a full blue-sheet animation cycle and check character previews, gameplay
  and cutscenes retain their lens.


### MAC-001 — Direct-camera mouse look

- **#381 Windows cursor confinement (2026-10-27, v0.3.5 hotfix):**
  `src/input/pc_mouse_capture.h` and `src/input/pc_controls.c` pin the hidden gameplay pointer
  to the client center, retaining raw relative aiming and existing release rules.
  Full-client confinement previously allowed pointer travel to the edges.
  Native Mac already requests SDL relative mode in `src/native/input_sdl.c`;
  no Mac adaptation is needed for this Win32 cursor-bound calculation. Existing
  Mac support remains **Awaiting validation**: physical repeated turns, Fire
  release, launcher start, focus/pause/settings and borderless transitions need
  a Mac host. Windows deterministic and OS checks are recorded in STATUS.md;
  they do not establish the reporter's intermittent launcher trigger.

- Mouse-chain bounded queries (PR #328, commit carrying this entry): shared
  camera, mounted and terminal helpers use `pc_guest_memory_in_span` on misses.
  Their original validated extents, mode/ownership checks, input consumption and
  fresh per-update snapshots remain; only known-size query selection changes.
  Windows uses resident-page batches; Darwin keeps its Mach adapter. Both host
  adapter bodies pass controlled-map tests on Windows; real host protection and
  mounted/terminal suites pass 13 checks. The resident Windows camera case now
  needs no full-region query. Updated native Mac tests and physical input remain
  awaiting validation; no platform-specific feature adapter is required.

- Bounded page batches (PR #328, commit carrying this entry): Windows' shared
  `pc_guest_query_span` checks up to four resident pages per query, including
  every page's read/write permissions. Auto Lookahead now uses that path while
  preserving its full player-record extent, temporary bit override and fresh
  per-call regions. Larger/cold/failed/guarded cases retain full-region fallback;
  Darwin retains the existing Mach query. Real private/mapped and camera-record
  fixtures cover intermediate/final protection changes and expiry; mutations
  reject checking only the first page. Windows tests/build pass. New Mac CI and
  physical aiming/gameplay remain awaiting validation; no Mac adapter is needed.

- General display guards (commit carrying this entry): `pc_display_menu.c`
  delegates each `guest_writable` call to a fresh shared bounded check. Windows
  gains the resident-page fast path; Mac uses real Mach bounds/protection instead
  of compatibility VirtualQuery's synthetic 4 KiB writable extent. Valid larger
  spans remain supported through full-region fallback. Production-adapter tests
  cover protected/boundary/overflow/multi-page accesses and reject retained
  snapshots. Windows tests pass; new Mac CI and physical UI/gameplay are pending.

- Display memory span queries (commit carrying this entry): `pc_guest_span.h`
  uses fresh Windows resident-page protection for known-size, single-page camera
  and projection accesses; all other cases retain full-region fallback. Shared
  bounds/snapshot policy is unchanged. Darwin retains its existing Mach query;
  the Windows-specific optimization needs no Mac equivalent for correctness.
  Private/mapped protection fixtures run through the registered decal suite on
  both hosts; Windows failure/fast-path mutations are host-specific. Local
  Windows tests pass; native Mac CI and physical gameplay remain pending.

- Mouse mapping reuse (commit carrying this entry): `pc_guest_memory.h`,
  `pc_camera.h`, `pc_terminal.h`, `pc_mounted.h` and both host input adapters
  share an OS mapping snapshot only within one mouse update. Bounds/write
  permissions remain checked and the next update queries afresh. The follow-up
  on `perf/mouse-memory-order` seeds at the lower player/FOV field so forward
  VM spans cover either address order. Both adapters use the shared helper;
  split/read-only/inaccessible mappings retain fallback validation. The registered
  mounted pytest suite now includes real host protection changes and an unordered
  negative control. These fixtures pass native Windows/macOS CI at `1dbd397`;
  physical Mac gameplay remains awaiting validation. No Mac-specific
  feature work is needed; Darwin still uses its existing Mach VM query.
  Windows executes both adapter bodies with controlled maps, plus actual guarded
  terminal/mounted fixtures. Physical Mac aiming/capture and native execution
  remain **Awaiting validation**; see STATUS for available-host evidence.

- **Themed keyboard editor (2026-10-22, uncommitted; native validation pending):**
  Shared native-menu builders expose every action and mouse option in main Settings
  and pause Options, with a keyboard/mouse diagram and two binding columns.
  Shared capture/release/conflict/save policy uses thin Windows and SDL raw-input
  adapters. Both launcher hosts and native settings dialogs limit keyboard actions
  to two sources, retaining controller capacity and reading legacy keyboard pairs.
  No feature adapter is deferred. Mac acceptance: compile and run SDL input tests,
  bind keyboard/mouse/wheel on both menu paths, cancel/focus/reopen, test conflicts,
  sensitivity dragging and persistence, and confirm Cocoa/launcher two-column UI.
  Windows evidence is recorded in docs/project/STATUS.md; it does not establish Mac validation.

- **Keyboard weapon slots (2026-10-22, uncommitted):** Shared slot bindings,
  edge/expiry policy and original guest inventory selection support Windows
  and SDL Mac. Cocoa in-game settings and both launchers expose ten slot actions;
  existing configurations migrate in memory without overwriting custom keys.
  No new OS-specific feature is deferred. Windows fixture/build results are
  recorded in docs/project/STATUS.md. Native Mac compilation and physical gameplay remain
  pending: bind slots, select single/dual weapons, try empty slots, switch
  devices and pause/focus, then save/reload both editors.

- **Mounted native cadence (2026-10-22, `fix/native-mounted-mouse`):** Shared
  `pc_mounted.h` now rebases consumed mouse batches and accepts advancing
  fractional game time at native presets. Windows and SDL adapters pass their
  native-mode selection; no new OS adapter is required. The old helper lost
  fresh packets at 60 and discarded zero-whole-frame motion at 120/144.
  Existing gun behavior plus cadence/pause/handoff and two negative controls
  pass five focused Windows checks; 159 broader timing/input/context cases
  also pass. Supported Windows build `a5e0dc2` and built-in checks pass; draft
  PR #304 carries this independent main-based fix. Mac execution and a live turret test of
  this addition remain awaiting validation; existing #182 acceptance applies.
  Windows actual-adapter checks now pass native/pause/live-rate handoff and
  return-to-foot cases with synthetic raw motion. Matching cases are in SDL's
  input self-test, awaiting native execution. The new packet fixture explicitly
  includes `<stddef.h>` after Mac CI exposed the missing `ptrdiff_t` declaration;
  that CI repair does not change gameplay behavior.

- **Alpha.2 self-test isolation (2026-10-22):** `tsfp_native_input_test`
  clears its synthetic terminal context after the fractional-time fixture,
  before the independent capture/fire case. The previous test retained terminal
  suppression after freeing its mock guest. The candidate reset passed the
  actual native renderer, input, Cocoa settings/debug checks and three CTests
  on the Apple M4/macOS 27 host with isolated settings. Gameplay code is unchanged;
  final clean release validation follows after integration.

- **v0.3.3 input self-test (2026-10-25):** The shared keyboard profile parser
  deliberately keeps two bindings per action. The synthetic SDL test now checks
  E and Space as Confirm, with press/release assertions, and verifies that the
  fixture's retained third entry (Return) remains inactive. It previously
  required Return despite that cap. This changes only test expectations;
  gameplay bindings and physical-input acceptance remain unchanged. Clean
  revision `6f105606` passed three CTests plus native renderer, input, Cocoa
  settings and debug checks on Apple M4/macOS 27.0.1. Synthetic checks do not
  replace physical keyboard or gameplay acceptance.

- **Issue #182 follow-up (2026-10-21, this change):** Shared
  `src/input/pc_mounted.h` routes mounted/CCTV AGUN aiming to the gun angles instead
  of hidden player angles. Both host adapters call it; no additional native
  adapter is needed. Authored limits, ownership, focus/capture, pause, stale
  input and normal-camera return are covered by `tests/gameplay/test_mounted_mouse.py`,
  registered in the Windows/macOS runtime workflow. Windows fixture and Release
  build checks pass; the user confirmed the CCTV turret retest works, and
  read-only telemetry confirmed gun-angle movement. Native compilation,
  fixture execution and physical mouse/controller comparison remain pending.
  Acceptance: aim/fire a Breaking and Entering CCTV gun, return to its pointer
  and on-foot controls, check pitch/yaw limits and pause/focus transitions.
  See [#182 validation](STATUS.md#mounted-and-cctv-gun-mouse-aim-182--2026-10-21).

- **Issue #7 follow-up (2026-10-16):** `src/interface/pc_terminal.h`
  on `7-breaking-and-entering-camera-turret-not-working` routes relative mouse
  counts to the active terminal's logical pointer in
  mode `0x2000`, preserving its 4–251 bounds and normal aiming on exit.
  Left-click emits Confirm only in terminal menus; release is required when
  entering/leaving with a held button. Shared context is in `src/input/pc_input.c`
  and the sampling hook in `patches/input/0940-terminal-mouse-select.patch`.
  Both `src/input/pc_controls.c` and `src/native/input_sdl.c` call this shared helper;
  no additional Mac adapter is required. `tests/input/test_terminal_mouse.py` is
  included in Windows/macOS CI. Windows build and physical terminal checks
  passed, including user-confirmed left-click selection and return to ordinary
  aiming/firing; results are recorded in [Issue #7 validation](STATUS.md#terminal-mouse-routing-issue-7--2026-10-16).
  Native Mac compilation and physical-pointer validation remain pending:
  check terminal selection/clicks, scripted/paused states and ordinary aiming
  after exit using isolated saves/settings.

- **Status:** Awaiting validation
- **Issue #18 follow-up (2026-10-16, commit 8dee97f):** `src/gameplay/pc_camera.h` no longer
  requires guest player ID zero. Both Windows `src/input/pc_controls.c` and native
  `src/native/input_sdl.c` already call this shared helper, so no additional
  Mac adapter is required. Windows regression `tests/input/pc_controls.c` reproduces
  the former rejection for ID 1 and checks fields for IDs 0–15; Release build
  and build-script checks passed. Mac compilation/execution remains pending.
  Acceptance: verify the native camera adapter with a nonzero guest ID as well
  as ID zero, retaining FOV, focus/capture and single-consumption behavior.
  Native multiplayer still depends on MAC-013; Windows checks do not establish
  native multiplayer support. [Measured scope](STATUS.md#nonzero-player-mouse-look-issue-18--2026-10-16).
- **PC behavior:** PC mouse counts update camera angles directly, with zoom/FOV scaling and no controller deadzone or turn-speed curve.
- **PC source:** Audited revision above; `src/input/pc_controls.c`, `src/input/pc_input.c`, `patches/input/0230-pc-controls.patch`, `tests/input/pc_controls.c`.
- **PC validation:** Input regression and deterministic mission replay, including zoom scaling, are recorded in [Windows validation](STATUS.md#validation); physical mouse fidelity remains a separate check.
- **Existing Mac support:** SDL relative counts now feed the shared `pc_input_look` camera model through the guest wrapper. Controller right-stick input stays independent; configured sensitivity, inversion and FOV scaling are shared with Windows.
- **Mac adaptation needed:** Implementation is present. Finish live gameplay checks across pause/settings/focus transitions and physical mouse use. Native adapter tests now cover equal counts batched or applied per event, both pitch limits and stale motion suppression.
- **Mac acceptance checks:** Equal counts give equal angles across polling rates; zoom scaling, inversion and pitch limits work; pause/settings/focus changes cause no jump or unwanted shot. Use isolated settings and a short mission check.
- **Mac implementation and evidence:** Implementation included with this backlog revision on `mac-port`: `src/native/input_sdl.c`, `src/gameplay/pc_camera.h`, `src/runtime/pc_guest_memory.h`, `src/input/pc_controls.c`, `patches/gameplay/timing/0860-native-camera.patch`. Native fixtures passed sensitivity, inversion, FOV, capture/release, focus and no-stick-injection checks. A bounded first-mission run logged 8/-2 counts changing yaw/pitch by -1.6/+0.4 degrees at sensitivity 0.2 and FOV 55. These are synthetic SDL events, not physical-device validation. Further native adapter checks for batched/per-event counts, pitch limits and stale motion passed; see [capture and camera validation](STATUS.md#mac-binding-capture-and-camera-checks-2026-10-15). [Validation and evidence](STATUS.md#mac-backlog-renderer-camera-and-prompts-2026-10-15).
- **Local source check (2026-10-17):** Terminal mouse C fixture passed
  on Apple Silicon with Clang. This does not verify native window input, rendering
  or gameplay. [Exact scope](STATUS.md#mac-current-main-backlog-audit-2026-10-17).

### MAC-002 — Keyboard/mouse prompts and device switching

- **#256 diagonal stick speed (2026-10-25, branch `256-diagonal-movement-speed`):**
  The shared binding sampler now applies the configured deadzone radially to
  gameplay movement and look axes on both host builds. Keyboard diagonal
  capping and stick-as-button thresholds stay as before. Windows build and
  shared math/patch checks are recorded in [docs/project/STATUS.md](STATUS.md#issue-256-diagonal-stick-speed-2026-10-25).
  The synthetic SDL gamepad self-test now compares straight and diagonal travel
  on both sticks; native Mac execution and physical controller gameplay still
  need validation.

- **#256 gameplay follow-up (2026-10-25, branch `fix/256-diagonal-and-mouse-capture`):**
  Live Windows Story displacement showed remaining diagonal attenuation after
  the radial input fix. Shared `pc_input_normalize_movement` now compensates
  the title's later movement step for keyboard and controller input. The SDL
  adapter restores desired relative mouse mode on focus return, except during
  menus or after explicit release. Windows live replay and host tests cover
  the new path; run native Mac input tests and physical movement/focus checks
  before claiming Mac acceptance.

- Fresh text chunks (commit carrying this entry): `pc_text_read_scope.h` uses
  the shared page-span adapter for bounded reads. Strings walk successive
  validated chunks; `pc_prompts_read_span` validates whole argument frames,
  including those crossing pages. Scope expiry and fresh scratch-write checks
  remain unchanged. Windows uses resident-page queries; Darwin retains Mach
  region queries. Focused tests and Windows Build pass; updated Mac CI and
  physical prompt/menu validation remain pending. No new Mac adapter is needed.

- **#319 (2026-10-24, branch `fix/319-controller-input`):** Shared runtime supplies pad defaults when
  Controller is selected over a keyboard-only launcher map, without changing
  saved keys or custom pad maps. Portable actual-parser/sampler coverage is
  registered in both host jobs. Windows local checks and virtual SDL validation
  are recorded in docs/project/STATUS.md; native Mac and physical controllers remain unverified.

- **HUD panel performance follow-up (2026-10-22, candidate `4a4aa02`):**
  The shared `text-panel-scope` patch reuses read mapping checks across the
  original panel's width/fit/draw sequence, inside the existing placement
  wrapper. Windows and native prompt feature definitions pass the same modeled
  panel fixture: five read queries become one; all measurements/draws, fresh
  scratch writes, operation expiry and placement boundaries remain covered.
  No OS adapter change is needed. Windows full build/built-in checks pass;
  a bounded native144 Venice run shows readable RANK/ordinal labels and
  preserves the isolated profile. Hosted Windows/Mac runtime CI passes at
  `c580949`, including Mac asset-free graphics contracts. Initial Windows
  before/after pacing results are mixed; no FPS benefit is established.
  Full rendered comparison and native Mac gameplay/performance remain pending;
  see docs/project/STATUS.md.

- **Crowded multiplayer CPU follow-up (`252e72e`, 2026-10-22):** The shared
  text read cache now keeps the complete OS-validated interval, avoiding repeat
  queries when guest stack reads descend or labels visit earlier addresses in
  the same mapping. The US outlined-label wrapper also shares one read scope
  across its four outline passes and foreground draw. Scope expiry at return
  and fresh scratch-write validation are unchanged.
  The existing Windows/Darwin VM adapters provide the interval; no new platform
  adapter is needed. Descending-read regression fails before the fix and passes
  after it. Both host feature defines pass the five-pass wrapper/ABI fixture,
  including a mutation that detects repeated queries. Native Mac gameplay and
  performance remain awaiting validation; see docs/project/STATUS.md for Windows evidence.
  Windows/macOS runtime, upgrade and patch-transition CI passes at PR #295
  implementation head `4a40569`; that does not replace a native gameplay run.

- **Issue #105 follow-up (`5c2f4ac`, 2026-10-21):** Shared prompt read scopes
  (`src/interface/pc_text_read_scope.h`, `pc_prompts.c`, `pc_prompt_guest.inc`,
  `pc_tutorial_ui.inc`, `pc_display_ui.inc`) reduce VM queries. The separate
  `late-frame-pacing` patch preserves the 60-Hz clock and lets already-late
  frames consume a real signalled vblank without another tick. Windows bounded
  weapon-set acceptance passed at 53.8-54.3 FPS (original ~20); see docs/project/STATUS.md.
  No new Mac adapter is required. Mac build/runtime validation remains pending:
  run shared contracts and isolated Arcade weapon-set/before/back-out checks,
  plus representative prompts/tutorial wrapping, before marking it verified.
- **Status:** Awaiting validation
- **PC behavior:** Formatted menu/HUD hints show current bindings, [Unbound] or [Mouse], and switch back to controller icons on deliberate controller activity.
- **PC source:** Audited revision above; `src/input/pc_prompts.c`, `src/input/pc_prompt_guest.inc`, `patches/input/0280-pc-prompts.patch`, `tests/input/pc_prompts.c`.
- **PC validation:** Windows wiring and regression coverage are present; no new Windows execution was performed for this audit.
- **Existing Mac support:** The shared formatter and guest text wrappers now compile on Mac. SDL keyboard/mouse and player-1 controller activity update the shared prompt model. Menu captures show current keyboard labels; saved binding changes update formatter output.
- **Mac adaptation needed:** Finish representative live gameplay hint checks and physical keyboard/controller switching. Original artwork and literal tutorial text are unchanged; a first-mission tutorial still says START, which is not a formatted prompt token.
- **Mac acceptance checks:** Menu and gameplay hints reflect saved bindings immediately; meaningful controller input switches icons; releases, held buttons and drift do not cause flicker. Preserve original artwork-based diagrams.
- **Mac implementation and evidence:** Implementation included with this backlog revision on `mac-port`: `patches/input/0850-native-prompts.patch`, `src/input/pc_prompt_guest.inc`, `src/runtime/pc_guest_memory.h`, `src/native/input_sdl.c`, `src/native/prompt_selftest.inc`, `src/native/settings_selftest.inc`. Passed custom/unbound/mouse labels, deliberate pad activity, held/release/drift suppression, SDL virtual-pad integration, live saved binding and guarded guest ABI tests. Reviewed main/paused Display menus show [E] Select and [F] Back. [Validation and evidence](STATUS.md#mac-backlog-renderer-camera-and-prompts-2026-10-15).

### MAC-003 — Debug settings and on-screen performance information

- **Delayed Present capture (2026-10-24, commit carrying this entry):** Shared
  `src/graphics/present_metrics.h` adds opt-in `TSFP_FRAME_PROFILE_DELAY_MS` so
  bounded recordings can cover later fights instead of menus. Common RHI and
  fallback GL already consume the same recorder; no new Mac adapter is needed.
  The production-recorder fixture passes on Windows for delayed intervals,
  preserved skipped/failed gaps, disabled recording, malformed limits and the
  unchanged row cap. Native Mac compilation/execution remains pending; verify
  this fixture and a delayed gameplay CSV there before marking it validated.

- **Status:** Awaiting validation
- **PC behavior:** F12 opens debug settings; F9 toggles FPS. Options show guest/shown FPS, frame intervals, issue counters and recent logs, with bounded optional performance logging.
- **PC source:** Audited revision above; `src/diagnostics/pc_debug.c`, `src/diagnostics/pc_debug.h`, `patches/diagnostics/0240-pc-debug.patch`, `tests/diagnostics/pc_debug.c`, `tests/diagnostics/pc_debug_game.py`.
- **PC validation:** Windows implementation and test scripts exist; current Windows runtime behavior was not retested during this audit.
- **Existing Mac support:** F9 toggles a host-only OpenGL text overlay; F12 and the Game Settings menu open a Cocoa debug panel. Both use the shared statistics, options, configuration and diagnostic classifier. POSIX log reading handles truncation/replacement and runs on the main thread.
- **Mac adaptation needed:** Implementation is present. Finish visual Cocoa layout, physical F9/F12 and live fullscreen panel/focus checks. A synthetic shortcut smoke run does not establish physical event delivery.
- **Mac acceptance checks:** Toggle in windowed/fullscreen modes; distinguish guest and interpolated presents; save/cancel and issue-counter reset work; log polling stays off the render thread; preserve unrelated JSON.
- **Mac implementation and evidence:** Commit `149939f`, integrated into `main`: shared `src/diagnostics/pc_debug.c`, native `debug_macos.inc`, `gl_debug.inc`, SDL/renderer hooks and `patches/diagnostics/0870-native-debug.patch`. Native GPU text/state/isolation tests, real Cocoa Save/Cancel, configuration preservation/failure, log rotation/reset and synthetic F9 repeat/release checks pass. [Measured validation](STATUS.md#mac-debug-and-performance-tools-2026-10-15).
- **Current-main follow-up (2026-10-17):** PC latency diagnostics were added in
  `e2f655a`. `PcDebugOptions` now has five fields, but Cocoa `save:` initializes
  only four, so Save writes `show_latency=false` even when it was previously
  true. Preserve this setting before further native validation; exercise true
  and false values, Save/Cancel and unrelated JSON on a disposable file.
  A native latency control and live measurements depend on MAC-013; setting
  preservation does not. The native Save path now reads current options before
  changing its four exposed fields; real Cocoa Save/Cancel and true-latency
  preservation checks pass in the native build-restoration run. See MAC-050 for the generated-header build dependency
  and MAC-051 for the separate build-identity overlay.

- **Shared definitions (2026-10-19):** `codex/shared-debug-settings` makes
  `src/diagnostics/pc_debug_options.def` the single list for fields, keys, labels and native
  debug controls. Both dialogs and shared JSON persistence iterate it. Cocoa
  Save/Cancel, hidden latency preservation, generated labels, capability
  visibility and failed-write behavior pass. Definition-extension tests run
  on each host in CI; Windows dialog execution remains pending. This does not
  supply the missing native network provider or complete every overlay check.
  [Evidence](STATUS.md#shared-debug-setting-definitions-2026-10-19).

- **Caller diagnostics (2026-10-24, uncommitted):** The existing Windows-only
  `recomp_sample.c` profiler now retains all captured caller frames (maximum32)
  and reports bounded-table overflow via `0510-sampler-caller-depth.patch`.
  Thread suspension and unwind depend on Win32/DbgHelp; Mac keeps its existing
  disabled sampler. Portable insertion/mutation tests run on both hosts, but a
  Mac sampling adapter and live attribution remain unimplemented. Existing Mac
  overlay/debug behavior is unchanged. This is diagnostic coverage, not FPS gain.

### MAC-004 — Capture-to-bind and conflict reassignment

- **Status:** Awaiting validation
- **PC behavior:** The Windows editor captures a pressed key, mouse button or wheel direction and offers to move a conflicting binding from another action.
- **PC source:** Audited revision above; `src/interface/pc_settings.c`, `src/input/pc_bindings.inc`, `tests/input/pc_settings.c`, `tests/input/pc_settings_game.py`.
- **PC validation:** Capture, conflict reassignment, Cancel and live persistence were checked in [PC controls menu](STATUS.md#pc-controls-menu-2026-10-15).
- **Existing Mac support:** Each of the four Cocoa binding slots now has a Record action. Capture accepts keyboard/modifier keys, five mouse buttons and wheel directions; Escape cancels, with separate Use Escape and Unbind buttons. Explicit conflict confirmation moves the binding using the same assignment/reserved-key rules as Windows. Punctuation and keypad keys now work through the SDL mapping; unsupported saved keys remain visible and preserved.
- **Mac adaptation needed:** Finish visual/editor interaction checks and physical keyboard/mouse capture on Mac, including non-US layouts. Native responder and modal-dialog tests passed; they do not establish physical macOS event delivery.
- **Mac acceptance checks:** Capture keyboard, five mouse buttons and wheel directions without leaking gameplay input; test conflict move/cancel, reserved keys, save/reopen and controls from another device using isolated files.
- **Mac implementation and evidence:** Implementation included with this backlog revision on `mac-port`, tested against `30653e3`: `src/native/binding_capture.inc`, `src/native/settings_macos.m`, `src/native/input_sdl.c`, `src/native/settings_selftest.inc`, and shared `src/input/pc_bindings.inc`, `src/interface/pc_settings.c`, `src/interface/pc_settings.h`. Cocoa responder/modal, shared move/cancel, save/reload, live punctuation/keypad input, backup and failure tests pass. [Validation](STATUS.md#mac-binding-capture-and-camera-checks-2026-10-15).

### MAC-005 — Multiple texture stages and shader fidelity

- **Status:** In progress
- **Main promotion:** The user-authorized promotion combines `be37669` with
  main's newer shared launcher at `c238ab3`, preserving both test sets. The
  patched runtime hashes remain identical to the scene-tested renderer;
  launcher checks pass independently. This does not close broader visual gaps.
  [Promotion evidence](STATUS.md#renderer-promotion-onto-current-main-2026-10-19).

- **Final bounded acceptance (2026-10-19):** Both hosted jobs pass at `c6b01d3`.
  The integrated executable also passes a 115.17-second opening-mission route:
  six reviewed images retain canyon geometry, characters, weapon/HUD and
  fire/smoke after movement/turns. The user reports the same Khallos sky on PC;
  no additional Mac-only sky correction is justified. This completes the short
  renderer goal, while this broader entry remains in progress. Windows gameplay,
  exact original-console fidelity and all-scenes coverage are unverified.
  [Final evidence and scope audit](STATUS.md#shared-renderer-final-bounded-acceptance-2026-10-19).

- **Khallos PC comparison (2026-10-19):** The user reports the same muted
  grey/stretched sky at the starting train stairs on PC. No PC revision or
  matched capture was provided; this is user-reported appearance evidence.
  Together with the source/pixel investigation, it supports preserving the
  shared sky and train motion effect. Exact original-console fidelity remains
  unverified; no additional Mac-only sky change is justified by these checks.
  [Evidence](STATUS.md#khallos-pc-appearance-comparison-2026-10-19).
- **Integrated-build acceptance (2026-10-19):** The documentation branch
  `codex/integrated-renderer-acceptance` depends on unmerged `9940dba`.
  Separate 15-second Khallos windows measured 60.001 and 119.998 successful
  API presents/s (120 mode: 59.999 guest + 59.999 interpolated), with no
  interval above 33.33 ms. These exclude loading/capture readback and do not
  certify display scanout or campaign stability. Reviewed frames retain the
  sky/train/scenery; intentional-motion versus matched-reference fidelity
  remains open. A 155-second validation-enabled Mansion run now shows actual
  spirits, red eyes and trails alongside hall geometry/fire in paired images.
  MAC-049 retains its remaining acceptance checks. No new runtime fix or
  Windows validation is claimed.
  [Evidence](STATUS.md#integrated-renderer-pacing-and-spirit-observation-2026-10-19).
- **Representative common-renderer scenes (2026-10-19):** The documentation
  commit on `codex/shared-scene-acceptance` depends on `0ded9e0`; the unchanged
  runtime is `5271401`. Short native MoltenVK Mansion exterior and entrance-hall
  runs retain geometry, foliage/rain, lighting, characters and chandelier/fire
  in paired guest/intermediate captures after synthetic turning/movement.
  A temporary affine-off control reproduced the guest image to within one
  channel unit at one pixel, with two other pairs exactly equal. No missing-rain
  correction was justified and no runtime/settings change was made. All three
  runs exited normally without validation errors. Original-Xbox fidelity,
  spirits and Windows runtime comparison remain open; this does not close the
  entry or establish a new FPS result.
  [Evidence](STATUS.md#shared-renderer-mansion-scene-acceptance-2026-10-19).
- **Shared paired-frame evidence (2026-10-19, commit carrying this update):**
  `codex/shared-interpolation-capture` depends on `codex/mac-moltenvk-default`
  at `65330e8` and its unmerged renderer stack. The common interpolation
  recorder now owns one-shot guest/intermediate capture on both host paths;
  legacy GL no longer carries a separate capture implementation. A receipt
  identifies the guest frame, actual blend and readback/present outcomes; the
  Mac runner rejects stale, incomplete and malformed evidence. Short Mac
  Vulkan Khallos and opening-gameplay pairs passed without validation errors.
  Windows and legacy GL live capture remain unrun. Sky fidelity is still open:
  the new comparison tool is not a sky correction.
  Sources: `patches/*-shared-interpolation-capture.patch`,
  `tools/testing/test-native-mac.py`, `tests/interpolation_capture.c`;
  [evidence](STATUS.md#shared-guest-and-intermediate-frame-evidence-2026-10-19).
- **Shared scene interpolation (2026-10-19, commit carrying this update):**
  `codex/shared-scene-interpolation` depends on `codex/khallos-sky` at `683026d`
  and its unmerged shared-renderer base. Both entry points now select the same
  title transform registers. The shared HLE path recognizes draws that use a
  precomposed projection/model-view matrix, recovers and interpolates its
  affine transform, and reapplies the projection instead of presenting the
  unchanged matrix. Portable production-code tests cover motion, FOV changes,
  unrelated stale registers, endpoints, HUD/unmatched draws and camera cuts.
  Short Mac Khallos captures retain scene geometry. Both drivers passed RHI and
  replay contracts; Windows runtime validation remains pending. A 15-second
  MoltenVK sample measured 119.802 host API presents/s, but a separate gameplay
  validation found portability swizzle errors. Fix feature negotiation and add
  format readback coverage before changing the default. Shared one-shot paired
  captures also remain missing. This does not fix the static sky or certify
  sustained 120 FPS. [Evidence](STATUS.md#shared-scene-interpolation-and-khallos-pacing-2026-10-19).
- **Portability swizzle repair (2026-10-19, commit carrying this update):**
  `codex/vulkan-portability-swizzle` depends on `f1f4236`. The common Vulkan
  device now queries/enables the optional image-view swizzle feature required
  by the existing A8/BGRX views. Shared resource tests cover those formats and
  ordinary-format controls; both Mac drivers pass, and a 95-second Khallos
  validation run reports no validation errors. Windows execution remains
  pending. This fixes device feature negotiation, not the static sky.
  Sources: `patches/*-vulkan-portability-swizzle.patch`,
  `tests/support/rhi_texture_resources.h`; [evidence](STATUS.md#vulkan-portability-texture-swizzles-2026-10-19).
- **Khallos investigation (2026-10-19):** The commit carrying this update on
  `codex/khallos-sky`, based on unmerged `codex/shared-renderer-default` at
  `d30b652`, adds the Mac SDL adapter for the shared frame-replay tool and a
  one-frame GPU readback contract. The dark/stretched Khallos sky reproduces
  at 60 guest presents with no interpolation and in isolated sky draws on
  both pinned Vulkan drivers. Sky root cause and visual correction remain
  open; Windows scene comparison is pending. Do not mark this entry verified
  from the new replay contract. Next acceptance: reproduce the cause in a
  shared regression, review a corrected matched sky capture, then check a
  short control scene on each available host.
  [Evidence and limits](STATUS.md#khallos-sky-isolation-and-native-frame-replay-2026-10-19).
- **PC behavior:** The Windows renderer evaluates multiple texture inputs and Xbox texture/combiner modes for layered materials and effects.
- **PC source:** Audited revision above; `external/ts3FPrecomp/src/d3d/d3d8_combiners.c`; native counterparts `src/native/gl_fragment.inc`, `src/native/gl_vertex.inc`, `src/native/gl_bridge.inc`.
- **PC validation:** Windows shader implementation exists; scene-by-scene material fidelity is not established by this audit.
- **Existing Mac support:** Four independent 2D samplers and T0-T3 coordinates now reach the fixed vertex, GPU program and CPU fallback paths. Ordinary/projected 2D, linear coordinate scaling, passthrough and absent stages are implemented, with per-stage wrap/filter state. High-bit FVF coordinate-size flags are no longer mistaken for programmable shader handles.
- **Mac adaptation needed:** Implement remaining dependent/dot-product and other Xbox texture modes, plus cube/volume integration with MAC-006. Continue matched scene/material checks and frame-time profiling; the 120-present setting is a target, not a demonstrated sustained rate.
- **Mac acceptance checks:** Use distinguishable multi-stage GPU fixtures plus matched mission captures; verify fallback agreement, alpha/depth behavior and interpolation. Reproduce the reported disappearing world separately before attributing its cause.
- **Mac implementation and evidence:** Implementation included with this backlog revision on `mac-port`: `src/native/renderer_gl.c`, `gl_fragment.inc`, `gl_vertex.inc`, `gl_bridge.inc`, `multitexture_selftest.inc`, `vertex_selftest.inc` (all latter paths under `src/native/`), and `patches/graphics/rendering/0840-shared-linear-formats.patch`. Real-GPU four-stage fixtures passed distinct coordinates, projected/linear inputs and FVF/GPU/CPU agreement. Reviewed first-mission captures show improved terrain, weapons and NPC surfaces. Complete fidelity and the reported world-disappearance case remain unverified. [Validation and evidence](STATUS.md#mac-backlog-renderer-camera-and-prompts-2026-10-15).
- **Branch reconciliation (2026-10-17):** `mac-port` at `1a8f4dd` contains
  `5cba184`, whose actual diff adds dependent AR/GB and DOTPRODUCT/DOT_ST
  sampling, signed mappings/source selection and GPU fixtures despite its
  debug-oriented commit title. This implementation is **absent from main
  `0dc9fea`**. Review and integrate that specific work on a focused branch before
  reimplementing it, preserving newer main renderer changes. The prior branch's
  recorded GPU results are historical; rerun native fixtures and matched scene
  checks after integration. Do not merge the entire older branch just to obtain
  this feature: its packaging/settings-test changes need separate review.

- **Current integration (2026-10-17):** `codex/mac-dependent-textures`
  reapplies only the four renderer/test files from `5cba184` on build fix
  `bab79f0`, retaining main's viewport/scissor/channel-clear work. All 36
  dependent-mode GPU cases (12 each through FVF, GPU programs and CPU fallback)
  and the complete native self-test suite pass on this Mac. Other texture modes
  and scene fidelity remain pending.
  [Evidence](STATUS.md#native-dependent-texture-integration-2026-10-17).

- **Fog input adaptation (2026-10-17):** `codex/mac-fog-rendering` now passes
  programmed fog through GPU and CPU vertex paths into the final combiner,
  using the shared PC unwritten marker for the sky/particle fallback. All 49
  GPU fog cases and the full native suite pass. Bounded Khallos/first-mission
  checks retain visible world and weapons; Khallos exterior sky remains dark
  and stretched. Remaining shader inputs/modes and matched scene fidelity are
  still open; this is not a sky fix. [Evidence and limits](STATUS.md#native-fog-combiner-input-2026-10-17).

- **Shared renderer preparation (2026-10-19):** The commit carrying this entry
  on `codex/shared-renderer-host`, dependent on `1a650b5`, introduces `RhiHost`
  for event pumping and physical drawable dimensions. Windows common device
  code and the native GL renderer consume that interface; SDL/Cocoa owns its
  main-thread events and cached worker dimensions. Twelve common core files
  compile on Mac; CI registers the same contract for both hosts. This prepares
  common graphics integration but does not add missing texture modes or select
  a new Mac GPU backend. Windows execution and common-RHI gameplay remain
  pending. [Evidence](STATUS.md#shared-renderer-host-interface-2026-10-19).

- **Common Vulkan backend (2026-10-19):** The commit carrying this entry on
  `codex/shared-vulkan-mac` depends on `e382e50`. The production RHI now creates
  SDL/Metal surfaces and compiles the same PC HLSL on Mac. The shared menu pixel
  contract and swapchain checks pass on the pinned SDK's two Mac drivers; the
  standalone probe needs no game files. This supplies a backend for the common
  frontend rather than a second shader implementation. The native game still
  uses GL; connect its common Xbox frontend, window creation and overlay host
  services next. Missing modes and scene acceptance remain open, and Windows
  RHI/WARP execution is pending. [Evidence](STATUS.md#common-vulkan-backend-on-mac-2026-10-19).

- **Normal shared builds (2026-10-19, commit carrying this entry):**
  `codex/shared-renderer-default` depends on `42462f0`. Normal Mac builds now
  choose the common Xbox/Vulkan frontend where the pinned SDK supports the OS
  (currently macOS 26+). Older hosts and explicit `--renderer gl` keep the legacy
  adapter; its missing features remain compatibility exceptions. SDK failure
  stops before patching/generation, and only `--setup` installs the pinned SDK.
  Local normal build, full native suite, launcher check, 101 portable checks
  and bounded startup pass. Both hosted jobs pass at `a4261ee` in run
  39314716127, including Mac Vulkan/Windows WARP GPU contracts. Full visual
  parity, Khallos sky and movie support remain open.
  [Evidence](STATUS.md#common-renderer-selected-by-normal-mac-builds-2026-10-19).

- **Common native frontend (2026-10-19):** The commit carrying this entry on
  `codex/shared-native-renderer` depends on `4d23a6d`. Opt-in Vulkan builds compile
  the same PC Xbox device, resources, shaders, combiners and display code. SDL
  window/event and CoreText text adapters replace the remaining host services.
  Shared HLSL dialect and point-output fixes address actual boot/mission errors;
  synthetic GPU contracts and bounded first-mission captures provide limited
  evidence. Default GL remains available. Next: common interpolation depth
  isolation, host-present measurement, fullscreen/settings integration and
  targeted cube/volume/movie/framebuffer/advanced-combiner scenes. Windows
  execution and complete scene parity remain pending.
  [Evidence](STATUS.md#common-xbox-renderer-on-native-mac-2026-10-19).

- **Shared interpolation depth (2026-10-19):** The commit carrying this entry
  on `codex/shared-interpolation-depth` depends on `2c63fbf`. The common PC/Mac
  renderer now gives every depth subresource touched by replay its own fresh
  depth/stencil history, retains it across target switches, and restores guest
  target bindings after replay. This moves the GL preservation behavior into
  common source and extends it to custom depth surfaces. The asset-free RHI
  contract and native Xbox-device checks pass on Mac; Windows WARP execution,
  host-present timing and broader scene parity remain pending.
  [Evidence](STATUS.md#shared-interpolation-depth-isolation-2026-10-19).

- **Shared presentation metrics (2026-10-19):** The commit carrying this entry
  on `codex/shared-present-metrics` depends on `3e78f7c`. Common RHI and fallback
  GL use `src/graphics/present_metrics.h` for bounded API-return timing and
  guest/interpolated/other counters. Vulkan acquire/present skips and errors no
  longer report success; the shared HLE only counts accepted replay presents and
  does not reuse a stale scheduling timestamp on failure. Windows runs the same
  source/fault contracts in CI; its execution and stable-120 gameplay remain
  pending. [Evidence](STATUS.md#shared-presentation-outcomes-and-timing-2026-10-19).

- **Hosted validation setup (2026-10-19):** The shared stack through `a0c8a1a`
  is published. Mac CI passes its source contracts but cannot yet reach GPU
  validation because the SDK CDN rejects urllib's default client. The dependent
  `codex/shared-platform-validation` change identifies the downloader and keeps
  the existing archive/version/checksum pins. Local regression checks pass;
  Windows subsequently passed 195 runtime checks, including the common renderer
  compile checks and WARP GPU contract, with two test-harness failures. Native
  path comparison and explicit interpolation fixture hooks fix those failures;
  the published rerun at `083e57c` passes Mac source/release checks and the actual
  hosted MoltenVK RHI GPU contract. The Windows rerun also passes its complete
  workflow, including 198 runtime checks and WARP GPU validation. This does not close
  renderer fidelity or scene acceptance. [Evidence](STATUS.md#shared-platform-ci-validation-2026-10-19).

- **Common settings acceptance (2026-10-19):**
  `codex/shared-native-window-acceptance` depends on `37b4af9`. The existing
  Cocoa graphics and SDL host fixtures now run against both native renderers.
  The common Vulkan device passes actual resolution/FOV Save, Cancel and
  failed-save rollback, then renders/readbacks/presents successfully. Automated
  resize/fullscreen/restore presentation also passes on Vulkan. GL repetition
  is pending because concurrent launcher work reconfigured the shared build
  directory; its source/output were preserved. Physical input and broader scene
  acceptance remain pending.
  [Evidence](STATUS.md#common-renderer-mac-settings-acceptance-2026-10-19).

- **Khallos motion-blur investigation (2026-10-19):** The documentation commit
  on `codex/khallos-sky-replay` depends on shared paired capture `5271401`.
  Xbox reference screenshots also show muted sky and moving-train streaks;
  preserve intentional effects rather than treating darkness/streaking alone
  as a defect. Three controlled before/after-overlay pairs show no sky changes
  outside the overlay rows. The temporary diagnostic was removed and the
  production Mac build restored successfully. A matched reference is still
  needed to distinguish replay errors from intentional blur; no sky fix or
  Windows scene parity is claimed. Status remains In progress.
  [Evidence](STATUS.md#khallos-motion-blur-reference-and-overlay-isolation-2026-10-19).

### MAC-006 — Cube and volume texture resources

- **Status:** In progress
- **PC behavior:** Windows has resource factories and sampling paths for cube/volume textures.
- **PC source:** Audited revision above; `external/ts3FPrecomp/src/d3d/d3d8_device.c`, `external/ts3FPrecomp/src/d3d/d3d8_combiners.c`; `src/native/renderer_gl.c`, `src/native/gl_bridge.inc`.
- **PC validation:** Factories are implemented on the Windows path; usage and fidelity in particular game scenes need targeted evidence.
- **Existing Mac support:** Default GL cube/volume creation returns `E_NOTIMPL`; cube metadata/level queries return false. The opt-in common Vulkan game now compiles the PC factories; face/slice/mip and scene acceptance remain unverified (see MAC-005).
- **Mac adaptation needed:** Add resource lifetime, face/slice/mip uploads and sampler integration. Coordinate with MAC-005; establish actual title usage before prioritizing expensive format work.
- **Mac acceptance checks:** Synthetic face/slice/mip checks and resource release pass on the GPU; validate a game scene if one uses these resources. Unsupported formats must fail explicitly.
- **Mac implementation and evidence:** Remaining work pending. Existing coverage:
  [native adapter validation](STATUS.md#native-mac-settings-and-graphics-adapters-2026-10-15).
- **Shared acceptance contract (2026-10-19):** The commit carrying this entry on
  `codex/shared-texture-contract`, dependent on `296f970`, adds one asset-free
  RHI GPU fixture for Windows and Mac. It checks cube faces, volume voxels,
  mip chains, pitched uploads, isolated updates, restricted views, retain/release
  and unknown-format rejection using common shaders/expectations. Local syntax
  and CI/backlog checks pass. Both hosted jobs and the Mac Vulkan/Windows WARP
  GPU contracts pass at `e864831`. The Xbox factories and title scenes remain separate
  acceptance work.
  [Evidence](STATUS.md#shared-cube-and-volume-resource-contract-2026-10-19).
- **Xbox factory follow-up (`4419205`, same published branch):** Adds the production
  resource implementation to both GPU fixtures. It covers automatic mip counts,
  linear/swizzled Lock/Unlock uploads, face/voxel sampling and child lifetime
  after parent release. Both hosted Mac Vulkan and Windows WARP execute these
  cases successfully at `f2c2261` (run 39308234267).
  Palette/compressed formats, other texture modes and actual title usage are
  outside this fixture and remain unverified.
- **Integrated native acceptance:** `3020167` combines the renderer and completed
  launcher sources. The real Xbox factory fixture passes locally through
  MoltenVK on Apple M5 Pro, including linear/swizzled uploads and child lifetime.
  Other formats and scene usage remain open.
  [Evidence](STATUS.md#shared-native-runtime-and-launcher-integration-2026-10-19).

### MAC-007 — Screen-copy effects and image surfaces

- **Status:** In progress
- **Shared clipping/acceptance (2026-10-19, commit carrying this entry):**
  `codex/shared-screen-copy-contract`, based on unmerged `c553f9a`, replaces
  separate rectangle clipping with `src/graphics/screen_copy_rect.h` for PC,
  common Vulkan and native GL. A reproduced negative-destination copy selected
  incorrect pixels in the common renderer; paired source/destination clipping
  fixes it and safely rejects empty/extreme rectangles. The real production
  copy/shader/resource fixture is registered for both hosted GPU backends.
  Local MoltenVK contract and complete GL tests pass; hosted Windows WARP and
  Mac Vulkan contracts pass at `42462f0` (run 39313104097). A bounded Khallos
  run retains visible exterior/weapon but stretched sky; matched title-effect
  scene fidelity remains pending.
  [Evidence](STATUS.md#shared-screen-copy-clipping-and-gpu-contract-2026-10-19).

- **PC behavior:** Windows can copy the full backbuffer or a rectangle into a texture for later draws.
- **Issue #21 channel-clear follow-up (this revision):** The lobby uses destination
  alpha as a temporary text mask. `src/graphics/pc_clear_rhi.inc` and
  `patches/graphics/rendering/0910-channel-clears.patch` preserve partial colour clears,
  including clearing alpha without erasing the finished RGB image. Capture
  version 8 records the channel flags and still accepts older captures.
  `tests/graphics/pc_clear.c` checks all 15 masks on the Windows GPU; portable mapping
  and capture tests are in `tests/graphics/test_channel_clears.py`. The shared mapping
  also feeds Mac, so `src/native/renderer_gl.c` now applies the corresponding
  GL colour mask and restores the existing render state. No additional native
  adapter is needed. Native GPU coverage now passes all 192 combinations of
  channel masks and independent depth/stencil clears at Original, 1080p and
  offscreen sizes, including alpha readback, viewport/rectangle clipping and
  restoration of write masks. [Native evidence](STATUS.md#native-channel-clear-gpu-validation-2026-10-17). Remaining scene acceptance:
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
- **240 FPS decoder coverage (2026-10-24, PR #328):** The Windows real-file
  fixture now compares EA and Free Radical intros at native60/120/144/180/240,
  with Original60 as the controlled-clock reference. Decoded picture/timestamp/
  PCM hashes match exactly; existing duration, polling and cleanup checks pass.
  GPU/audio-device output remains mocked. This adds no Mac decoder adapter or
  evidence of movie color/audible synchronization. See STATUS for results.
- **Native-rate pacing follow-up (`3c2fcc2`, local):** `pc_native_movie_pacing.h`
  provides per-playback 60/120/144 polling through the existing platform clock/
  wait interface. The additive `*-native-movie-pacing.patch` connects it to the
  existing Windows host XMV player and closes its timer with playback. Host
  movie decoding is still absent from the Mac path; when implementing it,
  consume this shared helper rather than adding independent pacing rules.
  Offline timing and actual-update-body fixtures cover cadence, delayed work,
  queue retry, timestamps, EOF and cleanup using mocked decoder/audio output.
  Native playback, audible sync, skip latency and full movie coverage remain
  acceptance work; no Mac movie support is claimed from these fixtures.
  Windows build/built-in checks, 152 shared checks and 21 patch-upgrade checks
  pass. A windowless real-file check runs the existing Windows host player and
  FFmpeg decoder with controlled time, comparing all EA-intro image/timestamp/
  PCM hashes across original 60 and native 60/120/144. GPU/audio-device output
  are mocked; this does not validate audible sync or native Mac playback.
  Real Windows waitable-timer runs also preserve all decoded EA-intro content
  at 60/120/144 and verify the playback timer handle is invalid after close.
  Poll completion includes decode/test hashing; display cadence and actual
  Mac clock/wait execution remain separate acceptance checks.
- **PC behavior:** Windows uses FFmpeg to decode XMV movies, draws their frames and submits movie PCM audio.
- **PC source:** Audited revision above; `external/ts3FPrecomp/src/d3d/d3d8_movie.c`, `tools/build/fetch-video.ps1`, `tools/build/build-native-mac.py`, `src/native/gl_bridge.inc`.
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
  `src/interface/pc_display_menu.c` uses the game's native arrow choice widget. Sources:
  `src/interface/pc_display.c`, `tools/launcher/DisplayModes.cs`, `SettingsPage.cs`;
  implementation `e3e99c1`, integrated on `release/resolution-v0.2.0-alpha.4`.
  SDL discovery otherwise retains its existing behavior. Mac mode discovery,
  native arrow spacing, lower modes and Apply/Keep/Revert/timeout still need
  native verification; Windows results do not verify Mac support.

- **Status:** Awaiting validation
- **PC behavior:** Display settings offers Apply/Keep/Revert with a 15-second automatic rollback, persisted resolution and widescreen camera/HUD handling.
- **Issue #19 decal projection follow-up (`19-floating-decals`):** `src/interface/pc_display_menu.c`,
  `src/interface/pc_display_menu.h` and `patches/graphics/rendering/0810-decal-projection.patch` make
  both depth-biased decal passes use the active camera's horizontal and vertical
  aperture, preserving their depth bias. This is shared guest code; the existing
  Mac build includes the same manual wrappers and display source, so no native
  adapter is required. Mac rendering remains unverified. Acceptance: in Arcade,
  shoot a wall and strafe/look around at default, 4:3, 16:9 and ultrawide modes;
  impacts must stay attached and HUD proportions must remain unchanged. Also
  check cinematic camera zoom and split-screen cameras with isolated settings.
- **PC source:** Audited revision above; `src/interface/pc_display.c`, `src/interface/pc_display_menu.c`, `src/interface/pc_display_renderer.inc`, `tests/graphics/pc_display_game.py`, `tests/graphics/pc_display_gameplay.py`.
- **PC validation:** [Display settings and texture preview](STATUS.md#display-settings-and-texture-preview--october-15) records Windows apply/save, timeout rollback, 4K and ultrawide render-target checks.
- **Existing Mac support:** Mac shares the menu/model and has working resolution targets, Cocoa save/reload and fullscreen. The in-game timeout interaction was not exercised end-to-end.
- **Mac adaptation needed:** Finish Keep/save/reopen, explicit Revert and representative aspect/HUD checks in the actual menu. Automatic timeout and return to first-mission gameplay have now been exercised.
- **Mac acceptance checks:** From actual game menus: apply, keep, reopen, revert and let a preview expire; test paused Options and return to gameplay. Confirm saved mode and render targets agree, using isolated configs.
- **Mac implementation and evidence:** Existing implementation unchanged. In the final bounded run, paused Options opened Display Settings, previewed 2560x1440, displayed the countdown and automatically restored 1920x1080 after 15 seconds. The isolated saved mode remained 1920x1080, and the run returned to visible first-mission gameplay. Apply/Keep/reopen and explicit Revert remain pending. [Validation and evidence](STATUS.md#mac-backlog-renderer-camera-and-prompts-2026-10-15).

### MAC-011 — Real texture-pack gameplay coverage

- Classic request deferral (commit carrying this entry): `src/textures/texture_pack.c`
  and `src/native/gl_texture_pack.inc` leave new replacements unrequested until
  Enhanced binds them. Cached switching, original fallback and content-change
  cancellation remain. No new Mac adapter is needed; both renderers implement
  the policy. Windows CPU/adapter tests pass all 24 cases, including deferred
  content changes and late activation in synchronous/background modes. Actual
  Mac gameplay, mode switching and pacing remain **Awaiting validation**.

- 2026-10-23 streaming integration correction (commit carrying this entry):
  the initial common-renderer Present-only pump missed Future Perfect's Swap
  route. Earlier background-on gameplay measurements did not include completed
  replacements and are withdrawn. Completion now belongs to shared present_resolve;
  legacy GL already funnels Swap through its Present adapter. Production-body
  tests fail before this correction and pass after it. Windows D3D11 build
  86b2c1c completed 132 replacement uploads in a bounded Venice Enhanced run;
  two reviewed frames showed no obvious blank/corrupt surfaces. A later actual
  Windows Video Options run applied Classic, reopened the page to confirm the
  effective value, then applied Enhanced and resumed gameplay in both modes.
  Apply disabled after each commit; inspected stills had no obvious blank/corrupt
  surfaces. Original60/native120 Venice pacing checks also completed; see STATUS
  for limitations. Replay still suppresses host shortcuts. Mac scene checks,
  broader Windows coverage and physical shortcut testing remain **Awaiting validation**.

- Background texture reads (commit carrying this entry): `texture_pack_stream.h`
  shares queue, cancellation and staging policy between common RHI and legacy
  GL; thin Windows event/lock and POSIX pthread adapters run disk work. GPU
  creation remains on each renderer thread; the GL adapter restores binding.
  Windows loader/queue/GL-stub and related native tools: 55 passed, two host-only
  skips. Native Mac CI at c4baddf executes the queue/loader fixtures successfully
  within 446 passing runtime tests (76 host/feature skips, 18 subtests); common-RHI
  asset-free GPU contracts also pass. Actual texture arrival GPU behavior, scene
  transitions and Enhanced appearance remain **Awaiting validation**. No new Mac feature
  duplication is needed. Acceptance includes Classic/Enhanced switching during
  queued loads, owner destruction/content changes, bounded memory and 60/120/144
  scene pacing on both Mac renderers; see docs/project/STATUS.md.

- PR #289 upgrade follow-up: shared `Core.InstallPending` now consumes packaged
  retired-icon hashes and transactionally removes only unchanged bundled pairs.
  Custom pairs survive; recovery/rollback restore removals. Both package builders
  receive the policy from `build_textures.ensure_pack`; no OS adapter was added.
  Windows launcher and packaging checks pass; native Mac shared fixtures run
  in PR CI. Mac scene acceptance remains **Awaiting validation**. Deployment
  needs a launcher and game release carrying these changes.

- 2026-10-22 character icon removal (commit carrying this entry): 159 replacements retired;
  164 known selection/profile icon keys intentionally use original assets.
  Shared pack now has 2,046 entries; character model materials are unchanged.
  No runtime or host adapter change is required. Windows offline hashes, pack
  audit and planner regression checks pass; actual Mac menu/GPU behavior remains
  **Awaiting validation**. See docs/project/STATUS.md; no gameplay run.

- 2026-10-21 #74 repository texture update (commit carrying this entry):
  700 new keys, 2,205 total, using the same audited DDS/JSON for both hosts.
  The shared LFS exporter now reads DDS stdin without a stale pointer-file size
  hint; the existing-pointer regression failed before the fix and passes now.
  Windows texture/build/planner/backlog checks: 22 tests and 21 subtests pass.
  No runtime adapter changed. Native Mac export and GPU/scene acceptance remain
  unverified; **Awaiting validation** is unchanged. No gameplay or release.

<!-- menu-expanded-progress -->
- 2026-10-21 expanded menu pass (included in the #74 texture upload):
  421 source-faithful schematic/UI/effect additions plus 158 newly detailed
  portraits and 63 scene thumbnails in this continuation. Same keyed RGBA8 files serve both hosts;
  26 additional cards recover matching native 512px original artwork.
  no runtime or adapter changes. Windows offline parser, alpha/pixel and full
  pack audits pass. All-menu coverage and model associations remain incomplete.
  Actual Windows/Mac menu rendering and GPU residency await final validation;
  no game launched. See docs/textures/main-menu-textures.md and latest STATUS entry.
<!-- /menu-expanded-progress -->

- 2026-10-21 menu follow-up (included in the #74 texture upload): corrected
  reflection, original-mask logo and two faithfully resampled font atlases now
  pass offline acceptance. Same keyed RGBA8 replacements serve both hosts; no
  adapter change. Windows DDS parsing and full Python pack audit pass; actual
  font/logo rendering, menu animation and Mac GPU behavior remain unverified.
  No gameplay launched. Awaiting validation; see docs/textures/main-menu-textures.md.

- 2026-10-21 main-menu focus (documentation included in the #74 texture upload): mapped
  logo/font/background and candidate schematic keys using existing Windows
  evidence. One 1254-square background draft is held, with no installed DDS or
  runtime/adapter changes. The same eventual replacement pack serves both hosts;
  Mac and Windows mapped appearance/alpha checks remain pending. Status remains
  Awaiting validation. See docs/textures/main-menu-textures.md and docs/project/STATUS.md.

- 2026-10-21 unlocked test launch correction (branch
  `fix/test-profile-texture-pack`): shared `testing_profile.prepare_run` now
  connects `assets/textures` explicitly while starting in Classic mode. The
  native Mac unlocked runner already imports that environment, so no adapter
  change is needed. Windows D3D11 startup logs confirm replacement uploads;
  shared/native preparation tests pass on Windows. Mac acceptance remains an
  unlocked scene with F10 toggling and replacement uploads visible in its log;
  native visual testing has not been performed for this correction.

- 2026-10-20 bundled texture delivery: shared tools/textures/build_textures.py prepares
  the Git LFS pack in assets/textures for Windows and native Mac (PR #235);
  both game packagers stream it
  directly into their ZIP. Shared Core.cs accepts bounded texture members,
  preserves existing pairs and settings, and cleans only staged payloads with
  existing OneDrive/link checks. Launcher 1.2.22 includes this behavior. Local
  Windows build/regressions passed before the delivery correction; 221 Python
  tests pass after it (one platform skip). Updated host CI is required.
  Native Mac install/cleanup fixture is registered in NativeTests.cs; host
  execution is evaluated in PR CI. **Awaiting validation** remains for real
  texture scene/GPU/display/residency coverage. See docs/project/STATUS.md bundled texture
  dependency entry. No graphics adapter or guest-clock behavior changed.

- 2026-10-20 texture build dependency and game ZIP inclusion (local #74 source):
  tools/textures/build_textures.py shares audit/download/cache policy across Windows and
  native Mac builders. play-native-mac.py automatically selects the prepared pack.
  Shared tools/launcher/Core.cs accepts bounded keyed DDS/JSON in game packages,
  seeds missing texture pairs, preserves existing pairs, and safely cleans staged
  payloads. Windows launcher build/regressions and 183 Python tests pass;
  one platform test skips. Mac native host build/install/GPU checks remain pending,
  so **Awaiting validation** is unchanged. No separate Mac policy implementation.
  Missing published textures-v0.1.0-alpha.1 prevents fresh-clone remote validation;
  local 1,505-texture reuse passes. No release/push/gameplay. See docs/project/STATUS.md texture
  build dependency entry; no cross-device synchronization claimed.

- 2026-10-20 texture batch 255: seven metal surfaces installed, one held;
  1,505 installed. Seven new Windows parser checks and full Python audit pass;
  previous 1,498 DDS hashes/checker unchanged. Six RAM reviews replay exactly
  after retiring new reference exports. Pack 1,165,240,310 bytes, temporary
  snapshot 267,500,128 bytes, within caps. No runtime/adapters/gameplay changes.
  **Awaiting validation** for Mac GPU, mapped scenes, display modes and residency.
  See docs/project/STATUS.md batch 255 and private review/hybrid-255 evidence.

- 2026-10-20 texture batch 254: five metal surfaces installed, three drafts held;
  1,498 installed. Fresh full Python audit and five new Windows parser checks
  pass; previous 1,493 DDS hashes/checker unchanged. Five RAM reviews replay
  exactly after reference retirement. Recoverable JSON formatting compaction and
  two never-previously-listed cache removals reclaimed temporary headroom.
  Pack 1,162,614,842 bytes; temporary snapshot 267,413,737 bytes, below caps.
  No gameplay or runtime adapters changed. **Awaiting validation** remains for
  Mac GPU, mapped materials/seams/light channels, display modes and residency.
  See docs/project/STATUS.md batch 254 and private evidence; no cross-device validation claim.

- 2026-10-20 texture batch 253: four metal replacements installed, four drafts
  held for tone/geometry correction; 1,493 installed. Fresh full Python audit and
  four new Windows parser checks pass; prior 1,489 hashes/checker unchanged.
  Five RAM comparisons replay exactly after retiring eight verified original
  exports (194,487 bytes). Pack 1,159,466,244 bytes; temporary snapshot
  268,188,079 bytes, below caps. No runtime adapters or gameplay changed.
  **Awaiting validation** remains for Mac GPU, mapped materials/seams/light
  channels, display modes and residency. See docs/project/STATUS.md batch 253.

- 2026-10-20 texture batch 252: eight metal-family replacements installed;
  total 1,489. Fresh full Python audit and eight new Windows parser checks pass;
  prior 1,481 DDS hashes/checker identity unchanged. Six RAM comparisons include
  enlarged vent-grid review and replay exactly after retirement of eight new
  source exports (273,500 bytes). Pack 1,158,327,982 bytes; temporary snapshot
  268,099,093 bytes, below caps. No runtime adapter changes or gameplay.
  **Awaiting validation** remains for Mac GPU, mapped seams/light channels,
  material lighting, display modes and residency. See docs/project/STATUS.md batch 252.

- 2026-10-20 texture batch 251: seven metal-family replacements installed;
  one brown seam draft held for tonal drift. Total 1,481 installed. Fresh full
  Python audit and seven new Windows parser checks pass; prior 1,474 DDS hashes
  and checker identity unchanged. Five RAM comparisons replay exactly, including
  after verified retirement of eight new original exports (325,816 bytes).
  Pack 1,156,007,778 bytes; temporary snapshot 268,023,162 bytes, below caps.
  No runtime adapter changes or gameplay. **Awaiting validation** remains for
  Mac GPU, mapped seams/light channels/materials, display modes and residency.
  See docs/project/STATUS.md artwork batch 251 and private provenance.

- 2026-10-20 texture batch 250: five wood/panel/equipment replacements installed;
  three drafts held for brightness or missing component detail. Total 1,474
  installed. Fresh whole-pack Python audit and five new Windows parser checks
  pass; prior 1,469 hashes/checker identity unchanged. Five RAM comparisons
  replay exactly. Pack 1,153,294,936 bytes; temporary snapshot 267,950,496 bytes,
  below caps. Five review JSON records compacted with semantic equality and
  byte-exact gzip recovery; no artwork changes. No runtime adapter changes or
  gameplay. **Awaiting validation** remains for Mac GPU, mapped seams/materials,
  display modes and residency. See docs/project/STATUS.md artwork batch 250.

- 2026-10-20 texture batch 249: four material/equipment replacements installed,
  four drafts held for markings, brightness or band geometry. Total 1,469 installed.
  Fresh whole-pack Python audit and four new Windows parser checks pass; prior
  1,465 hashes/checker identity unchanged. Six RAM comparisons replay exactly.
  Pack 1,151,369,672 bytes; temporary snapshot 268,172,153 bytes, below caps.
  Actual Windows allocation queried without errors; provenance retains source
  sizes and resampling. No platform adapter/runtime change or gameplay.
  **Awaiting validation** remains for Mac GPU, mapped seams/markings, material
  lighting, render modes and residency. See docs/project/STATUS.md artwork batch 249.

- 2026-10-20 hybrid queue reconciliation: shared Python planner accepts the
  environment_material alias and routes mixed sensitive categories to specialist
  review. 145 keys join bounded material batches; 1,191 move to specialist review.
  Eight planner tests pass on Windows; already registered in runtime-patches CI.
  No OS-specific adapter or runtime change. Mac execution is unrun. All 1,465
  installed DDS hashes unchanged and fresh Python audit passes; native evidence
  stays batch 242. No new artwork or gameplay. **Awaiting validation** remains
  for Mac GPU, mapped scenes, channel semantics, display modes and residency.

- 2026-10-20 inventory batch 247: final 243 known keys reviewed (195 opaque,
  48 nonopaque). Exact-set check proves all 6,812 supported catalog keys have a
  provisional appearance/alpha classification; zero unclassified. This closes
  the known-key first pass only, not discovery, category/use or scene validation.
  Thirty-two RAM sheets replay exactly; full provenance is 42,410-byte gzip.
  No new art installed. All 1,465 DDS hashes unchanged; fresh full Python audit
  passes and native evidence stays batch 242. Pack 1,149,619,742 bytes; temporary
  268,092,924 bytes, below caps. **Awaiting validation** remains for Mac GPU,
  mapped scenes, channel semantics, display modes and residency. No gameplay.

- 2026-10-20 inventory batch 246: 128 RGB/alpha pairs reviewed (87 opaque,
  41 nonopaque); 6,569 provisionally classified and 243 unclassified, all needing
  art. The previously unclassified installed disc-error screen is now recorded.
  Sixteen RAM sheets replay exactly; full provenance is 23,704-byte lossless gzip.
  No artwork generated/installed. All 1,465 DDS hashes unchanged; fresh Python
  audit passes, native evidence stays batch 242. Pack 1,149,619,742 bytes;
  temporary 268,034,212 bytes, below caps. **Awaiting validation** remains for
  Mac GPU, mapped scenes, alpha semantics, display modes and residency. No gameplay.

- 2026-10-20 inventory batch 245: 128 RGB/alpha pairs reviewed (90 opaque,
  38 nonopaque); 6,441 provisionally classified and 371 unclassified. Sixteen
  RAM-only sheets exactly replayed; 23,350-byte gzip retains full provenance.
  Alpha-only signage/diagrams, inverse masks and character identity retain
  explicit constraints. No artwork generated or installed. All 1,465 DDS
  hashes unchanged; fresh full Python audit passes, native evidence stays
  batch 242. Pack 1,149,619,742 bytes; temporary 267,996,909 bytes, below caps.
  **Awaiting validation** remains for Mac GPU, mapped scenes, alpha semantics,
  display modes and residency. No gameplay; classification is not completion.

- 2026-10-20 inventory batch 244: 128 original RGB/alpha pairs reviewed
  (93 opaque, 35 nonopaque), bringing provisional classification to 6,313 keys.
  Functional constants, diagrams, exact lettering, character atlases and
  channel-dependent effects retain explicit constraints. Sixteen RAM sheets
  replay exactly; full provenance retained as 25,738-byte lossless gzip JSON.
  No artwork generated/installed. All 1,465 DDS hashes unchanged and fresh full
  Python audit passes; native evidence remains batch 242. Pack 1,149,619,742 bytes;
  temporary 267,960,106 bytes, below caps. **Awaiting validation** remains for
  Mac GPU, mapped scenes, alpha semantics, display modes and residency. No gameplay.

- 2026-10-20 inventory batch 243: 64 original RGB/alpha pairs reviewed
  (43 opaque, 21 nonopaque), bringing provisional classification to 6,185 keys.
  Alpha-carried diagrams, numeral/icon masks, partial-opacity lettering and
  gradients retain explicit specialist constraints. Eight RAM sheets replay
  exactly. No artwork generated or installed; all 1,465 existing DDS hashes
  unchanged and fresh full Python audit passes. Native parser evidence remains
  batch 242. Pack 1,149,619,742 bytes; temporary 267,920,780 bytes, below caps.
  **Awaiting validation** remains for Mac GPU, mapped scenes, channel semantics,
  display modes and residency. No gameplay. See docs/project/STATUS.md inventory batch 243.

- 2026-10-20 texture batch 242: three mechanical replacements installed;
  five drafts held for excessive brightness or marking/component fidelity.
  Total 1,465 installed. Fresh full Python audit and three new Windows parser
  checks pass; prior 1,462 DDS hashes/checker identity unchanged. Six RAM
  comparisons replay exactly, including component/marking focus. Pack 1,149,619,742
  bytes; temporary 267,817,886 bytes, both below caps. Cached originals reused.
  **Awaiting validation** remains for Mac GPU, mapped components, seam sampling,
  reflections/lighting, display modes, alpha and 512 MiB residency. No gameplay or
  runtime changes. See docs/project/STATUS.md batch 242 and private acceptance.

- 2026-10-20 texture batch 241: five clothing/equipment/character replacements
  installed; two character drafts held for facial/component fidelity and one
  generation blocked without retry. Total 1,462 installed. Fresh full Python
  audit and five new Windows parser checks pass; prior 1,457 DDS hashes/checker
  identity unchanged. Six RAM comparisons replay exactly, including face focus.
  Pack 1,148,219,914 bytes; temporary 267,758,522 bytes, both below caps.
  **Awaiting validation** remains for Mac GPU, mapped character identity, skin/seam
  matching, equipment, lighting, display modes, alpha and 512 MiB residency.
  No gameplay/runtime changes. See docs/project/STATUS.md batch 241 and private acceptance.

- 2026-10-20 texture batch 240: six equipment/clothing replacements installed;
  two equipment atlases held for warning-symbol/stamped-marking fidelity.
  Total 1,457 installed. Fresh full Python audit and six new Windows parser
  checks pass; prior 1,451 DDS hashes/checker identity unchanged. Six RAM
  comparisons replay exactly, including enlarged marking focus. Pack 1,146,294,650
  bytes; temporary 267,702,477 bytes, both below caps. Cached originals reused.
  **Awaiting validation** remains for Mac GPU, character/weapon mapping, seams,
  lamps/reflections, display modes, alpha and 512 MiB residency. No gameplay or
  runtime changes. See docs/project/STATUS.md batch 240 and private acceptance.

- 2026-10-20 texture batch 239: seven prop/folded-metal atlases installed;
  one curved atlas held for tonal correction. Total 1,451 installed. Fresh full
  Python audit and seven new Windows parser checks pass; prior 1,444 DDS hashes
  and checker identity unchanged. Five RAM comparisons replay exactly. Sixteen
  archive-reproducible original exports retired, reclaiming 712,272 bytes;
  no denied paths retried. Pack 1,143,495,008 bytes; temporary 267,651,438 bytes,
  both below caps. **Awaiting validation** remains for Mac GPU, mapped scenes,
  reflections, display modes, alpha and 512 MiB residency. No gameplay/runtime
  changes. See docs/project/STATUS.md batch 239 and private acceptance.

- 2026-10-20 texture batch 238: three machinery atlases installed (yellow lamps,
  paired hinges and socket/latch panel); five lighting/layout failures held.
  Total 1,444 installed. Fresh full Python audit and three new Windows parser
  checks pass; prior 1,441 DDS hashes/checker identity unchanged. Eight cached
  originals reused; five RAM comparisons replay exactly. Pack 1,140,170,496
  bytes; temporary 268,300,554 bytes, both below caps. No exports or cleanup.
  **Awaiting validation** remains for Mac GPU, mapped scenes and lamp behavior,
  display modes, alpha and 512 MiB residency. No gameplay/runtime changes.
  See docs/project/STATUS.md batch 238 and private acceptance.

- 2026-10-20 texture batch 237: six masonry/timber/striated replacements installed;
  two panel/red-material tonal/detail failures held. Total 1,441 installed.
  Fresh full Python audit and six new Windows parser checks pass; prior 1,435
  hashes/checker identity unchanged. Five RAM comparisons replay exactly.
  Pack 1,138,071,624 bytes; temporary 268,246,333 bytes, below both caps.
  Original exports retained; JSON whitespace compacted with exact original-byte
  recovery; no denied cleanup retried. No gameplay/runtime changes.
  **Awaiting validation** remains for Mac GPU, mapped scenes, alpha, display
  modes and 512 MiB residency. See docs/project/STATUS.md batch 237 and private acceptance.

- 2026-10-20 texture batch 236: one corrected ochre rock installed; seven
  repeated layout/tonal failures remain held and need a different approach.
  Total 1,435 installed. Fresh full Python audit and one new Windows parser
  check pass; prior 1,434 hashes/checker identity unchanged. Eight existing
  references reused, five RAM comparisons replay exactly. Pack 1,137,325,450
  bytes; temporary 268,061,834 bytes, both below caps. No gameplay/runtime
  changes. **Awaiting validation** remains for Mac GPU, mapped scenes, alpha,
  modes and 512 MiB residency. See docs/project/STATUS.md batch 236 and private acceptance.

- 2026-10-20 texture batch 235: six cliff/stone/concrete replacements installed;
  two brightness/contrast failures held. Total 1,434 installed. Fresh full
  Python audit and six new Windows parser checks pass; prior 1,428 hashes and
  checker identity unchanged. Five RAM comparisons replay exactly. Historical
  JSON compaction preserves original-byte recovery; no denied cleanup retried.
  Pack 1,137,237,494 bytes; temporary 268,002,202 bytes, both below limits.
  No runtime change or gameplay. **Awaiting validation** remains for Mac GPU,
  mapped scenes, alpha, modes and 512 MiB residency. See docs/project/STATUS.md batch 235
  and private review/hybrid-235-acceptance.json.

- 2026-10-20 texture batch 234: six stone/masonry/plaster replacements installed;
  two color/contrast failures held. Total 1,428 installed; fresh full Python
  audit and six new Windows parser checks pass, with prior 1,422 DDS hashes and
  checker identity unchanged. Five RAM comparisons replay exactly; eight new
  reference exports retained after policy denial. Pack 1,134,088,316 bytes;
  temporary 267,970,440 bytes. No gameplay or runtime changes. **Awaiting
  validation** remains for Mac GPU, mapped scenes, alpha, display modes and
  512 MiB residency. See docs/project/STATUS.md batch 234 and private acceptance evidence.

- 2026-10-20 texture batch 233: three small surfaces/trim accepted and installed;
  five drafts held for ridge, atlas, border or brightness errors. Total 1,422
  installed; fresh full Python audit and three new Windows parser checks pass,
  with prior 1,419 hashes/checker identity carried forward unchanged. Five RAM
  comparisons replay exactly; seven reference exports retained after policy
  denial. Pack 1,133,516,898 bytes, below 6 GiB; temporary 267,793,785 bytes,
  below 256 MiB. No runtime changes or gameplay. **Awaiting validation** remains
  for Mac GPU, mapped scenes, transparency, modes and 512 MiB residency. See
  docs/project/STATUS.md batch 233 and private review/hybrid-233-acceptance.json.

- Texture batch 232: six timber/panel replacements installed; two drafts held
  for missing color detail/darkening. Sources seven 1774x887 and one 1254x1254;
  accepted outputs downsampled to four 512x256, one 1024x512 and one 1024x1024.
  Six new Windows native parser passes plus 1,413 unchanged hash/checker
  evidence; all 1,419 pass fresh Python audit. No runtime/adapter changes;
  MAC-011 remains Awaiting validation for Mac GPU/scenes/transparency and
  512 MiB residency. See docs/project/STATUS.md batch 232 and private
  review/hybrid-232-acceptance.json. Parser success does not prove scene fidelity.

- Texture batch 231: all six interrupted requests now have generated sources
  and review decisions; four stone/molding materials installed, two held for
  palette/brightness corrections. Sources three 1774x887 and three 1254x1254;
  accepted outputs downsampled to one 1024x512 and three 1024x1024. Four new
  Windows parser passes plus 1,409 unchanged hash/checker evidence; all 1,413
  pass fresh Python audit. Shared runtime/512 MiB residency unchanged.
  MAC-011 remains Awaiting validation for Mac GPU, scenes and transparency.
  See docs/project/STATUS.md batch 231 and private review/hybrid-231-acceptance.json.

- Texture batch 230: two new opaque rock materials accepted offline and installed;
  six unreturned generation requests remain unfinished after orchestration was
  stopped. Sources 1774x887 and 1254x1254 downsampled to 1024x512/1024x1024.
  Two new Windows parser passes plus 1,407 unchanged hash/checker evidence;
  all 1,409 pass fresh Python audit. No runtime or adapter changes. MAC-011
  remains Awaiting validation for Mac GPU/scenes/transparency/residency.
  See docs/project/STATUS.md batch 230 and private review/hybrid-230-acceptance.json.

- Texture batch 229: three held material corrections accepted/installed; five
  remain held for layout/tone failures. Three new Windows native parser passes
  plus 1,404 unchanged hash/checker evidence; all 1,407 pass fresh Python audit.
  Generated sources were three 1774x887 and five 1254x1254; accepted rust/wood
  upsampled to 2048-square and stone downsampled to 1024-square. No runtime or
  host adapter changes. MAC-011 remains Awaiting validation: parser success
  does not prove Mac GPU, mapped scenes, transparency or 512 MiB residency.
  See docs/project/STATUS.md batch 229 and private review/hybrid-229-acceptance.json.

- Batch 228 (issue #74): eight opaque material edits, six accepted/installed and two held for tone/palette correction. Actual sources seven 1254-square and one 1774x887; accepted outputs five 1024-square and one 1024x512, all downsampled. Six new Windows parser passes plus 1,398 unchanged-hash results; all 1,404 DDS files pass current Python audit. Exact memory review replay survives retirement of this batch’s eight disposable original exports. No runtime/adapter changes or Mac execution. **Awaiting validation** for Mac GPU, mapped scenes, transparency and 512 MiB residency. See docs/project/STATUS.md batch 228 and private review/hybrid-228-acceptance.json.
- Batch 227 (issue #74): 128 original RGB/alpha classifications (89 opaque, 39 nonopaque), exact memory replay and lossless gzip readback verified. All 1,398 installed DDS files pass fresh Python audit with unchanged-hash Windows parser evidence carried from batch 226. No artwork, pack/runtime or adapter changes; no Mac execution. **Awaiting validation** on Mac GPU, mapped scenes, transparency and 512 MiB residency. See docs/project/STATUS.md batch 227 and private review/hybrid-227-acceptance.json.
- Batch 226 (issue #74): 128 original RGB/alpha classifications (89 opaque, 39 nonopaque), exact memory replay and lossless gzip readback verified. All 1,398 installed DDS files pass fresh Python audit with unchanged-hash Windows parser evidence carried from batch 225. No artwork, pack/runtime or adapter changes; no Mac execution. **Awaiting validation** on Mac GPU, mapped scenes, transparency and 512 MiB residency. See docs/project/STATUS.md batch 226 and private review/hybrid-226-acceptance.json.
- Batch 225 (issue #74): 128 original RGB/alpha classifications (85 opaque, 43 nonopaque), exact memory replay and lossless gzip readback verified. All 1,398 installed DDS files pass fresh Python audit with unchanged-hash Windows parser evidence carried from batch 224. No art, pack/runtime or adapter changes; no Mac execution. **Awaiting validation** on Mac GPU, mapped scenes, alpha and 512 MiB residency. See docs/project/STATUS.md batch 225 and private review/hybrid-225-acceptance.json.
- Batch 224 (issue #74): 128 original RGB/alpha classifications (93 opaque, 35 nonopaque), exact memory replay and lossless gzip readback verified. All 1,398 installed DDS files pass fresh Python audit with unchanged-hash Windows parser evidence carried forward. No art, pack/runtime or adapter changes; no Mac execution. **Awaiting validation** on Mac GPU, mapped scenes, alpha and 512 MiB residency. See docs/project/STATUS.md batch 224 and private review/hybrid-224-acceptance.json.
- Batch 223 (issue #74): 128 original RGB/alpha inspections, 83 opaque, 45 nonopaque; exact memory replay and lossless gzip provenance readback verified on Windows. No artwork, pack/runtime or adapter changes. All 1,398 DDS files pass current Python audit with unchanged-hash Windows parser evidence carried forward. **Awaiting validation** on Mac GPU, mapped scenes, alpha and 512 MiB residency; no Mac execution. See docs/project/STATUS.md batch 223 and private review/hybrid-223-acceptance.json.
- Batch 222 (issue #74): eight generated opaque materials, six accepted/installed and two held for masonry-layout/wood-tone corrections. All 1,398 DDS files pass current Python audit; six new Windows parser passes plus 1,392 unchanged-hash results carried forward. No runtime/adapter changes; shared format and 512 MiB residency policy unchanged. **Awaiting validation** for Mac GPU, mapped scenes, alpha and residency; no Mac execution or gameplay. See docs/project/STATUS.md batch 222 and private review/hybrid-222-acceptance.json.
- Batch 221 (issue #74): 128 more RGB/alpha original inspections (74 opaque, 54 nonopaque); exact memory-sheet replay with no raster outputs, artwork or runtime changes. All 1,392 DDS files pass fresh Python checks; unchanged-hash Windows parser results carried forward. Shared renderer remains ce45eb4 and requires no adapter; Mac execution unverified. **Awaiting validation** for mapped scenes, GPU/alpha and residency. See docs/project/STATUS.md batch 221 and private review/hybrid-221-acceptance.json.
- Batch 220 (issue #74): 128 original RGB/alpha classifications via shared tools/textures/texture-triage-memory.py; sixteen sheets exactly replayed in memory with no raster files written. Windows actual-data replay and three invalid-input cases pass; Mac execution unverified, no OS adapter needed. All 1,392 DDS files pass current Python audit with unchanged-hash Windows parser evidence carried forward. No pack/runtime changes or gameplay. **Awaiting validation** for Mac GPU, mapped scenes, alpha and 512 MiB residency. See docs/project/STATUS.md batch 220 and private review/hybrid-220-acceptance.json.
- Batch 219 (issue #74): 128 original RGB/alpha classifications (81 opaque, 47 nonopaque); no artwork, pack, format, runtime or adapter changes. All 1,392 files pass current Python audit with unchanged-hash Windows parser evidence carried forward. Shared formats and 512 MiB residency policy unchanged. **Awaiting validation** on Mac GPU, mapped scenes, alpha and residency; no Mac execution. See docs/project/STATUS.md batch 219 and private review/hybrid-219-acceptance.json.

- Batch 218 (issue #74): eleven environment materials accepted offline; one rusty sheet held for brightness/color drift. All 1,392 installed files pass current Python audit; eleven new Windows DDS parser passes and 1,381 unchanged-hash results carried forward. Shared formats and 512 MiB residency policy unchanged; no runtime/adapter edits. **Awaiting validation** on Mac GPU, mapped scenes, alpha and residency; no Mac execution. See docs/project/STATUS.md batch 218 and private review/hybrid-218-acceptance.json.

- Batch 217 (issue #74): 128 original RGB/alpha classifications (85 opaque, 43 nonopaque); no artwork, pack, format, runtime or adapter changes. All 1,381 files pass current Python audit with unchanged-hash Windows parser evidence carried forward. Shared formats and 512 MiB residency policy unchanged. **Awaiting validation** on Mac GPU, mapped scenes, alpha and residency; no Mac execution. See docs/project/STATUS.md batch 217 and private review/hybrid-217-acceptance.json.

- Batch 216 (issue #74): seven held material corrections accepted offline; patterned metal remains held for pitch/shape drift. All 1,381 installed files pass current Python audit; seven new Windows DDS parser passes and 1,374 unchanged-hash results carried forward. Shared formats and 512 MiB residency policy unchanged; no runtime/adapter edits. **Awaiting validation** on Mac GPU, mapped scenes, alpha and residency; no Mac execution. See docs/project/STATUS.md batch 216 and private review/hybrid-216-acceptance.json.

- Batch 215 (issue #74): eight panel/material replacements accepted offline and four drafts held. All 1,374 installed files pass current Python audit; eight new Windows DDS parser passes and 1,366 unchanged-hash results carried forward. Shared DXT1 and 512 MiB residency policy unchanged; no runtime/format/adapter edits. **Awaiting validation** on Mac GPU, mapped scenes, alpha and residency; no Mac execution. See docs/project/STATUS.md batch 215 and private review/hybrid-215-acceptance.json.

- Batch 214 (issue #74): 128 original RGB/alpha classifications; no artwork, pack, format, runtime or adapter changes. All 1,366 files pass current Python audit with unchanged-hash Windows parser evidence carried forward. Shared formats and 512 MiB residency policy unchanged. **Awaiting validation** on Mac GPU, mapped scenes, alpha and residency; no Mac execution. See docs/project/STATUS.md batch 214 and private review/hybrid-214-acceptance.json.

- Batch 213 (issue #74): nine environment materials accepted offline and three drafts held for boundary/tone corrections. All 1,366 files pass current Python audit; nine new Windows DDS parser passes and 1,357 unchanged-hash results carried forward. Shared DXT1 and 512 MiB residency policy unchanged; no runtime/format/adapter edits. **Awaiting validation** on Mac GPU, mapped scenes, alpha and residency; no Mac execution. See docs/project/STATUS.md batch 213 and private review/hybrid-213-acceptance.json.

- Batch 212 (issue #74): 128 additional original RGB/alpha classifications; no art, pack, format, runtime or adapter changes. All 1,357 installed DDS files pass current Python audit with unchanged-hash Windows parser evidence carried forward. Shared DXT1/DXT5/RGBA8 support and 512 MiB residency policy unchanged. **Awaiting validation** on Mac GPU, mapped scenes, alpha and residency; no Mac execution. See docs/project/STATUS.md batch 212 and private review/hybrid-212-acceptance.json.

- Batch 211 (issue #74): five held materials corrected and installed; three pattern corrections remain held. All 1,357 files pass current Python audit; five new Windows DDS parser passes and 1,352 unchanged-hash results carried forward. Shared DXT1, no runtime/format/adapter changes. **Awaiting validation** on Mac GPU, mapped scenes, alpha and residency; no Mac execution. See docs/project/STATUS.md batch 211 and private review/hybrid-211-acceptance.json.

- Batch 210 (issue #74): eleven metal/machinery replacements accepted offline, one draft held and 128 original RGB/alpha pairs classified. All 1,352 installed files pass current Python audit; eleven new Windows DDS parser passes and 1,341 unchanged-hash results carried forward. Shared DXT1, no runtime/format/adapter changes. **Awaiting validation** on Mac GPU, mapped scenes, alpha and residency; no Mac execution. See docs/project/STATUS.md batch 210 and private review/hybrid-210-acceptance.json.

- Batch 209 (issue #74): eight environment replacements accepted offline and 64 original RGB/alpha pairs classified. All 1,341 installed files pass current Python audit; eight new Windows DDS parser passes and 1,333 unchanged-hash results carried forward. Shared DXT1, no runtime/format/adapter changes. **Awaiting validation** on Mac GPU, mapped scenes, alpha and residency; no Mac execution. See docs/project/STATUS.md batch 209 and private review/hybrid-209-acceptance.json.

- Batch 208 (issue #74): six metal-panel/atlas replacements accepted offline; two visual-fidelity drafts held and 32 further RGB/alpha originals classified. All 1,333 installed files pass current Python audit; six new Windows DDS parser passes and 1,327 unchanged-hash results carried forward. Shared DXT1, no runtime/format/adapter changes. **Awaiting validation** on Mac GPU, mapped scenes, alpha and residency; no Mac execution. See docs/project/STATUS.md batch 208 and private review/hybrid-208-acceptance.json.

- Batch 207 (issue #74): 128 additional original RGB/alpha classifications; no art, pack, format, runtime or adapter changes. Existing 1,327 DDS files pass current Python audit and unchanged-hash native evidence carry-forward. Shared DXT1/DXT5/RGBA8 support remains; **Awaiting validation** on Mac GPU, mapped scenes, alpha and residency. No Mac execution. See docs/project/STATUS.md batch 207 and private review/hybrid-207-acceptance.json.

- Batch 206 (issue #74): found and withdrew the batch 205 trim's blended-magenta border, then replaced it and installed seven further narrow trims using original-only repeated references. Eight new Windows DDS parser passes; 1,319 unchanged hashes carried forward and full current Python audit passed (1,327 total). Shared DXT1, no runtime/format/adapter changes. **Awaiting validation** on Mac GPU, mapped scenes, alpha and residency; no Mac execution. docs/project/STATUS.md batch 206 and private review/hybrid-206-acceptance.json supersede the earlier edge-color claim.

- Batch 205 (issue #74): one narrow metal trim installed after fixed-region carrier extraction and offline UV/edge review; 1,320 total. New Windows native DDS parser pass, remaining 1,319 unchanged-hash evidence carried forward. Shared DXT1, no format/runtime/adapter change. **Awaiting validation** on Mac GPU, mapped scenes, alpha and residency; no Mac execution. See docs/project/STATUS.md batch 205 and private review/hybrid-205-acceptance.json.

- Batch 204 (issue #74): eight narrow metal drafts held for aspect/layout correction; none installed. Existing 1,319 files pass current Python audit and unchanged-hash native evidence carry-forward. No format/runtime/adapter change; shared DXT1/DXT5/RGBA8 remains. No Mac GPU, alpha, residency or scene execution; **Awaiting validation**. See docs/project/STATUS.md batch 204 and private review/hybrid-204-acceptance.json.

- Texture batch 203 (2026-10-20, commit carrying this entry): 128 reused
  original RGB/alpha pairs classified, total 4,297. No art, format or adapter
  change; existing 1,319-file native evidence retained through unchanged
  DDS/checker hashes and current Python audit. Status: Awaiting validation;
  alpha-only text/icons, partial opacity and material behavior still require
  Mac scene/GPU validation. No new game or native parser execution.
  See [batch 203 evidence](STATUS.md#texture-batch-203-reused-texture-inventory-and-hidden-alpha-detail-2026-10-20).

- Texture batch 202 (2026-10-20, commit carrying this entry): eight standard
  DXT1 material replacements accepted offline; four layout/lighting drafts held.
  Total 1,319 installed and 4,169 provisionally classified keys. Shared format
  support is already present; no adapter change. Status: Awaiting validation;
  Windows parser/hash evidence does not validate Mac scenes, GPU memory or
  darker material/indicator visibility. No new gameplay on either host.
  See [batch 202 evidence](STATUS.md#texture-batch-202-metal-and-architectural-surfaces-2026-10-20).

- Texture batch 201 (2026-10-20, commit carrying this entry): 128 reused
  originals classified from separate RGB/alpha review, total 4,105. No artwork,
  pack or runtime changes; no platform adapter needed. Existing 1,311-file
  offline evidence is retained by verified unchanged hashes/checker, not a new
  native execution. Status: Awaiting validation; logos, text, masks, panoramas
  and alpha behavior still need final Mac scenes and GPU/memory validation.
  See [batch 201 evidence](STATUS.md#texture-batch-201-reused-asset-inventory-and-alpha-semantics-2026-10-20).

- Texture batch 200 (2026-10-20, commit carrying this entry): seven metal
  replacements accepted offline; one altered tread pattern held. Standard
  DXT1 uses existing shared support, requiring no new adapter. The 1,311-file
  Windows offline structure/hash evidence does not verify Mac curved mapping,
  brightness, adjacency, residency or scene fidelity. Status: Awaiting
  validation; retain consolidated Mac scenes, modes, memory and fallback checks
  near completion. See [batch 200 evidence](STATUS.md#texture-batch-200-metal-patterns-and-baked-shading-2026-10-20).

- Texture batch 199 (2026-10-20, commit carrying this entry): three
  character/clothing atlases accepted offline and five identity/tone failures
  held; 1,304 installed files have current offline structure/hash evidence.
  Standard DXT1 and shared loader support require no adapter change. Windows
  parser checks do not establish Mac model seams, skin/cloth appearance,
  renderer fidelity or GPU residency. Status: Awaiting validation; retain
  consolidated Mac scene/mode/memory/fallback checks near completion. See
  [batch 199 evidence](STATUS.md#texture-batch-199-character-identity-and-clothing-atlas-review-2026-10-20).

- Texture batch 198 (2026-10-20, commit carrying this entry): fourteen
  private DXT1 environment/prop replacements accepted; cloth and wood atlas
  held for correction. 1,301 installed and 3,849 original keys classified.
  Existing shared DDS support needs no adapter change. Windows offline
  structure, feature profiles and repeat review do not establish Mac scene
  lighting, UV behavior or residency; Awaiting validation. See
  [batch 198 evidence](STATUS.md#texture-batch-198-architectural-materials-and-prop-layout-checks-2026-10-20).

- Texture batch 197 (2026-10-20, commit carrying this entry): seven prior
  material/cutout drafts corrected and installed; one wire mesh remains held.
  1,287 installed and 3,753 keys classified. Existing shared DXT1/DXT5/RGBA8
  support needs no adapter change. Windows offline DDS and color/mask review
  does not verify Mac scene brightness, alpha blending, shader use or residency;
  Awaiting validation. See [batch 197 evidence](STATUS.md#texture-batch-197-cutout-corrections-with-complete-color-coverage-2026-10-20).

- Texture batch 196 (2026-10-20, commit carrying this entry): one private
  DXT5 hedge accepted offline, seven cutout/material corrections held; 1,280
  installed and 3,689 original keys visually classified. Existing shared DDS
  support needs no adapter change. Windows DDS and mask/composite review do
  not establish Mac scene brightness, blending or residency; Awaiting validation.
  See [batch 196 evidence](STATUS.md#texture-batch-196-alpha-bearing-materials-and-mask-fidelity-2026-10-20).

- Texture batch 195 (2026-10-20, commit carrying this entry): fifteen private
  DXT1 replacements accepted, one wood correction held; 1,279 installed and
  3,625 original keys visually classified. Existing shared DDS support needs
  no adapter change. Windows structure, profile and repeat-grid checks do not
  prove Mac scene, reflection or residency behavior; Awaiting validation. See
  [batch 195 evidence](STATUS.md#texture-batch-195-sixteen-asset-production-and-repeated-feature-checks-2026-10-20).

- Texture batch 194 (2026-10-20, commit carrying this entry): eight private
  DXT1 environment/atlas replacements; 1,264 installed and 3,561 original keys
  visually classified. Existing shared DDS support needs no adapter change.
  Windows offline structure/repeat-grid review does not establish Mac scene,
  brightness or residency behavior; status remains Awaiting validation. See
  [batch 194 evidence](STATUS.md#texture-batch-194-environment-materials-and-architectural-atlases-2026-10-20).

- Texture batch 193 (2026-10-20, commit carrying this entry): one private
  identity-preserving portrait installed, seven drafts held; 1,256 installed
  and 3,497 original keys visually classified. Existing shared RGBA8/alpha
  support needs no adapter change. Windows offline DDS checks do not establish
  Mac UI, scene or residency behavior; status remains Awaiting validation. See
  [batch 193 evidence](STATUS.md#texture-batch-193-portrait-identity-review-and-inventory-2026-10-20).

- Texture batch 192 (2026-10-20, commit carrying this entry): two private lamp
  replacements, including RGBA8 with exact resampled original alpha; 1,255
  installed and 3,433 original keys visually classified. Existing shared
  DXT1/RGBA8 support needs no adapter change. Windows offline checks do not prove
  Mac scene/alpha or residency behavior; status stays Awaiting validation. See
  [batch 192 evidence](STATUS.md#texture-batch-192-small-props-and-exact-alpha-lamp-2026-10-20).

- Texture batch 191 (2026-10-20, commit carrying this entry): four private DXT1
  corrections accepted; 1,253 installed and 3,401 original keys visually
  classified. Existing shared format support needs no adapter change. Windows
  offline checks do not establish Mac scene or residency behavior; status stays
  Awaiting validation. See [batch 191 evidence](STATUS.md#texture-batch-191-correction-acceptance-and-original-triage-2026-10-20).

- Texture batch 190 (2026-10-20, commit carrying this entry): three private
  shared DDS additions, including luminance preservation for the L8 panel;
  1,249 installed and 3,369 original keys visually classified. Existing shared
  format support needs no adapter change. Mac scene and residency validation
  remain pending; status stays Awaiting validation. See
  [batch 190 evidence](STATUS.md#texture-batch-190-metal-timber-and-visual-triage-2026-10-20).

- Texture batch 189 (2026-10-20, commit carrying this note) adds four private
  shared DDS replacements after Windows offline checks; 1,246 total installed.
  No adapter change is needed for the existing shared DXT1 loader. No Mac
  runtime/scene or GPU-residency validation was run; status remains Awaiting
  validation. See [batch 189 evidence](STATUS.md#texture-batch-189-stone-and-concrete-2026-10-20).

- **Status:** Awaiting validation
- **Windows replay link fix (2026-10-20, `fix/replay-texture-mode-link`):**
  `patches/graphics/textures/0320-replay-texture-mode-link.patch` supplies the existing shared
  `src/textures/texture_mode.c` to standalone Windows `d3d8_replay`, which intentionally
  omits `xbox_input`. This fixes the unresolved `tsfp_texture_is_4k` release
  build error without adding a second mode policy or the full input library.
  The new real-target build regression in `tests/gameplay/test_frame_replay.py` is in
  the existing Windows/macOS workflow; its build tools use the repository pins.
  No Mac adapter change is needed: native common and legacy renderers already
  own this source, and the added CMake block is Windows-only. Local Windows
  build/native checks and isolated synthetic D3D11 replay pass. Mac execution
  remains unverified on this host; existing Mac Vulkan replay checks remain
  the acceptance path. See [validation](STATUS.md#windows-release-replay-link-fix--2026-10-20).
- **In-game Display choice (issue #74 follow-up):** The shared
  `src/interface/pc_display_menu.c` adds a native **Textures: Classic / Enhanced** choice on
  the existing Display settings page. Arrows apply immediately for the session,
  reflect host toggles, and reinitialize from the current mode after page
  refresh. Resolution/FOV Apply and personal config files are unaffected.
  `src/textures/texture_mode.c` supplies an atomic explicit setter for both hosts.
  Common RHI already consumes this policy; legacy GL now selects both texture
  binding and mip filtering from the same mode, with its source linked by
  `texture-mode-ui.patch`. Native Cocoa/F10 controls remain separate follow-up
  work. Windows offline guest-callback, cached-loader and menu-pointer tests
  pass; native Mac compile/interaction and real-art checks remain pending.
  Acceptance: exercise the in-game choice with native keyboard/controller/mouse,
  verify cached originals/replacements on both renderer paths, and check
  refresh/reopen plus missing-pack fallback. See the Display-menu entry in
  `docs/project/STATUS.md`; this does not mark scene validation complete.
- **Texture alpha integration (PR #198):** The mode policy in
  `src/textures/texture_mode.c` now shares initialization and toggle behavior, with
  Win32 once/atomic operations and POSIX pthread/C11 atomic operations as thin
  adapters. The common native renderer links this source through
  `patches/graphics/textures/1780-texture-mode-shared.patch`;
  an explicit pack still defaults to replacements unless Classic is requested.
  This prevents the Windows alpha from introducing unresolved mode symbols in
  the current Mac common-renderer build. The existing PC-only F10/menu UI still
  needs native Cocoa/F10 wiring. The shared in-game choice above is implemented
  separately; Mac interaction validation remains pending.
  Legacy GL remains a compressed-texture adapter and rejects RGBA or
  alpha-incompatible replacements before upload, preserving original fallback.
  The shared DDS/loader/mode checks in `tests/textures/test_texture_runtime.py` and the
  artwork-tool suites are registered in Windows/macOS CI. Real-art GPU/scene
  acceptance and native interaction validation remain pending; portable compilation
  and synthetic loader results do not replace those checks.
- **Source dimension rounding (issue #74, batch 180):**
  `tools/textures/texture-pack.py` retains exact aspect matching by default and adds
  explicit `--allow-one-pixel-aspect-rounding` for opaque color sources with
  both axes >=512 and at most one long-axis pixel difference. Larger mismatch,
  non-opaque sources, channel conversion and original-alpha overrides remain
  rejected. Metadata records the rounding and resampling. Shared Python/Pillow
  code needs no Mac adapter or runtime change; source is included with the
  batch180 commit. Windows texture-tool/planner suite: 40 tests passed.
  Mac execution remains unverified. Acceptance: run `tests/textures/texture_pack_tools.py`
  on Mac, confirm default rejection and opt-in metadata/encoded output, and
  retain real-art scene checks for the consolidated validation phase.
  See [batch 180 evidence](STATUS.md#texture-overhaul-issue-74-hybrid-batch-180).
- **Original-alpha compilation (issue #74):** `tools/textures/texture-pack.py` accepts
  `--alpha-source` with its required SHA-256 to retain a verified original
  channel while compiling new RGB artwork. Metadata records source size/hash,
  bilinear resampling and lossy DDS caveats. Shared Python/Pillow code requires
  no OS adapter or loader change. Source commit `ab53ce3` is on the issue #74
  branch. Windows checks are recorded in the batch 85 STATUS entry; Mac
  execution and real-art appearance remain unverified. Acceptance: run
  `tests/textures/texture_pack_tools.py`, verify original-alpha provenance and decoded
  mask errors, then inspect cutout/material behavior in consolidated scene tests.
- **Hybrid production tooling (issue #74, 2026-10-16):**
  `tools/textures/texture-hybrid-plan.py` groups the remaining artwork into material
  batches, individual review, specialist review, corrections and visual triage.
  It uses standard-library Python and existing private JSON reports; no Mac
  runtime adapter is needed. Five planner tests pass on Windows; execution on
  Mac and actual artwork scene coverage remain unverified. The planner never
  installs art or counts unreviewed candidates as completed textures.
- **PC behavior:** Windows loads compiled DDS replacements with matching keys/mips and bounded residency.
- **Temporary storage accounting (issue #74, batch 172):**
  `tools/textures/texture-worklist.py` now counts comparison and unknown output
  subdirectories in the 256 MiB temporary limit, reports logical/allocated
  bytes, and exits 2 after writing diagnostics when at or above the limit.
  Shared Python code needs no Mac adapter; Windows tests in
  `tests/textures/texture_pack_tools.py` cover nested comparison files, unknown folders,
  excluded authoritative art/pack files, the boundary and CLI failure.
  Source is included with the batch172 commit; Mac execution remains
  unverified. Acceptance on Mac: run that test file and verify the report on
  the local texture workspace before production. This changes no runtime or
  gameplay behavior; scene validation remains pending.
- **Windows build fix (2026-10-15; integrated in current `main`):**
  `src/textures/texture_pack.c` restores the local `FILE *f` declaration used by
  `dump_original`. MSVC syntax compilation passes; no Mac adapter change is
  needed because the native renderer uses `src/native/gl_texture_pack.inc`.
  Existing Mac texture coverage remains awaiting validation as described below.
- **PC source:** Audited revision above; `src/textures/texture_pack.c`, `src/textures/texture_pack_format.h`, `tools/textures/texture-pack.py`, `tools/play-textures.ps1`; `src/native/gl_texture_pack.inc`.
- **PC validation:** [Display settings and texture preview](STATUS.md#display-settings-and-texture-preview--october-15) records four of eleven replacements loaded in the first mission; seven still need scene-specific checks even on Windows.
- **Existing Mac support:** Native replacement loading and synthetic real-GPU sampling already pass; the launcher accepts `--texture-pack`. Actual installed-art coverage has not been established on Mac.
- **Mac adaptation needed:** The real `private/texture-remaster/pack` directory is absent on this Mac (checked 2026-10-15); the user confirmed the textures have not yet been transferred. Obtain its existing location/copy before testing matching scenes and mapping replacement keys to captures. Do not recreate the loader or label all artwork verified.
- **Mac acceptance checks:** Compare original/replacement mission captures, confirm expected key hits, mip behavior, eviction/fallback and the 512 MiB residency limit. Keep the 6 GiB pack budget and isolated saves; no duplicate extract.
- **Mac implementation and evidence:** Synthetic DDS/GPU replacement and corrupt-fallback regressions passed again in this batch. Actual installed-art coverage remains pending because the real pack is unavailable on this device. [Validation and evidence](STATUS.md#mac-backlog-renderer-camera-and-prompts-2026-10-15).

- **Issue #74 transfer (2026-10-16):** Windows F10 and the PC Settings menu now
  carry the existing live Classic/4K switch in `src/textures/texture_mode.c`,
  `src/input/pc_controls.c`, `src/interface/pc_settings.c` and `patches/graphics/textures/0350-texture-toggle.patch`.
  Uncompressed original identities and alpha-compatible replacement formats,
  including RGBA DDS, are handled in `src/textures/texture_pack_format.h` and
  `src/textures/texture_pack.c`. Catalog, embedded/non-PAK inventory and artwork tools
  are under `tools/texture-*.py`. These changes were transferred from
  `codex/integrate-display-textures` to the issue #74 branch.
  Mac still needs a native toggle adapter and real-art scene/alpha validation;
  shared format checks and Windows tests do not establish Mac parity.
  Acceptance: switch both ways during rendering, retain originals on invalid,
  missing or over-budget files, preserve alpha and the 512 MiB residency cap.
  The 232 installed local replacements are partial coverage; consolidated
  scene testing remains deferred. See the issue #74 transfer record in STATUS.

#### Launcher v1.2.24 icon retirement release

MAC-011: shared launcher migration is included in v1.2.24 on Windows and native
Apple Silicon. The release workflow validates both host builds and packages.
No new adapter is needed; actual Mac game/menu appearance remains Awaiting
validation. This version bump alone is not publication evidence.

### MAC-012 — Standalone Mac runtime distribution

- **Game-only PR notes (2026-10-17, branch `fix/game-only-release-prs`):**
  `tools/release/publish-release.py` filters generated candidates by merged commit range
  and paginated changed-file metadata, excluding launcher-only/housekeeping PRs
  while retaining mixed PRs with game changes. No Mac adapter is needed for this
  shared GitHub metadata logic. Windows publisher regressions cover the filter;
  macOS execution remains unverified. Acceptance: run the same publisher tests
  on Mac. [Validation](STATUS.md#game-only-release-pr-notes).

- **Release metadata follow-up (2026-10-16, branch `fix/game-release-history-titles`):**
  `tools/release/publish-release.py` compares changes with the latest published game
  tag, excluding launcher releases, and creates short `TSFP v…` titles.
  `docs/release/releasing.md` specifies matching `Launcher v…` titles. No Mac adapter
  is needed for simplifying notes to PR/contributor changes and installation
  instructions, without redundant headings, source lists or build sections,
  or for GitHub metadata selection; the shared Python regression suite
  covers the behavior. macOS execution remains unverified.

- **Windows CI harness follow-up (PR #65, 2026-10-16):**
  `tests/launcher/launcher_settings.cs` gives cold WPF UI initialization a bounded
  60-second worker budget and preserves named assertion/timeout diagnostics.
  No Mac adaptation is needed: this is the Windows-only launcher test harness,
  not shared runtime behavior. Hosted Mac patch checks passed on the preceding
  PR revision; native Mac functionality is not inferred from that result.
  [Validation](STATUS.md#launcher-settings-ci-startup-budget--2026-10-16).

- **Release-test compiler discovery follow-up (2026-10-16, branch
  `fix-release-msvc-tests`):** `tests/support/native_compiler.py` supplies one compiler
  environment to audio, framebuffer, cutscene, and lifted-float regressions.
  Windows discovers standalone Build Tools as well as Visual Studio and loads
  x64 headers/libraries without a developer shell. The POSIX path still uses
  `CC` or `cc` and inherits its environment. No Mac adapter is needed for this
  Windows discovery fix; simulated POSIX checks are separate from pending
  execution on macOS. Acceptance: run `tests/support/test_native_compiler.py` and the
  affected native regressions on Mac. See
  [compiler validation](STATUS.md#release-regression-compiler-discovery--2026-10-16).

- **Status:** Verified
- **PC behavior:** Windows has a public runtime package with launcher, dependencies, licenses, checksums/build manifest and game-import workflow.
- **PC source:** Audited revision above; `tools/release/package-release.py`, `tools/release/Import-game.ps1`, `tools/release/ImportGame.cs`, `tests/launcher/release_import.py`; `tools/play-native-mac.py`.
- **PC validation:** Windows packaging/import tooling exists; this audit did not assemble or validate a new release.
- **Existing Mac support:** Checkout and standalone Apple Silicon builds share
  the native runtime; `tools/release/package-native-mac.py` now supplies the portable
  game ZIP, bundled dependencies, licenses and build provenance.
- **Mac adaptation needed:** Implemented for Apple Silicon with production
  launcher data import/checks and preservation. Developer ID/notarization and
  validation of older deployment targets remain separate follow-up work.
- **Mac acceptance checks:** Launch from a clean path without compiler/build tree, verify architecture/dependencies and hashes, reject unsupported XBE, preserve existing saves/settings, and exclude game data/personal files from public artifacts.
- **Mac implementation and evidence:** `release/macos-alpha1` adds native
  dependency bundling/relocation, signed binaries, pinned notices, source and
  provenance, and fresh/existing real-ZIP installation gates. Launcher 1.2.21
  loads hashed portable Vulkan dependencies without a checkout or Python.
  Native alpha.1 source rebuild and platform/GPU/input/settings/debug tests
  pass on the Apple M4 Mac mini. Real-ZIP fresh/existing installations verify
  all 61 files and signatures; a 95-second relocated runtime probe loads only
  bundled libraries, preserves personal settings and exits normally. Public
  macos-arm64 ZIP/checksum/build provenance are added to alpha.1. This build
  requires macOS 27.0+, with ad-hoc signatures; notarization remains future work.
  [Validation](STATUS.md#native-mac-alpha1-distribution--2026-10-20). Existing coverage:
  [native adapter validation](STATUS.md#native-mac-settings-and-graphics-adapters-2026-10-15).

### MAC-013 — System Link and Online multiplayer

- **Alpha.2 portable TLS follow-up (2026-10-22):** Windows' bundled CPython
  keeps `ssl.py` adjacent to its unchanged stdlib PEM trust resources instead
  of loading the module from inside the library ZIP. This fixes TLS context
  construction during the real package probe. The relocated-module regression
  runs in both host release CI suites. Native Mac game packages do not bundle
  this Windows Python/Online runtime; native transport work remains queued.

- **Status:** Queued
- **Temporary diagnostics experiment (`e9eed34`, 2026-10-16):** Windows
  `Play-Quiet.cmd` / `TSFP_DEBUG_QUIET=1` routes stdout/stderr to `NUL` and skips
  Debug log polling and performance-log writes, retaining FPS measurement and
  saved options. Sources: `src/diagnostics/pc_debug_output.h`, `src/diagnostics/pc_debug.c`,
  `tools/play.py`, `patches/diagnostics/0920-pc-debug-quiet.patch`,
  `tests/diagnostics/pc_debug.c`. Native defaults are unchanged. Mac adaptation is queued:
  native startup would need equivalent stream suppression; shared Debug checks
  alone do not verify a full native quiet run. Acceptance: isolated normal/quiet
  comparison with unchanged personal settings, no quiet diagnostic writes, FPS
  still measured, and reporting restored on a normal launch.
- **Issue #20 follow-up (2026-10-16, commit `e9eed34`):** Active Windows PC matches
  open the movement-send gate once per guest tick instead of waiting seven
  ticks. A subsequent candidate bounds full movement-state refresh to three
  match ticks (20 Hz), including unchanged held keys and mouse-only turning.
  This retains the original packet encoder; it does not remove host frame
  stalls. The existing network epoch and simulation/heartbeat/timeout clocks
  are preserved. Sources: `src/network/pc_network_guest.inc`,
  `patches/network/0890-pc-network-movement.patch`,
  `patches/network/0900-pc-network-state-refresh.patch`,
  `tests/network/network_guest_dispatch.c`, `tests/repository/test_patch_updates.py` on
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
  `src/network/pc_network.c`, `src/network/pc_network_guest.inc`, `src/network/pc_online.c`,
  `src/interface/pc_display_menu.c`, `patches/network/0470-pc-network.patch`,
  `patches/network/0640-pc-online.patch`, and `tools/online*.py`.
- **PC validation:** Main records local two-instance lobby/match loading in
  `docs/players/multiplayer.md`; cross-internet routing/full-match validation is pending.
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
  recorded in `docs/project/STATUS.md`. Native Mac execution remains unverified for this merge.

### MAC-014 — Patch upgrades across Windows watermark and native overlay guards

- **Renderer integration CI (2026-10-19, PR #191):** The current common stack
  passes hosted macOS patch transitions, runtime regressions, GPU contracts and
  launcher checks at `9940dba`. Windows exposed a mixed-line-ending assumption
  in the new historical-stack test. The test now appends edits in the existing
  file style and exercises both Git conversion settings; four local cases pass.
  This is a shared test correction, with no runtime adapter change. Windows and macOS
  reruns now pass at `c6b01d3`; prior historical unverified notes below are superseded only
  for the native scope recorded here.
  [Evidence](STATUS.md#renderer-integration-hosted-ci-and-line-ending-fixture-2026-10-19).

- **CI efficiency follow-up (branch `ci/faster-runtime-checks`):**
  `.github/workflows/runtime-patches.yml` and `tools/ci/ci-runtime-scope.py` select
  affected suites for PRs and main pushes; manual, patch/pin and workflow changes
  retain full Windows/macOS coverage. `tests/repository/test_patch_updates.py` shares only
  immutable pristine upstream bytes, with independent writable checkouts for
  every transition scenario. No native Mac adapter is needed for CI scheduling
  or fixture construction. `tools/build/apply-patches.py` eliminates a redundant dry-run
  process per staged patch, preserving full-stack validation before runtime
  writes; shared multi-file conflict/rollback tests cover this change.
  The integrated revision now passes these suites on hosted macOS and Windows.
  Acceptance: full transition suite and affected scope tests pass on both hosted
  platforms. See [CI validation](STATUS.md#runtime-ci-efficiency).

- **Status:** Verified
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
  `tools/build/apply-patches.py`, `tests/repository/test_patch_tracking.py`, `tsfp.ps1`, and
  `.github/workflows/runtime-patches.yml`. The legacy archive remains a
  migration aid for runtimes without a record.
- **PC validation:** Eleven patch-loader tests passed on Windows, plus fresh and
  existing-Windows fixtures using the complete main patch stack; repeated
  application made no changes. See `docs/project/STATUS.md`, 2026-10-15 patch preparation.
  Branch-switch repair: 14 patch-loader tests and the complete Windows Release
  build/native checks passed; see the October 16 runtime branch-switch record.
  Automatic tracking: 27 patch checks and 16 shared-tool checks passed, with
  one Windows symlink-privilege skip. Compilation and remaining native checks
  passed; the existing foreground-window prompt assertion failed twice.
  See the automatic tracking entry in `docs/project/STATUS.md` for the scope and evidence.
- **Existing Mac support:** The same Python patch loader serves both hosts.
- **Mac adaptation needed:** No native adapter changes; validate the shared
  loader on macOS. Legacy patches are never enabled by their archive location.
- **Mac acceptance checks:** Run patch-loader tests and apply/reapply the
  complete merged stack on disposable sources, preserving actual code conflicts.
  Include texture-to-main-to-texture transitions and unrelated-edit preservation.
  Cross-platform CI now covers source-only transitions, record corruption,
  changed/removed patches and rollback. Native Mac build validation is separate.
- **Mac implementation and evidence:** The integrated shared loader passes the
  complete transition suite on both hosted platforms at `d0edd6d`, including
  historical stacks, both Git line-ending modes, unrelated edits and rollback.
  This verifies the patch-loader scope. Separate interpolation harness failures
  are tracked below; Windows gameplay and the user's other checkout remain
  unverified.

- **Hosted integration follow-up:** `d0edd6d` passes macOS patch/runtime/GPU/launcher
  checks. Windows patch transitions pass, but three isolated interpolation tests
  need a missing config-lookup stub. The test-only correction passes all 20 local
  renderer-host cases; final Windows and macOS CI both pass at `c6b01d3`. See
  [harness linkage evidence](STATUS.md#interpolation-harness-windows-linkage-2026-10-19).

### MAC-015 — Release test privileges and disposable cleanup

- **Release follow-up:** `tools/release/publish-release.py` recognizes only pytest's
  `*current` sibling symlinks in `build/release-tests`, validates all targets
  before unlinking, and retains the normal recursive-cleanup guards. Native
  fixtures cover containment and target preservation. This affects Windows
  publication; a Mac publisher still needs its own native adapter/validation.

- **Status:** Awaiting validation
- **PC behavior:** Native-tool symlink tests skip only Windows privilege error
  1314; other errors still fail. Release test/staging cleanup removes validated
  disposable directories without shell Recycle Bin support, preserves archives,
  and does not mask the original test failure if cleanup also fails.
- **PC source:** Commit `b89fbe3` on `fix/release-cleanup`, integrated into `main`:
  `tests/gameplay/test_native_mac_tools.py`, `tests/launcher/test_publish_release.py`,
  `tools/release/recycle-release-work.ps1`, `tools/release/publish-release.py`.
- **PC validation:** 22 passed, three skipped; see `docs/project/STATUS.md` release cleanup.
- **Existing Mac support:** Shared symlink assertions remain enabled on macOS;
  native Mac cleanup is unchanged.
- **Mac adaptation needed:** No PowerShell adapter needed for native Mac.
  Validate the shared Python test change on macOS before marking verified.
- **Mac acceptance checks:** Run native-tool Python tests with real symlinks;
  cleanup and capture must still reject redirects and preserve target files.
- **Mac implementation and evidence:** macOS checks and other-device
  synchronization remain unverified.

### MAC-016 — Shared pickup-range comparison fix (issue #6)

- **Status:** Awaiting validation
- **Issue #24 follow-up (2026-10-16, this revision):** The same existing
  `patches/compiler/0300-recomp-float-joins.patch` from `837692d` also fixes the controller
  sensitivity slider's shared clamp at guest address `0x002EABEB`.
  `tests/gameplay/controller_sensitivity.c` and `tests/compiler/test_recomp_float_joins.py` execute
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
- **PC source:** Focused `fix-pickup-range` change (this commit): `patches/compiler/0300-recomp-float-joins.patch`,
  `tools/build/recomp-generation.py`, `tsfp.ps1`, `tests/compiler/test_recomp_float_joins.py`,
  `tests/gameplay/pickup_collision.c`.
- **PC validation:** The original US pickup tick reproduces the out-of-range grant
  with the pinned translator and passes isolated range/height/ABI cases with the
  patch. Build and gameplay evidence is recorded in
  [docs/project/STATUS.md](STATUS.md#pickup-range-comparison-fix--october-16-2026).
- **Existing Mac support:** The same patch stack and generator are used by both
  builds. `tools/build/build-native-mac.py` now rejects stale generator hashes and records
  them after `--generate`.
- **Mac adaptation needed:** No platform adapter is required for this math fix.
  Transfer the patch through Git, regenerate the game C and rebuild on Apple Silicon.
- **Mac acceptance checks:** Run the shared regression with the supported US XBE
  and Clang; verify near/far pickup behavior while turning and strafing in a bounded
  isolated native gameplay run. Body-collision symptoms need separate confirmation.
- **Mac implementation and evidence:** Shared implementation is present; native
  compilation/gameplay and cross-device synchronization remain unverified.

### MAC-018 — Story audio timing, queue retry and ADPCM resume (issue #8)

- **Output calibration (2026-10-24, PR #327):** Shared
  `RECOMP_AUDIO_DEFAULT_GAIN=0.5` lowers default output approximately 6 dB,
  retaining original sliders and explicit gain overrides. XAudio master and
  SDL per-stream application each apply it once. Actual-backend fixtures and
  timing tests pass on Windows; both host CI jobs include the new fixture.
  Mac listening and user acceptance of the new default remain pending. This
  is a loudness adjustment, not a demonstrated distortion/clipping repair.

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
  `4189924`: `patches/media/0010-dsound-audio-timing.patch`,
  `patches/media/0030-dsound-stream-lifecycle.patch`, `tools/build/recomp-generation.py`,
  `tests/compiler/test_recomp_generation.py`, `tests/media/audio_stream_timing.c`,
  `tests/media/audio_buffer_timing.c`, `tests/media/test_audio_timing.py`, `tsfp.ps1`, and
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
  including limitations, is recorded in `docs/project/STATUS.md` under issue #8.
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
- **PC source:** `patches/graphics/rendering/0760-framebuffer-sampling-passes.patch`, modifying
  shared `src/hle/hle_d3d8_texture.c`; `tests/graphics/framebuffer_sampling.c`,
  `tests/graphics/test_framebuffer_sampling.py`, `tests/repository/test_patch_updates.py`,
  `tsfp.ps1` and `.github/workflows/runtime-patches.yml`. Delivered on branch
  `9-khallos-express-level-is-transparent`, source base `800687d`.
- **PC validation:** Windows build/native checks and 29 patch/cache checks pass.
  A mutation check detects the stale-copy bug; the user confirmed solid walls
  after moving/turning in Khallos Express. Evidence and limits are in
  [docs/project/STATUS.md](STATUS.md#khallos-express-see-through-scenery-issue-9--2026-10-16).
- **Existing Mac support:** Native OpenGL already implements screen copying in
  `src/native/gl_screen_copy.inc`; both hosts consume this shared HLE cache.
  No separate platform implementation is expected for the refresh rule.
- **Remaining adaptation/acceptance:** Build on macOS and run the new native
  cache regression, GPU screen-copy checks and an isolated Khallos Express
  scene at 60 and interpolated 120 FPS. Check solid walls/weapon, windows and
  glow, plus a first-mission effects smoke test. Native rendering and other-device
  synchronization are unverified; Windows results do not establish Mac support.
- **Native progress (2026-10-17):** Shared cache and GPU screen-copy checks pass.
  Bounded Khallos Express runs at targets 60/120 show solid train panels/weapon
  after movement, turning and firing, plus window transparency. Dark/stretched
  exterior sky remains; the native fog register is missing and needs adaptation.
  Overall glow/exterior fidelity remains pending. [Measured scope and captures](STATUS.md#native-khallos-express-effects-check-2026-10-17).

### MAC-020 — Adaptive cutscene letterboxing (issue #33)

- **Status:** Awaiting validation
- **PC behavior:** Story and scripted in-game cutscene bars adapt to the selected
  display aspect ratio. The original 4:3 cinematic aperture is the reference;
  wider displays have thinner bars, with side bars when wider than that aperture.
  The cinematic camera and its CPU culling adapt together to retain composition
  and avoid revealing outside authored backdrops. Animated entry/exit, fades,
  subtitles and scope overlays remain.
- **PC source:** Branch `33-remove-the-letter-boxes-in-cut-scenes`, based
  on `5c9effd`: `src/gameplay/pc_cutscene.h`, `src/interface/pc_display_menu.c`,
  `patches/graphics/interface/0880-cutscene-letterbox.patch`,
  `tests/graphics/cutscene_letterbox.c`, `tests/graphics/test_cutscene_letterbox.py`.
- **PC validation:** Full Windows Release build/native checks, 27 patch transition
  checks and four letterbox tests passed. Original US cinematic routines execute
  in disposable memory and compare bars, other rectangles, subtitle calls and
  guest ABI across resolutions and animation phases. Mutations detect missing
  bar and camera corrections. The user confirmed the opening space scene looks
  correct in an isolated 1280x720 run. Live ultrawide/other scenes remain unverified;
  see [docs/project/STATUS.md](STATUS.md#adaptive-cutscene-letterboxing-issue-33--2026-10-16).
- **Existing Mac support:** The same manual guest wrapper is enabled by
  `TSFP_NATIVE_DISPLAY`; both hosts use the shared display mode and bar-height
  helper. Original guest coordinates still pass through each renderer's normal
  scaling. No OpenGL-specific implementation is required by the sizing rule.
- **Remaining adaptation/acceptance:** Run the source-only regression and native
  build on macOS, then check story and scripted cutscenes at 4:3, 16:9, 16:10 and
  ultrawide output, including resolution changes, fades, subtitles, scopes and
  60/120 FPS. Mac compilation, GPU execution and other-device sync are unverified.

### MAC-021 — Published-release launcher and pre-game settings

- **PR #390 malformed-metadata recovery:** All 28 hosted shared UI groups passed
  after notification repair. Packaged repair then exposed a native JSON exception
  type escaping backup-label fallback. JsonAdapter now normalizes malformed JSON
  to InvalidDataException; native parser/shared backup/texture-receipt regressions
  cover preservation and fallback. Full hosted acceptance reruns before release.

- **PR #390 native notification repair:** First hosted Mac build compiled/signed
  and passed save/texture regressions, but deferred TextChanged left the Save
  button stale during immediate text edits. Ui.TextChanged now observes native
  TextProperty notifications; the unchanged shared edit/revert contract will
  rerun on Mac. Full native acceptance remains awaiting validation.

- **Information density (2026-10-27, local `feature/launcher-density-1027`):**
  Shared `PageTheme.Details` collapses optional explanation/setup content in
  Settings and Saves; shorter copy and captions reduce reading and scrolling.
  Texture actions/state, Beta/compatibility warnings and restore protection stay
  visible. Windows builds, existing shared UI/settings/updater checks and page
  renders pass. Avalonia uses the same controls with no extra adapter needed;
  native Mac compilation, expansion/focus and scaling remain awaiting validation.
  [Evidence](STATUS.md#launcher-information-density-2026-10-27).

- **Shared visual polish (2026-10-27, local `feature/launcher-unified-pages-1027`):**
  `PageTheme` now supplies consistent headers and text-labeled status badges to
  Settings, Saves, Maintenance, What's New and Game versions. Settings field
  captions align across paired columns; display/performance precede textures.
  Windows compilation, full launcher regressions, 27 shared UI groups and
  rendered-page review pass. Avalonia consumes the same helpers; no separate
  Mac implementation is needed. Native compilation/rendering, scaling and
  keyboard acceptance remain awaiting validation on a Mac.
  [Evidence](STATUS.md#launcher-shared-page-polish-2026-10-27).

- **Save manager (2026-10-27, `feature/launcher-save-manager`):** Shared
  `SavesPage.cs`/`SaveBackups.cs` provide dated/versioned manual and pre-update
  backups, confirmed restore, undo backup and journal recovery. Both hosts route
  Saves into the main window. Common archives skip empty virtual-disk blocks;
  `LauncherPlatform.PrepareSparse` is a thin filesystem adapter. Windows build
  and synthetic backup/restore/failure checks pass; native Mac compilation,
  sparse allocation, UI and actual-save acceptance remain awaiting validation.
  Tests run on both hosts through `launcher_saves.cs` in shared UI checks.
  [Evidence](STATUS.md#launcher-save-manager-and-texture-health-2026-10-27).
- **Saved settings button state (2026-10-25, `fix/launcher-save-button-state`):**
  Shared `SettingsPage.cs` disables Save after successful persistence, tracks
  edits/reversions across controls, graphics, Discord and key bindings, and keeps
  failed saves retryable. Immediate texture actions and tab navigation are
  excluded. Both hosts use their existing disabled-button style; no new OS
  adapter is needed. Windows build and 20 shared UI groups pass. Native Mac
  compilation and live UI remain pending; the same new behavioral group runs in
  both host workflows. [Evidence](STATUS.md#launcher-save-button-state-2026-10-25).

- **v1.3.1 publication recovery (2026-10-24):** Hosted Windows and native Mac
  builds, regressions and dual-platform package verification passed in run
  `40157152003`. Its publication job lacked Git history for changelog filtering.
  Both unchanged CI packages were verified and published with the same publisher
  from the tested commit in a full-history checkout. Public manifests/receipts,
  eight assets/two ZIPs, Windows upgrade/rollback and v1.3.0 discovery passed.
  The workflow now fetches full history; a regression catches shallow publication
  checkouts. No runtime or OS adapter change; Mac notarization remains pending.

- **v1.3.1 release preparation (2026-10-24):** Shared version/assembly attributes
  advance for the existing 60/120/180/240 FPS Beta choices and capture-health
  diagnostics on main. Optional texture delivery, data preservation and one ZIP
  per platform remain unchanged. No new platform adapter is introduced. Both
  host build/package checks must pass before publication; local Windows results
  do not establish native Mac execution or gameplay. Evidence is recorded in
  [docs/project/STATUS.md](STATUS.md#launcher-v131-release-preparation-2026-10-24).

- **Bundled texture toggle (2026-10-20, commit carrying this entry on
  `feature/launcher-texture-toggle`):** Shared `GameSettings.cs` and
  `SettingsPage.cs` replace texture download/ZIP/folder controls with an
  off-by-default **Use enhanced textures (Beta)** checkbox. Both host adapters
  remove the obsolete install actions while retaining interrupted-install
  recovery. The saved launcher preference sets `TSFP_TEXTURE_MODE` to `4k` or
  `classic`; in-game/F10 changes remain session-only. No new native adapter is
  needed. Windows compilation, 13 shared UI groups and 35 settings checks pass.
  Mac compilation and UI execution remain pending on a Mac host; acceptance
  covers save/cancel, reopening, launch environment and unchanged texture files.
  [Evidence](STATUS.md#launcher-bundled-texture-toggle-2026-10-20).

- **v1.2.22 publication preparation (2026-10-20):** Restores the common publisher's
  one-versioned-ZIP-per-host policy and Latest release-page download links.
  Platform update manifests retain their existing names and package formats.
  All 25 portable publisher regressions pass. Both host checks on the preparation
  PR remain pending; native Mac validation is not inferred from Windows results.

- **Native frame-rate selector (2026-10-20, commit carrying this entry on
  `feature/launcher-native-frame-rate`):** Shared `GameSettings.cs` and
  `SettingsPage.cs` provide original 60 / beta 120 / beta 144 FPS, with restart,
  compatible-game and Arcade-regression guidance. Both platform adapters locate
  `pc-frame-rate.ini` beside the display preference. Shared tests cover default,
  cancel/save, game-menu edits, launch overrides, malformed/concurrent edits and
  rollback. No new OS adapter is required. Windows build, settings and shared UI
  regressions pass; Mac native compilation, execution and prerequisites remain
  unrun on this Windows host. Requires the separate native-high-refresh runtime;
  this does not certify its gameplay or merge that branch. Wider status remains
  Awaiting validation. [Evidence](STATUS.md#launcher-native-frame-rate-selector-2026-10-20).

- **Native confirmation dialog layout (2026-10-20, commit carrying this entry
  on `codex/launcher-dialog-layout`):** Windows uses native message boxes with a
  horizontal action row. Mac's separate windows did not inherit the launcher
  theme and stacked generic gray buttons vertically. `NativeWindow.MessageDialog`
  now loads the shared `Ui.Theme`/`PageTheme` button templates and colors, with
  equal minimum button widths, a right-aligned Cancel/Continue row, and bounded
  scrolling for long text. Confirmations and error alerts reuse this native
  window adapter; no Windows message-box or update behavior changes. Native
  regressions, prerequisite checks and inspected short/long/error renders pass.
  Windows/macOS CI results will be recorded in the PR; wider delivery status
  remains Awaiting validation.
  [Evidence](STATUS.md#native-launcher-confirmation-dialog-layout-2026-10-20).
- **Automatic restart after game updates (2026-10-20, commit carrying this
  entry on `codex/launcher-update-restart`):** Windows already used a copied
  updater to close/apply/reopen; Mac applied game packages inside the old UI
  process, leaving replaced launcher code active. Both helpers now use shared
  `tools/launcher/UpdateRestart.cs`; installation locks and relaunch commands
  remain in `LauncherPlatform.cs`. Native install, repair and restore route
  through `NativeStartup.HandoffGame`. Mac self-update and game-update restarts
  retain the selected installation root, including translocated app launches.
  Native regressions, shared success/failure/restore tests and an actual packaged
  helper replacement/restart/render pass locally. Windows CI is pending at
  implementation, with shared and separate-process regressions in its existing
  launcher workflow. No gameplay or browser-download delivery claim; status
  remains Awaiting validation for wider delivery work.
  [Evidence](STATUS.md#automatic-launcher-restart-after-updates-2026-10-20).
- **Download status spacing (2026-10-20, commit carrying this entry on
  `codex/launcher-download-spacing`):** The Windows source layout placed channel
  controls, transfer text and the progress bar in one grid row; the generated
  native Mac UI inherited the overlap. `tools/launcher/Launcher.xaml` now stacks
  these controls vertically so wrapping text expands the row. No OS adapter
  is needed. `NativeWindow.cs` adds offline download previews for normal and
  minimum sizes, including scrolled controls. Native build/regressions and
  inspected previews pass. Initial Windows CI exposed locale-dependent event
  decoding in `tools/release/launcher-release.py`; explicit UTF-8 and a CP1252 regression
  fix that shared build prerequisite. Windows/native Mac CI remains pending at
  implementation. The local game prerequisite check is blocked by the existing
  checkout's missing Python environment. Live transfer and Windows visual
  acceptance remain unverified; status remains Awaiting validation.
  [Evidence](STATUS.md#launcher-download-status-spacing-2026-10-20).
- **Game package compatibility correction (2026-10-20,
  `fix/launcher-compatible-discord-package`):** Windows runtime packages now
  place Discord documentation and the disabled template under `Support/Discord`.
  They no longer include the unsupported root `Discord-setup.txt` or personal
  `Settings/pc-discord.ini`. Existing launchers accept the supported tree;
  missing Discord preferences already mean disabled, and the shared settings
  model creates the preference when saved. No allowlist or Mac adapter change
  is required. `tests/launcher/launcher_updates.cs --verify-package` exercises production
  unpack/install on the exact local ZIP, with fresh and existing-personal-file
  fixtures; the Windows publisher requires it before upload. Host-specific
  validation is recorded in [STATUS](STATUS.md#launcher-compatible-game-package-correction--2026-10-20).
  Native Mac distribution/visual acceptance remains unverified locally; shared
  launcher behavior is unchanged. MAC-053 retains Discord runtime coverage.
- **Translocation recovery (2026-10-19, commit carrying this entry on
  `codex/mac-launcher-translocation`):** Windows keeps its existing portable
  installation behavior. macOS can run a downloaded bundle from a read-only
  `AppTranslocation` mount, hiding the real portable folder. `Program.cs` now
  routes that startup to `NativeInstallation.cs` before updater/log/lock writes.
  The user selects the real folder; shared manifest/path/hash checks and an
  exclusive temporary write probe validate it before normal startup continues.
  No game/settings/model policy is duplicated or moved. Native regression/UI
  and packaged installation checks cover recovery and personal-file preservation.
  Real browser-translocated first launch and Windows CI remain acceptance work;
  this is not Developer ID/notarization. Status remains Awaiting validation.
  [Evidence](STATUS.md#mac-launcher-translocation-recovery-2026-10-19).

- **v1.2.20 replacement release (2026-10-19):** Version bump packages the merged
  sealed Mac app and one-ZIP-per-platform policy. Implementation CI at `1454320`
  passed strict installed/repaired signatures and negative seal/plist tests;
  exact v1.2.20 publication checks remain required. The user explicitly requests
  retiring the faulty v1.2.19 release after the replacement is verified. Existing
  v1.2.19 Mac clients need one manual replacement. Developer ID/notarization and
  browser-download acceptance remain outstanding. See
  [STATUS](STATUS.md#launcher-v1220-replacement-release-2026-10-19).

- **Bundle signing correction (2026-10-19, `72a4bb3`, PR #201):** The published v1.2.19
  executable signature is intact, but the completed app is unsealed. The Mac
  builder now signs nested code and then the app, with strict validation before
  manifest generation. Shared publication/updater policy carries the canonical
  CodeResources seal; Windows behavior is unchanged. Package regression tests
  check missing/tampered seals and installed/repaired signature validity. Local
  21 policy tests pass; Mac host CI was pending at implementation; results are recorded in
[PR #201](https://github.com/TheDoffman/timesplitters-future-perfect-pc/pull/201). Developer ID/notarization and
  browser-download Gatekeeper acceptance remain unverified; no credentials are
  available. v1.2.19 requires one manual Mac replacement because its updater
  rejects the new seal path. No security bypass is implemented. See
  [STATUS](STATUS.md#mac-launcher-bundle-signing-correction-2026-10-19).

- **Single-ZIP publication (2026-10-19, `430cdfd`, PR #199):** The shared publisher and
  exact host artifact selections produce one versioned ZIP per platform, retaining
  existing client discovery names and manifests. No Mac OS adapter is required.
  Both host regression fixtures cover releases with and without old aliases.
  Local Windows packaging, 19 policy tests and published v1.2.18 discovery passed;
  Windows/Mac host builds, regressions and complete release verification passed
  in CI run 39360311404. Existing release assets remain intact; versionless
  Latest asset links must migrate to the release page for future releases.
  See [STATUS](STATUS.md#launcher-single-zip-publication-2026-10-19).

- **v1.2.19 release preparation (2026-10-19):** Shared version bump packages
  the already merged Windows and native Mac implementation through the common
  release workflow. Local Windows upgrade/rollback and full-suite evidence is
  in docs/project/STATUS.md; both host CI/package checks are required before publication.
  No adapter behavior changes here. Existing signing/notarization and
  clean-machine acceptance gaps remain; no local Mac execution is claimed.

- **Status:** Awaiting validation
- **API-free compatibility checks (2026-10-19, PR #186 integration, uncommitted):**
  The user's no-API-credit requirement supersedes the paid review/fix design in
  PR #190. Every PR now reports a read-only compatibility gate; affected launcher
  changes build/test Windows and Mac through the shared release pipeline. No AI
  service, secret, paid call or automatic repair PR remains. GitHub's API key and
  enable variable were removed. Local policy/release tests and 18 native launcher
  plus eight shared UI groups pass. CI run 39340698617 also passed both host
  builds and all package checks at `02acfe0`; runtime integration follow-up CI
  is pending. Existing
  delivery/signing limits remain. See [STATUS](STATUS.md#api-free-launcher-integration-2026-10-19).
- **Shared interface (2026-10-19, `2ac286d`, `ee495e6`, `a0c4e14`, PR #189):**
  Windows and Mac compile the same `*Page.cs` sources for settings, news, game
  versions, repair and diagnostics, plus shared `PageTheme.cs` and theme XAML.
  Home stays generated from `Launcher.xaml`. `Ui.cs` maps framework operations;
  native controllers retain OS/lifetime integration without separate layouts.
  Seven identical UI groups pass on Windows and Mac, alongside existing host
  regressions, source-mutation checks and actual Mac packaged repair/restart.
  Both packages and all 12 combined release assets pass verification. No game adapter is required; OS integrations and document/control
  rendering remain adapters. This supersedes the separate page implementations
  described below. See [STATUS](STATUS.md#shared-launcher-interface-2026-10-19).

- **Unified release automation (2026-10-19, `55d3141`, `d58bcf2` on `codex/launcher-dual-platform-release`):**
  `.github/workflows/launcher-release.yml` and `tools/release/launcher-release.py` build
  Windows x64 and native Mac ARM64 from one version/revision. Both host regressions,
  original artwork, manifests, checksums and source receipts must pass before
  publication; the release remains a draft until both uploads are verified.
  Existing shared release selection uses each host's manifest. No game adapter
  is needed. Both real Windows/Mac CI builds, Mac packaged repair/restart/render
  and combined asset verification passed. Offline tests cover interrupted
  publication recovery and immutable published assets. Actual publication is
  intentionally unrun until a new version is merged and released.
  Developer ID/notarization, clean-machine delivery and native game publication
  remain separate. See [STATUS](STATUS.md#dual-platform-launcher-release-automation-2026-10-19).
  The first Windows run caught a legacy-path casing regression in launcher
  preservation during game upgrades; shared preservation is corrected and Windows
  upgrade/rollback passes. Both host suites check selection from a combined release.
  Required CI also reuses the already-tested harness/downloader fixes from
  `37b4af9`/`083e57c`; no runtime adapter or dependency-pin change is introduced.
- **Visual follow-up (2026-10-19, `1a24135`):** `NativeHomeStyles.axaml` maps the
  Windows `PageTheme.cs` game-data expander and `Launcher.xaml` rollback face to
  native controls. `NativeNews.cs` now matches `WhatsNewPage.cs`'s 10-unit button
  spacing and selector alignment. The generated home layout includes the host
  styles, and preview mode renders the real news toolbar. Native visual review,
  expand/collapse and selector interaction, launcher checks and packaged
  repair/restart passed; see [STATUS](STATUS.md#native-launcher-visual-spacing-follow-up-2026-10-19).
  Windows source is unchanged; live Windows comparison remains unverified.

- **Integrated regression fix (2026-10-19, commit carrying this entry):** Windows
  CI at `ad9cb0f` exposed launcher deletion during game cleanup when a legacy
  manifest uses different filename casing. Shared `Core.PreserveGameFile` now
  restores case-insensitive protection while retaining native app resources.
  The existing Windows reproduction and a new native transaction/rollback
  check cover the fix; both jobs pass at `c553f9a` in run 39311742707, including
  104 Windows launcher regressions and 17 native groups. Integrated
  native game launch/quit and synthetic crash/report checks also pass.
  [Evidence](STATUS.md#integrated-host-validation-and-launcher-cleanup-fix-2026-10-19).

- **Parity completion (2026-10-19, `356e5a3` on `codex/mac-launcher-parity`):**
  Native Discord editing/IPC, native fatal/hang report open/export, shared rich
  Markdown, matching release/version/maintenance page structure, safe successful
  launch cleanup and isolated real-game lifecycle tests are implemented. Local
  packaged helper repair/restart and live Discord acknowledgement pass. See
  [measured acceptance](STATUS.md#native-launcher-parity-completion-2026-10-19).
  This supersedes the historical adapter gaps below. Remaining release acceptance
  is Developer ID/notarization, clean-machine
  delivery and published native runtime packages (MAC-012).

- **Native launcher port (2026-10-19, `codex/mac-launcher-parity`, commit `c252956`):**
  A standalone ARM64 Avalonia `.app` now shares the Windows v1.2.18 home layout,
  original art, settings/config models, release client, downloader, import,
  update/rollback transactions and diagnostic filtering. Host adapters are in
  `tools/launcher/LauncherPlatform.cs` and `tools/launcher/macos`; build and SDK
  pins are in `tools/build/build-mac-launcher.py` and `config/build/mac-launcher-sdk.json`.
  Native dialogs, primary display modes, binding capture, pre-game settings,
  local build launch, startup self-update and Finder actions are implemented.
  Windows remains WPF/.NET Framework and compiles with the shared changes.
- **Evidence:** 13 native regression groups, native app/render checks, shared
  Windows compilation and existing Mac launcher tests are recorded in
  [docs/project/STATUS.md](STATUS.md#native-mac-launcher-port-2026-10-19). Personal configs and
  saves were not used for automated gameplay. Local Vulkan prerequisites use
  the existing verified SDK helper; this does not certify game rendering.


- **Launcher v1.2.18 candidate (2026-10-19):** The local Windows candidate
  includes live Known Issues commit `2f5ca68` and main `d63a156`. Full launcher,
  layout, packaging and published-v1.2.17 upgrade/rollback checks passed as
  recorded in docs/project/STATUS.md. No new native Mac implementation is included;
  adaptation below remains queued and Mac execution remains unverified.

- **Live known issues (2026-10-19, `feature/launcher-live-known-issues`):**
  `ReleaseClient.cs` and `ReleaseNews.cs` fetch all bounded pages of open GitHub
  issues labeled bug, exclude PRs, and return a complete replacement snapshot.
  `WhatsNewPage.cs` shows literal linked titles, counts, checked times, manual
  refresh and stale/offline feedback independently of historical release notes.
  `Launcher.cs` checks on entry/activation when due and every five minutes while
  the page is visible; it suppresses overlapping requests and stops on close.
  Native Mac has no corresponding UI; adaptation remains queued. Reuse the
  filter/pagination/deadline semantics in a native client, including preservation
  of old results on later-page failure, empty-vs-offline distinction, safe links,
  and removal after closure or relabeling. Verify timer lifecycle, keyboard
  navigation and narrow layouts on Mac; Windows checks in docs/project/STATUS.md do not
  establish Mac execution. No personal data or GitHub mutation is involved.

- **Launcher v1.2.17 build (2026-10-19):** Prepared locally on
  `release/launcher-v1.2.17` from main `8fe6b48`. This version packages the
  existing Windows visual and lifecycle work below plus MAC-046 recovery;
  it adds no new native Mac behavior. Adaptation remains queued. Versioned
  Windows build validation is in docs/project/STATUS.md; Mac execution is unverified.

- **Visual polish (2026-10-19, `feature/launcher-visual-polish`):**
  `Launcher.xaml`, `Launcher.cs`, `PageTheme.cs` and `SettingsPage.cs` add a
  compact verified-data disclosure, matching native control styles, distinct
  aligned vector navigation icons, clearer labels, tighter settings and an
  uncropped original-art banner frame. Home scrolls when expanded content
  exceeds its viewport; existing incomplete data remains protected from import.
  Native Mac does not use this WPF UI; adaptation remains queued with this
  entry. Reuse the information hierarchy in the native UI rather than Windows
  templates. Acceptance: missing/verified/incomplete data actions, keyboard
  access, maintained drafts, readable minimum-size layouts, original art
  preservation and platform-appropriate icon/control rendering. Windows checks
  are recorded in docs/project/STATUS.md; no Mac execution is verified.
  The same branch's follow-up groups version/readiness/Play/data management,
  chooses one emphasized Home action, and adds labeled vector update states with
  local progress and reserved layout space. The banner contracts at smaller
  heights. Adapt these state semantics and action priorities to native controls
  with MAC-021; preserve pause/error progress reset and clean update completion.
  `tests/launcher/launcher_downloads.cs` covers pause/error/retry feedback. Mac behavior
  remains unverified and no native adapter was changed.
  A subsequent banner redesign on the same branch (based on `40b83a3`) replaces
  the inset photo with an edge-to-edge generated environment, larger original
  wordmark and uncropped original Cortez photo with a narrow background-edge
  transparency blend. `tools/build/build-launcher.ps1` embeds the private companion
  backdrop separately and requires it for release builds. For native adaptation,
  preserve both original image sources, uniform photo scaling, responsive action
  visibility and a fallback when private artwork is unavailable. Generated art
  and full provenance must be transferred privately, not through Git. Windows
  preview/package validation is recorded in docs/project/STATUS.md; Mac remains unverified.

- **Minimize on process start (2026-10-19, `fix/launcher-minimize-on-start`):**
  `tools/launcher/Launcher.cs` minimizes the Windows launcher immediately after
  a successful game process start, keeps its taskbar button and restores the
  previous window state when the game exits. Failed process creation stays
  visible; update cleanup still requires an observed window and clean exit.
  `tests/launcher_lifecycle` covers delayed windows, early exits, taskbar
  availability and restoration. Mac does not use this WPF window lifecycle;
  native Dock/minimization parity remains queued with this entry. Acceptance:
  native process start minimizes the launcher without hiding its Dock entry,
  early failure restores it, and UI visibility never authorizes update cleanup.
  Windows results are in docs/project/STATUS.md; native Mac execution remains unverified.

- **Windows menu command labels (#333, uncommitted):** The native owner-drawn
  menu now reads captions, shortcuts and separator types from its actual items.
  This fixes the stale positional table that drew Hide Menu Bar over Fullscreen
  after Textures was inserted. Real HWND tests cover rendered labels/shortcuts,
  command mnemonics, separators, reordered items and disabled choices. Cocoa
  does not use this Win32 adapter, so no Mac implementation change is required;
  these Windows tests do not establish native Mac UI validation.

- **Windows menu/dialog appearance follow-up (2026-10-18, implementation in this revision on branch
  `feature/system-theme-game-ui`, depends on local title-bar commit `7219fec`):**
  `src/interface/pc_menu_theme.h`, `src/interface/pc_dialog_theme.h`, `src/interface/pc_settings.c` and
  `src/input/pc_controls.c` theme the native menu bar/popups, binding editor/capture
  and Debug controls while preserving their commands and keyboard behavior.
  `tools/network/online_theme.py`/`online.py` theme the Tk browser/invitation windows;
  the modern-look revision retains Vista layouts and uses rounded dark surfaces,
  blue checkbox accents and state feedback, replacing the earlier Clam styling.
  Native dialog buttons/checkboxes also use rounded surfaces. These are custom
  Windows-like dark controls, not a switch to WinUI or Acrylic/Mica.
  Online captions apply after native window mapping and recover from wrapper
  resets; real mapped HWND tests cover re-showing windows and light/dark mode.
  Bundling and content identity include the new module. High contrast uses
  native system colors. The launcher interior and game renderer are unchanged.
  Mac has separate Cocoa settings/menus; this Windows DWM/GDI/Tk adapter does
  not change them or prove their appearance support. Native Mac Online remains
  separate deferred work. Acceptance: readable light/dark controls, live changes,
  menu mnemonics/checkmarks/F1/fullscreen behavior, unchanged settings, and
  native Mac appearance checks if corresponding adaptation is requested.
  [Validation](STATUS.md#windows-game-ui-theme-follow-up).

- **Windows title-bar theme experiment (2026-10-18, branch
  `feature/system-theme-title-bars`):** `tools/launcher/WindowTheme.cs` attaches
  to the main WPF and startup windows; `src/interface/pc_window_theme.h` and
  `src/input/pc_controls.c` apply Windows app mode to the game caption at startup and
  on settings/theme notifications. Interior colors and window styles are
  preserved; unsupported DWM attributes fall back to the standard title bar.
  No Mac adapter is needed for this Windows-specific DWM feature; existing
  Cocoa title bars are unchanged and their appearance is not validated here.
  Acceptance: light/dark preference changes while both Windows apps are open,
  high-contrast fallback, windowed/fullscreen transitions and clean shutdown.
  [Validation](STATUS.md#windows-title-bar-theme-experiment).

- **Status:** Awaiting validation
- **Repair / Verify and Diagnostics tabs (2026-10-17, commit `b0f927d`, [PR #98](https://github.com/TheDoffman/timesplitters-future-perfect-pc/pull/98)):** Merged as `860dccd`, prepared for launcher v1.2.12. `MaintenancePage.cs`, `Launcher.cs`/XAML
  and the shared `PageTheme.cs` tab styling group both pages behind one Repair /
  Verify sidebar entry. Native tabs retain reports, verification results and
  settings drafts; Diagnostics is collected on first selection or manual refresh.
  View logs stays visible on each tab, including during operations. Busy navigation
  is locked without disabling Cancel or logs. The separate Diagnostics sidebar
  entry is removed. Windows repair/report/privacy behavior is unchanged. Mac has
  no equivalent release maintenance UI; native adaptation is **queued** with the
  existing maintenance work. Acceptance: default repair tab, keyboard selection,
  preserved results/drafts, report/GitHub actions, visible logs in normal/recovery/
  busy states, usable cancellation and small-window layout. Windows validation is
  recorded in docs/project/STATUS.md; native Mac launcher execution remains unverified.
- **Embedded What's new page (2026-10-17, commit `cfbfe6c`, [PR #93](https://github.com/TheDoffman/timesplitters-future-perfect-pc/pull/93)):** Merged as `7ed93cb`, prepared for launcher v1.2.11. `ReleaseNews.cs`, `WhatsNewPage.cs`,
  `ReleaseClient.cs`, `Launcher.cs`/XAML and bootstrap preview routing add game/
  launcher views, installed/latest version comparison, exact installed notes,
  published known-issues extraction, native text rendering and retry with
  session-only retention. The footer contains Refresh notes and View release on
  GitHub; Home navigation uses the sidebar. Windows uses the existing compatible-release selection
  and launcher metadata verification. The current Mac launcher has no equivalent
  release browser. Native adaptation is **queued**: reuse release/channel/version
  rules, exact-tag lookup and bounded display-only text, with native embedded
  navigation and cancellable requests. Acceptance includes stable/preview filtering,
  older/unpublished installs, offline refresh with stale-note labeling, long text,
  code/HTML/image safety, channel changes during requests, preserved settings
  drafts and small-window/keyboard usability. See docs/project/STATUS.md for Windows evidence;
  native Mac launcher execution remains unverified.
- **Resumable downloads and recovery (2026-10-17, commit `c21df35` on
  `feature/launcher-download-recovery`):** Merged through [PR #91](https://github.com/TheDoffman/timesplitters-future-perfect-pc/pull/91) as `69204af`, prepared for launcher v1.2.10. `DownloadTransfer.cs`, `ReleaseClient.cs`,
  `SelfUpdate.cs`, `InstallationMaintenance.cs`, `Bootstrap.cs`, `Launcher.cs`/XAML
  and `MaintenancePage.cs` add separate bounded game/launcher caches, strict HTTP
  resume handling and complete-file checksum verification, cancellable copy and
  hashing, measured speed/ETA, pause/retry and embedded recovery actions. Cached
  bytes survive staging cleanup, restarts and cancelled preparation. Unknown and
  redirected paths are preserved/refused; successful staging cleans owned cache
  data. No game runtime or config format changes. The existing Mac launcher has
  no equivalent release-download UI. Native adaptation remains **queued**: reuse
  release identity, Range/If-Range validation, complete hash and bounded cache
  rules, with POSIX-safe atomic metadata and locking plus native progress/cancel
  and recovery controls. Acceptance includes connection loss/restart, ignored or
  malformed ranges, changed releases, cancelled verification/preparation,
  storage failures, long estimates, retained user files and usable offline entry.
  See the download-recovery entry in docs/project/STATUS.md; Windows checks do not establish
  native Mac launcher execution.
- **Embedded page and sidebar polish (2026-10-17, commit `0f30cbe` on
  `feature/launcher-page-polish`):** Merged through [PR #89](https://github.com/TheDoffman/timesplitters-future-perfect-pc/pull/89)
  as `1728f21`, prepared for launcher v1.2.9. `PageTheme.cs`, `MaintenancePage.cs`,
  `SettingsPage.cs` and `Launcher.xaml` provide shared blue/silver control styling,
  game/launcher health cards, expandable verification details, grouped settings,
  a fixed Save/Cancel footer, and fixed sidebar icon/label columns with explicit
  accessible navigation names. Settings draft/save/cancel, capture prompts,
  verification and repair ownership rules remain unchanged. New status text
  opens details for unhealthy verification results; smaller windows scroll the
  content while retaining the sidebar and footer. Existing Mac has no matching
  release-launcher UI; its Cocoa settings window remains separate. Native Mac
  adaptation is **queued**: follow the grouping, readable native controls and
  aligned navigation in that future UI, preserving keyboard/assistive access,
  all settings and user data. Acceptance includes minimum-size layout, dropdown
  and slider keyboard behavior, expand/collapse, repair busy/results states and
  retained settings drafts. See the page-polish validation entry in docs/project/STATUS.md;
  Windows render tests do not establish native Mac launcher support.
- **Repair/Verify and Diagnostics for v1.2.8 (2026-10-17, commit `920b789` on
  `feature/launcher-repair-diagnostics`):** `InstallationMaintenance.cs`,
  `MaintenancePage.cs`, `Launcher.cs`/XAML, `ReleaseClient.cs`, `SelfUpdate.cs`
  and `Core.cs` add embedded repair/diagnostic pages with the sidebar retained.
  Verification is read-only against installed manifests. Repairs pin exact
  published versions, validate archives and files, protect personal directories,
  preserve the independent launcher during game repair and retain the previous
  game rollback using separate repair transactions/recovery/cleanup. Diagnostics
  provide versions, GPU/driver/settings/health and allowlisted error summaries;
  clipboard reports exclude personal paths, arbitrary log text, saves and ISOs.
  Existing Mac has no corresponding release-launcher adapter. Reuse version,
  manifest ownership and transaction rules in its future native implementation;
  adapt WPF/WMI/clipboard and POSIX atomic recovery separately. Verify corruption,
  cancellation, staged updates, recovery, retained backups, privacy and page
  navigation with disposable fixtures. Shared game runtime is unchanged; native
  Mac execution is unverified. See the v1.2.8 validation entry in docs/project/STATUS.md.
  Follow-up on the same branch (2026-10-17, commit `e640b2f`): `DiagnosticIssueDraft.cs`
  and Diagnostics buttons open the bug/enhancement GitHub form with the filtered
  report, bounded URL prefills and full-report clipboard fallback. Both issue
  templates add an optional matching field. Future Mac adapters must preserve
  the review-before-submit flow, use a native browser/clipboard adapter, retain
  long/Unicode reports and avoid automatic issue submission. Acceptance checks
  include both form IDs, URI round-trips, privacy, busy state and clipboard fallback.
  Additional PR #88 UI follow-up moves Game files from the sidebar into
  Repair / Verify, below the repair cards as Open game folder. Preserve this
  placement, install-root target and busy state in the future Mac launcher; the
  Windows preview avoids opening Finder/Explorer. See the folder-action entry in
  docs/project/STATUS.md for Windows build, regression and rendered-layout evidence.
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
  to avoid offering a partial rollback. `tests/launcher/launcher_updates.cs` covers these
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
  `tests/launcher/launcher_updates.cs` covers repeated upgrades, deferred cleanup,
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
  `tests/launcher/launcher_updates.cs`; normal startup and updater command routes remain
  separate. These WPF commands have no existing native Mac counterpart. When
  adapting MAC-021, test native argument routing, safe preview isolation and
  diagnostics independently; Windows results do not verify Mac integration.
  Evidence: [Bootstrap diagnostics](STATUS.md#bootstrap-argument-diagnostics-2026-10-16).
- **Embedded settings (2026-10-16, commit `5eea0ca` on `feat/launcher-inline-settings`):**
  Windows launcher v1.2.2 replaces the separate settings window with an
  embedded `SettingsPage` in `Launcher.xaml`/`Launcher.cs`. Both tabs, binding
  capture and confirmations keep the sidebar visible. Home navigation keeps
  drafts; Cancel discards them; Save keeps the page open. Build references and
  isolated WPF checks are in `tools/build/build-launcher.ps1` and
  `tests/launcher/launcher_settings.cs`. Native Mac pre-game UI remains queued; its
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
  and icon remain embedded. Release source: `tools/launcher`, `tools/build/build-launcher.ps1`,
  `tools/release/package-release.py`, branch `release/resolution-v0.2.0-alpha.4`.
- **Mac support/adaptation:** Existing in-game Cocoa settings remain separate.
  Add native release discovery, import, update/rollback and pre-game UI adapters;
  reuse shared binding/display/config rules. Do not ship Windows binaries on Mac.
- **Acceptance:** Safe archive paths/checksums, stable/preview channels, failed
  update recovery, isolated settings save/cancel, conflicts, file preservation,
  primary display modes, and actual runtime consumption of all selected settings.
  Windows results do not verify Mac adapters or rendering.
- **Delivery order (audit 2026-10-17):** This ID covers a complete launcher,
  not just a settings window. Implement in independently validated slices:
  (1) MAC-012 runtime package/import/provenance; (2) pre-game settings with
  version capability gates and preserved drafts; (3) verified game/self-update,
  rollback and clean-start/clean-exit retirement; (4) resumable downloads and
  recovery; (5) repair/verify, privacy-filtered diagnostics and reviewed issue
  drafts; (6) release notes/known issues and accessible page polish. Preserve
  the detailed acceptance requirements above for every slice. Native game
  input/rendering work can proceed without waiting for this entire launcher.

### MAC-022 — Configurable gameplay FOV (issue #29)

- **Status:** Awaiting validation

- **Issue #339 follow-up (2026-10-24, commit carrying this entry):** Shared
  `src/interface/pc_display_menu.c` also retains custom FOV in Zeppelin air-lift state
  0x800. `tests/graphics/test_cutscene_letterbox.py` checks default/custom FOV, transitions,
  menu/cinematic exclusions and the original US lift routines with local assets.
  Windows regressions and supported build pass (see docs/project/STATUS.md). No Mac adapter
  is needed; native compilation and Zeppelin lift/exit/landing gameplay remain
  pending. Check all four lifts, relative weapon zoom and unchanged authored
  menu/cutscene/terminal lenses.

- **Issue #66 follow-up (2026-10-16, commit `df3bc2e`,
  [PR #72](https://github.com/TheDoffman/timesplitters-future-perfect-pc/pull/72)):**
  `66-bug-fov-returns-to-default-when-falling-from-a-ledge` updates
  `src/interface/pc_display_menu.c` to keep the selected FOV in falling state 4 as well
  as grounded state 2. The shared camera hook needs no Mac-specific adapter.
  `tests/graphics/cutscene_letterbox.c` covers both call sites through a fall/landing,
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
  `ef17e2a`; `src/interface/pc_display.c`, `src/interface/pc_display.h`, `src/interface/pc_display_menu.c`,
  `tools/launcher/GameSettings.cs`, `tools/launcher/SettingsPage.cs`.
- **Shared Mac support:** the existing shared camera/menu hooks and config model
  use the same behavior. Config accepts legacy V1 (55°) and V2 with explicit FOV;
  existing Cocoa resolution saves retain the loaded FOV through the shared writer.
  Native compilation and Cocoa FOV controls now pass the isolated checks below.
- **Native adaptation (2026-10-17, `codex/mac-display-parity`):** Added the
  40–100° Cocoa slider, numeric value and shared default. Real Cocoa/GPU checks
  cover draft cancellation, Save/reopen, and failed-save rollback of both FOV
  and resolution. See [native settings evidence](STATUS.md#native-display-settings-parity-2026-10-17).
- **Remaining:** guest menu slider, gameplay/zoom and ledge/landing checks on Mac;
  visual review of the live Cocoa panel. Windows launcher UI does not constitute
  a Mac launcher implementation.
- **Acceptance:** default framing unchanged; 40/55/75/100° at 4:3, 16:9 and
  ultrawide; zoom retains relative magnification; cinematics/terminals unchanged;
  configuration round trips and resolution timeout reverts both choices. See
  [Windows FOV evidence](STATUS.md#configurable-gameplay-fov-issue-29--2026-10-16).

Launcher v1.2.3 release compatibility (MAC-021/MAC-022): `GameSettings.cs` and
`SettingsPage.cs` disable custom FOV for packaged game versions through alpha.4
to retain V1 display compatibility. Windows model/UI regression coverage is in
`tests/launcher/launcher_settings.cs`; Mac launcher capability gating remains queued.

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
  `patches/graphics/interface/0930-story-objectives-health.patch`,
  `src/interface/pc_objectives.h`, `tests/graphics/test_objectives_health.py`,
  `tests/graphics/objectives_health.c`, `tests/graphics/objectives_layout.c`.
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

- **Auto-resolution follow-up (2026-10-21, this change on
  `fix/fullscreen-desktop-resolution`):** Shared display policy
  adds persistent Auto, effective dimensions, failure rollback and resize
  tracking. Windows reads the current fullscreen monitor; native
  `renderer_host_sdl.c` publishes physical drawable pixels on its window thread
  for both common and legacy GL renderers. `settings_macos.m` and the shared
  launcher expose Auto, with package capability gating for older games.
  No duplicated feature policy or deferred adapter is required. Native host
  validation remains pending: build both renderer paths, exercise fullscreen
  Retina/multiple monitors, window resize/minimize, manual/Auto previews and
  rollback, persistence, camera/HUD/cutscene proportions, and both launchers.
  Windows evidence does not verify these Mac checks.


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
  `src/interface/pc_tutorial_ui.inc`) and the existing native LEFT adapter; Mac visual
  checks remain pending.
  Preview, timeout rollback and saved custom modes remain. The subsequent
  `objectives-ui-integration` patch combines MAC-023 sizing with side placement
  in one health renderer wrapper, preserving its split-screen and panel fit.
- **Source:** Feature commit `2d69459` on
  `27-add-aspect-ratio-169-and-more-resolutions`, integrated with main `9e08fa3`.
  `src/pc_display*`, `src/interface/pc_ui_layout.h`, the `ui-layout` and
  `ui-narrow`/`tutorial-layout` runtime patches, `src/interface/pc_tutorial_ui.inc`, and
  `tools/launcher/DisplayModes.cs`/`SettingsPage.cs`.
- **Shared Mac support:** The catalog, config bounds, guest picker and title HUD
  hooks are shared. Native OpenGL uses the shared side-anchor rule and fits narrow
  widgets vertically, including their scissor rectangles. Cocoa consumes the same
  capacity and catalog without duplicating Original. The Cocoa aspect filter now
  shares nearest-mode selection with the guest menu, preserving custom modes and
  distinguishing Original from explicit 640×480. Real Cocoa/GPU tests exercise
  all six groups, custom mode, Save/Cancel/Defaults and failed-save recovery.
  See [native settings evidence](STATUS.md#native-display-settings-parity-2026-10-17).
- **Remaining:** Visual layout checks across the requested ratios and scenes;
  live Cocoa panel visual review. The Windows launcher is not a Mac launcher
  implementation. Existing split-screen layout is preserved.
- **Acceptance:** 4:3, 5:4, 16:9, 16:10, 21:9 and 32:9 menu captures with readable,
  unclipped, proportional text; full backgrounds; ammo/radar corners; centred
  crosshairs and scope aim; zoom controls aligned; pause/return; no leaked placement
  between batches; original/custom settings, save/cancel and timeout rollback.
  Check the first-mission canyon shading for a rectangular tint boundary at wide
  ratios, then check the weapon tutorial with default and longer rebound keys,
  wrapped descriptions, and keyboard/controller switching. Verify supported GPU
  sizes and allocation failure recovery. See issue #27/#31 in
  `docs/project/STATUS.md` for Windows evidence; do not infer Mac runtime verification from it.
- **Local source check (2026-10-17):** HUD placement and tutorial layout C fixtures passed
  on Apple Silicon with Clang. This does not verify native window input, rendering
  or gameplay. [Exact scope](STATUS.md#mac-current-main-backlog-audit-2026-10-17).

### MAC-025 — Reusable unlocked testing profile

- **Status:** Awaiting validation
- **PC behavior:** Local Test Lab fixture unlocks story/Arcade/characters and
  challenges without changing fresh collectible/pickup progress. Disposable
  copies use isolated configurations and the existing startup-video switch.
- **Source:** Commit `0c69834`, [PR #80](https://github.com/TheDoffman/timesplitters-future-perfect-pc/pull/80)
  on `test-profile-fast-start`; `tools/testing/testing_profile.py`,
  `tests/repository/test_testing_profile.py`, `docs/development/testing-profile.md` and `AGENTS.md`.
- **Shared Mac support:** The US save layout and runtime HMAC key are shared;
  fixture generation/preparation uses portable Python. Native execution unverified.
- **Remaining:** Integrate fixture cloning and `RECOMP_XMV_PLAY=0` with the bounded
  native runner. The new play/Recycle Bin wrapper is Windows only. No gameplay or
  existing native launcher behavior changed; no Mac adapter was modified.
- **Acceptance:** Load the profile in native menus, verify late story levels,
  Arcade character selection and challenge availability, retain fresh pickup
  data, isolate both display configs/controls and recycle only test disks.
  Windows evidence is recorded in `docs/project/STATUS.md`; other-device sync is unverified.

- **Native implementation (2026-10-17):** `codex/mac-test-lab` adds
  `--unlocked-profile` to `tools/testing/test-native-mac.py`, using shared fixture
  preparation with native XDG isolation, ownership locks and bounded cleanup.
  The local native-created fresh seed produced a verified immutable fixture.
  A 120-second isolated run loaded Test Lab and selected a late Story level
  through its difficulty screen. Checksums, HMAC and fresh pickup flags are
  preserved. Arcade character/challenge availability and actual later-level
  gameplay remain pending; no completion or pickup-progression claim.
  [Native evidence](STATUS.md#native-test-lab-runner-2026-10-17).

### MAC-026 — Character picker and weapon proportions (issue #22)

- **Status:** Awaiting validation
- **PC behavior:** Correct the horizontal aperture of newly built character
  picker and first-person weapon projections for the selected output aspect.
  Keep each lens, zoom, depth and the original 4:3 behavior; do not reuse or
  repeatedly scale the preceding character's projection during model changes.
- **Source:** Commit `6485069`, [PR #83](https://github.com/TheDoffman/timesplitters-future-perfect-pc/pull/83), on
  `22-character-selection-screen-is-stretched-when-using-higher-resolutions`;
  `src/interface/pc_display_menu.c`, `src/interface/pc_display_menu.h`,
  `patches/graphics/rendering/0830-model-projection.patch`, `tests/graphics/test_decal_projection.py`.
- **Shared Mac support:** The perspective hook is already enabled by
  `TSFP_NATIVE_DISPLAY`; this correction uses the shared aspect model. Both
  platform defines pass the source wrapper tests. No new OS/graphics adapter is
  required; native execution and visual fidelity are unverified.
- **Shared follow-up (2026-10-24):** `fix/native-water-rounding` removes the
  duplicate destination VM query after US151CF0's unconditional matrix writes.
  Camera-source checks remain local/fresh. Both wrapper defines and the local
  original-US output fixture pass on Windows; no new Mac adapter is needed.
  Native performance and visual validation remain pending.
- **Remaining:** Build and visually validate on Apple Silicon. Other-device
  synchronization is unverified; Windows captures do not establish Mac support.
- **Acceptance:** Cycle character models at 4:3, 16:9 and ultrawide without
  changing their proportions during loading; verify first-person weapons,
  aiming/zoom, split-screen, and unchanged world/decal/cinematic projections.
  See [Windows evidence](STATUS.md#issue-22-model-projections-2026-10-16).

### MAC-027 — Arcade RANK widget left placement

- **Status:** Awaiting validation
- **PC behavior:** In widescreen single-player Arcade, anchor the complete RANK
  widget (caption, ordinal and frame) to the left inset, matching the status icons
  below it. Flush preceding/following sprite batches at the placement boundary.
  Preserve 4:3, narrower outputs and the existing split-screen layout.
- **Source:** Commit `3e747a5`, [PR #83](https://github.com/TheDoffman/timesplitters-future-perfect-pc/pull/83), on
  `22-character-selection-screen-is-stretched-when-using-higher-resolutions`;
  `src/interface/pc_display_ui.inc`,
  `patches/graphics/interface/1020-arcade-rank-anchor.patch`,
  `tests/graphics/ui_layout.c` and `tests/graphics/test_ui_layout.py`.
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

### MAC-028 — Preserve frontend camera FOV (#82)

- **Status:** Awaiting validation
- **PC behavior:** Gameplay FOV settings leave the main-menu/profile background
  at its authored lens. In-game cameras retain the selected FOV, including
  falling/landing; aspect correction and cinematic framing remain active.
- **Source:** Branch `82-bug-fov-change-affects-the-main-menu`, based on
  main `abaf001`; `src/interface/pc_display_menu.c`, `tests/graphics/cutscene_letterbox.c` and
  `tests/graphics/test_cutscene_letterbox.py`.
- **Shared Mac support:** The camera function is already shared with native Mac.
  The scene check uses guest state and requires no platform adapter.
- **Remaining:** Run the camera regressions and compare frontend backgrounds at
  55 and 90 degrees on macOS. Native execution and other-device synchronization
  are unverified on this Windows host.
- **Acceptance:** Menu backgrounds retain their lens at either setting; entering
  gameplay and returning to the menu applies the appropriate FOV without
  accumulating changes. Preserve falling, terminal and cinematic behavior.
  See the issue #82 validation entry in `docs/project/STATUS.md`.

### MAC-029 — Skippable idle demo video (#50)

- **Status:** Awaiting validation
- **PC behavior:** The menu idle demo accepts the game's existing skip controls,
  closes its host audio/video and returns to the menu. Repeated stops are safe;
  subsequent movies and normal end-of-file playback remain functional.
- **Source:** Branch `50-cannot-skip-menu-idle-video`, initially based on main
  `abaf001`; final `xmv-stop.patch`, `tools/build/recomp-generation.py`, `tsfp.ps1`,
  `tests/media/xmv_stop.c` and `tests/media/test_xmv_stop.py`.
- **Shared Mac support:** Stop handling and the corrected US symbol map are
  shared. Native generation already refreshes symbols; metadata edits now
  invalidate generated wrappers. This does not add native host XMV playback.
- **Remaining:** Run the portable stop regression and generation checks on Mac.
  Native build/live validation and other-device synchronization are unverified.
- **Acceptance:** A stop reports EOF even without a host decoder; destroy is
  safe afterward. When native movie playback is available, verify idle-demo
  skipping, overlay/audio cleanup and returning to a responsive menu. Preserve
  original-body fallback and genuine Start behavior for other titles.
  See the issue #50 validation entry in `docs/project/STATUS.md`.

### MAC-030 — Menu mouse selection (#86)

- **Integration (2026-10-19):** Shared FOV pointer callback and drawn-bar capture
  now compile on native Mac as well as Windows. Native build and asset-free
  translation-unit compile pass; live slider acceptance remains pending. The
  newer audio/choice/carousel wrappers still need their separate native review.
  [Evidence](STATUS.md#shared-platform-integration-baseline-2026-10-19).

- **Status:** Awaiting validation
- **Issue #114 menu follow-up** (implemented on
  `114-bug-mouse-pointer-cannot-select-some-menu-footer-elements`):
  Tracks the recent slider, resolution-label and choice-arrow changes under
  [#114](https://github.com/TheDoffman/timesplitters-future-perfect-pc/issues/114).
  The same captured-bar pointer policy now handles Controls sensitivity and
  Display settings FOV. Sensitivity uses the selected player's original 0..1
  value in 0.05 steps and original preview, leaving Accept/Cancel unchanged.
  FOV edits only the 40..100 draft, with Apply/Back retaining save/discard rules.
  `menu-setting-sliders.patch` wraps the sensitivity painter and scopes capture
  of its actual bar rectangle; progress/capacity meters remain noninteractive.
  Source: `src/interface/pc_menu_sliders.inc`, `src/interface/pc_display_menu.c/.h` and the existing
  menu pointer adapter. Mac wrappers pass through pending SDL integration.
  The shared Display settings builder also reserves width for long resolution
  values beside their label; native Mac visual validation remains pending.
  Add both settings, per-player isolation, translated bar widths, focus/resize
  cancellation and Apply/Back checks to the native Mac acceptance work.
  `src/interface/pc_menu_choices.inc` and `menu-choice-arrows.patch` capture the standard
  type-1 choice arrows and emit one native Previous/Next edge per mouse click.
  Mac currently retains original drawing through pass-through wrappers; adapt
  pointer input and check both arrow directions, variable-width choices,
  disabled rows, held clicks and stale/resize/focus invalidation. The two custom
  type-1 painters also scope their row owner (`menu-custom-choice-arrows.patch`).
  Windows lobby clicks now select the logical network pad buffer used by native
  callbacks. `src/interface/pc_menu_team.inc` and `menu-team-mouse.patch` capture the local
  player's team arrows and inject directions/Select/Back before the parent tick;
  native wrapping/readiness/network callbacks remain authoritative. PC menu-only
  WASD aliases use D-pad input. Port these input adapters to SDL; verify lobby
  Options/Select Character, team colour changes and ready/cancel with mouse and
  A/D, plus immediate gameplay recapture. These paths passed local Windows
  System Link checks; Mac and remote Internet sessions remain unverified.
  Character-selection portraits use a separate custom carousel. The Windows
  adapter (`src/interface/pc_menu_characters.inc`, `menu-character-mouse.patch`) captures
  actual player-one portrait/variant quads, validates their container ownership,
  and uses native preview and confirmation callbacks. Port click-preview, wheel
  browsing and Select/Back/right-click cancellation; cover clipped/invisible
  portraits, focus loss, resize, stale draws and other players. Local Windows
  System Link portrait/variant selection and confirm/cancel were verified.
- **Issue #115 follow-up** (`115-bug-audio-menu-sliders-cannot-be-adjusted-with-mouse-pointer`): Windows Music/Sound volume bars now
  accept click-to-set and held dragging in main and paused Audio/Video Options.
  `src/interface/pc_menu_sliders.inc` captures both original painter layouts, reuses the
  shared proportional pointer mapping, and updates the game's staged 0..16
  volumes through its existing preview functions. Accept/Cancel remain native.
  `audio-menu-mouse.patch` adds two painter wrappers; Mac calls the original
  painters unchanged until the SDL pointer adapter below is implemented.
  Acceptance must include both bars at 4:3/widescreen/ultrawide, endpoint
  clamping, focus-loss/release/resize cancellation, disabled music/sound, and
  Accept/Cancel with keyboard/controller navigation retained. Shared policy
  tests run on Windows; native Mac input/rendering remains unverified. See
  `docs/project/STATUS.md` under Audio-menu mouse sliders for measured PC validation.
- **PC behavior:** Move the visible cursor over a main, pause or Tab/Objectives menu item to select
  it, left-click to activate, and right-click to go back. The hit test follows
  the proportional UI and presentation bars, using the full UI canvas rather
  than the cropped world viewport during paused updates. Keyboard/controller selection
  remains active until the pointer moves; mouse ownership suppresses old
  directional repeats until a fresh direction takes over, including after a click
  opens a submenu or goes back with the pointer stationary. Held clicks do not
  cross screens or fire when gameplay resumes. Confirm callbacks receive a scoped
  player-one pad edge, so mouse selection preserves valid controller mappings.
  Wheel notches navigate menu rows (including level selection) without activation;
  queued steps are bounded and discarded on clicks, menu changes, or focus loss.
  Wheel ownership spans the gaps between packets and continuous pointer motion;
  hover resumes on a new move after both inputs briefly settle. Clicks stay immediate.
  Bottom prompts use measured rendered text bounds and original enabled/hidden
  checks; clicks feed the existing actions before menu shortcut callbacks.
  Enabled footer prompts turn light blue under the pointer, preserving menu
  fade alpha. Profile-picker prompts are measured separately within their
  centered text rows, including Select, Back and enabled Delete Profile.
  Pause/Objectives Options and Status headings highlight and accept clicks;
  tab replacement waits until the original input dispatch has returned.
  Drawing reads pointer state without consuming wheel input.
  Right-click closes root Pause/Objectives through the original pause handler.
  Returning to gameplay restores mouse capture automatically while preserving
  focus-loss protection and suppressing held menu clicks.
- **Source:** Commit `e18baa9` on `86-enhancement-use-mouse-to-control-main-menu`,
  based on main `e942ef0`; `src/interface/pc_menu_mouse.h`,
  `src/interface/pc_menu_mouse_guest.inc`, `src/input/pc_controls.c/.h`, the final
  `menu-mouse*.patch` stack, and `tests/input/menu_mouse.c`.
- **Shared Mac support (2026-10-17, `codex/mac-menu-mouse`):** Enabled the
  shared guest policy using a locked SDL point/button/wheel snapshot. Window
  operations stay on the main thread; gameplay transitions request recapture
  only while focused. Held menu buttons cannot fire on return. Added the native
  per-frame mode hook as a separate reproducible patch. Real SDL regression
  checks pass; an isolated mouse-only run selected Test Lab, opened Story,
  scrolled two levels, returned with right-click and opened Settings.
  [Evidence and limits](STATUS.md#native-menu-pointer-adapter-2026-10-17).
- **Remaining:** Full acceptance matrix below, physical input, pause/Objectives
  tabs and footer prompts, widescreen/ultrawide alignment and gameplay recapture.
- **Acceptance:** Hover/click main/profile/settings menus at 4:3, 16:9 and
  ultrawide sizes; reject disabled/clipped items and presentation bars, retain
  keyboard/controller navigation, select the profile using only the mouse and
  verify controller mappings stay valid, wheel up/down through level selection
  without activating a level or leaking steps across screens; move the pointer
  up/down while scrolling and verify selection follows only the wheel until
  the gesture settles, then verify hover and immediate clicks work; click enabled footer
  prompts in main/pause/Objectives menus and verify hidden/disabled prompts are inert;
  verify hover tint follows the same hit area, clears on focus loss, retains
  fade transparency and does not drain queued scrolling; check profile Select/Back
  independently and disabled/enabled Delete Profile; switch Options/Status in
  both Pause and Objectives and reject stale labels during transitions;
  close root Pause/Objectives with right-click and resume mouse look without an
  extra click, including after Escape, while unfocused windows stay uncaptured;
  keep selection stable after mouse-driven
  transitions without further pointer motion, and prevent held selection clicks
  from becoming gameplay shots. Run `tests/input/test_menu_mouse.py` and native input
  tests. Windows evidence is recorded in the issue #86 entry in `docs/project/STATUS.md`.
- **Local source check (2026-10-17):** Menu pointer policy and guest-adapter C fixture passed
  on Apple Silicon with Clang. This does not verify native window input, rendering
  or gameplay. [Exact scope](STATUS.md#mac-current-main-backlog-audit-2026-10-17).

### MAC-031 — Quit to Desktop menu entries

- **Status:** Awaiting validation
- **PC behavior:** Main Story/Arcade menu and Pause append Quit to Desktop.
  The game's confirmation screen defaults to Cancel, supports normal menu
  controls, and posts WM_CLOSE only after confirmation. Existing Quit retains
  its return-to-menu behavior. Only player one's menus expose desktop exit.
- **Source:** Commit `e18baa9` on the #86 menu controls branch;
  `src/interface/pc_quit_menu.inc`, `src/interface/pc_display_menu.c/.h`, `quit-desktop.patch`,
  and `tests/graphics/quit_menu.c` / `tests/graphics/test_quit_menu.py`.
- **Native adaptation (2026-10-17, `codex/mac-quit-desktop`):** Enabled shared
  menu insertion and confirmation callbacks on Mac. Confirm queues SDL_QUIT;
  the main-thread event pump owns normal process exit. Cancel, Back, player-one
  checks, failed-post retry and duplicate-confirm protection remain shared.
  Both guest dispatch variants and real SDL queue tests pass.
  [Native evidence](STATUS.md#native-quit-to-desktop-2026-10-17).
- **Remaining:** Pause Cancel/Back/Confirm and mouse-confirm checks. Main-menu
  keyboard Cancel/Back/Confirm now passes; exit code is 0 after SDL shutdown.
  A wide gameplay run also shows the Pause entry, but does not establish its exit.
- **Acceptance:** Cancel and Back restore main/Pause without exiting; Pause
  stays paused; confirmed exit closes normally from both screens with mouse
  and keyboard/controller. Verify menu layout at supported aspect ratios.
  Windows evidence belongs to the issue #86 entry in `docs/project/STATUS.md`.
- **Local source check (2026-10-17):** Guest quit-confirmation C fixture with a mocked close callback passed
  on Apple Silicon with Clang. This does not verify native window input, rendering
  or gameplay. [Exact scope](STATUS.md#mac-current-main-backlog-audit-2026-10-17).

### MAC-049 — Spirit skull and eye rendering (#17)

- **Status:** Awaiting validation
- **Integrated native observation (2026-10-19):** Unchanged runtime `9940dba`
  completed a 155-second isolated MoltenVK run with validation enabled and no
  reported GPU errors. Five guest/intermediate pairs were reviewed; the later
  hall frames show skull geometry, glowing red eyes and dark trailing shapes.
  The closest skull in that run is partly off-screen. A separate 145-second
  follow-up retains unclipped distant spirits and a closer side view in three
  reviewed pairs, without validation errors. This establishes visibility in
  this encounter; controlled wall occlusion, complete multi-angle face fidelity,
  interpreted-vertex behavior and other encounters remain pending.
  [Evidence](STATUS.md#integrated-renderer-pacing-and-spirit-observation-2026-10-19).
- **Native check (2026-10-19):** Shared wrappers are present in the verified
  `5271401` Mac Vulkan build, and the spirit/cache-restoration tests pass within
  a 25-check focused suite. A 120-second Mansion run reached the entrance hall
  and reviewed its characters, chandelier and fire, but the retained frames
  did not show spirits. Do not count this as skull/eye/trail or occlusion
  acceptance. [Scope and evidence](STATUS.md#shared-renderer-mansion-scene-acceptance-2026-10-19).
- **PC behavior:** The US spirit renderer uses clockwise culling for rigid
  skull nodes while keeping skinned trails two sided. Its two eye-billboard
  batches use unbiased clip-space positions and no fog, preserving their
  red texture and depth testing. Overrides are restored after each node/batch.
- **Source:** Spirit rendering commit `70d8e7f` on
  `17-ghosts-in-mansion-of-madness-and-what-lies-below-are-nonvisible`;
  `src/graphics/pc_ghost_render_guest.inc`, the `spirit-rendering.patch` at the end of
  the shared patch stack, and `tests/graphics/ghost_rendering.c`.
- **Shared Mac support:** Guest wrappers and HLE render-state forwarding are
  shared with Windows. No shader hash/string rewriting, geometry fingerprint,
  Windows-only API, or diagnostic switch is required.
- **Remaining:** Build on macOS and verify GPU and interpreted-vertex paths.
  No native execution was available on this Windows host. Confirm the native
  unwritten-fog fallback observes the scoped fog-disable state. The native
  fog adapter now passes enabled/disabled and GPU/CPU fixture checks; actual
  Mansion spirit appearance/occlusion remains pending.
  [Fog adapter evidence](STATUS.md#native-fog-combiner-input-2026-10-17).
- **Acceptance:** Check Mansion entrance-hall spirits from several angles;
  preserve solid faces, wispy trails, red eyes and wall occlusion. Check
  ordinary particles/fog immediately afterward and other ghost encounters in
  both affected levels. Run `tests/graphics/test_ghost_rendering.py` and patch-transition
  suites. See `docs/project/STATUS.md` for measured Windows evidence and remaining limits.

### MAC-050 — Restore a reproducible native build from current main

- **Status:** Verified
- **Driver selection follow-up (2026-10-19, commit carrying this update):**
  `codex/mac-moltenvk-default` depends on the unmerged renderer/interpolation
  stack through `cc364fd`. New builds record MoltenVK in `BUILD.json`; both
  launchers and the isolated runner share `tools/build/vulkan_sdk.py` selection.
  Older manifests retain KosmicKrisp and explicit overrides remain available.
  `tools/launcher/macos/NativeHost.cs` uses a single bounded helper pipe after
  repeated checks exposed a dual-pipe ARM64 runtime crash. Native build/input/
  settings checks, 114 Python checks, 18 launcher groups, 30 fresh prerequisite
  checks and bounded launcher start/quit passed. Windows was not run for these
  Mac adapter changes. The default-path Khallos sample measured 59.995/119.995
  host API presents/s at 60/120; this is not campaign-wide FPS certification or
  a static sky correction. [Evidence](STATUS.md#manifest-selected-moltenvk-and-short-pacing-checks-2026-10-19).
- **PC behavior:** The supported build regenerates from the complete pinned
  patch stack and supplies generated build identity to the shared debug code.
  Current project rules permit focused branches on either platform.
- **Source:** Main `0dc9fea`; `tools/build/build-native-mac.py`, `tsfp.ps1`,
  `tools/build/network-identity.py`, `src/diagnostics/pc_debug.c`,
  `patches/diagnostics/0700-pc-debug-latency-watermark.patch`,
  `patches/diagnostics/0870-native-debug.patch`, and `AGENTS.md`.
  The original integration gaps were found during source review; repair evidence
  is recorded below.
- **Initial audit findings (repaired below):** The native builder used the shared
  pins, generator cache validation, patch stack and isolated regression paths, but its
  hard-coded `branch != 'mac-port'` rejection prevents the current focused-branch
  workflow. Shared `pc_debug.c` unconditionally includes `pc_network_build.h`;
  only the Windows script generates it and only Windows CMake wiring adds the
  generated include directory/version definition. A clean native compile is
  found to fail during restoration and is now repaired below.
- **Implemented adaptation:** Removed the obsolete platform-branch restriction
  while retaining host/XBE/pin checks. Provided reproducible native build identity
  and include/version wiring through a separate reproducible CMake patch.
  Repaired MAC-003's settings preservation and independently reapplied
  MAC-005's dependent-texture work, preserving current-main renderer changes.
- **Acceptance:** From a focused branch, build with the supported US XBE,
  Python 3.12+, pinned dependencies and full patch stack; regenerate wrappers,
  run native platform/MMIO/path/GPU/input/settings/debug tests, check manifest
  provenance, then perform a bounded menu/first-mission smoke test using the
  isolated runner at 60 and up to 120 host presents with a 60 Hz guest clock.
  Preserve both display configurations and personal data. Run affected shared
  checks and report Windows checks unavailable on this host.
- **Evidence/dependencies:** [Current-main audit](STATUS.md#mac-current-main-backlog-audit-2026-10-17).
  These prerequisites were absent during the initial audit and have now been
  restored from the local archive and pinned sources. Restore only required inputs;
  do not transfer an entire Windows build tree or duplicate game extracts.

- **Build repair (2026-10-17):** Implemented on `codex/mac-native-build`,
  based on audit `b884c68`. Focused branches now build; native identity/header/
  version wiring and dirty-source provenance are present. Fresh generation,
  compilation, platform/MMIO/path, real-GPU, SDL input, Cocoa settings/debug
  checks pass. Native upload fixtures explicitly select full-width placement;
  new pixel assertions check the updated proportional-menu contract. Current
  menu boot is visible; remaining scene/60–120 checks are tracked below.
  [Measured scope](STATUS.md#native-build-restoration-2026-10-17).


- **Bounded baseline verified:** Focused-branch generation/build and all native
  suites pass. Current-source first-mission smoke tests at 60 and the 120 target
  show visible gameplay; the wide run checks movement, firing, mouse look and
  keyboard pause/resume. [Scope and measured timing](STATUS.md#native-quit-to-desktop-2026-10-17).
  This status covers build restoration and bounded smoke only. Feature-specific
  acceptance remains in its own entry; Windows execution is unavailable here.

### MAC-051 — Version and build-identity watermark

- **Status:** Awaiting validation
- **PC behavior:** Windows presents a separate lower-right version/compatibility
  identity line on normal and interpolated frames, independently of the optional
  performance line, allowing screenshots to identify the running build.
- **Source:** `e2f655a`, integrated in main; `src/diagnostics/pc_debug.c`,
  `tools/build/network-identity.py`, `patches/diagnostics/0700-pc-debug-latency-watermark.patch`.
- **Native adaptation (2026-10-17, `codex/mac-build-watermark`):**
  `src/native/gl_debug.inc` now draws `pc_debug_watermark()` independently at the
  lower right, through the existing normal/interpolated host-presentation path.
  CoreText measures/truncates long labels; narrow windows fit the full cached
  line. Separate bounded texture/FBO caches preserve the optional FPS line;
  normal SDL shutdown releases them. Shared build metadata comes from MAC-050.
- **Validation:** Real GPU tests check visible text with FPS on/off, lower-right
  placement, narrow/long labels, cache destruction/recreation, pixel-unpack/PBO,
  texture/FBO/scissor restoration and guest-scene isolation. Reviewed generated
  bitmap matches VERSION and the build manifest identity prefix.
  [Measured evidence](STATUS.md#native-build-watermark-2026-10-17).
- **Remaining acceptance:** Live window/fullscreen visual checks and host capture
  review at 60/120 presents. Guest captures intentionally omit the host overlay.
  A compatibility identifier does not establish native network support; see MAC-013.


### MAC-052 — Experimental network bot setup (issue #25)

Issue #222 follow-up (`59cc489`, branch `222-bug-online-bots-are-stuttery-for-guests`): `src/network/pc_network_bot_motion.h` supplies shared
numeric replica interpolation, integrated through the existing Windows bot hook
and monotonic QPC adapter in `pc_network.c`. It requires no second Mac algorithm.
The original native fallback and pending PC XNet adapter (MAC-013) remain intact.
Native and mixed-peer movement/pose/lifecycle acceptance is still required;
Windows loopback evidence in `docs/project/STATUS.md` does not establish native execution.

- **Status:** Queued
- **PC behavior:** The host's multiplayer lobby Options flow exposes
  Arcade BOT SET below WEAPON SET in match settings, reusing its original
  selector, layout and editor. Opening host settings enables the session-only
  roster adapter. Final broadcast/load hooks repair the original late bot-count
  clear that prevented the initial prototype from spawning bots.
  The adapter validates ten bot records, preserves human IDs, caps combined
  slots at the selected map/network profile's capacity (bounded by the sixteen
  physical character slots), and leaves guest/offline/disabled configurations alone.
  The capacity follow-up gives humans priority, retains selected
  bots that cannot currently fit, and shows players/admitted bots/limit in the
  lobby, plus waiting bots on the host. Reliable lobby settings carry admitted
  bots to guests; both launch boundaries recheck capacity. A mode change that
  leaves too many human slots blocks launch without dropping players.
  Initial two-peer validation loaded the same roster on both PCs but left guest
  actors inactive. The follow-up adds living-body activation, bot-specific
  combat references and host-owned numeric state snapshots through LAN/Online
  transport. Follow-up restores radar eligibility and hit-collision flags,
  transfers actual live vitals and clip/frame channels, and recreates missing
  guest bodies on host respawn. The guest follow-up also transfers
  spawn counters, hit bounds and both animation/blend tracks,
  includes AI in score calculation and paused Scores rows, and replicates
  successful host firing events through guest-local weapon models. Locally
  replayed bot effects cannot apply duplicate damage; incoming damage to human
  victims keeps the original path. An original bounds-refresh wrapper preserves host
  hit geometry between snapshots, with body/spawn cache invalidation.
  The authority fix makes the host finish NPC deaths even
  when the shooter belongs to a guest. Guests send reliable hit requests and
  consume host vitals instead of applying speculative or echoed NPC damage.
  Snapshots carry explicit body absence and each bot's victim score column,
  including human kills; confirmed score increases drive guest notices.
  The presentation fix uses the actual localized kill HUD
  callback with persistent victim slots and retries unavailable local views.
  It evaluates guest poses without advancing them in the original animation
  pass, positions weapons after that pass, removes duplicate root movement,
  preserves local visibility/transform mode, and invalidates rotation correctly.
  The post-match follow-up includes AI in the saved Results/Scores records
  (names, character portraits, kills/deaths, ranks and scores), preserving
  the reliable host result packet. Completed-match menus omit health/shield
  gauges; gameplay, ordinary pause and Story objectives keep their behavior.
  PR #344's round-limit follow-up admits configured AI at the original
  score-limit controller filter (return67868, EDI slot), preserving original
  team/threshold/authority logic. Windows and native-fallback adapter fixtures
  run in existing CI; supported-US routine tests need private game data.
  Native Mac still needs the network adapter and live match/results validation.
  Current live acceptance and remaining gaps are in `docs/project/STATUS.md`;
  issue #25 is not complete from a build or roster check alone.
- **PC source:** Issue #25 checkpoint on `25-add-bot-support-to-online-matches`,
  checkpoint `0c8531c` plus the follow-ups included in the commit
  `Fix network bot replication, scoring and guest presentation`:
  `src/network/pc_network_bots.h`, `src/network/pc_network_bots_guest.inc`,
  `src/interface/pc_display_menu.c`/`.h`, `src/network/pc_network.c`/`.h`,
  `src/network/pc_network_bots_state.h`, `src/input/pc_controls.c`, and the
  `pc-network-bots-test` / `pc-network-bot-combat` / `pc-network-bot-scoreboard`
  / `pc-network-bot-shots` / `pc-network-bot-bounds`
  / `pc-network-bot-authority` / `pc-network-bot-presentation` patches.
  Post-match follow-up in `Fix bot post-match portraits, scores and HUD`:
  `src/network/pc_network_bots_guest.inc`,
  `src/interface/pc_display_ui.inc`, `src/interface/pc_ui_layout.h`, and results/UI regression tests.
  Capacity follow-up in `Enforce network roster capacity and display lobby bots`:
  `src/network/pc_network_bots_lobby.inc`, the shared
  roster model/guest adapter, `pc-network-bot-capacity` patch and model/ABI tests.
- **CI fixture follow-up:** Windows FOV tests include fail-fast stubs for
  the bot helpers linked through the menu translation unit; no production
  change or Mac adaptation is needed for this test-only fix. The full Windows
  native CI test selection passed locally (81 tests; see docs/project/STATUS.md).
- **Online visibility follow-up (issue #118, uncommitted):**
  `src/network/pc_network.c` now retains the locally created and registered session ID
  for bot snapshots. Online hosts do not receive their own discovery broadcasts,
  so looking up the host in the XNADDR conversion cache prevented all bot sends.
  Registration of foreign sessions cannot replace the local ID; unregister,
  key replacement and disconnect stop use of the preceding session. Guest
  host/session/match checks and the shared snapshot format remain unchanged.
  `tests/network/pc_network.c` covers Online send/receive without self-discovery,
  rejection and session lifetime. This is the existing Windows transport;
  no additional Mac adapter is needed now. When MAC-013 is implemented,
  preserve explicit session ownership rather than relying on self-discovery.
  Acceptance includes an Online host with an empty address-conversion cache,
  visible/animated guest bots, combat, respawns and reconnects. See
  [issue #118 validation](STATUS.md#issue-118-online-bot-session-ownership-2026-10-18).
  Native Mac execution and other-device synchronization remain unverified.
- **Merge integration:** PR #108 preserves main's menu-mouse/quit adapters
  alongside network bots. The input harness accepts either absolute cursor
  coordinates or sequenced gameplay motion. Renumbered this entry from MAC-028
  to MAC-033 to preserve main's separate frontend-FOV entry. No new Mac runtime
  integration is claimed; shared/native-fallback checks remain the boundary.
- **PC validation:** MSVC model and guest/native-fallback tests passed;
  complete patch migration tests passed. See issue #25 in `docs/project/STATUS.md` for
  the build/menu scope and remaining host/guest checks.
- **Existing Mac support:** The roster model is shared. Native wrappers retain
  original calls; the host menu, configuration override and state transport are
  Windows-only. Numeric snapshot definitions are shared without OS calls.
  Native Mac still lacks the PC XNet transport (MAC-013).
  The post-match HUD rule is shared by the native adapter; no separate Mac
  implementation is needed for that rule, but Mac visual validation is pending.
- **Mac adaptation needed:** Complete and validate PC AI replication first;
  then adapt networking and expose the same editor through the native adapter.
- **Mac acceptance checks:** Two native hosts, and mixed Windows/Mac peers,
  must agree on roster, movement, shots, damage, deaths, respawns, teams,
  scoring, rematches and disconnects. Verify disabled/offline behavior and
  reject incompatible builds. Check guest visibility across turns/respawns,
  weapon attachment alignment and localized confirmed-kill HUD notices,
  including delayed body absence. Preserve personal settings/saves.
  At match end, verify bot portraits and complete named score rows on both
  peers; Results/Scores/Options must hide health/shield, while Story objectives
  and ordinary pause retain them.
  Check mode/map capacity selection, full lobbies, joining/leaving humans,
  zero admitted bots, waiting-roster preservation, and matching host/guest
  lobby counts. Capacity hooks retain original calls on native Mac until its
  network adapter is implemented; Windows results do not verify Mac behavior.
- **Mac implementation and evidence:** No Mac execution or other-device
  synchronization verified for this prototype.

### MAC-053 - Discord Rich Presence (issue #63)

- **Status:** Awaiting validation
- **PC behavior:** Optional launcher Discord tab with an enable switch and public
  Application ID. A Windows worker connects to the running Discord desktop client
  over local RPC, sends Story mission/difficulty/co-op and cutscene/pause state,
  Arcade/League/Challenge map/game type/event, Online/LAN map/type and actual
  human/bot counts with capacity and lobby host/member state, plus elapsed time.
  It coalesces changes to at most one update per 15 seconds and reconnects after
  Discord restarts,
  and disconnects on game-window destruction. Default off; missing or invalid
  runtime configuration disables the integration without blocking play.
- **Source:** [Implementation `ff8d6c0`](https://github.com/TheDoffman/timesplitters-future-perfect-pc/commit/ff8d6c0240bdeccf85a49034dca121071a0adfec)
  on `feature/63-discord-rich-presence`, based on main `c006c24`;  `src/network/pc_discord.h`, `pc_discord_model.c`,
  `pc_discord_guest.h`, `pc_discord.c`,
  the camera sampling hook in `src/interface/pc_display_menu.c`, lifecycle calls in
  `src/input/pc_controls.c`, the `discord-presence` patch, launcher `GameSettings.cs`,
  `SettingsPage.cs`, portable launch/package scripts, and
  [setup instructions](../players/discord-presence.md).
- **Native implementation (2026-10-19, `356e5a3` on `codex/mac-launcher-parity`):**
  `pc_discord.c` now shares the protocol, configuration and rate-limit worker;
  `pc_discord_win32.h` and `native/discord_posix.h` supply only host primitives.
  Native startup/shutdown and guest camera publication use the same model and
  decoder, with editable launcher preferences. Eight real Unix-socket scenarios
  plus the policy suite pass; a bounded native game session was acknowledged by
  the running Discord desktop client, including Time to Split mission/difficulty,
  cutscene, gameplay and pause transitions. See
  [STATUS](STATUS.md#native-launcher-parity-completion-2026-10-19).
- **Remaining acceptance:** Live mode/mission coverage beyond the recorded run,
  other-device validation and native LAN/Online status after MAC-013. The existing
  shared decoder preserves occupied human slots, built-in text bounds and event
  caching. Native networking is not advertised as verified by this launcher work.
- **Acceptance:** Compile shared policy tests on Mac; verify disabled/missing and
  malformed settings, Discord absent/start/restart, fragmented/oversized/error
  frames, mission/difficulty/co-op, map/type/event and population transitions,
  pause/menu/mode transitions and shutdown. Verify a real Discord profile
  and elapsed timer on Apple Silicon with isolated saves/configuration. Never
  infer native support from the Windows results below.
- **PC evidence:** See [issue #63 validation](STATUS.md#issue-63---discord-rich-presence-2026-10-19)
  and [detailed activity validation](STATUS.md#issue-63---detailed-discord-activity-2026-10-19).

### MAC-054 — Fractional presentation targets and Windows transform setup (issue #167)

- **Fixture repair (2026-10-20, PR #219, commit `0f01b2ce`):**
  `tests/graphics/fov_settings.c` retained deleted motion APIs and the old timing label.
  Its replacement checks assert the fixed-60 menu's row set and preservation of
  unrelated runtime settings, keeping the existing FOV/texture/rollback coverage.
  No Mac adapter is needed: shared production menu policy is unchanged. The
  portable pointer fixture and its broken-setting-drag detection pass on Mac;
  subsequent Windows/macOS native CI passed in run 39381646838. The integrated
  Windows fixture also passes locally with MSVC, preserving main's stricter
  seven-row and Back-navigation checks. Full Mac gameplay validation remains
  pending. [Evidence and limits](STATUS.md#fixed-60-settings-fixture-repair-2026-10-20).

- **Current disposition (2026-10-20, commit carrying this entry):** Withdrawn at the user's
  request. Both hosts now use fixed 60 FPS; the interpolation/prediction engine
  is replaced with inert recorder ABI compatibility functions. Shared launcher
  UI removes timing controls and migrates old values without changing unrelated
  preferences. Native scripts reject higher targets and no longer default to 2x.
  Historical implementation notes below are superseded. PR #216 hosted run
  39380510747 passed Mac native regressions, patch upgrades and asset-free GPU
  checks. Full Mac game/visual validation remains pending; these CI checks do
  not establish gameplay fidelity. See [fixed-60 validation](STATUS.md#fixed-60-fps-restoration-2026-10-20).


- **Status:** Awaiting validation
- **Shared stack integration (2026-10-19, PR #186 follow-up, uncommitted):**
  Current main's presentation series overlapped the earlier shared renderer
  patches. The three presentation patches now apply after the shared host,
  replay-depth and present-result changes. CPU prediction uses the same replay
  depth cache; failed/skipped presents retain the shared counting and retry
  contract. Original payloads remain in `patches/legacy` for recorded-stack
  migration tests. This adapts the concurrent renderer integration repair to
  this launcher's earlier baseline without importing its scene/capture work.
  Local Mac source-only checks pass: 28 patch transitions/tracking tests (two
  Windows-only skips), and 71 renderer/frame-clock/CI-scope checks. Windows and
  actual GPU validation of the follow-up remain pending CI. Native fixed
  presentation targets remain queued; this is integration preservation only.
  [Evidence](STATUS.md#launcher-main-patch-integration-follow-up-2026-10-19).
- **Launcher integration (2026-10-19, PR #186, uncommitted):** The common page
  retains the new Windows controls through framework adapters. Native Mac does
  not yet implement fixed fractional targets or temporary monitor switching, so
  those selectors are disabled there. Existing Mac interpolation remains usable;
  changing FOV or interpolation preserves saved `frame_target`/`refresh_rate`.
  A shared UI regression exercises preservation on both hosts. Local Mac build,
  18 host groups and eight shared UI groups pass. This validates launcher behavior,
  not native 144 FPS gameplay. See [STATUS](STATUS.md#api-free-launcher-integration-2026-10-19).

- **Integration boundary (2026-10-19, `codex/renderer-main-integration`):** The PC timing patches
  now apply after the shared renderer series and reuse its replay-depth helper
  and successful-present accounting. CPU prediction cannot consume a capture
  request for a complete guest/intermediate pair. Native builds retain 1x/2x
  rendering with a 60 Hz guest clock; the requested Mac maximum remains 120.
  The launcher preserves saved PC `frame_target`/`refresh_rate` values during
  unrelated native edits without advertising their activation. Windows monitor
  enumeration stays in its host adapter; Mac reports desktop-only/unknown until
  a real native monitor adapter is implemented. No 144 FPS Mac mode is added.
  See [integration validation](STATUS.md#renderer-and-pc-presentation-integration-2026-10-19).
- **PC behavior:** Experimental fixed host clocks for 120/144 FPS, independent
  of the 60 Hz console clock. TSFP's c64–c75 camera/object matrices are now
  configured for Windows replay; projecting alone did not interpolate motion.
  A rational timer preserves the fractional 144/60 phase, drops expired slots,
  omits replaced guest endpoints from Shown, and predicts transforms for no more
  than one guest interval during CPU work. Three history lists keep both
  completed poses and the partial guest frame; a fourth transient list saves
  host state. Separate prediction color/depth buffers preserve partial geometry;
  feedback copies retain the latest real-frame image. During CPU prediction,
  clears and draws to shared effect textures/custom depth are held while the
  main scene continues. This avoids the earlier collapse toward 60 presents
  when a frame included an offscreen effect. Failed/unknown targets still
  disable prediction. This write guard is Windows-only; legacy/native replay
  keeps its normal effect writes.
  Unsafe matches/cuts and movies use
  guest fallback. This is a target, not a guarantee for every scene or machine.
  Fullscreen monitor refresh can separately select supported 120/144 Hz modes
  without changing desktop resolution. Window focus/exit restores the prior
  rate, with a sleeping helper for process-death recovery.
- **Settings follow-up:** Windows launcher provides explicit 60/120/144 setups
  that coordinate pacing, fullscreen, VRR, console scale, the FPS overlay and
  supported monitor requests. Live guidance identifies refresh/fullscreen/VRR
  mismatches. Explicit output resolution overrides console scale in the shared
  display model, so the UI disables that inactive control without losing its
  preference. Current primary-monitor Hz is visible. Buttons edit a draft;
  Save/restart applies it. Mac adaptation should present the equivalent native
  controls and validate its actual scene-size behavior before copying this UI.
- **In-game controls and diagnostics:** Windows Display settings now includes
  Smoothness / refresh with 60/120/144 FPS and Desktop/120/144 Hz choices,
  atomic save for next launch, restart feedback and Back/discard. The Windows
  file writer preserves unknown runtime settings; a native file adapter and
  native presentation-target support are still needed before exposing this
  submenu on Mac. The optional D3D11 GPU timestamp probe is Windows-only;
  native GPU timing needs the corresponding renderer API and real hardware.
- **Recovery follow-up:** Windows 120/144 targets now fall back to guest
  frames after an unavailable interpolation target and retry at most once
  per second. Failed redraws do not update timing from a stale present stamp;
  prediction restores guest state before returning. The shared legacy/native
  path retains its disable-on-target-failure policy and saved 1x/default 2x
  behavior. Native adoption of retry needs real-GPU resource-failure tests.
  Windows refresh restoration retains its pending state and crash guard if
  monitor enumeration temporarily fails. Mac needs its own mode-restore
  lifecycle adapter. Source: additive `interpolation-recovery.patch`,
  `src/interface/pc_refresh.c`, `tests/gameplay/frame_interpolation.c`, `tests/gameplay/pc_refresh.c`.
- **Transition evidence:** Windows `tests/gameplay/stability_transitions_game.py` has
  exercised two Story scenes, level restart/menu return, paused resolution
  keep/timeout, window/focus changes and profile persistence over relaunches.
  The original Controls save-delay finding is addressed by MAC-055 below.
  Native equivalents and physical high-refresh monitor tests
  remain unverified; see the latest docs/project/STATUS.md transition evidence.
- **Source:** `ae4775c` on `feature/167-144-fps`, [PR #188](https://github.com/TheDoffman/timesplitters-future-perfect-pc/pull/188); the additive `144-fps.patch`,
  `src/pc_frame_clock.h`, `pc_refresh.c`, `pc_refresh_guard.h`, `tools/build/crash_helper.c`,
  launcher `GameSettings.cs` / `SettingsPage.cs` / `DisplayModes.cs`,
  `tests/gameplay/frame_interpolation.c`, `prediction_targets.c`, `frame_clock.c`,
  `high_refresh_game.py`, `motion_settings_game.py`, `src/interface/pc_display_menu.c`,
  `pc_motion_settings.inc`, `pc_gpu_timing.inc`, `presentation-timing.patch` and
  [Windows settings usage](../players/windows-launcher.md).
- **Existing Mac support:** Native defaults still use 2x interpolation, with
  saved `frame_interp = 1` preserved. Its bridge already sets c60/c64–c75.
  Shared config read/write retains `frame_target` and `refresh_rate`, but target activation,
  endpoint prediction/waiting, skipped guest presents and launcher UI are
  Windows-only. Shared timing math has a portable native-compiler regression;
  native renderer/GPU validation has not been run on this Windows host.
- **Remaining adaptation:** Decide the native presentation targets supported
  by the attached monitor, integrate its host presentation clock and bounded
  boundary prediction, expose native preferences and separate guest/shown
  counters. Adapt monitor-mode enumeration and temporary switching/restoration
  to macOS APIs. Preserve existing 60/120 behavior while evaluating the new mode.
- **Acceptance:** Run existing native real-GPU/declaration/screen-copy tests,
  portable clock/config tests, isolated Story and Arcade camera pans, pause,
  menu/movie transitions and a slow-frame recovery check. Confirm distinct
  midpoint poses, stable HUD, retained preferences and a 60 Hz guest clock.
  Record CPU submission statistics separately from measured monitor scanout;
  do not infer Mac support or constant 144 FPS from Windows results.
- **PC evidence:** [Implementation and validation](STATUS.md#issue-167---experimental-smoother-presentation-2026-10-19).

### MAC-055 — Persist accepted frontend Controls before quitting Settings

- **Status:** Awaiting validation
- **Behavior:** Accept in main-menu Settings → Controls commits the draft,
  then immediately invokes the original signed-profile save and error-dialog
  workflow. Cancel retains the prior saved controls. The all-profile save pass
  preserves pending accepted edits because the guest's dirty flag is global.
  Pause-menu controls retain their existing behavior.
- **Source:** `903e33a` on `feature/167-144-fps`, [PR #188](https://github.com/TheDoffman/timesplitters-future-perfect-pc/pull/188), continuing the stability
  investigation there: additive `controls-accept-save.patch`,
  `src/storage/pc_profile_accept.inc`, `src/interface/pc_display_menu.c` / `.h`,
  `tests/storage/profile_accept.c`, `tests/storage/test_profile_accept.py`, and the Windows
  isolated integration runner `tests/storage/profile_accept_game.py`.
- **Existing Mac support:** Hook and guest save routing use shared source under
  `TSFP_NATIVE_DISPLAY`; no new OS adapter is needed. Windows compilation and
  execution do not establish native Mac save/menu behavior.
- **Acceptance:** Build the complete stack on Mac, run the portable routing
  regression, then use isolated profiles to change Inverse Look, Accept, close
  directly from Settings, and reload. Repeat with Cancel and verify unchanged
  signed bytes. Validate save-error/retry UI and multiple loaded profiles;
  ensure no draft is committed by Cancel. Preserve personal saves and configs.
- **PC evidence:** See the Controls Accept save follow-up in docs/project/STATUS.md.

### MAC-056 — Texture-camera composition alignment (issue #49)

- **Status:** Awaiting validation
- **Behavior:** Camera feeds keep their logo, mask and scanlines aligned with
  the scene on widescreen displays, including the U-Genix opening wall monitor.
  World-camera and ordinary HUD placement remain unchanged.
- **Source:** `61235d1` on `49-u-genix-screen-bug`, [PR #195](https://github.com/TheDoffman/timesplitters-future-perfect-pc/pull/195): shared
  `src/interface/pc_display_ui.inc`, `src/interface/pc_display_menu.h`, additive
  `camera-feed-placement.patch`, `tests/graphics/camera_feed.c` and
  `tests/graphics/test_camera_feed.py`.
- **Existing Mac support:** The wrapper is enabled by `TSFP_NATIVE_DISPLAY`
  as well as PC controls. Existing native GL and common RHI adapters implement
  STRETCH placement; no new OS adapter is required. Portable regressions are
  registered in both host jobs. Actual Mac rendering has not been validated.
- **Acceptance:** Build on Mac and run the camera-feed placement regression.
  In an isolated U-Genix mission, inspect Crow's opening monitor message and the
  following U logo at 4:3 and widescreen ratios. Confirm no left black strip,
  clipped picture or shifted mask, then check normal HUD and terminal cameras.
- **PC evidence:** See the issue #49 section in docs/project/STATUS.md.

### MAC-057 — Optional texture pack delivery through the launcher

- **Texture health (2026-10-27, `feature/launcher-save-manager`):** Shared settings
  show recorded installed/available pack versions, saved Enhanced mode and an
  unsaved-selection indicator. Verify compares indexed local hashes and enables
  the existing confirmed repair download for missing/changed parts. Custom keys
  are preserved; verification performs no writes. Shared Windows tests pass;
  native Mac UI/download execution remains awaiting validation. No new native
  texture algorithm is needed. See the save-manager evidence linked under MAC-021.

- **First downloadable pack (2026-10-25, `fix/texture-download-publication`):**
  Main's `published: false` index prevented discovery despite committed DDS
  assets. Published audited main `0ccf654f` as `textures-v0.1.0-beta.1`: 2,261
  replacements in 49 immutable, hash-verified parts. The generated schema-3 index
  activates existing Windows/Mac download controls once merged; no launcher
  executable or platform adapter change. Actual public launcher v1.3.1 on Windows
  parsed the full index, downloaded/installed a public part and reused it, while
  preserving isolated save/settings/ISO/custom-texture fixtures. All 17 portable
  publication/packaging tests plus three subtests passed. Native Mac download/UI
  execution of this first live pack remains unrun; shared algorithm is unchanged.
  [Evidence](STATUS.md#optional-texture-download-availability-2026-10-25).

- **Optional-delivery replacement (2026-10-22, `optional-texture-downloads`):** The user's new
  request supersedes the default-bundling follow-ups below. Shared `SettingsPage`,
  `TextureDownload` and `TexturePack` implement opt-in checks, confirmed downloads,
  file-hash adoption of bundled packs, incremental part updates and preservation.
  Both hosts bind the same busy/cancel behavior. Both game packagers omit textures
  by default; separate `texture-release.py` publication supplies the schema-3 index.
  No Mac-specific algorithm is required. Windows checks and portable package tests
  are recorded in docs/project/STATUS.md; native Apple Silicon build, download/cancel/update UI
  and preserved-pack gameplay still need host validation. The optional pack and
  launcher releases have not been published by this change.


- **Alpha.2 removal compatibility (2026-10-22):** Launcher 1.2.24 carries the
  shared retired-icon migration for both hosts. The game publisher requires
  stable 1.2.24+ with both platform downloads when the audited pack declares
  retirements. Existing custom-pair preservation/rollback tests and both host
  launcher builds are required before publication; the new version-gate cases
  run in the existing release-validation suite.

- **Alpha.2 packaging follow-up (2026-10-22):** Both game packagers use the
  shared `build_textures.write_source_files` helper to exclude compiled pack
  files from the nested source ZIP. The complete DDS/JSON pack remains in
  `Textures/`, and source-build reuse is documented. This removes a duplicate
  download/staging copy without changing either host's installer or runtime.
  The combined source/texture delivery regression runs in both release CI jobs;
  real package validation follows on each release host.

- **Publisher requirement follow-up (2026-10-20):**
  `fix/publish-required-textures` adds an unconditional repository-pack gate to
  `tools/release/publish-release.py`, including resumed preparations and pre-upload ZIP
  verification. It rejects missing/unhydrated assets, manifest mismatches,
  private metadata and omitted or changed DDS/JSON entries. The compatible
  Windows/Mac launcher gate can no longer be skipped by dropping texture metadata.
  This is shared Python release validation with no new OS adapter; the publisher
  still builds Windows packages. Native Mac packaging/runtime behavior is unchanged.
  Regression cases are included in the existing Windows/macOS release CI step;
  hosted validation is pending. Scene/installer validation gaps below remain.

- **Release preservation follow-up (2026-10-20, PR #234):** Game/launcher
  version bumps retain the loader, controls, installer, repository texture pack
  and default bundled-game delivery. Repository texture pack now runs on both
  version files and either host's game packager/publisher changes, for PRs and
  main pushes. It hydrates Git LFS and audits the committed DDS/JSON; the obsolete
  separate-release check is removed. This is shared CI policy with no host
  adapter changes. Upgrade/repair/rollback preservation remains required.
  Actual Mac installer interaction and scene fidelity remain awaiting validation;
  see [validation](STATUS.md#texture-preservation-on-version-bumps--2026-10-20).

- **Status:** Awaiting validation
- **Behavior:** Players explicitly download the optional GitHub texture pack or
  select a local ZIP/folder in launcher Graphics & resolution settings. The
  shared installer chooses the runtime's texture path, validates DDS/metadata,
  enforces 6 GiB, preserves unrelated files and recovers interrupted replacements.
- **Source:** `feat/launcher-texture-install` (uncommitted at entry creation);
  `tools/launcher/TexturePack.cs`, `TextureDownload.cs`, `SettingsPage.cs`,
  host window adapters and `tools/textures/texture-release.py`.
- **PC evidence:** Windows compilation, shared UI and installer tests cover
  placement, ZIP validation, checksums, cancellation and recovery. Final results
  are recorded in `docs/project/STATUS.md`.
- **Existing Mac support:** Uses the same service and UI; the adapter uses native
  file/folder pickers and the existing checkout/portable texture-path policy.
  Both host builds execute `tests/launcher/launcher_textures.cs` and shared UI contracts.
- **Remaining validation:** Native CI build/contracts and packaged interaction;
  verify opt-in confirmation, cancellation, installation under a portable root
  and a source checkout, then reuse existing loader evidence. Actual Mac texture
  scene fidelity remains under MAC-011 and the near-completion playtest policy.
### MAC-060 — Native fractional updates at 60/120/144 FPS

- **Procedural wind simulation (PR #367 follow-up):** Shared
  `pc_native_wind*` source and `0460-native-wind-history.patch` subdivide the
  coupled oscillator with per-owner history and exact game-clock admission.
  Original/whole/unsupported-scope loops remain authored; no host adapter,
  setting or save format differs. The portable solver/ownership/clock and
  wrapper-scan tests run on both CI hosts; original-code tests require the
  supported local XBE. Windows local original graphs cover two-owner data/
  matrices, pause/resume, fallback and missing-hook/frozen-matrix controls.
  A normal current-source rebuild and live wind rendering/performance remain
  pending; physical Mac acceptance remains **Awaiting validation**.
  [Contract and limits](../agent-context/native-wind.md).

- **Sampler allocation recovery (PR #367 follow-up):** Common
  `0500-sampler-cache-retry.patch` creates before replacing a sampler-cache owner,
  never retains failed entries, retries allocations while preserving live
  stages, and bounds repeated failure logging. Both hosts use this RHI policy;
  no Mac adapter or gameplay clock differs. Six Windows full-module checks pass,
  including the rejected pre-fix retry/eviction behavior and unchanged normal
  binding digest. The updated normal Windows build and bundled gates pass;
  its26 state/context checks include forward application to the original module.
  Hosted Mac checks and physical driver-failure/gameplay
  validation remain separate. This resource-recovery fix is not a measured FPS
  improvement. [Evidence](STATUS.md#sampler-allocation-recovery-and-current-native-cpu-attribution-2026-10-26).

- **Pushed-array address recurrence (PR #367 follow-up):** Common
  HLE strided copies advance a32-bit guest offset with original wrapping,
  retaining fresh reads, copy widths/order, destination width and fallbacks.
  Windows/Mac share `0570-strided-vertex-increment.patch`; no adapter or setting
  changes. The existing portable full-gather fixture adds large buffers,
  modular-wrap cases and a compiling widened-source negative control on both
  native CI hosts. Windows full gather/wrap/guard checks and clean Original60/
  native240 crowd runs pass; component timing gains and small-case/large-case
  limits are recorded in [STATUS](STATUS.md#pushed-array-address-recurrence-and-current-crowd-profile-2026-10-26).
  Physical Mac gameplay/performance remains **Awaiting validation**.

- **Shader constant uploads (PR #367 follow-up):** Common RHI vertex shader
  state compares fresh constant bytes and skips invalidation for unchanged
  values; changed poses still upload, new buffers remain dirty, screen zoom
  retains its policy and failed maps retry. Windows/Mac share the same
  `d3d8_vsh.c` tail patch; legacy GL retains its direct uniform path. Portable
  original-reference/alias/FP/dirty/failure fixtures run on both native CI hosts.
  Windows selected GPU replay preserves hand/weapon coverage and fresh poses;
  seven alternating timing pairs per frame show modest0.85–1.79% lower median
  renderer intervals, with variability. See [STATUS](STATUS.md#repeated-shader-constant-uploads-2026-10-26)
  for source hashes, pixel-rounding controls and scope. Physical Mac GPU/live
  acceptance remains **Awaiting validation**; Windows does not verify it.

- **Completed local state diagnostics (PR #367 follow-up):** Shared
  `pc_native_local_trace.h` observes local aim/camera and animation timelines
  at the completed US gameplay-update boundary in Original/native modes.
  It is default-off, read-only and row/byte bounded; no Mac adapter or saved
  setting is added. Collector/writer/guard/mutation tests run on both native CI
  hosts. CPU observations do not prove rendered poses or physical input latency.
  Windows live results belong in STATUS; physical Mac gameplay remains
  **Awaiting validation**.

- **Packed vertex conversion (PR #367 follow-up):** Shared HLE
  `0600-vertex-expand-strides.patch` dispatches byte widths/formats per attribute;
  the following `0610-vertex-expand-tiles.patch` bounds passes to32 vertices.
  Both retain fresh input, zero padding, arithmetic,
  allocation/failure policy and per-vertex attribute order. The fixed-function
  selector retains its generic body. No OS adapter is needed; the portable
  noinline compiler boundary keeps its existing scope. Full-function reference,
  alias/overlap, FP, guard-page and negative-control tests join both native CI
  hosts. Windows CPU-kernel results are in STATUS; live Windows/Mac acceptance
  and causal game FPS improvement remain **Awaiting validation**.

- **Live register references (PR #367 follow-up):** Shared generator/template
  policy retains addresses of the same live integer/vector TLS cells in four profiled
  routines (18EA90/21450/214C0/22BE0). No values are cached, ABI layout changed or
  guest clock altered. Common generated-code/callback/alias/thread fixtures
  run on both hosts. Regeneration is required; legacy/manual sources keep
  existing aliases. No Mac adapter is needed. Windows build/tests and live
  measurements are recorded in STATUS as available; physical Mac acceptance
  remains **Awaiting validation**.

- **Native PC locations (PR #367 follow-up):** The existing opt-in Windows
  CPU sampler can report bounded instruction RVAs, symbol offsets and PDB
  source basenames/lines for one requested symbol. DbgHelp is Windows-specific;
  the existing Mac no-op sampler remains unchanged. Portable selection/output
  controls run on both hosts, but do not provide a Mac native-stack adapter.
  This is diagnostic capability only; it changes no common game timing or
  rendering policy. Mac gameplay acceptance remains **Awaiting validation**.

- **Immutable render-state reuse (2026-10-26, commit8d87314c):** Common RHI
  `0490-render-state-cache.patch` retains up to32 exact descriptors each for
  blend, depth/stencil and rasterizer state objects across material switches.
  Dynamic stencil references/scissor rectangles and all guest updates remain
  fresh. Shared complete-module bind/ownership fixtures join Windows/macOS CI.
  No OS adapter or legacy-GL change is needed. Windows build/tests pass as
  recorded in STATUS; physical Mac scene/rendered acceptance remains
  **Awaiting validation**.

- **Player clear-slide guard (2026-10-25, PR #366):** Shared
  `pc_native_walk_guest.h` scopes fractional C54A0 calls so a completed clear
  slide selects the existing small-motion guard. Blocked/Original/whole paths
  retain their branch; no geometry, response constants or save layout changes.
  The optional synchronous C54A0/C4700 diagnostic uses existing Windows/Mac
  memory validation. Public scope/bridge tests and private original-code cases
  join existing native-timing CI; private cases skip without the supported XBE.
  Windows build/live results are in STATUS as available. No new Mac adapter is
  needed; physical Mac scene acceptance remains **Awaiting validation**.

- **Current Temple crowd acceptance (2026-10-25, PR #364):** Unchanged Windows
  runtime731288 completes capture-free ten-bot3440x1440 Enhanced-selected Temple
  runs at Original60/native144/native240, with complete45s guest-Present windows
  and sampled correct game speed. A bounded Windows read-only timing-call
  sampler independently agrees with accepted Presents at diagnostic144. No game
  adapter/policy change; test sampling uses the existing Windows runner. Physical
  Mac gameplay, normal live TRAP rendering and broader maps/visual fidelity remain
  **Awaiting validation**. Exact rates, captures, uncertainty and provenance are
  in STATUS; hosted Mac checks do not substitute for that hardware acceptance.

- **TRAP transform propagation (2026-10-25, PR #364):** Original-US fixture
  connects initializer/callback, post-update dirty invalidation, Euler cache,
  matrix helpers and original/accelerated affine point transforms through240.
  Windows CPU propagation passes; no production/adapter change is needed.
  Private-XBE cases skip on hosts lacking that asset. Actual frame scheduling,
  mesh/GPU rendering, collision services and physical Mac remain
  **Awaiting validation**; see STATUS for scope and measurements.

- **Deferred actor voices (PR #367 follow-up):** Shared numeric policy repairs
  exact164E00 elapsed-time admission. Original normal caller, queue setters,
  scheduler and RNG fixtures cover timing/cooldown/handles through240 with
  controlled voice definitions/audio consumers. No host adapter or audio clock
  change is added. Public scope checks run on both hosts; original routines
  skip without the private XBE. Live/audible and physical Mac acceptance remain
  **Awaiting validation**. See STATUS and the script-clock guide.

- **Registered yaw callback (PR #367 follow-up):** Shared numeric policy admits
  fractional entry at224170. Original registration/entity passes, dirty yaw
  cache and affine point fixtures verify fresh CPU propagation through240;
  original sound/clamp/event paths remain. No adapter is added. Asset-dependent
  originals skip without the supported XBE; public scope checks run on both
  hosts. Live collision/rendered pixels and physical Mac gameplay remain
  **Awaiting validation**. See STATUS and the script-clock guide.

- **Venice/Subway callback clocks (PR #367 follow-up):** Shared numeric policy
  admits positive fractional time at the original timed callbacks' entries.
  Original dispatch/reset/countdown/event tests run through240; Subway also
  checks original RNG/nonrepeat choices with controlled sound completion.
  No Mac adapter is needed; private-XBE checks skip without
  assets. Audible/live scene and physical Mac acceptance remain
  **Awaiting validation**. See STATUS and the script-clock guide.

- **Fresh TRAP steps (2026-10-25, PR #364):** Shared `pc_native_trap_guest.h`
  couples fractional force/travel with carried-tick contact lifetime, replacing
  PR363's held poses. Original/whole/catch-up arithmetic and state layout remain
  unchanged; post-store composition retains the GPU acknowledgement contract.
  No host adapter. Original-US and public clock fixtures cover fresh poses,
  event windows, pause, fallback and live rate switches. Windows full checks
  and rebuild pass; normal-play activation/rendering and physical
  Mac remain **Awaiting validation**. See STATUS and the script-clock guide.

- **Player-statistics clock (2026-10-25, branch `fix/native-statistics-clock`):** Shared
  `pc_native_statistics_guest.h` and D2CB7 numeric callee dispatch scope D1FD0
  time bookkeeping to authored carried ticks, restoring the float delta on
  return. Distance inputs stay per-frame; no OS adapter or saved layout changes.
  Original-US/public clock tests cover pause, fallback, counter eligibility and
  long-run float-addition parity through240. Windows build and isolated
  Original60/native240 Disco counter progression pass; native Mac
  physical execution remains **Awaiting validation**. See STATUS for live checks.

- **TRAP solver clock (2026-10-25, PR #363):** Shared numeric policy in
  `src/gameplay/pc_native_numeric.h` keeps two TRAP phase/travel conversions in the same
  whole-tick span as its fixed per-call impulse; a pause override rejects stale
  carried ticks. Original solver/events/state and host interfaces stay shared.
  Windows original-routine parity and asset-free policy checks are separate;
  private-XBE skips do not prove native Mac execution. Fresh fractional trap
  poses, live activation/rendering and physical Mac remain **Awaiting validation**.


- **TREE phase gate (2026-10-25, PR #363):** `src/gameplay/pc_native_numeric.h` admits
  live fractional intervals only at US1D88B0. The original callback owns phase,
  pose and single-event crossings; its old whole gate lost75% of time at240.
  No host adapter or state layout changes. Asset-free clock coverage runs on
  both CI hosts; original-US phase/pose/event checks require the private XBE.
  Windows build/tests and Mansion callback activation/phase at Original60/native240
  pass; full target rendering and physical Mac remain **Awaiting validation**.
  Exact source mapping, observations and limits are in STATUS.


- **Pushed model copies (2026-10-25, PR #359):** Common HLE size-dispatch
  removes dynamic small-copy calls per vertex while preserving bytes/order,
  guest addressing and ownership. No host-specific adapter; native Mac uses
  the same source. `test_push_gather.py` is registered on both CI hosts;
  physical Mac scenes and comparative pacing remain awaiting validation.

- **Status-effect growth (PR #344 follow-up):** Shared character adapter and
  numeric call scope preserve Original60 model scale/rotation through240.
  No new host adapter. Original-US helper tests cover slow/pause/catch-up,
  jitter/live switches and cleanup; missing private XBE skips these tests.
  Physical Mac and rendered effect activation remain awaiting validation.

- **Character-effect gate (2026-10-24, PR #344):** Shared numeric policy admits
  fractional updates in US1260C0. Its attached-effect timer otherwise loses
  half its elapsed time at120. Original-code/matrix tests cover supported rates,
  phases, slow motion and pause; remaining nested effects use spies. No new Mac
  adapter; private-XBE test skips do not certify Mac gameplay. Physical Mac
  effects/rendering remain awaiting validation. Windows evidence is in STATUS.

- **Completed Online rematch (2026-10-24, PR #344):** The existing Windows
  observer now optionally compares second-round final records after verifying
  reset. A240/Original60 loopback pair completes two rounds with matching eight
  participant records and six moving bots. No runtime/adapter change; physical
  Mac and cross-machine validation remain open. Exact evidence is in STATUS.

- **Arcade HUD grouping (2026-10-24, PR #344):** Shared `pc_display_ui.inc`
  applies the same left anchor to SCORE and RANK panels, preserving their
  original spacing/animation. The overlap also reproduces at Original60.
  No Mac adapter is needed; shared UI/batch/4:3/split-screen tests cover both
  callers in the existing host CI suite. Physical Mac rendering remains
  unverified; Windows scene evidence is recorded in STATUS.

- **Mixed-preset Online validation (2026-10-24, PR #344):** The existing
  Windows diagnostic runner selects independent Original60/120/180/240 peers,
  records movement checkpoints and uses separate startup taps plus a bounded
  alive wait. Windows60/240 and180/120 pairs reach six-bot Vietnam combat;
  accepted movement endpoints differ by at most0.006650 world units. Signed
  profiles/fixture are preserved. This changes no runtime feature or Mac
  adapter. Physical Mac multiplayer, cross-machine behavior and complete
  match/damage equivalence remain pending; see STATUS for rejected attempts
  and asynchronous-observation limits.

- **Zero-based index copies (2026-10-24, PR #344 after merged #328):**
  Shared HLE skips allocating/rebasing an index list whose minimum is already
  zero. Other ranges, fresh vertex gathering and host draw ordering remain
  unchanged. No Mac adapter is needed. Four Windows driver/mutation checks
  pass; both native CI hosts register them. Windows build, all patch upgrades
  and corrected context checks pass. Reviewed Disco combat shows intact
  geometry; clean240/Original60 runs measure237.89/60.00070FPS with fresh
  frames and near-unity game speed. All14CI checks pass on82b27c31, including
  Mac native runtime and patch upgrades/transitions. Physical Mac gameplay
  remains pending. See STATUS for scope/limits.

- **Temple crowd acceptance (2026-10-24, PR #328):** Source925f9f72's Windows
  binary completes isolated ten-bot Temple runs at Original60/native180/240.
  Complete45-second windows measure60.00054/179.99977/239.91279FPS with fresh
  guest frames and game/wall ratios within0.02% of unity. The240 minimum
  one-second rate is236. These are bounded Windows host-Present observations;
  native Mac gameplay, physical latency and every-map acceptance remain open.
  Mac native CI passes on925f9f72; no additional adapter is introduced.
  See the Temple STATUS entry for provenance, exclusions and visual evidence.

- **Combined main integration (2026-10-24, PR #328):** Main fcaedfe3's reload
  timing repair is combined with the guarded model-transform implementation.
  Windows build/bundled checks,161 combined regression cases and all21 patch
  upgrades pass. One isolated Windows session applies240/180/Original60/120
  live and passes12 pause/reload ammo checks. Shared code requires no new
  adapter; this is not native Mac execution evidence or full-game acceptance.
  See the PR #328 integration entry in STATUS.

- **Keyboard reload follow-up #337 (this revision, 2026-10-24):** The shared
  `pc_reload_input.h` model buffers brief presses and gives Reload priority over
  already-held keyboard Fire; fresh Fire still interrupts. `pc_reload_guest.h`
  reads the same US local-player/mode/weapon fields on both hosts. Windows and
  SDL adapters publish key edges and clear state on focus/capture/menu changes;
  the shared binding patch preserves controller contributions. Public schedules
  run in both CI jobs. Windows build and original-US integration pass; native
  Mac build, SDL event delivery and live reload/fresh-click acceptance are pending.
  See STATUS for bounded gameplay evidence; source is not yet published.

- **High-refresh Fire click follow-up #337 (2026-10-27, local branch
  `337-high-fps-fire-clicks`, not pushed):** Shared `pc_reload_input.h` now
  carries brief configured primary/secondary Fire presses across original
  60 Hz input polls, with earlier pulses cleared on Reload and all pending
  input reset on focus/capture/menu/eligibility changes. Windows and SDL already
  publish the same mouse/key edges; no new Mac adapter is needed. Windows
  host schedules and original decoder replay pass at 60–240 FPS; isolated
  120/180/240 FPS Story replays each consumed seven rounds from seven 1 ms clicks. Native Mac
  build and physical mouse checks remain pending on the Mac: click singly and
  repeatedly at 120/180/240, hold Fire, Reload while firing, switch menus and
  focus, and test secondary fire with a remapped key. See STATUS for evidence.

- **Reload input #337 (2026-10-24, this revision):** Shared numeric policy and
  `0650-global-compare-load.patch` let advancing native frames consume reload
  edges even when raw/game whole-tick phases differ after pause. Original60,
  paused-time blocking and original weapon/binding checks are retained.
  Windows original-decoder/reload integration and asset-free branch/mutation
  tests pass. No Mac adapter is needed; native Mac compilation and live reload
  checks after pause at 120/180/240 remain pending. See the #337 STATUS entry.
  Follow-up tests share 864 press/clock schedules between the asset-free gate
  and local original-decoder integration, including jitter, stalls and live
  timing-mode transitions. The missing-hook control must fail at a timing
  mismatch. No new host adapter or gameplay behavior is introduced; CI's
  asset-free schedules do not establish physical Mac keyboard/gameplay parity.

- **Model transform entry (2026-10-24, PR #328):** A shared
  default-off generated entry hook selects only the audited US151E80 leaf,
  after callbacks. Portable C keeps bounded disjoint transforms in local
  binary32 temporaries; rejected inputs retain the generated body. Both build
  paths compile the same helper; no Mac adapter is deferred. Ten Windows
  model tests pass, including original-US routines, four rounding modes,
  disabled-fast control and mutations. Public cases are registered on both
  CI hosts; current Mac execution and physical gameplay remain pending.
  Windows build/bundled checks pass. Isolated ten-bot runs measure 228.36FPS
  at native240 and 60.00015FPS at Original60, retaining game speed; reviewed
  native240 captures show intact scene/models. This does not prove Mac
  behavior or sustained240. See STATUS for numeric and measurement limits.

- **Texture binding scan (2026-10-24, PR #328):** Shared HLE now combines
  first-entry lookup and any-framebuffer-alias detection in one fresh pass.
  No platform adapter, content-check or copy-lifetime changes. Windows full-module
  alias/refresh regression checks pass, including negative controls. Mac GL/RHI
  execute the same registered fixture in CI. All14 checks pass on85ccf0bd,
  including both native hosts and upgrade/transition suites; physical Mac
  gameplay remains pending. See STATUS for build/live evidence.

- **Demo preset lifecycle coverage (2026-10-24, PR #328):** Original-US
  fixtures now execute eight pause-offset recording cases and32playback
  transitions through Original60 across120/144/180/240 with one/four
  controllers. Existing byte, timestamp, edge, lifecycle and mutation assertions
  remain;105 demo checks pass on Windows. No runtime adapter changes. Public
  CI skips private-XBE cases, so native Mac/live file playback remains pending.
  See STATUS for evidence and the bounded unsuccessful packaged-file search.

- **Weapon-menu acceptance (2026-10-24, PR #328):** Current Windows
  e9195841/ed07ee3d preserves issue#105's menu repair at Original60/native240.
  Reviewed captured routes and separate capture-free Present windows confirm
  navigation/rendered screens and approximately60/240FPS, with intact saves.
  Native240's recorder cap excludes the final back-out window; no missing
  window is treated as passed. Mac native CI passes the surface-fixture repair,
  but physical Mac menu/gameplay validation remains pending. See STATUS.

- **Surface fixture repair (2026-10-24, PR #328):** Mac CI33d7bf4f caught the
  portable trace fixture's obsolete144/120/60/144 cycle. It now matches the
  strict240/180/Original60/120 validator and records rate-appropriate deltas.
  The failure reproduces and passes after repair on Windows without weakening
  lifecycle/capture checks. Shared CI confirmation remains pending; no runtime
  or platform adapter changed. See STATUS for the validation logs.

- **Four-preset water cycle (2026-10-24, PR #328 follow-up):** The Windows
  integration route now switches240/180/Original60/120 using default native
  water. Portable image analysis accepts180/240 while retaining strict bounds,
  identity, original/native-path and complete-cycle checks.32 analysis checks
  pass locally; the suite remains registered on both CI hosts. One Windows
  run captures90 images per rate with89/89 changed pairs in every segment,
  reuses the same owner across the cycle and verifies menu labels/gray Apply
  in reviewed captures. Runtime/shader bytes are unchanged. This is bounded
  Windows visual/resource evidence; Mac GPU/gameplay and full-scene acceptance
  remain pending. See STATUS for provenance and measurement limits.

- **Discord sampling guards (2026-10-24, PR #328 follow-up):** Shared
  `pc_display_menu.c` validates each decoder read with a fresh bounded span.
  Fields, cadence, event cache and IPC behavior remain unchanged. Windows
  resident reads avoid full-mapping queries; Mac keeps real Mach bounds with
  no new adapter. Production-callback tests cover exact copies, protection
  changes, read-only/noaccess, overflow and fallback; Windows passes27 related
  checks with8 Mac-only skips, plus the supported build. Existing dual-host
  Discord suite registration includes the new cases. Mac execution of this
  follow-up and physical gameplay remain pending. The Windows45s ten-bot240
  run averages235.244550FPS (lowest1s199, p995.299ms), with fresh guest frames
  and game/wall0.999985. It is not sustained240 or Mac gameplay. See STATUS.

- **Texture survey lanes (2026-10-24, PR #328 follow-up):** Shared
  `0580-texel-sample-lanes.patch` groups four existing independent checksum lanes
  while retaining exact sampled bytes/order and checksums. It changes no texture
  update/upload policy or OS adapter. Windows production-code checks include
  scalar/footprint/bounds parity and tail/overread negative controls; both native
  CI jobs register them. Native Mac execution/gameplay remains unverified for
  this follow-up. The Windows build and bounded ten-bot Original60/native240
  routes pass with fresh frames/near-unity game speed, averaging60.000371 and
  226.507835FPS over45s. The latter is not sustained240 or a demonstrated total
  FPS gain; only the isolated checksum workload is measurably faster. See STATUS.
  Mac CI898ee923 subsequently fails compilation of the fixture's three variants.
  The follow-up avoids redefining libc copy macros, exposes compiler stderr and
  passes five Windows checks including macro-preservation/redefinition controls.
  Native Mac runtime CI passes on15cb9fba after that repair; physical gameplay
  remains unverified. The runtime is unchanged by the fixture repair.

- **Shader source lookup (2026-10-24, PR #328 follow-up):** The shared HLE cache
  verifies full current microcode on source-address hints before bypassing its
  hash. Changed bytes/counts and relocated sources retain the original fallback;
  shader selection, guest calls and lifetime rules are unchanged. Three Windows
  checks include stale-address and eager-hash negative controls, registered in
  both native CI jobs. No Mac adapter change; native Mac execution and physical
  gameplay still need validation. Windows merged source09f0b7fe passes32 combined
  checks and the supported build; its complete45s ten-bot Disco240 run averages
  232.374342FPS with fresh guest frames and near-unity game/wall time. This is
  bounded Windows evidence, not sustained240 or native Mac gameplay; see STATUS.

- **Script and presentation-event coverage (2026-10-24, PR #328):** Actual US
  script dispatch now covers every cadence phase through240 at normal/quarter
  speed, retaining original60 and the legacy missed-handoff negative control.
  Weapon sounds, camera effects and Arcade countdown fixtures add180/240 with
  unchanged assertions/tolerances. All four pass on Windows with verified
  private data. No runtime/adapter changes; existing dual-host registration
  remains, but private-XBE skips are not native Mac execution. Rendered opening,
  audible output and physical gameplay remain pending; see STATUS.

- **Acknowledgement-worker diagnostics (2026-10-24, PR #328):** Shared
  `0480-pushbuffer-diagnostic-policy.patch` caches the survey launch flag alongside
  the existing executor flag and avoids guest-offset lookup when disabled.
  No OS adapter or acknowledgement/clock policy changes. Eight Windows checks
  compile the complete scanner, preserving independent scan/execute modes and
  reports, with eager-lookup and executor-disabled negative controls. Both native
  CI jobs register the new suite; Mac execution/gameplay remain pending. STATUS
  records the separate, unshipped idle-delay diagnostic and performance limits.
  The Windows pushbuffer candidate also completes a ten-bot Temple240 route:
  229.667039FPS over45s, fresh guest frames and near-unity game/wall time. This
  is bounded Windows gameplay evidence, not sustained240 or native Mac gameplay.
  Merged head74e15bec passes Windows/Mac runtime and patch-transition CI. Its
  Windows Venice route records239.978339FPS over50s, but onlyfive scene samples
  fail the generic ten-observation crowd-coverage gate; STATUS retains this limit.
  A separate guest-only PC probe confirms the emulated busy-poll location with
  sparse hits, without proving it is the dominant cost. Neither observation
  establishes native Mac gameplay or supports changing acknowledgement policy.

- **Aim, physics and effects coverage through 240 (2026-10-24, PR #328):**
  Nine more original-US fixtures now include 180/240: player/NPC aim, coupled
  projectile motion, beam geometry, camera tracks, player fall/force and object
  rocking/bounce. All nine pass on Windows, retaining behavioral tolerances.
  Mounted-mouse packet/cadence coverage also includes 180/240; its 11-test host
  suite passes, including legacy-clock/consumption negative controls. These
  are shared test changes only, with no runtime/adapter changes. Public CI
  skips original-US cases without private data; updated native Mac host tests
  and physical gameplay remain awaiting validation. STATUS records scope.
  The same batch adds 180/240 to original light, network correction, screen/strip
  distortion, escort recovery and menu fixtures, plus portable mouse cadence
  and coupled-surface math: all eight selected tests pass on Windows. Surface
  math is not live water acceptance; the same Mac/private-data limits apply.

- **Additional mechanics fixtures through 240 (2026-10-24, PR #328):** Six
  original-US NPC, velocity, projectile, beam, camera and HUD fixtures now run
  at 180/240 as well as existing rates, retaining assertions and tolerances.
  All six pass on Windows with verified private XBE data. No runtime/adapter
  changes are needed. The suite is already registered on both CI hosts, but
  private-XBE skips do not validate these routines on Mac. Physical Mac and
  broader gameplay acceptance remain pending; see the latest STATUS record.

- **Vehicle coverage through 240 (2026-10-24, PR #325):** Existing shared
  vehicle/contact fixtures now cover 180/240 alongside Original60 and 120/144.
  All 19 checks pass on Windows with the supported private US XBE. No runtime
  or adapter changed. Asset-free contracts run on both CI hosts; private-XBE
  skips do not validate native Mac vehicles. Live Mac acceptance remains pending;
  see STATUS for scope and the earlier physics-mutation checkpoint.

- **Quiet trace dispatch (2026-10-24, PR #325):** Shared
  `0450-native-trace-quiet.patch` bypasses disabled diagnostic helpers when the
  launcher supplies a nonpositive trace budget and no watch/dump/filter/profile
  switch. Renderer-idle callbacks and explicit diagnostics remain. No Mac
  adapter is needed; the existing cross-host trace test compiles the actual
  module. Windows build/tests pass; native Mac execution is awaiting validation.
  Gameplay/performance evidence and limits are recorded in STATUS.

  PR #367 follow-up outlines first-use parsing and inlines the cached TLS guard
  in the same common module. Per-thread startup settings, concurrent quiet calls
  and enabled diagnostics have a Windows full-module regression with a rejected
  process-wide-cache mutation. The POSIX fixture uses pthreads; no new Mac adapter
  or atomic compiler extension is needed. Physical Mac execution and game pacing
  remain awaiting validation.


- **State-layout slot preparation (2026-10-26, PR #367):** Shared
  `0550-state-layout-slots.patch` reuses only layout-to-slot integer mappings in
  common HLE state forwarding. Each draw still reads live guest values; no Mac
  adapter is needed. Full-module original/current parity and layout/stale-value
  controls run on both native CI hosts through `test_state_layout_slots.py`.
  Windows source checks pass; physical Mac execution and live performance are
  awaiting validation. Component timing does not establish game FPS gains.

- **60-step presets through240 (2026-10-24, PR #325):** At the user's request,
  the shared game menu and launcher now offer Original60 plus120/180/240 Beta.
  Shared rate parsing and deferred live apply accept180/240; saved144 preferences
  load as120 without an implicit write. Direct144 overrides remain for old
  regression comparisons. No platform adapter is needed. Windows build, expanded
  timing contracts and minimized180/240 gameplay checks are recorded in STATUS;
  native Mac gameplay at the new targets remains **Awaiting validation**.

- **Playback exit metadata (2026-10-24, PR #325):** Shared demo scope clears
  game delta/ticks/exact units when original EOF/cancellation pauses guest time,
  preserving raw time and phases. Shared regression and original-US cancellation/
  EOF checks pass on Windows; the shared mutation rejects stale metadata.
  No new OS adapter is needed. Native Mac execution remains **Awaiting validation**.

- **PR #325 foreground comparison (2026-10-24):** Windows Hotel ten-bot
  Original60/native144 runs on the same binary measured60.000835/143.920524 fresh
  Presents/s with correct aggregate game speed; see STATUS for display and
  coverage limits. The Windows-only test runner now makes its fullscreen request
  explicit. Shared runtime source is unchanged. Native Mac runtime CI passed at
  `4655d8a`; Mac gameplay/performance acceptance remains **Awaiting validation**.

- **Recorded playback scheduler (2026-10-24, this revision):** Shared
  `pc_native_demo_playback_guest.inc` pairs with the recorder correction below.
  Exact raw phases schedule original decoding and fractional clock publication;
  `pc_native_visual_rebase` retains fractional level time. Bounded fresh reads use
  the existing Windows/Mach memory adapter. No new Mac adapter or file format.
  CPU tests cover all rate pairs, legacy zero-tick groups, pause, zero input,
  catch-up, cancellation and live60handoff; asset-free schedule/mutation cases
  run in existing dual-host CI. Windows original fixtures pass; native Mac and
  full-game/file playback remain awaiting validation. This supersedes the
  recorder-only native round-trip regression. [Evidence](STATUS.md#2026-10-24--recorded-playback-uses-fractional-native-time).

- **Native recorder tick cadence (2026-10-24, this revision):** Shared
  `pc_native_demo_guest.inc` skips fractional-frame serialization while keeping
  device/lifecycle calls. It carries offset game ticks and patches only newly
  written records, preserving live globals and the original format. Numeric
  sites `433A0/42535` provide branch selection/reset on both hosts. No OS adapter
  is needed; native Mac execution remains pending. Original-program tests cover
  pause, input bytes, capacity, restart and Original60 handoff. Native playback
  cadence and old zero-tick recordings are still unresolved. See
  [evidence](STATUS.md#2026-10-24--native-recordings-retain-authored-tick-cadence).

- **Raw-clock metadata and recording regression (2026-10-24, this revision):**
  Shared `PcNativeStep` publishes exact raw units/start phase, including pause.
  No guest ABI, file format, timing arithmetic or OS adapter changes. Windows
  CPU tests/build verify publication, reset, rate switches and interval splitting;
  native Mac execution remains pending. The original writer/reader fixture now
  also exposes twelve cross-rate round-trip failures. No playback resampler is
  shipped: a fixed60-packet prototype slows native recordings. See
  [evidence](STATUS.md#2026-10-24--exact-raw-clock-and-recording-round-trip-evidence).

- **Demo wait/exit clock correction (2026-10-24, commit carrying this entry):**
  Shared `pc_native_demo_guest.inc` and numeric sites `433E2/43473` remove the
  minimum-one-tick timer step at native rates. The wrapper preserves the input
  interval across controllers and restores raw time after exit/EOF overrides,
  retaining the original game pause. `0420-native-demo-timing.patch` connects the
  same guest wrapper on both hosts; no OS adapter is needed. Windows original-
  program fixtures cover duration, zero time, one/four controllers, completion
  and mutations; native Mac execution and live demo lifecycle remain pending.
  Recorded packet cadence is still unresolved, tracked by sixteen strict
  expected failures. [Evidence](STATUS.md#2026-10-24--demo-wait-and-exit-timers-use-fractional-time).

- **Water beta default (2026-10-24, `fix/native-water-rounding`, this revision):**
  Common-RHI native120/144 enables the shared persistent field without a test
  flag. Original60 and legacy GL retain original output; explicit environment
  opt-out remains. Shared HLE failure latching avoids per-frame allocation retries
  until scene/device reset. `tests/gameplay/native_surface_runtime.c` exercises that real
  adapter in dual-host CPU CI. Native Mac GPU execution/performance and scene
  acceptance remain awaiting validation; Windows results do not establish parity.

- **Water FPS round trips (2026-10-24, `fix/native-water-rounding`):**
  Shared GPU-cache suspend now invalidates timing/output while retaining owners
  across Original60, avoiding reconstruction on return to native. Constructor
  and device teardown still free resources. Actual guest/cache CPU checks and a
  committed full preset-cycle route cover Windows behavior; native Mac execution
  and transition pacing remain awaiting validation. No platform-specific policy
  or legacy GL rendering change is introduced. See STATUS for measured scope.
- **Persistent visible water candidate (`fix/native-water-rounding`, 2026-10-24):**
  `pc_surface_native*.h`/`.inc`, shared GPU packing, and
  `0440-native-surface-render.patch` connect exact native intervals to the original
  ripple shader with retained owners and scene/device cleanup. Common RHI uses
  the same implementation on Windows and native Mac; legacy GL remains original.
  Opt-in only, with a one-authored-tick cosmetic phase delay to preserve event/RNG
  ordering. CPU timing/guest checks run in the shared test group. Native Mac GPU,
  pause/switch/scene behavior and performance still require host validation.
  See the latest STATUS entry for measured Windows scope; no Mac result inferred.

- **Coupled water operator candidate (2026-10-23, `fix/native-water-coupling`):**
  Shared portable `graphics/surface_coupled.h`, coupled shader and separable
  GPU transform shader bodies and `graphics/surface_basis.h` preserve the coupled float32 feedback map in controlled CPU/WARP checks at 60/120/144.
  No game hook calls this candidate; normal water and legacy GL are unchanged.
  Existing native-timing CI includes its asset-free CPU fixture on both hosts;
  both native jobs pass for PR #316 head 67f9663. This verifies the CPU contract,
  not Mac water rendering.
  Windows WARP covers GPU transforms, fresh reconstruction every update, and
  the original water vertex/pixel programs. A later Original60 runtime capture
  establishes 128x128 wrapped sampling; the test now mirrors its pixel-space
  quad/viewport/screen-space undo and uses shared real Hartley weights. Earlier
  clamped DCT evidence remains diagnostic only. New basis CPU checks are in
  the shared timing suite; native Mac validation of this extension is pending;
  follow-up PR #321 also adds a Windows original-shader reference with captured
  cardinal smoothing. Float/byte divergence persists; no Mac GPU result is inferred.
  Shared `surface_state_shader.h` now materializes alpha-height for the original
  final ripple shader; Windows WARP checks that consumer at every candidate
  frame and through byte output. No runtime hook or Mac GPU acceptance is implied.
  The held-rounding follow-up shares forced-step coefficients and guarded byte
  boundary policy. Existing dual-host CPU tests cover forcing composition;
  Windows WARP compares the original byte trajectory, while native Mac GPU,
  scheduling/resource integration, disturbances and scene acceptance remain pending.
  On `fix/native-water-rounding`, shared `surface_gradient_shader.h` adds the
  unclipped gradient needed to retain first-pass clipping as well as rounding.
  Windows fixtures now cover strong initial signed height impulses and mixed
  frame rates switching within an authored tick. The shared body needs only a
  vector syntax adapter; these Windows tests do not verify Mac GPU execution.
  The same shared shader bodies now retain the second feedback pass's signed
  acceleration clamp and pre-damping height cap at authored boundaries. Windows
  tests cover near-limit velocity/height fields and reject removing either limit.
  This extends common policy; no new OS-specific logic or Mac GPU result is added.
  The Windows fixture now also runs original effect-19 noise shaders with the
  captured immutable source and additive blend before each authored update.
  Constant/mixed-rate byte equivalence uses controlled texture offsets; game RNG,
  queued splash draws and live submission remain unverified. The extra cases
  require a private capture and skip without it. No Mac result is inferred.
  `PcNativeStep` now publishes exact host-side interval metadata from the shared
  timing hook; guest memory/timing policy remain unchanged. Portable
  `graphics/surface_interval.h` splits it at authored boundaries and is covered
  by the dual-host timing suite. Windows tests cover jitter/pause/stalls and GPU
  flow through that splitter; native Mac execution of this extension is pending.
  Resource lifecycle and guest-local time-scope integration are still required.
  Shared `surface_mode.h` and `surface_evolve_shader.h` now support changing
  fractional intervals with immutable mode textures instead of per-delta image
  construction. The portable mode-packing fixture is in the existing dual-host
  timing suite. Windows WARP includes 144-Hz jitter and original-byte comparisons;
  native Mac GPU/CPU execution of this extension and game performance are pending.
  The first explicit Windows hardware check omitted sampler initialization;
  fixing that exposed a genuine later RTX byte-conversion threshold difference.
  Shared boundary correction now canonicalizes only within the unchanged
  arithmetic guard without guessing UNORM thresholds. All 40 RTX 4070 SUPER
  trajectories now pass, with high-rate output fresh every frame. D3D11 timing
  diagnostics are test-only; a Mac timing adapter and actual GPU correctness
  remain pending. Shared `graphics/surface_gpu.h` now owns the coupled RHI kernel
  and restores output state; Windows hardware trajectories use it. Its allocation
  failure fixture is registered for both host compilers. Native Mac execution,
  Vulkan lifecycle checks and guest resource/event hooks remain pending. The
  original event, smoothing and byte-boundary passes are still caller-owned;
  this does not enable a new in-game water path on either host.
  Shared `graphics/surface_gpu_cache.h` now retains four source image identities
  per owner in a five-resource registry, with allocation-free reuse and
  transactional replacement. `native_surface_gpu_cache.c` is registered for both
  host compilers; Windows CPU/GPU fixtures exercise it. Mac compilation/execution
  and guest/HLE reset/generation wiring remain pending; no separate Mac policy
  is needed because the cache uses the common RHI image reference contract.
  The opt-in `TSFP_SURFACE_CAPTURE` hook now records completed original feedback
  through the common RHI in deferred command order. The collector is shared and
  in the dual-host CPU suite; native Mac compilation/readback remain unverified.
  Legacy GL deliberately produces no capture because it lacks these RHI images.
  The base capture does not change visible/default water.
  `pc_surface_gpu_begin_bytes` now shares original byte input/residual initialization
  with the runtime GPU probe; the owner has 37 tested allocation-failure stages.
  `TSFP_SURFACE_GPU_CAPTURE=1` invokes it inside the existing deferred capture
  callback and releases it before returning. Windows live midpoint/endpoint
  comparisons pass; native Mac shader compilation/execution of this extension
  remains pending. The solver still does not drive visible water on either host.
  native Mac execution, GPU adapter validation, byte-feedback appearance and
  gameplay integration remain **Awaiting validation**; this is not an enabled
  Mac water feature. See STATUS and the coupled-water regression record.

- **Respawn diagnostic (2026-10-22, PR #307):** Read-only Windows CPU/health
  observations reproduce a 25.856-ms gap after health restoration; a follow-up
  sampling emulated GPU handshake words does not reproduce the large gap or
  identify its cause. This is instrumented diagnostic evidence, not a performance
  improvement. No runtime/adapter changes. Native Mac gameplay and pacing remain
  unverified; see STATUS for measurements, exclusions and next check.
  Subsequent Windows saved-stack diagnostics catch synchronous texture-file
  work in slow frames even in Classic mode. An isolated no-pack control avoids
  that path, but differing bot outcomes and sampler overhead prevent a causal
  FPS claim. No texture behavior is changed; per-upload timing remains next.

- **Integrated crowd pacing (2026-10-22, PR #307):** Windows `20dc25d`
  averages 143.803 guest presents/sec against playable `4adb6b0` 143.901 in the
  same configured ten-bot Venice route. Both contain isolated slow frames;
  neither establishes locked 144 FPS or removes the reported 110-FPS dips. These are
  capture-free minimized host Present intervals, with nondeterministic bots,
  not scanout or Mac performance acceptance. No runtime/adapter change. See
  STATUS for complete-window checks, clock ratios, hashes and respawn correlation.

- **Vehicle-exit acceptance (2026-10-22, PR #307):** Same Windows `20dc25d`
  executable passes the Siberia jeep drive/exit/walk/pause route at genuine
  Original60, native120 and native144. Y delivers the original exit action; weapon and
  walking camera return, and on-foot movement separates player from stationary
  jeep. Earlier E/F/R attempts used incorrect actions and are excluded. This is
  captured synthetic-input evidence, not physical-device or pacing acceptance.
  No runtime or OS adapter changed. Native Mac gameplay and other vehicles/
  seats remain awaiting validation; see STATUS and the regression record.

- **Corner-contact follow-up (2026-10-22, PR #307):** Windows `20dc25d`
  completes a new native144 walking detour and jeep probe. Solver-entry records
  show side-facing contacts at144 versus front-facing contacts in the genuine
  Original60 run, with small approach differences. The different outcome is
  not proof of skipped solver work; broader handling remains unverified. The
  existing rigid fixture adds 576 overlap/restitution/quantized-normal cases
  against independent impulses; all 19 vehicle tests pass locally. These
  private-XBE cases skip in asset-free CI. No runtime change or new OS adapter;
  actual Mac gameplay is still pending. See STATUS for measurements and limits.

**Live vehicle reference correction:** the vehicle camera/steering/terrain
recordings labeled 60 used native60, not Original60. Startup logs and the new
solver flag confirm this. Their measured values remain valid, but original-clock
live parity is unverified by those runs. Offline Original60 fixtures are separate
and unaffected. See STATUS for the explicit evidence correction.

The optional solver-entry contact trace is shared by both hosts through
`pc_native_contact_trace.h` / `pc_native_contact_guest.inc`; the VM-read adapter
uses the existing Windows/Darwin `pc_guest_memory.h` implementation. It separates
generated and selected lists before the original arena reset, with bounded CSV
output and no guest writes. Asset-free filter/list/writer checks are registered
in the existing both-host timing suite. Windows `20dc25d` builds successfully;
the foreground virtual-pad check explicitly skips. New original60/native60/
native120 traces independently verify the timing mode, capture selected pillar
contacts and stop at the pillar; the native144 walking approach fails to reach
the jeep and is excluded. Actual native Mac execution remains pending. This
diagnostic does not resolve the earlier pillar outcome or certify handling parity.
See STATUS for exact measurements and the corrected reference scope.

- **Controlled contact audit (2026-10-22, PR #307):** The existing shared
  `native_rigid_motion.c` fixture now passes 864 static-contact impulse cases
  against independent mechanics, with a missing-solve mutation. No production
  policy/adapter change. Windows vehicle suite passes 19 tests; this requires
  the private US XBE and skips in asset-free CI. Actual Mac execution, contact
  generation and the live pillar discrepancy remain unverified; see STATUS.

- **Vehicle obstacle follow-up (2026-10-22, built source `fb261e1`, PR #307):**
  Straight no-bot Windows runs stop at the Siberia pillar at60/144, while120
  reaches the bank. A second native60 run also stops with a different pose.
  Small approach differences remain; STATUS records observations and limits.
  No new production fix or Mac parity claim follows. Controlled contact,
  broader handling and actual Mac gameplay acceptance remain outstanding.

- **Vehicle chase camera (2026-10-22, source `24d0ad5`, PR #307):** Shared numeric policy
  admits fractional chase updates at `CA040` and nonzero spring groups at
  `CA424`. No new OS adapter. Full-original camera tests reproduce the old
  lag and pass after correction; registered asset-free load-lowering tests
  cover both hosts. Integrated `fb261e1` passes the full Windows build and
  selected Windows/Mac CI. Bounded no-bot Windows driving at 60/120/144 removes
  the large pre-fix camera-distance mismatch and passes steering/pause checks;
  see STATUS for measurements, asynchronous scope and binary provenance.
  Actual Mac gameplay, broader geometry and pacing acceptance remain pending.

- **Integrated vehicle build (2026-10-22):** Draft PR #307, source `d0116a7`,
  incorporates main `7545d4b`. Windows full build/built-in checks, 147 combined
  source checks, four patch upgrades and 88 repository checks pass. Selected
  Windows/Mac runtime CI jobs pass at this head; workflow completion is tracked
  in the PR. Private-XBE original routine checks remain distinct from asset-free
  CI. Short no-bot Windows runs at all three presets pass signed steering and
  pause checks; rejected setup runs and asynchronous timing limits are recorded
  in STATUS. Exact handling and actual Mac gameplay remain unverified.
  Shared policy requires no new platform adapter. Status remains Awaiting validation.

- **Vehicle steering follow-up (2026-10-22, WIP):** Shared numeric policy adds
  original coefficient site `17306A` to the existing fractional easing rule.
  No host adapter is needed. Original driver/steering tests reproduce slower
  native120 response before correction and pass afterward, including signed
  steering/release, speed target clamp, wheel eligibility and time variations.
  Both-host CI registration already includes the extended vehicle test file
  and its new asset-free SSE case. Windows build `464f850` and its built-in
  checks pass. Live handling and Mac execution of this follow-up remain
  pending; see docs/project/STATUS.md. This does not
  establish the cause of the earlier pillar/bank trajectory divergence.

- **Vehicle/rigid-clock candidate (2026-10-22):** WIP branch
  `fix/native-vehicle-physics`; shared `src/gameplay/pc_native_numeric.h`,
  `src/gameplay/pc_native_timing_guest.inc`, and `patches/gameplay/timing/0690-native-vehicle-physics.patch`.
  No OS-specific behavior or new adapter. Native intervals reach the original
  rigid solver and spring updater; linear adjustment and byte rest timer retain
  authored time units. Original-clock handoff rebases its timestep history.
  Windows timing suite: 67 passed; final focused four, companion114 and patch
  install/upgrade four also passed. Both-host CI registration includes an
  asset-free lifted byte-timer check; original routine tests require the private
  supported XBE. The extended four-case file also passes on Windows with the
  original moving-body/force/drag/rotation pipeline and a controlled support
  contact. `src/gameplay/pc_native_physics_guest.h` now isolates extra-step shuffle RNG;
  three-contact checks retain the same shared seed at 60/120/144. A scoped
  floating addition correction also prevents a ninth secondary solve at some
  fractional step sizes. Both helpers use portable C with host thread-local
  spelling, not a new platform adapter. Original-routine fast/slow/irregular
  checks and an asset-free nesting/isolation case pass on Windows. Final local
  suites pass 70 timing/vehicle, 115 companion and four patch-upgrade cases.
  These host results do not replace Mac execution. Mac
  compilation/execution and live driving, actual collision geometry,
  pause/catch-up and preset switching remain pending. This remains a WIP
  candidate, not verified Mac support or a shipped gameplay fix.

  Follow-up publication checks execute the original body-to-entity position/
  velocity, bounds, vehicle-component integration and sleep/wake routines.
  Shared byte-load policy now also covers the direct memory DEC at `171F83`;
  original/default flags and unrelated sites retain their behavior. Focused
  Windows checks restore the same sleep duration at 60/120/144, including slow
  and fast game speed, and verify immediate fractional wake motion. Mac tests,
  live driving and rendered model behavior remain pending; no adapter change.
  The shared manual wrapper uses a standalone `_gen` declaration so both host
  generators retain its original body. The first Windows link caught the
  comma-declaration defect; a scanner regression now covers this requirement.
  Corrected Windows build `ba4f0bf` passes generation/link/build checks. Actual
  Siberia jeep driving works at the three presets, but a matched throttle
  replay travels substantially less at native rates (see STATUS). Candidate
  acceptance still requires resolving that technical discrepancy.
  The prior playable Windows executable is restored; native Mac validation
  and full handling acceptance remain pending.

  Driver follow-up (same WIP branch): shared gate `17DA03` now admits positive
  fractional game intervals, replenishing inputs consumed by the vehicle each
  update. Original `17DA00/172EE0` tests reproduce missing native throttle before
  the fix and cover forward/reverse/braking/release, pause, speeds and ABI.
  No host adapter change; native Mac execution and live handling remain pending.
  Windows build `67c1864` and 112 timing/vehicle/translator/generation checks
  pass. Matched initial jeep acceleration now agrees within 0.1% across the
  presets, while later pillar/bank contact still differs. This is Windows input
  validation, not full handling acceptance or native Mac verification.

  Character-contact follow-up (same WIP branch): shared `166899` numeric gate
  now preserves displacement-derived relative velocity on fractional updates.
  Windows original `1667C0/166060` fixtures and the coupled solver plus
  `16E780/16DA90` callback path pass; old-gate controls fail at native120.
  Static/rigid contact paths and original mode retain their behavior. The
  controlled support body's motion was already correct; the fix changes stale
  impact information. No new platform adapter. Full game build/live contact
  and Mac execution of this addition remain pending; source tests require XBE.

  Spawn-motion follow-up (same WIP branch, commit titled `Preserve inherited motion on fractional object spawns`): shared gate `172AB5`
  now preserves initial joint-derived velocity/momentum on fractional spawns.
  Original history-copy, spawn and setter tests pass at 60/120/144 with
  rotated transforms, signed inheritance, offset vertices and failure/pause
  paths. The old gate fails at native120. No platform-specific code or adapter;
  Windows full build `3e1ee87` and built-in checks pass, including the previous
  character-contact addition. Native Mac execution and live spawn/contact
  acceptance remain pending. Candidate provenance and limits are in STATUS.

  Actor/vehicle impact follow-up (`60e5b36`, same WIP branch): native divide
  `17CBD6` no longer normalizes already normalized entity velocity a second
  time. Original60 and callback cadence are preserved. Original publication,
  registration, queue delivery, contact construction and suspension response
  pass controlled direct/queued checks; old division and opening only the
  gate both fail at120. No host adapter change. Windows full build and its
  built-in checks pass, along with 115 timing/vehicle/generation regressions.
  Live vehicle-actor acceptance and native Mac validation remain pending.
  A bounded Windows native144 probe reaches the actor callback in live
  Siberia contact and passes pause/resume; it does not measure impact strength.
  The current retained diagnostic build is `60e5b36`; playable `4adb6b0`
  remains restored. These Windows results do not establish Mac execution.
- **Texture-upload timing diagnostic (2026-10-22, `diagnostic/texture-upload-timing`):** Shared
  `texture_pack_trace.h` and traced reader expose opt-in bounded CPU wall spans
  for the common-RHI loader and legacy GL adapter. Windows uses QPC, POSIX uses
  CLOCK_MONOTONIC; texture behavior/budgets/defaults are unchanged. Disabled,
  failed-output, bounded-output and loader stage tests run in the existing
  both-host workflow. Windows full build and stage correlation pass at `2230901`
  (PR #308), with one explicit virtual-pad focus skip. All selected both-host
  CI passes; actual native Mac trace/gameplay remains awaiting validation.
  The worst 12.639-ms interval includes 2.215 ms of recorded attempts, while other
  slow intervals contain none. Key hashing/cached calls and trace-output cost
  remain outside the spans. This is diagnostic evidence, not a hitch fix; see
  STATUS for scope and the main-based executable's provenance.
  Follow-up source now measures source-key hashing on both RHI and legacy GL,
  including cached upload calls, with the same disabled/bounded contract.
  Forty-eight focused local tests pass, including real-loader hash ordering and
  no file/resource work on cached calls. The new hash fields are not present in
  the earlier `2230901` evidence. Windows build/built-in checks pass at `018f2be`
  (virtual-pad focus skipped). The live 50-second window records only 0.710 ms
  hashing, with no uploads in its four slowest intervals; no hashing optimization
  is justified. No live cached calls occurred; the fixture covers that outcome.
  Actual native Mac tracing remains pending for this extension.

- **Disabled trace-filter work (2026-10-22, `perf/disabled-trace-name-scan`):**
  Shared `0370-trace-filter-disabled-scan.patch` avoids scanning every guest function
  name when `RECOMP_TRACE_ONLY` is absent/empty. Enabled matching/hit limits and
  all other diagnostics retain their behavior. No host adapter is needed.
  Six actual-function/mutation cases are registered for both hosts and pass on
  Windows. Supported Windows build/built-in checks and a bounded ten-bot Venice
  session pass at `7b5f042`; foreground routing skips and gameplay remains in a
  visible background window. Actual Mac validation remains pending; removed scan
  counts and this uncontrolled run do not establish an FPS gain. See STATUS.
- **PFB worker notification rework (2026-10-22, `perf/gpu-flush-ack`,
  PR #310):** Shared exact-store notification wakes the original worker, keeping
  its register/DMA/fence service and activity accounting. Windows event/timer
  wait and native Mac condition-variable wait retain the 500-us idle fallback
  and 50-ms diagnostic ceiling. The former `92ffa8b` direct-clear loading stall
  is absent in four bounded Windows runs of `bf4b755`: Original60, native120 and
  native144 (notification on/off). Paired144 main-thread CPU falls 78.57% to
  58.44%, p99 interval 8.291 to 7.513 ms. A second Hotel pair improves typical
  pacing and main-thread CPU but retains an isolated43.776-ms hitch. Windows
  are nonforeground and combat varies. This does not resolve all reported crowded-map drops. Nineteen final
  Windows offline cases pass (Mac-only case skips); native runtime CI at
  `af96547` passes the real Mac event fixture. A declaration-placement conflict
  in historical Mac upgrade context is repaired; fresh/repeated and historical
  upgrade tests pass locally, with current-head CI pending. Native Mac gameplay, foreground pacing and broad campaign/mode
  coverage remain awaiting validation. See STATUS for exact evidence and limits.

- **Native-start clock handoff (2026-10-22, `fix/live-fps-clock-handoff`):**
  Shared HLE Swap/Present paths observe the last real vblank while native pacing
  bypasses the gate. First return to Original 60 can consume the tick already
  awaited by the guest clock, without a second wait. No new OS adapter or
  settings definition; existing real-event/cheap-frame pacing remains. Windows
  phase measurements reduce the extra gate wait from 16.391 to 0.0008 ms.
  Actual-source phase and mutation fixtures run in existing Windows/macOS CI.
  Mac execution and timing acceptance remain awaiting validation; check startup
  at 120/144 followed by Original 60 and subsequent steady pacing on each native
  renderer. [Measured scope](../analysis/native-high-refresh-regressions.md#native-start-return-to-original-60-2026-10-22).

- **Escort recovery follow-up (2026-10-22, PR #297, `535d4ce`):** Shared numeric addition
  at 0x6552D now uses raw fractional time, retaining the original match-dispatch
  pause gate and game-clock delay. Full supported-XBE match/mode/recovery chain
  checks and an asset-free clock contract cover original/native rates. No new
  host adapter; native Mac gameplay remains awaiting validation. The prior
  drain-only head 264bc50 passed both-host CI; follow-up results are in STATUS.

- **Walking boost resource (2026-10-22, `fix/native-boost-timing`, `bcb1f78`):** Shared
  `pc_native_numeric.h` scopes fractional charge scaling to US site 0x259060.
  Windows actual-routine evidence shows the old boost drain doubled at 120 and
  multiplied by 2.4 at 144; the fix preserves original active-update consumption
  at normal, slow/fast and catch-up intervals. No new OS adapter. Asset-free
  clock/resource tests run in both-host CI; the complete walking fixture needs
  the supported private XBE. Native Mac execution and live activation remain
  awaiting validation. See the walking/boost audit in
  `docs/analysis/native-high-refresh-regressions.md` for exact scope; the game rule-name table identifies mode 0x11 as Escort.

- **Full-float feedback follow-up (2026-10-22, PR #299, `6035ed7`):** The shared opt-in
  policy selects 32-bit float intermediates to remove measured half-float
  accumulation error. The common RHI already maps this format on D3D11/Vulkan;
  preservation now handles its capture-storage channel order too. Local WARP
  conversion and two-pass spatial diagnostics pass. The Mac asset-free common-RHI
  GPU conversion contract passes for `6035ed7`; Windows bounded scene routes
  pass. Native Mac wave shaders/gameplay, live preset switching and full scene
  dynamics remain awaiting validation.
  Legacy GL remains unsupported for this experiment, as before; normal-play
  defaults and original byte targets are unchanged. See STATUS for scope.

- **Coupled-water follow-up (2026-10-22, PR #298):** Controlled Windows WARP
  execution distinguishes fractional coupling from float32/half-float/UNORM8
  storage effects. It uses the original second shader with a fixed neighbor;
  full spatial dynamics and native Mac GPU/scene behavior remain awaiting
  validation. The shared historical-upgrade fixture now excludes the new
  coefficient patch when constructing old stacks lacking its prerequisite.

- **Precise fractional water coefficients (2026-10-22, `1d02157`):** Shared
  policy and HLE patch extend the existing disabled experiment; no new host
  adapter or capture format. The actual current guest uploader supplies the
  truncated reference values, replacing an earlier rounded diagnostic assumption.
  Queue/scope/replay and local Windows original-program GPU checks pass. The
  combined Windows build and three bounded Scotland routes pass; every captured
  image updates, but measured amplitude differs and motion parity is unproven. Native
  Mac GPU/scene execution and complete wave/appearance acceptance remain awaiting
  validation; this is not enabled for normal play. See STATUS for current scope.

- **Water damping coefficient diagnosis (2026-10-22):** The local Windows WARP
  original-shader case now isolates packed fractional coefficient drift and a
  precise anchored alternative. This is a diagnostic, not a runtime change.
  Shared CI knows the fixture, but skips without the private XBE/Windows WARP.
  Native Mac GPU/scene validation and full water acceptance remain awaiting
  validation. See STATUS and the fractional damping audit for measured scope.

- **Projection query candidate (2026-10-22, `perf/projection-memory-checks`,
  PR #300, `beff7a7`):** Shared `pc_display_memory.h` retains a VM region only during
  one post-guest camera/projection correction. It uses the existing Windows
  query and Darwin Mach region adapter, with no new OS-specific policy. Bounds
  and write permission remain checked; snapshots expire before guest execution.
  Sixteen focused Windows geometry/protection/ABI cases and both-host native CI
  pass, including the real-VM contract on Mac. Combined Windows build `4adb6b0`
  passes; two Venice ten-bot samples average 142.933/143.240 FPS versus 140.843
  before this change. One unexplained 60.555 ms hitch does not recur. Actual
  native Mac camera/cutscene visual acceptance and performance remain awaiting
  validation. See STATUS and the detailed regression record for scope.

- **PR #285 CI harness repair (2026-10-21):** Shared framebuffer/ghost fixtures
  now supply current include paths and host interfaces. Cutscene/decal fixtures
  extract camera code without the adjacent menu allocator that Clang rejected.
  No runtime policy or Mac adapter changes; 18 affected/related Windows cases
  and 99 release checks pass. Fresh macOS CI remains pending; these source
  contracts do not establish native Mac gameplay or visual parity.

- **Main integration for PR #285 (2026-10-21):** Renumbered from MAC-058 to
  avoid main's crane entry; keyboard entry remains MAC-059. Shared native timing
  and unified Video Options retain Auto mode, main's scoped prompt reads/fresh
  scratch checks, mounted input and late-frame gate correction. No separate
  platform policy was introduced. Windows development build and focused native
  checks pass; native Mac execution and physical/visual acceptance remain
  awaiting validation. See docs/project/STATUS.md for exact source parents, build and results.

- **Clock-aligned presentation evidence (2026-10-21, this revision):** The
  shared recorder serializes its existing host nanosecond timestamp without
  changing presentation policy. Windows RHI uses QPC and the isolated runner
  records matching input-request/observation clocks. Shared analysis rejects
  incomplete windows and discontinuous counters; portable tests are registered
  in runtime CI. Windows Build/checks and paired live FPS cycles pass their
  measurement gates, but reveal 36-39 ms return-to-original-60 gaps with and
  without experimental water. Mac clock-domain alignment and live behavior
  remain unverified; raw GL/SDL timestamps must not be assumed to share the
  Windows runner origin. Existing gameplay/platform support is unchanged.
  [Evidence and unresolved pacing](../analysis/native-high-refresh-regressions.md#clock-aligned-live-fps-pacing-2026-10-21).

- **Live water precision policy (2026-10-21, this revision):** Shared guest
  and HLE code now selects byte targets at original 60 and half-float at
  120/144, finishing queued old-mode draws before publication and converting
  changed sources before sampling. Existing transactional preservation handles
  conversion failures. Windows Build and production fixtures pass; a bounded
  Scotland 144/120/60/144 menu cycle supplies four validated 90-image segments.
  All four diagnostic flags stay disabled by default. Common RHI needs no new
  Mac-specific simulation; native Mac build/gameplay, transition latency and
  appearance parity remain unverified. Legacy GL still lacks half-float targets.
  [Measured scope](../analysis/native-high-refresh-regressions.md#live-water-precision-transitions-2026-10-21).

- **Feedback precision preservation (2026-10-21, this revision):** Common RHI
  copies live state into a provisional replacement before the host cache
  publishes a precision change. Failure keeps old content/bindings; converted
  CPU storage preserves capture/replay initialization. Windows WARP byte/half
  round trips and production cache rollback checks pass; the same GPU cases
  run through `tools/testing/test-rhi-mac.py` on an available Mac. Native execution
  remains unverified. Legacy GL explicitly returns unsupported and preserves
  the old texture; its float-target gap remains. FPS-driven policy and live
  gameplay transitions still need integration/validation on both hosts.
  [Measured evidence](../analysis/native-high-refresh-regressions.md#feedback-texture-preservation-2026-10-21).

- **Ordered water centering (2026-10-21, this revision):** Shared formula and
  float-constant state, common HLE queued transport/capture/replay, and RHI/GL
  adapters implement an opt-in second-pass correction. Windows original-code,
  queue/replay and GPU checks pass; corrected 120/144 final images stay fresh,
  and 144 variation no longer fades in the bounded sample. This does not prove
  original-byte appearance or full dynamics. Native Mac compilation/execution,
  live precision transitions and longer motion/pacing checks remain open.
  Legacy GL constant support is implemented, but its half-float target gap
  still blocks the combined experiment. All three switches stay off by default.
  [Measured evidence](../analysis/native-high-refresh-regressions.md#ordered-fractional-water-centering-2026-10-21).

- **Fractional buffer reuse (2026-10-21, this revision):** Shared guest
  wrapper/patch skips extra smoothing while preserving newest-state ownership.
  Original-code fixtures check lineage and ABI at 60/120/144 and across
  transitions. Windows Build, 91 checks and three patch migrations pass.
  Windows GPU images stay fresh, but 144 variation degrades; this experiment
  is not accepted or enabled by default. Centering correction, intermediate GPU
  observations, live precision transitions and actual Mac execution remain
  open. The wrapper is shared; there is no platform-specific copy of its policy.

- **Procedural shader arithmetic audit (2026-10-21, this revision):** Two
  original-program GPU fixtures use the common HLSL translator/RHI and pinned
  local XBE on Windows WARP. They identify per-pass centering drift and loss
  of variation in zero-radius smoothing. No runtime correction or Mac adapter
  change is made. Ordered state transport, buffer ownership, native Mac GPU
  execution and scene acceptance remain open; see STATUS and the arithmetic
  audit for the tested inputs and limits. CI registers the fixtures and skips
  when the required host or private executable is absent.

- **Half-float feedback experiment (2026-10-21, local source):** Shared
  `pc_surface_precision.h` and its original-constructor wrapper register only
  feedback intermediates; final textures and all guest allocations stay intact.
  Common HLE cache creation selects half float on Windows/native Mac RHI. The
  legacy GL adapter explicitly refuses the diagnostic; it needs float render
  storage and matching sampling/readback support. Its CPU pitch is already
  format-aware, correcting the preceding generic-GL audit. Four Windows image
  comparisons eliminate the fractional 144 freeze and demonstrate fresh 120
  output, while revealing appearance differences from the original byte path.
  Both experiments remain off by default. A shared real-GPU resource contract
  passes on Windows WARP and is wired into the existing Vulkan fixture; actual
  Mac execution, live preset switches and motion/appearance acceptance remain
  unverified. Windows capture-free Scotland pacing sustains 144.006 guest FPS
  with normal game speed; two rendered shoreline stills show intact water and
  reflection rendering, without establishing appearance parity.
  Windows Build, 89 focused checks and three patch migrations pass. See STATUS
  and the half-float regression record for image metrics and remaining scope.

- **Rejected procedural feedback experiment (2026-10-21, local source):**
  Shared `pc_native_surface_guest.h` and reproducible wrappers add an explicit
  `TSFP_SURFACE_FRACTIONAL_TEST=1` experiment; the default is unchanged. It
  preserves discrete disturbances while stepping feedback at native cadence,
  but Windows GPU images reject it: reduced variation at 120 and a flat result
  at 144. A same-build disabled Windows control retains normal variation.
  Portable fixtures cover original dispatcher/upload ABI and disabled
  pass-through, not GPU equivalence. Raw-channel histogram statistics now expose
  loss of variation without float32 accumulation error. The existing shared CI
  selections include these checks. Native Mac compilation/readback/appearance
  remain unverified; do not enable this candidate on either host. A replacement
  must retain fractional feedback precision and original effect behavior. RHI
  supports float target formats, while `src/native/renderer_gl.c` allocates
  render targets as GL_RGBA8; changing formats will require explicit adapter validation,
  not a Windows-only simulation. See the rejected-candidate record in STATUS.

- **GPU image freshness diagnostic (2026-10-21):** Shared
  `pc_native_surface_trace.h` and original-function wrappers use the common
  D3D surface readback interface, available in RHI and legacy GL, without
  changing the simulation. Windows captures prove repeated images on 205 of
  359 native-144 comparisons versus none at original 60. The bounded COM
  fixture and portable image-statistics tests run through shared CI; Windows
  Build and isolated captures pass. Native Mac compilation and actual GPU
  capture still require host verification, including surface lifetime/readback
  and raw channel semantics. The Windows runner CLI is host-specific test
  tooling; no duplicated gameplay policy is introduced. Fractional simulation
  remains unfinished on both hosts. See STATUS and the GPU image record.

- **Procedural surface audit (2026-10-21):** Windows-only bounded process
  observation in `tests/gameplay/stability_transitions_game.py --watch-surfaces` locates
  an active kind-1 resource in Scotland at original 60/native 144; sampled
  Venice gameplay has none. Replays are shared JSON fixtures. Production timing
  and shaders are unchanged. The five-slot observer is diagnostic tooling, not
  a second platform implementation. Native Mac needs equivalent bounded
  resource/render-target observation and actual scene/image validation. CPU
  stamps and Windows captures do not prove GPU image cadence or Mac behavior.
  Fractional procedural simulation is still open on both hosts; see STATUS and
  the linked live-resource audit for evidence and precise scope.

- **Fresh analogue input (2026-10-21):** Shared `pc_native_input_guest` in
  `pc_native_timing_guest.inc` now runs original axis/connection decoding on
  fractional native updates while preserving the published digital held/edge
  cadence. It removes an unintended 60 Hz hold without duplicating dead-zone
  policy or changing raw mouse handling. Original-code four-port 60/120/144
  checks verify stick/trigger freshness and unchanged absolute rumble commands;
  Windows build and live menu/gameplay checks pass. The existing shared timing
  test entry includes this XBE-dependent fixture when the supported asset is
  available. No new OS adapter is introduced, but native Mac controller/keyboard
  sampling, menus, pause/hotplug and physical rumble/latency still need actual
  host validation. See STATUS and the linked input regression record.

- **Completed-update pose observation (2026-10-21):** Shared
  `pc_native_pose_trace.h` / `pc_native_timing_guest.inc` add a disabled-by-default,
  bounded observer at the original gameplay clock boundary. Both timing paths
  use the same collector; no host-specific movement behavior is introduced.
  Windows original-60/native-144/120 comparisons show consecutive fresh local
  and remote positions across five holds at the selected update rates and
  consistent game time. Build and portable observer/statistics checks pass;
  the new checks are selected by shared CI. Native Mac still requires an
  equivalent bounded runner route and usable transport, followed by actual
  runtime/pose/pacing verification. No Mac validation is inferred from Windows
  results; GPU/scanout and the earlier locally stopped routes remain separate
  acceptance gaps. See the completed-update comparison in STATUS.

- **Remote correction/observation audit (2026-10-21):** The portable lifted
  `sub_00356E20` fixture verifies fractional correction and original guard paths;
  no production change is required for that routine. Windows read-only paired
  observations now use QPC and bounded background sampling instead of a coarse
  Python clock. `network_motion_stats.py` and its six portable tests join shared
  CI; Windows checks pass, but the process-memory sampler is Windows-specific.
  Implement equivalent bounded observation in the Mac runner for host validation;
  do not treat these loopback endpoints or intermediate memory changes as Mac
  transport, rendered-pose or latency acceptance. Native Mac gameplay, sustained
  remote smoothness and its transport adapter remain open. See the in-motion
  network audit in STATUS and the linked detailed regression record.

- **Venice comparison (this revision):** The existing shared recorder now has
  an opt-in `--profile-presents` route in the Windows bounded session runner,
  usable for original and native timing. Same-replay Venice comparisons show
  59.999/143.648 guest presentations/s and correct native clock speed, with
  explicit bot-workload/scanout limitations in STATUS. This changes test tooling,
  not platform gameplay. The Mac recorder remains shared; actual Mac Venice,
  pacing and gameplay acceptance require the native Mac runner and hardware.

- **Corrected original/mixed-rate comparison:** Explicit Vietnam Deathmatch
  with no bots passes bidirectional settled-position delivery at original 60/60
  and native 144/120 on Windows. `network_movement_stats.py` and its portable
  tests reject stationary peers, wrong scenes and stale/non-finite endpoints;
  the tests join shared runtime CI. No production or Mac transport change was
  made. Mac execution, continuous remote motion and cross-machine latency still
  require independent acceptance.

- **Mixed-rate loopback acceptance:** Windows System Link at native 144 host /
  native 120 client reaches a two-human Subway Thief match, with bidirectional
  settled-position delivery and client movement while the host menu is open.
  The original-60 attempt remains at team selection and is rejected for movement
  parity. `stability_transitions_game.py --lan-role` adds isolated loopback
  observation only. It needs Windows process-memory/window APIs; the unchanged
  shared timing code still needs actual Mac acceptance and a Mac transport.
  Cross-machine networking, remote pose smoothness and combat remain open.

- **Native network cadence audit:** Portable test composition of the shared
  native clock and Windows guest network adapters preserves console-clock
  packet gates at native 60/120/144, including wraparound, live rate switches,
  pause and a stall. The existing native fallback remains separately checked.
  No game or transport code changed. The fixture joins the existing shared
  runtime CI selection; actual Mac execution and the missing Mac XNet adapter
  are not validated or supplied by this simulated Windows boundary.

- **Native 120 Venice follow-up:** The same Windows replay on `dbb84e3`
  reaches 120.003 guest presentations/s with p99 9.429 ms, real-time game
  speed, matching two-shot/30-degree aim checks and zero game delta during
  pause. The test uses common production timing code without new host logic.
  Native Mac gameplay, input delivery and presentation still require their
  own run; this Windows result does not change the entry's validation status.

- **Settings ordering (this revision):** The common guest Settings constructor
  orders Audio, Video, Controls, Copy Profile and conditional Cheats, using
  consistent alignment without the old divider. It retains the native actions
  and multiplayer profile selector. Windows Build passes; Mac menu navigation,
  alignment and multiplayer profile selection require validation.

- **Menu order / divider removal follow-up (2026-10-21):** This revision updates shared
  `src/interface/pc_display_menu.c` and `tests/graphics/fov_settings.c` at `894cb45` insert Video
  before Controls in pause Options, matching the frontend. Frontend Cheats
  remains conditional and last. The silver Apply divider is removed again at
  the user's explicit request, superseding the previous note. Windows Build
  and six menu/display checks pass; no host-specific adaptation is needed.
  Native Mac menu navigation/visual checks remain pending. See docs/project/STATUS.md.

- **Pause Audio alignment / Video divider follow-up (2026-10-21):**
  Uncommitted shared `src/interface/pc_display_menu.c` / `tests/graphics/fov_settings.c` on
  `feature/native-144-fps` at `93a805a` retain the pause builder's centered or
  multiplayer layout when renaming Audio; only frontend Audio receives the
  left-aligned style. The native silver divider above Apply is restored after
  user feedback, superseding the removal noted below. No host adapter is
  needed. Windows Build, six menu/display regressions and bounded Story-menu
  captures pass; native Mac execution remains pending. Verify Audio alignment
  in main/pause and split-screen menus, submenu return, and the separator with
  both clean and dirty Apply. See the matching docs/project/STATUS.md validation entry.

- **Unified Video/Audio Options (this revision):** Shared `pc_display_menu.c`
  stages resolution, FOV, texture style, FPS and Subtitles in one Video Options
  page, with a disabled-until-dirty Apply. The remaining silver divider above
  Apply is removed in the menu follow-up (2026-10-21); its shared callback
  fixture passes with seven rows and zero separators. It removes
  submenus, the Back row, FPS disclaimer and default-FOV note, rounds shared
  frontend/pause highlights across menus,
  renames Audio Options and aligns its Settings entry. Subtitles updates the
  original profile field/save flag after successful application; native Audio
  persistence remains in use. The common guest renderer/dispatch paths serve
  both hosts without a new OS adapter. Windows Build and relevant shared/menu
  checks pass; native Mac layout, mouse/controller access, persistence and
  same-process application still require validation. See STATUS for scope.

- **Live FPS application (this revision):** Shared `pc_frame_rate_live.inc`,
  `pc_native_timing_guest.inc` and `pc_display_menu.c` apply saved 60/120/144
  choices at an update boundary, preserve native timer remainders, reset stale
  pacing state and return to original timing without restarting. Failed saves
  do not queue a change. The helper/disclaimer rows use 75% native text scale.
  Windows uses Interlocked publication; the existing C11 native Mac config
  build uses atomics, with immutable strings for concurrent readers. No separate
  Mac feature implementation is needed. Windows configuration/timing/menu,
  patch migration and Build checks pass (one focus-dependent routing skip).
  Same-process frontend preset changes pass on Windows; Mac menu rendering,
  atomic persistence, live switching and gameplay acceptance remain pending.

- **Region-driven force/drag (this revision):** Shared `pc_native_numeric.h`
  normalizes mode-0x800 force/drag and view settling while preserving original
  post-drag speed limits. The complete original selector, state setter, movement
  and falling routines pass 76 controlled normal/quarter/accelerated sequences,
  irregular frame intervals and unchanged catch-up checks on Windows. The final
  56 timing/backlog checks and supported Windows Build pass (`dbb84e3`). No host
  adapter is required. Mac execution, live region/geometry behavior and network
  peers remain pending. See STATUS and the native regression record for measured
  integration differences and the one-ULP speed-ratio correction.

- **Airborne view-height damping (this revision):** Shared numeric policy at
  `0xD41D0`/`0xD46B4` preserves original view-offset settling at fractional raw
  rates, including slow game time. The shared original-routine fixture covers
  both airborne paths, controlled landing/damage, terminal speed, pause and
  catch-up on Windows; 158 related checks and the full Windows Build pass.
  No host adapter is required. Native Mac execution and
  real world-collision/perceptual validation remain pending. Existing fractional
  gravity trajectory differences are explicit in the native regression record.

- **Hotel match-flow evidence (this revision):** Windows build `2adcf29` completes
  local Hotel deathmatches at original 60/native 144 and displays results;
  native results navigation also starts a new countdown. Saves/settings remain
  unchanged. No runtime or adapter change accompanies this check. Equivalent
  Mac match/respawn/results validation, online peers and other modes remain
  pending; see STATUS and the Hotel section of the native regression record.

- **Aim/weapon-offset easing (this revision):** Shared numeric policy now splits
  six original easing coefficients over fractional raw intervals, preserving
  original 60-Hz normal/slow-motion response. Original-block Windows tests cover
  both paths, 60/120/144, pause, snap, uneven steps and unchanged catch-up. No
  platform adapter is needed. All 157 related Windows checks and the full Build
  pass. Actual Mac execution and physical/visual aiming
  acceptance remain pending; see the native regression record and STATUS.

- **Animation timeline coverage (this revision):** Shared test coverage executes
  the complete original timeline and event readers at original 60/native
  60/120/144, including normal/quarter speed, reverse, loops, clamping, pause
  and catch-up. All 32 sequences and five related fixtures pass on Windows.
  No runtime/adapter change or rebuild is required; actual Mac fixture execution
  and skeletal/asset/blend validation remain pending. See the animation timeline
  section of the native regression record for controlled service boundaries.

- **Native weapon sway (this revision):** Shared `pc_native_numeric.h` and
  `pc_native_visual_time_guest.inc` sample the final weapon motion value at
  fractional level time. Integer crossings, RNG calls and authored disabling
  remain original. Complete-dispatcher and original model-store fixtures cover
  60/120/144, pause, quarter speed and fallback. Full Windows Build passes;
  actual Mac compilation, model rendering and perceptual smoothness remain
  pending. No adapter is needed because the existing observed visual clock and
  numeric hook are shared. See STATUS and the native regression record.

- **Pause/input phase coverage (this revision):** Shared timing/input tests now
  combine fractional pause offsets with button edges and held-state delivery
  across 60/120/144. All 864 schedules and all 50 native timing checks pass on
  Windows. No runtime or adapter behavior changes. The expanded fixture is
  already in shared CI scope; actual Mac execution remains pending. See the
  button-delivery section of the native regression record for mocked boundaries.

- **Additional Windows pacing evidence (this revision):** A bounded no-capture
  What Lies Below run on `d336cd9` measures 143.825 fresh CPU updates/s across
  62.485569 active gameplay seconds, p99 8.0834 ms and no interval over 16.67 ms.
  No source/adapter changes accompany this measurement. These minimized Windows
  results do not validate Mac pacing or monitor output; equivalent Mac scene
  measurements and platform acceptance remain pending. See STATUS and its
  linked regression record for the selection, provenance and remaining limits.

- **FPS overlay beta label (this revision):** Shared `src/diagnostics/pc_debug.c` appends
  `(Beta)` to active 120/144 targets and preserves original/native 60 labels.
  Existing Windows overlay checks cover all four cases; the full supported
  Windows Build passes. Both hosts consume the same formatter, so no Mac
  adapter is needed. Actual Mac build/display validation remains pending.
  The broader What Lies Below movement/fire/reload/pause comparison recorded
  in STATUS is Windows evidence only, not Mac acceptance.

- **Player hit displacement/recovery (this revision):** Shared numeric policy
  normalizes both the hit-vector decay and its walking-position contribution
  to fractional game time. Updating decay alone would increase displacement.
  Expanded original-routine tests execute the hit initializer, active player
  dispatcher and actual three-channel position-add block, with collision/world
  boundaries explicitly mocked. Equal-time displacement/decay, quarter-speed,
  uneven steps, repeat hits, pause and original behavior are checked. No new
  Mac adapter is required; native Mac compilation and actual combat remain
  unverified. See the player hit section in the native regression record.

- **Scotland route diagnostics (this revision):** The Windows bounded runner
  adds read-only player health. Repeated original-60/native-120/native-144
  routes all take damage and move below the upper path under fire; original
  60 dies in the final firing sequence. This prevents treating that route
  failure alone as a native regression, but damage, collision, firing and jeep
  parity remain unverified. Five sessions exit normally with preserved
  profiles. No gameplay or Mac adapter changes; Mac execution remains pending.
  See the Scotland route under fire section of the native regression record.

- **Live aiming/movement diagnostics (this revision):** Windows bounded runner
  `tests/gameplay/stability_transitions_game.py` adds sequenced synthetic mouse packets
  and position/level-clock samples during short key holds. Scotland camera
  turns/reversal and pause-input discard match at original 60/native 120/144;
  the short shore path is visually checked. Separate no-capture, three-second
  holds give game-time speed estimates within 0.227% of original 60 on this
  route. Short asynchronous observations are not exact-physics certification;
  see the native regression record. No shared gameplay or host adapter is
  changed by this diagnostic;
  native Mac/physical-device/scanout validation remains pending.

- **HUD message timing (this revision):** Shared `src/gameplay/pc_native_numeric.h`
  opens the audited HUD message pause gate on fractional game intervals,
  preserving authored delays, expiry, queue ordering and sound holds. The
  original-routine fixture `tests/gameplay/native_hud_messages.c` reproduces a doubled
  notice duration at 120 before correction and passes at original 60 and
  native 60/120/144 afterward, including quarter-speed and pause. Shared CI
  already runs its parametrized timing suite. No Mac adapter is needed;
  native Mac build and live message/voice synchronization remain unverified.
  Windows Release build/built-in checks and 133 regressions pass. Rebuilt
  Scotland observations at original 60/native 120/native 144 verify about 1.0
  message second per wall second, objective fade/removal and pause/resume.
  These Windows results do not establish Mac validation; see the HUD message
  timing section in `docs/analysis/native-high-refresh-regressions.md`.

- **Scotland opening follow-up (this revision):** Windows original-60/native-144
  comparison verifies intro-to-dock transition, three pistol rounds, short
  movement and pause/resume on the unchanged countdown-fix binary. This is
  opening-only evidence; jeep behavior, mission completion, pacing and Mac
  execution remain unverified. No shared source or OS adapter changed.
  See the Scotland opening comparison in the native regression record.

- **Pacing measurement follow-up (this revision):** The Windows bounded
  transition runner can disable renderer dumps and records that choice in
  provenance. On the unchanged countdown-fix binary, approximately 48-second
  Arcade samples average 119.958/143.936 CPU updates/s at 120/144, with 1.0
  game/wall time and p99 8.9563/7.9785 ms. These are scene CPU measurements,
  not full-game, scanout, physical latency or Mac validation. The existing
  Mac runner already makes capture optional; this Windows diagnostic option
  introduces no shared gameplay or host adapter change. See docs/project/STATUS.md.

- **Arcade countdown (this revision):** Shared `src/gameplay/pc_native_numeric.h` keeps
  the integer match-start counter on accumulated raw whole ticks at its one
  float-add site. Truncating fractional additions otherwise freezes native
  120/144 match start and suppresses controls. The complete original state
  dispatcher/mask-writer fixture passes at original 60 and native 60/120/144
  on Windows; 132 shared timing/regression checks pass. Full Windows build
  and live Vietnam match start/movement/fire/pause at 120/144 pass. Capture-heavy
  intervals still show pacing gaps. Source and test entry
  are shared with Mac; no additional OS adapter is required. Native Mac build,
  start/cue behavior and live control acceptance remain unverified. See the
  Arcade countdown diagnosis in `docs/analysis/native-high-refresh-regressions.md`.

- **Display menu / FPS preference (this revision):** Shared `pc_display_menu.c`
  groups Screen, Camera, Textures and Frame rate. `pc_frame_rate_settings.h`
  and the additive `native-frame-rate-settings` patch save original 60 or beta
  native 120/144 for the next startup. Resolution preview, FOV cancellation
  and live texture controls stay independent. The shared config library takes
  one startup snapshot so saving cannot change the live clock. No separate
  Mac menu implementation is required; Mac compilation, controller/mouse
  navigation, atomic POSIX save and restart acceptance remain unverified.
  Windows model/navigation/persistence, live menu layout and actual 144-to-120
  and 120-to-original-60 save/restart checks pass; see docs/project/STATUS.md. These do not
  establish Mac runtime support.
- **Status:** Awaiting validation
- **Behavior:** Opt-in `TSFP_NATIVE_FPS` / `native_fps` runs fresh title updates
  and rendering at a 60/120/144 target, disables interpolation, and retains the
  60 Hz console interrupt clock. Fractional motion deltas, integer-timer
  remainders, pause/resume and bounded catch-up share one implementation.
- **Source:** local development checkpoint on `feature/native-144-fps`: `pc_native_timing*.h`,
  `pc_native_timing_guest.inc`, `pc_native_clock.h`, `pc_native_rate.h`, `pc_native_numeric.h`,
  additive native timing/numeric/animation-scope/input/script-event/decay/camera patches and
  `tests/gameplay/test_native_timing.py`, including execution of the original US script
  and NPC/player dispatchers, animation-event readers, camera motion and
  camera-effect timer/decay/collision-call cadence and mode-27 track sampling.
  The original frontend backdrop motion and texture alternation also have
  fractional-time coverage. The entity post-update velocity guard at 0xA75E7
  uses live fractional game time, with the original routine and list dispatcher
  exercised by `tests/gameplay/native_entity_velocity.c`; collision services are stubbed.
  This shared numeric policy needs no second Mac implementation, but actual
  native execution remains pending. Usage is in
  `docs/analysis/native-high-refresh.md`.
- **Weapon sound helper:** Shared `pc_native_numeric.h` accepts live fractional
  time at the 0x1B6FF0 event guard. `tests/gameplay/native_weapon_sound.c` executes the
  original sound selector/repeat timer and both parameter builders, with a
  mocked audio backend and RNG. No platform audio adapter changes are needed
  for this guard; native Mac execution, audible output and enclosing weapon
  state-machine timing remain unverified. Windows build, 58 timing/regression
  checks, 17 fake-device audio checks and a bounded native gameplay smoke pass;
  see [STATUS](STATUS.md) and the weapon sound regression record for limits.
  The original deferred weapon cooldown branch also has shared 60/120/144,
  quarter-speed, pause and catch-up coverage in `native_weapon_cooldown.c`;
  its whole-tick subtraction already behaves correctly and is unchanged.
- **Generic firing event scope:** Shared `pc_native_weapon_guest.h` and the
  additive `native-weapon-event-scope.patch` keep the original projectile gate
  and scope event/sound timers to its carried tick span, restoring fractional
  time on return. The original-code fixture passes 96 configurations, nested
  sound timers, quarter-speed schedules, pause and catch-up; the former strict
  expected failure is removed. No host adapter is needed. Windows full build,
  built-in checks and the 120-second native-144 gameplay check pass. All 35
  patch transition/tracking rechecks pass after compatibility repairs;
  actual Mac compilation/runtime,
  real damage/ammo and other weapon callers remain unverified. See
  [the regression record](../analysis/native-high-refresh-regressions.md#generic-firing-event-tick-scope).
- **NPC firing event scope:** The additive `native-npc-weapon-event-scope.patch`
  reuses `pc_native_weapon_guest.h` for US routine 0x1BB5C0. The original-code
  fixture covers 96 configurations with original ammo and sound helper
  operations. No new host adapter is required. Source generation, 143 shared
  checks, five network dispatch/results checks and five targeted patch-transition
  tests pass. The timing scope composes with the existing bot-shot hook;
  Windows Release/built-in checks and the 120-second native-144 gameplay
  check now pass on `cfaef73`. A same-build repeat with COD absent passes
  again and averages 138.6 fresh CPU updates/s in the measured gameplay
  window versus 62.0 earlier; this is Windows evidence only.
  Actual Mac compilation/runtime remain pending. See
  [the NPC regression record](../analysis/native-high-refresh-regressions.md#npc-firing-event-tick-scope).
- **Object rocking integration:** `src/gameplay/pc_native_rocking_guest.h`, the shared
  numeric policy and additive `native-object-rocking` patch now integrate
  fractional motion while preserving original contact/impact rules. The
  original-code fixture advances all 144 transforms per second at normal and
  quarter speed, with exact original endpoints. Expanded checks retain the
  impact sequence and sound payloads across 96 combinations and cover
  recursive impulses, per-object isolation, reuse/reset and floor-contact
  pause. The prior cadence expected failure now passes; the shared suite
  passes 136 tests, and all 21 patch-upgrade checks pass. No OS adapter is
  required. The full supported Windows Release build and built-in checks
  pass on `dee6170`. Live scene acceptance and actual Mac compilation/runtime
  remain pending.
  The CRT asin/audio-device boundaries remain adapted fixture services,
  and entities outside the audited pool retain original motion. Source is
  committed as `dee6170` on `feature/native-144-fps`. See
  [the integration evidence](../analysis/native-high-refresh-regressions.md#native-object-rocking-integration).
- **Bridge-crash placement:** The crashed mothership's updater at 0x217050
  dropped fractional motion intervals behind its whole-tick entry gate.
  `pc_native_numeric.h` now corrects that one gate in shared source, leaving
  the separate damping loops unchanged. A complete original-code fixture
  reproduces the altered crash timing/landing and checks the correction at
  60/120/144 Hz, normal/quarter-speed, pause and catch-up. No OS adapter is
  needed. Uneven frame intervals and pauses in all crash phases also pass.
  The supported Windows Release build and built-in tests pass; bridge-scene
  replay and actual Mac execution remain pending. See
  [the investigation](../analysis/native-high-refresh-regressions.md#bridge-crash-placement-investigation).
- **Ship debris inheritance:** The shared gate at 0x216958 now divides the
  parent's displacement by positive fractional time even on zero-whole-tick
  frames. The complete original-code fixture checks velocity, both model
  branches, RNG/effect output, allocator failures, catch-up, zero time and
  ABI, with byte-identical original 60 FPS output. No OS adapter is needed.
  Windows Release/built-in tests and a 120-second isolated native-144 smoke
  check pass on `ef2ace3`; live debris and actual Mac execution remain pending. See the
  [debris check](../analysis/native-high-refresh-regressions.md#ship-debris-inherited-velocity).
- **Projectile travel:** Shared site 0xF4A66 now accepts every positive
  fractional game interval. The full original updater fixture checks travel,
  sweep/impact dispatch, expiry, pause, uneven intervals and native-60 identity.
  No platform adapter is needed. Scene geometry and damage application remain
  mocked; live plasma-sniper damage, other projectile families and actual Mac
  execution are unverified. Windows Release/built-in and 120-second isolated
  native-144 smoke checks pass on `ef2ace3`. See the
  [projectile record](../analysis/native-high-refresh-regressions.md#projectile-travel-on-fractional-frames).
- **Sniper damage:** Shared gate 0xBB6C0 now accepts a one-shot hit on any
  positive fractional game interval; pause remains blocked. The original
  sniper/actor damage chain reproduces dropped health changes and passes
  across native intervals, misses, target eligibility and rejection checks.
  No OS adapter is needed. Windows Release and the 120-second native-144
  smoke check pass on `fc3a7ce`; targeted live sniper and actual Mac execution
  remain unverified. See
  [the sniper record](../analysis/native-high-refresh-regressions.md#sniper-damage-on-fractional-frames).
- **NPC animation-marker shots:** Shared caller-specific transaction handling
  in `pc_native_weapon_guest.h` preserves one-time shot events at return sites
  0x27AE66/0x2B9BF8. The additive manual-wrapper patch composes with the existing
  network boundary. Original selector/caller/firing fixtures pass on Windows;
  continuous-fire scenarios retain their old cadence. No OS adapter change.
  Windows Release and a 120-second native-144 smoke check pass on `fc3a7ce`;
  targeted live AI confirmation and actual Mac execution remain pending.
  See [event evidence](../analysis/native-high-refresh-regressions.md#npc-animation-marker-shots).
- **NPC aim recovery/tracking:** Shared numeric policy advances the 0x2B93DA
  gate, normalizes only the 5% argument at 0x2B963F, and preserves whole-tick
  RNG decisions at 0x2B9478. New generic identity-default immediate/callee
  hooks are supplied through the shared patch stack. Original branch and
  helper fixtures pass across 60/120/144, quarter speed, rare decisions,
  pause and uneven/catch-up intervals. Full Windows Release and the opening
  native-144 smoke check pass on `dee6170`; targeted live AI and actual Mac
  execution remain pending. No OS adapter
  change is needed. [Aim evidence](../analysis/native-high-refresh-regressions.md#npc-aim-recovery-and-tracking).
- **Beam lifetime audit:** `tests/gameplay/native_beam_lifetime.c` executes the original
  shared effect manager with original switch/type tables. First-update,
  lifetime, explicit delay and paused-age checks pass on Windows at
  60/120/144 and quarter speed. Visual geometry/rendering are mocked; delayed
  trails remain open. No runtime or OS adapter change. Actual Mac execution
  remains pending. See [beam evidence](../analysis/native-high-refresh-regressions.md#beam-manager-lifetime-audit).
- **Beam geometry audit:** `tests/gameplay/native_beam_geometry.c` executes the shared
  original constructor, updater, renderer and manager through vertex writes,
  with controlled allocation, random values, trig table and rendering services.
  Windows checks cover immediate vertex-alpha onset, fade, pause and retirement
  at 60/120/144 and quarter speed; original/native 60 final geometry is identical.
  No runtime or OS adapter change is required. Actual Mac execution, scene
  visibility and GPU submission remain unverified; the user's delayed trails
  remain open. [Geometry evidence](../analysis/native-high-refresh-regressions.md#beam-construction-and-vertex-output-audit).
- **Effect visibility and tracing:** The geometry fixture now executes the
  original type-32 render-list branch and manager room visibility. Windows
  checks cover fractional eligibility, hidden/view-mask rejection, deferred
  starts and retired-list suppression. The portable read-only
  `tools/analysis/native_effect_trace.py` accepts an injected memory reader; the existing
  Windows runner exposes it through `--trace-effects`. No game or OS adapter
  changed. Actual Mac execution, upstream scene visibility inputs and GPU
  submission remain unverified. [Dispatch evidence](../analysis/native-high-refresh-regressions.md#effect-visibility-and-reproduction-tracing).
- **Kind-35 projectile correction (local source):**
  `src/gameplay/pc_native_projectile_guest.h`, the numeric call-site policy and the
  additive `-native-kind35-projectile-timing.patch` apply through the same
  manual wrappers on Windows and Mac. No OS adapter is required. Fractional
  travel/steering/trail anchoring now pass the original-code fixture while
  preserving whole-update random/target cadence. Tests cover impact/expiry
  dispatch with controlled services, pause, uneven intervals, missing
  emitters, recycled slots and nested scope isolation. Full Windows Release
  and the opening native-144 smoke check pass on `dee6170`. Live scene
  verification is pending; actual Mac generation/build/gameplay
  remains unverified. Other projectile kinds and damage falloff are open.
  [Correction evidence](../analysis/native-high-refresh-regressions.md#kind-35-projectile-motion-and-trail-correction).
- **Projectile damage-dispatch audit (local tests):** The same portable
  original-code fixtures now execute 0xDCEF0/0xB9920/0xB9720 for captured
  kind/profile combinations and include kind 46. Collision-record multipliers
  stay correct across 60/120/144 and slow motion for these paths. Non-flat
  profile controls preserve explicit coverage limits; actual target health,
  other legal combinations and Mac execution remain unverified. No platform
  adapter or gameplay policy changed in this audit.
  [Damage-dispatch evidence](../analysis/native-high-refresh-regressions.md#projectile-distance-accumulation-and-damage-dispatch).
- **Shared performance work:** `pc_prompt_guest.inc` reuses a call's validated
  stack span for stack-local text fragments, avoiding duplicate VM queries.
  Mapping checks remain fresh for each call and at protection boundaries.
  Windows real-VM boundary tests are in `tests/input/pc_prompts.c`; native Mac
  stack-fragment cases were added to `src/native/prompt_selftest.inc` but
  require an actual host run. This source change applies to both fixed-60 and
  native-refresh paths; it does not alter text formatting or wrapping.
  The follow-up in `pc_guest_memory.h` exposes the queried region and its write
  permission, so nearby text and scratch within that same region need no
  duplicate query. No snapshot survives a guest call. Windows real-VM tests
  cover same-page reuse, different/protected pages and changed permissions;
  `src/native/prompt_selftest.inc` adds read-only scratch and boundary cases.
  Actual Mac execution is pending; the common Mach query already supplies the
  region bounds and protection, so no new platform adapter is required.
  Source `6fc4d87` passes the full Windows Release build and native-144/original-60
  two-minute gameplay/pause checks. Reduced local query counts are verified;
  overall FPS gains and stable 144 FPS are not established.
  The separate GPU-flush experiment `aef8d29` failed live Windows progress
  and was reverted by `eae18a3`; no GPU acknowledgment change remains.
- **MapMaker random light preview (`8893ba8`, local):**
  `src/gameplay/pc_native_light_guest.h` and the additive `*-native-light-preview.patch`
  retain the guest-owned color during zero-whole-tick native intervals and
  pause. Original code otherwise wrote the dark palette endpoint on those
  frames. The shared original-code fixture checks exact packed/RGB-table
  outputs, discrete deadlines, RNG and ABI at 60/120/144 and quarter speed.
  It spies on model-color propagation; actual preview rendering is unverified.
  No platform adapter or host cache is added. The full Windows build, 138
  shared checks, 21 patch-upgrade checks and two-minute native-144 Story smoke
  pass. Actual Mac compilation and live MapMaker visual acceptance remain pending.
- **Movie polling pacing (`3c2fcc2`, local):**
  `src/gameplay/pc_native_movie_pacing.h` and the additive `*-native-movie-pacing.patch`
  bound the separate host-video loop at the selected native rate while keeping
  movie timestamps/audio and guest clocks unchanged. The helper uses shared
  clock and platform waits; existing Mac host movie playback is still absent
  (MAC-009). Model and actual update-body tests pass with synthetic backends;
  the full Windows build/built-in checks and real-file decode/hash comparison
  pass. Actual GPU/audio-device movie playback and Mac execution remain pending.
- **Raw mouse packet cadence (local source):** `src/input/pc_input.c` resets the
  consumed buffer's age so the next packet batch does not expire against a
  previous batch's timestamp. Windows and SDL adapters already use this shared
  helper; no additional Mac adapter is needed. `tests/gameplay/native_mouse_cadence.c`
  reproduces lost counts and validates 60/120/144 update rates, 125–8,000 Hz
  packets, uneven intervals, zoom/inversion and stale-input/focus/capture
  transitions. All 153 shared checks, seven input context checks, the full
  Windows build/built-in checks and a 120-second native-144 gameplay replay
  pass for `149bef9`. Actual Mac execution and physical-device latency remain
  open. Terminal-pointer timing is addressed by the separate correction below;
  live pointer acceptance remains pending.
  [Mouse cadence evidence](../analysis/native-high-refresh-regressions.md#raw-mouse-packet-cadence).
- **Terminal fractional pointer cadence (local source):** Shared
  `pc_input_consume_motion` and `pc_terminal_mouse` retain fresh packet batches
  and use positive finite fractional game time in native mode. Original mode
  keeps its whole-tick gate. Windows `pc_controls.c` and SDL `input_sdl.c`
  supply the same native-rate selection. The shared terminal fixture covers
  42 timing cases plus pause/ownership/return checks. Both adapters gain a
  synthetic integration check. All 161 shared/regression checks, the Windows
  build and its built-in adapter checks pass for `c4171db`; actual Mac
  execution and live terminal-scene acceptance remain pending. No separate
  Mac policy is required.
  [Terminal evidence](../analysis/native-high-refresh-regressions.md#terminal-mouse-fractional-cadence).
- **Arcade acceptance failure (Windows `c4171db`):** Vietnam Deathmatch renders
  and accepts pause/resume at native 144, but player movement/fire remain
  unresponsive despite advancing native game time. Original-60 mode on the
  same binary shows combat and ammunition consumption. Shared/native Arcade
  timing is not accepted; root cause is still under investigation. Mac
  execution is unverified, not inferred from the Windows failure.
  [Live comparison](../analysis/native-high-refresh-regressions.md#arcade-vietnam-comparison).
- **Latest Windows integration comparison (`3c2fcc2`, local):** Matching
  original-60/native-144 opening replays pass handoff, hand animation, movement,
  firing and pause/resume. Native CPU intervals average 140.399 updates/s at
  1x game speed with another game session open. No production or host-adapter
  change follows from these checks. Mac execution, later levels/modes and
  physical input/display latency remain unverified.
  [Comparison evidence](../analysis/native-high-refresh-regressions.md#latest-build-opening-comparison).
- **Fractional strip-distortion clock (`a08f41a`, local):**
  `src/gameplay/pc_native_numeric.h` extends the existing shared visual-clock sampling
  to the continuous strip effect at `0xF9B4B`. The original-code fixture in
  `tests/gameplay/native_strip_distortion.c` checks draw arguments, original whole-tick
  parity, native cadence, pause, rebases and view/range isolation. No new clock,
  patch or platform adapter is introduced. Actual Mac execution and live
  strip-effect rendering remain unverified. All 148 shared checks and the full
  Windows build/built-in checks pass; the existing interactive session was
  preserved on its preceding build, so it is not live evidence for this change.
- **Procedural-texture cache cadence (`b3c30cf`, local):**
  `src/gameplay/pc_native_numeric.h` keys the surface caller's two cache reads by native
  update ID instead of console vblank, and prevents the procedural dispatcher
  from updating on zero-float pause with a stale positive whole count.
  `tests/gameplay/native_surface_cache.c` executes the complete original caller and
  dispatcher with graphics/pass spies, covering phase offsets, resource aliases,
  view slots, UV motion, pause/resume and catch-up. The existing shared load
  policy supplies all hooks, so no new patch or platform adapter is required.
  The Windows build and its built-in checks pass. Extended original allocator/
  initializer checks pass new/reused/recycled resource stamps, pool exhaustion
  and four-view deduplication; GPU binding/clearing remain spies in that fixture.
  Actual Mac execution and live procedural-texture visuals remain unverified.
- **Fractional screen-distortion clock (`ae410c4`, local):**
  `src/gameplay/pc_native_visual_time.h`, `pc_native_visual_time_guest.inc`, the numeric
  policy and additive `*-native-visual-time.patch` observe direct level-clock
  stores without changing guest integers. Continuous increments accumulate
  live fractional time; explicit overrides rebase even for same-value writes.
  Only the audited distortion read at `0xF9726` samples the new time. This is
  shared title logic with no new platform adapter. All 146 shared checks pass
  on Windows; all nine original direct store instructions and the complete
  original distortion mesh/draw routine are exercised. The 21 patch upgrades,
  full Windows build and two-minute native-144 gameplay/pause replay pass.
  The original-60 replay was closed before completion and remains incomplete.
  Actual Mac builds, live effects and complete level/replay/network lifecycle
  checks are pending.
- **Screen-distortion absolute-time audit (`4ed4779`, local):**
  `tests/gameplay/native_screen_distortion.c` executes the complete original mesh/draw
  routine, original grid-size selector and sine-table initialization/helper.
  Graphics services are spies; controlled grid/viewport inputs are explicit.
  The shared native timing test measures whole-clock repetition at 60/120/144
  and quarter speed, compares a test-only fractional clock probe, checks exact
  original tick-boundary outputs and samples resets, pause and two player views.
  No production clock policy or platform adapter changes here. Actual Mac
  execution, clock lifecycle integration and live GPU visuals remain pending.
- **Offline network-hook overhead:** `pc_network_bots_guest.inc` and
  `pc_network_bots_lobby.inc` reject ineligible guest modes/actors before reading
  host network settings. Eligible calls still read live settings; no transport
  or environment cache is introduced. Windows guest-dispatch and original
  result-builder checks cover this ordering. The existing Mac fallback is
  unchanged and covered by the portable ABI test; actual Mac play and the
  queued MAC-013 transport remain unverified.
- **Existing Mac support:** The guest hooks, update clock and numeric conversion
  policy are compiled by the shared title build. The Mac entry point reports
  the opt-in native rate; normal play retains main's fixed-60 default. No second
  timing model or higher-frequency console clock was introduced.
- **Overlay follow-up:** Shared `src/diagnostics/pc_debug.c` reads the native rate for its
  target label and hides offline RTT. `0770-native-debug-target.patch`
  links the shared overlay to runtime configuration on both hosts. Actual Mac
  compilation and overlay display remain awaiting validation.
- **Remaining adaptation:** Validate compilation, waitable-timer behavior,
  common Vulkan and legacy GL rendering on actual Apple Silicon. Integrate a
  native launcher selector after game-speed and pacing acceptance; the normal
  launcher currently offers the fixed-60 path.
- **Acceptance:** Run portable timing/numeric/ABI tests, then isolated Story,
  Arcade, Challenge, split-screen, vehicle, cutscene/movie, recorded-demo and
  network sessions at 60/120/144. Compare actual movement, animation, weapon
  cadence, pause, saves and clock speed; measure update/present intervals
  separately from monitor scanout and perceived smoothness. Do not claim
  full-game or native Mac validation from the Windows first-mission run.
- **PC evidence:** [Native high refresh development](STATUS.md#native-high-refresh-development-2026-10-20).

**Rigid/rigid impulse acceptance extension (2026-10-25):**
On branch `test/native-vehicle-contact-acceptance`,
`tests/gameplay/native_rigid_motion.c` and `tests/gameplay/test_native_vehicle_timing.py` now
exercise original1667C0/166060 point velocity and16FC00 two-body response against
an independent generalized impulse calculation. The 432 cases include distinct
masses, spin, 3D lever arms/normals and rotated inverse-inertia tensors at all
five diagnostic rates plus an additional secondary-step duration. Damage/effect
notifications remain a checked boundary; scheduling and live geometry are not
provided by this extension. Existing both-host workflow registration includes
the fixture, but its private supported-US-XBE requirement still skips these
original routines on asset-free CI. No production behavior or OS adapter changes.
Actual Mac execution/live handling remain Awaiting validation; see STATUS.

**Forced/damped material motion follow-up (2026-10-25):**
On `fix/native-material-motion` (source d2ae41d6, main integration65058c27),
Shared `src/gameplay/pc_native_material.h` and the five audited127500 numeric sites in
`pc_native_numeric.h` split velocity, held forcing and UV travel together.
Original mode and unsupported decay bases retain the original path; no state
layout, resource owner or OS adapter changes. Asset-free partition tests and the
original surface/cache fixture are selected by the existing both-host native
timing suite. Private-XBE skips still do not prove Mac execution. Windows
build/gameplay evidence belongs in STATUS; physical Mac and actual changing
force-table/rendered-material acceptance remain Awaiting validation.

**Coherent character-growth observation (2026-10-25, PR #361 WIP):**
`pc_native_character_trace.h` / `pc_native_character_trace_guest.inc` optionally
observe the original126029->120AB0 matrix update. Before/after reads use the
existing Windows/Darwin mapped-memory adapter, resetting its cache across guest
execution. Default, failed and capped traces retain the original callee. This
adds no gameplay policy or OS adapter. Portable collector/writer and enabled/
failed-open original-routine checks join the existing both-host timing suite;
private-XBE skips are not Mac execution. Windows build/live evidence is in
STATUS. Physical Mac and rendered-effect parity remain Awaiting validation.

### MAC-059 — Physical keyboard entry in original text dialogs (issue #136)

- **Status:** Awaiting validation
- **2026-10-24 follow-up (this revision on `fix/native-water-rounding`):** The shared
  keyboard-row scanner reuses an OS-validated writable region within one traversal,
  expiring it before any guest dispatch. Windows uses VirtualQuery and native Mac
  uses the existing Mach VM adapter. No input-model/OS-event adaptation is needed.
  Shared scan and original text-edit tests pass on Windows; Mac execution and
  menu/gameplay pacing remain unverified. See the crowded-menu STATUS record.
- **PC behavior:** Physical letters, numbers, supported Latin-1 accents and spaces
  feed the original on-screen keyboard insertion routine. Backspace/Delete,
  Left/Right, Home/End, Ctrl+A, Enter and Escape edit/accept/cancel. Original
  selection replacement, length/width limits and save callbacks remain in charge.
  Typed E/F/Y do not trigger the normal menu shortcuts. Mouse/controller input
  remains available. Numeric-only dialogs, arbitrary Unicode and paste are outside
  this change.
- **PC source:** `136-enhancement-write-better-with-a-real-keyboard`,
  based on main `c932bee`; `src/input/pc_text_input.h`, `src/interface/pc_menu_text.inc`,
  `src/interface/pc_menu_mouse_guest.inc`, `src/input/pc_controls.c`; shared behavior tests in
  `tests/graphics/test_menu_text.py` and Windows adapter coverage in `tests/input/pc_controls.c`.
- **PC validation:** Windows x64 Release build and build tests pass. 77 selected
  pytest checks pass, one host-specific skip. Original checksum-verified US text
  insertion/edit routines pass replacement, cursor, maximum length and font-width
  rejection tests. Direct Windows message-handler tests pass. The initial live
  test exposed an embedded profile keyboard missed by descriptor-only detection;
  the shared code now recognizes the original character rows across descriptors.
  On 2026-10-21 the user confirmed physical typing and Enter work in Create
  Profile. Other dialogs and Mac physical input still need validation.
- **Existing Mac support:** Same queue and guest callbacks; `src/native/input_sdl.c`
  consumes SDL text events under the existing input mutex. Main-thread frame work
  enables/disables SDL text input; focus loss clears queued text. Added native
  input self-test coverage in `src/native/menu_input_selftest.inc`.
- **Mac adaptation needed:** None identified; compile and host validation pending.
  Cocoa composition/physical keyboard delivery is not established by Windows tests.
- **Mac acceptance checks:** Run native input self-tests and shared menu tests;
  create/edit a disposable profile and MapMaker name, including E/F/Y, Shift/Caps,
  supported accents, selection replacement, Backspace/Delete, Enter/Escape,
  blank/overlong input and focus loss. Verify mouse/controller on-screen keys and
  normal gameplay/menu bindings after closing. Check that unsupported Unicode
  is ignored rather than corrupting saved text.
- **Mac implementation and evidence:** Adapter is included on the source branch;
  no Apple Silicon host available. See the issue #136 entry in `docs/project/STATUS.md` for local evidence.

### MAC-058 — Crane and remote-view vignette sizing (issue #221)

- **Status:** Awaiting validation
- **PC behavior:** The Scotland the Brave crane's four mirrored `holoedge`
  texture pieces scale together to the output viewport, removing the central
  4:3 boundary. Control text remains outside the stretching scope. The same
  original vignette helper is used by remote-camera/gun and microscope views.
  Its inclusive full-canvas clip is normalized through shared `pc_ui_vignette_clip`
  in both RHI and legacy GL; the guest clip and unrelated widgets stay unchanged.
- **PC source:** `221-bug-scotland-the-brave-crane-widescreen-issue` (uncommitted);
  additive `*-crane-vignette.patch`, existing shared
  `src/interface/pc_display_ui.inc` placement helper and `tests/graphics/test_crane_overlay.py` /
  `tests/graphics/crane_overlay.c`. No renderer-specific shader or OS policy is added.
- **PC validation:** See `docs/project/STATUS.md`, “Crane vignette widescreen (#221)”.
  Asset-free scope checks and original US guest-routine execution pass on
  Windows x64. The user confirmed the corrected crane overlay looks good.
  Desert encounter and Mac scene checks remain pending.
- **Existing Mac support:** The wrapper uses `TSFP_NATIVE_DISPLAY` and the
  existing shared placement API. Its native-display preprocessor path was
  exercised by the Windows C fixture; this is not native Mac validation.
- **Mac adaptation needed:** None identified. Validate the existing common
  renderer and legacy GL placement implementations on their respective hosts.
- **Mac acceptance checks:** At 4:3, 16:9 and ultrawide, enter the Scotland crane;
  verify the dark edge overlay has no vertical 4:3 seams, retains transparency,
  and leaves control labels proportional. Leave the crane and verify HUD
  placement restores. Check remote views and split-screen viewport boundaries.
  The separate desert shattered-glass report is not yet reproduced or claimed
  fixed by this vignette change.

### MAC-061 — Host save-drive metadata and inline drive selection

- **Status:** Awaiting validation
- **PC behavior:** Create Profile lists mounted drives directly beneath the
  current save drive, with plain drive names/letters and actual space information.
  A different writable drive becomes the save location immediately; existing
  profiles remain on their original drive.
- **PC source:** Uncommitted on feature/keyboard-weapon-slots;
  src/pc_storage*.inc/.h, src/interface/pc_display_menu.c, src/input/pc_prompts.c,
  host-save-storage.patch, live-profile-storage.patch and native/main.c.
- **PC validation:** See docs/project/STATUS.md host-storage entry. Shared callback and host
  persistence fixtures run with isolated folders. Full multi-drive save/load
  playthrough is not claimed.
- **Existing Mac support:** Shared menu/routing policy plus getmntinfo, statfs,
  getattrlist and executable-path adapters; atomic POSIX preference writes.
  Native startup resolves the same preference before guest access.
- **Mac adaptation needed:** None identified; native compilation and mounted
  external-volume UI/persistence validation remain pending.
- **Mac acceptance checks:** Run storage, FOV and isolated-profile tests. Check
  startup and external volumes, plain drive labels, capacity display,
  unavailable/read-only media, immediate profile creation, restart persistence and profile
  preservation. Use a disposable save location; never redirect an automated
  Test Lab session onto personal saves.

### MAC-063 - Projected enemy health bars (issue #102)

- **Status:** Awaiting validation
- **PC behavior:** The Electro Chimp Discomatic health bars follow moving
  monkeys at widescreen resolution instead of drifting toward the center.
- **Source:** `102-electro-chimp-health-bars` and
  `patches/graphics/interface/0720-world-health-bars.patch` in the shared D3D compatibility path.
- **Windows evidence:** Release build, display test, patch-stack validation,
  focused patch-transition test, and isolated 1280×720 challenge replay passed.
- **Mac adaptation needed:** Confirm that the native renderer uses the same
  projected-draw placement. Apply equivalent placement only if its path differs.
- **Mac acceptance:** Build on a Mac and inspect the challenge at 16:9 and
  ultrawide ratios; check that health bars track the monkeys and ordinary HUD
  elements remain in place. Native Mac validation is pending.

## Shared uncertainties, not confirmed Mac-only omissions

Campaign completion, checkpoint/save-load round trips, physical-controller play,
multiplayer/online, vehicle/MapMaker controls and exhaustive audio/rendering
fidelity lack complete evidence across the project. They should not be described
as proven PC features missing only on Mac. Track focused validation in `docs/project/STATUS.md`.
The reported world disappearance remains unresolved until reproduced and checked;
none of the renderer entries above is a confirmed diagnosis of that symptom.

### MAC-062 - Stable and named game release channels

- **Status:** Awaiting validation
- **PC behavior:** Launcher 1.3.0 chooses Stable or a named opt-in game preview,
  keeps launcher updates independent, and confirms return to a lower Stable game.
  VERSION 0.3.2 prepares the next Stable game. Local changes on release-channels
  are uncommitted; no release has been published.
- **Source files:** tools/launcher/ReleaseClient.cs (GameChannels), Launcher.cs,
  SelfUpdate.cs, WhatsNewPage.cs, macos/NativeWindow.cs and NativeNews.cs;
  tools/release/release_channels.py, publish-release.py and both game packagers.
- **Existing Mac support:** Shared models, installer and generated layout already
  serve both hosts. Channel policy and shared behavioral fixtures are common.
- **Beta naming follow-up:** Preview's general testing track is Beta (-beta.N).
  Shared source migrates old general-preview preferences to beta, retains feature
  tracks, and leaves historical alpha/preview releases in Game versions only.
  Shared UI fixtures cover this policy on WPF/Avalonia; refreshed Mac CI is needed.
- **Preview grouping follow-up:** Shared GameChannelPickerPage now supplies
  Stable/Preview and the conditional testing-track selector on both hosts.
  game_preview_track persists the remembered feature across Stable. The generated
  layout hosts this common component; native host validation remains pending.
- **Mac adaptation needed:** NativeWindow maps the shared choices/checks to
  Avalonia; NativeNews supplies the named game channel. No separate policy or
  runtime implementation is needed. Native compilation/lifecycle remains unrun
  on this Windows host.
- **Mac acceptance checks:** Run dual-host launcher CI and native prerequisite/
  package/lifecycle checks; exercise old Preview migration, named-channel
  discovery, missing-channel retention, return to Stable confirmation, exact
  version installation, independent launcher updates and preserved personal
  data. Channel switches continue using existing saves: back up before testing.
- **Mac implementation and evidence:** Shared C# fixtures plus Windows host
  results are recorded in [docs/project/STATUS.md](STATUS.md#release-channel-strategy-2026-10-22).
  Native execution/visual acceptance is pending; Windows is not Mac evidence.

### MAC-064 - Self-contained runtime source

- **Status:** Awaiting validation
- **PC behavior:** Setup reconstructs the pinned runtime from bundled source,
  without downloading the original runtime repository. Existing checkouts and
  patch ledgers are preserved; third-party dependency downloads keep their pins.
- **PC source:** `f57875a5`, [PR #369](https://github.com/TheDoffman/timesplitters-future-perfect-pc/pull/369);
  `vendor/ts3FPrecomp`, `tools/build/prepare-runtime.py`, `tsfp.ps1`, shared CI and packaging.
- **PC validation:** See [STATUS](STATUS.md#self-contained-runtime-source-2026-10-26).
- **Existing Mac support:** `tools/build/build-native-mac.py --setup` uses the same
  Python importer, original commit, source bytes and complete shared patch stack.
  Mac release source ZIPs include the tracked snapshot.
- **Mac adaptation needed:** No separate implementation; run native host checks.
- **Mac acceptance checks:** Run snapshot/patch tests and native setup/build on
  Apple Silicon, verify executable modes and direct SDL/XbSymbolDatabase pins,
  inspect package source ZIPs and confirm existing local files survive setup.
- **Mac implementation and evidence:** At `f57875a5`, macOS CI passed snapshot
  and historical patch upgrades, runtime regressions and asset-free GPU contracts.
  Native Mac game build/package execution with private data remains unverified.

### MAC-065 - Independent game-disc import

- **Status:** Awaiting validation
- **PC behavior:** Import the supported game image without the external toolkit;
  reuse identical files and stop on conflicts without replacing user data.
- **PC source:** Uncommitted on `feature/independent-game-import`;
  `tools/compiler/disc_image.py`, `tools/build/import-game.py`, `tsfp.ps1`, `tests/compiler/test_disc_image.py`.
- **PC validation:** 32 focused importer tests passed, including a temporary image
  carrying the verified private XBE and a PowerShell project without external or
  venv directories. Full original ISO is unavailable. See
  [STATUS](STATUS.md#independent-disc-import-2026-10-26).
- **Existing Mac support:** Same stdlib implementation and Python CLI; no separate
  Mac parsing/extraction logic. Standard APFS hard-link publication is required.
- **Mac adaptation needed:** No additional adapter; validate native filesystem
  behavior and a supported original ISO. Game/runtime generation is unaffected.
- **Mac acceptance checks:** Run public fixtures on macOS and verify/import an
  available original ISO once, preserving the authoritative extraction; check
  case aliases, redirected paths, interrupted import and identical-file reuse.
- **Mac implementation and evidence:** Shared tests registered in both-host CI;
  local execution and full retail-disc import remain unverified on Mac.

### MAC-066 - Independent executable analysis

- **Status:** Awaiting validation
- **PC behavior:** Generate from the project-owned XBE reader instead of the
  upstream parser, keeping section/import/TLS metadata compatible.
- **PC source:** PR #370, continuing `feature/independent-game-import`;
  `tools/compiler/xbe_image.py`, `tools/compiler/xbe_kernel_exports.def`,
  `patches/compiler/0210-independent-xbe-reader.patch`, both build drivers and fingerprints.
- **PC validation:** Synthetic parser fixtures and exact private-US analysis
  comparison passed; full build/game evidence is recorded in docs/project/STATUS.md.
- **Existing Mac support:** The same stdlib reader, ABI data and bridge run on
  both hosts. The Mac driver sets the source root when reusing ignored deps.
- **Mac adaptation needed:** None identified; native validation remains pending.
- **Mac acceptance checks:** Run parser and generation tests, generate the
  verified US executable with `--generate`, build, and run isolated Story
  movement/fire/pause/resume. Also exercise `--runtime-root` with shared deps.
- **Mac implementation and evidence:** Shared source and both-host CI test
  registration; Windows evidence does not establish native Mac gameplay.

### MAC-067 - Independent disassembler input

- **Status:** Awaiting validation
- **PC behavior:** Production disassembly uses a project-owned executable view
  and instruction model/classifier, retaining pinned Capstone. Exact reads and
  probe windows respect section boundaries; input changes invalidate caches.
- **PC source:** Continuing PR #370 on `feature/independent-game-import`;
  `tools/compiler/recomp_image.py`, `tools/compiler/x86_decode.py`,
  `patches/compiler/0070-independent-disasm-input.patch`, `tools/build/recomp-generation.py`.
- **PC validation:** Original synthetic mappings/operand fixtures and the
  patched consumer's disassembler tests pass. Full-title classification matches
  1,403,940 decoded instructions. Build/game evidence is recorded in docs/project/STATUS.md.
- **Existing Mac support:** Same Python modules and pinned Capstone; uses the
  existing explicit source-root bridge for shared dependency checkouts.
- **Mac adaptation needed:** None identified; native validation pending.
- **Mac acceptance checks:** Run the shared input/consumer fixtures, regenerate
  the verified US game with `--generate`, rebuild, then test isolated Story
  movement/fire/pause/resume. Confirm source edits invalidate cached analysis.
- **Mac implementation and evidence:** Tests registered for both CI hosts;
  Windows results do not establish native Mac generation or gameplay.

### MAC-068 - Independent instruction traversal

- **Status:** Awaiting validation
- **PC behavior:** Production disassembly inherits project-owned linear scan,
  bounded realignment, cached direct-edge reachability and instruction queries.
  Table/prologue recovery is now covered by MAC-069; function discovery remains upstream.
- **PC source:** Active independent-replacement goal, continuing PR #370;
  `tools/compiler/x86_flow.py`, `patches/compiler/0080-independent-disasm-traversal.patch`, cache and
  generation fingerprints, `tests/compiler/test_x86_flow.py`.
- **PC validation:** Synthetic cycles, calls, branches, invalid bytes, overlap,
  convergence, zero targets and instruction-limit tests; production method-origin
  assertions and existing consumer tests. Build/game results are in docs/project/STATUS.md.
- **Existing Mac support:** Shared Python and pinned Capstone; no host-specific
  traversal path. Existing source-root bridge also handles shared dependencies.
- **Mac adaptation needed:** None identified; native acceptance remains pending.
- **Mac acceptance checks:** Run flow/input/fingerprint tests, regenerate the
  verified US game, rebuild and run isolated Story movement/fire/pause/resume.
- **Mac implementation and evidence:** Both-host CI registration; Windows
  validation does not establish native Mac gameplay.

### MAC-069 - Independent table and body recovery

- **Status:** Awaiting validation
- **PC behavior:** Production disassembly uses project-owned table measurement,
  case realignment, block boundaries and read-only body/prologue probes. Function
  detection and translation remain upstream.
- **PC source:** Continuing PR #370 on `feature/independent-game-import`;
  `tools/compiler/x86_recovery.py`, `patches/compiler/0090-independent-disasm-z-recovery.patch`, source
  fingerprints and `tests/compiler/test_x86_recovery.py`.
- **PC validation:** Synthetic table ownership/alias/bounds and probe-shape
  fixtures plus existing consumer tests. Build/game evidence is in docs/project/STATUS.md.
- **Existing Mac support:** Shared Python and pinned Capstone; the source-root
  bridge handles the same module in shared dependency checkouts.
- **Mac adaptation needed:** None identified; native acceptance pending.
- **Mac acceptance checks:** Run recovery/input/fingerprint tests, regenerate
  the verified US image, rebuild and validate isolated Story movement, firing,
  pause/resume. Verify cache invalidation and preserve the canonical fixture.
- **Mac implementation and evidence:** Tests registered in both-host CI;
  Windows validation does not establish native Mac generation or gameplay.

### MAC-070 - Independent function discovery and metadata

- **Status:** Awaiting validation
- **PC behavior:** Production analysis uses project-owned candidate/body/tail
  discovery, labels, strings and indexed references. Shared tails keep their
  owner's extent; overlong ASCII runs no longer create suffix labels.
- **PC source:** Continuing PR #370; `tools/compiler/x86_functions.py`,
  `tools/compiler/recomp_symbols.py`, `patches/compiler/0100-independent-functions.patch`, both source
  fingerprints and `tests/compiler/test_x86_functions.py` / `test_recomp_symbols.py`.
- **PC validation:** Original miniature programs and metadata edge cases plus
  existing consumer tests. Semantic function/reference comparison and full
  generation/build/gameplay evidence are recorded in docs/project/STATUS.md.
- **Existing Mac support:** Shared Python and permitted pinned Capstone; same
  source-root bridge, no host-specific analysis algorithm.
- **Mac adaptation needed:** None identified; native acceptance pending.
- **Mac acceptance checks:** Run shared function/symbol/input/fingerprint tests;
  regenerate the verified US image and compare callable ranges; rebuild and run
  isolated Story movement/fire/pause/resume while preserving the canonical fixture.
- **Mac implementation and evidence:** Both-host CI registration; Windows
  results do not certify native Mac generation or gameplay.

### MAC-071 - Independent analysis runner and publication

- **Status:** Awaiting validation
- **PC behavior:** Project-owned configuration, CLI, seed/section selection,
  analysis sequencing, JSON/assembly output and content-verified cache. The
  standalone analyzer needs no toolkit checkout; full generation still does.
- **PC source:** Continuing PR #370; `tools/compiler/analysis_runner.py`,
  `analysis_config.py`, `analysis_output.py`, `analysis_cache.py`,
  `patches/compiler/0110-independent-pipeline.patch` and both source fingerprints.
- **PC validation:** Original runner/cache fixtures and existing consumer checks;
  generation/build and bounded isolated game evidence in docs/project/STATUS.md.
- **Existing Mac support:** Shared Python/Capstone with the same source bridge;
  pathlib, temporary-file publication and no Windows-only analysis behavior.
- **Mac adaptation needed:** None identified; native acceptance remains pending.
- **Mac acceptance checks:** Run runner/cache/input/fingerprint tests; regenerate
  verified US metadata/code with `--generate`, build and run isolated Story
  movement/fire/pause/resume. Confirm cache reuse and changed-output rejection.
- **Mac implementation and evidence:** Both-host CI registration; Windows
  verification does not establish native Mac generation or gameplay.

### MAC-072 - Independent symbol adapter and pipeline driver

- **Status:** Awaiting validation
- **PC behavior:** Production symbol scanning and generation use project-owned
  adapters/orchestration; parsing and disassembly call project scripts directly.
  The separately licensed XbSymbolDatabase library still supplies signatures.
- **PC source:** Continuing PR #370; `tools/compiler/xdk_catalog.py`,
  `tools/compiler/compile_pipeline.py`, `patches/compiler/0120-independent-symbol-driver.patch`,
  generation fingerprint, catalog/driver tests and both-host CI registration.
- **PC validation:** Original protocol/routing/failure fixtures and existing
  scanner consumers; symbol/generated-output/build/game evidence in docs/project/STATUS.md.
- **Existing Mac support:** Shared Python subprocess argv and path handling;
  exact scanner executable lookup supports native and Windows names.
- **Mac adaptation needed:** None identified; native acceptance pending.
- **Mac acceptance checks:** Run catalog/driver/fingerprint tests, scan the
  supported US image with the pinned native database, regenerate with
  `--generate`, build and validate isolated Story movement/fire/pause/resume.
- **Mac implementation and evidence:** Windows results do not establish native
  scanner/generation/gameplay acceptance; see docs/project/STATUS.md for measured scope.

### MAC-073 - Independent function catalog

- **Status:** Awaiting validation
- **PC behavior:** Production generation classifies measured functions using own
  decoded evidence. Pointer candidates have separate measured entry blocks;
  diagnostic categories never fabricate function extents or CRT symbol names.
- **PC source:** Continuing PR #370;
  `tools/compiler/function_catalog.py`, `function_evidence.py`, `function_categories.py`,
  `patches/compiler/0060-independent-classification.patch`, driver/fingerprint and original tests.
- **PC validation:** Synthetic bounds, operands, ambiguity, graph and standalone
  CLI tests; actual generation/build/game evidence recorded in docs/project/STATUS.md.
- **Existing Mac support:** Shared Python/Capstone implementation, no host-specific
  classification policy. Tests registered for both CI hosts.
- **Mac adaptation needed:** None identified; native acceptance pending.
- **Mac acceptance checks:** Run catalog/evidence/category/fingerprint tests,
  regenerate with `--generate`, compare executable generation, build and verify
  isolated Story movement, firing and pause/resume.
- **Mac implementation and evidence:** Windows checks do not establish Mac
  generation or gameplay acceptance; see docs/project/STATUS.md for measured scope.

### MAC-074 - Independent generation support

- **Status:** Awaiting validation
- **PC behavior:** Production generation uses own manual-source inspection,
  translation reports/declaration headers and atomic generated-text publication.
  Existing definitions/wrappers and unchanged generated-file timestamps persist.
- **PC source:** Continuing PR #370; `tools/compiler/c_source_scan.py`,
  `tools/compiler/generation_output.py`, `patches/compiler/0380-independent-generation-support.patch`,
  generation fingerprint, original tests and shared CI registration.
- **PC validation:** Scanner/output and existing title-wrapper fixtures;
  generation/build/isolated game evidence recorded in docs/project/STATUS.md.
- **Existing Mac support:** Shared standard-library implementation; native text
  newlines and atomic file replacement use host APIs through Python.
- **Mac adaptation needed:** None identified; native acceptance pending.
- **Mac acceptance checks:** Run scanner/output/fingerprint and title-wrapper
  tests, regenerate with `--generate`, compare generated code, build and verify
  isolated Story movement, firing and pause/resume.
- **Mac implementation and evidence:** Windows evidence does not establish native
  generation or gameplay acceptance; see docs/project/STATUS.md for measured scope.

### MAC-075 - Independent translator decoding

- **Status:** Awaiting validation
- **PC behavior:** Production translator uses own instruction/operand decoding,
  restart/CFG traversal and basic-block construction around permitted Capstone.
  CLI output paths resolve before changing subprocess directories.
- **PC source:** Continuing PR #370; `tools/compiler/translation_decode.py`,
  `patches/compiler/0160-independent-translation-decode.patch`, command-builder compatibility,
  generation fingerprint and original/shared consumer tests.
- **PC validation:** Operand/bounds/segment/CFG/overlap/restart fixtures and
  existing translator consumers; generation/build/game evidence in docs/project/STATUS.md.
- **Existing Mac support:** Shared Python/Capstone implementation and tests;
  no host-specific instruction or block policy.
- **Mac adaptation needed:** None identified; native acceptance pending.
- **Mac acceptance checks:** Run original decoder/driver/fingerprint and existing
  translator tests with available native C compiler; regenerate with `--generate`,
  review output, build and verify isolated Story movement/fire/pause/resume.
- **Mac implementation and evidence:** Windows checks do not prove native
  compiler or gameplay acceptance; see docs/project/STATUS.md for measured scope and skips.

### MAC-076 - Independent translation layout and hook planning

- **Status:** Awaiting validation
- **PC behavior:** Translation uses own executable layout and HLE marker/symbol
  planning, with original-body retention and guest ABI thunk generation.
- **PC source:** Continuing PR #370; `tools/compiler/translation_layout.py`,
  `tools/compiler/translation_hooks.py`, `patches/compiler/0130-independent-translation-bindings.patch`,
  generation fingerprints and original/shared/native consumer tests.
- **PC validation:** Bounds/replacement, lexical marker, symbol ambiguity and
  ABI fixtures; generated thunks execute with production register-frame macros.
  CI consumer scaffolding is updated for the current ABI and platform-specific
  header collision checks; portable consumer C also executes through MSVC.
  Actual generation/build/game evidence and limitations are in docs/project/STATUS.md.
- **Existing Mac support:** Common Python policy and runtime ABI macros; tests
  use the shared compiler helper on both hosts. No separate host hook planner.
- **Mac adaptation needed:** None identified; native acceptance pending.
- **Mac acceptance checks:** Run original layout/hook/fingerprint fixtures and
  translator consumers, compile/execute the generated thunk fixture, regenerate
  with `--generate`, build and verify isolated Story movement/fire/pause/resume.
- **Mac implementation and evidence:** Windows/native-C results do not prove
  Mac generation or game acceptance; see docs/project/STATUS.md for measured host evidence.

### MAC-077 - Independent branch feedback and unresolved diagnostics

- **Status:** Awaiting validation
- **PC behavior:** Own cumulative branch ledgers and separate seed export;
  unresolved diagnostics use selected XBE layout and recorded function bounds.
- **PC source:** Continuing PR #370; `tools/compiler/branch_feedback.py`,
  `tools/compiler/unresolved_report.py`, `patches/compiler/0050-independent-branch-diagnostics.patch`,
  source fingerprints and original/shared consumer fixtures.
- **PC validation:** Standalone and production CLI, crash/saturation/overlap,
  input preservation and layout tests; measured build/game checks in docs/project/STATUS.md.
- **Existing Mac support:** Shared Python policy and own image/recovery tools;
  original tests registered in the common native CI suite.
- **Mac adaptation needed:** None identified; native acceptance pending.
- **Mac acceptance checks:** Run original diagnostics and consumer fixtures;
  regenerate with `--generate`, build and verify isolated movement/fire/pause.
- **Mac implementation and evidence:** Windows evidence does not certify native
  Mac generation or gameplay. See docs/project/STATUS.md for actual results and limitations.

### MAC-078 - Independent C operand emission

- **Status:** Awaiting validation
- **PC behavior:** Own register/subregister, address and memory C expressions;
  decoder preserves address overrides, with offset wrapping before FS addition.
- **PC source:** Continuing PR #370; `tools/compiler/translation_operands.py`, address
  metadata in `translation_decode.py`, `0190-independent-translation-operands.patch`,
  generation hashes and original/native/consumer fixtures.
- **PC validation:** Native subregister/address/floating-stack execution and
  actual runtime pointer/memory macros; build/game evidence in docs/project/STATUS.md.
- **Existing Mac support:** Shared Python expressions and runtime ABI interfaces;
  compiler fixtures and existing translator consumers run in both CI hosts.
- **Mac adaptation needed:** None identified; native acceptance pending.
- **Mac acceptance checks:** Run operand/decoder/fingerprint and consumer tests,
  regenerate with `--generate`, build, verify isolated movement/fire/pause/resume.
- **Mac implementation and evidence:** Windows source/native-C evidence does not
  establish Mac generation/gameplay or full x87 precision; see docs/project/STATUS.md.

### MAC-079 - Independent data-transfer and stack lowering

- **Status:** Awaiting validation
- **PC behavior:** Own MOV/MOVZX/MOVSX/LEA/PUSH/POP emission preserves title
  hooks and implements segment-free LEA and stack width/address/alias ordering.
- **PC source:** Continuing PR #370; `tools/compiler/translation_moves.py`,
  `0180-independent-translation-moves.patch`, final replacement ordering in the
  patch loader, generation hashes and original/native/consumer fixtures.
- **PC validation:** Native sign extension, hook order/frame publication,
  stack aliases/word widths and trace bytes; generation/game details in docs/project/STATUS.md.
- **Existing Mac support:** Shared Python lowering and existing guest ABI;
  native compiler fixtures and inherited consumers registered on both CI hosts.
- **Mac adaptation needed:** None identified; native acceptance pending.
- **Mac acceptance checks:** Run move/operand/fingerprint/consumer fixtures,
  regenerate with `--generate`, build and verify isolated movement/fire/pause.
- **Mac implementation and evidence:** Windows validation does not establish
  Mac generation or gameplay. See docs/project/STATUS.md for measured limits.

### MAC-080 - Independent scalar integer lowering

- **Status:** Awaiting validation
- **PC behavior:** Own arithmetic/logic emission samples operands at their width,
  computes carry/borrow, retains result snapshots and the byte-decrement hook.
  INC/DEC preserve carry; NOT preserves flags; NEG avoids signed-overflow C.
- **PC source:** Continuing PR #370 from `830958be`;
  `tools/compiler/translation_integer.py`, `0170-independent-translation-integer.patch`,
  generation fingerprints, final patch ordering and original native fixtures.
- **PC validation:** Native exhaustive-byte/seeded-word/dword results and carries,
  decoded carry chains, memory accesses and title hooks; see docs/project/STATUS.md for
  generation, Windows build and isolated gameplay evidence.
- **Existing Mac support:** Shared Python implementation and runtime ABI;
  compiler fixtures and consumers are registered on both CI hosts.
- **Mac adaptation needed:** None identified; native acceptance pending.
- **Mac acceptance checks:** Run integer/move/operand/consumer fixtures,
  regenerate with `--generate`, build and verify isolated movement/fire/pause.
- **Mac implementation and evidence:** Windows results do not establish native
  Mac generation or gameplay. Flag-condition lowering and other families remain
  separate replacement work.

### MAC-081 - Independent shift and rotate lowering

- **Status:** Awaiting validation
- **PC behavior:** Own shifts/rotates honor widths and count masking, preserve
  zero-count emitted state and publish carry; known constant no-ops retain the
  prior flag interpretation. Word double shifts accept count 16; larger counts
  use an explicit no-change policy for architecturally undefined results.
- **PC source:** Continuing PR #370 from `9c2e509b`; `translation_bits.py`,
  `0150-independent-translation-bits.patch`, fingerprints, fixtures and final ordering.
- **PC validation:** Native bit-step and actual x86 instruction value/carry
  comparisons across byte values and count encodings, wider seeded/boundary
  cases; see docs/project/STATUS.md for generation/build/game evidence and scope.
- **Existing Mac support:** Shared lowering and portable C reference fixtures;
  the Windows ML64 hardware reference is additional host-specific evidence.
- **Mac adaptation needed:** None identified; native acceptance pending.
- **Mac acceptance checks:** Run bit/integer/operand/consumer fixtures,
  regenerate with `--generate`, build and verify isolated movement/fire/pause.
- **Mac implementation and evidence:** Native Mac generation and gameplay remain
  unverified. Dynamic zero-count conditional provenance and complete flag lowering
  remain separate replacement work on both hosts.

### MAC-082 - Projected enemy shield effects (issue #358)

- **Status:** Awaiting validation
- **PC behavior:** The shielded robot in Something to Crow About keeps its
  shield over its body on wide displays instead of shifting toward the old
  4:3 center. Ordinary HUD sprites and explicit placements stay unchanged.
- **Source:** `358-enemy-shield-effects-offset`,
  `patches/graphics/interface/0730-world-shields.patch`, shared D3D compatibility path.
- **Windows evidence:** User's 3440×1440 capture, D3D draw bisection, and
  placement replay identify the 24-byte projected shield strips. Release
  build, captured-frame replay, and isolated live validation at 3440×1440
  pass. See `docs/project/STATUS.md`.
- **Mac adaptation needed:** Confirm native renderer follows the shared
  projected-draw placement. Apply equivalent handling only if its path differs.
- **Mac acceptance:** Build on Mac and inspect shielded robots at 16:9 and
  ultrawide, plus health bars, HUD widgets and menu overlays.

### MAC-084 - Independent multiply and divide lowering

- **Status:** Awaiting validation
- **PC behavior:** Own width-aware products and quotients, sampled divisors,
  defined multiplication carry/overflow and explicit fatal divide errors before
  quotient/remainder writes. Guest SEH remains separate foundation work.
- **PC source:** `8763627e` on `feature/independent-bit-operations`; `translation_product.py`,
  `guest_integer_fault.c`, `0200-independent-translation-product.patch` and consumers.
- **PC validation:** Original compiled references, native x86 oracle, aliases,
  divide fault state and real host adapter; see docs/project/STATUS.md for current results.
- **Existing Mac support:** Shared lowering and C adapter use POSIX SIGFPE;
  portable fixtures run with undefined-behavior sanitization in Mac CI.
- **Mac adaptation needed:** No separate arithmetic implementation; native
  signal/crash integration and supported-title acceptance remain unverified.
- **Mac acceptance checks:** Run product/consumer/native fault fixtures, regenerate
  with `--generate`, build and verify isolated movement/fire/pause.
- **Mac implementation and evidence:** Native Mac generation/build/gameplay are
  pending. CI is distinct from title gameplay and guest SEH remains unfinished.

### MAC-083 - Independent bit tests, scans and byte swaps

- **Status:** Awaiting validation
- **PC behavior:** Width-correct bit tests/modifications, signed memory bit
  strings, captured carry consumers, scan zero flags and dword byte swapping.
- **PC source:** `52a85efc` on `feature/independent-bit-operations`;
  `tools/compiler/translation_bitops.py`, additive patch and original native fixtures.
- **PC validation:** See docs/project/STATUS.md for current native, generation and game results.
- **Existing Mac support:** Shared Python emission and portable C fixtures run
  through the same build/CI paths, with sanitizer checks on supported compilers.
- **Mac adaptation needed:** No separate algorithm; native title acceptance
  remains pending. Windows ML64 references are supplementary host-only tests.
- **Mac acceptance checks:** Run translation and patch consumers, regenerate,
  build and verify isolated movement, firing and pause/resume.
- **Mac implementation and evidence:** Shared source implemented; native Mac
  generation/build/gameplay unverified. LOCK and full flag provenance are deferred.

### MAC-085 - Shared impact spark timing (issue #371)

- **Status:** Awaiting validation
- **PC behavior:** Weapon impact sparks update their geometry on every advancing
  native frame instead of waiting for a complete 60 Hz tick, addressing the
  black impact streaks seen at 180 FPS across weapons.
- **PC source:** `48de7c0` on `371-laser-impact-flashes`; shared
  `src/gameplay/pc_native_numeric.h` and original guest regression fixture.
- **PC validation:** Original US spark updater passes fractional/whole/pause
  tests at 60, 120, 180 and 240 FPS, including an unhooked negative case.
  Windows live firing validation is recorded separately in `docs/project/STATUS.md`.
- **Existing Mac support:** Shared numeric policy is compiled by both hosts.
- **Mac adaptation needed:** None identified; native gameplay remains to be
  checked on Mac.
- **Mac acceptance checks:** Run the original spark fixture, build the native
  game, and fire several weapons at nearby scenery at 60 and 120+ FPS while
  inspecting impact effects and pause/resume.
- **Mac implementation and evidence:** Native Mac validation pending; Windows
  results do not establish visual parity.

### MAC-086 - Independent exchange lowering and atomic helpers

- **Status:** Awaiting validation
- **PC behavior:** XCHG/XADD/CMPXCHG use original lowering, sampled addresses,
  byte/word/dword atomics and captured arithmetic conditions.
- **PC source:** PR #375, implementation `de66eb9a`; `translation_exchange.py`
  and `independent-translation-exchange.patch` (including original host helpers).
- **PC validation:** Native reference, decoded-consumer and concurrency fixtures;
  generation/build/game results are recorded in STATUS.md.
- **Existing Mac support:** Shared emission and Clang sequentially consistent
  atomics. Portable tests use the same helper bodies with pthread contention.
- **Mac adaptation needed:** No separate algorithms. Confirm aligned mapped-RAM
  operations in the native title; MMIO and guest fault semantics remain separate.
- **Mac acceptance checks:** Run exchange/consumer/contention tests, regenerate,
  build and verify isolated movement/fire/pause on the native host.
- **Mac implementation and evidence:** Shared implementation complete; native
  Mac generation/build/gameplay pending. Windows x86 oracle is supplemental.

### MAC-087 - Independent comparison snapshots and sign extension

- **Status:** Awaiting validation
- **PC behavior:** Original CMP/TEST snapshots and CBW/CWD/CWDE/CDQ emission.
- **PC source:** PR #375, implementation `f941d1d4`; `translation_compare.py`
  and `independent-translation-compare.patch`.
- **PC validation:** Native width, hook, carry and preservation contracts plus
  decoded consumers; generation/build/game evidence in STATUS.md.
- **Existing Mac support:** Shared Python emission and portable C, registered
  in native CI. No separate platform algorithm is needed.
- **Mac adaptation needed:** None identified; native title acceptance pending.
- **Mac acceptance checks:** Run comparison/consumer fixtures, regenerate and
  build, then verify isolated movement, firing and pause/resume.
- **Mac implementation and evidence:** Shared implementation; native Mac
  generation/build/gameplay unverified. Full flags and guest faults remain work.

### MAC-088 - Independent string-instruction lowering

- **Status:** Awaiting validation
- **PC behavior:** Original byte/word/dword MOVS/STOS/LODS/CMPS/SCAS and
  direction emission, including repeats, overlapping copies and address-size wrap.
- **PC source:** PR #375, implementation `c4a56fbe`; `translation_strings.py`
  and `independent-translation-strings.patch`.
- **PC validation:** 126 focused and 56 patch/context checks passed; normal
  generation/build/native checks and isolated game acceptance passed. See STATUS.md.
- **Existing Mac support:** Shared Python emission and portable C; new fixtures
  are registered in both-host CI. Only the supplemental x86 oracle is Windows-only.
- **Mac adaptation needed:** None identified; native title acceptance pending.
- **Mac acceptance checks:** Run string/consumer tests and patch upgrades;
  regenerate/build and verify isolated movement, firing and pause/resume.
- **Mac implementation and evidence:** Shared implementation; native title
  generation/build/gameplay unverified. Complete flags and guest faults remain work.

### MAC-089 - Independent condition-code generation

- **Status:** Awaiting validation
- **PC behavior:** Captured flags drive branches, conditional moves, SETcc and
  LAHF, including signed overflow and defined auxiliary carry.
- **PC source:** PR #375, this change; `translation_conditions.py`,
  `translation_integer.py`, `independent-translation-conditions.patch`.
- **PC validation:** 131 focused, 37 join, 109 timing and 56 patch/context
  checks passed, plus supported-US
  generation, Release build and isolated movement/firing/pause/resume. All 19
  protected files unchanged; see STATUS.md for measured evidence and limits.
- **Existing Mac support:** Shared Python/C generation, no host-specific feature path.
- **Mac adaptation needed:** None identified; native title acceptance pending.
- **Mac acceptance checks:** Run condition/consumer and patch upgrade tests;
  regenerate/build and verify isolated movement, firing and pause/resume.
- **Mac implementation and evidence:** Shared implementation, native title
  generation/build/gameplay unverified; complete EFLAGS remains work.

### MAC-090 - Independent SSE emission

- **Status:** Awaiting validation
- **PC behavior:** Scalar/packed floating and bit operations preserve lane bits,
  use proper double widths, evaluate full packed roots/reciprocals, and guard
  float-to-integer casts. Numeric hooks and integer-flag provenance are retained.
- **PC source:** PR #375, SSE replacement stage; `translation_sse.py` and
  `independent-translation-sse.patch`.
- **PC validation:** 174 translator/generation/private join checks and 109 timing
  checks passed. Supported US regeneration, normal Release/native build checks
  and isolated Original60 movement/firing/pause/resume passed; all 19 protected
  files unchanged. All 56 patch/context/backlog/snapshot checks passed; see STATUS.md.
- **Existing Mac support:** Shared portable C/Python emitter and host math library.
- **Mac adaptation needed:** None identified; native title acceptance pending.
- **Mac acceptance checks:** Run SSE/translator and private game-routine fixtures,
  regenerate/build and verify isolated movement/firing/pause/resume.
- **Mac implementation and evidence:** Shared implementation; native title
  generation/build/gameplay unverified. Guest FP environment/faults remain work.

### MAC-091 - Independent MMX emission

- **Status:** Awaiting validation
- **PC behavior:** Packed integer operations use captured lanes, bounded shifts,
  defined overflow/saturation, correct memory widths and guarded conversions.
- **PC source:** PR #375, MMX replacement stage; `translation_mmx.py` and
  `independent-translation-mmx.patch`.
- **PC validation:** 288 translator/generation/private join/timing checks passed.
  Supported US regeneration, normal Release/native build checks and isolated
  Original60 movement/firing/pause/resume passed; 19 protected files unchanged.
  All 56 historical patch/context/backlog/snapshot checks passed; see STATUS.md.
- **Existing Mac support:** Shared portable Python/C implementation and host math.
- **Mac adaptation needed:** None identified; native title acceptance pending.
- **Mac acceptance checks:** Run own MMX/translator and private fixtures, generate
  and build, then verify isolated movement/firing/pause/resume.
- **Mac implementation and evidence:** Shared implementation; native title
  generation/build/gameplay unverified. MMX/x87 aliasing and guest FP environment
  remain execution-foundation work.

## Entry template

Copy this template for each new entry and add its queue row. Replace all placeholders.

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


### MAC-092 - Independent x87 emission

- **Status:** Awaiting validation
- PC behavior/source: PR #375 follow-up; original
  `tools/compiler/translation_x87.py`, `tests/compiler/test_translation_x87.py` and additive
  `patches/compiler/independent-translation-x87.patch`. The shared emitter preserves
  binary64 stack/TOP and numeric-policy interfaces while replacing upstream
  x87 emission. Guest rounding and guarded integer conversion are explicit;
  memory80 formats convert to binary64. Foundation/FP environment remain work.
- Mac support: portable Python/C and libm use existing shared interfaces; no
  Mac-specific algorithm is required. CI runs portable fixtures; supplemental
  native x86 instruction oracles are Windows-only.
- Remaining validation: Windows generation, Release build and isolated first-mission
  movement/fire/pause/resume passed; see STATUS.md for counts and limits. Native Mac
  title verification must use the supported private US data; CI unit/GPU checks
  alone do not prove gameplay. Preserve saves/settings and report rendering,
  input and audio limits. See STATUS.md for actual PC acceptance evidence.

- CI fixture follow-up: `3ad34bc7` Mac CI exposed missing SET_LO16 in the
  inherited classification fixture. The additive fixture repair includes the
  production definition and runs the same C via MSVC too (47 focused checks
  passed locally). This changes tests only; repaired CI is pending and native
  Mac title acceptance remains unverified.

### MAC-093 - Independent control-transfer emission

- **Status:** Awaiting validation
- PC behavior/source: PR #375 follow-up in
  `tools/compiler/translation_control.py`, `tests/compiler/test_translation_control.py` and
  `patches/compiler/independent-translation-control.patch`. Original call/return/branch,
  bounded jump-table and callee-cleanup emission preserves the common guest ABI.
- Mac support: portable Python/C emission through existing shared runtime
  interfaces; no duplicate host algorithm. CI selects the shared contracts.
- Remaining validation: Windows generation, Release build and isolated first-
  mission movement/fire/pause/resume passed. All 304 regressions and 56 patch/
  context/backlog/snapshot checks passed. Native Mac generation/build/gameplay
  still require supported private US data; CI unit/GPU checks alone do not prove
  title behavior. See STATUS.md for evidence, preserved personal data and
  rendering/input/audio limits. All 16 checks passed for `43ee2929`; the preceding Windows
  string-fixture timeout was addressed by caching setup with coverage unchanged.


### MAC-094 - Independent instruction and block lifting

- **Status:** Awaiting validation
- PC behavior/source: PR #375 follow-up:
  `tools/compiler/translation_lifting.py`, `tests/compiler/test_translation_lifting.py` and
  `patches/compiler/independent-translation-lifting.patch`. Original dispatch and block
  state propagation preserve the common generated-code/condition interface.
- Mac support: shared portable Python and emitted C; no host-specific algorithm
  or adapter required. Both CI hosts select the decoded/compiled contracts.
- Remaining validation: Windows generation, Release build and isolated first-mission
  movement/fire/pause/resume passed; 345 regressions and 127 final lifting/CI
  checks passed. All 56 patch/context/backlog/snapshot checks passed.
  All 21 generated files match the accepted baseline.
  Native Mac generation/build/gameplay require private
  supported data and its host; Windows evidence cannot establish that behavior.
  See STATUS.md for measured outcomes and limitations.


### MAC-095 - Independent lifter context and helper recognition

- **Status:** Awaiting validation
- PC behavior/source: PR #375 follow-up in `tools/compiler/translation_context.py`,
  `tools/compiler/translation_helpers.py`, `tools/compiler/translation_names.py`, SDK-derived
  `tools/translation_host_symbols.*`, `tools/compiler/generate_host_symbols.py`,
  `tests/compiler/test_translation_context.py` and the additive context patch.
- Mac support: shared portable Python and deterministic checked-in name data;
  SDK inventory regeneration is a Windows maintenance task, not a Mac build
  prerequisite. No duplicate host algorithm or adapter is required.
- Remaining validation: Windows final regeneration, Release build and isolated
  first-mission movement/fire/pause/resume passed. All 21 generated files match
  the accepted baseline. 352 regressions passed before the final span repair;
  17 focused/SEH checks passed after it, plus 56 patch/context/backlog/snapshot
  checks. Native Mac generation/build/gameplay require its host and supported
  private data. See STATUS.md for exact evidence, CI revision and limits.


### MAC-096 - Independent batch translation and dispatch output

- **Status:** Awaiting validation
- PC behavior/source: PR #375 follow-up in
  `tools/compiler/translation_batch.py`, `tests/compiler/test_translation_batch.py` and
  `patches/compiler/independent-translation-batch.patch`. Original batch assembly and
  generated dispatch retain injected function translation and runtime ABI inputs.
- Mac support: shared Python and portable generated C; no duplicate host
  algorithm or adapter. Both CI hosts select the same output/dispatch contracts.
- Remaining validation: Windows generation, Release/native checks and isolated first-
  mission movement/fire/pause/resume passed. All 363 translator/generation/join/
  timing regressions passed. Native Mac title generation/build/gameplay need its
  host and supported private US data. CI checks alone cannot establish title
  behavior. See STATUS.md for measured results and exact CI revisions.


### MAC-097 - Independent translator context and entry discovery

- **Status:** Awaiting validation
- PC behavior/source: PR #375 follow-up in `tools/compiler/translation_discovery.py`,
  `tests/compiler/test_translation_discovery.py` and the additive discovery patch.
  Original context/CFG/table/entry logic serves the remaining function emitter.
- Mac support: portable Python around the shared project decoder and permitted
  Capstone; generated C uses the existing shared runtime ABI. Both CI hosts
  select synthetic/compiled contracts. No separate host algorithm is required.
- Remaining validation: Windows final generation, Release/native checks and isolated
  first-mission movement/fire/pause/resume passed. All 21 generated files match
  the accepted batch baseline. Final 100 focused checks and eight crash-capture
  checks passed; 379 regressions passed before the final read-window correction.
  All 56 patch/context/backlog/snapshot checks passed.
  Native Mac title generation/build/gameplay need its host and private US data.
  See STATUS.md for actual CI failures/repair, measured results and limitations.

### MAC-098 - Independent complete function emission

- **Status:** Awaiting validation
- PC behavior/source: uncommitted PR #375 follow-up in
  `tools/compiler/translation_function.py`, `tests/compiler/test_translation_function.py` and
  `patches/compiler/independent-translation-function.patch`. Original assembly, flag
  joins, stack captures and accessor output replace the remaining translator.
- Mac support: portable Python and shared generated C/runtime ABI; both hosts
  select the original compiled contracts and existing consumer/negative controls.
  No separate host algorithm is required.
- Remaining validation: Windows generation, Release/native checks and isolated
  first-mission movement/fire/pause/resume passed. All 389 regressions and 438
  consumer checks passed (56 consumer skips, 101 subtests). Generated function
  bodies match after reviewed comments and x87 accessor changes; personal files
  remain unchanged. All 56 historical patch/context/backlog/snapshot checks passed.
  Native Mac title generation/build/gameplay require its host and private US data.
  See STATUS.md for measured results and explicit validation limits.

### MAC-099 - Independent translation command line

- **Status:** Awaiting validation
- PC behavior/source: uncommitted PR #375 follow-up in `translation_cli.py`,
  `compile_pipeline.py`, original CLI tests and the additive CLI bridge patch.
  All four generation stages now invoke project tools directly; the runtime
  remains an explicit data/header/hook input, never a lifting Python provider.
- Mac support: shared Python selection, generation and emitted C. Both CI hosts
  select synthetic/compiled contracts. No separate host algorithm is required.
- Remaining validation: Windows generation, hook refresh, Release/native checks
  and isolated gameplay passed; all 21 generated files are unchanged. Focused
  checks passed 142 tests; the broader run had 412 passes and one timeout whose
  unchanged isolated recheck passed. All 56 historical patch/context/backlog/
  snapshot checks passed. Native Mac title generation/build/gameplay require
  its host and private US data.
  See STATUS.md for measured results and limitations.


### MAC-100 - Project guest register and memory primitives

- **Status:** Awaiting validation
- PC behavior/source: PR #375 guest-ABI follow-up (based on `eb7b0ba1`) in
  `patches/compiler/independent-guest-abi.patch`, `tests/compiler/guest_abi.c` and
  `tests/compiler/test_guest_abi.py`. Project-owned TLS register declarations, lane
  layouts, scalar memory access, partial writes and stack primitives preserve
  the generated-code ABI while making unaligned access and alias order explicit.
- Mac support: shared implementation with GCC/Clang unaligned scalar alias
  types and compiler TLS; MSVC uses its corresponding unaligned qualifier.
  Both CI hosts select the same standalone, production and mutation fixtures.
- Remaining validation: 125 final focused Windows checks, supported-US
  generation, fresh Release/native checks and isolated movement/fire/pause/resume
  passed. Only the runtime header changed among 21 generated C/header files.
  Initial fixture failures and unchanged timing rechecks are recorded in STATUS.
  Historical runtime upgrades passed; the missing backlog queue row was repaired.
  Native Mac title build/gameplay require its host and private data. Remaining header helpers, memory mapping and kernel
  storage are separate work.


### MAC-101 - Project indirect guest-call dispatch

- **Status:** Awaiting validation
- PC behavior/source: PR #375 follow-up based on `b19e8634` in
  `patches/compiler/0195-independent-guest-dispatch.patch`, `tests/compiler/guest_dispatch.c`
  and `tests/compiler/test_guest_dispatch.py`. Original common dispatch helpers replace
  repeated inherited indirect-call bodies while preserving lookup priority,
  diagnostics, trace admission, lazy expressions and stack recovery.
- Mac support: shared C implementation; no new platform adapter. Both CI hosts
  select the same full-header/isolated ABI/feedback matrix and negative controls.
- Remaining validation: 175 focused Windows checks, supported-US generation,
  corrected Release/native checks and all 17 CI checks on `00405685` passed.
  After initial inconclusive runs under substantial concurrent-game load, the
  clean Windows replay passed movement/fire/pause/resume with protected files
  unchanged. See STATUS.md for evidence. Native Mac title build/gameplay require
  its host and private US data. Passing asset-free CI is a separate claim.

### MAC-102 - Project call checks and HLE argument frames

- **Status:** Awaiting validation
- PC behavior/source: PR #375 follow-up based on `00405685` in
  `patches/compiler/0196-independent-call-frames.patch`, `tests/compiler/guest_call_frames.c`
  and `tests/compiler/test_guest_call_frames.py`. Original call snapshots/checks and
  HLE scratch-frame capture/invocation preserve lazy diagnostic arguments,
  register results, nested frames and wrapping cleanup.
- Mac support: shared C implementation; no new platform adapter. Both CI hosts
  select the production/isolated diagnostic modes and negative controls.
- Windows validation: 184 affected checks and 70 historical checks passed;
  generation, Release/native checks and isolated movement/fire/pause/resume
  replay passed. All 26 protected files were unchanged; see STATUS.md.
- Folder reconciliation with main retained all replacement stages; regenerated
  C/header bytes match and the rebuilt Windows gameplay replay passed again.
  The shared fixture and standalone Windows runner routing repairs are in PR #375.
- Remaining validation: native Mac title generation/build/gameplay require its
  host and private US data. Physical input, audio listening and full-campaign
  behavior are outside the bounded injected-input Windows replay.

### MAC-103 - Project vector constructor and obsolete SIMD helper retirement

- **Status:** Awaiting validation
- PC behavior/source: PR #375 follow-up based on 46e68827, in
  patches/compiler/0197-independent-vector-support.patch and
  tests/compiler/test_vector_support.py / vector_support.c. Independent SSE/MMX
  emitters replace the retired operation helpers; the model-transform scalar
  constructor preserves raw bits and clears upper lanes.
- Mac support: shared portable C, no platform adapter required. CI selects the
  production/isolated headers, mutation controls and existing SIMD oracles.
- Windows validation: 32 focused and 695 compiler checks passed; supported-US
  generation, Release/native checks and isolated movement/fire/pause/resume
  passed, as did all 73 historical/snapshot/context/backlog checks. All 26
  protected files were unchanged. Native Mac title acceptance remains pending. See STATUS.md for
  measurements and the injected-input/minimized-window acceptance limits.

### MAC-104 - Direct scalar extension and numeric helper retirement

- **Status:** Awaiting validation
- PC behavior/source: PR #375 follow-up based on e56623d1, in
  tools/compiler/translation_moves.py, patches/compiler/0198-independent-scalar-support.patch
  and tests/compiler/test_scalar_support.py / scalar_support.c. Unsigned masking
  and sign-bit transformation implement MOVSX/MOVZX with one source read;
  string direction samples DF before its argument. Obsolete helpers are removed.
- Mac support: common C/Python; no platform adapter. Both CI hosts run the
  exhaustive extension/direction checks, preserved rounding assertions and
  compiled negative controls alongside existing x87 and integer regressions.
- Windows validation: all 17 prerequisite CI checks passed on e56623d1; all 24
  focused and 707 compiler checks passed. Production generation matches the
  7,894 predicted expression replacements; Release/native checks and isolated
  movement/fire/pause/resume passed, with all 26 protected files unchanged.
  All 73 historical/snapshot/context/backlog checks passed. Native Mac title
  acceptance remains pending; see STATUS.md for measurements and bounded replay limitations.

### MAC-105 - Canonical project guest execution header

- **Status:** Awaiting validation
- CI follow-up: `0201-runtime-header-consumer-fixture.patch` repairs extraction
  of the admission helper in an inherited guarded-call test. Both native CI
  hosts exposed the missing helper at 519b9d34. The new project regression also
  executes the oracle with MSVC alone; this fixture repair changes no game code.
- Repair validation on Windows: 11 focused, 718 compiler and 73 migration/
  repository checks passed. The prior verified executable is unchanged; no new
  game build or replay is claimed. Native Mac CI for the repair remains pending.
- PC behavior/source: PR #375 follow-up based on 60c47380;
  src/runtime/recomp_types.h, compiler batch/CLI routing and
  patches/compiler/0199-independent-runtime-header.patch. Generation owns its
  default header input; service implementations remain separate replacement work.
- Mac support: common C/Python with existing host compiler adapters. Shared CI
  selects service/admission contracts and toolkit-free synthetic generation;
  no new platform-specific implementation is required.
- Windows validation: 66 focused and 716 compiler checks passed. Supported-US
  generation changes only the types header; Release/native checks and isolated
  movement/fire/pause/resume passed, with all 26 protected files unchanged.
  All 73 historical-upgrade/snapshot/repository checks passed. Native Mac title
  acceptance remains pending; see STATUS.md for measurements and replay limits.

### MAC-106 - Project branch-feedback provider and generated header

- **Status:** Awaiting validation
- PC behavior/source: PR #375 branch-feedback stage based on 676545eb;
  src/runtime/guest_branch_feedback.c, tsfp_branch_feedback.h, canonical header
  callbacks, compiler batch/hook publication and
  patches/compiler/0202-independent-branch-feedback.patch. Optional observations
  use atomic target flags/counters and bounded synchronized site records.
- Mac support: shared C/Python with MSVC intrinsics or compiler atomics. Existing
  source-layout routing supplies the own include directory on both hosts; no
  separate gameplay policy or new OS adapter. Both CI hosts select recorder,
  generated-header and inherited consumer contracts.
- Acceptance: run native recording/concurrency/failure fixtures, enabled and
  disabled synthetic generated code, patch transitions and complete generation/
  build, then isolated title boot/movement/fire/pause/resume. Explicit recorder
  initialization/dump is tested separately; forced process termination is not
  an exit-dump check. Windows passed 739 compiler and 73 migration/repository
  checks, followed by the publication/isolation checks, enabled and normal
  Release builds and isolated Story movement/fire/pause/resume replays.
  See STATUS.md for hashes, measurements and limits. Native Mac title acceptance
  remains pending; shared-source and Windows results do not establish it.
  The first Mac CI run at 3965c6df exposed a warning in the deliberately broken
  double-address fixture; it now explicitly discards the first value while
  retaining both evaluations. The repair passed both native CI hosts at
  0a991a8e; it changes no runtime source or Windows gameplay evidence. Mac title
  acceptance remains separate.

### MAC-107 - Project native continuation service

- **Status:** Awaiting validation
- PC behavior/source: PR #375 native-continuation stage based on 0a991a8e (all 17
  prerequisite CI checks passed); src/runtime/guest_nonlocal_jump.c,
  tests/compiler/nonlocal_jump_runtime.c and test_nonlocal_jump_runtime.py,
  patches/compiler/0203-independent-nonlocal-jumps.patch. Own TLS continuation
  storage/restoration replaces the inherited implementation. Windows checks
  stack membership and requires a foreign adapter to transfer control; absent
  or returning adapters stop before an invalid native unwind.
- Mac support: common C and canonical guest-cell interface, with pthread test
  coordination. POSIX retains the existing same-stack assumption. No Mac fiber
  ownership adapter is provided or claimed. Register foreign handlers at startup
  or while guest threads are quiescent.
- Acceptance: direct real-provider cases, compiled negative controls, emitted
  calls using the canonical header, patch upgrades, native build and isolated
  title regression. Windows fiber handoff/error paths are host-specific checks.
  The supported-US translator detects neither CRT helper, so game boot alone
  does not exercise this service. Windows passed 759 compiler checks, 73
  migration/repository checks, Release/native build checks and an isolated
  Story regression replay. See STATUS.md for source/binary provenance and scope.
  Portable native CI passed on both hosts at f85490ad; native Mac title
  and fiber acceptance remain separate and unverified.


### MAC-108 - Project guest state and ABI diagnostics

- **Status:** Awaiting validation
- PC behavior/source: PR #375 working-tree stage tested from f85490ad;
  src/runtime/guest_state.c, guest_abi_diagnostics.c, tests/compiler/guest_state_runtime.c,
  guest_abi_runtime.c and test_guest_services.py; patch 0204-independent-guest-state.
  Project storage preserves native-thread register ownership and process-wide
  history. Optional diagnostic reports retain bounded target sets and add
  serialized reporting plus defined modular stack-delta arithmetic.
- Mac support: shared C, compiler atomics and pthread contract fixtures; no new
  OS adapter is needed. Fibers still share native-thread cells; this stage does
  not replace thread scheduling, context switching, TIB allocation or memory.
  Diagnostic serialization does not make the volatile call ring coherent.
- Acceptance: cross-unit and thread initial-state/ownership checks, report
  deduplication/capacity/concurrency, wrapping deltas, actual call macros on/off,
  compiled negative controls, full patch upgrades, native build and isolated
  Story regression. Windows passed 774 compiler and 73 migration/repository
  checks, Release/native build checks and the isolated replay. Portable contracts and patch upgrades passed on both CI hosts at c42f3b1e;
  native Mac title gameplay remains pending. See STATUS.md
  for exact measurements and limits.


### MAC-109 - Project native image loading

- **Status:** Awaiting validation
- PC behavior/source: PR #375 working-tree stage tested from c42f3b1e;
  src/runtime/guest_image.c and guest_image.h, tests/compiler/guest_image_runtime.c
  and test_guest_image.py, additive patch 0205-independent-image-loader.patch.
  Original byte inspection/copying replaces inherited XBE parsing, validates
  before mapping/writes and publishes title/import/TLS/bounds metadata.
- Mac support: shared C with explicit little-endian reads and no host APIs;
  both hosts use the same provider. Host mappings, MMIO, TIB initialization and
  thread scheduling remain separate. No native Mac title execution is inferred.
- Acceptance: native synthetic malformed/overflow/overlap/atomic-failure and
  byte/metadata oracles, compiled mutations, supported-US private comparison,
  complete patch upgrades, production build and isolated Story movement/fire/
  pause/resume. Windows passed 64 installed image and 838 full compiler checks, 73 migrations/
  repository checks, normal Release/native build checks and isolated Story
  movement/fire/pause/resume. Candidate baseline and final Python byte oracles
  passed. Native Mac title acceptance remains pending. The
  f3fd8a08 native CI jobs timed out formatting an expected byte-mismatch report;
  the diagnostic repair passed all64 image checks locally with CI=true and
  passed all 17 remote checks at 1976a07d, including Windows/macOS native and
  patch upgrades. Runtime code is unchanged. See STATUS.md.


### MAC-110 - Project guest allocation ownership

- **Status:** Awaiting validation
- **PC behavior:** Project heap, virtual-reservation and physical ownership
  replace inherited allocator bodies through additive patch 0206. Exact/ranged physical
  requests validate before zeroing, metadata exhaustion fails atomically, and
  graphics/audio retain extent/enumeration contracts. Placement/slack policy
  changes are explicit; see [allocation contracts](../agent-context/independent.md#guest-allocation-ownership).
- **Source/PR:** `src/runtime/guest_allocation.c`, `guest_allocation.h`,
  `patches/compiler/0206-independent-guest-allocations.patch`,
  `tests/compiler/test_guest_allocation.py` and its original native harnesses;
  PR #375, commit 3b722823, validated from a working tree based on 1976a07d.
- **Mac support present:** Same portable provider and shared kernel routes;
  Clang atomics implement serialization, with no new platform adapter or SDK.
- **Remaining adaptation:** None identified for this bounded ownership layer.
  Host maps, MMIO, threads and broader kernel/device replacement remain separate.
- **Acceptance:** Windows: 858 installed compiler tests passed, including 20
  allocation checks with 2,100 randomized operations, 8,000 concurrent cycles and
  five negative controls. Release/native checks and isolated Story movement,
  firing and pause/resume passed; protected files stayed unchanged. All 73
  migration/tracking/snapshot/context/backlog checks passed. All 17 checks at
  3b722823 passed, including portable Mac contracts and patch upgrades. Native
  Mac title gameplay remains unverified. See
  [STATUS](STATUS.md#project-guest-allocation-ownership-2026-10-28) for updates.

### MAC-111 - Project guest thread storage

- **Status:** Awaiting validation
- **PC behavior:** Main/private TIB/TLS/object construction and heap/fixed stack
  ownership use the project provider. Failed host creation and failed timer TIB
  creation release their owned stacks. Published identity storage remains live
  for the guest-memory lifetime; host lifecycle and scheduling remain separate.
- **Source/PR:** `src/runtime/guest_thread_memory.c`, its header,
  `tests/compiler/test_guest_thread_memory.py` and original native harness,
  `patches/compiler/0207-independent-thread-memory.patch`; PR #375, validated
  from a working tree based on 3b722823.
- **Mac support present:** Same C provider and shared kernel routes, with Clang
  atomics; no new SDK or host adapter. Timer and USB use the same fixed slots.
- **Remaining adaptation:** None identified for storage construction/ownership.
  Existing template-copy and identity-lifetime limits are shared and explicit.
- **Acceptance:** Windows 870 installed compiler checks passed, including 12
  thread-storage checks with 400 concurrent
  identity/stack cycles, five negative controls, supported-US bootstrap bytes and
  actual installed adapter/failure paths. Release/native checks and isolated
  Story movement/firing/pause/resume passed; protected files stayed unchanged.
  All 73 migration/tracking/snapshot/context/backlog checks passed. All 17 CI
  checks at 7fef998e passed, including Mac contracts and patch upgrades. Native
  Mac title gameplay remains unverified. See
  [STATUS](STATUS.md#project-guest-thread-storage-2026-10-28).

### MAC-112 - Project virtual-memory services

- **Status:** Awaiting validation
- **PC behavior:** Guest Nt allocation/release/query/protection policy is project
  owned, with complete output-span checks and exact reservation release. Native
  pointer-width VM policy stays separate from the seven-word guest record.
- **Source/PR:** `src/runtime/guest_virtual_memory.c`, `host_virtual_memory.c`,
  allocator virtual release and headers; original compiler fixtures and
  `patches/compiler/0208-independent-virtual-memory.patch`. PR #375, commit
  47d687ef, based on 7fef998e.
- **Mac support present:** Same providers/adapters and guest ownership. Guest
  buffer validation uses real Mach region queries through `pc_guest_memory.h`.
  Protection-output overlap uses the host page size, including 16 KiB pages.
- **Remaining adaptation:** Existing host mapping/protection implementation,
  including compatibility native VirtualQuery's placeholder record and unaligned
  mprotect limitations, remains separate replacement work. Guest ABI queries
  and buffer validation do not use that placeholder. No dependency upgrades.
- **Acceptance:** Windows 889 compiler/73 migration checks passed before the
  final POSIX page-size adapter correction. After it, all 21 installed VM
  checks and 125 repository checks passed, including both adapter host branches
  and real Windows VM probes. Final Release/native checks and isolated Story
  movement/firing/pause/resume passed; 26 protected files were unchanged.
  All 17 CI checks at 47d687ef passed. Native Mac title gameplay remains
  unverified. See
  [STATUS](STATUS.md#project-virtual-memory-services-2026-10-28).

### MAC-113 - Project native memory ownership

- **Status:** Awaiting validation
- **PC behavior:** Thirteen native Mm bodies use explicit native block ownership,
  matching release kinds, host page protection and region queries. Actual host
  pointers remain pointer-width; guest allocations retain their own provider.
- **Source/PR:** `src/runtime/host_blocks.c`, `host_blocks.h`, `host_pages.h`,
  `src/native/host_pages.c`, original compiler fixtures and additive
  `patches/compiler/0209-independent-native-memory.patch`. PR #375; implementation
  based on 47d687ef.
- **Mac support present:** Same ownership policy with pthread_once initialization,
  mmap/munmap/mprotect and real Mach region query/object-port cleanup. No toolkit
  VirtualQuery placeholder is used by the new Mm queries. Host-page rounding
  includes 16 KiB pages; callbacks preserve failed-release ownership for retry.
- **Remaining adaptation:** Guest Mm/statistics and main mapping services remain
  separate replacement work. Native Nt platform callbacks retain their existing
  limitations. Process-lifetime Mm metadata does not claim safe global worker
  shutdown. See [contracts](../agent-context/native-memory.md).
- **Acceptance:** Windows candidate ownership/page/adapter checks passed (15),
  including eight compiled negative controls and real Windows protection. The
  controlled POSIX/Mach model passed on Windows. All 906 installed compiler and
  73 historical migration/repository checks passed. Release/native checks,
  generation consistency and isolated Story movement/firing/pause/resume passed;
  26 protected files were unchanged. All 17 CI checks at e90b9f7e passed.
  Native Mac title gameplay remains unverified. See
  [STATUS](STATUS.md#project-native-memory-ownership-2026-10-28).

### MAC-114 - Project guest memory services

- **Status:** Awaiting validation
- **PC behavior:** All 25 guest Mm bridge bodies use project ownership/policy,
  validated guest output spans and explicit advisory semantics. Five remaining
  native statistics/physical/hint bodies use original adapters. Guest statistics
  are nine little-endian words; native pointers remain full-width.
- **Source/PR:** `src/runtime/guest_memory_services.c`, `memory_statistics.c`,
  headers, `src/native/host_memory_stats.c`, original compiler fixtures and
  `patches/compiler/0211-independent-guest-memory-services.patch`. PR #375;
  implementation based on e90b9f7e.
- **Mac support present:** Shared services reuse the existing allocator and
  guest VM resolver. The host adapter uses Mach availability counters, handles
  shorter returned record revisions and releases its host port. This avoids the
  old Darwin compatibility path's uninitialized memory-statistics fields.
- **Remaining adaptation:** Main guest mappings, native Nt host callbacks and
  other kernel/device services remain separate replacement work. Advisory/debug
  results, IO scratch buffers and GPU tail-address behavior remain explicit
  compatibility limits; see [contracts](../agent-context/guest-memory-services.md).
- **Acceptance:** Windows candidate 25 checks passed, including 16 compiled
  negative controls, real Windows availability and controlled Darwin cases.
  Windows Release build/native checks/crash symbols and isolated Story movement,
  firing and pause/resume passed; all 26 protected files were unchanged.
  All 931 installed compiler and 21 documentation/context/backlog checks passed.
  All 73 historical migration/repository checks passed. New-commit shared CI and
  native Mac title gameplay remain pending. See
  [STATUS](STATUS.md#project-guest-memory-services-2026-10-28).
