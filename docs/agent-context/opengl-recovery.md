# OpenGL startup recovery: implementation in progress

Goal: Vulkan remains primary, no launcher selector, and only Vulkan startup
failure may trigger an accepted OpenGL backend. D3D11 is not automatic recovery.
Branch feature/opengl-startup-recovery is based on local4c287926 (Windows fallback
removal), which follows main694fc53a. No goal completion or universal readiness
is established by this checkpoint. Keep all platform recovery gates disabled.

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
