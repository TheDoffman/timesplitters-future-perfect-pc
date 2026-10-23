# Subsystem entry points

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

<!-- packet:start -->
Root tracked policy: `src/pc_input*`, `pc_controls*`, `pc_bindings.inc` for input;
`pc_display*`, `pc_settings*`, `pc_refresh*` for display/configuration;
`pc_menu_*`, `pc_ui_layout.h`, `pc_cutscene.h`, `pc_camera.h` for menus/camera;
`pc_debug_options.def` for the shared debug fields/keys/labels/capabilities;
`src/graphics/` for common shader/math policy; `src/native/` for host adapters.
Patches connect these files to upstream; the generated guest files are not the
authoritative place to implement portable behavior.

Textures: root `src/texture_pack*`, `texture_mode*`, runtime `src/d3d/` and HLE
texture hooks; offline tooling `tools/texture-*`. Sanitized replacements now live
in `assets/textures` (DDS via Git LFS), audited by `tools/build_textures.py` for optional delivery;
private source art and original assets stay local. Networking: `src/pc_network*`,
`pc_online*`, runtime HLE/kernel patches and `tools/online.py`. Crash capture:
`src/pc_crash*`, `pc_hang_policy.h`, runtime crash hooks and launcher diagnostics.
Audio/movie timing is runtime `src/hle/hle_dsound*`, `hle_xmv*`, not launcher code.
Find the connecting patch by symbol/path using `tools/repo-context.py find`.
Consult `docs/shared-platform.md` for shared versus adapter responsibilities.
<!-- packet:end -->

| Area | Start here (tracked root unless noted) | Useful checks/docs |
| --- | --- | --- |
| Bindings, input prompts | `src/pc_controls.c`, `pc_input.c`, `pc_bindings.inc`, `pc_prompts.c` | `tests/pc_controls.c`, `tests/pc_prompts.c`, `docs/windows-settings.md` |
| Mouse/menu handling | `src/pc_menu_mouse_guest.inc`, `pc_menu_choices.inc`, `pc_menu_sliders.inc`, `pc_menu_team.inc` | `tests/test_menu_mouse.py`, `test_terminal_mouse.py`, `test_ui_layout.py` |
| Auto-look / camera / cutscene | `src/pc_auto_look_guest.inc`, `pc_camera.h`, `pc_cutscene.h`, `pc_ui_layout.h` | `tests/test_auto_look.py`, `test_cutscene_letterbox.py`, `test_camera_feed.py` |
| Display model and native UI | `src/pc_display.c`, `pc_display_*.inc`, `pc_settings.c`, `src/native/` | `tests/test_pc_display_modes.py`, `test_debug_settings_model.py` |
| GPU clear / screen copies / sampling | `src/pc_clear_rhi.inc`, `src/graphics/`, runtime `src/d3d/` | `tests/test_rhi_menu_sampling.py`, `tests/rhi/`, `docs/shared-platform.md` |
| Texture loading and formats | `src/texture_pack.c`, `texture_pack_format.h`, `texture_pack_io.h`, `texture_mode.c` | `tests/test_texture_runtime.py`, `tests/texture_pack_tools.py` |
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

## State and data boundaries

`pc_story_menu.inc` inserts a Local/Online chooser only at the supported-US
main-menu Story row (return `0x002D0C8E`, ID 0, inherited callback). It uses the
existing menu dispatcher and guest layout/input helpers. Local opens the original
`0x005238E0` descriptor; Back restores the captured main menu. Four verified INT3
bytes `0x1201C..0x1201F` dispatch the chooser. `pc_online_connect_mode` selects
Arcade or Story for the companion; its mode becomes active only on bridge success
and resets on disconnect. The unchanged `pc_online_connect` remains Arcade.
Story Online currently opens the shared network lobby after directory connection;
Story-specific lobby contents/loading/replication remain pending. Native builds
share the chooser and Local route; Online reports unavailable pending MAC-013.
Callback fixtures and the optional checksum-verified US entry contract live in
`tests/test_quit_menu.py`; they do not prove runtime navigation or co-op gameplay.

For opt-in Story lobby diagnostics, `pc_display_menu_mouse_footer` honors
`TSFP_STORY_TRACE` only for a connected Windows Story session in lobby phase 11.
It reports numeric menu descriptor callbacks/footer fields once per descriptor;
it does not change lobby behavior. The measured host descriptor is `0x004E99C4`,
with builder `0x001A8DA0` and tick `0x001A8D30`. Its existing Options and character
actions still enter the ordinary multiplayer menus. Story lobby replacement is
pending; a connected bridge is not evidence of Story loading or replication.

Online bot presentation (#222) uses `pc_network_bot_motion.h` from the
`pc_network_bots_guest_frame` hook. The transport still consumes each validated
host snapshot once. The guest caches it and updates the displayed body, spatial
bounds and matching animation frame times every frame over a 50 ms span, retargeting
from the current display pose when arrivals vary. Discrete vitals, scores, shots,
absence and spawn transitions retain host values; no guest AI or extrapolation
is enabled. A gap over 250 ms snaps to the next received host state. Match,
host, actor-pool and inactive-state changes clear presentation history. The
Windows QPC adapter supplies monotonic seconds independently of guest game time.
Human controller replication and the host's three-tick send interval are unchanged.
Native Mac fallback remains original until its network adapter exists (MAC-052).
Only clip-block `+8` (model `DC`/`134`) is interpolated within the same clip/cycle;
the other animation channels and crossfade controls stay atomic. US `5CEF0`
can promote the secondary clip during zero-advance evaluation, so mixing fade
duration/elapsed with another snapshot's clip identity can select an invalid clip.

Camera and perspective corrections use `pc_display_memory.h` to retain one
OS-validated writable region only after their original guest routine returns.
The stack-local snapshot expires when the correction returns; no guest calls,
allocations or protection changes may occur while it is reused. Bounds,
overflow, read-only and guard/no-access rejection remain enforced. Other
display/menu validation still uses its existing checks. This avoids repeating
the VM query for camera, output and source matrices within the same region;
it does not cache matrices, reduce updates or change projection arithmetic.
Decal corrections validate the camera and its source matrix before the output
because the original callers build their temporary output on the guest stack.
Separate camera/stack regions therefore need two queries, not three.

The native SDL input self-test ends its synthetic terminal context before
testing independent mouse capture/fire behavior. The fractional-terminal case
otherwise leaves `camera_input.terminal_active` set after its mock guest is freed,
suppressing the next case's Fire button. This is test isolation inside
`tsfp_native_input_test`; ordinary frame/input behavior is unchanged.

`config/*.default.*` is tracked default configuration; `config/pc-controls.json`
and `config/pc-display.ini` are personal. `private/game` is the single extracted
game-data copy; `private/saves` is personal; `private/test-fixtures/unlocked` is
the canonical local fixture and must remain unchanged. Test runners copy it into
bounded disposable test roots. `private/texture-remaster/pack` is the one active
6-GiB-capped art-production pack; source art and original game assets stay ignored.
Sanitized compiled DDS/JSON replacements are tracked in `assets/textures` and
published as optional packs; `tools/texture-repository.py` stages them with LFS,
`tools/build_textures.py` audits them. Leave pointers unhydrated for unrelated
source work. Optional pack publication must reject missing LFS content. Ordinary
game packages omit textures; existing installed packs and runtime fallback remain.

For installed layouts, use the launcher platform adapter instead of assuming
checkout names. Never include private data, generated guest code, credentials,
environment dumps or personal configuration in durable context. Store paths,
symbols, semantics and sanitized measured conclusions instead.

## Prompt text layout read scopes

`pc_prompt_guest.inc` reads through `pc_prompts_read_region`; the bounded
thread-local `pc_text_read_scope.h` cache uses existing OS VM query adapters.
It is valid only during synchronous text layout, with no mapping/protection
changes or event dispatch. Nested scopes retain intervals; the next outer scope
clears them. Scratch writes still use a fresh protection query.
The cached interval covers the full OS-validated region, so descending guest
stack reads and earlier labels in the same mapping reuse one query. It never
extends beyond that region or survives the outer layout scope.
The US outlined-label wrapper (`sub_00041E30`) retains this same scope over
its four outline draws and foreground pass, then expires it at return.
Scope wrapping, label construction/drawing and nested prompt measurement use the shared
hooks in `pc_tutorial_ui.inc`, `pc_display_ui.inc` and `pc_prompt_guest.inc`.
`pc_display_menu.c` includes `pc_prompts.h` before the .inc callers to avoid
implicit declarations. Do not extend caches across menu ticks or frames.
Tests cover region boundaries/expiry and actual prompt-hook scratch ABI/fallbacks.
Issue #105 also requires the runtime late-frame pacing patch; caching alone
measured 30 FPS. See STATUS.md for measured acceptance and Mac limits.

## Physical keyboard text entry (#136)

`src/pc_text_input.h` defines the shared bounded per-dialog text queue and edit
commands. Windows `pc_controls.c` supplies WM_CHAR/WM_UNICHAR; native Mac
`input_sdl.c` supplies SDL text events and starts/stops text input on its main
thread. Both adapters suppress keyboard menu bindings while a text owner is active
and discard queued text on focus loss or owner changes.

`src/pc_menu_text.inc`, included by `pc_menu_mouse_guest.inc`, recognizes live
character-row callbacks (`0x0007DB20`) rather than one dialog descriptor. Create
Profile embeds this keyboard in descriptor `0x004E8D54`; testing only standalone
`0x004D8D78` misses it. The original guest insertion/edit/Done routines retain
selection replacement, limits and save behavior. Escape queues the owning menu's
Back action, preserving embedded-menu callbacks. Numeric descriptor `0x004D94D4`
is excluded. Supported input follows the original Latin-1 tables; paste and
arbitrary Unicode are outside this change. See MAC-059 and the #136 STATUS entry
for physical Windows confirmation and pending Mac/dialog-matrix validation.

## Automatic display size

`pc_display_current()` is the selected/persisted mode: `0,0` retains console
resolution, `1,1` means Auto, and other valid dimensions are manual. New missing
settings default to Auto; existing settings are preserved. Render/camera/UI code
must use `pc_display_effective()` or the shape/scale helpers, never interpret
the Auto sentinel as physical pixels. The shared catalog appends Auto without
renumbering existing ratio groups. Guest Display settings and Cocoa expose it.

On Windows Auto reads the fullscreen window's nearest monitor desktop mode,
or its non-minimized client pixels in windowed mode. The SDL main thread
publishes drawable pixels via `pc_display_host_size`; the render thread reads
an atomic snapshot. Frame completion checks at most four times per second;
GPU allocation failure retains the last effective size and retries later.
Unsupported/empty sizes retain the last valid output (guest output before a window
exists); current supported limits remain landscape 640x480 through 7680x4320,
no wider than 4:1. Manual selection disables this tracking. Desktop resolution
and refresh rate are never changed.

## Crane viewport edge clipping (#221)

`pc_ui_vignette_clip` in `pc_ui_layout.h` converts the crane/remote-view
vignette's full 0,0..639,479 clip to host-exclusive 640,480 bounds only for
STRETCH tag `0x00137010`. Shared RHI `d3d8_GetScissor` and legacy GL
`native_apply_scissor` apply this policy before output scaling. Stored guest
clips, other tags and partial rectangles remain unchanged. The title helper
scope supplies the tag only for complete single-player non-4:3 views.

The pending Story startup attempts original D50E0 once for each context still
inactive at the first evaluator, rather than requiring the whole roster inactive.
A remote spawn may arrive before a guest reaches this boundary. Active contexts
are preserved, original ownership checks remain in D50E0, and the pending token
is consumed once so subsequent deaths never trigger initialization. Mixed-state
regression passed; the changed automatic-ready paired route reached [2,2] on
both peers with matched movement and reviewed weapons/HUD views. Host original
spawn correctly denied its remote context; guest initialized that own context.

A wrapped-row replacement experiment found that callback198E20 draws the roster
as well as Arcade details. Replacing the entire callback hid the roster despite
showing localized briefing. That experiment was reverted (saved ignored under
logs/story-briefing-experiment). Preserve198E20; replace only its lower details
callee1986A0 or an equally scoped drawing boundary. Header flags4/style11 also
failed to expose year/contrast and were reverted. Briefing remains unfinished.

The lower-details replacement now scopes original198E20 through a synthetic body
callback and intercepts only its1986A0 call. Existing40F00 word wrapping returns
a guest string, drawn by41DC0(x,y,text,color), with original font/fade services.
The roster still draws through original198E20. Paragraph placement remains a
runtime visual check; no source fixture can certify the final bounds.

The narrow briefing renderer retains both rosters live, but initial visual review
showed paragraph clipping. It now measures wrapped height through3FCF0, reduces
shared font scale only as needed within the lower panel and restores all font
state after drawing. The fitting correction awaits its changed visual check.

The fitted Scotland briefing was visually reviewed complete on both peers while
preserving roster/controls. Header readability uses the original briefing-menu
opacity branch during the synchronous heading draw, restoring descriptor after.
Revised notes now require the year (superseding the earlier optional-year
clarification); the later metadata renderer supplies it.
The heading opacity correction awaits runtime review alongside four-player work.

Four real instances now verified admission and matching host settings in one
Story lobby. Name opacity correction and complete fitted Scotland/TimeToSplit
briefings reviewed with roster intact. The year requirement was subsequently
restored in the revised notes. This does
not validate four-player level load or world/interaction/next-level authority.

## Mounted and CCTV gun mouse aim (#182)

`src/pc_mounted.h` handles guest player mode `0x80` before ordinary player
camera updates in both `pc_controls.c` and `native/input_sdl.c`. The US guest
`sub_0025D520`/`sub_00258A20` aim an AGUN through player entity data `+0xC14`;
ordinary player yaw/pitch do not turn it. The shared helper validates its AGUN
tag and reverse owner link, uses local input port zero (not guest player ID),
and updates gun pitch/yaw with authored limits, sensitivity, inversion, FOV and
optional gun speed. Mouse displacement is consumed once without frame-time
multiplication. Invalid ownership, pause, focus loss and stale motion consume
input without falling through to hidden player angles. Terminal mode `0x2000`
and its left-click mapping retain `pc_terminal_mouse` behavior. Controller
input remains in the guest routines. The separate vehicle-seat mode `0x8000`
is not routed by this helper. `tests/test_mounted_mouse.py` runs the guarded C
fixture on Windows/macOS; physical Mac validation is tracked under MAC-001.
The mounted helper receives the adapter's explicit native-timing selection.
Native mode accepts finite positive game-float time at `4E794C`; Original60
retains its whole-time gate at `4E7944`. Both consume/rebase mouse batches with
`pc_input_consume_motion`, preventing fresh packets from expiring against a
previous batch timestamp. Paused/invalid native time consumes without aiming.
The helper still handles only AGUN mode `0x80`, not vehicle-seat mode `0x8000`.
Host input self-tests route synthetic motion through `pc_controls_apply_mouse`
in Windows and SDL, with zero-whole positive time, paused stale whole time,
live return to Original60 and return to on-foot controls. Windows execution
passes; native SDL self-test execution remains a host validation requirement.

## Unified Video Options and native presets (PR #285)

`pc_display_menu.c` stages resolution (including Auto), aspect, vertical FOV,
texture style, FPS and subtitles behind one dirty-state Apply. Auto uses the
`1,1` saved sentinel and is labeled Auto, never displayed as a pixel size.
Controls follows Video Options; conditional frontend Cheats is last. Audio
retains its frontend/pause-specific alignment and common highlights are rounded.
`pc_frame_rate_settings.h` persists the beta presets; live requests enter
`pc_frame_rate_live.inc` and commit at the guest clock boundary.

The merge retains main's nested text-read scope and fresh scratch-write checks
rather than the older per-hook stack-query optimization. The real-protection
fixture in `tests/pc_prompts.c` retains boundary/ABI checks; query reduction is
measured inside the cache by `tests/text_read_scope.c`, because reads now execute
in a separately compiled translation unit. Mounted mouse aim remains ahead of
ordinary camera input in both adapters; fractional terminal handling is retained.
Native high-refresh validation is MAC-060, distinct from crane MAC-058 and
text input MAC-059.

## Issue #74 menu texture additions

The branch pack now contains 2,046 accepted replacements. The same keyed DDS
files serve Windows and the common native Mac renderer; loader contracts are
unchanged. Menu coverage is 574/750 keys, with 164 original character icons, eight
intentional functional originals and four unresolved replacements.
`config/texture-originals.json` records excluded icon keys; the worklist rejects
installed replacements for those keys and hybrid planning retains the original
lane. Character model materials remain in scope. Source resampling and original-detail
recovery are distinguished from newly generated art in `docs/main-menu-textures.md`.
Offline acceptance does not certify mapped scene appearance or Mac GPU behavior.

The original-icon policy also includes historical public DDS/JSON hashes.
`build_textures.ensure_pack` rejects reintroduced icon keys and emits these
identities in `BUILD.json` under `textures.retired` for both game packagers.
The shared launcher retires only matching installed pairs transactionally;
custom artwork stays protected. See the launcher guide for recovery behavior.

## Native movement boost resource

The US walking routine `sub_00258F50` gates a movement boost on mode byte
`0x7DE456 == 0x11`, selected player `0x7DE498`, input bit `0x800`, and positive
entity+0x1D0 resource. The fixed subtraction at `0x259060` is now scoped through
`pc_native_boost_drain` in `pc_native_numeric.h`. Native frames split one active
original-update charge by `max(0, min(1, raw_delta, game_delta))`: slow motion
retains the original zero-whole-tick gate's reduced consumption; fast/catch-up
updates still consume at most one charge. Original mode and other sites retain
exact subtraction. No constant table, boost speed, threshold or stored clock
is changed. The complete US routine fixture uses a controlled flat collision
floor; it does not establish real-map geometry, input acceleration or network
transport parity. The game rule-name table at 0x4D1F90 + index*0x20, decoded
through the existing Discord string-table path, identifies index 17 as Escort.
A bounded frontend lookup confirms the name; live boost activation remains
unverified.

Escort recovery takes a separate route: active-match `sub_00068830` calls
`sub_000681C0`, then `sub_000654E0`. Its original game-clock delay is more than
five seconds; eligible match updates include zero-whole-game-tick frames.
`pc_native_boost_recover` therefore scales only addition site `0x6552D` by
`max(0,min(1,raw_delta))`. The original dispatcher gates pause; its selected-actor
write and mirror, threshold overshoot and health-dependent clamp are preserved.
The full three-routine fixture controls external services and validates those
boundaries separately from the walking drain's active-game-update budget.

## Experimental water coefficient precision

`pc_surface_coefficients` in `pc_surface_bias.h` preserves the current guest
uploader's clamped/truncated byte anchors, then scales acceleration/displacement
and exponentiates damping without repacking fractional values. The existing
bias+float+fractional opt-in gates enable it. `pc_native_surface_guest.h` copies
original parameters into the HLE state before temporary guest scaling; whole
steps retain original uploads. The HLE path copies bank 0 stages 3/5/7 and bank
1 stage 6 into ordered commands only for shader 0x4BBF10, caching duplicates and
clearing on scope exit. Defaults and original/catch-up behavior stay unchanged.
The constant-uploader fixture documents the current lifter's truncation; this
change does not claim original Xbox MXCSR rounding fidelity or full wave parity.

The opt-in precision policy now selects full-float format 0x4B for registered
feedback intermediates. Final byte textures, guest headers, rate gating and
normal-play defaults are unchanged. Common-RHI promotion/demotion preserves
live GPU contents and capture bytes; the transaction still flushes before copy,
rebinds only on success and retains the old resource on failure. This removes
measured half-float accumulation error but does not prove original wave parity.

Optional game packages use `build_textures.optional_identity` to retain only the
small `textures.retired` migration, without any DDS/JSON payload or LFS access.
This preserves the original-icon fix using the existing shared launcher consumer;
ordinary missing texture files never cause removal of an installed pack.

Merge with main (2026-10-22): the common button/lookup dispatcher retains Story
chooser and lobby interception alongside Audio, subtitle-row and settings
callbacks. Both sets of callbacks remain in the shared translation unit.

Revised Story notes require year, selected difficulty and population in the
lobby header. `story_lobby_status` uses the mission record's localized year ID
and actual occupied controller rows, with scoped small-font state below the
original title/thumbnail. It never substitutes profile completion for selected
difficulty. Trophy icons and changed runtime layout still need validation.

Story preserves authored active-character membership when the Arcade network
tick reconciles its player-only actor pool. Original3559A0 otherwise removes
every nonplayer from list74C37C/7A3580 via15BCC0 at355E9F. Death/script removal
callers remain unchanged. This preserves NPC membership, not NPC replication or
verified world authority; its live first-level effect still needs measurement.

The direct-load Story adapter now selects original stage1 and calls65940
without the frontend factory's unconditional stage0 reset, then records stage1
on each peer. Stage0 constructs an intro configuration;
changing its scene/flags afterward was insufficient gameplay initialization.
This corrects factory selection, while synchronized intro playback remains open.

Fresh four-instance Windows captures show Time to Split with localized 2401,
Normal and 4/4 Players, while keeping title/thumbnail/roster/briefing and host
footer. This validates header placement; trophy rendering remains unfinished.

Initial pending Story context spawns discard cached Arcade map pads before
calling original D50E0. Original CF060 can then choose authored Story starts.
Only still-inactive contexts are affected once per pending load; active remote
contexts and later deaths retain their existing handling. Runtime placement
requires fresh first-level captures and cross-peer positions.

Story character membership is now preserved during pending load as well as
active Story scenes. Restricting the prune guard to network phase15 allowed
newly initialized NPCs to be removed during loading. A separate eight-event
trace records phase/scene/pending/count; it does not alter character state.

Story GO now asks original65940 for stage0 and retains its cinematic scene,
movie ID and flags, including the original no-intro fallthrough. Received intro
selection is derived from original mission/movie metadata, while retaining the
host gameplay difficulty. Only host65C70 advances after the intro; its pending
config uses the original reliable network start handshake for all peers. The
manual context-spawn adapter runs only for gameplay stage1. These source changes
still require a changed four-peer runtime and duplicate-character visual check.

The first live intro reached original scene24 on all peers but remained
network phase10; the stock ready routine rejects cinematics, eventually causing
a peer to leave. Host/guest captures show the authored cinematic, not accepted
co-op readiness. The new cinematic-ready adapter runs only original transport
startup while skipping its gameplay actor loop, restoring all cinematic state
afterward. Long intro stability and the changed handoff still need validation.

## Direct keyboard inventory slots

`pc_weapon_slots.def` appends host-only actions after the 24 Xbox controls;
`pc_weapon_slots.h` owns press edges and expiring requests. `pc_bindings.inc`
loads/saves them through the normal binding editor and migrates only missing
slots. Stock White=1 and Black=2 move to free Z/X keys; existing custom and
explicitly unbound actions remain intact. No migration writes until Save.
Both Win32 and SDL gate requests on focus, mouse capture, menus and text input.

The additive keyboard-weapon-slots patch wraps US `sub_001BF140`, the original
player weapon-input routine. `pc_weapon_slots_guest.inc` runs on that guest
thread after ordinary input, using the ordered table at `0x4FC888`, the original
variant filter `sub_001BAA70`, usability `sub_001BA410` and selection
`sub_001BA130`, then refreshes the weapon HUD with `sub_00142820`. It deduplicates
weapon groups, checks local input slot zero rather than network player ID, and
never grants inventory. Empty slots do nothing. Settings expose exactly ten bindable
slots, defaulting to 1–9/0. The separate 70-entry internal table bound covers
weapon IDs and variants; it does not add settings rows.

## Themed live keyboard and mouse editor

`pc_keyboard_menu.inc` is included by `pc_display_menu.c`, adding Keyboard and
Mouse to both main Settings and paused Options. It uses the original menu
builders, callbacks, highlights and footer navigation. The controller-style
bindings pages draw a keyboard/mouse diagram with primary/alternate columns;
`pc_keyboard_menu_model.h` partitions all 34 actions across short pages.
`PC_BINDING_SLOTS` is two in the shared model and native dialogs; old keyboard
profiles use their first two sources without modifying disk until saved.
Controller profiles keep their existing source capacity.

`pc_binding_capture.h` handles opening-input release, capture, final-input
release, Escape cancellation, focus loss and timeout. `pc_bindings.inc` owns
its lock/state; Windows and SDL supply raw physical keys and wheel pulses, latching press
events so quick taps are retained between polling ticks.
While capturing, controller polling and menu pointer dispatch are suppressed,
so E/F/Y and mouse clicks bind without activating menu commands. Assignments
use the existing atomic settings save/conflict API. Mouse sensitivity previews
live, debounces writes, flushes on exit, and restores previous values on error.

Capture starts once in the new page update. Native descriptor +12 is a per-frame
callback, not a destructor; installing capture cleanup there cancels it every
frame. The callback fixture in `tests/fov_settings.c` exercises both native
per-frame callbacks and requires a selectable capture instruction row.

## Host storage choices in Create Profile

`pc_storage_menu.inc` injects mounted host drive rows at native storage-button
site 001944C3, directly in Select Storage Device. Rows sort by drive letter on Windows and case-insensitive volume name on Mac;
Rows show only volume names and drive letters, with no game/save role suffix.
Native menu sizing is widened before its
title at 0019440B. Labels are ASCII-safe for the guest font. Windows lists mounted
drive letters; Mac lists the startup volume and /Volumes mounts. Neither lists
unmounted partitions. The native 00192770 update and 00191470 icon adapter keep
host indices away from the console device table. Selection delegates to original
00192E60 with console index zero, preserving the save/overwrite state machine.
After a drive change, synchronous 00024EC0/00190F70 enumeration rebuilds the native
profile list (74F9FC) before duplicate-name validation against the new location.

`pc_storage.inc` supplies actual volume metadata, cached five seconds for original
storage-text replacements. `pc_storage_routes.inc` validates a writable save
folder, prepares UserData/45410066 for the native free-space query, and atomically
saves save-location.txt beside RECOMP_INPUT_CONFIG. Changes immediately redirect
future UDATA opens through the registered kernel adapter; failed persistence
rolls that route back. Existing handles, caches and partition devices stay put.
Startup also honors the preference; profiles are neither moved nor deleted.
An unavailable configured location fails explicitly instead of silently
creating a new profile elsewhere. TSFP_SAVE_LOCATION_LOCK protects isolated test
sessions from changing real drive preferences. Windows resolves junctions and
volume identities; drive metadata retains executable-volume identity internally.
The Mac metadata adapter includes unistd.h before calling getattrlist; sys/attr.h
provides attribute structures but not that function declaration. The production
pc_prompts.c compile contract checks this include order in Mac CI.


Story/main merge (2026-10-23): Story chooser callbacks now use verified unused
US INT3 bytes 0x1237b..0x1237e, distinct from keyboard callbacks 0x1201c..0x1201f
and drive callbacks 0x12379..0x1237a. The shared button hook retains both Story
interception and drive-dialog handling. The common display fixture executes
both Story pose preservation and host storage tests after resolving duplicates.
Main's bot presentation helper remains alongside the separate Story replica path.
No new OS adapter is introduced; native Online remains limited by MAC-013.
