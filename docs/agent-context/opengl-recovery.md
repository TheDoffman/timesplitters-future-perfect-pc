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
