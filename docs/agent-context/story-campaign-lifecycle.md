# Shared Online Story campaign lifecycle

The user confirmed on 2026-11-09 that Time to Split is playable from start to
finish with two, three and four players. This is human acceptance of mission
one, not agent verification of the remaining campaign.

`pc_story_load.inc` includes `pc_story_campaign.inc` before the shared adapters.
It owns campaign eligibility and the original US mission-table scene lookup:
13 records at4D1AC0/stride44h, gameplay scene at+6, intro/outro at+20h/+24h.
The lifecycle stays within typed Online Story, enabled networking, gameplay
stage1, original gameplay flags80008, and active phase15. Checkpoints, restart,
cameras, objectives, dialogue, world objects, effects and drops use that common
scope. Intro/outro movies and rewards retain their separate transition paths.
Arcade, System Link and Local Story retain their original paths.

## Loading, completion and ownership

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

Extra slots use the mission's original primary-start selector only when its
authored start list is exhausted. Peer-local private pad IDs7F01..7F03 carry
distinct numeric starts, with matching record/tag/index hooks. They never index
ordinary pad tables directly. Rejoin uses the host's validated numeric pose.
The first mission's hanging-start pad8000 remains restricted to its opening;
checkpoint starts use the original selector instead.

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

`Test-StoryCoop.ps1 -Players 2|3|4` opens isolated manual games. The bounded runner
accepts three players too. Use `tests/network/story_pair_game.py --load-probe
--lobby-controls --mission 1 --difficulty 0 --players 3 --fire-probe --restart-probe --skip-intro`
for Scotland the Brave loading, owned movement/fire and coordinated level reload.
It waits for actual gameplay after later intros, compares the selected scene and
each player's original weapon/ammo on restart. The authored checkpoint walking
route remains specific to mission one; do not relabel it as campaign coverage.

The shared helpers require no new OS adapter. Existing native Story transport
remains unsupported (MAC-013); Windows fixtures do not establish Mac/Linux live
Story acceptance. Keep that capability gap distinct from shared policy coverage.
