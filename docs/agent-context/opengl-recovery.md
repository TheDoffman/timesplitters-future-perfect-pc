# OpenGL startup recovery: implementation in progress

Goal (amended by the user on 2026-11-22): Vulkan remains the default, and the
launcher must offer Vulkan and OpenGL so a user can explicitly choose OpenGL
when needed. This supersedes the earlier requirement for no launcher selector.
Automatic recovery remains limited to Vulkan startup failure on accepted hosts;
explicit OpenGL selection must start OpenGL directly. D3D11 is not restored.
Branch feature/opengl-startup-recovery is based on local4c287926 (Windows fallback
removal), which follows main694fc53a. No goal completion or universal readiness
is established by this checkpoint. Keep all platform recovery gates disabled.

## Launcher selector acceptance added to the goal

- Restore a player-facing Vulkan/OpenGL selector using shared launcher settings
  and launch policy, with thin host adapters. Keep Vulkan selected for new or
  missing preferences; preserve a user's saved choice across launcher restarts
  and updates without changing unrelated settings, saves, or installed textures.
- Pass the selected backend to the game at launch. A deliberate OpenGL choice
  must not first attempt Vulkan; Vulkan retains the separately gated startup
  recovery policy. Show a clear error when the chosen renderer cannot start.
- Verify both choices through the actual launcher on available hosts, including
  persistence, switching back, correct backend logs, menu/gameplay progress,
  input and window handling. Add meaningful shared settings/launch tests and
  record unavailable native-host checks explicitly.
- Update player documentation and launcher compatibility/context records with
  the implementation. The amendment itself did not implement the selector;
  implementation evidence is recorded below. All existing graphics and platform acceptance work
  remains required; manual selection does not approve automatic recovery.

## Shared startup policy

`src/graphics/rhi_startup.h` owns one transaction for every OS. Patch3052 routes
production rhi_device_create through it. It destroys failed primary resources
before trying OpenGL, changes g_rhi only after successful recovery, destroys
failed OpenGL resources too, and logs both-failed versus disabled states. It
never changes saved configuration or recovers from later presentation failures.
It requires RHI_HAVE_OPENGL plus TSFP_OPENGL_RECOVERY_ACCEPTED; the latter
defaults to0. The shared OpenGL RHI is compiled beside Vulkan by ordinary builds through
patch3054, but automatic recovery remains unapproved. Explicit developer
RECOMP_D3D8_BACKEND=opengl does not change saved preferences. Tests use
resource-owning spies to cover order, disabled hosts, missing backend, both
failures, explicit-GL failure without recursion and descriptor preservation.

## Shared shader compiler and context boundary

`opengl_shader.cpp/.h` reuses production HLSL and DXC's register/layout policy.
Patch3053 factors one compiler and exposes reflection metadata only for GL;
ordinary Vulkan compilation retains its original flags. SPIRV-Cross emits GLSL
410, semantic-named varyings (a pixel shader can consume a subset), independent
texture/sampler register mapping, DX-layout uniform blocks and vertex inputs.
Shader metadata tells the backend which GL blocks/texture units/attributes to
bind. Offscreen GL bottom row is RHI logical top: clip Y flips in the vertex
shader, data upload/readback remain unchanged, and final screen blit must flip Y.
Depth is remapped from D3D/Vulkan0..1 to GL-1..1. The Windows hardware suite verifies asymmetric pixels, depth/stencil, culling,
array/cube/volume images, MSAA, resolves and screen copies. Canonical wide
interstage wires allow a pixel shader to consume fewer vector components;
textureSamples and textureQueryLevels are lowered to bound-view count uniforms for GLSL410.

`opengl_host.c/.h` is the small window/context boundary: WGL4.1 on Windows,
SDL GL on native hosts, function lookup and swap interval. It does not own the
window or save settings. The native owner now stages a replacement GL window through
opengl_window_sdl.h, commits event/input/settings bindings only after required
window calls succeed, and preserves the original opaque RHI window token for
size queries. A failed staging destroys only the new window. Context and SDL
GPU calls dispatch to the main event thread; CPU-only state uses the same
serialization without GL. Actual native event servicing, fullscreen transitions,
logical identity and debug/input behavior remain unverified on-host.

SPIRV-Cross is pinned to SDK1.4.363.0's commit in config/build/spirv-cross.json.
`python tools/build/spirv_cross.py --setup` verifies the2MB source archive and
installs only needed sources/headers/LICENSE into ignored external/spirv-cross.
Normal verification checks installed file hashes. `spirv_cross.cmake` compiles
the same static implementation for each host; no shader-generation fork or
extra compiler process per shader. Windows Setup and native common-renderer --setup install it; ordinary builds
verify before compiling. All three package writers retain its exact license,
pin and receipt. macOS retains a separate driver SPIRV-Cross notice too.

## Checkpoint evidence and next work

The shared startup transaction also passes GCC -O2 under Ubuntu24.04 WSL; this
is a policy test, not Linux GPU/game acceptance.
Windows11 x64, RTX4070SUPER/617.42, MSVC19.44 and SDK1.4.363.0:
`opengl_backend.c` now supplies the complete RhiBackend table, compiled through
`opengl_backend.cmake`. `opengl_api.h` loads core4.1 procedures plus optional
storage/view extensions. Buffers, image/view ownership, shared shader programs,
state/stream bindings, outputs, readback, resolve, queries and flipped screen
presentation are implemented once. Resources bound into state retain references;
failed device setup and repeated cleanup are safe. This is not game acceptance.

`tests/graphics/rhi_vertex_streams.h` adds shared indexed/nonindexed stream,
normalized attribute, index/base-vertex offsets, packed depth, BC array layer,
front-face/cull and five sampler-address contracts. Vulkan intentionally rejects
raw depth readback; its sampled-depth coverage remains separate. Readback must
finish preceding GPU queries even when color writes are disabled.
`test-rhi-windows.py --backend opengl` runs the full production RHI contract on
GL; normal invocation still runs the original8 cases. `--opengl-recovery` runs
five cases in an explicitly accepted fixture: Vulkan success, missing driver,
no suitable Vulkan device, both renderers failing, and GL partial-init failure.
The last two require explicit both-failed diagnostics and safe repeated cleanup.
Build with TSFP_TEST_OPENGL=ON; only the recovery fixture enables
TSFP_TEST_OPENGL_RECOVERY. This is not a production acceptance override.

All five recovery cases passed (fixture SHA256
ce2c16bf4582f2243f2b9528d310f634ca09547772cf584195d636996f2eafd6).
All8 original cases and explicitGL passed with recovery OFF (fixture SHA256
f73cdea03982e02716cc4c0dacab727b66db164324717f1b8497247959fa45de).
Vulkan success cases used active SDK validation. Reports: logs/rhi-windows.json,
rhi-windows-opengl.json and rhi-windows-opengl-recovery.json. Standalone shader
translation and real WGL pixel probes also pass with the updated shader adapter.
Sixteen selected Python policy/installer/renderer checks passed. Test-only failure
environments use child-process overrides; no installed driver or personal setting
is modified. Production recovery remains OFF on every platform.

Remaining implementation/acceptance: efficient context scheduling and actual-game
acceptance; native Vulkan-to-GL window replacement with event/input/debug bindings and stable
logical window identity; native builds and GPU execution; representative game
menus/gameplay/effects, display modes/resizing, settings preservation, bounded
performance/stability and explicit per-platform acceptance. The current single-sample subset/stencil adaptation is described below; native
GL4.1 still requires actual host capability and performance acceptance. Other review targets:
null non2D resources, interstage arrays, depth-bias clamp and native mirror-once.
Windows driver extensions do not establish macOS4.1 capability. Mac OpenGL is
deprecated; preserve existing legacy-Mac behavior and do not claim future OS
support. Existing legacy frontend is src/native/renderer_gl.c; recovery uses the
common frontend rather than promoting that separate incomplete frontend.

Local shader commands:

    python tools/build/spirv_cross.py --setup
    cmake -S tests/opengl-shaders -B build/opengl-shaders [host generator]
    cmake --build build/opengl-shaders --config Release --parallel 2
    build/opengl-shaders/Release/tsfp_opengl_shader_test.exe build/opengl-shaders/generated
    build/opengl-shaders/Release/tsfp_opengl_pixel_test.exe build/opengl-shaders/generated

Create the generated output directory first. Native builds require the verified
TSFP_VULKAN_SDK and SDL3 package paths; executables omit Release/.exe on native
single-config generators. These native commands have not been run here.

## Ordinary build and first game run (after58d450d9)

Patch3054 adds opengl_backend.cmake to common Vulkan game builds; legacy Mac GL
builds do not acquire this dependency. Setup verifies the same static source pin
on every host; package writers copy Licenses/SPIRV-Cross-OpenGL.txt plus the
Graphics pin/receipt. Native builds/packages have not been executed on-host.
Windows normal tsfp.ps1 Build passed with MSVC and all mandatory build checks.
123 selected Python tests passed, with9 host-dependent skips; package fixtures
assert the exact new notice and provenance files are retained.

The first isolated game probe exposed a real context handoff: OpenGL was current
on the creation guest thread, while a later clear ran without a current context.
`opengl_backend.c` now serializes each public RHI entry and acquires/releases a
context lease through opengl_host.c (SRW lock/WGL or pthread/SDL). Internal g_*
calls stay in the lease. The GPU fixture tests worker clears/readback/present
while the creation thread stays alive, then hands rendering back to that thread.
Optional KHR_debug errors are logged as renderer errors. This baseline favors
correct thread ownership; its per-operation context switches need optimization.
Do not assume this fixes all native thread/event constraints.

ExplicitGL asset-free contracts pass. The ordinary game SHA256
078bd47d0c40df65663bee6ec6cca17edec86404b47bb35abe73a5f0dec82442
was tested for120s using native_refresh_game.py --backend opengl --fps60
--windowed --stress --pause-check --capture --label gl-lease-01. This **failed**
gameplay acceptance: it reached Story/opening geometry, presented without renderer
errors and observed pause/resume, but firearm handoff, movement, animation and
ammunition checks failed. Captured opening geometry/text is upright and textured;
that is limited scene evidence. Late reported presentation was about6-7 FPS.
This capture-enabled run is not a clean performance benchmark, nor proof of the
cause of slow progression. It ended at the runner's bound (terminated, exit1),
not a tested clean game exit. Report and three retained frames are under
private/test-saves/high-refresh/60-gl-lease-01-opengl-*. Separate compiler latency,
context handoff overhead and game progression before enabling recovery.
No personal saves/settings/textures were used or modified. All production gates
remain OFF. Native SDL window replacement and GL4.1 capability adaptation,
actual game effects/display/settings/performance/stability acceptance remain.

After context leases: all five accepted-fixture recovery cases passed again,
including the new live-creation-thread handoff check in recovered GL. All8
original recovery-OFF cases and explicitGL passed; Vulkan validation stayed active
on the positive Vulkan paths. Historical-controls-to-current-stack upgrade passed
with patch3054 omitted only from the historical input, restored by full upgrade.
Recovery acceptance was restored OFF after tests. Current fixture hashes:

- rhi-windows: `6cc59cc17f29d5d1c5453f1cedd6b554bf70200deeca847879574fec8545a57e` (passed).
- rhi-windows-opengl: `6cc59cc17f29d5d1c5453f1cedd6b554bf70200deeca847879574fec8545a57e` (passed).
- rhi-windows-opengl-recovery: `5bb4c4a33d9aaf8bb021b708ae992e38be99ae85d390f5ae8608869cfd09a6c3` (passed).

## Context worker and native window transaction (after886eb735)

The old per-call leases were correct but expensive: a temporary WGL timing probe
recorded22748 binds/430ms by100 presents and706358/11977ms by400 presents during
a20s menu run. Removing CPU-only binds improved throughput, but its120s gameplay
run gl-lazy-01 still reported about12-19 FPS late. These probes changed the next
action; their temporary timing instrumentation is not shipped.

Current shared scheduling: `opengl_calls.inc` supplies typed synchronous host
thunks. CPU-only binds/refcounts modify serialized state without GL. Final
buffer/image/view/shader/sampler releases enqueue embedded retirement nodes;
the context owner drains them before GPU operations and at destruction. This
preserves bound-resource lifetimes and program invalidation without driver calls
on guest threads. WGL retains a context on one dedicated worker; stop joins it.
SDL retains its context on the main event thread and dispatches GPU operations
there through SDL_RunOnMainThread. Never wait on guest rendering from the native
main thread while it needs to service queued SDL callbacks; native event-loop
acceptance remains required. A Windows pass does not establish that property.

The Windows GPU fixture now exercises simultaneous resource create/clear/readback/
retirement from two guest threads, context/worker recreation on the same window,
and repeated destruction. All5 actual recovery cases, all8 recovery-OFF cases
and explicitGL pass with this scheduler; positive Vulkan paths use validation.
Reports: logs/rhi-windows*.json. Acceptance restored OFF. Latest fixture hashes:

- rhi-windows: `2eebeec95ee8de6e7632111aba8cf08da1776ab3779298b771b0da0a17d81dd7`.
- rhi-windows-opengl: `2eebeec95ee8de6e7632111aba8cf08da1776ab3779298b771b0da0a17d81dd7`.
- rhi-windows-opengl-recovery: `b3a45751d382f815069c9528244174698ec637bcc9bce2052f55c5c67ed3ca42`.

Two120s ordinary-game runs on Windows11/RTX4070SUPER617.42 pass first-mission
progression, firearm handoff, movement, animation, firing and pause/resume without
renderer errors: gl-worker-01 (resources/frame intervals) and gl-worker-visual-01
(capture only), executable SHA256
c291b08cfa97d97b54811a0a1e6b588f8700d9f3695752759df0fa58a6f476a9.
In the non-capture85-115s window,1800 CPU presentation intervals had16.666ms
median,17.179ms p95 and17.880ms max; these are not scanout/GPU timings. Late
resource samples held550 handles,359530496-367816704 private bytes and
267882496-269619200 working-set bytes. The last private sample was359530496;
this bounded window is not a leak-free or long-soak claim. Captured operation
reported about59.6FPS, with readback overhead. Both sessions were terminated at
the runner's bound (exit1), so clean exit is still unverified.

Reviewed private/captures/opengl-recovery/worker-gameplay.bmp shows first-level
world textures, fog, held weapon, tutorial text, reticle, ammunition HUD and
impact sparks upright. SHA256
7ac155a6ed27a4013c2ebfe4981c2d64481be3df67d9091f558f3b1337bfad00.
It is an offscreen guest-frame capture, not final desktop presentation proof.
Keep display/fullscreen/resizing, graphics-quality/effect matrices, representative
additional scenes, actual recovery game launch, settings/texture preservation and
bounded longer stability/clean exit as separate acceptance work. No production
platform is approved yet.

Native adapter source now stages4.1 core/doublebuffer/depth/stencil windows,
preserves logical size/title/position (when queryable), fullscreen mode, density,
resizability and visibility/maximized/minimized state. Commit rebinds the host,
releases old input capture and updates settings/debug before destroying the old
window. Wayland can leave coordinates to its compositor. No saved preference is
written by this transaction. tests/graphics/test_opengl_window.py compiles the
real helper against injected SDL operations and covers every staging failure,
commit failure, success, unavailable position, hidden/maximized/minimized, already
GL and non-main calls. Registered in the native CI suite. MSVC and WSL GCC pass;
shared GL plus native adapters pass GCC syntax checks.136 selected Python tests
pass with9 skips. These are not native SDL/GPU/game acceptance. Native event
servicing, native build/package execution and required4.1 resource-view/stencil/
mirror-once adaptation remain; Linux/Mac gates stay OFF. Existing legacy Mac GL
still uses its original frontend, and Apple's OpenGL remains deprecated.

## OpenGL4.1 views and Windows display/stability checkpoint (after33e5a469)

The common backend now copies sampled subset views into versioned private
single-sample images when texture views/immutable storage are unavailable.
This covers array layers (including one-layer arrays), cube faces, volume mips,
compressed blocks, packed depth and integer stencil. Uploads, clears, draws and
resolves invalidate snapshots. Refresh completes before binding all samplers,
preventing later copies from disturbing earlier bindings. Copies use bounded
staging (512MiB maximum); they may cost more than driver texture views. Requested
MSAA remains saved, but active rendering is single-sample if the driver cannot
provide required multisample views and stencil sampling. Overspecified mip chains
are rejected. Shader sample-count and mip-count queries lower to bound-view
uniforms, avoiding later GLSL textureSamples/textureQueryLevels requirements.

`tests/graphics/rhi_gl41_views.h` verifies source changes, multiple samplers,
GetDimensions, array/cube/volume/depth/stencil and compressed views. Run
`test-rhi-windows.py --opengl-gl41` to deliberately disable those view/storage
extensions in the fixture. Its report/log have distinct gl41 names. This test
omits multisample contracts explicitly; normal explicitGL tests retain them.
NVIDIA still supplies other extensions: this is not native Mac4.1 acceptance.
Standalone shared-HLSL translation, generated GLSL syntax and both GL suites
pass after the query lowering. Ordinary Windows Build passes as well.

Windows11 x64, MSVC19.44, RTX4070SUPER/617.42, SDK1.4.363.0; source HEAD33e5a469
plus this checkpoint's changes. All runs use isolated Test Lab saves/settings,
verified US XBE and child-only overrides. The nonshipped acceptance harness now
supports an explicit developer OpenGL StartInfo probe; this is distinct from the
unchanged launcher's Vulkan LaunchGame path. No player renderer selector exists.
The harness drains shared child logs, verifies foreground game identity around
DXGI desktop capture and preserves requested graphics values/default Vulkan.
It checks every logged scene size/MSAA pair, and AO requires an actual successful
render/apply diagnostic rather than a saved checkbox. Synthetic input is scoped
to the test game. Captures and resource-measurement runs remain separate.

- gl-display-auto-03:120s PASS, Auto/MSAA4/SMAA2/AF16, clean exit0. Verified desktop
  presentation at640x480,1920x1080 fullscreen,1008x729 resized and640x480 restored.
  Reviewed frames show upright world/fog/weapon/tutorial/reticle/ammunition HUD.
  Game SHA256 b17ad140d55d446541da06e90ab9ca85e22ce740a45ef8b6c92fff2d7e5b04a5.
- gl-quality-ao-fxaa-02:120s PASS, startup1920x1080 fullscreen/MSAA8/AF16/AO1,
  actual SSAO applied at8 samples/1920x1080, clean exit0. FXAA1 is requested and
  preserved; independent FXAA application is not established by a separate log.
  Reviewed desktop frame is upright with world/weapon/HUD. Game SHA256
  fda24e029946c1a35ed1ccbf318ca3d7abfb1f20e79e7419fa14b0e272e463f2.
- vk-primary-control-01:120s PASS via actual unchanged launcher, Vulkan gameplay,
  settings and clean exit0; same fda24e... game. No recovery was invoked.
- gl-soak-01:300s PASS after latest query lowering. Game SHA256
  3426c47c19d61fd95f25a596fea8ca11c50884d9c0595c2bcf5582b3da3c75d4.
  Movement, weapon, pause/resume, last30s active gameplay/60Hz clock, bounded
  private/GPU memory/handles and clean exit0 pass. Median growth between sampled
  windows: private4734976 bytes, handles-6, dedicatedGPU-593920 bytes. The85-115s
  window reports59.98 guest ticks/s and553 handles. These are guest/CPU/resource
  measurements, not scanout/GPU timings or proof against slower leaks.

Reports, logs and retained desktop PNGs: logs/renderer-validation/<label>/.
Earlier gl-display-fixed-01 failed navigation; fixed-02/auto-01 captures were
occluded by test UI and are not visual acceptance. Auto-02 correctly rejected
lost foreground. gl-quality-ao-fxaa-01 failed startup-fullscreen due to the probe's
background flag; the corrected probe's later run above supersedes it. Preserve
these distinctions rather than relabel failed trials as accepted evidence.

Production recovery is still OFF on every host. Remaining: automatic recovery in
an ordinary-game launcher trial with forced Vulkan failure and both-fail proof;
additional representative scenes; explicit installed-texture preservation;
null non2D resources and relevant capability review; native build/package/GPU
execution, main-thread event servicing, mirror-once/compression compatibility,
quality/display/performance acceptance. Linux WSL build work is available but not
yet completed here. Native Mac hardware is unavailable. Legacy Mac frontend is
unchanged; Apple OpenGL is deprecated. Windows checks cannot approve native hosts.

Latest query-lowering contract recheck: all5 accepted-fixture recovery cases
and all8 ordinary recovery-OFF cases pass; acceptance restored OFF. ExplicitGL
and restricted4.1 also pass. Ordinary/explicit/restricted fixture SHA256
`a2b5aaf9bee2f0abc5fdd1fa3f80cd24ae3b28ad46dafa54925ac6961bb717f3`;
recovery fixture `3e8a4ff7fae2044e98f42c2fed2fc06126bd4c9fb8efb1017d235a43c8e9b220`.
Positive Vulkan paths also pass the final recheck with active SDK validation;
the recovery fixture was rebuilt with acceptance OFF afterward. Those final
rebuilds have distinct hashes: ordinary `77266c99780686c7fcf31ea0cf0e3308d7db652adc8862751e65e7d2d7ac59bb`,
recovery `b8d3410541fe8cc829e5c33582ef4a189bb165462c4b486dc646f331ef3c0760`.
The earlier explicitGL/restricted4.1 hashes above describe their actual runs.

## Actual launcher recovery and terminal failure (afterac4fb17d)

The opt-in Windows runner accepts --startup-recovery opengl|both-fail only with
its actual Vulkan launcher route. It inherits a nonexistent Vulkan driver path
in the test harness/child only; launcher defaults and saved renderer policy are
unchanged. Successful recovery requires one attempt/selection, the original
Vulkan failure reason, only OpenGL guest presents, gameplay/quality checks and
clean exit. Both-fail requires one attempt, both reasons, no selection/presents,
bounded exit with a nonzero code and byte-identical Settings files. Synthetic
installed custom DDS files are hashed before/after both routes; this proves
file preservation, not installed Enhanced-pack visual fidelity.

The trial uses a local ignored CMake project include, setting
TSFP_OPENGL_RECOVERY_ACCEPTED=1 solely on xbox_d3d8's rhi.c source. It does not
change tracked production eligibility. The normal game must be reconfigured
without that include and rebuilt after testing. Do not package the trial binary.
The full ALL_BUILD trial initially hit unrelated smoke targets lacking project
includes; the explicit futureperfect_recomp target builds successfully. The
supported tsfp.ps1 Build remains the ordinary full-build/check route.

A real both-fail probe exposed a missing integration step: HLE shadow_create
logged CreateDevice failure, destroyed its window, then let the console guest
run blind. Patch3055 terminates common-frontend startup with status2 after a
flushed explicit error; failed backend attempts were already destroyed by the
shared RHI transaction. It does not change the legacy-Mac frontend's historical
path. Raw RHI fixtures still return failure for repeated-cleanup tests.

Test injection also has a source-build detail: removing sandbox DXC DLLs does
not guarantee failure because the development executable has an absolute DXC
fallback. gl-launcher-both-fail-01 therefore timed out while GL worked, not an
engine failure. The nonshipped tests/launcher_lifecycle/DxcUnavailable.c fixture
exports DxcCreateInstance returning E_FAIL; only a copied DLL in the disposable
install uses it. No system/source dependency is renamed or modified. With this
injection gl-launcher-both-fail-02 exposed the blind-run defect; it is historical
failed evidence. After3055, gl-launcher-both-fail-03 passes in0.42s with exit2,
no presents, exact settings and installed custom asset preservation.

Before3055, gl-launcher-recover-01 passes120s through actual LaunchGame: Vulkan
vkCreateInstance failure, one OpenGL recovery, gameplay/movement/weapon,
MSAA4/SMAA2/AF16/actual SSAO, saved quality/defaultVulkan and clean exit0.
Game SHA256 0d2cad5ca70faaca6b6e2d8c9e1531ccdac63d9c884d0318b73e8605e2f2bdc3;
sourceac4fb17d plus uncommitted test harness and local trial definition.
Reports: logs/renderer-validation/<label>/. Later3055 game provenance and Scotland
acceptance are recorded after their runs, separately from this earlier binary.
All production platform gates remain OFF pending the remaining goal checks.

### Shutdown integration and final trial evidence

Scotland trials01/02 reached gameplay and completed fullscreen/resize but failed
clean exit with0xC0000005 immediately after the exit-trace stack heading. Trial03
added an AV guard around diagnostics, which passes injected unit faults, but still
failed in the game. Do not report those runs as overall acceptance. The exact
internal faulting instruction was not captured. Patch3056 now logs successful
exit status without walking stacks after CRT/driver teardown; nonzero exits retain
AV-guarded tracing. Only tracing is guarded. The Windows native test includes
production exit_trace.c, injects stack-capture/symbol/guest-stack faults, checks
requested0/2 outcomes, and confirms an AV outside reporting still fails. It is
registered in the native_windows CI suite. The guard alone was insufficient;
the normal-exit policy change is part of the validated fix.

After3055/3056, gl-recover-scotland-04 passes120s actual-launcher automatic
Vulkan-to-GL recovery, scene4 gameplay/movement/weapon, active4x MSAA/SSAO/SMAA,
Auto resizing, fullscreen/windowed/restore, installed custom DDS preservation,
saved quality/defaultVulkan and clean exit0. Game SHA256 `11c959a9e59c67274e93ecb7de3f0c9c7804a31d896c1a46095fd3a7870cc632`.
Reviewed DXGI desktop frames vulkan-0.png and vulkan-1.png are actually recovered
OpenGL: upright cave/wood surfaces, held pistol, reticle, ammunition HUD and bullet
impact; later movement approaches a wall. The filename reflects the launcher's
requested primary, not the renderer that presented. Captured display dimensions
and scene-target checks pass; these capture-enabled samples are not a benchmark.
The final log explicitly records normal exit with stack tracing skipped.

gl-launcher-both-fail-04 passes again with the same trial executable: exit2,
one recovery attempt, both failure reasons, no presents/selection, exact saved
settings and installed custom DDS preservation. Windows11/MSVC19.44/RTX4070SUPER
617.42/SDK1.4.363.0; sourceac4fb17d plus the patches/test changes in this checkpoint
and the local accepted rhi.c definition. Older successful both-fail03 used game
bd7112dbfe13560ce2b7826816043c3e1683e924a8567d0320cdacd731a0bda8.

The trial CMake include was then removed and ordinary tsfp.ps1 Build plus its
mandatory tests passed. No accepted definition remains in the ordinary game
project/cache. vk-after-recovery-fixes-01 passes120s Scotland gameplay through
unchanged LaunchGame with actual Vulkan, requested4x MSAA/SMAA/AF16 and actual
SSAO, preserved custom DDS/defaultVulkan/quality and clean exit0. Its ordinary
executable SHA256 is `f6ec54e5dc74e4713132bf1190af148f49f3fb56f98340994eefb4ad22c73f26`. Production recovery remains OFF everywhere.

The first broad patch-upgrade test run overlapped the addition of3056: its static
fixture file list lacked the newly added exit_trace.c target, causing15 failures
and61 passes. This is invalid final upgrade evidence; the full suite is being
rerun against the now-stable patch list. Both full-stack revalidation runs pass.
The final unit/hardware/upgrade results are recorded after completion.

Linux preparation: separate /root/tsfp-opengl-recovery checkout at ac4fb17d,
pinned requirements installed, SPIRV-Cross verified, Linux SDK1.4.363.0 verified
and the same supported US XBE hash confirmed. The Windows worktree .git file uses
a Windows absolute path, so Linux cloned the common TS Decompile/.git object
store, without editing another checkout. The SDK verifier rejected a symlinked
installation; the new checkout instead has real directories with hardlinked
immutable SDK files from /root/tsfp-linux. Never run setup or modify these shared
SDK files; verify only. SPIRV-Cross and private/game are read-only reuse links to
the Windows source/data. Do not duplicate game extracts or alter other checkouts.
Fetch this checkpoint's final commit before native source/build/GPU testing.
These are prerequisite results only; no Linux game/GPU acceptance is established.

### Linux build baseline and newly identified wiring gap

At committed sourcec479c63a, the separate Linux checkout imports pinned runtime
d90c92988a9f99d0580d2ca3f7efd6ec3a7b7845 and applies the full patch stack. Existing
SDL/Vulkan submodule caches were shallow and Git refused clone references, so
own pinned submodules were fetched normally; other checkouts remain unchanged.
`test-rhi-linux.py --build-only` passes the existing common Vulkan/replay build
and MMIO test. The ordinary runner with explicit
/usr/share/vulkan/icd.d/lvp_icd.json then passes Vulkan pixel/resource/presentation
and production replay contracts under Mesa software rendering. Linux-native
logs/rhi-linux.json records compiler/SDK/driver/executable hashes and scope.
Windows-side transcript logs/opengl-linux-vulkan-baseline.log retains output.
These are neither hardware game performance nor OpenGL GPU acceptance.

The standalone fixture does not initially include OpenGL. More importantly,
patch3054 conditions its include on TSFP_PC_CONTROLS_DIR, while ordinary native
Mac/Linux build commands pass TSFP_NATIVE_DIR and TSFP_NATIVE_RHI instead. No
assignment bridges those paths in the current runtime. Therefore previous notes
about all-host ordinary OpenGL wiring describe intent, not verified integration;
the native wiring remains incomplete. A local standalone compile with explicit
TSFP_PC_CONTROLS_DIR=/root/tsfp-opengl-recovery/src successfully builds
opengl_backend.c, opengl_host.c, opengl_shader.cpp and static libxbox_d3d8.a;
it is only a compile probe and does not fix native game build commands or prove
linking/GPU execution. Its fixture cache now retains that experimental flag.

Next: after the live Windows upgrade suite finishes against its stable patch
list, add a follow-up shared CMake patch deriving the OpenGL source path from
TSFP_NATIVE_DIR on common native builds, without enabling unrelated Windows PC
feature flags or changing legacy Mac builds. Revalidate/migrate patches, then
sync Git to Linux and build/test the actual native OpenGL path. Keep every
production acceptance gate OFF. Native main-thread servicing,4.1 capabilities,
null non2D resources, full native game build and acceptance remain unresolved.

## Native source wiring and typed null textures (afterdf6814a7)

Patch3057 derives the graphics source directory from TSFP_NATIVE_DIR/.. when
TSFP_PC_CONTROLS_DIR is absent. It stays inside the existing common-frontend
branch, so legacy native GL builds acquire neither OpenGL RHI nor compiler
requirements. It does not turn on unrelated Windows PC feature flags. Seven
CMake executions of the production fragment cover Windows Vulkan/D3D, Linux,
Mac, legacy Mac, source precedence and missing source. The native CI suite now
registers tests/graphics/test_opengl_build.py. Actual native linking/GPU checks
remain separate; Linux must be synchronized to this commit and its experimental
PC_CONTROLS_DIR cache entry removed before checking the ordinary path.

The prior broad upgrade suite finished75 passes and2 failures. Both failures
constructed historical inputs with renderer patches whose shared graphics
prerequisites had intentionally been omitted. RENDERER_STARTUP_PATCHES now
excludes3050..3057 only from those two old inputs; full upgrades still install
them. Five targeted tests pass: both corrected historical upgrades, fresh full
installation and both graphics branch round trips. Explicit assertions require
shared startup policy, terminal startup failure, safe exit tracing and native
source derivation in the upgraded files. These tests do not waive runtime edits
or weaken conflict/preservation checks. Full-stack revalidation also passes.

OpenGL sampler metadata now records dimension and scalar/comparison type. Device
creation owns21 tiny zero textures covering2D/array/cube/volume/MS/MS-array and
float/signed/unsigned/depth combinations that GLSL supports. Null bindings select
the matching texture target/type and private nearest/clamped samplers, with a
comparison-never sampler for shadow reads. They cannot retain a previous cube/
volume binding or sample a caller's nonzero border color. Partial device failure
and repeated destruction delete these objects with the other backend resources.

The shared shader adapter guards each sample/fetch result using a per-unit bound
uniform, preserving other inputs and ordinary shader constants. This defines
zero for null fetches even at arbitrary coordinates/LODs, without depending on
out-of-range driver reads. DXC still compiles the same HLSL; a small pinned
SPIRV-Cross emitter override wraps the individual texture operation. Discarded
recompile passes use placeholder expressions and are left to recompile normally.
Resource helper arguments must be inlined by DXC; forced noinline resource-object
copies produce invalid SPIR-V in the pinned compiler and are not worked around
by disabling validation. Tests retain nested helpers that access global textures.
Dimension/count queries retain the existing dummy-view metadata contract.

Uniform declarations now follow the complete preprocessor preamble, outside
conditional extension branches. Placing them after the last #extension alone
had hidden new bound uniforms inside a disabled branch of the SSAO shader. The
actual SSAO and shader/pixel contracts now pass. Backend translation/GL compiler
failures also emit bounded diagnostics immediately, so later successful shader
creation cannot erase the first failure reason.

New tests/graphics/rhi_gl_null.h seeds real textures, then checks bind/unbind/
rebind pixels for2D, array, cube, volume, signed/unsigned, comparison depth and
(normal suite) multisample/array textures. It covers nonzero border samplers,
out-of-range null loads and independent inputs through retained nested helpers.
The pixel helper diagnoses failures and rejects NaNs. Both normal and restricted
4.1 contracts pass, including worker recreation/cleanup; the restricted route
still omits dedicated multisample/AO contracts. Explicit/recovered GL acceptance
now requires the typed-null PASS marker. All five recovery cases and eight
ordinary cases pass with Vulkan validation on positive Vulkan paths. Recovery
fixture acceptance is restored OFF. Standalone HLSL translation, real WGL sampled/
tinted and masked-zero pixels, and generated GLSL syntax also pass.

- rhi-windows.json: `eb4286eccd9b8e57cf6b875a1ae3d7e0a2769b1b67e6bfd0760f0ecf3921732f` (passed).
- rhi-windows-opengl.json: `65856ee1ebc2dbe9e147f08a127270ba805877c4eaceac776ad156f7fa1dc67d` (passed).
- rhi-windows-opengl-gl41.json: `65856ee1ebc2dbe9e147f08a127270ba805877c4eaceac776ad156f7fa1dc67d` (passed).
- rhi-windows-opengl-recovery.json: `2aeb68d1973cae8f8bebfc62dd64dba9a2ca337a8400e8561ec6159dfaf5fb3f` (passed).

Ordinary Windows Build and mandatory checks pass with production recovery OFF.
The120s gl-typed-null-game-01 developer StartInfo probe passes first-mission
movement/weapon, actual MSAA4/SSAO/SMAA, quality/defaultVulkan/custom DDS preservation
and clean exit0. Game SHA256 `d7e05eaf975cafe3a0d3c5161443d499f530ae60032ec55a8dec2b19d26331e7`.
Its85-115s window reports59.98 guest ticks/s and555 handles. It is a bounded
CPU/resource/guest-clock check, not GPU completion/scanout timing. A separate
capture/display run gl-null-visual-01 passes Auto fullscreen/resize/restore and
the same gameplay/settings checks with clean exit0. Reviewed desktop frames
opengl-1.png and opengl-2.png show upright world/NPC/fog/flare, held weapon,
tutorial/reticle/ammunition HUD and changing viewpoint/ammunition. Both runs use
the same executable. Windows11 x64/MSVC19.44/RTX4070SUPER617.42/SDK1.4.363.0,
source parentdf6814a7 plus this checkpoint's patches/backend/shader/test changes.
Reports: logs/renderer-validation/<label>/. Capture runs are not benchmarks.

All production gates remain OFF. Next checks: actual native build/link and GL
GPU execution with main-thread callbacks/window recovery,4.1 mirror-once and
compression capabilities, full native game/package and available-host acceptance.
Native Mac hardware remains unavailable; retain exact missing acceptance and
legacy compatibility/deprecated OpenGL limitations. Do not claim universal support.

At committed ada78e05, Linux full-stack revalidation and all7 CMake cases pass.
With TSFP_PC_CONTROLS_DIR removed from the fixture cache, the ordinary native
path builds actual opengl_backend/host/shader objects and links production replay
successfully. Transcript: logs/opengl-linux-native-path-build.log. This resolves
the source-path/link gap; it is not a full native game or GL GPU acceptance pass.
The standalone pixel probe now prints actual GL vendor/renderer/version to keep
subsequent native pixel results attributable to the real driver.

The first Linux standalone run builds and passes translation plus preceding
sample/tint/null-mask pixel assertions, but aborts at host presentation. This is
not a GL presentation pass. SDL swap-interval and swap failures now log separate
reasons; driver identity is flushed before assertions so failed runs retain it.
Investigate the native error before changing presentation policy or acceptance.

Native presentation diagnosis: the first pixel run is Mesa llvmpipe LLVM20.1.2,
GL4.5 Core Mesa25.2.8-0ubuntu0.24.04.4, not hardware acceleration. SDL refuses
swap interval0 with "That operation is not supported" before attempting a swap.
The new common opengl_present.h policy treats swap control as optional: attempt
it once per requested-value change, log refusal and still perform the real swap.
Saved preference is not changed and no actual interval is falsely claimed. A real
swap failure remains fatal to that present and is reported once until recovery.
WGL and SDL supply only setting/swap calls; policy is shared. Native legacy GL
is unaffected. Test fixture callbacks prove refusal still swaps, changes retry,
repeated failures retain nonzero status with bounded logs, recovery/recreation
and preference preservation. Registered in native CI; Windows unit/WGL pixels
pass. Native pixel rerun is required before calling Linux presentation passed.
Standalone success text now says OpenGL driver rather than claiming hardware.

### Confirmed Linux swap and available accelerated driver

At sourcea81715a4, the Linux presentation-policy unit and standalone GL pixel
probe both pass. Mesa llvmpipe LLVM20.1.2, GL4.5 Core Mesa25.2.8 accepts the real
swap after SDL rejects interval0; the preference remains requested0 and effective
interval is unspecified. It is software rendering, not hardware performance.
Native logs/opengl-pixels-linux.json records source/executable/generated shader
hashes, compiler/tool/SDK pin and actual loaded DXC path/hash. LD_DEBUG confirms
the pinned SDK1.4.363.0/x86_64/lib/libdxcompiler.so was loaded (SHA256
e26ca85aa40bad99f5aed8f90f274277ab05ef4a03827f9033e3026ca9227562).
Compiler GNU C++13.3.0, CMake4.4.4; default SDL probe uses llvmpipe.

The existing WSL bridge and d3d12_dri module are installed. Adding only the bridge
library path still selects llvmpipe, but child-only GALLIUM_DRIVER=d3d12,
MESA_D3D12_DEFAULT_ADAPTER_NAME=NVIDIA and LD_LIBRARY_PATH prefix/usr/lib/wsl/lib
select Microsoft Corporation / D3D12(NVIDIA GeForce RTX4070SUPER), GL4.6 Core
Mesa25.2.8-0ubuntu0.24.04.4. The same sampled/tinted/masked pixels and actual swap
pass on this accelerated configuration too. It still refuses interval0; the
shared optional-control policy reports that and allows the successful swap.
This is the WSL driver implementation of OpenGL, not a new game D3D11 renderer.
Native logs/opengl-pixels-linux-d3d12.json retains the environment and hashes;
Windows transcript logs/opengl-linux-d3d12-probe.log. Do not claim Linux full-RHI,
window-recovery, game, performance or platform acceptance from this small probe.
Use the accelerated environment for remaining Linux acceptance where applicable.

After the presentation change, Windows explicitGL, restricted4.1 and all5 actual
recovery fixture cases pass again; acceptance restored OFF. Ordinary Windows
Build and mandatory checks pass. gl-present-fullscreen-01 passes120s first-mission
startup1920x1080 fullscreen, active MSAA8/actual SSAO, requested/saved FXAA1/AF16,
quality/defaultVulkan/custom DDS preservation and clean exit0. FXAA application
still has no independent game diagnostic. Its noncapture85-115s window reports
59.98 guest ticks/s and558-560 handles; not a GPU/scanout benchmark.
Game SHA256 `10f68d2af2246f2937669dd88eb8c358cf5adbb8b49f0d890b8319cacdc42277`. Sourcea81715a4, Windows11/MSVC19.44/RTX4070SUPER617.42/SDK1.4.363.0.

Current remaining work: native full-RHI OpenGL/recovery fixtures and main-thread
worker/event-loop checks; full native game/package, menus/gameplay/effects/display
and bounded performance/stability on available Linux hardware; explicit startup
capability review (including4.1 mirror-once/compression), Windows final platform
acceptance decision and per-platform gates. Native Mac hardware is unavailable;
keep its exact missing checks, deprecated GL limits and legacy compatibility
recorded. All production gates remain OFF; no publication or merge.


### Native full-RHI fixture opt-in

`tests/rhi` now accepts TSFP_TEST_OPENGL, defaultOFF, to compile the same common
backend in the native contract executable. Its startup acceptance is explicitly0;
this opt-in is not production recovery. `test-rhi-linux.py --backend opengl`
selects an isolated explicit-GL window and full shared pixel/resource contract.
`--gl-driver software|d3d12|system`, optional `--gl-adapter`, and `--gl41`
apply only to the child environment. Vulkan-only controls cannot be mixed with
OpenGL. Driver-specific receipts prevent software results replacing accelerated
results. The Vulkan replay contract is not claimed for an explicit-GL run.

Native SDL worker tests pump main-thread callbacks while two threads create,
clear/read and retire resources; main then reads/presents again. Both native and
Windows fixtures recreate and repeatedly destroy the device on the same window.
The runner requires typed-null pixels, worker dispatch, cleanup and observed
requested driver, plus restricted-view markers when requested. Existing CI's
Linux foundation suite checks child-only driver selection and rejection of partial
or erroneous output. Windows Python checks:14passed,1Linux-only skip. Native
compilation and GPU execution for this new full-RHI path are pending at this
source checkpoint; earlier standalone pixels do not establish these results.
All production gates remain OFF.


First full native software run at1656ab60 compiled and dispatched worker work,
but failed the sample-averaging pixel assertion. Existing allocation probes only
checked for GL error; OpenGL can round requested2 to actual4. Unsupported8 also
emitted an expected API error during capability probing. Replace allocation
probes with exact GL_SAMPLES queries for both 2D multisample and array targets,
requiring query2/core4.3 support for the already extension-dependent MS path.
No unsupported sample allocation or false exact-count claim is needed. Typed-null
MS tests choose an advertised count, still requiring both positive/missing cases.
Mesa compiler warnings retain their severity in WARNING logs; GL error-type
messages remain ERROR, and GL status/compile/link failures keep their checks.
The failed run is not Linux acceptance; reruns are required. Gates remain OFF.


At00e9c226, the software fixture completes all RHI/MSAA/SSAO/typed-null/query and
worker/context recreation pixel checks, but the runner incorrectly requires
Vulkan validation on an OpenGL process. Restrict that requirement to Vulkan;
OpenGL retains its own complete marker/error/driver checks. Move renderer,
vendor and version logging from Windows-only context code into shared device
initialization so native receipts can establish the actual driver too. Preserve
non-error HIGH shader warnings as WARNING; lower-severity performance hints keep
the existing suppression. The failed receipt remains failed pending rerun.


At016713cf, native software full-RHI receipt passes (3.215s execution), including
worker/main-thread dispatch, all MSAA/SSAO/null/query contracts and two context
recreations; executable2bedb96eb4b3b8b6b59d3065c92422ef3686895643758eb3b15641de6177acda.
Windows explicitGL and restricted4.1 fixtures also pass after exact-count changes.
Accelerated WSL/NVIDIA full-RHI run reaches the cube/mip resolve preservation
assertion and fails. Add precise samples/subresource/pixel/RGBA diagnostics to
that assertion before choosing an implementation fix. The accelerated failed
receipt remains failed, and no platform acceptance is inferred. Gates remain0.


The accelerated failure at6bdcdfb6 writes green tosub1(face0,mip1), while the
requested destination issub11(face5,mip1). An ignored direct GL reproduction,
without the RHI/shader/game frontend, confirms Mesa25.2.8 D3D12/NVIDIA changes
face0 and leaves face5 blue; llvmpipe changes only face5. Same-context GL errors
remain0. Resolving first into a plain single-sample2D texture, then copying into
the selected cube face, preserves all faces on accelerated WSL too. Integrate
that common path only for MS-to-cube resolves, leaving ordinary2D resolves and
all non-MS copies unchanged. Temporary allocation failure returns error and
partial storage is retired normally. It adds one allocation/copy for this case;
full-suite and game/performance acceptance are still required. Direct ignored
probe logs:cube-probe-d3d12.log,cube-probe-software.log and
cube-probe-staging-d3d12.log in the native checkout. Native restricted4.1 full-RHI
receipt passes on the accelerated driver(1.784s); gates remain0.


After84c0fe5b, Windows full/restricted GL and all5 accepted recovery fixtures pass
(acceptance restoredOFF). Native optimized accelerated GL then corrupts the heap
at varying later allocations. ASAN/Valgrind completion alone was misleading:
filtered Memcheck identifies a real4-byte read/write4bytes beyond an internal
Mesa48-byte allocation, at msaa_color_surface.h:23 after uploads atline18.
A debug-symbol fixture copy isolates that source line; the MSAA color companion
alone reproduces it. Many other Valgrind reports concern untracked WSL GPU
mappings; those do not establish a clean memory result. No personal state used.

An ignored optimized backend variant reusing the existing4.1 snapshot path for
cube subsets completes the full accelerated suite. Integrate the change narrowly
for cube-face views exposed as2D/array; full cube sampling and ordinary array
views retain their existing storage. The snapshot refreshes only when the image
version changes and uses the already tested shared format/upload logic. This
avoids driver resource-state corruption after sampling a face and resolving its
MSAA companion. It adds cached face storage/readback when changed; game-level
performance still needs acceptance. Exact final narrowing and normal suites
must pass again. Native23 Python/C checks, Windows22checks(1Linux-only skip),
and20 portable Mac-package checks pass. Mac package requires Vulkan/MoltenVK
before verifying SPIRV-Cross; explicit legacy-GL build skips this compiler, so
the inspected package path does not add a legacy-build dependency. Native Mac
hardware and actual legacy package acceptance remain unavailable. Gates remain0.


### Full native GL contract passes; primary-window gap confirmed

Atf0d57ff7, optimized full-RHI receipts pass for accelerated WSL/NVIDIA(2.149s),
llvmpipe(2.823s) and accelerated restricted4.1(1.299s). All use fixture SHA256
fa14e523eec8c563573afb791555882f93ce34212f5c23b62ace6ea5182aaa07.
Native receipts:logs/rhi-linux-opengl-d3d12.json,rhi-linux-opengl-software.json,
rhi-linux-opengl-d3d12-gl41.json. Checks include pixels, full MSAA/SSAO where
advertised, face/mip preservation, typed-null, visibility, worker dispatch,
resource retirement, actual swap and repeated device/context cleanup. Driver
and SDK/compiler/source hashes are retained. The prior optimized heap crash
no longer occurs after the narrowed face-view snapshot change. ASAN and noisy
WSL Memcheck diagnostics are supporting investigation, not clean acceptance.
Windows full and restricted GL pass again. Production recovery remainsOFF.

The ordinary Linux Vulkan-only fixture and production frame replay both pass
on lavapipe(4.934s), confirming the runner returns to its normal Vulkan build;
fixture955e39ae18bdca7bfcfb10133cb24674f9600b6a4b45f55204cc73206a7fef45.
No GL replay is claimed. Native game/menu/input/display/performance/package and
automatic recovery acceptance remain separate, still pending.

Native logs/native-vulkan-window-no-driver.log confirms a missing ICD fails
SDL_CreateWindow before shared rhi_device_create. The native primary-window
adapter now retries an API-neutral window after logging the SDL Vulkan error,
letting shared startup attempt Vulkan and apply its existing acceptance policy.
It never chooses OpenGL, changes settings or enables a production gate. The
existing replacement helper can later stage an OpenGL drawable after failed
Vulkan cleanup. Neutral-window failure remains a clear window error. This path
is confined to the common native frontend; legacy Mac GL is unchanged. The
portable OS-call fixture passes Windows checks for Vulkan-window success,
Vulkan-window failure then neutral success, both window creations failing,
main-thread guard, flags/title/size retention and no renderer selection/commit.
Actual native adapter compilation/recovery/window-rebind tests are next.


### Native automatic-recovery fixture wiring

The native RHI fixture now uses the real primary-window helper and real GL
window-replacement helper with a fixture-owned commit callback. The retained
logical token is asserted while title/dimensions and the replacement's GL flag
are checked after automatic recovery. `TSFP_TEST_OPENGL_RECOVERY` is a private
fixture compile switch, requires TSFP_TEST_OPENGL and defaultsOFF. Ordinary
Linux runner explicitly resets itOFF on every configure. Production gates stay0.
This fixture verifies its own window owner; it does not prove production input,
settings-dialog/render-host rebinding or guest wait-loop service in an actual game.

Expected startup failure verifies no ready renderer, unchanged Vulkan selection,
and repeated cleanup; partial GL drawable failure can occur after replacement.
Native fixture builds also provide a nonshipped libdxcompiler stub for genuine
both-fail tests, using portable32-bit HRESULT/export/calling conventions with
the same Windows lifecycle stub source. The installed pinned SDK is never
renamed/modified. Windows window/runner units:15passed,1Linux-only skip. Actual
native compile and injected recovery runs are pending this wiring checkpoint.


Atc036186c, actual native full-GL compile/pixels and23 window/CMake/runner units
pass. A private accepted fixture then passes all5 real injected cases: Vulkan
success(1.187s), no-driver→GL(2.094s), no-device→GL(1.994s), both-fail(.014s),
and partial drawable failure(.188s). Fixture SHA256
 a5dea5ab58438f0a10d8867f28d19eedc2f142a5ea3e98ca25d9d4190706d2af.
Vulkan success uses validated lavapipe; recovered GL uses accelerated WSL/NVIDIA.
Missing-driver recovery includes real neutral-window creation and GL replacement.
Both-fail uses the real nonshipped DXC stub and no presents. Partial failure
replaces the window, fails target initialization and safely repeats cleanup.
Acceptance is restoredOFF and rebuilt. Native logs/rhi-linux-opengl-recovery.json
andnative-recovery-<case>.log retain this first manual run. These are fixture
window ownership checks, not actual guest/settings/input/game/package acceptance.

`test-rhi-linux.py --opengl-recovery --gl-driver d3d12 --gl-adapter NVIDIA
--driver-manifest /usr/share/vulkan/icd.d/lvp_icd.json` now repeats the matrix,
records source/compiler/SDK/executable/case logs, validates every contract, and
restores private acceptanceOFF in finally, including failures. Driver-specific
recovery receipts are separate from explicit-GL and ordinary Vulkan results.
Build-only/restricted/explicit-GL recovery combinations are rejected. Ordinary
runner always resets private acceptanceOFF. Both normal startup and the native
fullscreen smoke's fresh window use the same neutral retry helper.

Existing registered foundation tests verify isolated faults, exact missing-driver
and DXC-stub injection, unchanged save/display paths, and reject missing window/
cleanup markers, ineffective compiler injection or rendering after both-fail.
Windows17passed,1Linux-only skip. The portable stub compiles and loads as a Windows
DLL: stdcall, exact signed32-bit E_FAIL and cleared output pointer are verified.
Integrated runner rerun is required after this source checkpoint. Production
recovery gates remain0; no publication/merge.


### Actual game builds and first native integration checks

At121561a5, integrated native recovery matrix passes all5cases, private gate
restoredOFF, plus26 registered Python/C checks. Native receipt
logs/rhi-linux-opengl-recovery-d3d12.json; total15.463s including restore rebuild.
Windows ordinary Build/mandatory checks pass. Fresh explicitGL game
`gl-native-fixes-02` passes120s Auto1x1/MSAA4/SMAA/AF16/AO, fullscreen/resize/
restore, automatic scene target resize, movement/weapon, settings/custom DDS
preservation and clean0. Captures88/106 reviewed: world/weapon/HUD, sky/fog/fire,
changed camera/ammunition. The prior01 receipt staysFAILED: I passed0x0 forAuto,
but the runner's actual Auto setting is1x1; its resolution assertion correctly
rejected that request. No test was weakened. Game SHA256
 d58b7df6e1ea9bc83e21b3aba5b2c5334a01dd3f59c34ba2b8e4333def899d62.
Matched uncaptured120s `gl-perf-fixes-01` and actual-launcher
`vk-after-native-fixes-01` pass: fixed1280x720/MSAA4/SMAA/AF16/AO, clean0 and
preservation. Observed85-115s guest rates60.02/60.03Hz, handles555/643flat,
CPUcore equivalents1.48/.71 respectively. These are bounded stability/process
observations during another build, not GPU completion/scanout benchmarks.

The native Linux actual game builds successfully with Clang18/O2/debug symbols,
assertions, shared US generation and MMIO checks. Source121561a5, clean manifest;
executable717d5b82f7cfdee2de3d62c0ac15f65c78af5bf90f8cd6b1fa353ba63afd7aa8.
Native logs/opengl-native-game-build.log and build/linux/game/BUILD.json retain
provenance. Initial actual executable renderer-self-test reached Vulkan
clear/draw/depth/overlay and window modes, then lost the WSL X connection;
subsequent GL could not initialize video. Weston sockets/logs show its display
service recovered automatically; no host restart was issued. Those results fail
acceptance and are not GL rendering evidence.

Direct Wayland actual Vulkan self-test instead reaches a fullscreen/restore
assertion failure, still unresolved. Direct GL reveals a real replacement bug:
SDL returns cached window coordinates but rejects SetWindowPosition for a normal
Wayland window. Skip querying/restoring placement on that display backend; the
compositor owns it. Preserve all other transaction checks and non-Wayland
positioning. The portable unit now proves a successful cached-position query
cannot cause a forbidden Wayland move. Native window-mode self-test diagnostics
now identify request/sync/active-state/drawable/clear/present failures per step,
retaining all acceptance predicates. Windows unit passes; native rebuild and
rerun required. Actual guest/menu/input/display/performance/package and native
Mac hardware checks remain pending. All production recovery gates remain0.


### Real guest-loop deadlock and native wait servicing

Atad4c833c, the rebuilt native executable initializes OpenGL on direct Wayland
and passes actual frontend clear/draw/depth/windowed-present self-test steps.
Both Vulkan and GL fullscreen checks stillFAIL: request1/sync1 but active0,
expected/drawable640x360, clear1/present0. WSL's compositor does not acknowledge
fullscreen in this run; assertions remain intact. This is not Linux fullscreen
acceptance. Native logs/native-wayland-placement-{vulkan,opengl}.log retain it.

The actual GL guest30s run stalls after1 completed frame, CPU under2s; its old
runner's >0-frame criterion returns0 but that is NOT gameplay acceptance. A
Vulkan control with a copied/verified fictional Test Lab fixture and synthetic
input progresses1500guest frames in30s and exits0. GL with the same fixture/
input stillstalls at1. Owned-process GDB proves main thread in platform
wait_single(INFINITE), while worker thread waits in SDL_RunOnMainThread from
lease_clear_color. Native logs/native-gl-guest-thread-stacks.log and
native-{gl-input,vk-first}-guest/probe.json retain source/ELF/results. Actual
ELF0c7ca9195270bf92108128bd2e75197f2317428e2b5f24f54696e39e5e806df7.
Native capture folder reused by the old runner can contain prior BMPs; the
Vulkan images were moved into its owned evidence folder before GL rerun, so no
stale image is claimed as GL. Improve per-run capture freshness before using
that runner for visual acceptance.

Patch3058 adds an optional atomic owner-thread wait-service callback to generic
POSIX host primitives. Zero budget queries ownership; ordinary blocking single/
multiple waits poll readiness/deadline and service a one-millisecond idle budget
outside object locks, with a TLS recursion guard. Unregistered/foreign threads,
zero-time polls and dedicated sub-millisecond waits retain their original paths.
The native common RHI SDL adapter registers at window bind, unregisters at
unbind, and uses SDL_WaitEventTimeout(NULL) to wake for main-thread callbacks
without consuming queued input. Worker frame-pump requests are asynchronously
coalesced onto the main thread; input/developer frame work stays in that existing
shared reader, rather than running for every GPU call. No rendering/gameplay
policy is copied. TSFP_RHI_RENDERER guards the opt-in and queue changes; the
legacy Mac GL path keeps its original behavior. Windows uses its real OS waits.

The new native UBSan fixture covers owner-only progress, callback SetEvent
without held object locks, recursion prevention, deadlines, zero-time polls,
foreign waits, auto/manual reset, multiple and thread waits, and unregister.
Registered in common native suites plus Linux CI. Historical minimal upgrade
inputs omit3058 with the other startup-only patches; real full upgrades retain
it. Windows29 supporting tests pass,1Linux-only skip; native unit/build/game
rerun remains required before considering this fix verified. Gates remain0.


At5617455d, the native UBSan wait fixture plus runtime-suite checks pass13tests;
ordinary native game rebuild/MMIO succeeds. Actual `gl-main-wait-01` stillstalls
at1frame and is NOT accepted. `gl-main-wait-02` owned-process GDB confirms the
callback is installed and recursion guard0, but the main thread has advanced
out of the kernel wait into a pure guest floating-point polling loop
(sub00156840, branch156877). The render worker still awaits main-thread GL.
The wait-service fix alone therefore cannot provide continuous SDL dispatch.
Its unit result must not be conflated with actual guest-loop readiness.
Further native execution/dispatch integration is required before gameplay and
recovery acceptance; no generated guest code was edited. All gates remain0.
The Vulkan control's archived BMP was reviewed as rendered starfield/lens-flare
intro imagery; it is not proof of active gameplay/menu/input acceptance.


### Continuous SDL owner loop for eligible native startup

Patch3059 exposes rhi_startup_may_use_opengl: a side-effect-free query using
startup's existing backend list/environment resolution and accepted Vulkan→GL
recovery gate. Native main uses it after existing helper/self-test routes. If
GL can be selected, the complete guest body (TLS registers, memory/kernel setup,
crash stack and entry) runs on an owned POSIX worker while SDL main continuously
services callbacks. Otherwise guest_main executes inline as before. The legacy
Mac path does not acquire TSFP_RHI_RENDERER and remains inline; unverified Mac
normal Vulkan startup with recovery0 is also unchanged. No guest polling branch,
clock or generated code is edited. Windows has its own entry/host loop.

The new native guest_dispatch_sdl.h initializes missing video only on the owner,
allocates16MiB worker stack, preserves the worker's result, joins after done and
uses a pthread cleanup handler so explicit thread exit/cancellation also releases
the owner loop. Native window requests from guest workers synchronously marshal
to the owner; native_window reads/creation remain on that thread. The RHI fixture
checks dispatch eligibility without changing backend selection. The headless
native fixture models a guest busy-polling while another thread synchronously
requests main work, plus TLS isolation, result/join, existing-video retention,
wrong owner, init/create failure and explicit thread exit. Registered in common
native and Linux CI suites. Windows45 supporting checks pass,7 native-dependent
skips. Native compilation and real guest progress remain pending at this source
checkpoint; all production recovery gates remain0.

## Restored selector implementation checkpoint

GameSettings.Renderer and SettingsPage.GraphicsRenderer restore one shared
Vulkan/OpenGL choice. Save, Cancel, dirty state, reopen, both child environments,
inherited override removal and malformed/retired D3D11 migration have behavioral
coverage. Legacy manifest renderer=gl defaults to OpenGL; all ordinary builds
default to Vulkan. A saved supported choice overrides that default.

New native build receipts and all three package writers expose
supported_renderers. Native packagers preserve the actual receipt list (old
receipts default to Vulkan only), and legacy Mac builds declare only OpenGL.
Missing development receipts permit both compiled development backends; an older
installed receipt permits its original primary only. An unsupported saved choice
is preserved and causes a clear update/select-supported-renderer startup error.
It does not silently launch another backend. The Windows acceptance fixture also
marks its known dual-backend game build.

The real Windows hardware harness no longer overrides the OpenGL environment in
a special developer child: both choices select/save GraphicsRenderer and call
the actual LauncherWindow.LaunchGame. Previous developer-probe runs remain
historical evidence. Native launch environment/library preparation still needs
explicit OpenGL validation, especially the Mac Vulkan prerequisite checks; this
checkpoint does not establish native selector gameplay or package acceptance.

Windows build-launcher.ps1 -Test passes, including 54 settings checks and shared
UI switching/persistence/cancel/older-package/legacy tests. Portable renderer,
shared-layout and native package checks:66 passed,1 host skip. Compile-only real
Windows acceptance harness passes. Actual game acceptance is pending this change.

The b05ba5d2 continuous native SDL dispatch build and14 native focused checks
pass. gl-guest-loop-01 still reports only1 completed guest present in30s, despite
additional guest/audio/shadow-frame activity. Do not claim native gameplay
acceptance; inspect the current frame/dispatch evidence before another rerun.

Native launcher compile and Linux adapter checks pass at65d753d2. The first shared
UI run stopped because a synthetic renderer=vulkan receipt omitted runtime hashes;
the fixture now uses minimal compatibility metadata, while separate native GPU
receipt/closure tests retain their validation. This is a fixture correction,
not a weakened native package or shader compiler prerequisite.

## Windows selector gameplay and host verification checkpoint

With game SHA25661074a371c2b125c91e0f813a7cb5b5b831c7d323aa55c0bb8dcc76767abb76d,
gl-selector-02 and vk-selector-01 each PASS120s through actual SettingsPage and
LauncherWindow.LaunchGame. Both reach representative first-mission gameplay,
movement and equipped weapon, Auto1x1,4xMSAA/SMAA/AF16/AO, fullscreen/windowed/
resize/restore, scene resize, clean exit0, retained selected renderer/quality and
synthetic installed custom DDS. OpenGL capture88s reviewed: world, weapon, HUD,
sky/fog/fire visible. Visual-run guest clocks are ~60Hz; these capture/display
runs are not matched uncaptured throughput measurements or scanout benchmarks.

The failed gl-selector-01 is retained: gameplay/display/exit passed but an old
post-run assertion still required Vulkan after saving OpenGL. It now checks the
saved selected backend. The harness also starts each new case from the opposite
saved renderer and requires UI Save to become enabled, so future Vulkan cases
prove an actual OpenGL-to-Vulkan UI transition. The runner's top-level launch
route metadata now says launcher for both choices, matching the actual case.

Final Windows launcher build/regressions pass in
logs/launcher-renderer-selector-final-02.log (including54 settings checks).
An intermediate final-log run hit the intended global update guard because a
hardware game was concurrently running; no product guard was changed. Portable
checks remain66 passed/1 native host skip. Both native launcher projects compile
from Windows with installed.NET9.0.312 and existing net8.0 restore caches:
logs/launcher-selector-{linux,mac}-compile.log. This is compilation, not native
Mac/Linux UI/runtime acceptance or a replacement for pinned native SDK checks.

Native WSL verification was interrupted after the first Linux UI fixture error.
Host C: free space reached ~1MiB, Ubuntu returned read-only filesystem and I/O
errors, then Wsl/Service/CreateInstance/E_FAIL. The checked-clean fetch could not
write FETCH_HEAD, so the native checkout remains65d753d2, before a47a7391's
fixture correction. No host/service/distro restart or filesystem repair was
issued. Reclaimed regenerable Windows game .obj files and duplicate
build/windows/crash-symbols.zip, preserving EXE, matching PDB, sources, captures,
logs, game assets and saves; C: then had ~2GiB free. Recreate the archive via the
supported build before any package delivery. Native tests need a healthy writable
WSL filesystem and Git fast-forward before retrying. All production automatic
recovery gates remain0; Windows capability audit and native acceptance are still
required by the full goal.

Receipt detail: gl-selector-02's older top-level launch_route still says
 developer-start-info; its nested case, compiled harness and observed launch log
correctly identify the actual launcher route. The runner is corrected for future
receipts; historical evidence was not rewritten. vk-selector-01 used the saved
Vulkan default; the new opposite-choice Vulkan switch is a separate case.

The stricter vk-selector-02 now PASS120s: it starts with saved OpenGL, changes
the actual UI to Vulkan, requires dirty Save, launches Vulkan and preserves that
choice through clean exit. Auto/display/effects/input/custom DDS checks pass;
compiled game hash is unchanged. New top-level and nested launch_route both say
launcher. The viewed Vulkan88s captures show world/weapon/HUD/fire/sky, but their
scene/HUD colors differ visibly from gl-selector-02's88s capture. Vulkan01/02
also differ in transient haze/objective animation. Do not infer pixel parity
from these timed runs or assert which backend is correct. A matched guest-state
frame capture/replay or equivalent controlled comparison is now an explicit
remaining graphics acceptance check, alongside the capability audit. Inspect
channel mapping, constants, blending and game transient effects against the same
frame before diagnosing or changing a backend. Automatic recovery remains OFF.

## Minimum OpenGL capability checks

Shared opengl_capabilities.h defines the common frontend's minimum capabilities:
core4.1, complete EXT_texture_compression_s3tc (BC1/2/3), and mirror-once sampling
via core4.4 or ARB_texture_mirror_clamp_to_edge, EXT_texture_mirror_clamp or
ATI_texture_mirror_once. BC1-only advertising does not satisfy BC2/3. These are
checked after procedure loading and before framebuffer/VAO/null-resource setup.
Missing support logs the specific requirement and destroys the staged context;
no renderer is reported ready. Legacy Mac's separate frontend is unchanged.

Sources: [Khronos S3TC](https://registry.khronos.org/OpenGL/extensions/EXT/EXT_texture_compression_s3tc.txt),
[mirror clamp](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_texture_mirror_clamp_to_edge.txt),
and [core4.6 appendix documenting4.4 promotion](https://registry.khronos.org/OpenGL/specs/gl/glspec46.core.pdf).
Do not replace required sampler semantics with clamp or silently drop compressed
textures to get an older driver through initialization.

The portable C unit covers old/new core versions, BC1-only versus full S3TC,
each mirror extension alias and core4.4 promotion. It is registered in common
native shards and the Linux explicit graphics checks. The real RHI pixel fixture
also samples negative and greater-than-one texture coordinates that distinguish
mirror-once from clamp, wrap and mirrored-repeat, and samples BC1/2/3 RGB plus
BC2/3 partial alpha. Both accepted-recovery runners now include missing-core,
missing-S3TC and missing-mirror cases. Fixture-only capability masking compiles
out of production. After each rejected recovery, the test clears the mask and
creates/destroys OpenGL again on the same window, checking repeated cleanup and
context recreation instead of only a failure string.

Windows hardware accepted-recovery matrix:8/8 PASS (including all3 minimum
capability failures), accepted executable SHA256
9de9352ea85881cc5d060734d4f0cb3cad60311a8646f7bcb7f1348571c85ae9.
This initial matrix preceded the added compression/mirror pixel cases; the final
matrix must be recorded separately. Ordinary full/restricted OpenGL passes after
startup-check integration; new mirror pixels pass GL/Vulkan/D3D11; new BC1/2/3
pixels pass GL. Other final pixel/combined matrix checks remain pending below.
All checks use RTX4070SUPER/NVIDIA617.42 and the pinned shared compiler. The
fixture-only accepted gate was restoredOFF and rebuilt; normal game gates stay0.

Portable first batch:9 capability/build/present checks pass. A broader batch
has26 passing/1 host skip and3 pre-existing SDK installer fixtures stopped by the
real5GiB free-space prerequisite. No setup guard or SDK was changed. The relevant
recovery/GL selection rerun passes; native GPU reruns remain unavailable while
WSL fails to start. The game EXE has not yet been rebuilt for these startup-only
checks; selector gameplay61074a... remains earlier evidence.

Final capability/texture GPU matrix after all pixel cases:8/8 PASS, accepted
fixture SHA256cedcd860828e997a45dd0ca621978968e2105877ba848fafb1129e5749c77b1c.
logs/rhi-capability-final02-recovery.log and rhi-windows-opengl-recovery.json
record results. The private accepted fixture was restoredOFF/rebuilt in finally.
Ordinary restricted4.1 GL and D3D11 also pass the new mirror/S3TC pixel tests;
normal Vulkan passes in the matrix. One intermediate new compressed test used a
null array instead of an array containing a null view; Vulkan correctly exposed
that fixture API misuse. It was corrected without changing production semantics.
Portable final:21 capability/build/present/startup/shard checks PASS, plus13 Linux
foundation checks PASS/1 host skip;3 SDK-storage-dependent fixtures explicitly
excluded after their earlier5GiB guard failures. Native real GPU checks are not
replaced by these portable tests.

## Controlled scene color diagnosis

The isolated diagnostic capture probe is in ignored build/frame-parity, derived
from the acceptance harness but uses GameSettings.StartInfo directly to add the
capture environment. It is explicitly not another LaunchGame acceptance run.
No PC post-AA/AO/MSAA enhancement is enabled for this comparison. It passes120s,
uses the same fictional Test Lab isolation, bounded256MiB capture/512MiB free-
space watchdog, and records exact EXE61074a... provenance. The one completed
format-v10 capture at guest swap4800 contains354 draws/7506 chunks/72 textures,
8966920 bytes, SHA2562e14f8e1ca7147c6d529ac8e18e25bd22c9d6f860efaf81f2d4fad194b748382.

Production d3d8_replay target built from normal Windows libraries/source; a
temporary CMAKE_PROJECT_INCLUDE helper was unnecessary because the target already
existed, and the cache include was restored empty afterwards. Diagnostic EXE is
outside game package inputs, copied with pinned DXC to build/frame-parity/replay,
SHA25680f3fdf947fecde7e9223bff1bb6244ddceacb2b6edd97b16187175208fab8c6.
All three backends replay the same capture:0 failed host calls,0 malformed/
unmapped/unknown chunks. Reviewed logs/frame-parity-01/{vulkan,opengl,d3d11}/
frame000.bmp: Vulkan and D3D11 agree in scene colors, OpenGL is blue/pink in
world/HUD where those two are brown/gray. GL-vs-Vulkan RGB MAE10.02049 on0..255,
90.88% pixels differ. Timed gameplay ambiguity is now eliminated for this frame.

Prefix replay1 is identical black; prefix2 already differs (MAE5.58644). The
first color-producing draw (draw1) uses vertex declaration color dxgi87/BGRA8.
OpenGL g_layout_create derives components=4 from GL_BGRA, while prepare_draw's
VertexAttribPointer consumes that as ordinary RGBA. This is a concrete candidate
for the first draw discrepancy; add an asymmetric BGRA vertex pixel regression
before changing the binding. Do not modify gameplay colors, fog or shaders to
compensate. Other differences still need controlled replays after the repair.

## Packed BGRA vertex color repair

The asymmetric packed BGRA vertex regression in rhi_vertex_streams.h reproduces
this failure before the repair: red0.78431374 instead of0.15686275. Failure is
retained in logs/rhi-bgra-before.{log,json}. The GL binding now uses GL_BGRA as
VertexAttribPointer's size token for RHI_FORMAT_B8G8R8A8_UNORM, retaining the
existing unsigned-byte type and normalized=true. RGBA/float attributes are
unchanged. The vertex declaration, not a shader/color/fog workaround, owns this
ordering. See [Khronos vertex BGRA rules](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_vertex_array_bgra.txt).
The shared header is used by all RHI fixtures, and GL receipt validators now
require its success marker. This shared backend change also applies to native
common RHI; legacy Mac GL remains separate and unchanged.

Full GL, restricted4.1 GL, Vulkan and D3D11 GPU contracts pass after the repair.
Rebuilt production replay uses the exact same354-draw capture in frame-parity-02:
0 failed/malformed/unmapped/unknown operations for all backends. Reviewed GL image
now has the expected brown world, gray crosshair/tutorial buttons and white HUD.
GL-vs-D3D11 differs at10/307200 pixels, RGB mean absolute error0.000044488 on0..255,
maximum6,8 channel values differ by>1. GL-vs-Vulkan mean0.14823459/max12 matches
the existing Vulkan-vs-D3D11 mean0.14820530/max12 baseline closely. Vulkan's before/
after image is byte-identical. logs/frame-parity-02/pixel-comparison.json retains
exact metrics; this is one representative scene, not whole-game certification.

No game EXE was rebuilt for these replay checks yet; the61074a... isolated game
produced the immutable capture before the fix, while the rebuilt replay tool
contains the current backend. Final supported game build, actual launcher GL
capture/gameplay, native host acceptance and Windows startup-recovery enablement
remain separate required work. All production automatic-recovery gates stay0.

## Rebuilt game and recovered native host (after54ee1d6)

The supported Windows build completes with all mandatory tests at code54ee1d6.
The first full rebuild (after earlier object cleanup) hit MSVC C1002/pass2 heap
exhaustion in recomp_context_0752/tsfp_context_run_003A03A0, already an /Ob1 policy
exception. Resuming with -CompileJobs1 succeeds while preserving completed
objects; no generated C, compiler policy or optimization flags were changed.
Matching PDB/crash-symbols.zip are recreated. Normal EXE SHA256
 a0ed2d1729f6bc7202178a9914e79b0d635c137abbe0dd282a11d0bbe2fda159.

gl-color-fix-selector-01 PASS120s through actual selector/LaunchGame on this EXE:
Auto1x1,4xMSAA/SMAA/AF16/AO, effective scene resize, fullscreen/windowed/resize/
restore, movement/weapon gameplay, retained saved renderer/quality/custom DDS,
clean exit0. Reviewed88s image now has brown world, gray crosshair, white HUD
and yellow objective text. Observed game clock~59.99Hz,583 stable handles;
this captured/display-changing run is not a clean throughput benchmark.
The matching vk-color-fix-selector-01 Vulkan control also PASS120s on the same
EXE with the same graphics/display/input/preservation checks and exit0; its
observed game clock is~60.01Hz and648 handles stay stable. These are actual
selector transitions in both directions, not direct backend overrides.

WSL now starts normally and the native checkout is writable. No shutdown,
service/host restart, remount or filesystem repair was issued. Clean Git
fast-forward to54ee1d6 succeeds. Native launcher --test passes54 shared UI groups
including renderer switching;29 focused native units pass. The full native8-case
RHI recovery matrix also passes with D3D12(NVIDIA) GL and lavapipe Vulkan,
including BGRA/mirror/S3TC pixels, missing capabilities, window replacement and
cleanup. Receipt rhi-linux-opengl-recovery-d3d12.json confirms the private gate
restoredOFF. Source54ee1d6 normal native game builds and passes MMIO/decoder tests,
ELF SHA2564d50e77e4b14f66f835159b6f4fe392a3a541f9bb6c37773bc5cf2ce96bcc5ab.
Its supported_renderers receipt declares Vulkan/OpenGL.

## Native gameplay and dispatch performance still incomplete

The old gl-color-fix-guest-01 diagnostic exits0 after120s with at least300 logged
guest presents but fresh images remain intro/starfield; it is not gameplay
acceptance. Its simple progress flag is insufficient. The Linux Story fixture
requires --story-benchmark AND --frame-profile for state-based intro skip and
at least15s active gameplay. One attempted wrapper omitted frame profiling and
failed CLI validation; vk-color-fix-story-01 copied stale summary data and is
invalid evidence. The corrected ignored guest-story-probe.py now copies only
fresh reports and propagates failures. vk-color-fix-story-02 PASS120s on the
normal ELF, reaches active scene2/paused0, has2426 active-window samples over
55.7..119.49s,~38.02 host guest presents/s with software lavapipe. Fresh final
capture reviewed: visible first-mission world, weapon, HUD, fire and sun. This
is WSL/software Vulkan, not physical Linux Vulkan throughput; concurrent Windows
compilation is also outside a controlled performance comparison.

Two ignored source-include trials isolate the eligible GL main dispatch loop:
normal SDL_WaitEventTimeout(NULL,1) logs at least300 presents/120.20s (~0.305 CPU
core-equivalents); SDL_PumpEvents+SDL_Delay(0) logs300/30.32s (~0.597 cores);
SDL_PumpEvents alone logs1200/30.21s (~1.463 cores). Same30s normal Vulkan control
logs1500/30.12s with software lavapipe (~5.966 cores). Counts are periodic lower
bounds, not exact average FPS or scanout. These comparable-intro trials identify
a dispatch-latency bottleneck; they do not certify active GL gameplay.

Trial source/header/CMake hashes and executable metadata are archived in
logs/gl-pump-trial-{01,02}/trial-build.json with matching poll.h/trial.cmake.
No trial code is committed or left active. Both finally paths clear the private
include, rebuild, and restore the exact normal4d50e77e... ELF; current
CMAKE_PROJECT_INCLUDE is empty. The permanent native dispatch fix must preserve
SDL ownership, queued input, guest TLS and cleanup, while checking throughput
and idle CPU; do not silently ship a diagnostic busy-poll loop. Native actual
GL Story/effects/window/performance/package acceptance, WSL fullscreen limitation
review, Windows recovery-gate approval and unavailable Mac checks remain in the
full goal. All production recovery gates remainOFF.

## Adaptive native dispatch implementation checkpoint

The native SDL GL host now exposes an owner-thread progress serial, incremented
only when main_operation executes on the SDL owner. CPU-only operations still
run on foreign threads under their existing call_lock and do not write it.
renderer_rhi_sdl provides the thin host getter to native main; there is no new
RHI backend policy/vtable or generated-code change. Windows host dispatch and
legacy Mac's inline entry route are unchanged.

guest_dispatch_sdl pumps callbacks continuously during a burst, extending an
active window on each progress change. After two1ms idle-timeout quanta without
work it returns to bounded1ms waits. Pumping/NULL waits preserve queued input.
The optional progress callback retains the old bounded-wait behavior when absent.
Unsigned serial inequality handles wrap without inventing a time source. Guest
TLS/setup/cleanup/stack size and pthread result/join handling remain unchanged.

The actual updated C helper passes its native Clang headless busy-guest unit
against a read-only Windows source/header snapshot with output in native/tmp.
It covers burst progress, idle waits, deadline/wrap behavior, TLS ownership,
return/join, preexisting video, init/thread failures, wrong owner and pthread_exit.
Native build, real-driver throughput, active Story and idle CPU checks are still
required. This checkpoint records implementation, not acceptance; all production
automatic recovery gates remainOFF.

Adaptive91b5683b native game build and3 focused dispatch/wait/capability tests
pass. gl-adaptive-story-01 passes120s with the proper Story observer:1500 samples
over55.50..119.05s verified active scene2,~23.59 host guest presents/s,
median40.82ms/p9554.61ms/worst64.31ms;~1.483 CPU core-equivalents. ELF SHA256
5bdf19f5acff5f336e56d777e8decf76685db4870baa9c50ef08afd74ac2e88e.
Fresh final capture reviewed: correctly brown world, weapon, HUD, sun and fire.
No rendering/parse errors; clean exit. This establishes active native GL
progress, not complete input/display/performance acceptance. Software lavapipe
Vulkan's earlier38fps is a different driver path, not an apples-to-apples GPU
comparison. Native GL throughput remains below60 in this WSL bridge environment.

A new headless idle CPU regression sleeps the guest100ms and bounds owner CPU
below25ms; the actual run consumes2.689ms. This rejects a permanent busy spin
while tolerating CI scheduling variation. The real native RHI handoff fixture
also now requires the GL owner progress serial to change across worker GPU work;
its receipt validator rejects missing progress evidence. Driver verification
must follow after this fixture update.

Windows Vulkan validation is available: the existing SDK Bin contains the
API1.4.363 Khronos layer, signed by LunarG with Valid Authenticode status. DLL
SHA2562acc317ef880f73a9862f23a964694c03f531b866fab6d70f1412fcbae60caac.
Explicit child-only --validation-layer-path external/vulkan-sdk/windows/1.4.363.0/Bin
passes the full Windows Vulkan RHI contract with validation on and no diagnostics.
No download, registry change or SDK mutation is needed. Earlier no-layer logs
reflect discovery without this explicit path; they do not prove missing files.


## Current-source recovery integration recheck (2026-11-22)

At clean source95d815c9, the native three focused dispatch/wait/capability units
and all eight actual-driver RHI recovery cases pass again. The native fixture
now observes real GL owner dispatch progress, and its runner restores the
private recovery gateOFF. Evidence: logs/adaptive-dispatch-final-units.log,
logs/adaptive-dispatch-final-rhi.log and rhi-linux-opengl-recovery-d3d12.json.
These tests do not approve Linux automatic recovery or native Mac support.

The same source passes actual Windows launcher startup recovery in
gl-current-startup-recovery-01: a child-only missing Vulkan manifest produces one
vkCreateInstance failure, one recovery attempt, and only OpenGL guest presents.
The 120s run reaches Story gameplay with movement/weapon checks, actual MSAA4,
SMAA/SSAO, Auto scene resize, fullscreen/windowed/resize/restore and clean exit0.
Saved Vulkan selection/quality and installed custom DDS remain preserved.
Reviewed frame0 has correct world/weapon/HUD colors and visible fire/objective.
The85.22..114.82s observation reports59.966 guest ticks/s,583 handles throughout,
and1.486 CPU core-equivalents; captures/display changes exclude benchmark claims.

Trial EXE SHA25611efbd20c0f8bde8cec02f85fc4ea97ca74d3816c5621f30b45b2e35d484c12d
uses a local ignored source-only TSFP_OPENGL_RECOVERY_ACCEPTED=1 definition.
Its cached in-game version stamp predates this source; the recorded clean Git
source, runner hashes and executable hash identify this test, not that stamp.
The CMake project include must guard CMAKE_CURRENT_SOURCE_DIR==CMAKE_SOURCE_DIR
because nested SDL/platform project() calls also evaluate the include; an initial
unguarded local configuration failed before building and was corrected.
No production policy or compiler flags were changed by this diagnostic injection.

gl-current-both-fail-01 passes with the same trial: missing driver plus the
sandbox-only unavailable-DXC fixture yields both failure reasons, exactly one
recovery attempt, no selection/presents and bounded exit2. Settings and custom
assets remain byte-identical. The normal cache was then cleared of the private
project include and the supported ordinary Build/check route started; record
its final result separately. Never deliver the accepted trial binary.
All production recovery gates remainOFF pending final platform decisions.


Ordinary current-source Windows Build and mandatory checks finish successfully,
including recreation of matching crash symbols. CMAKE_PROJECT_INCLUDE is empty.
Normal EXE SHA256713c7644dad6492117d1ba5788425501dc6e9e9666e2280a696f376bded60017.
Fresh/repeated build and historical native-checkpoint, controls, and Mac-stack
upgrade tests pass4/4 (34 deselected,157.99s) against the stable3058/3059 patch
list: logs/opengl-final-patch-upgrades.log. This is focused upgrade coverage;
it does not relabel all repository tests or native Mac acceptance as passing.


Native gl-adaptive-controls-01 completes120s with the normal91b5683b ELF
5bdf19f5... and3300 periodic guest presents,0 logged failed presents/runner exit0.
The isolated script sends W70..74s, fire80/84s, pause95s and resume99s.
Owned scene/flags samples confirm pause95.185..99.192s and resume99.693s.
The active55.35..93.86s window has876 host guest presents,22.72fps,
p9556.06ms/worst75.64ms and no interval over100ms. Final image has correctly
colored world/weapon/fire/sun/HUD. This is bounded windowed WSL evidence only.
The added diagnostic player observer reports no displacement and weapon-1;
therefore movement/weapon-state acceptance did NOT pass. Do not treat the
wrapper's coarse progress flag as full controls acceptance. The firing script
was delivered, but delivery alone does not prove the firing behavior. Pause/
resume proves that some live input survives native GL window replacement.
Evidence includes the isolated input JSON, guest-controls-probe.py and fresh
captures in the native checkout; retain the failed/unverified observation.
Linux automatic recovery staysOFF pending corrected state/control verification,
display/fullscreen, performance and packaged-host acceptance. Native Mac remains
unavailable with its own unchanged acceptance gaps; Windows checks cannot waive
those requirements. No personal save/settings directory was used.


## Renderer-aware native launch preflight

NativeWindow Play and command-line --check now pass the saved renderer to
NativeHost.CheckBuild. Configure uses the already scrubbed/selected child backend.
For Mac common OpenGL, PortableGpuEnvironment checks DXC and other shared dylibs,
sets DYLD_LIBRARY_PATH, and does not require Vulkan loader/MoltenVK/ICD metadata.
Vulkan keeps its original checks, environment and driver restrictions. Full
installation Verify still checks the primary package; legacy renderer=gl routes
are unchanged. The checkout helper passes renderer metadata into launch_environment:
OpenGL verifies the pinned compiler SDK but does not select/inject Vulkan driver
settings. No save/settings/manifest writes are added. Native Mac runtime behavior
remains unavailable, not approved by a source fix.

New actual C# shared UI regression exercises the portable GL environment with
missing Vulkan files/driver metadata, rejecting a changed compiler. Python SDK
regressions verify GL bypasses Vulkan selection but still requires compiler
integrity. The45 SDK/native-Mac launcher Python checks pass. Native C# build/run
and Mac compile checks must follow; all production automatic gates remainOFF.


Renderer preflight c8b8d7d2: the supported native Linux launcher build/test passes
55 shared UI groups, including the production PortableGpuEnvironment GL test.
Mac launcher compile succeeds on Windows with two existing Avalonia obsolete
Checked/Unchecked warnings,0 errors. This does not execute a Mac binary or
approve its GPU/legacy path. Evidence: native logs/renderer-preflight-native-launcher.log
and Windows logs/renderer-preflight-mac-compile.log. The45 Python checks also pass.

## Native controls verified after local ownership and readiness checks

The previous current-context observer could sample uninitialized/transient
state: scene2 is published before local ownership is established. A diagnostic
using the persistent local context at0x7b0c48 checks local id0, body+0x150 back
reference and actor->body ownership before accepting observations. The initial
strict probe gl-local-controls-01 rejected that transition and is not passing
evidence. gl-local-controls-02 records raw transition observations, filters them
for assessment, verifies ammo16->15 at84s and pause/resume, but W70..74s precedes
readiness/capture and produces no displacement. Neither earlier run is relabeled.

gl-ready-controls-01 uses the same normal native ELF5bdf19f5... and changes only
the isolated schedule: W85..90s, after the fire/capture input. It completes120s,
exit0,3600 periodic guest presents and0 failed presents.126 observations pass
local ownership validation. Positions move31.2309 game units in84..92s;
weapon14 ammo decreases16->15 at84.141s with112 reserve; pause/resume also works.
The active55.49..94.01s window has901 host guest presents,23.366fps,
p9555.424ms/worst70.661ms, no interval>100ms. These are WSL bridge observations,
not scanout or physical Linux performance approval. Script and input are archived
beside fresh receipts/captures in logs/gl-ready-controls-01. SHA256:
guest-ready-controls-probe.py d9ed3e7cae5f33ac83ef3564ff4417e657424bf17a153486a02585dba433dc37;
input JSON74cdad4dd73ef41a0f61889a76c52214bfd3c4379dc9f087dfaf18760fb326bf.
This resolves the narrow movement/firing/pause evidence gap, not the remaining
native fullscreen, enhanced settings, automatic real-game recovery, package,
selector launch lifecycle or physical-host performance gaps. Linux/Mac gatesOFF.


Current Windows RHI recheck after c8b8d7d2: all eight accepted-fixture cases pass,
including missing driver/device, both failing, partial cleanup and all three
minimum-capability failures. The default Vulkan case requires the explicit
verified SDK validation-layer path. Accepted fixture SHA256
372a936d5eab97221037075887fd10440adeb5420035a7b231d90c08096585ba.
Finally restores acceptanceOFF and rebuilds; explicit GL and restricted4.1
contracts then pass on fixture9f4e9d0ebd0deb9674703b8d40875c0b95bf37500e2e3a90ce26c14060837113.
Evidence: logs/rhi-final-current-{recovery,restore,explicit-gl,gl41}.log and
rhi-windows-opengl*.json. No production gate was enabled by those tests.
A separate300s actual-selector OpenGL soak is running; its outcome remains
pending, and its handle must be observed rather than launching a duplicate.
