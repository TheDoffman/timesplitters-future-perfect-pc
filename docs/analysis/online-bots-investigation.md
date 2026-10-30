# Issue #25: network bots investigation

Status: an experimental host editor and match-configuration prototype are
implemented locally. Fully synchronized Online/System Link bot gameplay is
**not established**; issue #25 is not complete.

## Test implementation

The host's lobby Options flow now exposes Arcade's original **BOT SET** row
below **WEAPON SET**, including its selector and edit callback. The original
editor retains bot sets, characters, teams, skill and Add/Remove behavior.
The temporary top-of-menu experimental button has been removed.
Host UI construction temporarily uses Arcade setup mode, restoring the real
network mode before input or network processing resumes. No transport flag is
changed. Entering host settings enables the roster adapter for that session.

`src/network/pc_network_bots.h` validates the editor's roster and combines it with the
active human IDs, preserving holes and the total sixteen-character limit.
`src/network/pc_network_bots_guest.inc` applies that configuration only on a PC network
host in the original network setup modes. Disabled, guest, offline, invalid,
empty and full-roster cases leave the original configuration unchanged.
`src/interface/pc_display_menu.c` reuses the host-side Arcade menu construction. The separate
`pc-network-bots-test` patch adds reproducible wrappers; generated C is rebuilt
through the supported workflow.

This prototype does not implement a new AI replication protocol or establish
that the original network paths replicate local AI correctly. Bot movement,
weapons, damage, scoring, respawns and cross-peer agreement must be tested and
any missing replication implemented before treating this as working Online
bot support. See the remaining acceptance work below.

Investigated on Windows against source `33fc619`, branch
`25-add-bot-support-to-online-matches`. Addresses below apply only to the pinned
US title `45410066`, executable SHA-256
`3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc`.
The original executable was read and hash-checked, not modified.

## Verified original paths

- Online enters the original System Link frontend after configuring the bridge:
  `src/interface/pc_display_menu.c::online_activate`, guest `sub_001A0A60`.
- `sub_003533F0` reads the network-active flag at `0x0076B108`.
- English localization IDs `0x36D`, `0x3B1`, `0x3B2`, and `0x3B8` are
  Setup Bots, Add Bot, Remove Bot, and Bot Set. The labels were read from an
  isolated running game's localization tables, rather than inferred from
  unrelated menu functions.
- `sub_002C2C20` constructs Setup Bots. Its child descriptors at `0x0052374C`,
  `0x0052373C`, and `0x0052372C` point to constructors `sub_002C1EA0`,
  `sub_002C2BC0`, and `sub_002C19F0` respectively.
- The editor maintains ten records of seven 32-bit words at
  `0x0075FD90..0x0075FEA8`. `sub_002C0310` populates these from a saved bot set;
  `sub_002C2CA0` and `sub_002C1370` inspect enabled entries.
- `sub_002D2C70` builds match configuration at `0x0078AD58`. Character, team,
  and skill arrays are written at `0x0078AD6C`, `0x0078AD7C`, and `0x0078AD8C`.
  The branch at `0x002D36C2` excludes modes 11, 12, and 13 from the local bot
  assembly loop. Its excluded path explicitly clears `0x0078AD5A` at
  `0x002D383F`. The local path instead fills the arrays and writes its bot count
  at `0x002D382F`.
- `sub_003568C0` checks network player activity using the 16 records rooted at
  `0x00777244`, stride `0x124`, including flags at record offset `0x48`.
- The existing movement sampler `sub_003588D0` iterates a peer's controller
  records. It reads a record index at offset `0x3C`, then addresses player
  state through `0x007B0C48 + index * 0x1584`. The existing
  `pc_network_guest_sample` wrapper retains that controller-oriented path.

These observations do not prove that enabling bot configuration produces
playable or synchronized bots. The initial prototype patched only the builder, which was insufficient:
`sub_002D38C0` clears byte `0x78AD5A` at `0x002D3947` after the builder
returns. This explains the reported empty match. The revised wrappers apply
the host roster BEFORE `sub_00356490` serializes it into `0x774044` and
BEFORE `sub_000658D0` copies it into pending load configuration `0x7DE340`.
Guests consume the host configuration without rebuilding their own roster.
The regression test reproduces the clear and checks bytes captured by the
original serializer, including register/stack preservation. Other original
network paths remain in place.

## Spawn and activity follow-up

The first repaired launch retained six bots in the wire, pending and live
configuration and allocated six actors (`0x7A2D00`), but all six actor positions
remained zero. Allocation is not evidence of visible, active bots.
`sub_003568C0` accepts only active network controller records. The revised
activity wrapper accepts validated AI slot indices only at the actor update
and HUD character-list call sites that already iterate humans plus AI. Controller
initialization (`0x0015C5A3`) and network serializers keep their original
predicate, avoiding null human-state access for a bot slot.
`sub_00358560` already gives host authority to AI indices; the wrapper preserves
that authority check. Replication still requires two-peer gameplay validation.

The original footer draw routine `sub_002C8370` independently excludes network
modes. It now uses the same scoped Arcade Custom UI construction/draw rules
as the Bot Set row. Bot editor callbacks restore the network mode before
returning, including when editing custom saved sets.

## Remaining implementation and acceptance work

1. Trace local bot spawn, AI ownership, and transforms through match loading;
   distinguish AI actor state from the network controller/player state above.
2. Establish how host-owned bot actors can be represented on guests, including
   movement, animation, weapons, shots, damage, death, respawn, teams and scores.
   Reuse original serializers only after verifying their actor/state assumptions.
3. Validate the implemented host editor/configuration hook and establish roster
   distribution with a match generation. Verify human joins, disconnects,
   match changes, and zero-bot behavior against the real game. Guests must not
   control the roster. Pure count/ID/role checks already have unit coverage.
4. Include protocol changes in compatibility identity so older builds cannot
   enter a bot-enabled match. Keep the directory and relay transport independent
   of gameplay AI; the same gameplay implementation should serve LAN and Online.
5. Validate two isolated processes entering the same match: identical roster,
   visible moving/firing bots on both, bidirectional human/bot damage, consistent
   deaths/respawns/scores, a rematch, and cleanup after a disconnect. Then test
   through the Online bridge. Unit tests and a lobby screenshot alone are not
   sufficient acceptance evidence.

Native Mac networking remains a separate deferred adaptation, tracked by
MAC-013 and MAC-033. Native wrappers keep their original behavior.

## Initial investigation scope

Two bounded Test Lab probes used disposable copies under
`private/test-saves/issue25`, with isolated controls and both display settings,
startup videos disabled, and capped logs. Each was terminated after localization
inspection and its known disposable partition images were recycled by the runner.
The existing executable's SHA-256 was
`455166dd4c885123e880e77934df9b58b1ff599973b0e3d40455db113ad9898c`;
its source revision was not independently established, so these are localization
observations, not validation of a build of the current branch. No host/guest bot
match or gameplay validation occurred. Personal saves/settings were unused;
the canonical Test Lab fixture was used only as the runner's copy source.

The subsequent prototype build and in-game editor verification are recorded
under issue #25 in `docs/project/STATUS.md`, including the current executable checksum and
selected menu captures. Neither phase establishes cross-peer AI gameplay.

## Two-peer result after the activity fix

The bounded second run reached one shared, ready System Link lobby and loaded
both peers with identical `2 humans / 6 bots` broadcast, pending and live counts.
After the opening countdown, sampled host AI positions changed across the map.
Guest AI positions stayed `(0, 0, 0)` throughout the remaining gameplay window.
The shared roster and host activity fixes therefore do **not** complete issue
#25. Guest AI state replication must still be implemented; do not describe this
build as working synchronized LAN/Online bot multiplayer. See the follow-up in
`docs/project/STATUS.md` for build identity, test logs and retained UI evidence.

## Follow-up: invisible actors and combat references

The next two-peer reproduction found allocated models on both peers, but their
entity flags lacked bit 0, unlike human bodies. Host AI positions changed while
guest positions remained zero. Bots also lacked both fields supported by the
original actor-reference encoder: the human-state pointer at entity + `0x150`
and the generic network object ID at + `0x230`.

The current uncommitted adapter enables living host AI bodies after their spawn
state and refreshes spatial membership. It adds actor-reference kind 4 with a
validated bot slot, preserving the original human/object encodings. A separate
20 Hz host state channel carries numeric transforms, actor/lifecycle flags,
current/maximum health and score fields. It never transfers guest pointers or model handles.
Guests refresh their own allocated bodies after applying these snapshots.

The original `sub_003593D0` eligibility predicate rejects bots before the
combat encoder is called. Its wrapper therefore accepts exactly the same
validated AI entities as the bot codec. Merely extending the encoder would
leave human attacks on bots rejected. Subsequent disassembly corrected the
initial health interpretation: live health is **entity + `0x1CC`**, armor is
`0x1D0`, the last damage tick is `0x1D4`, and maximum health is `0x1D8`.
Actor `0xB08`/`0xB0C` are health configuration values and cannot establish damage.

Snapshots use the existing version-framed UDP/Online bridge transport. Reception
checks the configured host, session key, match signature, slot bounds, finite
numeric values and increasing ticks before queuing a state. Extension packets
are consumed before the original game's receive-capacity check and decoder.
Leaving gameplay clears pending states; compatibility identity includes the
changed runtime sources. Native Mac retains its original wrappers.

These are implementation details, not acceptance evidence. Record visibility,
combat, respawn and synchronization outcomes separately in `docs/project/STATUS.md`.

## Checkpoint: radar, collision, pose and guest body recreation

The radar actor loop also calls the controller-only activity predicate. Its
specific return address (`0x00133BC9`) now accepts allocated AI slots; controller
enumeration remains unchanged. Reviewed two-peer captures show bot radar markers
on both PCs.

Living bot bodies require flags `0x61`: drawing bit 0 alone leaves the original
hit-collision passes disabled. The adapter now restores all three flags after
spawn and replicates them to the guest, while preserving dead/spawn exclusions.
Snapshots carry actual live vitals, actor flags from `0x24`, body angles and the
animation clip/frame channels. Clip changes use the guest's own animation setter;
no model, pose-buffer or other guest pointer is transferred. Missing guest bot
bodies are recreated through the original Arcade constructor when an active,
living host snapshot arrives, without appending duplicate actor-list entries.

An optional `TSFP_BOT_TRACE` file records up to 256 original damage calls to bot
victims, including live health/armor before and after. Isolated input-state files
also accept `VK down sequence dx dy`; motion is consumed once per new sequence.
These diagnostic paths are opt-in and do not change normal input routing.

The latest bounded LAN run records guest-human shots reducing bot health to zero
on both peers, bot-versus-bot damage, and recreated guest bodies after deaths.
Host-human direct-hit attribution, full animation fidelity and guest weapon
visuals remain unverified or unfinished, as do rematches, disconnects and the
actual Online bridge. See the latest checkpoint in `docs/project/STATUS.md`; this commit is
work in progress, not completion of issue #25.

## Guest animation, firing and score follow-up

The guest now receives both original animation tracks (`model + 0xD4` and
`0x12C`), their numeric playback channels and blend state. It resolves clip
indices locally. The later presentation follow-up below corrects the original
cache/evaluation approach. Actor `0x94` is the equipped weapon enum, not an animation clip. The
body heading at `0x70` and numeric aim/muzzle vectors are replicated too. The
original third-person weapon updater positions/creates guest-local weapon
models after the character pose is evaluated.

Combat events include the victim's spawn counter at entity `0x258`. Replicating
that counter and the host's hit bounds prevents shots from using a stale life
or the guest constructor's default collision shape. Restore these bounds after
pose, weapon and spatial refreshes, which can overwrite them. The original
`sub_0009EAB0` boundary also restores cached host bounds between snapshots,
only for the same guest body and spawn counter. Incoming host damage keeps
the original event path. Guest-origin human shots remain original too.

Score calculation and sorting have controller/peer filters in addition to the
HUD activity checks. Only their AI iterations use the original offline actor
branch; human membership checks remain unchanged. Paused Scores has its own
two activity call sites. Score eligibility uses the persistent actor record,
so a deleted body during respawn does not remove a bot's score row.

The original NPC firing function (`sub_001BB5C0`) has no working network-event
sender. A separate wrapper records successful host calls as a serial number,
spawn/tick, hand/fire mode, weapon definition, target slot, numeric weapon
transform and aim/muzzle coordinates. Snapshots carry the latest event. The
guest resolves its own target and model, consumes each serial once, and drops
events older than 30 guest ticks or belonging to another spawn/weapon. It then
uses the original firing presentation without running AI decisions. Local
bot-sourced damage is rejected on guests; original incoming host damage is
allowed, including when presentation produces delayed effects. No host pointer
or model handle travels over the wire. Cosmetic events can be lost with UDP;
they do not establish damage authority.

With `TSFP_BOT_TRACE` enabled, successful guest replay attempts also write a
bounded companion `.shots` diagnostic (at most 128 entries). A successful call
is not visual acceptance: verify the actual model, flash/tracer, sound and
projectile behavior, and record measured results in `docs/project/STATUS.md`.

## Host-owned bot deaths and victim scores

The previous health snapshots did not establish a single death authority.
Guest-local `BB6C0` applied a hit after sending it, and an incoming damage
event could subtract HP again after a newer snapshot had already incorporated
that hit. A local kill could therefore be undone by the next host snapshot.
There was a second, independent failure: `BABA0` gates completed death on
`35CC10`, whose sender checks ownership of the shooter through `35BE90`.
The host could reach zero NPC HP from a remote human hit but fail that gate,
never awarding the kill or finishing the corpse transition.

The new `pc-network-bot-authority` patch wraps this gate only for configured
NPC victims. The host completes the original death/score logic; guests cannot
complete a speculative NPC death. NPC death uses authoritative snapshots,
without also emitting the stock human death packet. Guest human hits still
use the original reliable `35C870` serializer and its source-ownership checks,
but no longer run local NPC damage. Echoed NPC damage is ignored because its
result is already carried in host vitals. Human victims retain stock handling.

Every bot snapshot includes the host's victim column from the actor kill
matrix, so human kills of bots synchronize as well as the bot's own score row.
The column is applied even when a body is absent. Its confirmed increases can
invoke the original kill-announcement presentation without replaying death or
incrementing scores locally. Duplicate snapshots cannot repeat that notice.
An explicit missing-body snapshot hides the old replica and attached weapons;
a subsequent live snapshot restores its draw/collision/damage-acceptance bits
or recreates a missing body. The replicated body mask now includes bit `0x10`,
which the damage entry checks independently of the hit-collision bits.

This supersedes the earlier guest-damage behavior described above. Validation
must cover actual completed deaths, respawns, and human victim-matrix entries;
HP reaching zero or a kill notification alone is insufficient evidence.

## Guest presentation and confirmed kill HUD follow-up

`sub_000CF440(human_state, victim_actor)` is the original localized **You
killed** HUD path; `sub_00165010` is a separate announcer callback and does not
replace it. Confirmed victim-column increases now use `CF440` for a local human.
Use the persistent actor slot, including when its corpse is already absent.
Keep undelivered confirmations pending while the local view is unavailable,
consume all positive counter increments, and suppress duplicate snapshots.
Health and scoring remain host-owned.

The model follow-up corrects three identified presentation mistakes:

- `sub_00096C20` prepares lighting, not skeleton animation. Remove its call
  from the network pump. The engine allocates fresh bone buffers each frame in
  `A0130`/`A00B0`; third-person weapon positioning now runs after the normal
  `5FDC0` pose pass. Its existing zero-advance mode evaluates guest NPC channels
  without advancing the host's animation a second time. Other actors and native
  fallback keep their original path.
- Clip flags `0x400` and `0x800` apply pending root translation/rotation in
  `5CEF0`. Those movements are already included in the received body transform;
  clear them on both received tracks. Preserve guest visibility bit `0x10` in
  actor `+2C`, which `A0130` sets from that machine's camera.
- Body byte `+32` selects transform mode; it is not a dirty flag. Preserve it
  and invalidate the rotation matrix with body `+28` bit `0x1000`, as consumed
  by `A14B0`, before refreshing spatial sectors.

These are targeted corrections, not visual proof that all intermittent model
disappearances are resolved. Record actual two-peer/capture evidence in
`docs/project/STATUS.md`; software shadow-renderer dumps do not prove the D3D11 window's
contents. Genuine cloak/death rules and normal renderer culling remain intact.

## Completed-match Results and Scores

The post-match UI reads a separate table at `6F0B08`, with `0x68` bytes per
slot, populated by `sub_00063EB0`. Its network loop previously rejected bots
at the controller/peer checks, leaving names, rank, score and character IDs
zero despite the live scoreboard including their actors. At return PC
`63F05` only, ESI is the actor slot: configured AI iterations now take the
original actor-copy path. Human membership filters remain intact. Later in
the same builder ESI is an actor pointer; preserve the network path there,
including the `+A50` score offset copy.

The existing reliable results sender `357B00` includes all 16 slots' kill
matrices/deaths and sorted indices. Receiver `64C60` fills the saved table
and marks `6F118C` before calling the same builder. That flag preserves final
host counters rather than overwriting them with a guest's last live snapshot.
The original-US regression executes this builder for host and guest records,
checks portraits/names/ranks/scores, and verifies packet counters survive.
Disabling the new caller exception reproduces the missing bot records.

The health renderer is also used by the menu renderer at return PC `2E58C9`.
Suppress that draw in completed-match phase `7DE30C == 8` outside Story mode
10. Preserve the emulated return stack, ordinary pause, gameplay HUD calls,
and Story objectives. This rule lives in the shared UI adapter; it requires
no generated-code edits or additional patch. Mac visual validation remains
pending; Windows runtime evidence is recorded in `docs/project/STATUS.md`.

## Combined lobby capacity

The capacity adapter follows US `2BEA00`: map index `78AD4C`, map rows at
`4D2390` (17 rows, stride `3C`), capacity columns starting at `+C`. Frontend
network modes 11 and 13 select columns 2 and 3; mode 12 retains column 0.
The supported stock rows contain 16 for those two network profiles and 14
for the default profile. Read the selected row rather than assuming every
mode supports 16; the physical 16-slot arrays remain a separate upper bound.
`64EA0` is a minimum-player query, not a maximum. Do not infer a game-type
capacity from the unrelated category fields in the game-type table.

Humans have priority. The adapter admits only selected bots that fit and
preserves the original roster so waiting bots return when space opens. It
also writes a zero bot count when full or when the host deselects every bot,
clearing stale counts. Human IDs remain stable: holes below the highest human
slot conservatively reserve space and can leave fewer bot slots than the
visible human population alone suggests.

The existing reliable settings sender `354240` refreshes the admitted roster
in lobby state 11. Packet `2C` already carries the match configuration to
guests, so no additional transport or incompatible use of spare bytes is
needed. Final `356490`/`658D0` hooks recheck after the original builder clears
the count. The stock human-capacity query `35E630` is bounded by the selected
profile, and `2D38C0` refuses a launch whose human slot span exceeds it.
An unknown map capacity fails closed instead of guessing.

Only inside current-lobby draw `197890`, formatter return PC `197707` changes
the population text before its original measurement/alignment. It shows
players, admitted bots and limit; the host additionally sees waiting bots or
an over-capacity message. Guests use the host's transmitted admitted count.
Server-browser strings and offline menus retain their original paths. The
new reproducible `pc-network-bot-capacity` patch installs these wrappers;
native Mac wrappers preserve original behavior pending network adaptation.
