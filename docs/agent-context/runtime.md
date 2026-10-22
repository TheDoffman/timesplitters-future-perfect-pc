# Runtime and patch navigation

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

`tools/apply-patches.py:patch_records` defines effective order (not just filename
sort). The loader stages an unwind of the EXACT previous recorded patch bytes,
then applies the entire current top-level stack before updating runtime files.
Find its ledger with `git -C external/ts3FPrecomp rev-parse --absolute-git-dir`:
`tsfp-patches.json` lives THERE, not in root `.git` or the build folder.
A conflict "during unwind" means current runtime/recorded stack diverge; it
does not establish a bad new patch. Preserve edits and investigate the named
hunk; never delete the ledger, reset the runtime or remove dependency patches.
<!-- packet:end -->

## Source ownership and execution

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
Build `67c1864` passes and matched live initial acceleration now agrees within
0.1% at the three presets. Later pillar/bank contact diverges, so full handling
acceptance remains separate. Latest candidate recovery is `vehicle-67c1864`;
the earlier candidate binary was recycled and the playable crowd pair restored.

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

`zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz-auto-display.patch` changes the
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

## Native FPS integration with current main (PR #285)

The effective tail now installs native timing and retires interpolation before
fixed-60 defaults, then applies main's late-frame gate correction before the
remaining native update/surface patches. Original 60 remains the default;
`pc_native_timing_guest.inc` commits requested 120/144 changes at update boundaries
and HLE skips the original flip gate while native timing is active. Keep both
`test_frame_clock.py` (real Arm body) and `test_native_timing.py`; neither alone
validates the combined stack. Water precision experiments remain off by default.
