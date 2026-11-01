# Story guest lobby host-loss regression - 2026-10-31

The Story menu fixture also checks chooser reentry with a cached matching
child, plus mismatched/absent-child rejection on both adapter builds. Live
acceptance compares the initial Local/Online chooser with its reopened state
after host-loss return, then repeats Back/reentry before Online reconnect.

The registered Story menu fixture covers both native loss callers, reset before
companion shutdown, captured main destination and reused main-record rebuild.
Other notification/caller/browser/mode/gameplay cases and native fallback stay
original. Supported-US contracts pin1CC440's1AF950 calls,8D notification and
the network selector destination in1AF950. Live acceptance uses two isolated
lobby peers: host Back to main, passive guest return, matching main layout,
and reconnect/discovery after the loss. No mission completion is required.
Historical controls/Mac subsets exclude the leave, departure and lobby-loss
wrappers; the complete-stack migration installs them with their dependencies.

# Story Game Over Quit regression - 2026-10-31

The registered Story lobby fixture covers both roles' EAF60 Quit routing,
local-port0 teardown despite another current port, host advertisement retirement,
guest-only disconnect and unchanged successful result/Arcade/later-stage paths.
Supported-US contracts pin the failed Quit caption/callback and its generic
exit tail. Existing abort fixtures cover the shared lobby/main factories and
retained-party dispatcher. Distinguish synthetic failure setup from real combat
death and full mission completion in live validation.

# Rejoin view and weapon-model regression - 2026-10-31

The registered Story fixture covers an inactive first cinematic snapshot, an
active host cinematic followed by its end, one-shot reconciliation and restored
camera globals. World identity tests use colliding prop/model data: native FP
viewport rooms CA..CE with flag40000000 never publish or resolve prop removals,
even after caching a prior key. Ordinary prop removal/ambiguity checks remain.
Live acceptance must capture the immediate rejoin view and repeated host weapon
changes, then inspect the guest's actual model asset/deletion flags and selected
weapon. Inventory/ammo checks alone do not establish that the model still draws.

# Story rejoin regression - 2026-10-31

The registered Story lobby fixture covers phase12 pending-copy preparation,
request retry bounds, active roster ownership, host epoch/pose reply, incorrect
nonce/host rejection, completed-bootstrap replay rejection, temporary pad
coordinates/heading and a checkpoint restart after adopting the host epoch.
Live acceptance uses two disposable unlocked profiles: host reload before guest
Leave/rejoin, verify weapons and actual movement, then another host reload and
movement. Captures alone do not establish input or checkpoint restoration.

# Story departure regression - 2026-10-31

Existing Story fixtures verify the exact departure bypass, inactive context,
unchanged real/local/NPC death paths, disconnected-slot respawn rejection,
directly restored body retirement and repeated-frame idempotence.
Supported-US contracts pin the peer-removal BABA0/15BCC0 calls and D5480's two
co-op death calls. Original-US execution verifies D2CE0's inactive transition
and15BCC0 unlink retaining unrelated NPC entries. Live acceptance must show a
departing guest reaching main while the host continues, then a host restart
without the departed body. Full mission completion remains user testing.

# Story abort regression - 2026-10-30

Existing registered Story lobby fixtures exercise confirmed host/guest abort,
host lobby-return reception, deferred final frontend factory, root-construction
exclusion, guest companion teardown, lost-host scheduler and Arcade fallback.
Normal reward fixtures remain. The original profile-save fixture also exercises
abort in frontend101 with the real local body/metadata in network slot1. US contracts pin Online2EBE60's Yes branch2EBDB0,
the local confirmation,3592F0 recording/exit calls and main/lobby descriptors.
Original config fixtures execute65420 versus65470 flags,356920 phase16 and
3592C0's reliable classB send. FOV stubs include the new original delegates.
The shared network dispatch suite also compiles on Windows and native enabled/
fallback paths. Story fixtures check phase10 header/body presentation, strict
control ownership, collapsed detail bounds and Arcade fallback. Original-US
fixtures execute the retained-party clock/preparation reset and verify peers,
phase and roster survive. Native live transport remains separate acceptance.

# Story Online Back regression - 2026-10-30

Existing story_menu.c fixture executes browser, host and guest Back wrappers,
checks original callback/cleanup execution, destination argument restoration,
Story chooser selection, original guest reset before companion disconnect,
and original main-menu rebuild after Back (validated parent/active child and
matching captured descriptor). Arcade, offline, gameplay,
invalid storage and out-of-scope destinations retain original routing. The
supported-US contract pins the three original pushed destinations. Full-menu
FOV/settings tests protect unrelated Back callbacks. Native chooser fallback
continues through the same registered test_quit_menu matrix. Live captures and
original recording evidence remain separate from fixture results.

# Story browser advertised level regression - 2026-10-30

Existing test_quit_menu gains story_browser.c on Windows policy and native
fallback routes. It tests changing advertised scenes independently of this
peer's lobby selection, original picture/name IDs, opacity/layout fitting,
frame/bitmap/font-scale restoration, advertised rule2 versus launch rule10, Arcade and invalid-index/scene
fallbacks, and absent sprites. Supported-US call-byte checks tie1CBF9C->64E20
and1CBFBF->2BECE0 to the actual renderer ABI. Historical Mac stack simulation
excludes2435 until the full current stack is applied. This existing suite is
already registered in runtime CI. Live browser captures remain separate evidence.

# Matching Online reward baselines - 2026-10-30

The original guest reward engine fixture starts with the guest first-mission
co-op bit already set, matching the user's unlocked test profile. It failed on
the guest-only2CB210 snapshot before removal, then passed with the unchanged
Online baseline and original reward count1. Own completion statistics and signed
save-view tests remain. XBE assertions tie2D2CB4's Online branch and2D2CBF's
snapshot call to the supported US bytes. This covers the missed matching-profile
case; resetting guest progression to force a new unlock does not certify it.
Live23 verifies matching unmodified profiles: both first pages show Vietnam,
Hotel and Venice, guest cursors0/3/6/9 precede waiting at12, and both peers reach
the next intro. Guest signed completion persists with original masks intact;
canonical fixture/signatures are preserved and disposable disks/locks recycled.
Evidence is linked from STATUS. This is synthetic completion, not a playthrough.

# Empty guest reward entry regression - 2026-10-30

The guest rewards engine fixture now enters through the production menu factory
scope with the real zero-count descriptor5236DC, rather than calling build()
directly. It checks actual count remains zero, the resulting disabled wait panel,
held timer, frame/descriptor restoration and host/offline/other-scene/other-menu
fallbacks. The test also verifies original2D71C3's immediate loading descriptor
and523900's stock callbacks from the supported US XBE. Patch2430 remains excluded
from historical-Mac stack simulation until the full current stack is applied.
Live20 reproduces the stock loading screen with an unmodified unlocked fixture;
rebuilt live21 verifies waiting for that same zero-unlock case. Rebuilt live22
uses a separately signed pre-launch first-mission reset to verify a new unlock,
then waiting. Both final pairs reach the next intro, preserve signed local
progress and the canonical fixture, and recycle their disposable disks/locks.
This is a bounded synthetic completion route, not natural mission acceptance.

# Guest completion and reward pages - 2026-10-30

Existing CI suite test_quit_menu now includes test_story_original_guest_rewards
with story_rewards_guest.c. It translates original668F0/69900/69A60/356870,
3568C0/64D60/2D2BC0 and2CB210/2CB490/2CB550/2CBBA0/2CBAF0/2CBB60.
Production profile-selection/record/page adapters run against isolated8MiB RAM. The peer's
local profile is network slot1, remote slot0 is unloaded with sentinel data.
Checks cover original co-op completion/statistics/dirty flags, own unlock delta,
original multi-page rows, original automatic timer tail call, waiting exhaustion,
already-earned rewards and preserved host/offline exits. The refresh spy asserts
the original menu argument survives both confirmation and timer tail calls.
Uncontrolled services
fail fast; unlock queries are controlled, not a full authored reward catalog test.
The same fixture also executes original696F0/69790/697D0 save accessors.
A slot0 scratch checksum and real slot1 metadata reproduce the wrong save view;
the production1922D0 scope maps captured profile identity/data/checksum/aggregate, retains metadata
updates and restores the remote/network rows afterward. Host save remains
original. Historical Mac stack excludes2420/2425 until the full stack is applied.

# Movie score-packet regression - 2026-10-30

The original Story configuration fixture also translates3578A0/64810/646B0.
An empty late gameplay score snapshot and30 NPC slots reproduceFFFFFFFF in
movie buffer7DE530 through the stock16-entry ranking overflow. The production
score decoder guard preserves the buffer, NPC count and movie clock. Shared
network dispatch tests check movie/frontend guards, ordinary/custom-map100
recovery, disabled/offline transport and native-network/fallback builds. These
remain in the existing CI-owned suites; no new behavioral test entry is needed.
Historical Mac migration excludes2415 until applying the complete shared stack.

The phase15 movie/frontend send-gate regression in test_network_guest_dispatch
keeps original pump calls and network epochs intact while preventing gameplay
context copies, then checks immediate gameplay send recovery. The native timing
suite's network cadence/mutation cases verify the shared adapter at native rates
without changing60Hz clock units. These are existing CI-owned suites.

# Rewards transition and movie fixtures - 2026-10-30

The verified-US engine fixture executes65940/64CE0/357540 and65470/65C70,
plus60310/60390/60410/606E0/60740. It covers authored next-mission selection,
reward readiness with no bodies, register/scene restoration, cached movie-frame
preservation during a guest wait and real load/host/other-caller file cleanup.
Uncontrolled NPC/body services remain fail-fast spies.

story_lobby reproduces the actual unlock-page frontend GO: caller2D3980,
scene106/stage3,16 humans/old scene24. Production preparation builds the next
Story config once, retains two joined humans and difficulty, does not increment
or broadcast a second generation, reuses the pending copy and accepts the guest
handoff. Guest unlock constructor/page/timer tests check a visible waiting panel,
disabled row, absent navigation callbacks, held page/timer and preserved original
host/offline behavior. Existing result and malformed-header coverage remains.
The existing CI suites own these tests. Historical Mac patch subsets exclude2410
with the other Story patches before migration to the complete current stack.
Synthetic scheduler probes are explicitly not full mission or second-level
playability acceptance; retain their outcome separately from these fixtures.

# Manual Story verifier layout repair - 2026-10-30

Compatibility entries such as tools/lobby_server.py are stable logical names.
The manual Story verifier resolves bare tools entries through project_paths
before checking their source hash, including grouped tools/network files. It
retains completed-executable/game verification, normalized hashes and rejection
of changed source; it does not rewrite compatibility manifests or require a game
rebuild. test_story_coop_local covers unchanged grouped files and edited-source
rejection. The local launcher remains Windows-only; native launchers are unchanged.

# Main integration - 2026-10-30

Story fixtures and the pair runner moved to tests/network; the manual launcher
is tools/testing/story_coop_local.py and Test-StoryCoop.ps1 uses that path.
Direct invocation initializes the shared grouped helper paths. Native enabled,
native fallback and Windows dispatch fixtures all remain in the network matrix.
Numbered Story patch dependencies retain payload lifetime before world-body
visibility and main's layout/native adapter ordering. The patch index is rebuilt
from the merged Git index. Native boot conflict preservation mutates main's
current TSFP_NATIVE_DIR main/mmio source list and still asserts no file writes. Main's pinned pytest-xdist/execnet are required for
release session-fixture tests.

# Validation fixtures and Story probes

test_story_original_splitter_camo translates original11F780/11F7E0/11D510/
11D400 from the verified US executable. Captured host01000010/phase1 changes
original opacity from the guest's1 to0. The production presentation helper also
executes an original material shimmer band, restores full opacity on clear,
retains host fade/cycle fields, actor/sound handles and registers, and leaves
host/inactive paths alone. Random cycle activation helpers remain unreachable;
original cosmetic noise/material RNG is allowed. Allocation/world/AI services
are not run. This bounded engine fixture does not establish rendered fidelity.
Story fixture checks SNP5/SPP5 size/assembly, numeric camo and power-up cloak
coexistence/clear. Online fixture checks host capture and guest application;
state validators reject malformed camo. Motion fixture checks smooth elapsed
fade within a phase and atomic cloak/off changes. Both are in existing CI suites.
No full first-mission completion is attempted.

`test_story_original_barrel_authority` starts with the recorded218650 death
callback and translates BB6C0/BB490/BA230/35C870/35AF40/355EC0/35BE90 from the verified US
XBE. Controlled spatial selection feeds a nearby NPC into those original routines.
Without scope the remote source produces no broadcast and HP stays30; production
host scope broadcasts once and original BA230 subtracts10 HP. Following unscoped
shots, guest role and non-Story sessions remain rejected. Allocation/presentation
remain separate in the original-blast fixture; production Story fixture now also
checks the real218702 kind47/weapon35/non-null-direction call becomes one kind4
fireball event. This is bounded engine/dispatch evidence, not a rendered scene.

`test_story_original_barrel_blast` translates originalEFBD0/EDD40/EDBE0/EFF90
from the verified US executable and compiles the production cosmetic guards.
Controlled allocation and particle/audio services now cover both profile1 and
original40/47 dispatch. Seed the EFBD0 eight-entry and EDD40 43-entry jump tables
from the verified XBE, as well as effect constants. Otherwise authored branches
jump to unseeded RAM and the fixture cannot exercise them. Tests assert the
four exact authored emitter descriptors and scale setters, two sounds, profile
and center retention, plus feedback eight units away (outside profile1's5-unit
range but inside the authored25-unit range). Baseline executes three later BB850
damage passes and one186480 cell pass; replay executes none for all profiles.
Story events remain absent. The packet fixture preserves47 and40, optional
finite direction, null peer-local owners, duplicate rejection and the original
standard barrel path. Invalid profiles/direction-presence tags are rejected.
This Python suite already runs in runtime CI. Constructor/dispatch evidence is
separate from a rendered barrel retest and native Online acceptance.

`test_network_original_primary_room` translates originalA1EA0/9E430/A79B0
from the checksum-verified US executable. Captured primary27/overlaps[5,27]
omits the actor from visible room5 even with full scissor/forced masks; the
production shared room helper restores it for16 views. Its disabled-helper
mutation fails. Invalid/out-of-scene/nonoverlapping IDs and corrupt room counts
leave membership unchanged. Story and Online dispatch fixtures check published/
applied primary IDs, packet limits and invalid-state rejection. Existing runtime
CI already runs both owning Python suites. These checks establish render-list
selection, not a newly rendered boundary scene or native Online transport.

The authoritative pickup-state fixture compiles production Story drop code for
both test display configurations. Captured slot28/ID91/weapon25/ammo18qty126
reaches original construction without using the NPC inventory budget. The
original C1950/358160/355EC0 fixture verifies an actual item record, original
Story placement defaults, exact quantities and scoped/restored runtime ID;
asset allocation services remain controlled. States cover moving/landed drops,
reliable recovery/removal, duplicates/reordering, foreign sender/wrong match,
invalid quantities/NaNs, allocation retry and stale local-storage ownership.
The Story fixture also exercises the actual1E dispatcher. Original-US byte
contracts identify the early physical-throw return before the stock send call.
These are constructor/protocol checks; real collection/rendering remains a scene
acceptance test. The existing corpse tests must continue to pass.

The corpse regression translates original B3630 plus15C310/27CBD0/27CEF0/
27CB10 and compiles the production helper. Allocation/pose-copy services are
controlled fixtures: it checks a separate type1000 body, recorded clip74/time1.8,
kind metadata, bounded ring recycling, authored/missing descriptor preservation,
restored source/stack, positive-health/retired-body rejection and inert original
Story callbacks. `story_lobby.c` exercises dead->absent after held-model cleanup,
actor release and exactly one cosmetic copy despite duplicate packets. This is
lifecycle/constructor evidence; it does not establish a rendered corpse scene.

The original-US862C0 fixture executes the actual portal-scissor routine with the
production A2430 adapter: replicas invalidate their room cache while original
portal rejection and valid portal rectangles remain intact. The A17C0/A1AB0
fixture reproduces stale room-plane modes and verifies shared NPC refresh
recomputes them. The original35B190/C2090 fixture verifies temporary wire actors
lack Story descriptors and, through the production adapter, reaches C1950 with
numeric weapon/ammo arguments. Controlled allocation failure then verifies rule
and original receiver-global restoration without loading assets. Real corpse,
pickup and later-room scene acceptance remains separate.

The six-bug follow-up covers owned weapon destruction before replica body removal,
view eligibility only for current posed replicas, and full opening world-body hiding. A retained mode3 after pistol equip must
release opening pose replay/visibility; it is not an active cinematic lifetime.
`network_shot_present.c` compiles the production presenter extracted by the existing
dispatch fixture: definitions41/56/61, missing ordinary-gun models, stale weapons,
restored targets and both Story/Arcade modes. The pinned-US contract verifies the
three original model-creation exclusions. This tests dispatch, not the rendered
later Splitter scene. No new runtime wrapper or wire layout is introduced.

World probes record bounded live bodies outside the active roster, masks/rooms/
bones and type32 held ownership. Two/four-peer combat probes rotate aligned host
captures alongside guest movement without extra screenshots per step. Corpse
fixtures retain host-present unlisted bodies through later snapshots, remove
them on host absence, and observe new bounds inside the original room refresh.

Opening visibility fixtures now seed the original world draw list and require
only hidden humans/attachments to be removed, the count to match, model data to
remain local, and the normal list to reappear after the opening. Team fixtures
also require the peer-cache/config to be initialized before human construction.

The human-team regression fixture also requires every scoped human setter to
retain team1 and D7570 to correct the actor before body backlinks are attached.
The live trigger was a team3 human observed on guest3; see STATUS and the retained
failure receipt. These source changes do not add another runtime wrapper.

Preview1 HANDLE FIRST follow-up extends the existing production Story fixture
with interleaved NPC serials, complete-pair retention, loaded mesh gating, recorded
stale-body reconstruction, numeric cloak state, local speech/lip playback, inset
camera start/stop, attached opening visibility, host Game Over context, coordinated
restart/guest Continue authority, stable prop lookup/removal and ambiguous-key
rejection, cosmetic blood replay/dedup, preserved local impact feedback and history expiry,
and signed16 ammo edge cases. The existing
network guest fixture tests the exact five-byte prop codec and missing resolution.
Run `pytest tests/test_quit_menu.py tests/test_network_guest_dispatch.py` and the
complete `tests/test_patch_updates.py` migration suite. Original-call spies prove
adapter contracts; they do not establish WAN audio/visual fidelity or completion.
Bounded live acceptance uses `tests/network/story_pair_game.py --load-probe --players 4
--fire-probe --combat-probe --skip-intro`; checkpoint/restart probes use isolated
Test Lab copies and never require completing the first mission. See STATUS for
the exact build and measured scope; Mac Online acceptance remains pending MAC-013.

See [build-test-details](build-test-details.md) for detailed adapter contracts and validation routes.

The team fixture verifies the scoped33ACDB setter receives1 for Story humans,
with original arguments/result/stack restored and NPC/other-caller/Arcade
fallback. Supported-US checks confirm original Story selects that shared team.
The four-peer runner checks every loaded human actor retains team1.

The handoff authority fixture verifies host/original fallback, guest animation-
callback suppression, independent pistol/rifle queues, original co-op slot1
ownership and preservation of its existing pistol ammo. Original-US contracts
include the rifle grant call sites. The isolated opening test compares each
owned ammo total with the host to catch animation-triggered duplicate awards.

The Story pose fixtures enforce the original256-byte sender ceiling, assemble
reordered/missing physical/pose parts and reject mixed checksums before applying
SNP2 animation and bounds. They reject
non-finite channels/negative bounds and stale body generations, and preserve
duplicate protection. The display fixture executes the common pose renderer,
verifying local asset/bone handles, crossfade asset resolution and root-motion
flag clearing. Guest dispatch checks Story frozen evaluation as well as existing
bot behavior. Runtime world snapshots expose host/guest clip/time metadata;
source checks alone do not prove live NPC animation or hit synchronization.

Story `--combat-probe` extends the bounded four-peer runner after firearm checks.
It uses ordinary movement/mouse/fire inputs, checks the original NPC ID decoder
would resolve the same entity, searches a bounded opening route for a nearby live enemy marked locally visible,
and requires a host-received guest-source damage trace plus agreeing NPC health.
No target, health drop from unrelated AI combat, or camera/ammo-only evidence is
not acceptance. Read-only world snapshots now include class/team, pose metadata,
last-damage tick and first-ID-match evidence from the original entity pool.
The hit-trace C fixture verifies one unchanged original call, stack/result/health
effects, disabled tracing and invalid-backlink/other-mode fallback.

The Story loadout fixture executes production ammo/grant observers, reliable
serialization, receive dispatch, loading/ownership gates and original-API calls.
It checks replay does not refill ammo, other players remain untouched, existing
inventory is preserved, and malformed/non-host/stale-generation commands do not
grant items. Supported-US call-site checks bind both handoff pairs to their
original inventory functions; live four-peer ammo/fire acceptance is separate.

The extra-start fixture verifies distinct signed16 pad IDs, private template
positions with unchanged room/heading data, tag/inverse lookup, delayed cached
resolution, late remote setup after activation, authored slot0/1 selection, checkpoint release, allocation failure and other-scene
fallback. Complete-stack fresh/repeated and historical Mac migration must also
include the three new original-pad wrappers; source tests alone cannot establish
collision-free playable starts.

The Story fixture now exercises host NPC serialization and guest body creation
from production adapters, initial state/team/identity/list preservation,
duplicate/out-of-order messages, loading queues, absence and ordinary fallback.
It rejects non-host senders, short messages and non-finite state. The reliable
creation message replaces a reproduced null-target guest spawn, rather than
discarding the valid spawn. Fresh runtime construction/pose/combat still needs
the changed four-peer build. The fixture also checks periodic host publication,
same-tick/rate bounds, later roster removal, absence and loading suppression.
Historical Mac migration excludes the dependent
creation patch before applying the complete stack.

The pending-start fixture covers state4 after Arcade ready placement is skipped,
alongside16, and confirms the one-shot token prevents later respawn attempts.
Active2 and script-suspended256 contexts retain their original handling. The
adapter validates the full cached-pad span before clearing each initial pad.

The activity adapter fixtures cover an existing Story NPC, stale/backlinked
entities, player contexts, controller callers, mode/phase and pool bounds. Guest
dispatch tests retain controller enumeration and ordinary bot behavior. The
supported-US fixture executes3568C0/358560 to prove the original controller gate
rejects NPCs and the subsequent script-ownership gate accepts only the host.
They also cover a dormant script actor with no active body and rejection of an
obsolete prototype, and verify dispatch supplies the caller's original entity.

The isolated `story-local-load` route enables Local NPC lifecycle tracing for a
direct original-versus-Online comparison. Scene telemetry includes placed-NPC
count+30 and labels unverified allocation counters by their numeric addresses.
Those counters must not be interpreted as human population. Numeric guest
return candidates are observations, not a validated stack unwind.

The NPC lifecycle patch's four boundaries keep independent eight-call trace
budgets. Complete-stack migration checks the scripted-spawn hook installation.
Scene metadata comparison now proves identical NPC budgets and record counts
between Local and four-player Online; it does not prove the spawn events execute.

Story pair-load samples include the numeric active config tail60..9F, to compare
mission/variant initialization independently of the limited scene/difficulty
summary. The authored-record trace is separately bounded to24 masked records.
Neither a predicate fixture nor a generated wrapper proves NPC initialization;
the first changed runtime probe still failed firearm acceptance after60s.
Local and paired Story samples also record the scene setup pointer, authored
NPC budget index/count, bounded record counts and actor/human allocation counts.
The refreshed Local reference reaches six characters and weapon14 with budget
index8/count5; use it to distinguish resource availability from NPC replication.

The original-US Story fixture executes6B100 with the verified rule jump table:
stock four-player/co-op rejection, stock network exclusion, and retained Normal
versus Easy/Hard and solo-only restrictions. Adapter tests check restored argument
and return/stack behavior, co-op normalization, retained solo/difficulty masks
and Arcade pass-through. Complete-stack migration installs the new wrapper;
the historical Mac-only fixture excludes this dependent patch before migration.

The Story lobby fixture covers the guest intro-to-game pending copy while the
original receiver is still phase15, with no host broadcast from the guest.
Ready fixtures cover zero-actor transport for pending Story gameplay and
ordinary pass-through after that pending start is consumed. The supported-US
357540 fixture executes its real transport tail for gameplay flags80008 too.

The suspended-spawn trace source check also verifies the original context reset
is preserved with its return/stack effects and existing suspension flag. The
patch migration assertion includes the original D50E0 wrapper.

The Story source fixture verifies that suspend/restore tracing preserves the
original suspension flags/context and return/stack effects. The migration
fixture also verifies installation of both original script-control wrappers.
`--script-observe-seconds 20` adds a passive bounded wait only when a peer is
script-suspended; it retains the initial startup evidence and assertions rather
than substituting the later state as a passing start.

`tests/network/story_pair_game.py --load-probe --lobby-controls --players 4 --fire-probe`
extends the isolated route to four actual game processes and primary-fire input.
Weapon IDs, aim and muzzle telemetry use the existing bot-shot field contracts;
they are not ammo or hit counters. Fire-input completion is recorded separately
from load/movement acceptance and requires reviewed captures. Neither proves
Story NPC/script authority or coordinated next-level behavior. Spawn-pad source contracts also check scoped slot/ownership/stack preservation,
valid-result passthrough and inactive/non-Story guards. The migration check
asserts the separate CF060 wrapper is installed. Revised notes require the
header's name, year, difficulty/trophy, thumbnail and population.

`tests/test_quit_menu.py` also compiles `tests/network/story_lobby.c` against the
production Story lobby adapter with isolated guest RAM. It checks host/guest/mode
guards, host Options and each peer Progress routing, preserved network inputs and frontend mode,
and difficulty confirmation without a local-game launch.
`tests/network/story_pair_game.py --load-probe --lobby-controls` adds host Scotland/Hard,
guest reliable settings receipt and Progress/Back before GO and movement.
The controls route sends an ignored guest Confirm rather than Ready; host GO must
work using automatic original readiness. Review its guest-confirm-ignored capture
for only Back/Player Progress, alongside the live lobby authority assertion. The
runner removes TSFP_STORY_LOAD_PROBE so this exercises normal typed Story start.
The changed route needs its own runtime receipt and reviewed captures; the prior
startup-only receipt does not certify these controls. These callback
contracts do not establish live lobby rendering or synchronized Story play.
The isolated `story-host-controls` scenario extends `story-host` with Y/level/
difficulty/X Progress/Back. Expected descriptor checks stop the route if level
or difficulty navigation differs, before a subsequent confirm could press GO.
Run with native process access: restricted probes have failed before the menus
with an invalid-handle exception; native-access runs reached the lobby cleanly.
Its bounded pipe drain uses `read1` and flushes each chunk: live descriptor
assertions must not read a log buffered until child shutdown. A menu mismatch
after that fix is real routing evidence; inspect the captured screen and the
`PC-STORY-SELECT` trace rather than treating the generic exit check as acceptance.
The difficulty step chooses mission 1 and checks the live allocated descriptor
against its trace, plus mission retention and custom-builder entry. Review the
capture for actual Easy/Normal/Hard choices; tracing alone is insufficient.
The probe's raw `mission` field is guest frontend data, not the host-owned Story
choice in network menus. Retention is checked against the custom dialog's own
mission trace. Review the returned lobby header for the selected mission name
and thumbnail, in addition to confirming menu descriptors.

`native_walk_trace.c` tests the actual diagnostic bridge; `native_walk_policy.c`
checks scopes/guards and `native_walk_contact.c` executes original C54A0 with
recorded geometry-service boundaries and independent missing-hook controls.
They check acceptance/ABI rather than complete real geometry. See the
[contact trace schema](native-player-contact.md) before interpreting live CSVs.

## Choose the cheapest relevant check

Online v2 discovery now has immutable `arcade`/`story` session types. Omitted
types retain Arcade; Story capacity is four including the host. The directory
checks type before admission (including private codes), serializes final-seat
joins under its existing lock, and rejects type changes on either heartbeat.
`Session.create/join`, automatic hosting and the companion's `--mode` propagate
this type; Story fails closed against old directories and releases rejected
registrations. Existing `test_online.py` covers mode isolation, limits, concurrent
admission, expiry/departure and old-directory cleanup; the real loopback relay
and `test_online_host.py` TLS invitation checks run for both modes. These are
transport/admission checks only: no Story guest loading or gameplay is established
by them. The in-game chooser is described in `systems.md`. `test_quit_menu.py`
compiles its production callbacks for Windows/shared-native fallback, covering
row routing, original Local descriptor, Back, cancelled/successful typed Story
connections and allocation failure. Its optional local US-image test checks the
original Story route and callback padding. `test_fov_settings.py` retains whole
menu translation-unit coverage. Live navigation and Story gameplay require
separate checks.

For bounded Windows entry navigation, use
`tests/stability_transitions_game.py --run-name online-story-entry --scenario
story-entry`. It visits Local/Back/re-entry and opens/cancels the Story directory
using isolated Test Lab and online settings. Local Back returns to the chooser;
another Back returns to Main Menu. The cancellation helper checks only this
game's `pythonw.exe` child chain, including the venv interpreter child, and the
exact Story window title. Checking only direct game children misses the window.
Passing this route establishes entry navigation, not lobby or gameplay acceptance.

The `story-host` scenario uses its own ephemeral loopback-only directory and
disposable companion automation command under `private/network-validation`,
reusing the same isolated Test Lab profile routing. It connects Story, visits
Start New Match and captures the stock host lobby with opt-in numeric tracing.
Successful runs established host role 1/lobby phase 11 and the menu callbacks
documented in `systems.md`; later local runs failed at `No renderer capture`.
Do not label those failures passed, infer that a stock Deathmatch lobby is Story
co-op, or change capture cadence based on an assumed meaning of `RECOMP_FPS`.

`tests/test_network_guest_dispatch.py` includes `network_bot_motion.c`: a
20 Hz snapshot stream must produce continuous 60 Hz display transforms, while
jitter, loss, angle wrapping, death/absence/respawn and authority remain bounded.
Its zero-duration mutation must fail the cadence assertion; its mixed-crossfade
mutation must fail the atomic clip-transition assertion. Frame sampling also
covers native 120/144 Hz without changing the simulation clock. Existing Windows
and macOS CI invoke this suite; the helper itself has no OS dependencies.
`tools/test-online-bot-motion.py --tag <name> --seconds 60` runs a Windows-only,
bounded production Online host/guest pair through a loopback directory/browser/
bridge, disposable Test Lab saves and separate input/runtime/display settings.
It checks the supported XBE and sealed build before launch, records executable
and compatibility identities, and recycles only its disposable partition images.
Its asynchronous ReadProcessMemory observations measure actor update holds,
not rendered frames, monitor scanout or Internet latency. Window captures are
limited to the test processes. Benchmark fixture junctions are staged locally
as five manifest files, then checked by the normal fixture validator.

Launcher publisher tests require full checkout history for release notes.

`test_pushbuffer_policy.py` compiles the full scanner: disabled/scan/execute/both,
presence semantics, decoding/reporting, and repeated-lookup/executor mutations.

`test_window_theme.py`: menu drawing, mnemonics, inserted/reordered rows (#333).

`test_audio_output_gain.py` / `test_audio_timing.py`: XAudio/SDL, fake devices,
gain overrides, mute, live volume and voice parity.

`test_controller_profile.py`: devices/maps/disconnect and SDL virtual input;
physical pads/Mac gameplay remain unverified.

`test_native_demo_timing.py`: guards, mutations, ABI, pause/EOF/cancel, catch-up;
eight recording-phase and32native/Original60/resume cases through240. Missing
XBE skips originals; live/file/Mac playback and sub60 remain open.

`test_native_movie_file.py`: two XMV intros, Original60/native60..240 including144;
exact decoded pictures/timestamps/PCM and duration. Controlled
and Windows clocks; private-data skips, mocked GPU/audio output. Surface-trace
fixtures use240/180/Original60/120, matching the strict live-cycle validator.

`test_online_theme.py` isolates Tk lifecycles. `test_framebuffer_sampling.py`
checks refresh/stale copies and first-entry/any-framebuffer alias lookup against
the original searches, including wrong-first/last mutations (Mac GL/RHI, Windows RHI).
`test_trace_filter.py` compiles real trace dispatch: quiet calls, explicit
diagnostics, budget, idle callback and bypass-removal mutation.

180/240 coverage includes settings, clocks, input, animation, weapons, networking,
movies, demos and legacy144 migration, not sustained FPS. `test_native_timing.py`
covers original-US movement/aim/physics/projectiles, effects/menus/cameras/lights,
network/recovery/HUD, weapon sounds and countdowns through240. Script events cover
every cadence phase at normal/quarter speed, retaining the legacy missed-event
control. Keep references/tolerances and verify rate coverage when extending.
Mouse cadence/surface math cover240; math is not live water. Mounted/terminal
tests check permissions, bounded queries, 125..8000Hz input and clock/cache/consume
mutations. Private-XBE skips do not establish Mac execution of lifted routines.

`test_sample_chains.py`: depth/saturation/counts and mutation controls on both
hosts; Mac does not execute Windows stack walking.
Runner timing includes16ms test-input polling; normal play does not. Its
`replay_tick` file opens are harness overhead.

`tests/test_network_guest_dispatch.py` checks 20Hz snapshots rendered at
60/120/144/180/240Hz, mixed 60..240Hz intervals, jitter/loss, wrapping, death/respawn, authority and atomic clip
transitions, including cadence/crossfade mutations. Both CI hosts run it.
Windows `test-online-bot-motion.py`: isolated 0/120/180/240 peers.
`--require-results`: matching records; `--short-match`: five points via menu readback.
Results transitions: `--post-results-seconds`/`--inspect-results-tabs`/`--rematch`.
`--require-rematch-results` compares round-two results after reset checks.
`test_network_bot_results.py`: US routines and AI mutations.
Async reads do not measure latency.

Dual-host `test_launcher_release.py` / `test_publish_release.py` cover
Stable/alpha baselines, pagination/completeness, notes and scope/merge/rename/
ancestry. Offline publisher tests need no game/UI rebuild.

`test_auto_look.py`: real records/permission expiry. `test_decal_projection.py`:
four-page batches, larger fallback, permissions and mutation controls.
`test_discord_presence.py`: production read bounds/copies, fresh permissions,
read-only/noaccess, large/cold fallback and retained/full-query mutations.

Character fixtures run original dispatcher/timers and status3 scale/rotation/
composition/cleanup through240, slow/pause/jitter/live switches and old-path
controls. Scale tests use real matrix helpers; allocation/draws are absent.
`native_character_trace.c` tests copied status3 update pairs, identity/frame/
scene guards, removal, invalid/unreadable values and8192row/8MiB bounds. The
original scale fixture also runs with tracing enabled and a failed file open;
stdout/behavior stay identical and open failure is latched. The clock fixture
matches production's manual TLS include order so missing register declarations
fail locally. Paired matrices surround126029->120AB0, after timer advancement;
read-cache lifetimes end before/after guest calls. Captured actual scoped deltas
support per-update growth checks; async samples and an ideal constant60 curve
are insufficient. It is an opt-in diagnostic, not a GPU/pacing acceptance route.
`native_material_motion.c` compares180 fractional partitions with an independent
one-step forced/damped affine map, including weak/zero drag, slow/fast time,
mixed pieces and untouched rejection. `native_surface_cache.c` executes original
127500 with nonzero held forcing/drag at60/120/144/180/240 and mixed rates,
normal/quarter speed and pause, checking U/V velocity/travel against a closed
form authored recurrence. Independent mutations remove damping, force or travel
normalization; GPU calls remain spies. Force-table sampling is controlled, so
these checks do not certify live varying forcing or rendered-material parity.

### Native vehicle integration and diagnostic checks

`tests/test_native_vehicle_timing.py` runs portable byte-load, physics-scope,
partition and lowering contracts on both CI hosts. With the verified private US
XBE it also lifts the original dispatcher, spring/linear/rest routines, rigid
clock/solver/integrator, publisher, bounds, sleep/wake, camera and driver at
60/120/144/180/240.
Private-XBE skips on CI do not certify those routines on Mac.

- `native_rigid_motion.c` checks forces, drag, quaternion/position publication,
  controlled support contacts, RNG isolation and secondary-step boundaries at
  all five rates, slow/fast/irregular time and pause. Static contacts compare an
  independent impulse equation, including penetration escape velocity. Mutations
  independently remove gates, timers, contacts, RNG and boundary corrections.
  The rigid/rigid extension independently checks 432 frictionless impulse cases:
  both masses, existing spin, three-dimensional normals and lever arms, diagonal
  and rotated world inverse-inertia tensors, approaching/separating bodies,
  Original60/native120/144/180/240 and an extra 0.7ms secondary step. It verifies
  relative point velocity and both force/torque impulses before integration,
  including linear and world-origin angular impulse conservation. Independent
  mutations remove the second mass or its torque response. Original166060 is
  lifted for both-body point velocity; original16DF60 damage/effect notification
  is a checked service boundary. This is distinct from full clock scheduling,
  live terrain selection and post-impact handling acceptance.
  Geometry, room/model notifications and broadphase services remain controlled.
- `native_vehicle_driver.c` checks original `17DA00/172EE0` throttle, brake,
  reverse/release and steering recurrence, including wheel eligibility, speed
  clamps and ABI. `legacy-driver` and `legacy-steering` must fail independently.
- `native_vehicle_camera.c` executes full `CA040` plus original spring/history
  helpers. Check fresh updates, aligned camera checkpoints, pause and ABI;
  gate/count mutations must fail before entering a zero-count decrement loop.
- `native_spawn_motion.c` executes original joint-history and spawn routines
  through real velocity/momentum setters. Check all carried-clock phases,
  signed/rotated inheritance, missing history/allocation and pause. The legacy
  mutation fails at native120; controlled services do not certify live models.
- `native_vehicle_actor_impact.c` connects original velocity publication to
  queued actor contact, impact and suspension helpers. Old division/gate-only
  mutations fail; geometry/effect callbacks are controlled.
- `native_contact_velocity.c` checks rotating-body/static/character contacts,
  axes, time scales and nullable output. Both legacy gate mutations must fail.
- `tests/test_native_timing.py` retains both physics-history handoff and boost
  resource cases alongside the coupled-surface fixture. Historical upgrade
  fixtures exclude the later vehicle/texture/panel patches where their earlier
  dependencies are absent; the current full stack must include all of them.

For live driving, use `--native-fps 0` for true Original60 and verify `native=0`
in a solver trace. `--native-fps 60` means native60: older `vehicle-*-60` labels
are invalid original-clock evidence. Default Y delivers logical exit bit0x40;
F/R deliver0x20/0x10. Verify camera24->0, weapon restoration, on-foot movement,
pause/resume and unchanged saves. In Siberia choose LEFT from Recommended to
None, verify roster257, and allow six seconds for vehicle entry before steering.
`testing_profile.publish_input_state` atomically replaces input beside its source,
retrying Windows reader sharing for at most0.5s.
Never truncate a live input-state file: the adapter polls no sooner than16ms and treats
an empty read as key release. Writer timestamps mark publication, not guest consumption. `test_testing_profile.py` checks publication, sharing retries, failure preservation
and temporary cleanup on both hosts.

Bracket camera/body reads by frame and player-camera identity; globals can differ
midframe. Retain excluded samples. STATUS/regression records preserve
counterexamples; async reads do not prove completed-frame trajectories or
broad handling parity. Current-build Siberia contact acceptance (STATUS,
2026-10-25) also finds large post-pillar variation between two true Original60
runs with the same sampled entry pose. Treat that route as nondeterministic;
matching host key durations and initial pose do not fix decoded input boundaries,
all physics state, contact selection or captured-frame stalls. Native180/240
exit/walk/pause pass, but endpoint differences alone cannot identify a native
collision defect. Use controlled off-center geometry/coherent solver state for
the next comparison rather than tuning to one live endpoint.

Contact tracing requires isolated `TSFP_CONTACT_TRACE` path, pool slot0..1023
and finite inclusive `TSFP_CONTACT_TRACE_Z=min,max`. It records generated and
selected lists separately, including empty snapshots, up to16,000 rows/8MiB.
Discard capped partial snapshots. The allocated arena is observed after selection;
its normals can already be merged. `native_contact_trace.c` tests identity,
filters, invalid/unreadable values and limits; physics scope covers nesting and
Original/native paths. `--trace-vehicle-contact` separately enables bounded
original entry traces, incompatible with `--watch-input`; entries do not prove
impulse magnitude. Inherited diagnostic environment is stripped by the runner.

Pacing observations use complete capture-free Present intervals in
spawn+5..+55 seconds with clock neighbors and scene/pause checkpoints. Record
actual window visibility, foreground/client size and bot roster. Successful
presents are not GPU completion or scanout. External stack sampling reports
suspension cost and remains diagnostic; health restoration/keypress timing does
not identify hitch cause. An empty optional-pack control must not remove an
installed pack or edit personal settings. Performance evidence and exclusions
remain in STATUS; mixed bot outcomes prevent attributing gains to one fix.

`surface-native-long` restores focus before `water-stable-start`, then settles.
Its pacing observations must retain foreground visibility throughout the chosen
window. Reject a background run even when its recorder has complete clocks;
do not shorten the window or substitute a background FPS claim. Scotland
standing-water results do not replace crowded-combat or campaign acceptance.

Texture traces use a bounded queue: workers stamp reads; the renderer owns CSV
and image/view stages. Total spans include waits. Cancelled requests neither
dereference owners nor log on workers. Both-host `test_texture_runtime.py` tests
streaming stage order, disabled clocks/files, output failure/bounds, cached
switches and load/view fallbacks. Current rows include key hashing and cached
calls; cached rows must have zero read/resource stages. Verify the 8,192-row
coverage before analysis. Earlier `2230901` traces excluded hashing/cache work.
Use isolated CSVs and intersect attempt timestamps with Present intervals;
QPC/CLOCK_MONOTONIC share the host presentation clock domain. These spans measure
CPU work/waits, not GPU completion, and tracing perturbs timing. STATUS records
partial slow-frame attribution; native Mac gameplay remains separate acceptance.

`stability_transitions_game.py --scenario crowd-venice --native-fps 144
--display-size 3440 1440 --fov 73 --no-native-trace --profile-presents
--no-captures --seconds 180` replays the tracked unlimited-score ten-bot route.
Use `--native-fps 0` for Original60. It requires active Venice checkpoints and
ten configured bots; it does not prove every bot is simultaneously visible.
`crowd-hotel` (scene39), `crowd-temple` (scene49) and `crowd-disco` (scene56)
use frequent move/fire/respawn input. `--crowd-weapons rockets` selects the normal
Rockets preset; use spawn+5..35s to fit the recorder cap. Routes focus at startup.
`--fullscreen` starts borderless via isolated settings and records the request.
Check actual client size/foreground state. `--watch-actors` reads position/health
with pool rechecks; proximity is not visibility.
Use complete spawn+5..55s windows (Disco/Temple end50s), clock neighbors and
scene/pause evidence. `--profile-delay-seconds 0..600` delays CSV rows, retaining
the 20,000-row cap; record it in provenance and verify endpoint coverage.
The present-metrics fixture checks parsing, delayed intervals and cap/disable behavior.
Disable images/tracing for pacing. `--profile-cpu` enables 200Hz diagnostic
snapshots; concurrent rewrites may be partial. `--trace-textures` records bounded
upload-stage timing. Check completeness before differencing either diagnostic.
`test_menu_text.py` covers scan-local VM reuse/expiry, invalid rows and unchanged
original text editing; eager-query/stale-cache mutations must fail. Both hosts'
existing runtime CI includes it. STATUS retains measured performance and limits.
`tests/test_trace_filter.py` compiles the actual runtime name-filter function,
checks zero label scans for absent/empty filters, and retains exact matching,
independent hit limits and zero/negative caps. Its eager-scan mutation must fail.
It is registered in both-host native runtime CI. For changes to this function,
apply the complete patch stack first; passing scan-count checks establish the
removed work, not a gameplay FPS improvement.
`tests/test_gpu_flush_ack.py` compiles the actual patched notifier and lifted
store. It checks post-write notification, exact instruction/register selection,
untouched guest words and lifecycle guards. Omitted/reordered notifications and
a direct-clear mutation must fail. Lifter cases include absolute/indexed/immediate
stores and excluded loads, narrow/segmented stores and register moves. The actual
worker wait is exercised with both host adapters, missing-event/timer-failure
fallback, the 50-ms diagnostic timeout ceiling and the original one-millisecond
busy window. Native `native_platform.c`
also tests real event timeout, prequeued/consumed signals, delayed thread wake and
manual-reset behavior through the microsecond adapter. The registered Python
suite compiles/runs this fixture with the actual compatibility layer on Mac; Windows adapter mocks do not establish Mac execution. Apply all patches,
regenerate/build, then test the previously failing match-loading route before
measuring performance.

`test_text_read_scope.py` checks ascending/descending reuse, boundaries, minimum
spans, nesting and expiry. `guest_span.c` covers real page/cross-page reads;
`pc_prompts.c` checks crossing strings/argument frames, guard/protection changes
and scratch-write ABI in Windows Build. Compare capture-free gameplay with
matched resolution/FOV/bots; query counts alone do not measure FPS.
The suite extracts outlined-text wrappers for both host defines, checking five
draws, ABI, invalidation and fresh scratch writes; removing the scope must fail.
Panel coverage extracts `pc_panel_text_original`/`sub_0012E3F0`: three fit calls,
two outlined labels, one read query, ten draws and thirteen scratch-write checks.
Its mutation removes only the panel scope, retaining outlined scopes. Placement callbacks
assert they run outside the reused mapping lifetime. This is a boundary/ABI
fixture, not execution of the full original panel or a rendered comparison.
`0x12E3F0` uses `pc_panel_text_original`; others keep original callbacks.
`test_ui_layout.py` checks RANK/SCORE left anchoring, batch restoration and
unchanged split-screen/4:3. The scope suite checks callback lifetime.
The first native144 Venice panel comparison is recorded in STATUS: average FPS
and tail intervals move in different directions, so query savings alone must
not be reported as a pacing improvement. Exclude a measurement window when
another game starts during it, even if that run reports a better average.

`test_decal_projection.py` checks ABI/all matrix values, zero weapon/preview
queries and one same-region camera-source query, including split stack output.
Retained-source-snapshot mutation must fail. Local `projection_output.c` lifts
checksum-verified US151CF0, checks 16 writes/math/bounds/stack over 48 lenses;
missing-store mutation fails. Private-XBE cases skip CI. `display_memory.c`
tests real host protection/bounds, multi-page spans and the production display
adapter; retaining its snapshot across calls must fail. Win guard is included.
`guest_span.c` checks private/mapped pages, permission changes, cold/cross-page
fallback and released mappings. Windows additionally checks failed page queries
and disabled-fast-path/write-permission/page-boundary negative controls.
Registered projection/cutscene suites cover both hosts; query savings alone
do not establish FPS improvement.

Native `--input-self-test` must reset the synthetic terminal context between
its fractional-time test and mouse-capture/fire test. The stale context caused
a false failure during alpha.2 validation; clearing it within the self-test
passes the actual Mac input, renderer, settings/debug checks and CTests with
isolated configurations. Use a real local build/test root for partition cleanup;
a runtime-root whose build directory redirects elsewhere is rejected.

Hosted-only contributors need no local model, Ollama service or installed router.
The context/maintenance checker uses Python and Git. The optional broker's Node
contract tests run in CI with disposable/mocked fixtures; they invoke no hosted
or local models. Running those contracts locally needs Node, while the supported
game builders retain their existing dependencies. Guide/index upkeep applies
equally to human and model-assisted changes and can produce a new CI failure
when a covered system changes without its documentation update.

| Change | First checks | Further evidence |
| --- | --- | --- |
| Agent context/index | `python tools/repo-context.py check`; `python -m pytest tests/test_repo_context.py -q` | Changed-path maintenance check against base |
| Patch loader/order | `tests/test_patch_tracking.py`, `tests/test_patch_updates.py` | Full stack transition fixtures; supported build |
| Guest generation/wrappers | `tests/test_recomp_generation.py`, float/vehicle join tests | Regenerate using verified local XBE, build |
| Settings/input/menu | Affected `tests/test_*` and native C fixtures | Relevant native target, bounded isolated acceptance |
| Renderer/shader/resources | Shared source contracts, `tests/test_renderer_host.py`, `tests/test_rhi_menu_sampling.py` | Windows WARP / Mac `tools/test-rhi-mac.py`; scoped scene evidence |
| Launcher | `python tools/launcher-compatibility.py --local` | Host build/lifecycle and affected `tests/launcher_*` fixtures |
| Issue automation | `tests/test_issue_forms.py`, `tests/test_issue_resolution.py` | Read-only fixtures, workflow checks; no posting for tests |

Enhancement routing uses the Ideas contact link in the issue chooser. The existing
`test_issue_forms.py` refresh fixture verifies that release updates preserve that
link and do not restore the removed enhancement issue form. The existing Windows
`launcher_maintenance.cs` draft checks cover the shared launcher Ideas destination,
empty diagnostics payload and unchanged bug-report round trip. These run in the
already registered issue-form and launcher workflows; native Mac builds consume
the same `DiagnosticIssueDraft.cs` URL policy.
The mocked version-refresh PR creation check also verifies that the generated
description preserves Ideas routing instead of referring to a removed form.
Version-only bumps use launcher release-policy, compatibility and both-host
package checks without LFS hydration. They do not prove publication.

Read each harness before assuming it is asset-free. Patch transition tests can
need pristine pinned upstream files and Git access. Use existing dependencies;
do not download/setup everything for a documentation edit. On Windows call
`.venv/Scripts/python.exe` explicitly if `python` resolves to the Windows alias.
In PowerShell each tool call has fresh process state: set `workdir`, use
`login=false` / `-NoProfile`, absolute or root-relative paths, and check
`$LASTEXITCODE` for native tools. A later `Write-Output` or pipeline can hide a
failed native exit code. A profile warning alone does not prove filesystem
operations are forbidden.

The optional `--npc-observe-seconds 0..30` paired-probe flag passively holds
the scene after opening inputs for a simultaneous read-only sampler, then
captures both worlds. It never certifies a death or invents a combat trigger.

Online corpse regression: the existing `test_network_guest_dispatch.py` fixture
checks dead-body absence, missed absence followed by respawn, draw retirement,
repeated snapshots and living replacement on Windows/native-network adapters.
`test_quit_menu.py::test_story_original_corpse_copy` executes the supported-US
B3630 constructor with the shared helper, verifying bounded recycling, pose copy,
descriptor fallback and script callback suppression. Story lobby fixtures retain
the accepted original retirement behavior after sharing that implementation.

The original corpse fixture also executes the production world-removal adapter
with native networking enabled: the original destructor asserts the cosmetic
copy already exists, then unlinks/marks the source removed. A later replay must
not allocate a second corpse. This covers the real ordering missed by the
initial snapshot-only regression.

Online animation cadence: `network_guest_dispatch.c` changes primary frame time
and atomic blend elapsed on consecutive host ticks and requires each pose to be
sent, with repeated same-tick callbacks silent. The previous three-tick cadence
fails this regression on both Windows/native-network fixture variants. Existing
motion tests retain jitter/loss and atomic crossfade guards; guest AI stays frozen.

NPC facing regression: `network_bot_motion.c` checks degree-domain90-degree
turns and350/10 crossings. The former radians wrap fails these checks. Existing
interpolation mutation, jitter/loss and atomic crossfade guards stay selected
through `test_network_guest_dispatch.py`; Story/corpse fixtures remain unchanged.

Four-player rewards: test_story_original_guest_rewards translates original
356800 alongside profile mapper356870. All guest slots1..3 map the single
scene106 frontend camera to context0, retain profile slots and preserve other
port/scene/count/movie/offline routes. Its disabled-mapping mutation must fail.
The patch-update migration registry includes2465; existing CI selects both.

## NPC arm/weapon pose regression

`test_quit_menu.py::test_original_character_limb_pose` is already selected by
runtime-patches CI. It compiles the production pose/weapon helpers with supported
US5AFB0 andA2B50. The recorded arm mask8184/clip108 and a diagnostic aim frame use
a differently indexed local resource; unloaded clip0 is deliberate. It verifies
arm controller capture/apply, sampler asset/frame arguments, unloaded-resource
fallback, numeric rejection, attachment refresh order and room/origin propagation.
The missing-arm-state mutation must fail in Windows and native-network modes.
59320/150960 and weapon selection/matrix construction are stubs; this fixture does
not establish rendered color, final bone geometry or live Mac behavior. The normal
Story/Online fixtures also check expanded-message bounds and assembly. Use the
isolated paired runner for live lifetime/startup regressions, and user gameplay
for the close combat visual acceptance captured in the demonstration.

Dropped-weapon cadence: `story_drop_state.c` publishes60 distinct movement and
rotation samples across60 consecutive simulation ticks, rejects duplicate hooks,
and retains reliable one-second recovery with no idle movement packets. Existing
spawn/pickup-ID/quantity/removal/stale/foreign/reuse checks remain. The companion
`test_story_drop_cadence_rejects_legacy` restores the former six-tick gate and
requires failure at the first held transform. Both are in existing runtime CI.
