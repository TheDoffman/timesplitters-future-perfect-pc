# Subsystem contracts and diagnostics

Original Story25A420 puts human actors on team1, but the ordinary33ACB0 network
pump overwrites them from per-controller Arcade team data. The shared Story
team adapter normalizes that D7550 call for validated human pool slots.
D7570 also writes slot+1 directly while initializing remote bodies. Initial
construction precedes the eligible setter scope, so initialization wrappers
alone did not correct remote teams. The existing Story frame adapter validates
real body/context/pool backlinks and calls the original setter for a human
whose team differs from1, before NPC updates. This keeps derived team color
consistent and never changes controller ownership or NPC assignments.
NPC factions, lobby colors, other team callers and other modes remain original.
Original-US and source contracts bind the intended caller and shared-team value;
The latest Windows four-peer probe confirms all four human teams are1 on every peer.

NPC pose replication uses the shared numeric `PcCharacterPose` model in
`src/pc_character_pose.h`. Bot snapshots retain their existing binary layout;
the shared collector/validator and `pc_character_pose_present` handle primary
clips, crossfades, overlays and blend channels for both bot and Story replicas.
The internal Story SNP2 snapshot is372 bytes, including optional pose and hit
bounds. Original353990 has a256-byte outgoing stack buffer, so it is transported
as164-byte physical and240-byte pose messages, with match/slot/sequence and a
shared checksum. Guests assemble complete matching parts before applying pose
state; absence needs only the physical message. Missing/reordered or mixed
parts cannot cause partial state application.
Guests retain local model/asset/bone handles, strip already-applied root-motion
flags, evaluate valid replicas without advancing a second animation simulation,
and restore authoritative bounds after local evaluation. The replica guard
checks match, slot, entity backlink and spawn generation for all32 actor slots.
Only the host runs Story NPC scripts. Expanded sender scratch spans are checked
and the wire magic/length distinguishes the new snapshot from SNP1. Source
checks pass; changed four-peer pose/combat runtime acceptance is pending.

Opt-in Story hit tracing composes with the existingBB6C0 damage wrapper. It
records only valid human-source/NPC-victim pairs, original replay state, network
ID, damage and health before/after, up to128 observations. The original damage
body executes once with its original result/stack behavior; tracing changes no
health, ownership or hit routing. Disabled/non-Story paths remain untouched.

The first-level handoffs award pistol25/eight magazines and rifle32/six
magazines through original1BA2B0/1BFCA0.
`pc_story_loadout.inc` observes that actual host event and sends a versioned
pointer-free loadout command to remote humans, through the existing reliable
Story channel. Each guest waits for a ready, locally owned context and invokes
the original ammo/inventory/select APIs on its own actor. Host identity,
match generation, recipient mask and bounded authored values are validated.
Replicated NPC animation must not execute the guest218910/21B540 inventory
callbacks again. Those callbacks are host-only in this scope. Separate pending
commands and per-weapon replay tracking preserve both awards, including for the
original co-op guest and two-player sessions, without refilling an owned weapon.
Periodic resend recovers readiness races; per-slot generation/serial tracking
prevents refill on replay. Existing inventory and weapon selection are retained
when the grant already exists. Local/Arcade/native fallbacks remain original.
The changed Windows four-peer probe passes: each player receives weapon14 with
16 loaded/112 reserve rounds, moves independently, and consumes three rounds
when firing. Peer actor positions remain consistent; combat hits, NPC/world
interaction and restart/next-level behavior require separate acceptance.

First-level starts: stockCF060 can select later scene pads during remote setup,
including for original co-op slot1. Before the first checkpoint, Online Story
uses authored first-player/second-player pads1f40/1f62 and extra signed16
indices7f02/7f03. The pad/tag/inverse-index
adapters resolve private32-byte copies of the authored1f40 primary start, offset
by minus/plus2 world units onX. Original coordinates, room and heading remain
otherwise intact, and the original player placement/collision and spawn packet
run normally. The original read-only25EE60(0) checkpoint predicate ends the opening override;
a first active body alone does not establish that network setup has finished.
Checkpoint selection stays original. Cached initial packets resolve locally; Local Story and other scenes are unchanged. The changed Windows four-peer probe confirmed matching starts four units apart,
health35 and subsequent three/four-unit bounded movement by both extra humans.
The later loadout adapter supplies the authored grant to the added humans, as
verified by the four-peer opening/movement/ammo probe described above.

Story NPC creation is separate from controller membership. The stock spawn RPC
only names a slot and assumes its body already exists; the first restored Story
spawn run reached NPC scripts but crashed guests on a null slot. Shared
`pc_story_npc.inc` records scalar constructor identity and replaces that NPC-only
pre-spawn notification with a host-authored post-spawn creation/state message
on the original reliable channel. Guests construct through original DAC30,
retain local pointers, apply identity/team/initial physical state and roster
membership, and leave Story script simulation to the host. Messages carry no
guest pointers; host identity, generation, sequence, slot, loaded character kind
and finite scalar values are checked. Early arrivals queue until scene readiness.
The first changed four-peer run constructed NPCs on all guests without the old
null-body crash, but the host first spawn preceded guest readiness and later
script roster changes were missing. The frame adapter now republishes recorded
NPC state every six simulation ticks through the reliable channel, including
absence and current roster membership. Same-tick calls do not retransmit; only
the host publishes. This supplies creation recovery and ongoing physical state;
The changed Windows four-peer probe retained the same NPC identities on every
peer, recovered the early body and reflected its later removal; sampled NPC
positions differed by at most0.012 game units. Full NPC animation/combat/world acceptance remains unverified. Human starts
and four-player firearm handoff now pass their separate changed runtime probe.

Story NPC activity: Local spawn stacks establish D8E30 ->15EAE0 ->27F8E0 ->
15F8C0 at the opening. D8E30 first calls3568C0 fromD8E5D; its ordinary network
path rejects every actor without a controller record. `pc_story_network_actor_active`
accepts only that caller's valid non-player Story actor, with matching pool index,
entity backlink and no player context. It leaves controller enumeration, humans,
other modes and the later original358560 host-ownership gate unchanged. The changed runtime reaches host NPC scripts and spawns; its four-peer
physical-state acceptance is described above.
The caller supplies its actual entity from original D8E30's EBX register. This
also accepts a dormant script actor whose pooled active-body pointer is0, while
rejecting a prototype that refers to a different active body. An inactive script
must be allowed to run before it can create its first body.

NPC lifecycle tracing can also observe the original offline Story path when
both `TSFP_STORY_TRACE` and `TSFP_STORY_TRACE_LOCAL` are explicitly set. The Local
path additionally requires network disabled, active Story mode4 and rule10.
Scripted spawn observations include bounded numeric guest return candidates
from128 stack bytes, to locate its original activation path. No Local behavior
changes; ordinary launches do not enable this diagnostic.

NPC initialization investigation: the paired scene setup matches the Local
reference (budget index8/count5, model record count26). `pc_story_network_npc_trace`
separately observes model-cache creation, character-list setup, scripted spawn
and its stock network request/replay gate, with independent eight-call budgets.
It retains original behavior and return/stack effects. This distinguishes missing
resources, allocation failure and rejected spawn events; no NPC fix is claimed.
The changed lifecycle run observes all five D73B0 cache calls returning1 and the
bootstrap returning, while the host never enters15F8C0/35CF80 through the opening
timeout. This points investigation upstream of scripted spawn requests. The
scene's placed-NPC count at setup+30 is0 Online; its Local value is not yet
measured, so that value alone does not establish a missing scene record.

The opt-in authored-record startup trace emits at most24 observations per
process for records with network/count masks, recording caller, active scene/
flags/count, normalization and original predicate result. It observes both
adapted and rejected guard paths, with no second predicate execution. The first
authored-record runtime probe still had no NPC/firearm handoff after60s; the
remaining initialization gate is unresolved.

Story authored records: original6B100 rejects bit31 in network matches, and its
00400000 co-op mask requires exactly two contexts. `pc_story_network_asset`
adapts these inputs for connected Story gameplay with up to four players, without
changing global player counts or difficulty checks. The argument is restored
after the original predicate; masks for solo-only records remain restrictive.
The preceding four-peer handoff fix kept all contexts alive/stage1 for60s, but
only four human characters existed and every player remained unarmed. Source
tests verify the new filter; NPC/opening acceptance still needs a changed build.

Story movie handoff: original class3B receiver352BC0 invokes GO2D38C0 on a
changed generation while still in phase15, before switching to10. The guest
pending-copy call2D398D is accepted only during an active Story intro, so its
selected mission/difficulty/stage and pending spawn token advance with the host.
The first natural four-peer intro reached gameplay on all peers, but guest stage
stayed0 and opening failed before this correction; combat remains unverified.
The ready adapter also skips original357540's Arcade staging-pad/body loop for
a pending Story gameplay start. That loop explicitly replaces each cached pad
with original6C1E0's team selection. Original Story scripts and the pending
context adapter retain responsibility for authored starts; later ready calls,
ordinary multiplayer and native fallback retain original behavior.

Initialization/spawn calls D2FD0/D3C50/D2CE0/D50E0 made during script suspension
now share the bounded script-state trace. This separately observes the original
context reset path after phase15, without changing ownership or state writes.
The source fixture checks that the original reset still executes and retains
the suspension flag and return/stack effects. Runtime cause remains unverified.
A separate bounded pad-source diagnostic records CF060 input cache1504,
random-selection switch6B988C, original pad result and caller before fallback.
It distinguishes network-provided pads from the authored Story selection; no
selection policy changes are implied by this diagnostic.

`pc_story_network_script_trace` observes original D4B00/D4BE0 directly because
their script suspension writes bypass D2CE0. It retains the original call and
side effects, records peer/caller/context/result before and after, and limits
opt-in output to 32 calls independently of startup tracing. No script state is
forced, replicated or suppressed by this diagnostic.
The changed four-peer run recorded D4B00 from caller1E4B9B on every peer at
different ticks, suspending each local context. No D4BE0 restore appeared through
a further 20 seconds. Host context 0 changed from 256 to 2 as remote contexts
initialized, while guest-owned contexts stayed 256. The reset writer remains
unresolved; no script-control or world-synchronization fix is claimed.

Story rule10's original CF060 selector returns -1 when the player index exceeds
the valid authored starts. `pc_story_network_spawn_pad` preserves valid results;
for connected Story slots 2/3 only, an explicit -1 retries the original selector
with a scoped first-player lookup index, then restores the real context slot.
This reuses an authored start without changing ownership or copying bodies.
Source tests cover slot/stack preservation, valid-pad passthrough and non-Story
and lobby guards. A changed four-instance run observed missing starts for slots
2/3 resolving to authored pad D9 and all players placed in the start area. It
still failed before movement because one peer retained script flag 02000000.
Original D4B00 sets that flag and writes context state 256 directly; D4BE0 clears
it and restores state 2. This bypasses the traced D2CE0 setter. Script authority
and coordinated suspension/restoration remain unresolved; no flag is forced.

Four-player lobby admission passed, but the first four-player load check failed:
slot 3 stayed stationary and its owning peer changed context state 2 to 256 at
tick 14. Firing was not reached. The opt-in Story trace separately observes up
to eight original D2CE0 requests for state 256, including caller/context/result,
because repeated preparation exhausted the generic sixteen-observation budget.
This trace calls the original setter unchanged; its cause remains unresolved.
A changed trace build then passed four-player load/movement with matching
positions and active contexts, without reproducing state 256. This does not fix
the intermittent failure. All four firing-input captures were unarmed views;
shots, hits and Story world authority remain unverified.

Online Story now publishes host mission/difficulty through the existing reliable
lobby configuration sender. Publication preserves capacity/ownership header
bytes; the start adapter normalizes the occupied roster only at GO. Story lobby
admission uses a four-human limit. Guest descriptor4E99D4/tick1A8F30 shares the
Story header and own-profile Progress action; guest Y cannot open Options.
Host Scotland/Hard, guest Progress/Back and removal of the hidden load opt-in
passed a bounded paired Windows check. Guest footer labels remain wrong despite
correct Progress routing. Guest readiness now uses original359EB0(0,1) only when local node774028+2FA
is unready; its A callback is removed. Footer B/X records are applied after the
original dynamic Ready caption update. The guest footer/Confirm and host GO load were reviewed live; the changed
pair then failed an inactive second engine context, so gameplay acceptance is
still unresolved;
briefing/year/icons, world authority and next-level coordination remain open.

`pc_story_load.inc` adds a typed Story start adapter for an Online Story connection. It validates counts and
mission records, substitutes scene/difficulty/Cortez and the observed Local
flags in the host's existing start config, then derives the guest selection from
that reliable config. `pc_network_bots_guest_config` invokes it before the
original serializer/pending copy; only the pending-copy function identity may
activate the per-instance Story mode afterward. The host derives occupied slot extent from the
original controller roster rather than byte0, which the stock builder left at
map capacity16 in the failed paired load. Holes, frontend network mode, phase,
role and local ownership are retained; only the host rewrites the capacity to
the occupied extent. Guests require the received count within four. Bounded
opt-in trace reports actual wrapper address/counts. Other connection modes retain their original behavior.
The first partial-config version reached both pending-load hooks but crashed in
`sub_0008DC70`; exact matching dumps are recorded in the investigation receipt.
The current direct-load variant calls the original Story stage builder
`sub_00065940(config)` once on the host at lobby phase11 with stage1 and scoped
frontend mode4/mission/difficulty. The earlier `sub_002D2C70` route reset stage0
internally and built an intro config before the adapter rewrote scene/flags. Before the factory it supplies the original Local Story
rule10/variant2 selection; mode4's factory reads these fields instead of resetting
them. Received experimental configs must match this Story rule pair. It restores original guest registers/frontend fields
and the network header before normalizing counts and selecting direct gameplay.
Pending phase10 and guests consume that same config without rebuilding/reseeding
it. Factory runs passed the previously observed loader fault, but both mode14
and one-view mode4 reached Game Over with health60. The mode choice did not
explain that failure. Original Local Story additionally supplies rule10/variant2
before factory construction; the current correction supplies that selection
before rule-dependent initialization. Runtime acceptance and world/script
synchronization remain separate requirements. No life/death/pause flags are forced;
the paired probe still rejects Game Over and requires live cross-observed movement.
Opt-in `pc_story_failure_trace` wraps the original failure entry points67450,
66FC0 and main evaluator681C0, plus context init/state D2FD0/D3C50/D2CE0, through
a separate reproducible patch. It records engine context count and up to four
context states at+114 (not actor-body health), and
the guest caller and original state/result changes (bounded16 observations),
then preserves the original return/side effects. Native/disabled paths remain
direct pass-through. This identifies the initiating predicate without masking it.
The opt-in startup adapter `pc_story_network_frame` runs before evaluator681C0.
Original rule10 bypasses the network state6 spawn loop, leaving both contexts
inactive after network setup. Once per pending load, at initial state7 with clear
result flags, the adapter calls original D50E0(context,1) for each context still
in initial state4 or16. State4 is observed after skipping Arcade ready placement;
accepting only16 skipped the original spawn entirely. The adapter then runs
the original evaluator; it never forces health, result or pause flags. Actual
runtime activation, ownership and full-view behavior still require validation.
These host hooks require the Windows Online adapter; shared source provides
inert native fallback until MAC-013 supplies native XNet transport/session mode.

Story host controls live in `src/pc_story_lobby.inc`, dispatched by
`pc_display_menu_lookup`. Only connected Story host role 1/lobby phase 11
overrides the original host tick (descriptor `0x004E99C4`). Y opens the original
Story level builder with scoped frontend-mode changes. Selection
stores the mission and explicitly opens difficulty under the captured network
parent, bypassing the original local selector's frontend/load changes; difficulty
confirmation stores the host's choice and returns to the captured lobby, bypassing the
original local-game launch. X opens the local Player Progress descriptor.
`pc_story_lobby_button` rewrites level/difficulty row callbacks to the existing
synthetic `STORY_SELECT_CB`, at the verified US row-builder callsites. Its lookup
dispatches by descriptor/stage. This avoids profile-guided indirect-call guards
calling the original known callback directly before manual lookup; no generated
code or new synthetic addresses are needed. Opt-in `TSFP_STORY_TRACE` reports
row rewriting and mission confirmation for live integration checks.
Difficulty uses an allocated descriptor and four caption buffers (144 bytes
total), built through the existing synthetic `STORY_BUILD_CB`. Its custom builder
creates Easy/Normal/Hard rows and highlights the saved difficulty without changing
the network frontend mode. Back returns to level selection; confirmation stores
the difficulty choice and returns to the captured lobby. The original local
difficulty builder is bypassed; scoping it to mode 4 still returned to the lobby
in the live probe. Selection/back dispatch recognize the allocated descriptor.
The host footer keeps its physical action IDs and substitutes Back/Player
Progress captions. Callback tests cover routing, input preservation, invalid
choices and mode isolation. Custom difficulty, confirmation and own Progress/Back
were reviewed in the isolated Windows run recorded in STATUS.md.
Story choices now live in `story_lobby_mission`/`story_lobby_difficulty`, preserving
guest frontend fields while the network lobby is active. The host header row's
draw callback is rewritten to the existing synthetic builder callback; scoped
lookup invokes the original Story row renderer with the selected mission as a
temporary row ID, restoring it afterward. Its actual header geometry remains
unverified. This is an incomplete lobby layer: briefing, guest controls, settings
replication and Story GO/loading are still outstanding. Native Online remains
dependent on MAC-013's transport adapter.

Native player contact: [fractional clear-slide policy and trace](native-player-contact.md).
C54A0 keeps an eligible clear slide through its original small-motion guard;
opt-in C4700 pairs distinguish actual sweep results from sampled positions.

## Crash breadcrumbs and capture health

`pc_breadcrumbs.h` owns a 64-slot atomic numeric history shared by the Windows
`pc_crash.c` packet and Darwin `native/crash_posix.c` mapping. Producer calls
never allocate, wait or write files; helper snapshots omit interrupted slots.
`pc_display_camera` forwards scene/mode/menu/pause/network/cutscene values from
the existing safe one-second guest sampler, even with Discord presence disabled.
Capture/guest lifecycle and modal-depth events come from the host adapters.
The helper formats `breadcrumbs.txt` during a crash/hang; it is sampled context,
not a reconstructed call stack or guaranteed checkpoint/load-event history.
`pc_capture_health.h` validates the launcher session token and defines the small
heartbeat JSON. Each helper atomically replaces `capture-health.json` every two
seconds in its safe report directory. No valid token means no health file.
Windows packet schema is now3; a mismatched older helper cannot claim readiness.


Windows `pc_menu_theme.h` snapshots each native menu item's caption, tab-separated
shortcut and separator type before owner drawing. Command IDs stay untouched;
do not reintroduce positional label tables when adding PC Settings options.
The per-window metadata is freed on detach. Cocoa menus do not use this adapter.

Recorded-input wait/exit timing: `pc_native_demo_guest.inc` captures the incoming
raw interval around original `43220`, with nested TLS restoration. Numeric
multiplies `433E2/43473` use that interval instead of the original minimum-one
tick. State2 overwrites raw time while looping controllers; the scope prevents
later controllers from inheriting that overwrite. The executed exit site is
remembered across cleanup changing the mode. Exit/EOF retain game pause
but restore incoming raw whole/float time. Original60 keeps original behavior,
including its multi-controller multi-tick asymmetry.
When that path pauses both guest game-time fields, it also clears the published
game delta/ticks/exact units. Raw metadata and clock phases stay intact, so
procedural consumers cannot advance using the earlier pre-exit interval.

Recording state3 now serializes only on raw whole-tick frames. Scoped mode load
`433A0` skips that writer branch on intervening native frames; device polling
and lifecycle checks still run. Pending game ticks survive offset phases after
pause and a live return to Original60. After the original writer, the wrapper
corrects only its newly written records' game byte/start timestamp, preserving
live globals, input bytes and the14-byte format. Buffer identity/cursor bounds
guard those writes; `42535` clears host state on same-buffer initialization.
Unexpected level-clock rebases also clear pending history. An untouched
Original60 recording retains its original path.

`pc_native_demo_playback_guest.inc` schedules authored input groups with exact
raw units, retaining fractional phase across120/144 and a live60handoff. It calls
the original decoder on the selected group, then publishes fractional raw/game
intervals and matching `PcNativeStep` metadata. `pc_native_visual_rebase` preserves
the level clock's fractional offset. Recorded raw timestamps are interval-END
values; level timestamps are interval-START values. Zero-time polling keeps
prior held buttons and suppresses packet consumption. `42535` resets playback
as well as recording; identity changes invalidate history. Fresh bounded VM
reads precede decoding; unreadable/partial groups reach original EOF.

Old native zero-tick groups fold into the following positive group, including
pending game ticks. A leading zero activates that repair in Original60; ordinary
console recordings retain their Original60 decoder. Catch-up consumes at most
one authored group per update to preserve presses, repaying at most three ticks
of pending time on subsequent frames. Sustained sub60 playback can outrun that
budget; full-game/file playback, overload and native Mac acceptance remain open.

`PcNativeStep` now exposes exact `raw_units/raw_phase` alongside game units,
using `units_per_tick`. Both phases describe the interval's start. Raw units
advance during pause; reset/Original60 clear the metadata. Guest fields, clocks,
save formats and pacing are unchanged. Playback sampling must use exact raw
time rather than summing float deltas. Catch-up packets retain their original
multi-tick bytes; the round-trip fixture covers recorder/reader rate dependency.

Native vehicle/physics candidate (`fix/native-vehicle-physics`):
`pc_native_numeric.h` opens vehicle gate `0x17E6C3`, scales linear adjustment
`0x17E6EC` by the smaller live raw/game interval capped at one tick, and retains
the rest timer's authored raw/game cadence at byte load `0x176673`. The same
policy now applies at rigid-body memory DEC `0x171F83`, preventing resting
bodies from sleeping 2/2.4 times too soon at 120/144.
Rigid-clock loads `0x171FB9/0x17208E/0x172119` select `ceil(game_delta)` solver
substeps; multiply `0x172099` uses the live interval instead of a fifteen-frame
average. Original 60 remains on its existing arithmetic. On native-to-original
handoff, `pc_native_timing_guest.inc` rebases that old history at the first
advancing interval, deferring across pause. Routine tests establish elapsed-time
and timer behavior. Original body integration now has force/drag/rotation and
controlled support-contact checks at all three rates. The original entity
publisher and bounds transform, sleep/wake and vehicle component integration
now run in that fixture. Real broadphase geometry, model/render
notification and Mac gameplay remain unverified. The scoped
RNG helper in `pc_native_physics_guest.h` now keeps additional primary solves
from advancing the shared sequence through contact/constraint shuffle sites;
controlled three-contact tests verify its cadence. `17164F` also prevents a
rounding-induced ninth secondary solve. These results do not certify arbitrary
contact geometry or vehicle handling. A subsequent Windows build runs actual
Siberia jeep driving at all presets, but matched throttle replays expose lower
travel at native rates; see STATUS for measurements and limits. The playable
combined crowd build was restored while this discrepancy is investigated.
The original driver dispatcher `17DA00` independently retained a whole-tick
gate at `17DA03`, dropping throttle on intervening native updates. Its numeric
policy now uses positive game time, matching the vehicle consumer's cadence.
The original trigger producer `172EE0` checks forward, reverse, braking and
release on fractional frames; seat weapons and live handling remain unverified.
The rebuilt live replay now matches initial acceleration within 0.1% across
the three presets, but later terrain contact differs. Do not infer collision
parity or measured 144 FPS from that corrected throttle window.
The subsequent `166899` correction keeps character contact velocity fresh
on fractional updates. Original contact math and the body callback forwarding
path verify the fix. In the controlled co-moving support case, body motion
was already correct; the previously stale normal/vector notification changes.
Character movement and world geometry remain fixture inputs, not live coverage.
The object-spawn gate `172AB5` now retains animation-derived initial motion
on fractional events. The original history copy and spawn/setter path test
linear velocity and angular momentum, including a rotated owner transform,
an offset model vertex, pause, absent history and allocation failure. This is
shared numeric policy; live visual/physics and native Mac acceptance are pending.
Windows candidate `3e1ee87` builds and passes its built-in checks with both
contact and spawn fixes; it is retained separately from normal playable `4adb6b0`.
Actor/vehicle impact divide `17CBD6` now preserves the native 60 conversion
factor: actor entity velocity is already normalized by the post-update.
Original mode and contact dispatch cadence remain unchanged. Original
publication, event queue, contact construction, impulse selection and actual
suspension component changes are covered by `native_vehicle_actor_impact.c`.
This establishes response to controlled individual contacts, not repeated
contact dynamics or live driving acceptance.

## Texture-upload timing diagnostic

`texture_pack_trace.h` records bounded opt-in CPU wall spans when
`TSFP_TEXTURE_UPLOAD_TRACE` names a CSV. The common-RHI loader and legacy GL
adapter share it, including the traced reader in `texture_pack_io.h`.
The disabled collector performs no clock/file work. Rows identify key, backend,
mode, dimensions, outcome and monotonic nanosecond boundaries for source-key
hashing (including `cached` calls), open, transfer/allocation,
validation, image creation and (common RHI) view creation. This is CPU elapsed
time, not GPU completion. Reading/validation/fallback and live texture switching
are unchanged. Logging occurs after the measured end, can perturb subsequent
frames and is capped at 8,192 rows/below 4 MiB; do not use it as clean pacing data.
Cached rows have zero read/image/view stages and retain existing request/cache state.
The legacy GL path also closes a trace with `path_rejected` for an oversized path.
Older `2230901` traces began after hashing and excluded cached calls.
See `docs/textures.md` and the runtime tests for usage and failure/bound checks.

| Area | Start here (tracked root unless noted) | Useful checks/docs |
| --- | --- | --- |
| Bindings, input prompts | `src/pc_controls.c`, `pc_input.c`, `pc_bindings.inc`, `pc_prompts.c` | `tests/pc_controls.c`, `tests/pc_prompts.c`, `docs/windows-settings.md` |
| Mouse/menu handling | `src/pc_menu_mouse_guest.inc`, `pc_menu_choices.inc`, `pc_menu_sliders.inc`, `pc_menu_team.inc` | `tests/test_menu_mouse.py`, `test_terminal_mouse.py`, `test_ui_layout.py` |
| Auto-look / camera / cutscene | `src/pc_auto_look_guest.inc`, `pc_camera.h`, `pc_cutscene.h`, `pc_ui_layout.h` | `tests/test_auto_look.py`, `test_cutscene_letterbox.py`, `test_camera_feed.py` |
| Display model and native UI | `src/pc_display.c`, `pc_display_*.inc`, `pc_settings.c`, `src/native/` | `tests/test_pc_display_modes.py`, `test_debug_settings_model.py` |
| GPU clear / screen copies / sampling | `src/pc_clear_rhi.inc`, `src/graphics/`, runtime `src/d3d/` | `tests/test_rhi_menu_sampling.py`, `tests/rhi/`, `docs/shared-platform.md` |
| Texture loading and formats | `src/texture_pack.c`, `texture_pack_format.h`, `texture_pack_io.h`, `texture_pack_stream.h`, `texture_mode.c` | `tests/test_texture_runtime.py`, `tests/texture_pack_tools.py` |
| Texture production and audit | `tools/texture-worklist.py`, `tools/texture-hybrid-plan.py` | Root AGENTS texture rules, `docs/textures.md` |
| System Link / online / bots | `src/pc_network.c`, `pc_network_guest.inc`, `pc_network_bots*`, `pc_online.c`, `tools/online.py` | `tests/test_network_guest_dispatch.py`, `test_online.py`, `test_network_identity.py` |
| Discord presence | `src/pc_discord*` plus runtime integration patches | `tests/test_discord_presence.py`, `test_discord_macos.py` |
| Saves and profiles | Runtime save-routing patch; `tools/testing_profile.py`, `tools/test-native-mac.py` | `docs/testing-profile.md`, `tests/test_testing_profile.py` |
| Crash/hang diagnostics | `src/pc_crash.c`, `pc_hang_policy.h`, `tools/crash-symbols.py` | `tests/test_crash_capture.py`, `test_native_crash.py`, `test_crash_symbols.py` |
| Guest lift/runtime correctness | Runtime `tools/recomp/`, manual hooks, `tools/recomp-generation.py` | `tests/test_recomp_float_joins.py`, `test_recomp_vehicle_joins.py` |
| Audio/movie startup | Runtime `src/hle/` sound and XMV files | `tests/test_audio_timing.py`, `test_xmv_stop.py` |

Names in this table are entry points, not a complete call graph. Read the
applicable adapter plus one relevant test. Tests may encode historical migration
or compatibility behavior, so confirm current defaults in root instructions and
effective source. Preserve unknown/unsupported saved settings across hosts.

The shared title header `pc_gpu_ack.h` selects the audited PFB flush store through
the generated `RECOMP_AFTER_STORE32` hook. It is included by the existing title
policy header for both Windows and Mac, independent of native timing activation.
The runtime validates the request and wakes its existing acknowledgement worker;
game input, clock policy, frame target and saved settings are unchanged. See the
runtime guide for register guards and the required regeneration boundary.

Ghost Gun living-actor reveal alignment (#181/#241) is handled by
`pc_display_weapon_effect` in `src/pc_display_ui.inc`. The integer rectangle
returning to US caller `0x001232A9` is the final filtered screen-copy composite
over the actor's projected bounds. In a full widescreen view it uses full-view
placement, with deferred draws flushed before restoring the surrounding UI
placement. Guest coordinates, UVs/tint and the five preceding blur passes stay
unchanged. The separate humanoid ghost composite retains its existing guarded
call site. Original 4:3 and split-view paths keep their existing behavior.
`tests/test_ghost_actor_overlay.py` covers placement, deferred batches, ABI,
both platform wrappers and a local supported-US-routine check; see build/test
for test prerequisites. Windows scene confirmation is recorded in `STATUS.md`;
native Mac visual validation remains pending under MAC-036.
