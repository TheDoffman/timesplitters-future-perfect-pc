# Runtime and patch navigation

Guest dispatch, register/memory primitives and translation tooling are in [own interfaces](independent.md).

Patch filenames use four-digit base-sort slots and descriptive names; see
[patch maintenance](../../patches/README.md). This replaces the repeated `z`
prefixes without changing patch bytes, effective order or context options.
The loader retains its dependency overrides, and recorded old names unwind
using their stored contents. Keep the five legacy migration patches.


Product lowering and its fatal divide boundary are documented in [own tooling](independent.md).


Independent tooling: [ownership and validation](independent.md), including compiled inherited SAR sweeps.

The exact pristine runtime base now lives in `vendor/ts3FPrecomp/source`.
`tools/prepare-runtime.py` verifies blob SHA-256/Git identities, gitlinks, tree
and original commit before creating the ignored runtime checkout. It retains
`d90c929...` as shallow HEAD so existing patch ledgers, pristine `git archive`
fixtures and packaging provenance remain valid. Do not edit snapshot files;
runtime changes remain in the shared patch stack. The origin URL is attribution,
not a setup dependency. `tests/test_runtime_snapshot.py` exercises the importer
with Git transports disabled and applies the entire current stack twice. Importer changes select all runtime CI suites; Mac execution still requires its host.

Procedural wind: [coupled simulation, ownership, exact-clock admission and validation](native-wind.md).

`0550-state-layout-slots.patch` prepares the render/texture-stage slot numbers
for the recognized XDK layout once per layout change. It caches only integer
mapping results: each draw still loads current guest values in original order
and retains conversion, comparison, effect overrides, texture-operation replay,
pixel-shader forwarding and diagnostics. Render states absent from older layouts
still read as zero. No guest address/pointer, state value, pose or texture is
retained by this optimization. The full-module `test_state_layout_slots.py`
compares ordered host-call digests against the reverse-patched original across
all three layouts, changing memory bases/values and host-call feedback. Layout
invalidation and stale-value negative controls must differ from the original.

`0500-sampler-cache-retry.patch` repairs common `d3d8_states.c` sampler ownership.
Create a replacement before changing the cache count/cursor, destroying its
victim or invalidating borrowed stage pointers. A failed allocation leaves no
entry, so later applies retry while existing bindings remain usable. Successful
replacement keeps the original bound-stage invalidation/rebind behavior.
Failure warnings are capped at four per cache lifetime and reset on shutdown.
Descriptors,32-entry capacity and the successful draw path remain unchanged.
This is shared Windows/Mac RHI policy, not an FPS gain or a new backend cache.

The full-module `test_render_state_cache.py` tests failures in empty/full caches;
the pre-fix module must fail both recovery assertions. Nine repeated failures
retain live owners and emit at most four warnings; successful replacement
invalidates/rebinds borrowed stages and shutdown releases each owner once.
Normal binding digests still match the pre-change module and no-reuse, lost-mask
and cached-failure controls remain rejected. Both native CI hosts run these
asset-free checks; they do not force a real driver/device allocation error.

Shader constants: [bitwise dirty state and upload retry](vsh-constants.md).

`0570-strided-vertex-increment.patch` follows the strided-copy specialization in
common HLE `hle_d3d8_vertex.c`. Each attribute copy advances a32-bit guest
source offset and a `size_t` destination offset instead of multiplying per
vertex. Source arithmetic retains modulo2^32 behavior, including wrapped first
indices and strides; destination arithmetic retains the original host width.
`HLE_PTR` still resolves each read, scalar copy order/widths and the fallback
remain, and each gather allocates/reads fresh data. No retained vertex/pose data.
The full gather/guard/wrap fixture is described in [build-test](build-test.md).
Host-call captures occur after gathering: replaying unchanged captured inputs
does not independently exercise this optimization.

Completed local aim/animation state: [optional shared trace](native-local-trace.md).

`0600-vertex-expand-strides.patch` specializes packed-program vertex expansion in
common `hle_d3d8.c`; `0610-vertex-expand-tiles.patch` bounds passes to32 vertices.
Attribute/width dispatch moves outside the vertex loop within each tile;
fresh stream-zero tails and converted prefixes still use a new zeroed allocation
per draw. Parsed prefixes precede tails; per-vertex attribute order is preserved
even when outputs overlap. Raw copies use exact byte widths, SHORT/NORMPACKED3
arithmetic and generic width/format fallbacks remain, and stream lookups/failure
checks keep their original order. The fixed-function selector tail-calls the
unchanged generic body; the packed setup has a portable noinline boundary to
retain that compiler scope. No guest clocks, vertex cache or retained pose is
introduced. `test_vertex_expand.py` compares full output/stride/failure/stream/
allocation/FP behavior against a frozen pre-change body, including guard pages
and independent broken-copy/order/fallback controls. Live results belong in STATUS.

`0540-stable-gpr-refs.patch` and the title's four-site selector keep references
to live integer/vector cells in profiled routines; see [contract/tests](gpr-refs.md).

`0520-sampler-pc-locations.patch` adds default-off `RECOMP_SAMPLE_PC_SYMBOL`
to the existing Windows sampler. One exact executable symbol gets at most512
native PC rows per thread/report, with executable RVA, symbol offset, source
basename and PDB line. A coverage footer counts all matching addresses/samples
and omitted samples; truncated tables cannot support complete differential
claims. Resolution happens only after sampled threads resume. No per-sample
collection or guest behavior changes; source paths are not dumped. Mac's
existing no-op sampler is unchanged; portable reporter fixtures run on both.

`0490-render-state-cache.patch` gives common `src/d3d/d3d8_states.c` bounded
32-entry descriptor caches for immutable blend, depth/stencil and rasterizer
objects. Current bindings borrow cache objects; shutdown destroys each owner
once. Failed creates are retried without inserting a null entry or evicting
an existing object. Full zero-initialized descriptors replace blend/raster
dirty hashes, with last-descriptor fast paths. Material transitions still
translate current state and bind each draw; stencil reference and scissor
rectangle remain dynamic. Sampler recovery is covered separately above. The complete-module
`test_render_state_cache.py` compares emitted binds against the pre-cache
module and checks replacement, failure/retry, shutdown/restart and negative
controls. This is common RHI policy, independent of native FPS/guest updates.

Native walking's C54A0 scope and C596C clear-slide branch are shared through
`pc_native_numeric.h`/`pc_native_timing_guest.inc`; the original geometry remains
in use. See [policy, diagnostic schema and tests](native-player-contact.md).

When release PDBs lack line records, entire retained COFF function sections can
be matched against the current PE while ignoring typed relocation fields.
Require the primary native entry and complete remaining bytes, retain folded
aliases and reject a changed instruction-byte control; a short prefix or nearest
name alone is insufficient to attribute a secondary unwind fragment.

<!-- packet:start -->
Runtime layers: generated Xbox guest calls title hooks in
`external/ts3FPrecomp/titles/futureperfect/src/recomp_manual.c`, HLE in
`external/ts3FPrecomp/src/hle/`, kernel/NV2A services in `src/kernel/`, then Xbox
graphics frontend in `src/d3d/d3d8_*.c` and RHI backends. Those short `src/...`
paths are RUNTIME-relative, distinct from tracked root `src/pc_*.c`.
`hle_d3d8.c` owns Swap/Present integration; `kernel_bridge.c` and
`xbox_memory_layout.c` contain vblank/flip-gate plumbing. Start FPS investigation
there only when timing/trace evidence implicates it. Visible menu names may
not exist as strings in generated guest code; find a measured caller/address.
Sol can request exact public source ranges and lexical references from the local
broker; each result identifies the root/runtime revision and actual file hash.
Generated guest C remains excluded from hosted retrieval.

Profiler names alone do not prove function identity. DbgHelp may assign an
unnamed static PC the nearest public name, and export-only inferred sizes may
span unrelated native unwind entries. Retain raw executable RVAs and actual
unwind bounds before changing a purported hot helper. Chained unwind fragments
may also require primary-entry validation; unresolved identity stays explicit.
The PC reporter's `symbol_complete` flag checks name truncation, not bounds.
Reject dropped-chain windows as complete profiles and separate sampler pause/
reporting overhead from clean Present measurements. See STATUS for the focused
external diagnostic and its limitations.

`tools/apply-patches.py:patch_records` defines effective order (not just filename
sort). The loader stages an unwind of the EXACT previous recorded patch bytes,
then applies the entire current top-level stack before updating runtime files.
Find its ledger with `git -C external/ts3FPrecomp rev-parse --absolute-git-dir`:
`tsfp-patches.json` lives THERE, not in root `.git` or the build folder.

The Temporal Uplink map (#53) uses two `D3DDevice_CopyRects` calls in one
frame: a 640×480 screen snapshot before a clear, then a 128×128 map texture
copy after that clear. The shared HLE hook in `hle_d3d8.c` retains the first
host texture for that frame and copies guest region (176,0)–(312,181) into
the second texture. `d3d8_screencopy.c` performs the scaled GPU texture copy
on RHI; `src/native/gl_screen_copy.inc` supplies the native GL adapter. The
production RHI fixture in `tests/rhi_screen_copy.h` checks that the saved
image survives a clear. Reproduce visually with Scotland the Brave's Uplink
map mode; inspect the hologram rather than a replay-only frame, since replay
captures do not serialize host screen copies.
A conflict "during unwind" means current runtime/recorded stack diverge; it
does not establish a bad new patch. Preserve edits and investigate the named
hunk; never delete the ledger, reset the runtime or remove dependency patches.
<!-- packet:end -->

`0720-world-health-bars.patch` handles #102; `0730-world-shields.patch` extends
it for #358. Electro Chimp health bars use 16-byte transformed vertices; the
shielded enemy in Something to Crow About uses 24-byte 20-vertex strips. Both
have depth near 1 and already follow the widened camera, so the automatic 4:3
HUD squeeze shifts them toward screen center. Ordinary HUD sprites at depth
near .001 and explicitly placed overlays retain their placement. Validate
with Monkeying Around → Electro Chimp Discomatic and Something to Crow
About's shielded robot at a widescreen resolution.

`0620-zero-base-indices.patch` follows the renderer/native stack. Pushed-array
draws with minimum index zero pass their current index bytes directly to the
synchronous host draw; other ranges retain allocation/rebasing. Vertex gathering,
primitive conversion and draw order are unchanged. No retained index cache.

`0560-strided-vertex-copy.patch` dispatches pushed-array attribute copies by byte
size once per attribute, keeping common1/2/3/4/6/8/12/16-byte copies constant
inside the vertex loop. It preserves original32-bit guest address arithmetic,
attribute order, zero-filled holes, readability checks and allocation ownership.
Unusual sizes retain the original generic copy; no geometry is cached. The
change is shared by Windows/native Mac. See `test_push_gather.py` for complete
original-function comparisons and guard-page read/write limits.

`0650-global-compare-load.patch` routes direct, unsegmented 32-bit CMP operands
through the existing `RECOMP_LOAD_GLOBAL_U32` policy, just like MOV. Flag snapshots
read each operand once, including fused and separated branches. Indirect, narrow,
segmented and TEST operands retain original behavior. Regenerate guest C after
applying it. The shared title policy opts in only audited addresses/sites.

`0410-model-transform-entry.patch` adds a default-off RECOMP_FAST_ENTRY policy
hook after entry tracing and before input loads. Explicit return tracing and
forced-return diagnostics keep the generated body. US policy selects only151E80;
other titles/addresses retain the original path. A zero result must leave guest
state unchanged; success includes exact registers, memory and stack return.

`0020-dsound-output-headroom.patch` defines the shared default host gain in
`src/hle/audio_output.h` and uses it in XAudio initialization. The tracked
`src/native/audio_sdl.c` uses the same default. Guest volume/timing is unchanged.

The additive `controller-profile-routing` patch repairs #319 in shared
`src/input/input_bindings.c`: parse-time classification of keyboard-only pad
profiles enables pad defaults after platform focus checks. It follows the
keyboard-settings/weapon-slots patches; no generated guest hook is needed.

`0390-keyboard-reload-buffer.patch` runs after the full input/timing stack. It
collects configured Reload/X and Fire/trigger keyboard contributions separately,
passes them through the shared host model, then merges untouched pad magnitudes.
No generated guest input or controller behavior is rewritten by this patch.

The final `radial-stick-deadzone` patch changes shared analog axis sampling in
`src/input/input_bindings.c` to use the root `pc_input` radial helper for both
movement and look. `tools/apply-patches.py:patch_records` places it after
`0390-keyboard-reload-buffer.patch` so the historical keyboard patch still
applies during branch transitions. Keyboard normalization and stick-button
thresholds are unchanged; `tests/test_patch_updates.py` covers those transitions.

## Source ownership and execution

`0590-texture-bind-scan.patch` runs after the renderer/native stack. Texture
binding resolves the first matching cache entry and any framebuffer alias in
one fresh scan. Null bindings clear both outputs; nonzero stages request only
an entry. No retained index, checksum/upload or framebuffer-copy policy changes.
The full-module framebuffer fixture compares all cache sizes through512 against
the original two searches and rejects first-only/last-entry regressions.

`0580-texel-sample-lanes.patch` groups the texture survey's four independent hash
lanes. Sample addresses/order, all checksum bits, small-input handling and tails
match the original scalar recurrence. It changes neither sampling/full-check
frequency nor upload/invalidation policy. Tests compare actual production code
with the frozen scalar reference, including exact word-read footprints/bounds.

`0530-shader-source-lookup.patch` adds a guest source-address hint to the existing
HLE slot-program cache. Every hinted hit still compares the full current
microcode with the immutable host copy and requires the same instruction count.
Misses use the original content hash, collision check and creation path; guest
calls, selected-slot rebinding, declarations and host-program lifetime remain
unchanged. Relocated identical code updates the hint. This avoids repeated byte
hashing without treating guest addresses as immutable shader identities.

`0480-pushbuffer-diagnostic-policy.patch` makes `nv2a_pb_scan.c` read both
RECOMP_PB_SCAN and RECOMP_PB_EXEC once, on its first scan call. These are launch
diagnostics; presence (even empty/"0") retains the original opt-in semantics.
Disabled or empty/reversed intervals return before obtaining guest memory.
Enabled decoding, independent executor mode and report output stay intact.
The complete production scanner is compiled in `test_pushbuffer_policy.py`,
including eager-environment and executor-disabled negative controls. This
removes repeated environment lookup from the acknowledgement worker; it does
not change busy-bit acknowledgement, fences, clocks, or idle/wake policy.

`0450-native-trace-quiet.patch` applies after native timing. With an explicit
nonpositive trace budget and no watch/dump/filter/profile environment switch,
`recomp_trace_enter/exit` bypass disabled diagnostic dispatch. The entry guard
follows the existing renderer-idle callback. Per-thread configuration is read
once, like existing diagnostic caches; explicit diagnostics retain their original
budget, initialization and output. Crash/throw/unimplemented reports are untouched.

The quiet guard now inlines its cached TLS load and outlines only first-use
environment parsing. It retains per-thread startup configuration: changing the
environment does not alter an initialized thread, while a new thread initializes
independently. `test_trace_filter.py` compiles the complete tracing module and
checks that distinction, concurrent quiet calls, enabled diagnostics and Windows
idle cadence; a shared-cache mutation must fail. No process-wide cache or C
atomic support is introduced. Private alternating native probes measure tracing
cost only; game pacing requires a separately identified executable and scene.


`0420-native-demo-timing.patch` follows the native surface stack and wraps US
`43220` with `pc_native_demo_guest.inc`. Regenerate guest C: the original is
`sub_00043220_gen`. Timer multiplies `433E2/43473` use the wrapper's raw-time
scope. Numeric load `433A0` can skip recording or zero-time playback; cursor
store `42535` resets both histories. The existing wrapper includes the bounded
`pc_native_demo_playback_guest.inc` scheduler and calls `pc_native_visual_rebase`
after original decoding. No additional wrapper/patch/generated edit is needed.
Historical pre-native upgrade fixtures exclude the dependent demo patch.

`0510-sampler-caller-depth.patch` keeps captured CPU caller chains up to the
existing 32-frame walk limit instead of silently truncating at six. The opt-in
Windows sampler keeps its 8,192-key per-thread cap, reports dropped new chains,
and clamps the displayed requested depth. Normal execution has no new work.
Portable insertion tests run on both hosts; Mac still lacks this Win32 thread
suspension/unwind sampler. No guest timing or rendering code is changed.

`0440-native-surface-render.patch` follows capture and connects the native-water
path, enabled with 120/144 beta on common RHI. `TSFP_SURFACE_NATIVE_TEST=0`
is a diagnostic opt-out; Original60 and legacy GL keep original feedback. `pc_surface_native_guest.h` forces only the
outer dispatcher on positive matching native intervals; nested event/smoothing/
feedback calls skip partial frames with the original stack return. Whole frames
retain their original calls, including the single feedback update on catch-up.
Final-pass prepare/finish and constructor reset are copied deferred operations.
`d3d8_surface_field` overrides the sampled view only for that final pass;
`d3d8_surface_shutdown` invokes the registered owner cleanup before device teardown.
Original byte targets and guest identities are never replaced by float storage.
Legacy GL keeps its original rendering; this candidate needs common RHI access.
The sampled-view hunk anchors only common frontend code, so stacks without the
optional texture-pack override can still apply and reverse the same patch.

`0430-native-surface-capture.patch` runs after the existing native surface tail.
It wraps the original `195110` feedback call with `pc_surface_capture_guest.h`
and includes `pc_surface_capture_hle.inc` beside the HLE texture cache. The
optional `TSFP_SURFACE_CAPTURE` diagnostic copies resource/texture identities
and coefficients before the guest call, then queues a readback after its two
draws. It resolves existing host textures by the copied full identity; it neither
creates textures nor calls guest code from the deferred queue. The original
feedback, RNG calls and guest ABI remain unchanged. This observes completed GPU
pixels, unlike capture records containing stale render-target system-memory
copies. The probe is bounded to 16 Scotland samples and is not a pacing test.
The optional `TSFP_SURFACE_GPU_CAPTURE=1` branch of that existing callback now
executes `pc_surface_gpu_begin_bytes` and fractional shared-kernel updates using
the actual source views. It releases all diagnostic GPU resources before
returning and keeps the original visible feedback/final passes. No further patch
or guest wrapper was added for this extension. That diagnostic still preserves original visible output; the separate native
render hook above supplies persistent owners and event/fractional integration.

Issue #222 changes the shared include reached by the existing bot frame wrapper;
it adds no guest wrapper, wire field, generated-code edit or patch dependency.
`src/pc_network_bot_motion.h` owns replica presentation between host snapshots.
The existing title integration includes it through `pc_network_bots_guest.inc`.
Apply the complete patch stack and use the supported builder as usual.
Vehicle exit uses the original logical input mask 0x40 in `D2470` (`D254F`
queries `427B0`, `D255B` sets driver input+0x18). `17DAF1` consumes that action
and respects vehicle data+0x58 flag 0x10 before routing `17C740`. Do not infer
logical masks from Xbox analog indices: the default keyboard Y delivers this
exit action, while F/R deliver 0x20/0x10. See the live exit scope in STATUS;
this audit changes no input binding or exit mechanics.

Original contact-term routine `168720` sets penetration escape velocity using
the authored 60-Hz constant times 0.2 and clamped overlap (0..0.1 world units),
then combines it with restitution at contact+0x20. That output is a velocity,
not per-frame displacement. Do not multiply it by the render target. The full
rigid-motion fixture checks nonzero depth, rebound and quantized normals against
an independent impulse equation; source/fixture evidence does not replace live
contact generation and terrain coverage.

`pc_native_contact_guest.inc` observes exact `172114 -> 171560` solver entry
before the RNG scope changes. It runs in Original and native modes when explicitly
enabled. `pc_native_contact_trace.h` reads the current generated arena and selected
pointer array, retaining each list separately with body pose/velocity and solver
intervals. The CSV's `generated` rows mean allocated arena records as they exist
at solver entry: selection `167540` may already have merged/changed records, so
these are not untouched producer outputs or a trace of each selection decision.
`171FB0` resets the arena after solving; asynchronous empty-arena reads
cannot establish absence of contacts. The shared host adapter checks readable VM
spans with `pc_guest_memory.h`, discarding its mapping cache before guest execution.
The optional diagnostic writes no guest state and does not measure pacing.

The rigid-motion fixture checks original contact solver `16FC00` and integrator
`16CA00` against analytical static-contact impulses across signed axes, eccentric
points, inverse masses and native intervals. Keep authored linear drag in the
expected bound. This protects impulse/time units but controls contact generation
and selection; the live pillar outcome remains unresolved. Original `170500`
calls `16D380` and additionally `175510` for vehicle class `0x400000`;
`170690` selects contacts.
These boundaries need separate geometry evidence before changing mechanics.

Vehicle chase camera `CA040` (modes 24/32) uses physics-history average
`165650` for elapsed time. Its entry whole-tick gate now follows positive
live game time. The `CA424` count load retains carried groups of five spring
substeps, but supplies one group on an advancing zero-whole interval to avoid
zero division/loop underflow. Original60, authored coefficients, target/world
geometry and multi-tick groups retain the original code. Pause rejects stale
whole counts at entry. The full-original `native_vehicle_camera.c` fixture
controls collision services/history/targets and host CRT math; it is not live
terrain acceptance. See STATUS for frame-bracketed live camera/body readings;
these asynchronous observations are not exact completed-update traces.

The candidate `-native-vehicle-physics.patch` adds a default-identity
`RECOMP_LOAD_U8_AT(site, value)` hook to register MOVs from byte memory and
the input of byte-memory DECs. The latter keeps its flag snapshot on the
actual stored result, with CF unchanged. Title policy changes only US vehicle
rest load `176673` and rigid sleep DEC `171F83`; other loads/decrements retain
their original value. Their clocks use the slower raw/game authored cadence,
including phase-shifted carried ticks. The loader places it after the numeric policy
and before `-native-surface-live.patch`. Historical pre-native stacks exclude
this dependent patch; current full-stack application and unwind remain required.
See `tests/test_native_vehicle_timing.py` for synthetic byte semantics and local
original vehicle/rigid-clock fixtures. The original solver/body math and a
controlled support-contact path now run in `native_rigid_motion.c`, followed by
the original `1719E0/165A40/9E840` entity publication/bounds path and
`1677E0/167880` sleep/wake. The vehicle-specific component updater `1753A0`
also runs with controlled parameters. Material, world geometry, damage and
model notification remain service boundaries; driver/handling is unverified.
`pc_native_physics_guest.h` wraps `171FB0` with a nested, thread-local scope.
Call `172114 -> 171560` assigns carried whole primary steps to the original
shared RNG; extra primary steps use a private advancing sequence only at
shuffle calls `1715A0/170E51 -> 153B40`. The original generator still runs;
its two seed words and count are restored after private draws. Other gameplay
RNG calls stay original. The three-contact routine fixture checks fresh
substeps and identical shared seeds at 60/120/144. Numeric ADDSS site `17164F`
snaps the accumulated secondary time to `8*step` only within four float epsilons
of that boundary; otherwise rounding could run a ninth solve. Original timing
is unchanged. The added manual `171FB0` wrapper requires supported guest
regeneration before building. Its `_gen` extern must be a standalone declaration: the
generator's manual scanner does not recognize comma-separated declarations.
The corrected `ba4f0bf` Windows build links and drives the Siberia jeep, but
matched throttle replays reveal less travel at native rates. Full handling/
collision acceptance remains pending, and the prior playable binary is restored.
The live jeep callback at vehicle-data `+0x68` is `177AE0`; dispatcher `17E680`
calls it before `174860` force and `174D60` component-force preparation. Treat
these as investigation entry points, not a proven acceleration root cause.
The subsequent original driver fixture proves `17DA03` still skipped positive
fractional intervals while `17E680` cleared throttle/brake fields `+530/+534`.
The shared numeric gate now admits every advancing native interval there too.
`17DA00 -> 172EE0` runs in `native_vehicle_driver.c` with controlled action,
model and wake services; original mode and zero-float pause retain their gates.
Steering easing in `172EE0` computes a speed-dependent target, multiplies its
error by live game time at `173062`, then by 0.07 at `17306A`, and publishes
the result to eligible wheel components. The latter site now shares the
existing aim-easing coefficient subdivision so fractional raw intervals retain
the original one-tick response at the current game speed. Whole/catch-up steps
and overshooting weights retain original arithmetic. This is separate from
the unresolved real-terrain trajectory differences.
Build `67c1864` passes and matched live initial acceleration now agrees within
0.1% at the three presets. Later pillar/bank contact diverges, so full handling
acceptance remains separate. That historical candidate was superseded by `vehicle-d0116a7`;
the earlier candidate binary was recycled and the playable crowd pair restored.
Contact-velocity gate `166899` is now also keyed by positive live game time:
the original `1667C0` otherwise omits a character's displacement-derived
velocity on zero-whole native updates. Its temporary EDI is immediately
restored and the guarded division already uses float game time. Static and
rigid-body contacts are unchanged. `native_contact_velocity.c` runs original
`1667C0/166060`; the motion fixture also executes original `16E780/16DA90` up
to a controlled body callback, checking the reported relative velocity. Its
zero material coefficients disable actual damage/effects. This does not
establish live character collision or explain the static-pillar trajectory.
Spawn gate `172AB5` now also uses advancing game time. Original `1727A0`
creates a body from a model joint and optionally inherits its movement; this
is a one-time spawn, not a continuously updated collider. Its existing
`172B01` division already uses fractional time, but the integer gate dropped
initial velocity/momentum on fractional spawns. `57040` copies joint history
from the model matrices (called at `A7425`); `native_spawn_motion.c` runs that
original copy, spawn, matrix math and `167CE0/167C00/167AE0` setters with
controlled allocation/model services. Caller `2107E8` supplies a nonzero
inheritance factor; `1FA7B2` explicitly disables it. Live spawn/rendering
acceptance remains separate from this routine evidence.
The supported Windows `3e1ee87` build passes with both contact and spawn
changes. Its candidate EXE/PDB are retained in `vehicle-3e1ee87`; the older
`vehicle-67c1864` pair was recycled, and the normal playable pair restored.
No live contact/spawn check of this new build has run yet.
The next actor/vehicle contact correction changes only divide `17CBD6`:
`A7360` already publishes actor `+F8/+FC/+100` in units per authored tick,
so native `17CAE0` needs a constant 60 conversion to units/second. Dividing
by the event interval again doubled the reported speed on carried-tick120
frames while leaving zero-whole frames correct. Gate `17CBAF` is deliberately
unchanged. Registration `182C04 -> A5CE0` stores the callback at `+170`;
player collision `C4A3F` and queued `16C126` deliver it. Queue production
`1709F0/170B53 -> 16C070` drains through `171FB0 -> 16C0D0` before entity
post-update in `A7870`. `native_vehicle_actor_impact.c` couples original
publication with direct/queued callback, contact construction and suspension
impulse response; geometry/motion and final effects remain controlled.
`0370-trace-filter-disabled-scan.patch` moves the function-name length calculation in
runtime `recomp_trace.c:trace_only_match` after the empty-filter return. Lifted
function entries therefore do not scan diagnostic labels when
`RECOMP_TRACE_ONLY` is absent/empty. Enabled exact-name matching and per-filter
hit limits retain the original implementation; watch/dump/profile, trace budget
and renderer-idle behavior are unchanged. This applies to both host builds.
`tests/test_trace_filter.py` compiles the actual patched function and requires a
restored-eager-scan mutation to fail. Removed scan counts are not an FPS result.
`0040-gpu-pfb-flush-ack.patch` adds a default-no-op `RECOMP_AFTER_STORE32` hook to
ordinary 32-bit MOV memory stores. It runs after the original write, excludes
segmented/narrow/register operations, and preserves existing numeric hooks.
The title policy (`pc_gpu_ack.h`, included by `pc_native_numeric.h`) selects only
verified US instruction `0x3D66C3`, before its PFB flush poll at `0x3D66D0`.
Runtime `xbox_Nv2aNotifyPfbFlush` signals an auto-reset event after validating
that exact register and the active service. The caller changes no guest word;
the existing worker retains its busy-bit/DMA/fence pass and 500-us idle fallback.
Windows waits on the event plus its existing high-resolution timer. The native
adapter `w32_WaitForEventMicroseconds` waits on the event condition variable with
a monotonic deadline (relative pthread wait on Mac), avoiding multi-wait polling.
Both hosts retain the existing 50-ms maximum wait for large diagnostic idle
values. The new native declaration precedes CreateEventA, preserving the old
SetEvent/timer context used by historical Mac patch upgrades.
Queued requests coalesce without an explicit reset. Shutdown publishes stop,
drains notifier users, wakes/joins the worker, then closes the event; thread-start
failure closes it too. Event creation failure retains polling. The cached
`RECOMP_PFB_WAKE_ACK=0` switch disables notifications for same-binary diagnostics.
No clock/settings policy changes. Regenerate after applying the patch. The former
direct-clear prototype stalled loading and is rejected; this rework requires
fresh live acceptance, not just offline checks.

| Layer | Actual location | Change/reproduce through |
| --- | --- | --- |
| Host policy, settings, input, shared helpers | Root `src/pc_*`, `src/graphics/`, `src/native/` | Tracked source; patched upstream CMake includes it |
| Runtime integration and title hooks | `external/ts3FPrecomp/titles/futureperfect/src/recomp_manual.c`, title `CMakeLists.txt` | A separate tracked patch |
| HLE graphics, audio, input, movies | Runtime `src/hle/hle_d3d8*.c`, `hle_dsound*.c`, `hle_input.c`, `hle_xmv*.c` | Tracked patch |
| Xbox kernel/thread/memory/NV2A | Runtime `src/kernel/`, `src/nv2a/` | Tracked patch; distinguish Win32 and native adapter paths |
| Xbox GPU frontend / host backend | Runtime `src/d3d/d3d8_*.c`, `rhi*.c` | Tracked patch plus shared root shader policy |
| Generated guest functions | Runtime `titles/futureperfect/src/recomp/gen/` | Generator/manual hooks; NEVER direct edits |
| Lifter and symbol data | Runtime `tools/recomp/`, `scripts/recompile.py`, `config/extra_symbols/` | Reproducible generator patch, regenerate |
| Build products | `build/windows/`, `build/macos/` | Supported builder; not an assumed location for guest C |

Generated functions often have address-based names. Do not infer a menu handler
from a plausible label. Search only the evidenced address/symbol, inspect bounded
lines and trace the actual caller. `recomp_manual.c` wraps selected functions;
generated declarations are in `recomp_funcs.h`. Use
`python tools/recomp-generation.py external/ts3FPrecomp` to check the fingerprint.
New HLE declarations/manual wrapper names can require generation even when a
small function-body change only needs compilation. Follow the builder's check.

## Patch lifecycle

The combined vehicle/main stack keeps surface coefficients before preservation,
vehicle physics before live surface support, then full-float preservation and
flip handoff. Numeric addition dispatch retains both physics secondary-step
rounding and Escort recovery; neither replaces the other during integration.

Top-level `patches/*.patch` is the active stack. The legacy subdirectory contains
migration inputs only. The loader moves networking ahead of native boot, build
identity after Discord, and the presentation integration series to the end;
consult the generated ordered index instead of guessing from the number of z's.
The `-late-frame-pacing.patch` immediately follows `-fixed-60-fps.patch`
within the timing tail, before the later native numeric/surface patches.
Earlier 144-FPS, presentation-timing, recovery and shared-presentation patches
remain structural dependencies even though higher-FPS behavior is disabled.
The loader orders the late-frame correction explicitly; do not rely on filename
length. Its actual Arm-body contract is in `tests/test_frame_clock.py` using
`tests/flip_gate_deadline.c`: a cheap frame waits, a late frame consumes an
already signalled vblank without another tick, and no signal means no bypass.
The bypass no longer requires the retired interpolation idle callback.
`-native-flip-handoff.patch` follows the timing tail and moves the prior gate
boundary into shared `g_flip_gate_previous`. Both native Swap and Present-only
paths call `xbox_Nv2aFlipGateNativeFrame` to observe the last real vblank without
waiting or consuming its event. This lets the first return to Original 60
consume the tick already awaited by the original guest clock, including games
that started at native 120/144 before Arm ever ran. Arm still requires a real
signal and sufficient elapsed time; cheap original frames remain paced.


The unverified #105 per-thread flip-gate experiment was excluded from the active
stack; its recovery copy remains in ignored logs. The verified repair combines scoped
prompt-read validation with the late-frame pacing correction, not thread-owner
changes. Windows measured 53.8-54.3 FPS in weapon set versus the original ~20;
see STATUS.md for scope, provenance and pending Mac validation.

Create a new patch against the **correct preceding effective stack**, validate
hunk counts/context with the complete loader and transition tests. A dry-run of
one patch against pristine upstream does not validate integration. Adding a new
patch whose assumptions depend on the fixed-60 tail may need an explicit loader
ordering change and tests; a longer filename alone is insufficient.

On a conflict, record upstream HEAD, dirty runtime paths, ledger location,
reported phase/patch/hunk and proposed recovery. The loader uses a small temporary
source tree, not a duplicate game extraction. It preserves unrelated changes and
checks for concurrent modifications. Escalate a specific provenance/recovery
question when needed; repeated application with unchanged bytes cannot fix a
conflict. Do not alter MSVC flags to compensate for an unexplained patch failure.

## Renderer and timing boundaries

### Following one frame without rediscovering the tree

Start with this chain, then verify the current source and conditional branch:

1. Guest call declarations/manual hooks connect lifted Xbox calls to HLE. Inspect
   runtime `titles/futureperfect/src/recomp_manual.c`, `src/hle/hle.h`, relevant
   `HLE_EXPORT`/`HLE_ORIGINAL` declarations and `tools/recomp/`. Root
   `tools/recomp-generation.py` fingerprints these inputs. New wrapper names can
   require regeneration; a host implementation body edit may only need a build.
2. `src/hle/hle_d3d8.c` implements `D3DDevice_Swap` and Present integration. The
   observed desktop Swap path counts a guest swap (`xbox_FpsCountSwap`), arms the
   flip gate (`xbox_Nv2aFlipGateArm`) and invokes the title's original Swap through
   `HLE_CALL_ORIGINAL`. Read the current branch/guards and associated timing
   counters before interpreting a slow frame; symbol presence is not a profile.
3. `src/kernel/kernel_bridge.c` supplies the vblank tick. Its observed release
   call to `xbox_Nv2aFlipGateRelease` precedes the connected-interrupt check.
   Do not assume a menu with no connected ISR should receive no gate release.
   Inspect deadline advancement/resynchronization and actual thread delivery.
4. `src/kernel/xbox_memory_layout.c` owns gate configuration, event/counters,
   Arm/Release/idle behavior. An arm's configuration, first-vblank and event
   readiness checks are distinct from the source of the vblank tick. Compare
   thread IDs and arm/release/wait measurements before changing synchronization.
5. Xbox frontend state/resources/draws are under `src/d3d/`; host adapters select
   the RHI/window backend. A guest Swap count, host API present, GPU completion
   and monitor scanout are separate observations. Use the relevant contract or
   bounded captured-frame/scene check to isolate that layer.

These are navigation facts checked against pinned runtime `d90c929` with local
patches, not a verified cause of #105. The working runtime can contain edits or
a partially migrated stack. Record file hashes/ledger/build provenance; request
fresh source if it disagrees with a guide. Generated guest C stays local and can
be investigated by LOCAL without transmitting it to the hosted advisor.

Common Xbox frontend: `d3d8_device.c`, `d3d8_resources.c`, `d3d8_states.c`,
`d3d8_shaders.c`, `d3d8_screencopy.c`, `d3d8_display.c`; backend selection through
`rhi.c`, `rhi_d3d11.c`, `rhi_vulkan.c`. Legacy Mac GL remains separate. Shared
algorithms live in root `src/graphics/` and settings in `src/pc_debug_options.def`.
See [shared-platform](../shared-platform.md) for adapter and GPU contract details.

Original rendering defaults to 60 and the console interrupt clock remains 60 Hz.
At `c932bee`, launcher opt-in native 120/144 beta settings require a compatible
game build; that selector does not prove the current runtime supports them.
Historical interpolation names and tests do not restore the retired engine. API presentation
counts are not GPU completion or display scanout. To investigate slowdowns,
compare stable/slow/stable timing, distinguish CPU work, GPU work, synchronization
and intentional waits, then run one discriminating intervention. Issue #105 measured repeated text VM queries plus late-frame gate quantization;
see STATUS.md for its scoped Windows acceptance.

## Auto display and cinematic matte hook

`1330-auto-display.patch` changes the
`sub_0001EFB0` cinematic matte hook to `pc_display_effective()`. It must consume
resolved physical output dimensions. The renderer uses effective-mode presence
for camera/UI adaptation, retaining guest output if no usable window exists,
while menu save/preview code retains the
selected Auto sentinel. Apply after the original cutscene-letterbox patch;
`test_cutscene_letterbox.py` checks the resulting production wrapper.

## Crane and remote-view vignette (#221)

`*-crane-vignette.patch` wraps the US `sub_00137010` drawing helper through
`pc_display_aim_panel`. Its four mirrored `holoedge` quadrants and optional
side fills share full-view placement, flushed before and after the group.
Crane (`sub_001371A0`), microscope (`sub_00137720`) and remote-gun/view
(`sub_00137E30`) callers keep their following text outside this scope. The
existing full-view guard retains original 4:3, partial and multiplayer paths.

The captured crane also uses an inclusive 639x479 last-pixel scissor. The
crane patch's RHI hunk applies shared `pc_ui_vignette_clip` to a temporary
rectangle before scaling; it never changes the guest-readable scissor state.
Legacy GL uses the same helper. This prevents the amplified right/bottom gap.

## Character effects update gate

`pc_native_numeric.h` maps the US1260C0 whole-tick load to positive live game
time in native mode. This dispatcher is called by player/NPC updates; its
attached-effect timers/transforms already use fractional delta. The old gate
lost subticks (11FEC0's timer advanced0.5 seconds per second at120). EAX is
replaced before arithmetic, so this changes eligibility only. Original mode,
whole-tick fields and zero-time pause remain unchanged. The original-dispatcher
fixture covers11FEC0's timer and real matrix helpers; other nested systems and
rendered effects need separate evidence. No generated C or new patch is edited.

The status-3 growth path (`120AB0`) compounds its scale matrix each update.
Only its `120BBF` call to constructor153600 now uses
`pc_native_character_guest.h`: recover the one-raw-tick scale at the current
game speed, then raise it to the fractional raw interval. Rotation, composition,
timer and cleanup execute original code. Inactive/whole/catch-up updates remain
exact; stack arguments are restored after the constructor. The model fixture
covers complete original helpers, uneven intervals and live preset changes.
This proves transforms, not rendered status-effect activation or emission.

## Native FPS integration with current main (PR #285)

The effective tail now installs native timing and retires interpolation before
fixed-60 defaults, then applies main's late-frame gate correction before the
remaining native update/surface patches. Original 60 remains the default;
`pc_native_timing_guest.inc` commits requested 120/144 changes at update boundaries
and HLE skips the original flip gate while native timing is active. Keep both
`test_frame_clock.py` (real Arm body) and `test_native_timing.py`; neither alone
validates the combined stack. Water precision experiments remain off by default.
`PcNativeStep` additionally carries exact host-side game units, start phase and
units per tick. The shared guest timing hook publishes `g_tsfp_native_step`
on native updates and clears it on Original-mode updates. This does not change
guest word layout, float deltas, whole ticks, ISR or pacing policy. The metadata
is a snapshot before guest-local time overrides; a future procedural consumer
must honor those scopes and its own resource initialization/reset lifecycle.

## Outlined text read scope

`*-text-outline-scope.patch` wraps US `sub_00041E30`, whose synchronous body
draws four outline offsets and then the foreground through `sub_00040FA0`.
The existing prompt read scope now spans those five passes and ends before
returning to the caller. The wrapper preserves the original draw routine and
guest ABI, enables the same path for `TSFP_PC_CONTROLS`/`TSFP_NATIVE_PROMPTS`,
and performs no AI, animation, rendering-quality or frame-rate throttling.
The mapping cache remains read-only; prompt scratch writes query live protection.
Adding the manual wrapper requires the normal guest regeneration/build route.

`*-text-panel-scope.patch` extends reuse across the original shared HUD panel
`sub_0012E3F0_gen`: width/fit measurements and subsequent label draws share
one read scope. `pc_panel_text_original` is passed into the existing placement
wrapper, so its pre/post sprite flushes execute outside the cached lifetime.
Font metrics, label contents and all original drawing remain live; this caches
mapping metadata only. The panel's font/colour setup is synchronous and does
not change VM protection. Scratch writes retain fresh writable checks.

## Experimental water coefficient precision

`*-native-surface-coefficients.patch` follows `*-native-surface-constants.patch`
in the explicit loader tail. It extends the existing HLE water float-constant
scope with copied original parameters and precise coefficients at bank 0 stages
3/5/7; bank 1 stage 6 still supplies centering. The generic deferred/capture/replay
payload format is unchanged. Scope clears, parameter refresh at equal timestep,
invalid input and duplicate suppression are tested from the production functions.

## Experimental full-float water storage

`*-native-surface-float32.patch` follows the preservation/live-surface patches.
It extends the existing common-RHI preservation transaction to A32B32G32R32F,
including reversed 32-bit channels in CPU capture storage. The existing shared
resource adapter already allocates/uploads this format. Half-float conversion
remains supported. Legacy GL still declines the experiment as documented.

## Keyboard inventory selection

The additive `*-keyboard-weapon-slots.patch` wraps US `sub_001BF140` after
its original body. The shared `pc_weapon_slots_guest.inc` selects owned usable
inventory through the original variant/cycle predicates and switch/HUD calls;
see systems.md for the verified addresses. It preserves requests across native
frames without a simulation tick and expires old requests after 150 ms. Host-only
bindings are appended after Xbox controls and never emit controller buttons.
SDL focus loss clears the request/rearm state; capture state is atomic across
SDL and guest threads. Regenerate guest bodies through the supported build when
adding/changing the wrapper, and run `tests/test_weapon_slots.py` on both hosts.

## Keyboard menu callback integration

The additive `keyboard-settings-menu` patch blocks port-0 input dispatch during
shared binding capture (before controller/focus guards), and limits parsed
keyboard profiles to two sources per action. Controller profiles retain four.
The themed editor lives in `src/pc_keyboard_menu.inc`, included by the existing
display-menu hook. It uses verified unused callback bytes in the US executable;
no generated C is edited. Menu construction, updates, row drawing and live saves
remain shared between Windows and native Mac. Device illustration geometry
uses the original menu primitives, not external artwork.

Binding parsing preserves explicit `none` positions. Saving two slots keeps an
empty Primary distinct from a populated Alternate, including after restart.

## Host save storage adapter

The additive host-save-storage patch adds read-only xbox_save_root_utf8 after
kernel path initialization. Windows main and native/main.c use it to resolve a
persisted next-launch save location before kernel bridge/guest access and publish
the final root to shared storage metadata. Console disk geometry and device IDs
are unchanged. pc_display_menu lookup supplies scoped host row selection/icon
callbacks and a guarded native profile update; generated C is not edited. All
other profile states continue through the original guest callbacks.

The later live-profile-storage patch adds xbox_set_profile_root_utf8. A locked
snapshot redirects only UserData rules in both host path translators. Existing
open file handles, game assets, TitleData/cache paths and partition images retain
their backing. Both entry points register this adapter with shared storage policy.
It accepts an existing absolute directory, rejects invalid paths, and accepts an
empty string to clear the override. save_paths/native_paths cover UDATA aliases,
unchanged other routes, rejection and override removal.

## Replacement texture frame completion

`patches/0360-texture-upload-streaming.patch` calls `tsfp_texture_frame_complete` from common
`present_resolve`, guarded by TSFP_TEXTURE_PACK. This covers the title
`dev_Swap` path, the older `dev_Present` path and the public frame helper. Legacy GL calls
the same shared queue pump from its own Present adapter. This completes
background CPU reads on the renderer thread with one replacement upload per
frame. It does not change guest texture dimensions/data, update clocks or frame
rate selection. Source lives in `texture_pack_stream.h` and the existing loader
adapters; no generated guest edits or new renderer thread owns GPU resources.

Replacement completion/cancellation increments a binding revision. The common
renderer checks that revision alongside mode before draws, so textures left
bound across frames see arrivals and queue-full retries without rebinding every
draw. The streaming patch must follow 0350-texture-toggle.patch, which creates this
binding helper. The texture runtime fixture compiles that actual patched helper.

Keep this patch LF on disk before refreshing its index. A CRLF working copy
produced a different hash/target spelling from the normalized Git blob; the
Linux CI context check exposed it. Runtime behavior was unaffected.

Future Perfect reaches host_Swap from its HLE shadow frame boundary. A pump in
dev_Present alone does not run for that path. Keep the completion call in the
common resolve function, after the skipped-Swap guard; tests execute both actual
entry bodies and verify one completion per presented frame, none for a skip.

Standalone translator consumers gate live-register header/declarations on the supported protocol; see [register contracts](gpr-refs.md).

Original x87 emission now joins SIMD in the additive translator stack; see
[independent emitter contracts](independent.md#original-x87-emission). Storage
and FP environment remain upstream; game acceptance is tracked separately.

The additive `independent-x87-fixture.patch` exports the inherited classification
C fixture and includes production SET_LO16; it does not alter generated code.

The control-transfer emitter now follows x87 in the additive stack; call ABI,
local dispatch and table-boundary contracts are in [independent.md](independent.md).
