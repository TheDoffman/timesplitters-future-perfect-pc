# Arcade join in progress

Development branch: `feature/arcade-join-in-progress`, based on the unmerged
client/server split at `e97ee97a`. Online Arcade enables the shared join adapter
by default; `TSFP_ARCADE_JOIN_IN_PROGRESS=0` disables it for rollback. The installed
mini service now carries the private admission update. Keep all service policy,
deployment sources and server tests private as required by
[the client/server boundary](online-client-boundary.md).

The existing title receiver can load the current round for a new peer. Its
current host-side configuration uses the initial human population. Increasing
that count alone is unsafe: original `D3B00` allocates context, control and weapon
arrays using that count. A late player would access uninitialized storage.

`3020-arcade-join-contexts.patch` wraps that original allocation. The adapter in
`pc_arcade_join_guest.inc` reserves sixteen entries in the original three human
storage arrays. Its `D2FD0` wrapper restores actual population counts before the
original player constructor runs; no hidden player bodies are created up front.
It is restricted to online Arcade mode. Local actor
pointers never enter a checkpoint or the service. Native Windows Deathmatch
bot replacement and empty-slot admission pass. Native Mac late-join Team
Deathmatch also passes with the bot's team and an initial personal score of zero.

The function emitter supplies an audited alternate entry at `D3143` and an early
tail at `D3AD5` inside original `D2FD0`. A requested remote context uses the
original per-player constructor with its original stack locals and callee-saved
registers. It skips the bulk clearing and global input-reset tail. Ordinary entry
executes the full original function. The adapter reserves each context's local
auxiliary storage and supplies the loop frame; no generated C is hand edited.
Bot conversion runs original `15DDA0` retirement under the old population to
release navigation reservations, inventory and mode-object ownership. It retains
the registered actor row and compacts the original bot body table without moving
remaining actor IDs. Single-player construction preserves the running context
and camera globals. Only actual bot slots run retirement: unused actor rows can
contain nonzero sentinels. `15BCA0` registers an absent replacement actor once,
retaining human-before-bot registry order without moving actor IDs.
The compiled fixture checks the stack, registry, bot table, score and team rules.
An earlier eager-body prototype stalled in initial bot pathfinding; the lazy
replacement passes the three-player Windows Deathmatch probe. Tests alone are
not gameplay proof for other admission cases.

Unused saved character choices can name meshes absent from the running world.
Resource indices alone do not prove loading: original `A6880` consumes the
resource entry's actual model pointer at `+4`, which must be present/readable.
At initial allocation, unused slots with unloaded meshes adopt the first human's
loaded character/variant in all four round configurations, before a late peer
receives them. Already-loaded choices and live bots remain unchanged; no profile
is written. Empty-slot newcomers may therefore share an existing player's avatar
for that round. The compiled fixture checks both replacement and preservation.

Owner tracking uses authenticated virtual IP identity as well as native records:
record relocation during migration retains scores; record reuse by a different
member resets the personal row. Only initial human-prefix owners start cached.
The host retries original targeted `357B80` GO for new initialized contexts at
half-second intervals (maximum 180), stopping when authenticated native progress
reports the matching native ID and IP active. Existing members retain their
ordinary handshake. Crossplay's private observer request `arcade_join: true`
explicitly enables the same adapter for its isolated native child only. The
compiled guest fixture runs both with the default environment and explicit `1`.

`pc_arcade_join.h` preflights live/pending population writes. Existing human IDs
remain fixed; a new high human slot consumes the first bot slot, leaving all
remaining actor IDs and score matrix columns unchanged. Human holes remain holes
until assigned. The map's original capacity lookup bounds admission. The helper
does not change score rows or match clocks. `tests/network/arcade_join.c` exercises
limits, holes, bot replacement, read-only destination failure and preservation of
actor data, scores and timer through the production helper.

The user's replacement rule is **keep the bot's team, start at zero**. A joining
player's personal score row and total are reset; existing rows and the match
clock remain intact. Team modes adopt the authoritative team from the checkpoint
before checking configuration compatibility. Remaining bots keep their actor IDs.
Configuration matching preserves the local runtime mode-state reset latch
(`config+5D` bit `40`, set by original `22CEB0`) rather than treating it as a
shared rule. Neighboring flag bits still require equality; the compiled fixture
checks both cases. Countdown and local human-count exceptions remain unchanged.

Native `AMPG` progress accepts legacy 16-byte reports and extends to 32 bytes
with map limit, human/bot masks and the local actor slot (distinct from peer ID).
Only validated fresh counters update client metadata. `AMST` phase 5 catches up
the joining member alone; the host and existing players retain phase 1. Native
restoration acknowledges the exact pinned checkpoint sequence. Refreshed pinned
checkpoints must be reapplied before readiness. Host migration phase 2 takes
precedence; delayed phase-5 packets cannot reenter catchup after phase 1 resumes.

Private admission tests cover fresh host slot reports, one pending reservation,
loading deadlines, exact assigned-slot readiness and checkpoint digest/sequence
acknowledgements. Those policy tests do not establish that a player can enter and
play a running match. Native Windows empty-slot Deathmatch and Windows/Mac bot
takeover in Team Deathmatch pass. Repeated post-join Deathmatch crash migration
passes from the original Windows host to the other Windows player, then to the
late-joining Mac; selected-backup scores/countdown survive and five bots keep
fighting. Full-match rejection has private service coverage. Join-specific
results/rematch, other modes and public-WAN gameplay remain separate acceptance
gaps. See existing [Arcade migration](multiplayer-ui.md)
and [crossplay acceptance](../development/arcade-host-migration.md#validation).

Installed-mini Deathmatch acceptance `latejoin-mini-default-a` passes with three
Windows games, no enable flag and identity `2d48f1eb`. It retains the original
host, three humans/five bots, initial zero score and advancing clocks throughout
movement and continued play. This uses the mini's existing HTTPS/UDP listeners
over LAN; it does not establish public-WAN reachability. Client code is pushed;
the two deployed service modules remain private, with the previous service saved
privately for rollback. No public release is implied.
