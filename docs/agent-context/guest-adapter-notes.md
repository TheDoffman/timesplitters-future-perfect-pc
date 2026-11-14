# Guest adapter notes

## Runtime routing

[Sticks](controller-deadzones.md).
[2990](control-presets.md).
[Tools](developer-tools-integration.md).

[FOV node filter](video-options.md); [Uplink](native-uplink.md).
[UI](controls-options.md); [Kill feed](../development/arcade-kill-feed.md).

[Effect depth](native-effect-depth.md).
[Linux](linux-game.md); [MSAA](smaa.md).
[MP hooks](multiplayer-exit.md)
2465 wraps356800 for rewards;
profile mapper356870 stays original. See [contract](systems-details.md#four-player-rewards-context).

Story host loss:2460 wraps1AF950. The menu adapter scopes
the native guest-lobby8D notification to main return, preserving transport reset
and rebuilding main in its own layout context. See [contracts](systems-details.md).

Rejoin uses the D4BE0/A9070 camera/world wrappers. Rejoin restores a missed camera-end transition once;
world identity excludes native first-person viewport models before cached-key
lookup. No wrapper or generated-code change is needed. See
[contracts](systems-details.md).

Story in-progress rejoin uses existing2D398D pending-copy,9A420 synthetic-pad
and class1e adapters; no new wrapper or generated-code edit is required. The
original spawn resolves the temporary host-relative pad through D4F50/2B05A0.
See [rejoin contract](systems-details.md).

Story peer departure: additive2450 wraps BABA0. Its adapter replaces only the
first-mission remote departure caller35C199 with native context inactivity,
leaving the original peer cleanup and actual combat deaths intact. See
[departure contract](systems-details.md).

Story abort routing: additive2440 wraps3592F0 (reliable lobby-return receiver)
and65420 (full-disconnect scheduler). Original unloading and reliable classB
transport remain; Story adapters select lobby/main and avoid campaign recording
on abort. Online confirmation2EBE60 uses existing menu lookup. See
[contracts](systems-details.md).

Story browser level details: additive2435 wraps1CBDA0/2BECE0. Only validated
Story advertisements in the System Link list scope a Story metadata lookup;
its1CBFC4 picture draw uses the thumbnail and adds the localized level name.
Arcade, joined lobbies and native fallback retain original callbacks. See
[contracts](systems-details.md).

Empty guest rewards: additive2430 wraps menu factory79860. Only joined first-
mission guest rewards106 with an actual zero unlock count redirects descriptor
5236DC (loading) to523900 (reward pages/waiting), preserving save and transport.
See [contracts](systems-details.md).

Guest profile saves: additive2425 wraps original1922D0 through a scoped
persistent-profile view for joined first-mission reward saves; network slots
are restored afterward. Original validation, checksums, signed writes and
error/retry UI remain. See [contracts](systems-details.md).

Guest reward pages: additive2420 wraps2CBAF0 so original automatic timer tail
calls reach the same final-page waiting policy as confirmation. Guest completion
recording and own-profile selection use existing shared Story source; native fallback
keeps the original page callback. See [contracts](systems-details.md).

Network score lifetime: additive2415 wraps stock3578A0 after the shared
network adapter include. Movie/frontend scenes hold late gameplay score
snapshots before their actor/ranking writes. See [contracts](systems-details.md).

Story movie lifetime: additive network2410 wraps original60310 through the
shared Story adapter so a guest's blocked movie advance does not release its
still-rendered stream at99BB7. Real load teardown and other modes retain the
original close. See [contracts](systems-details.md) and [fixture](build-test-details.md).

# [Runtime](pause-glass.md)

Patch2180 wraps US1C2AF0 for an already-eligible fresh player trigger only;
the patch loader orders it after native minigun/timing wrappers. It preserves
held-fire and continuation calls. See [input contract](interaction-input.md#fresh-weapon-clicks-434).

Story co-op integration: [contracts](systems-details.md).

Audio recovery patches0055-0058: [buffering, lifecycle, format and device contracts](audio-buffers.md).
Audio release/exit/completion patches2300-2306: [contracts and diagnostics](audio-buffers.md).


Patch 2140-native-minigun-intent follows the native weapon wrappers in loader dependency order. It scopes held-primary intent around original US 1C74D0 event 1. Generic shots remain on whole ticks; numeric site 1C7600 normalizes release damping. See native-minigun.md.

`2130-game-dpi-awareness.patch` attaches the Windows game manifest supplied by
`tsfp.ps1` to the title executable. Its system DPI declaration prevents Windows
bitmap scaling and virtualized windowed Auto dimensions without a path-specific
compatibility override. It preserves linker-generated UAC/common-controls and
does not change native Mac targets; see the build/test guide for probe coverage.

Unchanged patch stacks reuse content validation: [cache contract](release-speed.md).

`0040-dsound-positional-stereo.patch` keeps XDK distance attenuation and adds
shared stereo projection with XAudio2/SDL output adapters; see
[positional audio](../development/positional-audio.md) for field provenance,
cursors, Audio Options toggle and host limits.
`0050-environmental-acoustics.patch` adds opt-in world-coordinate room/portal routing; see the audio guide for limits.

`0045`: [range audit](../development/positional-audio.md#distance-falloff-audit).
`0059/60/61/2307/08/09/10`: [audio feature/bus/mix/DSP/gain/activity contracts](../development/audio-overhaul.md).

Kernel: [objects, threads and boot](kernel-objects.md), [devices and names](kernel-devices.md).

Unused POSIX memory APIs retired: [caller audit and strict fixture controls](retired-memory-compatibility.md).

Main guest mappings: [providers, aliases and lifetime](guest-host-mappings.md).
Native Nt host mappings: [reservation ownership and protection](native-virtual-memory.md).

Guest Mm/statistics: [ownership, host snapshots and ABI](guest-memory-services.md).

Native Mm ownership and host page operations: [contracts](native-memory.md).

Virtual-memory adapters: [ownership and ABI contracts](virtual-memory.md).

Thread/TIB and stack providers: [construction, lifecycle and callers](thread-memory.md).

Heap/reservation/physical allocation now routes through the project ownership
provider; see [allocation contracts](independent.md#guest-allocation-ownership).
Host mappings and thread/TIB storage now have project providers; device services
and whole-runtime worker quiescence remain transitional.

Image inspection/loading now routes through the project provider before host
mapping and during materialization; see [image contracts](independent.md#native-image-loading).
Mapping/TIB construction use project providers; device services remain transitional.

Guest architectural storage and bounded ABI reports now use project providers;
see [state contracts](independent.md#guest-state-and-abi-reports). The loader
uses project memory/TIB allocation; fibers still share their native thread's cells.

Native continuation storage/restoration is project-owned through additive
`0203-independent-nonlocal-jumps.patch`; see [contracts and host limits](independent.md#native-nonlocal-jumps).

Optional branch-feedback recording is project-owned; compatibility includes
route to `src/runtime/` through `0202-independent-branch-feedback.patch`.
See [provider, generation and test contracts](independent.md#guest-branch-feedback-recorder).

The canonical guest header now lives in `src/runtime/recomp_types.h`; generation
reads it directly. The final `0199-independent-runtime-header.patch` keeps the
transitional template identical, before layout routing. See
[header ownership and service boundaries](independent.md#canonical-guest-header).

The subsequent `0201-runtime-header-consumer-fixture.patch` repairs the inherited
guarded-call test's function-backed macro extraction; it changes no game source.

Scalar support follows vector retirement: MOVSX/MOVZX emit their own masks and
sign handling; old numeric helpers are removed, while regression oracles execute
real emitted instructions. See [scalar contracts](independent.md#scalar-numeric-support).

Vector helper retirement follows call frames in the loader, before layout
routing. `XMM_SCALAR` remains the model-transform ABI with an original
bit-preserving body; emitted SIMD operations need no legacy helper layer.
See [vector support](independent.md#vector-runtime-support) for contracts/tests.

Guest call frames, dispatch, register/memory primitives and translation tooling are in [own interfaces](independent.md).

The final `platform/2090-repository-source-layout.patch` routes the runtime CMake
source/test paths and Python compiler bridge into the responsibility folders.
It runs after all historical patches; prior ledgers still unwind before applying it.

Active patches now live in responsibility folders under `patches/`; consult
[the folder map](../../patches/README.md). Recursive discovery excludes legacy
and sorts stable basenames globally before dependency overrides. Folder moves
preserve exact ledger bytes and runtime output. Duplicate active names fail.

Patch filenames use four-digit base-sort slots and descriptive names; see
[patch maintenance](../../patches/README.md). This replaces the repeated `z`
prefixes without changing patch bytes, effective order or context options.
Maintenance commands use `tools/build/` and `tests/repository/`.
The loader retains its dependency overrides, and recorded old names unwind
using their stored contents. Keep the five legacy migration patches.


Product lowering and its fatal divide boundary are documented in [own tooling](independent.md).


Independent tooling: [ownership and validation](independent.md), including compiled inherited SAR sweeps.

The exact pristine runtime base now lives in `vendor/ts3FPrecomp/source`.
`tools/build/prepare-runtime.py` verifies blob SHA-256/Git identities, gitlinks, tree
and original commit before creating the ignored runtime checkout. It retains
`d90c929...` as shallow HEAD so existing patch ledgers, pristine `git archive`
fixtures and packaging provenance remain valid. Do not edit snapshot files;
runtime changes remain in the shared patch stack. The origin URL is attribution,
not a setup dependency. `tests/repository/test_runtime_snapshot.py` exercises the importer
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


## Host systems

[Sticks](controller-deadzones.md), [Rollback](control-presets.md), [updates](main-menu-update.md).
[Tex](texture-preference.md); [mods](mod-workshop.md).

[Uplink throw tests and policy](native-uplink.md#throw-impulse).
Networking: `pc_network_enabled()` reads startup config once,
then uses shared atomic flags in the indirect-dispatch hot path. The System Link
menu explicitly calls `pc_network_enable_lan()`; Online connect/disconnect publishes
its own flag. Missing launch files are not retried on every guest call, and an
explicit disconnect does not re-import stale startup credentials. Windows, Mac
and Linux share this policy. See `tests/network/network_eligibility.c`.

[Developer Tools integration](developer-tools-integration.md).

[Uplink](native-uplink.md);[projectile motion/beam damage](native-projectiles.md).
[Input](controls-options.md).

[Icons](controls-options.md).
[Display](video-options.md).
[Tabs](../development/graphics-options.md).
[exFAT](host-storage.md)
# [Bindings](pause-settings.md)

[SDL](linux-game.md).

Native input self-tests use click/Escape capture after F1 became the optional
Developer Tools shortcut. This changes only synthetic test events in
src/native/input_sdl.c; Linux passes, native Mac execution remains pending.

PC bar: [state](interaction-input.md#pc-settings-bar-visibility).

[Clicks](interaction-input.md#fresh-weapon-clicks-434).

[Story](systems-details.md). [Exit/save](multiplayer-exit.md).

[Mech](mounted-mouse.md).

Minigun: [policy](native-minigun.md).

[Kernel](kernel-objects.md).

Memory: [retired APIs](retired-memory-compatibility.md).

Guest storage: [allocation](independent.md#guest-allocation-ownership), [threads](thread-memory.md), [virtual memory](virtual-memory.md).

Image/state/ABI: [image](independent.md#native-image-loading), [execution](independent.md#guest-state-and-abi-reports).

Wind: [time scopes](native-wind.md).

Aim/animation: [diagnostic limits](native-local-trace.md).

GPU cache fixture/config: [runtime](runtime.md).

Native player contact: [clear-slide policy/trace](native-player-contact.md).
Registers: [policy/tests](gpr-refs.md).
Script/audio clocks: [contracts](native-script-clocks.md).
