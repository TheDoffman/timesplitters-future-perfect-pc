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
defaults to0. The shared OpenGL RHI is implemented and registered in the opt-in Windows
hardware fixture. Ordinary game builds do not compile it yet and retain
Vulkan/no-automatic-recovery behavior. Tests use
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
textureSamples is lowered to backend sample-count uniforms for GLSL410.

`opengl_host.c/.h` is the small window/context boundary: WGL4.1 on Windows,
SDL GL on native hosts, function lookup and swap interval. It does not own the
window or save settings. Native window recreation after failed Vulkan is still
required; existing Vulkan SDL windows cannot simply create an OpenGL context.
Native event-thread, input/debug/window bindings and logical window identity
must survive that replacement. SDL native context code is unverified on-host.

SPIRV-Cross is pinned to SDK1.4.363.0's commit in config/build/spirv-cross.json.
`python tools/build/spirv_cross.py --setup` verifies the2MB source archive and
installs only needed sources/headers/LICENSE into ignored external/spirv-cross.
Normal verification checks installed file hashes. `spirv_cross.cmake` compiles
the same static implementation for each host; no shader-generation fork or
extra compiler process per shader. Setup/package/license integration into
ordinary game builds is still required before enabling the production backend.

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

Remaining implementation/acceptance: ordinary setup/build/package/license wiring;
native Vulkan-to-GL window replacement with event/input/debug bindings and stable
logical window identity; native builds and GPU execution; representative game
menus/gameplay/effects, display modes/resizing, settings preservation, bounded
performance/stability and explicit per-platform acceptance. Native GL4.1 without
ARB_texture_view/immutable storage still needs subset/stencil-view adaptation or
an explicit capability failure before startup is accepted. Other review targets:
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
