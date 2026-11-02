# SMAA implementation and validation

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

`src/graphics/pc_smaa.inc` owns render-thread resources, compilation, resize,
three passes and cleanup. Input is the existing box resolve at fitted output
size; the final pass preserves black bars and restores output/viewports. All
sampled slots 0–2 are unbound after each pass. Off allocates nothing; shader,
image/view/state or constant-buffer-map failure releases SMAA resources and uses
the original resolve for the remaining device lifetime. Shutdown resets fallback.
Shared D3D11/Vulkan policy; legacy GL retains the key without applying it.

The shared launcher `GameSettings.Smaa`/`SettingsPage` owns the UI and atomic
settings save. See [player behavior](../players/smaa.md).

The unified frontend/pause Video Options adds native selector ID11 after Texture
style. It stages `staged_smaa` against `applied_smaa`; Back discards edits, menu
rebuilds retain them, and resolution confirmation defers the batch. Apply saves
SMAA last, before publishing FPS/textures/subtitles, and rolls back earlier
display/rate saves if SMAA fails. `pc_smaa_settings.h`, implemented once by the
runtime config unit, preserves unrelated runtime.conf bytes and replaces only
the `smaa` setting through an exclusive temporary file and atomic rename. It
publishes a process-local atomic value only after success; the render thread
reads it through `pc_smaa_enabled` without UI-thread GPU work. Startup env values
are superseded only by an explicit successful Apply. The in-game and launcher
controls therefore use the same persisted key; no second preference file exists.

`tests/graphics/test_smaa.py` checks pinned upstream hashes and embedded shader
freshness, then compiles `smaa_rhi.c` against production display/RHI code on
Windows WARP and hardware when available. It covers diagonal smoothing, unchanged flat/axis pixels,
alpha, odd resize, fitted bars, viewport restoration, Off and shader/image/map
failure fallback/recreation. Launcher settings tests exercise persistence,
unrelated-key preservation and the checkbox draft/save/cancel path. Runtime CI
selects the new test. Windows/Mac packages include `Licenses/SMAA.txt`.
Historical patch-migration fixtures omit SMAA until their complete-stack upgrade.
The guest FOV/settings fixture covers the ninth row, staging, Back, rebuild,
Apply and failed-SMAA-save rollback of earlier resolution/FPS writes. GPU fixtures
also exercise the real saved/live model and read-only preference failure.

Native Mac GPU/gameplay acceptance remains separate; see MAC-129 and STATUS.

Video Options setting-row clicks reuse the native switch sound; see
[click feedback and tests](video-options.md).
