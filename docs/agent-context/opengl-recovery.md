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
defaults to0. No production OpenGL RHI backend is registered yet, so all current
production builds retain Vulkan/no-automatic-recovery behavior. Tests use
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
Depth is remapped from D3D/Vulkan0..1 to GL-1..1. Hardware asymmetric image,
depth, culling, array/cube/volume, MSAA and screen-copy contracts are still needed.

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
Windows RTX4070SUPER/617.42: shared startup/installer checks pass; the shader
contract builds, translates HLSL and rejects invalid HLSL. Generated GLSL passes
glslang syntax/link checks, then the pixel probe creates a real4.1 core context,
links generated shaders and reads expected sampled/tinted pixels. Fetch-only
textures and legacy HLSL vector conditionals compile on the driver. These are
small shader/context contracts, not a complete RHI backend or game acceptance.
All8 existing Windows production RHI cases still pass with Vulkan validation.

Next: implement OpenGL RhiBackend resource/view/state/draw/readback/resolve/query
operations using the shared HLSL compiler; thin native window recovery adapters;
ordinary build/setup/package/license integration and platform acceptance gates;
GPU contracts then isolated menu/gameplay/effects/fullscreen/resize/settings,
bounded performance/stability acceptance. Run native builds/contracts where
available, record exact gaps, and enable only passing hosts. Mac OpenGL is
deprecated; preserve existing legacy-Mac behavior without claiming future OS
support. Existing renderer code is src/native/renderer_gl.c; reuse shared policy
without promoting that separate incomplete legacy frontend to universal fallback.

Local shader commands:

    python tools/build/spirv_cross.py --setup
    cmake -S tests/opengl-shaders -B build/opengl-shaders [host generator]
    cmake --build build/opengl-shaders --config Release --parallel 2
    build/opengl-shaders/Release/tsfp_opengl_shader_test.exe build/opengl-shaders/generated
    build/opengl-shaders/Release/tsfp_opengl_pixel_test.exe build/opengl-shaders/generated

Create the generated output directory first. Native builds require the verified
TSFP_VULKAN_SDK and SDL3 package paths; executables omit Release/.exe on native
single-config generators. These native commands have not been run here.
