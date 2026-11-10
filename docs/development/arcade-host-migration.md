# Arcade host migration groundwork

Status: **coordinator prototype only; active-match host migration is not usable.**
The user selected Arcade first on 2026-11-10; Story is excluded. Branch
`feature/online-host-migration` depends on the unmerged shared relay branch at
`a2d81939`. Neither the running Mac mini service nor the friend-test relay branch
has been changed by this work.

## What is implemented

`tools/network/online_migration.py` implements an isolated, clock-injected
coordinator for the future authenticated directory/relay integration:

- Per-match identities, stable participant IDs and monotonically increasing
  authority epochs prevent an old host from regaining authority after a handoff.
- The host publishes bounded checkpoints. The server retains only the latest two
  and binds each digest to the match, epoch, sequence, tick and payload. A client
  is a candidate only after its **native game** acknowledges a fresh checkpoint.
- The freshest acknowledged checkpoint wins, with stable peer ID breaking ties.
  A graceful host departure or four-second loss of its lease starts preparation.
- No player has host authority during preparation. All surviving native games
  must restore the selected checkpoint and acknowledge the new epoch and digest.
  Bridge heartbeats do not count as engine readiness.
- A failed successor is retired and another eligible player is selected under a
  new epoch. A ready successor excludes a stalled nonhost after the preparation
  deadline. No viable backup closes the coordinator instead of faking success.
- Story, legacy schemas, stale acknowledgements, ID reuse, oversized checkpoints
  and unauthorized checkpoint publication are refused.

The caller must authenticate every operation and hold its directory room lock.
The coordinator does not own sockets or credentials. Its opaque checkpoint bytes
are not yet a native game-state format. It is intentionally not bundled or
enabled: importing it into the server without the remaining steps would not
implement the requested feature.

## Confirmed native constraints

The supported US XBE was verified against the repository SHA-256 before read-only
inspection. No generated C or game data was edited.

The current Arcade host-loss path in original `35BEF0` reaches `65420` at return
site `35C360` during phases15/16. The existing
`pc_multiplayer_disconnect_exit` adapter routes that loss back to the network
menu and closes the companion. `pc_multiplayer_exit.inc` also makes deliberate
host Return to Lobby unload the match for the party. Both paths must distinguish
a completed migration from an ordinary exit.

Original host creation `3569D0` clears session state, recreates the local node,
rebuilds profile/controller membership and assigns the host role. Calling it in
the middle of a match is not a supported continuation path. A role-word change
alone does not reconstruct that initialization or transfer game state.

Current game host/member role is stored at `76B140`, the local node pointer at
`774028`, and the host peer ID at `774040`. These original game IDs are distinct
from the companion's virtual-address peer IDs. Never assume the next companion
peer is the next native player slot, renumber existing players or overwrite
personal profile/configuration memory.

The bridge and relay currently assume peer1 is the host. Guest credentials are
distributed to that owner, and guest-to-guest traffic passes through it. Changing
only the directory owner would leave data, authentication and guest host pointers
addressed to the departed machine.

`PcNetworkBotState` transports presentation/combat state and selected score
fields. It is not a complete serialized host checkpoint or AI execution state.
The original engine also owns game-mode objectives, pickups, RNG state and
reliable peer/channel state. These need explicit investigation and portable
serialization/restoration; copying arbitrary guest memory between native
processes/platforms is not an acceptable substitute.

## Required integration before enabling

1. Define and verify the Arcade checkpoint schema using actual game state,
   including scores/clock/rules, mode objectives, actors/bots and world state.
   Keep local inputs, cameras, profiles, native resources and pointers local.
2. Add shared native capture/restore/readiness hooks at safe game-thread points,
   including how a guest becomes authoritative and how old-host departures are
   retired without the stock full-party teardown. Validate original functions
   rather than assuming that a role flag is sufficient.
3. Bind the coordinator to authenticated directory membership and native progress;
   rotate/revoke owner and host-to-peer credentials under each epoch. Route only
   the selected active host and reject packets from old epochs before delivery.
   Stable virtual IPs and native peer identities must survive the handoff.
4. Add bounded reliable checkpoint transfer over the existing authenticated path.
   Treat missing fragments and late checkpoints as failures. Pause/resume the
   match consistently during preparation and show migration status to players.
5. Exercise three actual game instances: graceful quit, terminated host process,
   failed replacement, repeated migration and old-host reconnection. Verify the
   current map/mode, scores, remaining time, actor ownership, bots and ability to
   finish/rematch. Repeat on physical Windows/Mac and outside the home network.

Until then the correct user-facing statement remains: the first host leaving
ends the active match. The prototype is not a testable host-migration release.

## Validation

`tests/network/test_online_migration.py` has 18 coordinator contracts. They cover
both departure types, engine readiness versus heartbeat, checkpoint selection,
stale/mismatched/cross-match ACKs, candidate failure and disconnect, survivor
timeouts, repeated migrations, admission/size bounds and unrecoverable shutdown.
The module is registered in the native runtime CI suite. These are deterministic
policy tests, not live relay integration or game-continuation acceptance.

Existing relay validation is documented in
[the shared relay guide](online-relay.md#protocol-and-validation).
