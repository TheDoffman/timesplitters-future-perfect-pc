# Optional graphics settings (in development)

Acceptance requirement: every graphics setting, the FPS/online-ping counter,
and Clear Shader Cache must be available in both the in-game settings (main
menu and pause) and the launcher on Windows, macOS, and Linux. Both interfaces
must preserve the same preferences between launches. Apply changes live where
supported and explicitly label settings requiring restart. FPS/Ping shows only
FPS and network latency in milliseconds during online play, not frame time.

## Required settings parity

The task is not complete until every row below can be changed from both
**main menu and pause menu -> Video Options** tabs and the launcher's
**Graphics settings**. This includes Windows, macOS, and Linux; implementation
and native validation must be reported separately for each host.

| Control | Choices / behavior |
| --- | --- |
| Multisample antialiasing | Off, 2x, 4x, 8x; supported fallback; restart notice where needed |
| Edge smoothing | Off, FXAA (or SMAA); usable with MSAA |
| Anisotropic filtering | Original, 2x, 4x, 8x, 16x |
| Ambient occlusion | Off / On; half-resolution processing |
| Shader cache | Off / On, plus Clear Shader Cache action |
| FPS / Ping | Off / On; FPS plus latency in ms during online play only |

Both entry points must read the same saved choice after reopening. In-game
changes apply live where supported. Restart-required controls must say so;
unsupported hardware must retain the requested preference while using a safe
fallback. Verify setting round trips in both directions, failed-save behavior,
and preservation through game/launcher updates. No F1-only controls satisfy
this requirement.

## Current implementation

`src/graphics/graphics_options.h` and `graphics_settings.h` own the versioned
`TSFP_GRAPHICS_V1 msaa edge anisotropy ao cache` record in `pc-graphics.ini`,
beside the display configuration. Defaults are `1 0 1 0 1`: effects disabled,
original filtering, cache enabled. FPS/Ping uses `debug.show_counter` in controls
JSON. No saves or personal settings are migrated. Both interfaces persist these
same files; launcher saves participate in its transaction, concurrent-change
checks and rollback. Invalid files are not silently overwritten. Native setters
publish a complete snapshot only after a successful atomic save.

`src/interface/pc_video_tabs.inc` supplies the persistent top tab strip in both
main/pause Video Options. Display contains aspect ratio, resolution and frame
rate; Graphics contains effects, filtering, cache and texture style; View & HUD
contains vertical FOV, subtitles, Kill Feed and FPS/Ping. Click a tab, or focus
the strip and use left/right (Confirm cycles forward). Back returns to Settings.
Sections reserve the same native row count so the tabs stay in place. The active
tab has a persistent blue fill independent of setting-row focus.
`src/interface/pc_graphics_menu.inc` supplies the Graphics tab's controls.
Native changes save immediately; failed writes restore the selector. The launcher
uses its normal Save workflow. MSAA requires restart and is labeled accordingly;
FXAA, AF, AO and the counter update live. Cache operations re-read the preference.
Switching tabs retains pending display/texture/subtitle changes. Apply still
uses the original timed resolution confirmation and rollback. Effects, cache,
Kill Feed and FPS/Ping keep their immediate persistence behavior.

### Antialiasing and filtering

Patch 2495 connects final-resolve FXAA and live AF policy to shared preferences.
FXAA operates in output pixels after the existing supersampling filter. The
production-resolve test checks changed edges, unchanged flat areas, exact OFF
restoration and failed-save isolation. FXAA can be combined with MSAA. The production sampler fixture also verifies
1x/2x/4x/8x/16x policy changes, immediate restoration to Original and preservation
of nearest-filtered HUD/font textures (`test_render_state_cache.py`).

Patches 2497/2499/2504-2511 implement MSAA scene and guest-surface ownership.
`msaa_targets.h` allocates matching color/depth attachments, chooses the highest
supported requested-or-lower count, and preserves the old allocation on failure.
The saved request remains unchanged when hardware requires fallback. At one
sample there is no extra resolve image. D3D11 and Vulkan resolve on the GPU.

Guest textures stay single-sampled, with per-surface coverage companions in
`msaa_color_surface.h`. Binding resolves the previous attachment and seeds the
next; shared depth history survives color-target changes. Resolve targets the
correct cube face/mip. Image/subresource aliases synchronize surface locks, CPU
uploads and screen copies; partial writes preserve untouched pixels. Surface
locks read back the GPU. Parent texture locks retain their existing CPU-upload
buffer semantics. Resize, release and interpolation use the same ownership rules.

### Ambient occlusion

`ssao_shader.h` / `ssao_pass.h` implement half-resolution SSAO with a deterministic
32-tap spiral, reconstructed camera-space positions, depth-derived normals,
tangent bias and radius falloff. A depth-aware full-size composite preserves
alpha and rejects foreground/background bleeding. Odd sizes round up. MSAA depth
uses the nearest sample. Viewport origin/extent bounds both sampling and shading,
including split-view rectangles. Defaults in `ssao_settings.h` use radius .75, bias .025 and strength 1.5;
broader artistic tuning remains subject to gameplay review.

Patches 2500-2502 submit AO after world rendering and before HUD drawing through
`pc_graphics_world_finish` (US 1CE9A0). Deferred submission copies the 16 projection
floats; capture v10 opcode 32 stores them, with v5-v9 read compatibility. Replay
checks exact payload sizes and handles unaligned input. Invalid/orthographic
lenses and offscreen targets are excluded. Device teardown frees AO resources.
Patch 2513 declares Vulkan texture slots 10/11 used by AO compositing; the Linux
pixel test caught this omission even though all shaders compiled successfully.

### Cache and counter

Patches 2496/2498 implement persistent shared shader caching and clear controls.
The V2 envelope bounds keys/blobs, checks full keys and payload integrity, and
rejects truncated/trailing data. Compiler/backend ABI tags invalidate stale
entries. Patch 2503 protects Vulkan pipeline caches: the envelope
checks size/checksum and driver vendor/device/UUID before submission. Temporary
writes use process/thread identities on Windows and mkstemp on POSIX.

Clear removes recognized shader blobs and pipeline files only, preserving unknown
files and nested folders and rejecting links/reparse points. It suppresses stale
in-memory Vulkan pipeline-cache rewrites for the current process. Disabled effects
retain the original rendering path; disabling disk cache does not require removing
already-created in-memory shaders.

FPS/Ping displays presented FPS and fresh online latency in milliseconds, with no
frame-time figure. Offline/LAN latency is hidden. Tests cover missing peers,
five-second stale-sample expiry and recovery; this is formatter/clock evidence,
not a real internet-peer test.

## Platform coverage and limitations

Windows uses the shared D3D11 path. Native Vulkan uses the same HLSL/algorithms.
Patch 2512 exposes common renderer declarations under TSFP_RHI_RENDERER.
`src/native/vulkan_probe.cmake` links xbox_config for cache paths.
The legacy OpenGL adapter (`graphics_preferences_gl.inc`) preserves preferences
and supplies cache controls, but explicitly labels graphics effects as requiring
Vulkan. These effects are not implemented in legacy GL.

Linux host integration was tested against `feature/linux-launcher` revision
802391863019b651dbb20e1e7625bd86e6153395 in an isolated copy, retaining its Linux
adapters. This graphics branch does not yet contain that branch's entire Linux
foundation; shared UI/model guards support MACOS and LINUX. The isolated source
export compiled its launcher but lacked Git metadata for the packaging receipt.
Do not treat this as a release-package validation or a merge of that branch.

| Validation | Evidence and scope |
| --- | --- |
| Windows game | Full build and built-in checks passed; isolated first-mission gameplay with 4x MSAA, FXAA, 16x AF and AO |
| Windows rendering | WARP shared RHI pixel tests, production FXAA resolve, AO queue/capture/replay, cache integrity and alias/copy regressions pass |
| Windows UI | Main and pause Graphics menus visually checked; 640x480 main menu fits; live AO change persists after reopening; WPF launcher panel fits at 1000x900 |
| Captured scene | 509 draws and one AO command; AO OFF/ON replay has no failed calls; visible terrain/bridge shading, mean RGB decrease 2.16/255 |
| MSAA replay | Same scene at Off/2x/4x/8x reports actual requested counts; inspected edge changes with scene content preserved |
| Cache reuse | Isolated cold/warm Windows replays reuse disk entries; repeat-frame differences at most one channel level in seven pixels, also seen without cache changes |
| Linux WSL | Native preference/cache/AO-queue tests pass; 16 production DXC shader variants compile; full shared RHI suite passes on lavapipe software Vulkan, with isolated cold/warm runs confirming disk shader-cache reuse |
| Linux launcher | Shared Avalonia UI save/reopen suite and adapter/settings/install/texture-preservation tests pass under Xvfb |
| Not verified | Physical Linux GPU gameplay, native macOS builds/rendering/UI, full release packaging, all-map AO tuning, live split-screen AO placement and real online latency |

The Linux pixel suite covers AO contact shading, flat surfaces, sky, alpha,
viewport isolation and resize, plus MSAA samples, cube/mip resolves, copies,
companions and shared depth. It validates fallback on a device supporting 4x but
not 2x. This is stronger than syntax validation but is not physical GPU evidence.

## Repeatable checks

- Windows: `powershell -NoProfile -File tsfp.ps1 -Action Build`.
- Launcher: `powershell -NoProfile -File tools/build/build-launcher.ps1 -Test`.
- Graphics: `python -m pytest tests/graphics/test_rhi_menu_sampling.py tests/graphics/test_graphics_options.py tests/graphics/test_graphics_resolve.py tests/graphics/test_shader_cache.py tests/graphics/test_ambient_occlusion_queue.py -q`.
- Patch reproducibility: `tests/repository/test_patch_updates.py`, including fresh application and graphics-on/off branch round trips with LF/CRLF and unrelated local edits.
- Capture/replay: `tests/gameplay/test_frame_replay.py` and renderer-host fixtures.
- Native shaders: `VULKAN_SDK=<sdk-root> python -m pytest tests/graphics/test_graphics_spirv.py -q`; configured broken SDKs fail, absent SDKs/platform mismatches explicitly skip.
- Native Vulkan: standalone `tests/rhi` CMake target `tsfp_rhi_vulkan_test`, using the pinned SDK and applicable platform adapter.

Runtime CI selects the focused graphics checks. Keep native skips distinct from
passes. No generated runtime, private capture, personal configuration or save is
part of the source change. Native macOS and physical Linux acceptance must be
reported separately before claiming those hosts have been validated.

## Local completion audit (2026-11-02)

The requested shared implementation and Windows validation are complete locally.
This is not a release or a claim of native Mac/physical Linux acceptance.

- MSAA Off/2x/4x/8x: production scene allocation, per-sample averaging,
  subresource/alias/copy tests, capability fallback, and captured-scene matrix.
- FXAA with MSAA: production-resolve pixel checks plus combined-effects gameplay.
- AF through 16x: saved options, live sampler translation at every level,
  restoration to Original and nearest-filtered HUD preservation.
- Half-resolution SSAO: Windows and software Vulkan pixel tests, shader variants,
  deferred/capture/replay checks and one live-game scene comparison.
- Cache controls: persistent cold/warm Windows and Vulkan shader reuse,
  corruption/invalidation/clear containment tests, menu callback and launcher
  deletion helpers. The launcher requires a stopped game before clearing.
- Main/pause/launcher settings: shared persistence and failed-save checks;
  main/pause Windows visuals and WPF/Avalonia UI round trips.
- FPS/Ping: shared toggle, presented FPS, online milliseconds, offline/stale
  handling; actual internet-peer acceptance is not claimed.
- Reproducibility: fresh application, repeat application, graphics branch round
  trips with LF/CRLF and preserved local source edits; context/diff checks pass.

Final Windows checks: full game build and built-in tests; full launcher suite;
8 focused graphics tests; 7 renderer-state tests; 28 menu/renderer/replay checks
with 3 explicit platform skips; 2 graphics branch-transition tests. Physical
Linux and Mac checks remain as listed above. The separate Linux integration
worktree retains its four launcher adaptations; integrating the Linux foundation
into this feature branch or main has not been authorized or performed.

### Tabbed-menu follow-up (2026-11-02)

Windows x64/D3D11 on base 54c06a4c plus local changes: full supported Build and
built-in checks pass. Twelve focused menu/settings tests pass, with two native
Mac compile cases explicitly skipped. Tests cover section membership, tab
cycling/hit boundaries, pending changes, failed writes and resolution rollback.
Live isolated 640x480 main-menu checks confirm all three tabs fit, retain their
position and active highlight, accept mouse selection, and update FPS/Ping live.
Story gameplay was reached. Live pause navigation could not be completed because
synthetic keyboard input did not register reliably in this session; this is not
evidence of a diagnosed game-input regression. Both entry points use the same
shared Video menu. Physical controller, Mac, Linux and widescreen tab acceptance
remain unverified for this follow-up. No commit, push or publication performed.

### Compact tabs and restart handoff (2026-11-02)

Video Options uses native font scale 1.6 instead of 2.0. Rounded tab backgrounds
use disjoint strips, and captions are centered using the guest font measurement.
The shared menu prompts Later/Restart Now after a successfully saved MSAA change;
Later is the default and Back cancels the prompt. It warns that restarting leaves
the current game. Failed persistence restores the selection without restarting.
`src/interface/pc_game_restart.inc` preserves command arguments, working directory
and the launch environment. Windows passes an inherited synchronization handle
using a restricted handle list; POSIX passes a pipe and waits for parent exit.
Early bootstrap (patch2514 and native main) waits before opening saves/devices.
Spawn failure leaves the current game open. Windows native close / SDL quit remain
the owners of normal shutdown. A 60-second startup wait fails closed.

Windows supported Build/built-ins pass; focused checks: 13 passed, 2 Mac-only
skips. A real two-process fixture checks shutdown ordering, duplicate requests,
arguments containing spaces and inherited save routing. The same POSIX fixture
passes with GCC -Wall -Wextra -Werror on Ubuntu24.04 WSL; this is not Linux GPU
or Mac validation. Live ultrawide main Graphics layout fits inside the theme.
The user is testing the game; full in-game Restart Now and native Mac remain
unverified. No commit/push. New reported AO-like weapon/geometry halos are under
investigation and supersede earlier visual completeness claims.

### AO silhouette regression (2026-11-02)

User isolated the black gun/terrain outlines by turning only AO off. The original
50-unit radius lets separate foreground geometry darken a distant backdrop. The
production contact radius is now .75, bias .025 and strength 1.5. The exact game
defaults live in `ssao_settings.h` and are used by the GPU fixture: a foreground
plane at depth2 must not shade the separate plane at10, while nearby 1.85/2
contacts must retain shading. Both even/odd targets and compositing are checked.
The new fixture fails with the old production radius and passes with the revised
defaults. This corrects the contact range; it is not a new world-occlusion system
or a dedicated first-person-weapon mask.

Windows full Build/built-ins and 13 focused tests pass (2 Mac-only skips).
The earlier 640x480 bridge capture replays with AO on/off, zero replay errors;
new maximum per-channel difference is8/6/4 levels versus the old recorded max70.
This capture has no first-person weapon. The actual user-reported gun/canyon view
still needs visual confirmation in the rebuilt game. Another desktop launcher
session is running; it was left untouched. Native Mac/GPU Linux remain unverified.

### Multi-frame transition regression (2026-11-02)

`tests/graphics/graphics_transition.h`, called by the production display-resolve
fixture, keeps color, MSAA companion and AO targets alive across six frames:
world, full-white flash with cleared depth, two fading frames, and world recovery.
A separate bright region stands in for a weapon; every pixel is checked, including
background and viewport edges. This detects black output, stale resolves, missed
full-screen coverage and background loss while the foreground survives.
The fixture uses real MSAA seed/resolve, SSAO render/apply and FXAA display resolve.
It is synthetic; it does not execute the guest explosion or its draw ordering.

Windows WARP: 48 combinations (1/2/4/8 samples, AO off/on, FXAA off/on,
4:3/16:9/ultrawide small targets) passed across six consecutive frames, repeated
in separate cache-disabled, cold-cache and warm-cache processes. Cache files are
isolated via subprocess APPDATA, populated on cold startup and checked unchanged
on warm startup. Existing shader-cache corruption and RHI/AO checks also pass
(four pytest cases). This is 864 pixel-checked frames, not an FPS benchmark.
The existing CI test_graphics_resolve.py entry automatically includes this check.

The reported blackout is at the later canyon ship explosion during gameplay,
not the opening cutscene. Cause remains unconfirmed; MSAA is a hypothesis, not a
finding. The new sequence passes, so no additional renderer change was justified
by it. Live reproduction at 60/120/180/240 FPS, real display resolutions, AF
combinations, and Mac/Linux GPU acceptance remain outstanding. Desktop automation
returned a stale game-window handle twice while opening an isolated 240-FPS-target
Test Lab run; no canyon verification was obtained. Do not describe this test as
proof that the reported blackout is fixed or that all enhancements are safe.

Before publishing the graphics feature, retain an actual explosion sequence with
all effects off, each effect separately, and combinations that reproduce it.
Inspect the flash and following world/weapon frames, not just a final screenshot.
Repeat the reproducing combination after repair at the supported FPS settings;
record measured rates and backend. Missing live/platform evidence stays pending.

Merge validation: the Windows Release rebuild and built-ins pass with latest main
(a1f039cb). The 127 CI planning/context checks pass locally after updating the
patch-order contract and normalizing patch2514 to LF. Historical migration fixture
sets now exclude graphics2495-2514 until the complete-stack upgrade; injecting
these new patches into a pre-display/pre-feed checkpoint was not a valid old
installation. Complete current-stack and graphics round-trip coverage remain.

POSIX restart posts SDL_EVENT_QUIT directly. This allows the standalone replay target to link the display module without game-only input symbols; the real process-handoff fixture verifies the SDL quit request.

Native Mac GPU CI327eb4c reproduced visibility0.874528 on a z=2 foreground plane at half-target pixel33 in the65x33 scene, with a distant z=10 background and radius0.75. Shared SSAO now explicitly rejects equal raw-depth taps and distances outside the radius before angular weighting, and bounds reciprocal-square-root input. Existing odd/even flat/sky/foreground and close-contact assertions remain unchanged. Exact compiler/driver cause is not established; native Mac rerun is required.
