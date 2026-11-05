# Systems guide continuation

[Main guide](systems.md).

## Background replacement texture reads

`texture_pack_stream.h` owns a shared single-reader queue for common RHI and
legacy GL. The worker reads/validates DDS data without touching texture owners or
GPU resources. Submit/cancel/pump run on the renderer thread; release and content
changes cancel pending owners before reuse. A full 64-slot queue retries when the
texture is next bound. Two completed payloads and one read bound CPU staging to
72 MiB; the existing 512-MiB GPU budget and DDS/alpha rules still apply.

Original views remain usable during loading. Each Present pumps at most one
nonempty payload into GPU resources; F10 switches cached views immediately. The
GL adapter restores the previous texture binding after upload. Shutdown joins
the reader; normal cancellation never waits for disk. `TSFP_TEXTURE_STREAMING=0`
selects the synchronous diagnostic fallback. Per-texture messages now require
`TSFP_TEXTURE_PACK_TRACE=1`; explicit original dumping remains synchronous.

Classic now defers new replacement requests in both adapters (`pack_checked=3`).
Binding in Enhanced starts the deferred request; until completion the original
remains visible. Returning to Classic retains cached replacements and permits
already submitted requests to finish. Hash/content invalidation, cancellation
and explicit original dumps still run. Deferred bindings in Classic perform no
replacement I/O, GPU allocation or repeated trace writes. A `deferred` trace
records the initial hash only; late activation has zero hash/source-byte fields.

The common renderer pumps from present_resolve, shared by Swap and Present.
The initial Present-only prototype never delivered replacements in this title;
its apparent pacing gains are invalid evidence. Actual DDS upload records and
rendered Enhanced scenes are required alongside queue unit tests.

The in-game Video Options Apply path calls the explicit tsfp_texture_set_4k
setter. Unlike the host toggle, that setter does not emit a mode log line.
Reopening Video Options reads the effective mode; a pending row label alone is
not evidence that the change was applied. Texture selection is session-only.

Integration #308/#311: upload timing follows the queued request by value. Only
the renderer opens/writes/closes the bounded CSV; the worker stamps read times
in its exclusively owned record and publishes it under the queue lock. Completed
records measure hash, worker read/validation and renderer image/view creation.
Total duration includes queue/frame scheduling waits, not GPU completion.
Cancellation discards the record with the payload; a full queue logs its failed
attempt and starts a new record on retry. Disabled timing takes no clocks/files.

`pc_surface_native.h` schedules the persistent cosmetic field from exact native
clock metadata. It begins from post-event/smoothing byte input on authored updates,
then evolves the remainder and intervening fractional frames. This places the
cosmetic field one authored tick behind the original completed feedback, retaining
original event/RNG ordering. It does not add input latency to gameplay. Duplicate
frames do not advance; missing intervals, changed scopes and new identities wait
for an authored update. Pause preserves phase. Original60 suspends the clocks and invalidates field
readiness while retaining bounded GPU owners; constructor/device reset releases
them. Returning to native waits for an authored boundary before reusing an owner. `pc_surface_native_hle.inc` resolves copied full texture identities,
uses the five-entry retained-image cache, packs alpha-height through
`pc_surface_gpu_pack`, and borrows that view only during the original final shader.
No per-frame readback or queue flush belongs to this path. Older fractional/float/
live experiments are mutually exclusive. Native Mac common-RHI execution remains
unverified and legacy GL uses original output. Bounded Windows Scotland captures at 120/144 produce fresh final images on
partial frames; sustained pacing, broader visual acceptance and Mac remain open.

`pc_surface_gpu_cache_suspend` invalidates every output without freeing images,
shaders or retained identities. The deferred `hle_d3d8_surface_native_suspend`
clears the borrowed view and clocks, avoiding shader/resource recreation across
an Original60 round trip. Original60 still performs no coupled updates. Full
scene/device cleanup remains distinct. The creation log identifies actual
new owners; unchanged identity reuse emits no allocation notice.

Native water defaults on for common-RHI 120/144 beta presets; Original60 and
legacy GL remain original. `pc_surface_native_policy` accepts an explicit
`TSFP_SURFACE_NATIVE_TEST=0` recovery override and excludes old experiments.
The HLE adapter latches allocation/shader failure until constructor/device reset,
so fallback does not retry expensive setup each frame. Cleanup is registered
before first allocation; empty resets do not alter RHI texture bindings.

## Character status-effect model growth

`pc_native_character_guest.h` normalizes only scale-constructor call120BBF:
subdivide the one-raw-tick factor at current game speed. Original rotation,
composition, cleanup and whole/catch-up arithmetic remain intact. The original
helper fixture covers60..240, slow time, pause, jitter and live switches.

`TSFP_CHARACTER_TRACE` selects126029->120AB0: matrices/deltas/identity/removal,
validated mappings, cross-frame/scene rejection,8192rows/8MiB cap. Mapping caches
never span guest execution. Disabled/failed/capped traces keep original callee.
CPU evidence only; rendering/pacing and physical Mac remain unverified.

## Temporal Uplink map display

The shared HLE hook copies the preclear map into the handheld texture. Native
GL uses src/native/gl_screen_copy.inc and preserves framebuffer/scissor state.
Guest geometry and controls remain unchanged.

Temporary UV matrices: [render epoch and arena lifetime](render-matrix-cache.md).

## Forced and damped material scrolling

`pc_native_material.h` splits127500 velocity/travel in raw game time:
12765Bforce,1276FD/127705drag,12773B/127791UV. Weak drag uses stable limits;
Original/whole/catch-up/unsupported bases keep old math. Force/clamp, wrapping,
resources/events, layout and pause stay original. Varying-force/rendered/Mac
parity remain open; see surface/cache tests and STATUS.

## Scripted object clocks

TREE1D88B0 admits fractional time at its gate; EAX is overwritten before use.
Original phase/pose, events, terminal clamp, layout and whole updates remain.
TRAP1D8AB0 now couples fractional kicks/travel and carried-tick contact lifetime.
Original whole/catch-up and pause remain exact. See [script clocks](native-script-clocks.md)
for the map, hook composition, rejected candidates and live/Mac acceptance limits.

Statistics: D2CB7 scopes D1FD0 to carried ticks. Time totals keep Original
cadence; distance stays per-frame. D2079 blocks paused stale ticks.
Impact sparks: see [native spark timing](native-impact-sparks.md).

[Devices](kernel-devices.md).

Project-owned I/O contracts: [sharing](kernel-sharing.md),
[request lifetime](request-lifetime.md), [driver calls](driver-call.md),
[transfer/queues/events/targets](transfer-completion.md),
[symbolic links](symbolic-links.md), [path routing](guest-paths.md) and
[host storage](host-storage.md).

Vehicle gunner mouse uses shared `src/input/pc_mounted.h` for mode `0x400`,
resolving the occupied seat and its gun on every update. Driver/passenger seats
retain their existing route. Vehicle limits and locked states differ from mode
`0x80`; see [mounted and vehicle mouse contracts](mounted-mouse.md). Both Windows
and SDL adapters consume this helper. Exact mission validation remains separate.

[Tank keyboard/mouse scope and original aim route](mounted-mouse.md).

Kernel: [pool](kernel-pool.md), [time](kernel-time.md), [RTL](kernel-rtl.md), [numbers](guest-numbers.md), [crashes](crash-evidence.md).

Online uses the build interpreter: [paths](multiplayer-ui.md).

Assault objective brackets (US142D10) use a tagged proportional placement.
`PcUiWorldAnchor` takes the complete first outline draw's horizontal midpoint;
all following labels retain that anchor until the next placement boundary.
The common renderer and legacy GL fit uniformly around this projected target,
not screen centre. This preserves both alignment and glyph/box proportions.
Guest flush boundaries isolate markers and restore the parent HUD. The existing
wide/single-player guards preserve 4:3 and split-screen. `test_ui_layout.py`
checks actual viewport scaling, target position, reset and seven display shapes.

Bot corpses: [contracts](systems-details.md#online-bot-corpses).

[Story version](systems-details.md#story-co-op-release-flag).

Physics: [poses](systems-details.md#guest-ragdolls-and-physical-npc-drops).

[Language startup](language-packs.md).

[2x texture workflow](original-upscale.md).

[RTL](guest-stack-trace.md), [host policy](execution-host.md).

Local player assignment: controls-options.md.
