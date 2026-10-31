# Story guest lobby host loss - 2026-10-31

Story chooser rows use the original main menu's centered32200 builder style
for both Local and Online. Selection IDs, callbacks and sizing stay the same.

Chooser reentry also rebuilds the matching active child in its own layout
context after79B70. Opening the captured main alone does not invalidate the
cached Local/Online record. Both Story open and main return use the same bounded
parent/child-descriptor helper; mismatched or absent children remain untouched.

The asynchronous lobby tick1A8F30 calls1CC440, whose host-loss8D notification
uses1AF950 and directly opens network selector4E9974. It bypasses the guest's
Back callback and the chooser Back layout repair. Additive2460 wraps1AF950.
Only frontend101 Online Story notifications8D/port0 from1CC4D9 or1CC552 on
the owned guest lobby4E99D4 (validated parent/child backlink and chooser) use
the main return. Reset guest transport with original1CC360 before closing the
companion, then open the captured main descriptor and rebuild its actual child
with78980 in its own layout context. The chooser Back uses the same helper.
This also covers a cached main record; it does not depend on79860 constructing
a new record. Other notifications/callers, browser, modes and gameplay remain
original. Native live Online acceptance remains MAC-013.

# Story Game Over Quit - 2026-10-31

Original result builder2F5170 installs EAF60 for failed Story Quit (caption17AB).
That callback closes result menus and tail-calls generic65470; it never enters
the Online pause Leave confirmation2EBE60. The existing2390 wrapper now routes
EAF60 through story_leave_begin only in the first-mission joined gameplay scope
with no successful result bit8000. Host Quit retains the party and returns all
peers to the lobby through the existing reliable classB flow; guest Quit uses
the existing full disconnect/main destination. Reuse the local-port0/all-port
menu teardown, frontend factory, profile-save remap and lobby reinitialization.
Successful result Continue, rewards, other modes and later stages stay original.
No new wrapper or transport message is introduced; native acceptance is MAC-013.

# Rejoin camera and first-person model lifetime - 2026-10-31

A rejoining guest may receive an inactive cinematic snapshot without seeing the
host's earlier active-to-inactive edge. The accepted bootstrap marks its local
view pending reconciliation. Once the host is out of the cinematic and the owned
context is active, use existing scoped D4BE0 to restore the normal local camera.
Clear the pending/observed-active state together, so an observed cinematic ending
does not invoke the original end twice. Active host cinematics remain intact.

World-prop identities must exclude first-person models. Original1BE1A9 assigns
synthetic viewport rooms CA..CD (CE for the nonlocal fallback), and1BE1C9 marks
these local models with entity flag40000000. The world-key helper rejects this
combination before consulting its cached key. Otherwise an unnamed host weapon
model and the guest's own model can share a prop identity: host A9070 cleanup on
weapon switch publishes a removal, and the guest destroys its local weapon.
The central exclusion covers both host removal publication and guest key lookup,
including models whose pool slot previously cached a prop key. Ordinary prop
identity, ambiguity rejection, native weapon cleanup and player inventory remain.

# Story in-progress rejoin - 2026-10-31

Native received config2D398D at78AD58 also runs in guest phase12/frontend101.
Accept this exact pending-copy boundary with the existing Story config checks;
the prior phase10/11/15 gates skipped it, leaving stage2 and inactive startup.
For first-mission gameplay joins, defer D50E0 startup until a reliable class1e
StoryRejoin reply supplies the host reload epoch and current spawn pose. Bind
the request/reply to the native active roster peer, config base and nonzero
per-activation nonce; reject other peers, stale replies and invalid coordinates.
Retry requests at most once per60 guest ticks. The temporary synthetic pad uses
the host context0 eye/pad XYZ and heading plus the existing per-slot offset;
original D4F50/2B05A0 still performs floor/collision placement. Clear this override
after startup and on reload. No entity pointers or checkpoint memory cross peers.
Ordinary NPC/loadout/checkpoint identity still includes the reload epoch; normal
restart messages retain strict current-match/next-epoch validation. Checkpoint
captures continue through the existing periodic authored-index replication.
This uses the shared Story adapters; native live acceptance remains MAC-013.

# Story guest departure - 2026-10-31

The retained recording and isolated guest-exit run show the remaining host in
Game Over, followed by a disconnected body after restart. Original35BEF0 removes
a peer's body by BABA0 at return35C199, before its normal15BCC0 character unlink
and3561D0 roster cleanup. BABA0 calls D5480 for a player body; Story's D5480 then
calls D3C50 for both co-op partners. This is a departure, not combat death.
Additive2450 wraps BABA0. Only that exact first-mission remote-human departure,
validated against its context pool/slot/entity backlinks and remote local port,
uses original D2CE0(context,16) to become inactive without death. Original peer,
actor-list, inventory bookkeeping and roster cleanup continue. Real damage,
local bodies, other callers and other modes retain original BABA0.
Restart keeps stable slot IDs and the original slot limit. D50E0 entry rejects
an exact remote context whose roster active bit is clear, before optional tracing.
Some level/checkpoint constructors restore the context without that final spawn
entry. Before Story evaluation, reconcile those contexts to native state16,
unlink only their body with original15BCC0 and clear the same61h entity bits
as native35C274/35C283. Repeated frames leave the retired body untouched.
Payload readiness ignores those departed contexts instead of suppressing the
remaining players' inventory updates. NPCs and active remote players remain
subject to the original guards; no slot compaction or synthetic death is used.

# Story in-match abort routing - 2026-10-30

Retained user video237s shows the guest Leave confirmation, then SELECT LEVEL
at250s. Online pause2F5520 installs2EC060, opening524A80; its2EBE60 action
selects2EBDB0 for row40h (Yes). That action records668F0 before65420. Local
Story's separate2EC090 also records668F0 before65470. Shared lookup scopes
2EBE60 to the first-mission joined gameplay session; Yes closes only local
port0 menus and skips campaign recording. Cancel remains original. Host abort
selects frontend mode13 and original65470: unload65F40 calls356920, phases16/17
send reliable classB via3592C0 and restore host11/guest12. Additive2440 wraps
3592F0 before its own668F0 call, so guest lobby returns also skip awards.
Guest abort selects frontend mode2 and original65420, the full disconnect exit.
The2440 scheduler wrapper selects main for unplanned host loss too. Original
peer cleanup and unload remain responsible for transport/gameplay retirement.
At frontend101's final root child factory, the pending destination is consumed;
root construction/unrelated descriptors are excluded. Main returns run1CC360
while sockets exist and then close this peer's companion. Lobby returns retain
transport and role. Host abort clears original356490's advertised in-game marker
774178 before lobby advertisements resume:352D6D..352D8C otherwise auto-starts
the stale intro config as the guest's active frontend generation differs. Real
GO sets it again. A synchronous story_lobby_return_build scope lets lobby
adapters install their Story callbacks before the16/17 handshake finishes.
Never temporarily overwrite the transport phase here: nested original pumps
can progress it, and restoring an older phase strands the guest. The scope
extends only the Story host/member predicates during this owned factory.
A separate pending-return flag also admits the actual lobby ticks in frontend101
at phases16/17. Before readiness, that tick runs original3544B0 at most twice
to finish the stock16->17->11/12 dispatcher, then clears the flag. Ordinary
frontend pumping otherwise leaves a returning guest stranded17. No arbitrary
phase assignment or new transport messages are used.
At the completed lobby phase, run original3535C0->352680->35A3C0 once before
Ready. This resets the old match clock, pause/preparation bits and network
object caches while preserving connected peers, phase and player records.
The original-US execution fixture verifies that preservation.356BB0 is the
full disconnect and must not be used for a retained lobby.
Abort dispatch uses original2EC650's all-port menu-state teardown, avoiding
campaign recording. Confirmed local Leave closes its pause dialog first; a
remote host-return packet does not close the unpaused guest's menu tree.
Only the scoped guest-exit65470 tail changes to original65420 for a full
local disconnect. Unscoped result scheduling remains original.

Story lobby presentation remains active in frontend101/phase10 for host and
joined guest while the outgoing lobby fades during a new load. Controls and
config ownership remain restricted to lobby phases11/12. A bounded live trace
caught the original Arcade1986A0 formatter looping in its font wrapping path
when Story drawing lookup rejected phase10. Keep the Story header/body adapters
through that transition and skip the Arcade detail callback when transition
bounds collapse; never feed Story data to the legacy Arcade formatter.
The original profile-save identity remap also covers pending abort destinations,
including frontend101: the guest's persistent profile may still occupy network
slot1 before the final destination factory. Original notification10000 otherwise
saves the scratch slot0 checksum view and displays a damaged-profile dialog.
The same identity/aggregate mapping and original save path are reused; no file
checksum is fabricated. Native fallback remains original; live Story is MAC-013.

# Story Online Back destination - 2026-10-30

The retained recording at64.98s shows Story Back at the multiplayer network-type
selector. The shared menu lookup now scopes original browser1A4350, host-lobby
1A8CE0 and guest-lobby1A8EE0 Back callbacks to Online Story, frontend101, and
an owned initialized Story chooser. Original cleanup still runs. A synchronous
scope identifies only that callback's final destination (4E9974,4E9404,4E93F4).
The existing79860 menu-factory wrapper redirects that exact descriptor to
story_storage and restores its stack argument afterward. Before constructing
Story, it calls original1CC360 (the skipped network-type builder's356BB0 reset)
while native sockets still exist. Then the callback closes its companion. Native
socket shutdown alone leaves stale guest handles and breaks recreated discovery.
After Story Back reopens the captured main descriptor, it validates the active
child at story_parent+170h and rebuilds that matching record with original78980.
Bounded diagnostics found matching initial/returned main flags but the layout
context still owned by the outgoing Story chooser and an extra title-height
contribution. Rebuilding in the main record's own context restores the original
layout. Changing flags on the outgoing or completed main record did not fix it;
those attempts and diagnostics are removed. No generated-code or patch changes
are needed.

Unrelated factories, Arcade/System Link, Local, gameplay, invalid chooser storage
and callbacks outside this scope keep their original route. This change covers
frontend Back only; in-match leave/disconnect behavior is a separate notes bug.
Native keeps its existing Local route; native Online Story remains MAC-013.

# Story browser selected level details - 2026-10-30

The user recording's51.77s frame shows Matches Available with Test Lab/Story,
no level name and a placeholder thumbnail. Joined Story lobby assets render.
Live bounded function profiling identifies1CBDA0 as the System Link browser row
renderer, installed by1CC0B0 at1CC117. The similar1972C0 renderer is a separate
route and is not called by this screen; do not use it to repair this browser.

pc_story_browser_match wraps1CBDA0's (menu,item,rectangle) ABI. In frontend101,
Online Story, it validates item+14's advertised index below20, then reads its
CC-byte record at790960: scene+14 and rule+18. Live Story advertisements use
rule2, unlike launch-config rule10. The advertised scene resolves against
one of13 original Story rows at4D1AC0, stride44 (scene+6, name+2, sprite+14).
The1CBFC4 picture callback temporarily replaces2BECE0's bitmap argument with
that Story sprite. Stock rectangle/cropping/fade remain. It draws the localized
name below Story, fitting left of the picture. Registers, argument, font/scale
and any nested draw scope are restored. No local selection or advertisement is
changed. Unknown scenes, missing sprites, other modes and native fallback stay
original. The original browser's64E20 lookup targets Arcade metadata instead.

# Empty guest reward scene entry - 2026-10-30

The user's09706983 screenshot shows host unlocks while the guest renders LOADING.
Both manual profiles persisted completion. Original2D6E10/2D71C3 selects loading
5236DC when this peer's original2CB550 count is zero; it never calls2CBBA0.
The earlier engine empty-page test called the constructor directly, and live17
seeded a new unlock, so neither covered this scene-entry branch. Host-first live18
still displayed the guest's seeded unlock, rejecting movie skip order as the
explanation of this particular path.

Additive2430 wraps79860's descriptor argument, changing5236DC to523900 only for
joined first-mission guest rewards106/stage3 with zero actual unlocks. The normal
reward constructor then builds the existing disabled Waiting for host panel.
The argument is restored after original factory execution. No unlock is invented,
no host award is copied, and save/transport scheduling remains original. Host,
offline, non-reward scenes, other descriptors and native fallback stay original.
Old-build live20 reproduces LOADING; rebuilt live21 shows waiting and follows
the host into the next intro with signed completion saved. Both use disposable
unmodified unlocked fixtures and synthetic completion, not a mission playthrough.

# Guest rewards before the host wait - 2026-10-30

Original2D2C70 returns early for network guests, but its host path also skips
2CB210: the network predicate at2D2CA3 branches past that snapshot at2D2CB4.
Guest-only snapshotting was a regression: matching unlocked profiles then showed
host rewards but an empty guest list. pc_story_network_frame invokes
story_rewards_begin once real first-mission guest contexts/profile exist;
original69A60(1) selects that peer's port0 profile while preserving the original
Online baseline. It must not call2CB210 just for guests. Guest completion66FC0
remains intentionally suppressed. Gameplay activation/restart reset tracking.
No host profile or reward list crosses the wire.

After validating the reliable host's gameplay-to-ending config, prepare calls
story_rewards_record before replacing the pending stage or retiring contexts.
Only mission0/stage1/scene2 to authored ending stage2 qualifies. Original668F0
records co-op completion and marks local profiles dirty under a temporary success
bit, then original69A60 evaluates this peer's own profile. Registers/result bits
are restored; no result menus, transport teardown or guest scheduler run. Later
copies see stage2 and do not record twice; unrelated configs/roles stay original.

Guest unlock constructor2CBBA0 now runs unchanged while cursor75FD88 is below
original2CB550's reward count. Patch2420 wraps2CBAF0, covering both menu lookup
and original2CBB60's direct timer tail call. Non-final pages remain original;
the final guest page increments the cursor and rebuilds the same descriptor as
Waiting for host instead of exiting through5236DC. The refresh forwards the
original callback stack/menu argument into78980, including the timer tail call;
a fresh zero-argument guest_call would corrupt this ABI. Waiting holds confirmation
and timer. No rewards go directly to waiting; host/offline pages stay original.
Live validation exposed a separate save-view mismatch: network construction
moves the guest's Test Lab metadata/body to slot1, leaving a scratch profile
with cached CRC0 in slot0. Original1922D0 enumerates persistent profile0 and
reports error7 even without test seeding; the real metadata retains the correct
disk CRC in slot1. Patch2425 scopes that synchronous original save to persistent
profile-ID order. The Online selection captures persistent metadata identities
before opening the browser: joining already moves their bodies, before Ready;
metadata pool order must not be assumed to identify the selected profile. The
save scope finds those unique captured records among the16 active rows,
snapshots the network table, maps loaded bodies and their aggregate view,
then restores the table with saved updates copied back to their network slots.
Original file/checksum validation, signed writes, dirty state and retry/error
menus remain intact. Missing/ambiguous mappings, invalid counts or allocation
failure retain the original route. Host/offline/native fallback is unchanged.
The original-engine fixture verifies guest profile slot1, untouched remote data,
dirty/completion counters, old/new unlock comparison, multi-page display, timer
and final wait. Native Local/Arcade fallbacks remain original; native Story
acceptance remains queued under MAC-013.

# Late gameplay scores during Story movies - 2026-10-30

Natural-ending retest on8dbdad1b failed in60740 at movie frame93, preceded by
an asynchronous stream read into0003FFFF instead of its live buffer. Original
64810 initializes7DE4C0 rankings for790950+7A2958 actors although its table has
only16 entries. At slot28 it overwrites7DE530 withFFFFFFFF. Story NPC counts
persist into movies: bounded early-completion sampling retained5; a normal
playthrough can have many more. A controlled before-fix live test with30 NPCs
reproduced the buffer overwrite and froze ending frame27. The translated
original3578A0/64810/646B0 fixture reproduces the exact overflow with30 NPCs and an empty score snapshot.

Additive patch2415 wraps incoming stock3578A0 with pc_network_guest_scores in
pc_network_guest.inc. Active desktop-network movie flags or frontend101..107
hold only this gameplay decoder, before its actor writes, clock update and
ranking rebuild. Reliable configuration, ready and control traffic remains in
the original network pump. Ordinary gameplay, custom-map100, offline/disabled
transport and builds without desktop networking retain the original decoder.
The guard is shared with native networking; no OS adapter or clock change.
NPC counts and cinematic buffers are preserved rather than clamped or repaired
following corruption. Fixed live playback with30 seeded guest NPC slots reached
frame1059 without buffer corruption, then both peers reached the next intro.
The user confirmed the natural ending crash fixed after this bounded test.
Native Mac live acceptance remains pending.

# First-mission rewards and movie ownership - 2026-10-30

The original user capture records2->44->106, then a wrong first intro with a
sixteen-player header and63390 filename-trim failure. The initial candidate's
2EB... result scopes did not cover the observed unlock-page route. A bounded
synthetic scheduler reproduction confirmed the same fault on both peers with
ff61ff5b. The decisive trace is rewards stage3, scene106, original frontend GO
serializer caller2D3980/address78AD58/phase15, with16 humans and old scene24.
Original unlock builder2CBBA0 uses page callback2CBAF0 and timer2CBB60; its
pages return through the frontend, rather than the in-game result callbacks.

pc_story_network_prepare recognizes that exact host first-mission rewards GO
boundary. It gives original65940 completed mission0/stage3/difficulty, retains
the last validated Story party header, then infers the new authored mission.
The existing GO sender owns the generation increment and broadcast: this scope
must not send or increment again. Original2D398D pending copy and the guest's
normal received-config path remain unchanged. The original-engine stage fixture
covers authored next-mission selection; production tests reproduce16/scene24
at the actual GO boundary and check one rebuild, retained party/difficulty,
no duplicate generation/send, pending-copy reuse and guest follow.

For a joined first-mission guest on rewards106/stage3, original unlock pages
precede the Mission complete/Waiting for host panel described above. The waiting
panel has a disabled row and no confirm/back callback. Only exhausted guest pages
are held; host, offline and unrelated menu callbacks retain the original route.
Ready357540 uses zero bodies and a temporary eligible scene for rewards only,
then restores actual scene/flags/count;64CE0 otherwise rejects frontend106.

Original99BB2 invokes65C70 and then60310 even when Story guest advance is
suppressed.60310 closes the stream and clears6D782C/6D7830/7DE530 while C95E0
can still render that scene. Shared patch2410 retains the stream only at99BBC
for a joined guest, phase10/14/15, state7 and movie flags20000000. Real pending
load state12, host, offline and other callers keep original cleanup. The engine
fixture checks cached-frame coordinates and subsequent file close. This proven
lifetime gap is separate from the captured wrong-config63390 failure.

The corrected next intro exposed a separate stock358EB0 fault on both peers:
its3544B0 caller copies context+1500 inventory data in movies after those player
records have been retired. The existing pc_network_guest_tick cadence adapter
had been opening that entire gameplay-send gate, which also contains358F40.
For phase15 frontend scenes101..107 or movie flags20000000 it now pins the last
send epoch76B0EC to the unchanged network epoch76B0F0, rather than epoch-1.
Original3544B0 still executes its transport, heartbeat and timeout work. Once
gameplay returns the adapter immediately reopens the movement gate; duplicate
pumps and the original60Hz clock keep their existing semantics. This shared
Windows/native-network policy also protects ordinary Online transitions; it
neither edits the roster nor suppresses the whole network pump. The dispatch
fixture verifies held gameplay sends, retained original/control calls and
immediate recovery; native cadence/mutation fixtures retain clock-unit checks.

Opt-in Story traces record bounded reward callback addresses and stage/advance/
result/config boundaries. No asset-loader sentinel suppression, clock change or
second-mission NPC adaptation is introduced. Native Mac Story acceptance remains
unverified; existing Windows entry guards and native Local/Arcade paths remain.

# Main integration - 2026-10-30

The Story series now lives in src/network with numbered patches. Main's native
network guards and the existing Story Windows scope are retained. Story menu
callbacks use verified US INT3 bytes 123C9..123CC; Unlock Content retains
1237B..1237E. The original-entry byte contract and settings fixture cover the
collision repair. Original camouflage fixtures seed SIMD constants emitted by
main's translator; their host/guest opacity and material assertions are unchanged.

# Subsystem contracts and diagnostics

Splitter camouflage (2026-10-28): repeat capture has host kind78 slot7/8
actor+C6C=01000010, phase+C88=1 and numeric fade/cycle state through+CB8; the
guest has zero flags/state. Existing cloak200000/phaseDE4 is a different effect
and stayed zero on both peers. Original11F780 reads bit10 and phase20/time24/
duration2C relative to actor+C68: recorded host opacity0 versus guest opacity1.
Original11F7E0 advances fade/flicker and uses11D510/11D400 to select local body/
weapon models; guest NPC authority skips the15EAE0 effect dispatcher.

Shared pc_character_effects.h carries56 numeric bytes (flags01000010 plus
C88..CB8), excluding actor/model pointers, sounds and other effect ownership.
Inactive snapshots canonicalize to zero. Validators reject unrelated flags,
phases above2, nonfinite/unbounded values and zero fading denominators. Story
SNP5/SPP5 assembles452 bytes from a236-byte physical prefix and216-byte pose;
physical/checksum/stock header totals244 bytes, within the stock256-byte limit.
Checksums, host/match/sequence checks and atomic assembly remain. Existing room,
power-up cloak and corpse/drop contracts are retained.

Online bot snapshots use the same numeric state and validation. Their existing
length and outer compatibility checks reject prior layouts. Only elapsed fade
time interpolates within the same phase/duration and monotonically increasing
timer; discrete transitions and random shimmer values stay atomic.

After frozen skeleton/weapon preparation, pc_character_camo_present runs only
original11F7E0 for validated guest NPCs. It temporarily disables independent
cycle bit01000000, then restores every host numeric field/flag and guest registers.
The original material selector keeps all local body/weapon ownership. Cosmetic
shimmer RNG remains local; no AI, health, Story event or global clock is invoked
or changed. Ending camouflage restores full-opacity material127 once for the
same body generation; unrelated inactive actors are untouched. No new runtime
patch/wrapper is required. The user confirmed first-mission camouflage fixed;
native Online validation remains separate.

First-mission barrel callback218650 (recorded mesh1341/script50435833) calls
EFBD0 at2186FD/218722 with profiles47/40 and the shooter's weapon/source.
The missing-effects follow-up corrected the earlier interpretation of their
empty ordinary particle-table rows: EDD40's original jump table sends both
profiles to EF444, creating four dedicated emitters (7B0120,736828,7B06C0,7B0800)
with their authored scaling. Replacing those profiles with1 loses that branch,
layered sound and the original25-unit camera-feedback range (profile1 uses5).

StoryEffect kind4 now preserves a=1/40/47 and the finite direction vector;
decal_flags carries optional direction presence (0/1) for this kind only.
The wire still carries b=23 as a tag, not a guest weapon/owner handle. Guest
playback calls originalEFBD0 with that profile, locally copied position/direction,
weaponUINT_MAX and null source/attacker. No substitute fireball is added.
EDD40 sets effect+C bits20001 before startup for all three approved profiles;
EDBE0 Story events, EFF90 damage and186480 navigation mutation stay suppressed.
Original particle/audio/camera dispatch remains. Reliable host/match validation,
bounded replay and duplicate rejection apply to both authored profile events.

The prior host ownership repair remains: EFF90 scopes the effect's original10C
source;35AF40 accepts that same remote human only during this first-mission host
barrel update. Original serializer/broadcast/NPC health execute, and nested scope
restores afterward. Other shots, ordinary Online, guest damage and other scenes
retain existing ownership. No new runtime wrapper or OS adapter is required.
This follow-up uses the retained20261030 recording, and the user confirmed the restored effects in gameplay. Native Online Story
remains MAC-013.

The 2026-10-28 visibility-boundary capture showed living posed kind75 actors
with host primary room5 but guest primary27, despite local overlap list[5,27].
Original9FBE0 keeps the old primary while bounds still overlap it. OriginalA1EA0
builds the render-candidate lists from primary34 only, unlike9E4B0's collision
lists from all overlaps. The invisible actor therefore never reaches A2430 when
only room5 is rendered; forcing masks or its scissor cannot repair that omission.

Story SNP4/SPP4 adds numeric primary_room (396-byte assembled state,180-byte
physical prefix); each stock fragment remains below256 bytes. Online bot state
also carries the numeric room at the end of its existing fields. Receive size,
state validation and source compatibility reject older layouts. After local pose,
bounds and9FBE0 refresh, shared pc_network_bots_entity_room selects that room only
if it is a real loaded scene room and occurs in the local bounded10-room overlap
list. It invalidates the room-plane cache on change, retains overlap membership
and leaves portal rectangles/frustum/depth tests to the original engine. This
also respects interpolated Online bot geometry: a future room outside current
local overlaps is not adopted. No pointers/camera visibility cross the wire.
The original render-list fixture reproduces room27 omission, then checks repeated
room5 selection with the production helper; disabling the helper fails it.
The user confirmed the corpse, weapon-drop/pickup and visibility-boundary fixes
on Windows in the first mission. Native Mac Online acceptance remains separate.

The corpse-only demonstration captured four valid death poses (clips73/74),
then matching actor retirement on host and guest. `story_npc_apply` previously
removed the guest's type8 body directly when the host cleared its actor link.
The original dead-state path27F160 also uses B3630 to copy a posed character
into the independent type1000 cosmetic corpse ring. Guests skip that AI path.
`pc_character_corpses.inc` now copies a dead, current-generation replica before
retirement, after removing owned held models. It leaves the original ring's
capacity, recycling and scene cleanup intact; it retains no live NPC/AI actor.
Living replacements, stale matches/generations, removed bodies and invalid poses
are excluded. Duplicate retirement snapshots cannot make another corpse.

B3630's descriptor+58 read needs a local resource even for reconstructed guests.
When none exists the adapter uses a bounded zeroed guest-stack descriptor above
the constructor's stack. It temporarily clears the source's life/callback for
its final3EF event, then restores descriptor/life/callback/ESP. Original15C310,
27CBD0,27CEF0,27CB10 fixtures verify no second Story AI transition is dispatched.
Existing authored corpse flags are preserved. All models/bones remain peer-local;
no new wire message, actor-pointer transfer or generated wrapper is introduced.
The user confirmed the first-mission corpse fix on Windows on2026-10-28;
native Mac scene acceptance remains separate.

The user-guided 2026-10-28 observation found guest-only dormant bodies (health1,
flags78, outside the roster) blocking the route while no corresponding host body
was active. On guests, unlisted/unactivated nonhuman placeholders outside the
current replica generation now lose the low collision/hit bits70. Their scene
objects/resources remain available; a host snapshot restores authoritative flags.
Host actors, humans, active replicas and removed/reused bodies are excluded.
Original73380 at733E4 skips a world body's collision contribution without bit40;
the pinned-US contract checks that gate alongside the live placeholder evidence.

The ally boundary capture retained live replica masks while model regions were
omitted. The original movement path A7360 invalidates entity+2B4 bit31 as well
as transform rotation state. Network refresh previously updated room membership
without dirtying this cache; originalA1AB0 trusted stale room-dependent plane
modes. The actual A17C0/A1AB0 regression reproduces this before the correction.
Shared NPC refresh and the scoped Story A2430 adapter now dirty the cache;
original portal rectangles, real rooms, frustum and depth remain authoritative.
This replaces the ineffective primary-room0/full-viewport workaround. Later-room
visual acceptance remains separate from the original-code regression.

The weapon-drop demonstration recorded seven completed host pickups and zero
new guest pickups across matching deaths. Original C2090's Story physics path
returns atC2A34 before the stock3581B0 send atC2D22. Its remaining stock decoder
also rebuilds through a temporary actor with no authored Story descriptor; the
prior inventory replay workaround alone did not solve these captured drops.

`pc_story_drops.inc` now snapshots the completed, original30-record/D8-stride
pickup pool at7B0D20, limited to dropped weapon items whose source is an NPC.
The first mission sends bounded numeric states (magic31504453,72bytes) on the
existing1E channel. Creation/removal, ID/quantity changes and one-second recovery
are reliable; changed throw/landing transforms publish at most every6ticks.
The guest uses original C1950 with the Story placement descriptor4DB9A8, exact
host quantities and scoped541EA4 host runtime ID. It follows host transforms
without replaying local throw physics, and uses that ID for original pickup
requests. No actor, resource, physics or model pointer crosses the wire.

Per-slot serial/match checks reject stale/duplicate/foreign updates; failed
construction retries. Local entity-ID ownership protects reused storage during
replacement/removal. First-mission NPC stock0x16 replays are skipped to avoid
a delayed duplicate; human and other-mode paths retain their existing behavior.
Original constructor/ID allocator fixtures reproduce weapon25/ammo18qty126 from
the capture, and state fixtures cover movement, recovery, removal and retries.
`PC-STORY-DROP` tracing is opt-in/bounded. Actual pickup visibility, collection
and later boundary acceptance on the new build remain separate user retests.

All remote opening bodies now use private pad tokens7F01..7F03 based on primary
pad1F40, at X offsets+2/-2/+4. The old player2 pad1F62 was ahead of the hanging
body despite sharing its camera. Private record storage is96bytes; pad/tag/index
helpers agree, and checkpoints retain their original selection. Shared Online
ammo pickups preserve a positive reserve when loaded clips exceed the computed
cap; accepting a pickup must never drain existing ammo. Negative reserves can
still recover through positive pickups. Other writers require separate evidence.

The revised six-bug investigation found guest NPCs losing all local presentation
view bits while the host retained eligibility. `story_npc_visibility_prepare`
sets current local view bits for validated posed replicas before originalA0130;
the original pass still allocates bones/weapon entries and the renderer retains
depth/frustum clipping. This follows the remote-human eligibility policy rather
than relying on a guest AI/host-ownership path. It can process up to32 actors;
it preserves host authority, generation checks and unrelated culling flags.

Original15DDA0 callsDAAE0 beforeA9070. DAAE0 destroys held models and can spawn
pickups; directly calling A9070 for a guest replica skipped the held-model cleanup.
Replica retirement now removes only hand-owned attachments before the body,
validating attachment7C -> hand and handEC -> actor, without generating pickups.
The same cleanup runs before reconstructing a locally retired replica, including
when its body link is already cleared; reused bodies and unrelated weapons remain
protected by the recorded replica and attachment ownership checks.
Opening suppression clears both full temporary culling words while awaiting
the authored pistol handoff. Mode3 can remain selected during normal gameplay;
it must not extend opening pose replay or body hiding beyond that handoff. The
mode-based extension caused remote humans to slide until the next cinematic.
Model/bone storage remains local; actual later cues retain their own lifetime.

Original1BE870 at1BEED9..1BEF03 omits held models for definitions41/56/61. Shared
`pc_network_bot_shot_needs_model` admits their numeric fire replay withoutE8,
while ordinary guns still wait for a model and stale weapon events are rejected.
Both Story's queue and the shared Story/Arcade presenter use this rule. The new
production-presenter fixture checks real dispatch, readiness and owner restoration;
rendered Splitter effects still need a later-scene acceptance check.

Story corpse lifetime follows host `present` and spawn generation, independently
of `in_roster`. Original15BCC0 only removes the actor from7A3580/count74C37C;
it does not destroy the world entity. A present dead replica keeps its pose and
bounds until host absence or replacement. Snapshot bounds must be installed
before9FBE0: its room query uses entity position+1A8 +/-1C0. Refreshing first
queries old geometry, especially on construction and death-pose changes.

Opening attachment validation follows original1BE870: attachment+7C points to
actor+A4+hand*130, and that hand record+EC points to the actor. Requiring an
actor backlink directly at attachment+7C skipped genuine held weapons. Both
ownership links are checked before filtering the existing opening draw list.

Opening visibility also removes the validated human bodies/world weapons from
the linked visible-world list (7B4CC0, count7B54D8) already populated by original
9FBA0 inside A0130. Clearing masks after that enqueue left the standing model
queued. The bounded filter validates the list before rewriting it, preserves
unrelated entries and local model storage, and lets the next original visibility
pass restore entries after the opening. The fixture checks list/count restoration.

Stock35D150/35AD90 and68830 copy human teams directly from the peer cache at
777288+slot*124 and the active config team bytes. Normalize those before body
construction and keep them on team1 even when the actor already has team1;
fixing D7550 alone does not correct the source of subsequent network snapshots.

A four-player follow-up caught a completed human body on team3 at the opening.
The D7570 adapter had waited for entity/context backlinks that its caller had not
yet attached; the setter also covered only one caller. All validated Story human
D7550 calls now use team1, and D7570 immediately corrects the actor's team data
before attachment. NPCs, Local Story and Arcade retain original team rules.

Story preview1 follow-up: `pc_story_npc.inc` assembles eight independent serials
per NPC, retaining the newest complete physical/pose pair during interleaved UDP
delivery. SNP4 includes numeric cloak phase/fades; handles remain local. A loaded
kind/mesh may construct a guest NPC without a prior local instance, and only a
previously recorded stale replica is detached before reconstruction.
`pc_story_dialogue.inc` reliably sends line IDs/flags and body generations; guests
create positional audio and lip tracks through164C40 and advance the cosmetic
1CB260 audio clock without running host AI. Opening visibility also clears the
attached world-weapon masks. Cinematic v3 shares the separate inset camera's
authored tag, target tag and lifetime through peer-local CC170/C9C90.

`pc_story_world.inc` uses authored type/kind/tag (or initial quantized position)
for persistent prop identities; codec5 carries only that key. Duplicate local
identities resolve to nothing. Reliable host A9070 removal invokes local cleanup,
including collision; guest damage requests retain the original host hit protocol.
Effects v2 additionally sends only known blood texture indices957..963 and numeric
decal arguments;1128C0 raycasts/creates the decal locally without health changes.
Guest-owned local blood remains immediate even when host cosmetics are offscreen.
A32-entry local history skips matching nearby host decals of the same projection
mode for30 ticks, preventing doubled feedback while leaving other impacts/pools.
Game Over selects the host's primary menu context, hides guest restart buttons,
and bypasses stock network teardown for coordinated host reloads. Guest reward
Continue callbacks cannot independently advance/close the menu. Shared Online
ammo pickups clamp before narrowing signed16 reserves, including overfull clips.
Disconnect/leave behavior remains separate. Native fallback calls the originals;
Online transport support still depends on MAC-013.

See [systems-details](systems-details.md) for detailed adapter contracts and validation routes.

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
`src/network/pc_character_pose.h`. Bot snapshots retain their existing binary layout;
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

Story host controls live in `src/network/pc_story_lobby.inc`, dispatched by
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

First-mission presentation repairs (2026-10-26): `pc_story_cinematic.inc`
replicates a validated numeric fade record and primary human pose during the
opening before the host pistol grant, or an authored script camera. Context
+14F8 is a first-person weapon animation selector, not a cinematic detector.
SCY2/SPY2 fragments share a checksum, epoch and sequence; guests assemble both
before applying a pose. D4B00/D4BE0 wrappers capture host camera actions by the
loaded camera tag (C6100 lookup), never pointers or script callbacks. Replay
scopes original local-Story camera loops to the owned context and restores all
pool/current globals and CPU state. Native/non-Story fallback remains original.
Runtime screenshots and scope are recorded in STATUS; source tests alone do not
establish the reported persistent post-sniper black screen is resolved.

`pc_story_effects.inc` makes the first mission's216230/219CA0 random decisions
host-only. The separate world-effects patch captures actual FFDA0 ground and
EFBD0 explosion scalar arguments; SFW1 carries vectors/color, not handles.
Guests replay original local creation APIs and update a locally owned projectile
trail from host flight snapshots. A bounded replay window preserves a delayed
one-shot event even after a newer flight sample. Other missions remain original.
Match/host/finite/sequence guards reject invalid or obsolete data. Exact event
serial/position trace comparison is distinct from visual timing review.

The reproduced opening black screen had matching host/guest camera transforms
but guest camera+21C bit0 clear. Original CB110 implements Local Story's shared
single-view policy by disabling slot1; that is inappropriate for separate Online
views. Cinematic snapshots carry the host's numeric world-render enable bit;
guests apply only that bit to their own camera, retaining unrelated flags and
local camera handles. Source tests explicitly reproduce the disabled guest view.

### Story follow-up: firing, checkpoint inventory and completion

`pc_story_shots.inc` observes successful `1BB5C0` firing, carrying numeric weapon
transforms/aim/muzzle and actor slot/spawn. Humans publish only from their owning
peer; the host validates the sender's assigned slot before relaying. Guests accept
only host events, deduplicate serials, resolve local bodies and replay cosmetically.
Remote actor/projectile damage is suppressed outside original incoming host damage.
The shared bot presenter now accepts the codec's `UINT32_MAX` targetless shots.
Story NPC hit identity reuses actor codec4 across32 slots; original reliable damage
and host-only NPC death remain authoritative, including guest lethal hits.

Continuous Story pose packets are unreliable; spawn/death/roster changes and
60-tick recovery remain reliable. Roster removal does not retire a host-present
corpse. Host absence or a changed body generation retires the guest body and its
validated held attachments. Checkpoint inventory is reapplied through original `25EC70` only after
all final network contexts reach state2; weapon selection follows inventory copy.

Lobby title/status use measured bounds and preserve thumbnail space; difficulty
uses original localized labels/style. `pc_story_network_stage_builder` adapts
original `65940` at completion/ending boundaries: ending movie, rewards106, then
host continuation. `pc_network_player_payload` guards `35BD70`/`35DB60` while
joined human contexts/bodies are absent in frontend/movie transitions, preserving
control/config transport. The matching12ce Windows dump resolves to `35B5D0`
after scene101. First-mission completion and visual parity still require gameplay
validation; source fixtures alone do not establish a completed mission.

The opening handoff uses SLD2: award and actual host equip are distinct. A guest
waits for the original host actor's equipped weapon before calling its inventory
APIs. `A0130` world visibility is wrapped after its original culling/mask build;
only human main-world-entity low16 view bits are hidden during the opening, until
pistol equip or a captured checkpoint. Weapon/arm model pointers and animation
remain local. Original visibility construction restores ordinary masks afterward.

Difficulty row argument5 selects; argument6 draws. Synthetic BODY dispatch on the
custom difficulty descriptor calls original `2CAD80` with a temporary selected
mission/co-op profile scope, restoring globals afterward. Never route this trophy
drawer to the selection callback. Header title/status form a measured vertical
block as well as fitting available width. Story shot replay waits for the local
weapon model/definition to be ready, with a bounded120-tick expiry.

The custom difficulty builder calls original `71BE0(1,9.0,0)` before creating
rows, reserving the original trophy gutter to keep icons clear of label text.

## Online bot corpses

Online guests use the same original B3630 cosmetic corpse copy as Story NPCs.
The common `pc_character_corpses.inc` owns pose/health/ownership validation,
local descriptor fallback and callback suppression; the Story call site retains
its match/generation guards. `pc_network_bots_preserve_corpse` additionally
restricts Online calls to the current guest bot actor and matching spawn.
The shared guest frame copies a dead replica before an absent/new-spawn snapshot
replaces it, or after applying a final dead pose whose drawing bit is cleared.
Per-slot copy markers reset with match/pool/host and body generation, preventing
cached snapshots or repeated tombstones from making duplicate corpses. Living
replacements are excluded. The original bounded corpse ring owns recycling and
scene cleanup; health, scores, AI and human player death remain host/stock-owned.
Both Windows and TSFP_NATIVE_NETWORK compile the same policy. Physical Mac and
Online scene acceptance remain separate from deterministic fixtures.

Online corpse follow-up: the isolated paired probe reproduced bot retirement
before the snapshot hook, and respawn reused the same entity/spawn counter.
The snapshot-only copy was therefore too late. The existing A9070 world-removal
adapter now calls the common corpse bridge before original destruction on both
network hosts. The bridge owns entity/spawn deduplication scoped by actor pool,
match and host, so removal hooks and snapshot fallbacks cannot double-copy.
Human, Story NPC and cosmetic-ring objects fail the bot ownership guard.

## NPC facing

US body C0/C4/C8 and heading70 use degrees (360 at475C88, pi/180 at475D0C).
`pc_network_bot_motion_angle` now wraps at360, preserving the shortest turn at
0/360. A radians wrap on these values caused large heading errors while each
per-tick snapshot retargeted a50ms interpolation. Position interpolation,
atomic animation/lifecycle handling and the existing pose protocol remain.
The isolated paired probe compares heading/body yaw within matching live
slot/spawn generations; these asynchronous samples measure state agreement,
not reviewed render output. Head/torso overlay cache ownership remains local.

## Four-player rewards context

Scene106 creates one frontend context even with four joined peers. Original
D2FD0 assigns the port/camera via356800; its retained network slot writes past
that context on guests. Additive2465 routes356800 through the shared
pc_story_network_rewards_context_slot adapter. Only joined first-mission guest
rewards, one allocated context, zero scene flags and port0 map to context0.
Profile mapper356870 remains original, so awards/save ownership retain each
peer's own network slot. Gameplay, movies, other ports and native fallback
retain original mapping. The original-engine reward fixture checks slots1..3
and rejects a disabled-context-mapping mutation; live four-peer validation is
recorded in STATUS.md. No packet, profile or scheduler change is involved.
