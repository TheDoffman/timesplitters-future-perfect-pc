# Shared Online Story campaign lifecycle

The user confirmed on 2026-11-09 that Time to Split is playable from start to
finish with two, three and four players. This is human acceptance of mission
one, not agent verification of the remaining campaign.

The shared lifecycle and host pause-menu changes were committed as4d00f4e0
and integrated into online-story-coop on2026-11-09. Continue Story work there;
the temporary lifecycle branch is no longer the active development branch.

`pc_story_load.inc` includes `pc_story_campaign.inc` before the shared adapters.
It owns campaign eligibility and the original US mission-table scene lookup:
13 records at4D1AC0/stride44h, gameplay scene at+6, intro/outro at+20h/+24h.
The lifecycle stays within typed Online Story, enabled networking, gameplay
stage1, original gameplay flags80008, and active phase15. Checkpoints, restart,
cameras, objectives, dialogue, world objects, effects and drops use that common
scope. Intro/outro movies and rewards retain their separate transition paths.
Arcade, System Link and Local Story retain their original paths.

## Loading, completion and ownership

Story Coop Developer Tools use host-owned speed/audio/god/ammo values over
the reliable class1E channel (`pc_story_tools.inc`). Guests retain only local
camera/movement controls. Host checkpoint advance uses native capture followed
by the synchronized restart, flushing capture before reload; it never sends
bookmark pointers or snapshots. See [Tools policy](developer-tools-integration.md).

`pc_story_network_prepare` preserves the joined ownership header through every
intro, ending, reward page and next-mission GO. Late-join bootstrap applies to
any recognized gameplay mission. `pc_story_network_stage_builder` uses the
selected mission's real gameplay scene for original65940's eligibility checks.
Missions without an ending movie must enter rewards from gameplay directly:
pretending scene106 is already open makes the original builder advance too
early. Guest completion recording accepts a direct gameplay-to-rewards message
only when that mission has no ending movie. It temporarily restores the selected
mission/difficulty globals for original668F0 and preserves the prior values.

Original ready exchange, reliable generations, reload epochs and match identity
remain unchanged. No guest pointers or checkpoint buffers cross the wire.
Checkpoint capture/restore extends native two-player bookmarks to slots2/3;
host-only full-level/checkpoint reload, Game Over, leave, departure and rejoin
rules apply throughout the campaign. Guest rewards precede the waiting page.

## Authored starts and cameras

Guest health HUD recovery lives in `pc_story_hud.inc`, called from the shared
Story frame adapter. Original14D3A0 draws body+268's component while its float
at+140 is positive. Original13A2C0 ages that timer and40 segment flash clocks,
updates20 health/armor threshold states and starts a two-second display when
they change. It never changes health. A7870's only13A2C0 call is in its first
entity-list pass (head7B4CC0, next+0), gated by body+78==0; the other pass omits
HUD aging. Do not force the bar hidden or subtract damage again to repair it.
The adapter supplements this original update only for a validated locally owned
guest human missing that native path. It scans the bounded update list once,
skips bodies already eligible there, fails closed on invalid/cyclic lists, and
deduplicates by guest frame/match/reload epoch/scene. The guest-frame counter
7497B8 keeps fractional beta-rate updates distinct within one60-Hz tick. Reciprocal human
context/body/actor links, active state and existing Story/cinematic/freeze
guards apply. Host, NPCs, Local Story and Arcade retain native behavior.
The original-US HUD fixture covers all13 scenes,2/3/4 humans, expiry and later
damage, native-list suppression, duplicate/fractional frames and invalid/paused scopes.
`story_pair_game.py --mission 1 --health-fade-probe --skip-intro` uses ordinary
host pistol input and read-only guest health/HUD captures through current Online
menus. Require damage to show the HUD, stable health through a four-second wait,
and an expired timer; inspect hit/expired PNGs. Saves/settings remain isolated.
Recycle obsolete command PNGs before a new pass: retained captures from earlier
tests count toward the runner's48-image limit. Keep selected before/after proof.

Human animation recovery in `story_human_pose_frame` is conditional on actual
model-clock progress. The earlier Scotland investigation found missing callbacks
and frozen clocks, but callback absence alone does not prove that a model is
stalled. A mission-one host/guest trace showed native rendering advancing the
clock by1/60 second and the old fallback adding another1/60 each update.
The adapter now compares primary/next clip times, clip identities and loop cycle
with its last completed observation, including any fallback advance. Native
progress resets the stall count; a single held update is allowed. Only two
successive unchanged observations enable one5FEB0 advance per update. This
preserves native30Hz held frames and recovers continuously frozen models after
a short observation window. No playback speed, clip selection or callback is
changed. The original5FDC0 freeze/cinematic path remains authoritative.
Clock history is local per human and bound to body/model, match, reload epoch
and scene. Invalid/frozen/scripted contexts clear their history; global
cinematic/pause/non-Story exclusions clear all history. Restart/rejoin or a
replaced model must establish fresh observations. Bounded optional
`TSFP_STORY_TRACE` records previous/observed/post-fallback clock values and
whether recovery ran (at most16 rows per slot/process).
The campaign CPU matrix covers all13 missions,2/3/4 humans and both roles:
clock progress, intermittent held updates, sustained stalls and continued
recovery, next-channel progress, loop changes, reload/model identity, authored
callbacks and frozen/cinematic exclusions. The original-US contract verifies
the advancing wrapper and freeze gate. STATUS records live timing evidence
separately from full mission acceptance.

Online roles are fixed by `story_player_character` in `pc_story_lobby.inc`:
slot0 is Cortez12, slot1 is this mission's original Splitscreen sidekick from
mission-table row+Eh, slot2 is selectable Private Hicks104, and slot3 is
Private Jones103. First-mission P2 is the original Story Hicks75. Lobby,
validated host/guest launch configs, restart and next-mission preparation all
apply the same mapping while retaining joined ownership and other config data.
P1 remains Cortez12 even where the original final mission selected variant116.

Story's native friendly predicate D6F00 ignores actor team numbers in mode10.
It tests bit8000 in the character definition selected by actor+48 (table53C508,
stride20). Cortez and the authored sidekick carry that bit; selectable Private
Hicks/Jones do not. Shared team1 therefore did not prevent the reticle's D00F0
predicate from treating extra humans as hostile. Do not alter global character
flags: an NPC can use the same definition and must retain authored behavior.
Additive patch3060 wraps D6F00 through `pc_story_network_friendly`. It returns
friendly only for two validated human contexts in typed Online Story: canonical
context/actor slots, active party bounds, reciprocal body/actor/context links
and the existing Story team scope must all match. All other arguments/modes
use the original predicate. No model metadata, target selection, damage rules
or color tables are rewritten. Original D00F0 then suppresses its hostile
reticle indication naturally. Native Story transport remains under MAC-013.
The original-US targeting fixture executes actual D6F00 and D00F0 code: it
first reproduces hostile extras, then covers every human pair across13 missions,
2/3/4 humans and both peer roles, preserving friendly/hostile NPCs (including an
NPC with Private Hicks's definition), incomplete links, Local Story and Arcade
team rules. Existing runtime CI runs this through `test_quit_menu.py`.
The four-player runner's `--friendly-probe` uses ordinary mouse input to aim
owned P1/P2 views at P3/P4, captures the native F84 target and checks bounded
D011C reticle-caller traces. It restores the original look before movement.

`pc_story_network_spawn_pad` asks original CF060 for local slot0 for P1 and
local slot1 for every other online player. The temporary slot, cached-pad and
random-map selector changes are restored before returning; player ownership,
body creation, native authored starts and checkpoint lookup remain engine-owned.
This replaces the former mission-one placement of all peers beside Cortez and
the primary-pad fallback for extra players. P2 keeps that native start. P3/P4
use private tokens7F02/7F03 and copy its32-byte record, offsetting world X by
-1.25/+1.25 metres and world Z by+2 metres. This leaves the mission-one
Hicks approach clear. Height, heading and metadata stay authored.
Private rejoin tokens7F01..7F03 retain P2 pad metadata and the host's validated
numeric rejoin pose; record/tag/index hooks never index ordinary pad tables with
a private token. Local Story, Arcade and movies use their original paths.

All players retain the first mission's native type27 hanging camera intro,
including secondary humans (user correction2026-11-23). No upright secondary
view composition, initial-look rewrite or opening viewmodel omission is applied.
Native camera updates, rendering, animation callbacks and pistol handoff remain
on their original paths. Spawn spacing above is independent of camera position.
The bounded runner accepts `--presentation-probe --opening-view-probe --skip-intro`
for mission0. Fresh early capture sidecars record camera type, animation time,
eye/forward/up/target and human positions. The probe requires all four owned
views to retain type27, advancing animation and inverted up vectors; later
movement, firing and restart probes verify the handoff into ordinary gameplay.
Movement inputs run in actual human-slot order. The role/start probe checks
separated adjacent guest positions and native P1/P2 source selection.
The handoff ammo probe compares pre-movement snapshots; later native Story
grants/pickups may legitimately change one owner's ammunition. Firing consumes
from each owner's post-movement ammo baseline.
The bounded spawn trace reserves six observations per actual slot so repeated
lookups for one human cannot consume another human's evidence.

Camera tags, fades, inset views, active cue lifetime and rally positions use the
shared numeric snapshot path. A fresh later mission must not wait for Time to
Split's pistol handoff: `opening` is specific to mission one. Later missions
lock input for the active authored camera and release it at its end.
For later missions, the owned actor's valid inventory selection is saved at
the active-camera boundary and reselected through the original API after the
camera ends and state2 returns. This prevents a cinematic's older unarmed view
from replacing a recently delivered firearm, without awarding inventory/ammo.
The save is bound to that context/actor and consumed once; reset invalidates it.
The first mission's sniper preparation guard, ammo/grant callsites and broken barricade
mesh remain scoped to their authored content. Do not copy those scripts, weapon
IDs or mesh IDs into other missions. The previously deferred extra sniper freeze
still needs human retesting; generalization is not proof of its resolution.

## Authored inventory grants

The first three-player Scotland probe reached scene4, released all cameras and
passed owned movement, but both guests retained fists (view weapon37) while
the host received its firearm. Bounded tracing identified native script grant
callerC0407 awarding raw weapons56 and5 to the primary human. Later missions
relay only this authored primary-human grant; ordinary pickups and NPC grants
stay individual. Its preceding native ammo callsC03A5/C03BD carry exact primary
and secondary ammo quantities. A grant alone left guests with empty firearms;
the relay now retains these quantities and uses the original ammo-pickup API
before the original inventory grant/select calls. Scotland requests80 pistol
rounds; the native cap produces60 total (eight loaded/52 reserve). Keep the
native cap rather than interpreting the request as a magazine count.
The mission-one pistol/rifle ammo pairs and animation callback suppression
retain their original content scope.

Loadout records use version3 magicSLD3 and a44-byte layout, adding two bounded
ammo-kind/quantity pairs to the legacy fields. Values are numeric, with no
pointers; legacy mission-one records require zero exact-ammo quantities.
Per-weapon replay slots and selected-scene match validation retain both awards.
Later missions use equipped2 for a
carried item without changing selection; equipped1 selects a newly awarded
firearm. Existing inventory never refills or overrides the user's selection.
If the grant arrives before the host's equip action, remember the guest's
selection and finish that transition once, only if the guest has not switched
weapons meanwhile. The later equip snapshot must not award ammo again.
The gameplay content identity separates these builds from older previews.
The campaign fixture executes these grants with every later scene and party
size, each guest ownership slot, reordered carried/firearm messages, grant-before-
equip delivery, intervening guest selection, rejected ammo bounds and repeat
delivery. Live firing and restart acceptance is recorded separately in STATUS.

## Regression and live routes

The existing `test_quit_menu.py` Story lobby fixture includes
`story_campaign_lifecycle.inc`: all13 authored scene IDs times2/3/4 players.
It executes production checkpoint capture, extra bookmarks, ordered capture
plus restart reception, full-level reload, duplicate rejection, guest authority,
fresh camera state, guest reward recording/readiness and late-join preparation.
Original-code config tests seed all13 US records and movie metadata, then execute
the real65940 completion/ending/reward builder for every party size. Existing
mission-one and mutation checks remain active. These are controlled engine-boundary
tests; they do not prove authored checkpoint scripts, collision or full missions.

`Test-StoryCoop.ps1 -Players 2|3|4` opens isolated manual games.
Both this launcher and `story_pair_game.py` now use the current `online_private`
HTTPS directory/UDP relay implementation via `story_coop_local.LocalTestDirectory`.
Only the isolated service URL and certificate pin are overridden. Automated
inputs traverse Story > Online > Online > Matches Available > Start New Match >
Create Lobby (guests select the listed host). No `TSFP_ONLINE_TEST_COMMAND` or
removed `lobby_server` fixture is used by these Story routes. Captures retain
network-chooser, native-match-browser and native-create-options for review.
Role/start evidence resolves the owned slot from the active actor with local_port0;
guest admission can differ from process launch order. Reordered actor/ownership
fixtures reject missing or ambiguous owners before checking native start traces.
`Test-StoryCoop.ps1 -Seconds 15` bounds a launcher smoke run; manual play has no
limit by default. Directory certificates remain in isolated test storage.
The native menu's lobby wrapper must delegate to `story_lobby_lookup` before
its original/Arcade fallback. That preserves automatic guest readiness, authored
Options/Progress controls and Story party returns; bypassing it leaves GO blocked.
The production FOV/dispatcher fixture exercises this composition through the
public lookup for both host and guest, including one-time reliable Ready.
 The bounded runner
accepts three players too. Use `tests/network/story_pair_game.py --load-probe
--lobby-controls --mission 1 --difficulty 0 --players 3 --fire-probe --restart-probe --skip-intro`
for Scotland the Brave loading, owned movement/fire and coordinated level reload.
It waits for actual gameplay after later intros, compares the selected scene and
each player's original weapon/ammo on restart. The authored checkpoint walking
route remains specific to mission one; do not relabel it as campaign coverage.

The shared helpers require no new OS adapter. Existing native Story transport
remains unsupported (MAC-013); Windows fixtures do not establish Mac/Linux live
Story acceptance. Keep that capability gap distinct from shared policy coverage.

## Character/start parity validation (2026-11-21)

`test_story_original_splitscreen_spawns` executes the verified original US CF060
against two authored starts and checkpoint bookmarks, comparing online parties
of2/3/4 with its actual local P1/P2 results. Its all-primary mutation must fail.
`test_story_original_config_stages` compares P2 with the original65940 builder
for all13 missions. The campaign fixture checks host/guest config ownership and
roles; the start matrix checks all13 scenes and both peer roles. These remain
in the existing menu CI route. The bounded four-client first-mission runner
adds `--role-spawn-probe`, which checks config/model IDs and traced native starts;
read-only actor snapshots now include model kinds. Live results are in STATUS.

Patch3045 wraps original158BC0, the Story character dependency preload. After
its authored NPC cast is registered, `pc_story_network_player_assets` passes each
configured human model to the original D7780 dependency registration. This
includes body, hands and animation resources for Hicks/Jones, which are absent
from some missions' fixed cast. It runs only in typed Online Story gameplay
loading; movies, local play and other modes retain their original preload.
The native wrapper shares the adapter but its unsupported Story transport stays
inactive. Missing assets are not hidden with a filename-pointer guard.

The same patch wraps original62F50's exact generic-catalog request. Typed online
Story gameplay reads the original complete dictionary from the bounded P5CK
entry in `pak/arcade/l_100.pak`, through the existing translated game root. Its
918 generic rows match the ordinary Story base; original per-level/cinematic
tables still merge afterward. The extra human body/hand rows are otherwise
absent from the Story catalog. `pc_story_catalog.h` bounds the index/payload,
validates offsets and strings, and rejects compressed/malformed entries. The
replacement uses the original scene arena (3DFA0 tag1), preserving registers,
arguments and the ordinary fallback. No game archive is modified or extracted
to disk. The menu CI route tests the reader, guest adapter and original catalog.

Original62790 omits the shared character archive in Story. Before158BC0, the
adapter mounts `pak/chr.pak` through original622D0(path,0,1), exactly matching
the Arcade mount parameters. Native archive deduplication, lookup and lifetime
remain responsible for the payloads. No model-cache retry or forced resurrection
is added. Resource snapshots include the model pointer at+4; dictionary entries
or metadata flags alone do not establish loaded/constructed models.
