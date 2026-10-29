# Water coefficient conversion

The supported US constant uploader (`3D9520`) contains four rounded `CVTSS2SI`
conversions. `translation_sse.py` emits `nearbyint` for those instructions and
`trunc` for their `CVTT` counterparts. Both currently use the host floating-point
environment; this is not an independent guest MXCSR implementation.

`pc_surface_coefficient_byte` in `src/graphics/pc_surface_bias.h` follows the same
clamp, binary32 multiplication by 255, and rounded conversion. Under the default
nearest/even mode, `.105`, `.65`, `.98` become bytes 27, 166, 250; `.5` becomes 128.
The former C cast truncated those values and no longer matched the current
translator. Do not restore truncation or hard-code one rounding mode to silence
the original-shader checks.

The float precision override uses that byte divided by `255.f`. Capture and
runtime GPU configurations use the same byte divided by `255.` to retain their
double precision. Both HLE adapters still reject non-finite authored coefficients
before constructing GPU configurations. Experimental gates, resource ownership,
original uploader execution, shader code and acceptance thresholds are unchanged.
The helper is shared by Windows and Mac; no platform-specific policy is added.

Validation routes:

- `test_surface_coefficients.py` executes mathematical references in all four
  rounding modes, every byte midpoint and its adjacent binary32 values, clamps
  and non-finite helper inputs. On x86 it also compares native SSE operations.
  Truncating and ties-away mutations must compile and fail the numeric oracle.
- `test_surface_shader_feedback.py` compares actual translated original uploads
  with the shared helper, then runs original WARP shaders and existing spatial,
  storage, smoothing, reset and output-restoration negative controls. Private XBE
  and captured-data prerequisites remain explicit.
- `test_combiner_constants.py` retains ordered payload, scope, cache, capture and
  replay checks. `native_surface_runtime.c` verifies the actual adapter's rounded
  GPU configuration, non-finite rejection and allocation-failure/reset behavior.

Windows checks do not establish native Mac GPU execution, complete wave/scene
parity, a fresh game release build, or original Xbox floating exception state.
