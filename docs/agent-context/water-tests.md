# Coupled water operator checks

Detailed surface checks from the [build and test guide](build-test.md).

`tests/stability_transitions_game.py --scenario surface-feedback --run-name
water-live-feedback --seconds 100 --no-captures` replays the committed Scotland
route and requests 16 completed GPU feedback triplets, with isolated Test Lab
saves/settings and executable/XBE provenance. Add `--native-fps 144` for the
native clock while retaining the original water feedback. `--no-captures` skips
ordinary screenshots; this scenario still explicitly reads its feedback fields.
`test_captured_live_feedback` in `tests/test_surface_shader_feedback.py` compares
that local capture with the original translated shaders and coupled one-tick,
two-half-tick and three-piece updates on local hardware. Capture Original60 in
`water-live-feedback-original` with `--native-fps 0` for the second case. Hardware
byte captures must not be used as WARP goldens: UNORM conversion differs between
those adapters, and the WARP suite uses its own original shader reference. Missing local capture
is a skip, never proof. `native_surface_capture.c` is in the dual-host CPU timing
group and verifies copied source order/identities and rejection without partial
publication. Actual Mac RHI readback and game execution remain separate checks.

`native_surface_coupled.c` runs on both CI hosts without assets: 65 controlled
modes, split composition, 60/120/144 trajectories, invalid-input nonmutation,
and positive/repeated/complex roots against independent original equations.
`test_original_surface_shader_feedback` compares fixed-neighbor and 64-square
spatial GPU cases with the unmodified game's float32 output. Separate controls
retain byte-storage differences. Shared GPU forward/inverse transforms are checked
against CPU asymmetric-corner, checkerboard and noise references. Removing the
velocity correction or breaking the inverse transpose must fail after compilation.
These are operator checks, not runtime/performance or Mac GPU acceptance.

`surface_vertex_definition.c` uses the production parser/emitter on private
microcode: the supported XBE's six-instruction vertex program and both original
pixel programs run with synthetic geometry and callback-2BF10 c17-c20 offsets.
Translation helpers use isolated configuration. Single-pass and gradient-only
byte controls isolate rounding; no controlled scalar/storage test alone establishes
a replacement for the original map. The pinned private XBE and Windows WARP are
required; missing-asset skips are not validation. Detailed history is in the
high-refresh regression guide.

The surface spatial fixture also runs at 128x128, matching the observed game
feedback target. Its wrapped case mirrors the live quad, viewport, screen-space
undo and linear wrapped sampling. A seam-adjacent impulse distinguishes the
periodic real Hartley basis from the clamped DCT control; forcing a clamped basis
must fail the original-shader comparison. `native_surface_basis.c` checks shared
basis orthonormality and diagonal-neighbor eigenvalues at sizes 1/7/64/128 under
both boundary rules, plus invalid input, through existing Windows/Mac timing CI.
Live draw evidence and its binary/source provenance are recorded in the high
refresh regression guide. The fractional candidate comparison still excludes
smoothing. `test_original_smoothed_surface_feedback` separately executes the
captured cardinal smoothing and both original feedback passes at 60 Hz, checking
the filter against explicit wrapped neighbors. A zero-offset mutation must fail.
This reference includes actual byte/half/float storage and mixed-format controls;
it excludes disturbances, final shading and game RNG. Do not infer gameplay
equivalence from the controlled float reference.

`surface_final_gpu.inc` executes original effect 22 (callback 2C320, definition
4BC318) on the captured cardinal offsets. It checks final RGB against explicit
height differences, through float and actual byte render targets. The candidate
uses shared `surface_state_shader.h` to materialize alpha-height after each fresh
inverse transform. A constant-alpha header mutation must fail the final shader
comparison. Final shader agreement with the float32 reference does not establish
original byte-storage appearance or scene rendering; both differences are reported.

`test_original_byte_rounding_flow` adds an independent 60-Hz original-shader
reference starting from an actual byte target. `surface_rounding_gpu.inc` holds
the first-pass clipping/quantization residual over each authored interval, executes GPU
fractional transforms/steps at every frame, and writes original byte state only
at interval boundaries. The frame schedule splits steps that cross a boundary.
It compares every boundary's G/B and final byte ripple RGB, with and without
original smoothing, at 60/120/144 and a mixed 144/120/60/144 sequence. The test
clock now uses the production `pc_native_timing_step` with a controlled
72-MHz counter and `pc_surface_interval_split`; the mixed sequence switches at
nonzero phase and produces 117 advancing frames over 60 ticks. A half-second
pause inserted during motion must produce no slices and identical final output.
This is not live host scheduling or resource integration. Mild and strong signed initial height pulses
plus near-limit velocity/height fields are covered. Missing forcing, missing
ambiguity handling, omission of gradient clipping, unlimited acceleration and
missing authored boundary limits are required failing mutations. Saturated
60-Hz final-color repeats must match the independently measured original count;
the higher-rate and mixed schedules still require fresh final colors every frame.
Direct guard cases reject a distant replacement and canonicalize nearby values
even away from ideal byte thresholds. This is controlled trajectory validation,
not runtime integration or measured game performance. The portable coupled CPU
test also checks held forcing across split steps and rejected-output preservation.

Rounding profile 3 (`surface_noise_gpu.inc`) runs original effect-19 vertex/pixel
programs on captured A8L8-to-RGBA source with linear wrap and ONE/ONE RGB-only
blending before smoothing. `surface_vertex_definition.c` checks the four vertex
instructions/input mask 0x010e. Both noise cases explicitly skip without the
private Original60 Scotland frame-3600 capture; no game bytes are tracked.
Controlled UVs replace RNG. Initial draws verify additive green and preserved
red/blue/alpha; duplicate candidate noise must fail boundary equality. Queued
splashes, live submission and whole-scene rendering remain separate acceptance.

`native_surface_interval.c` checks exact game/raw conservation and boundaries
across frequencies, presets, jitter, pause, catch-up, resets and invalid input.
Reset-phase mutation must fail. Guest tests check publication and Original60
clearing. Exact raw metadata retains all60 boundaries during paused144;
summing float deltas loses one. Both hosts run these CPU tests.

`native_surface_mode.c`, in the same dual-host suite, checks immutable mode
packing against the existing affine operator and independent one-tick equations,
including real/repeated/complex roots, irregular split composition and rejected
output preservation. The GPU rounding fixture now uses the shared evolve shader
with two immutable mode textures and a 16-byte time uniform; it no longer creates
coefficient textures keyed by fractional duration. A fifth schedule alternates
144-Hz intervals by +/-0.1 ms while retaining one second of active time. Every
profile includes that jitter, smoothing off/on and pause preservation. Replacing
the evolve shader's first diagonal entry with 1 must fail the byte comparison.

Windows GPU checks: `tests/test_surface_shader_feedback.py --hardware-check
--profile 0` verifies boundaries; `--benchmark --profile 3` measures GPU spans,
not game FPS. Default pytest uses WARP. RTX4070 SUPER results are in STATUS;
other GPUs/Mac remain unverified. Preserve the arithmetic-distance guard and
its removal mutation: hardware conversion thresholds differ from ideal rounding.

The rounding fixture uses the GPU owner. `surface_owner_gpu.inc` checks
input rejection, reset, packed alpha-height, sentinel target preservation,
color/depth and multiple/empty viewport restoration, plus binding cleanup.
Removing reset/output restoration fails. `native_surface_gpu_owner.c` injects
failure at every allocation; `native_surface_gpu_cache.c` checks retained image
identity, transactional replacement, capacity and clear/recreation. These run
in the dual-host CPU suite; Windows shader checks do not prove Vulkan behavior.
`native_surface_native.c` checks phase, boundaries, pause/gaps and default/override
policy. `native_surface_render_guest.c` tests the actual default-enabled guest
adapters at 144 updates/60 events, ABI and Original60 recovery.
`native_surface_runtime.c` injects allocation failure into the real HLE adapter:
no repeated setup until scene/device reset, no bindings changed by empty reset.
All three run in dual-host native timing CI; this is not Mac GPU evidence.
`--scenario surface-native` uses Scotland. Native water is the common-RHI beta
default; `--original-surfaces` forces baseline water, `--native-surfaces` forces
on. `--native-fps 0` selects true Original60. `--trace-surface-images` reads final
GPU images for freshness, not pacing. `surface-native-long` adds a 90-second
stationary window then fire/pause/resume; use `--profile-presents --no-captures
--seconds 230` for host Present timings. Provenance records the explicit/default
policy. Historical patch fixtures exclude both tail surface patches.

Rounding trajectories now use `pc_surface_gpu_begin_bytes`; original-shader and
broken-gradient controls still test residual/clipping behavior. Captured replay
keeps independent unpack/residual setup. The Windows runner's
`--scenario surface-feedback-gpu --run-name water-runtime-gpu --native-fps 144
--seconds 100 --no-captures` invokes the shared solver in the deferred renderer.
Its `feedback.bin.gpu` stores 16 TSWGPU1 records: 92-byte identity plus two
128-square float4 fields. The third captured-feedback test checks matching
metadata, finite values and both states within 32 float epsilons; original byte
endpoints remain exact. A missing capture skips; an existing runtime capture
without its sidecar fails. Per-sample creation/readback precludes speed claims.

`--scenario surface-native-switch --native-fps 240
--trace-surface-images --trace-surface-switches --seconds 230` runs the committed
`scotland_switch` Video Options route with default water policy. It requires
90 images each at240/180/Original60/120. Historical144 cycles require explicit
expected rates in `validate_switch_cycle`; partial/misordered cycles still fail.
After re-enabling, the image may hold until the first authored tick. Guest/cache
tests cover between-tick resumption, time mismatch, constructor cleanup and
suspend/reuse versus destruction. Images do not measure switch latency.
