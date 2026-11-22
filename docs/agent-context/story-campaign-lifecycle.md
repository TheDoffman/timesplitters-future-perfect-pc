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

Human model animation advancement is repaired in `story_human_pose_frame`.
The paired Scotland recording showed moving player contexts with stationary
model clocks; the isolated observer confirmed normal game delta/playback rate
but empty body model callbacks on both peers. Registering a render callback
alone did not advance their clocks. For fully linked live humans in ordinary
state2 without an authored callback, advance the model once after the original
game update and cinematic adapter. Use5FEB0, the original advancing wrapper
through5FDC0, so authored freeze flags retain their existing path. No callback,
clip selection or native controller is replaced.
Existing callbacks, scripted/cinematic contexts, frozen/dead actors, departed
players and non-Story sessions remain untouched. A fresh/restarted/rejoined
body receives the same update without recreating its actor or inventory.

The campaign CPU matrix covers all13 mission scenes,2/3/4 humans and both roles,
preserved authored callbacks and frozen/cinematic exclusions.
The original-US contract verifies the advancing wrapper and native freeze gate.
STATUS records bounded live evidence separately from full mission acceptance.

Online roles are fixed by `story_player_character` in `pc_story_lobby.inc`:
slot0 is Cortez12, slot1 is this mission's original Splitscreen sidekick from
mission-table row+Eh, slot2 is selectable Private Hicks104, and slot3 is
Private Jones103. First-mission P2 is the original Story Hicks75. Lobby,
validated host/guest launch configs, restart and next-mission preparation all
apply the same mapping while retaining joined ownership and other config data.
P1 remains Cortez12 even where the original final mission selected variant116.

`pc_story_network_spawn_pad` asks original CF060 for local slot0 for P1 and
local slot1 for every other online player. The temporary slot, cached-pad and
random-map selector changes are restored before returning; player ownership,
body creation, native authored starts and checkpoint lookup remain engine-owned.
This replaces the former mission-one placement of all peers beside Cortez and
the primary-pad fallback for extra players. It introduces no position offsets.
Private rejoin tokens7F01..7F03 retain P2 pad metadata and the host's validated
numeric rejoin pose; record/tag/index hooks never index ordinary pad tables with
a private token. Local Story, Arcade and movies use their original paths.

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
