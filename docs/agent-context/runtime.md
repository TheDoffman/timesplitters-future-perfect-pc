# Runtime and patch navigation

`*-story-human-teams.patch` wraps D7550. Only the33ACDB network-team call for a
validated human actor in live Story replaces the requested Arcade team with1,
matching original Story25A420. Arguments are restored after the original setter;
other callers, NPCs and native/non-Story fallback retain original behavior.

`*-story-handoff-authority.patch` wraps original218910/21B540. Connected first-
level Story guests skip their inventory side effects when rendering replicated
NPC poses; the host runs the unchanged callbacks and publishes their awards.
Other modes/native fallback retain the original. The existing ammo/grant adapters
observe the pistol25 and rifle32 authored pairs; no generated body is edited.

The supported353990 broadcast path copies packets into a256-byte stack buffer.
Do not send the372-byte internal SNP2 state directly: bounded physical/pose
messages and checksum-based assembly preserve that original sender limit.

Story poses reuse the existing frozen-animation and bounds wrappers through
`pc_network_bots_guest.inc`; no additional guest wrapper is required. Those
shared boundaries accept only a current validated Story NPC replica as well as
ordinary bots. Primary/crossfade clip binding is shared in the display-menu
translation unit; original model handles and frame-arena bones remain local.
Native/disabled transport fallbacks retain original guest calls.

`*-story-extra-loadout.patch` wraps original1BA2B0 ammo grant and1BFCA0 inventory
addition. The scoped adapter observes only the supported first-level authored
handoff call sites and the host's primary actor; it preserves original arguments,
return and stack behavior. The source owner is `src/pc_story_loadout.inc`, included
through the shared Story load adapter. Regenerate the full stack for the wrappers.

`*-story-extra-starts.patch` adds wrappers for original9A420 pad lookup,
9A4C0 tag lookup and9A510 inverse index. Only two reserved first-scene Online
Story indices resolve to private copies of the original primary start; all
ordinary/native paths retain the original bodies. The wire remains the original
signed16 spawn-pad field. Regenerate the complete stack after wrapper changes.

`*-story-npc-creation.patch` adds original DAC30/35BE90/3580D0 boundaries and
composes the existing15F8C0 diagnostic with the Story spawn adapter. Ordinary
multiplayer/native fallback stays original. The original NPC pre-spawn sender
is replaced only for connected Story host NPCs; the completed original host spawn
then emits a versioned creation message through353A90. Never hand-edit generated
bodies; the full stack must regenerate these new wrappers.

`*-story-npc-lifecycle-trace.patch` wraps D73B0 model-cache construction,
15C560 character-list bootstrap, 15F8C0 script spawn and35CF80 network spawn gate.
The shared opt-in observer emits at most eight entry/exit pairs per boundary in
Story gameplay. Each original runs once with its original arguments and return.
Native/disabled paths pass through; it does not create or replicate NPCs itself.

`*-story-authored-records.patch` wraps original6B100's scene-record eligibility
filter through `pc_story_network_asset`. For connected Story gameplay only, it
removes bit31's ordinary-network exclusion and accepts authored two-player co-op
records for three/four humans. Single-player-only records and the original
difficulty/mission filters remain unchanged. Argument bytes are restored after
the original call; native fallback and other modes call the original directly.
This boundary is used by object/script creation and authored pad selection.

`*-story-context-spawn-trace.patch` wraps original D50E0 without altering its
ownership checks. D50E0 and the existing D2FD0/D3C50/D2CE0 wrappers use the separate
script trace budget only while phase15 retains script-suspension flag02000000.
This observes context replacement after suspension; ordinary spawn calls remain
direct pass-through. Complete-stack migration verifies the additional wrapper.

`*-story-script-trace.patch` wraps original D4B00/D4BE0 suspend/restore calls.
The shared callback records caller, argument, peer role/phase, result flags and
context states before/after, with a separate 32-call opt-in trace budget. Both
original bodies execute unchanged, including native/disabled fallback. It needs
supported regeneration and complete-stack migration validation.

`*-story-spawn-pad.patch` separately wraps original CF060 spawn-pad selection.
It calls `pc_story_network_spawn_pad`, which retains successful original results
and uses the original first-player lookup only for a missing Story start for
online slots 2/3. The temporary lookup index is restored before body creation;
ownership and life-state logic are unchanged. Native fallback calls the original.
The historical Mac migration fixture excludes this dependent Story patch, then
asserts that the complete migration installs its wrapper.

The opt-in Story load diagnostic adds original-body wrappers for67450,66FC0
and681C0, plus context initialization/state routines D2FD0/D3C50/D2CE0, in the
separate `*-story-failure-trace.patch`. They call shared
`pc_story_failure_trace` to record the guest caller and state/result changes,
bounded16 observations and gated by the Story load/trace opt-ins. They never
modify the original failure predicate or suppress its state transition. Wrapper
changes require supported regeneration; do not edit generated functions.
The Mac-only upgrade fixture excludes this network-dependent diagnostic from its
historical stack, then asserts the full migration installs the failure wrappers.
The681C0 wrapper now enters `pc_story_network_frame`, which supplies the scoped
opt-in pending-load spawn adapter and then the unchanged traced evaluator.

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

Issue #222 changes the shared include reached by the existing bot frame wrapper;
it adds no guest wrapper, wire field, generated-code edit or patch dependency.
`src/pc_network_bot_motion.h` owns replica presentation between host snapshots.
The existing title integration includes it through `pc_network_bots_guest.inc`.
Apply the complete patch stack and use the supported builder as usual.

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

Online Story lower-details hook wraps original1986A0 through a separate patch.
The combined198E20 roster renderer remains original. Only a scoped typed Story
body draw at return199650 substitutes selected briefing text; other callers and
connection modes run original details. New manual wrapper requires regeneration.

## Native FPS integration with current main (PR #285)

The effective tail now installs native timing and retires interpolation before
fixed-60 defaults, then applies main's late-frame gate correction before the
remaining native update/surface patches. Original 60 remains the default;
`pc_native_timing_guest.inc` commits requested 120/144 changes at update boundaries
and HLE skips the original flip gate while native timing is active. Keep both
`test_frame_clock.py` (real Arm body) and `test_native_timing.py`; neither alone
validates the combined stack. Water precision experiments remain off by default.

## Outlined text read scope

`*-text-outline-scope.patch` wraps US `sub_00041E30`, whose synchronous body
draws four outline offsets and then the foreground through `sub_00040FA0`.
The existing prompt read scope now spans those five passes and ends before
returning to the caller. The wrapper preserves the original draw routine and
guest ABI, enables the same path for `TSFP_PC_CONTROLS`/`TSFP_NATIVE_PROMPTS`,
and performs no AI, animation, rendering-quality or frame-rate throttling.
The mapping cache remains read-only; prompt scratch writes query live protection.
Adding the manual wrapper requires the normal guest regeneration/build route.

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

A separate `*-story-character-roster.patch` wraps original15BCC0. Only the
Arcade network reconciliation caller355EA4 is suppressed during connected
Story pending loads and active rule10 scenes, including pre-play phase14. Other removal callers and native fallback remain original;
new wrapper needs supported regeneration and complete-stack migration.

The separate `*-story-intro-transition.patch` wraps original65C70. A connected
Story guest cannot independently run the intro next-load builder. Host movie-end
pending-copy caller65CDC normalizes the Local config to the occupied network
header, increments the original start generation and uses356490 to serialize
it reliably before658D0 copies it. Source contracts cover generation wrap and
guest authority; runtime cinematic/handshake/visual acceptance remains pending.

`*-story-cinematic-ready.patch` wraps357540 at the original loader boundary
3D67D. During connected Story movies only, it runs the stock ready exchange
with a scoped zero actor-loop count and cinematic eligibility flag cleared,
then restores count/flags. The original peer state and ready packet execute;
no player bodies are spawned into the intro. Ordinary/native calls are original.

Story human teams are normalized in the existing681C0 frame adapter once a
real entity/context/pool backlink is available, before NPC replica updates.
OriginalD7570 can initialize remote teams before the scoped setter is eligible;
wrapping initialization alone did not correct the four-peer snapshots. The
frame path calls the original setter only when a validated human differs from
team1, preserving its derived color and original local-body ownership. NPCs and
other modes remain untouched. D7550 pump normalization covers loading too.

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


Story/main merge (2026-10-23): retain all Story wrappers and main's keyboard/save
storage patch stack. Story chooser callback IDs move to verified US INT3 padding
0x1237b..0x1237e to avoid keyboard dispatcher collisions. Regenerate through
the supported builder; no generated guest source is edited by hand.
