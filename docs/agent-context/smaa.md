# SMAA implementation and validation

Patch2595 runs after grouped/native target migrations and configures xbox_config
with the active TSFP_SHARED_SOURCE before the
legacy dependency-relative include path. Reused Mac/other-host runtime trees
must not read a different checkout's SMAA settings header. The CMake regression
compiles real settings against a relocated source, with a stale-header negative
control, for both native and Windows source routes in test_smaa.py.

Additive `graphics/rendering/2470-smaa-antialiasing.patch` runs after the final
timing/config migrations. It wraps `d3d8_display_resolve` around the existing box
resolve, then the shared `src/graphics/pc_smaa.inc` RHI passes. `smaa` defaults
off in RecompSettings, survives runtime read/write, and accepts `TSFP_SMAA`.
Prepare every backend's runtime through the full patch loader. No guest or
generated-code change is needed by the feature.

Shaders and immutable lookup bytes are pinned under `vendor/smaa`; regenerate
`smaa_shader.h` with `tools/build/embed-smaa.py`. Only sampler bindings are
adapted; the reference algorithm and lookup bytes remain unchanged. The shader
uses 1x High, luma detection, gamma-encoded UNORM and zero subsample indices.
The generator explicitly decodes the pinned upstream file as Windows-1252,
and reads/writes project shader files as UTF-8. Its check runs in Python UTF-8
mode on every host to catch locale-dependent generation.

`src/graphics/pc_smaa.inc` owns render-thread resources, compilation, resize,
three passes and cleanup. Input is the existing box resolve at fitted output
size; the final pass preserves black bars and restores output/viewports. All
sampled slots 0–2 are unbound after each pass. Off allocates nothing; shader,
image/view/state or constant-buffer-map failure releases SMAA resources and uses
the original resolve for the remaining device lifetime. Shutdown resets fallback.
Shared D3D11/Vulkan policy; legacy GL retains the key without applying it.

The shared launcher `GameSettings.Smaa`/`SettingsPage` owns the UI and atomic
settings save. See [player behavior](../players/smaa.md).

The unified frontend/pause Graphics tab uses selector ID11 for post-process
anti-aliasing (Off/FXAA/SMAA). `staged_aa`, `applied_aa` and `post_aa_row` retain
Apply/Back, rebuild and Keep/Revert behavior. `post_aa_settings.h` maps the
selector to existing graphics/SMAA preferences with rollback on a failed save.
The display resolve suppresses FXAA while SMAA is enabled, so even legacy
combinations run only one post-process method. The launcher uses the same
mapping through `GameSettings.PostAntiAliasing` and its existing save transaction.
`pc_smaa_settings.h` still persists only the runtime `smaa` key atomically and
publishes live after success. The renderer reads it on the render thread.
Startup env values are superseded only by an explicit successful SMAA save.

`tests/graphics/test_smaa.py` checks pinned upstream hashes and embedded shader
freshness, then compiles `smaa_rhi.c` against production display/RHI code on
Windows WARP and hardware when available. It covers diagonal smoothing, unchanged flat/axis pixels,
alpha, odd resize, fitted bars, viewport restoration, Off and shader/image/map
failure fallback/recreation. Launcher settings tests exercise persistence,
unrelated-key preservation and the checkbox draft/save/cancel path. Runtime CI
selects the new test. Windows/Mac packages include `Licenses/SMAA.txt`.
Portable package fixtures assert the license is included; the Windows fixture
also checks the shipped bytes against its source.
Historical patch-migration fixtures omit SMAA until their complete-stack upgrade.
The guest FOV/settings fixture covers the Graphics row, staging, Back, rebuild,
Apply and failed-SMAA-save rollback of earlier resolution/FPS writes. GPU fixtures
also exercise the real saved/live model and read-only preference failure.

Native Mac GPU/gameplay acceptance remains separate; see MAC-129 and STATUS.

Video Options setting-row clicks reuse the native switch sound; see
[click feedback and tests](video-options.md).
