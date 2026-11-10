# Arcade host migration

Status: native checkpoint/authority adapters and authenticated v3 relay integration
are implemented on `feature/online-host-migration`, dependent on the unmerged relay
branch at `a2d81939`. Live game continuation acceptance is still pending. The live
Mac mini service and published friend-test binaries have not been updated.

## Shared implementation

`online_arcade_service.ArcadeService` owns separate v3 Arcade rooms under the
existing directory lock. Stable member bearer credentials authorize HTTPS updates;
wire and relay keys rotate on every authority epoch. During preparation the relay
forwards no game traffic. It resumes only after surviving native engines restore
and acknowledge the selected checkpoint. Native progress counters, rather than
companion HTTP heartbeats, renew four-second engine leases. Retired peers cannot
rejoin the active round or regain authority. v2 Story and direct sessions retain
their existing behavior.

`online_arcade_client.ArcadeSession` supervises the dynamic host bridge and a separate
authenticated loopback control socket. It verifies SHA-256 checkpoint identities,
retries bounded CRC-checked fragments, and sends native acknowledgements to the
service. Native hosts capture twice per second; each newly validated checkpoint
is acknowledged immediately, with bounded ACK retries. Graceful departure waits for a backup to acknowledge the final frozen
checkpoint, naming its exact sequence and retrying the frozen upload. A six-second
normal-exit deadline falls back to the freshest viable backup so a broken client
cannot indefinitely prevent departure; a lone owner can leave without a backup.
Crash migration chooses the
freshest acknowledged checkpoint and may roll back to that frame. No viable backup
closes the room. The server keeps the latest two checkpoints in memory. End-of-round
lobby metadata retains the last nonce until the next round advances the epoch;
transport cleanup never rewinds the native progress counter.

`pc_arcade_checkpoint.h` defines a pointer-free schema (magic/schema/size, round
nonce, epoch, sequence, level, tick, rules, sixteen score rows/totals/timestamps,
and up to ten bot snapshots). Capture requires active Arcade phase15/play7 and
matching bot ticks. Restore preflights all destination spans and bot objects before
writing scores, bot numeric state, elapsed ticks and the separate remaining
match countdown at config+60. Compatibility compares shared rules while allowing
local player counts, non-team human placeholders and the live countdown to
differ between healthy peers; local fields are preserved. If a selected backup needs
a body already removed locally, preparation first uses the existing stock bot
revival allocator to create a local body; full restore preflight still follows. Player/controller pointers,
profiles, rendering resources and local inputs remain local.

`pc_arcade_control.h`, `pc_network_arcade.inc` and `pc_arcade_migration_guest.inc`
implement bounded native reassembly, restore/ready, XNet session-key adoption and
authority commit. Companion virtual IPs are mapped through the original network
peer table; they are not native player slot IDs. Promotion preserves the existing
actor pool and local node instead of calling stock host creation, which resets
the match. The shared network-frame hook pauses stock processing during handoff;
`pc_native_timing_guest.inc` holds game delta while render/wall clocks continue.
`pc_multiplayer_exit.inc` defers a normal host exit until the final checkpoint is
acknowledged and the service has fenced that owner. Results callbacks require
actual active-match engine phases before deferring, because directory metadata
can briefly lag behind the engine transition.

`3000-arcade-migration-build.patch` enables the same adapter for Windows, Mac and
Linux game builds. Online shared Arcade uses v3; Story remains v2. The browser also lists/joins
explicit legacy Arcade rooms via typed directory-status fallback, preserving
automatic self-hosting. Authentication, certificate and active-match errors never
trigger that fallback. Isolated browser
automation opts in with `migration: true` so legacy/direct regressions remain
meaningful. Companion bundles, server installation and compatibility identities
include all new modules. Matching native rebuilds are required.

## Validation

- `tests/network/test_online_migration.py`: deterministic election, freshness,
  failed candidates, stalled survivors, repeated migration and old-epoch fencing.
- `tests/network/test_arcade_checkpoint.py`: compiles production checkpoint,
  control codec, transport and game-thread adapter against isolated guest-memory
  fixtures. Includes six bots, score/tick restore, missing/stale/invalid state,
  authenticated control, fragmented transfer and native peer-ID promotion.
- `tests/network/multiplayer_exit.c`: production pause/results leave hooks defer
  host unload until fencing, then unload only the departing game.
- `tests/network/test_arcade_service.py`: three real TLS/UDP companions, using the
  exact C fixture ABI. Reordered final-upload/leave, normal departure and replacement-host loss (either
  companion crash or engine stall with live HTTP heartbeats) restore
  the expected bytes and keeps real gameplay datagrams flowing between survivors.
- `tools/testing/test-online-bot-motion.py --migration crash --tag UNIQUE`: isolated
  two-game native acceptance (`crash` or `leave`), requiring checkpoint
  publication/engine ACK before departure, native promotion, surviving score rows,
  the selected backup tick and bot motion/combat after migration. Normal build identity/seal and test-save isolation are enforced.

Fixture and socket tests do not demonstrate that original AI decisions, weapon
inventory, pickups, objectives or every Arcade mode survive a live handoff.
Bot snapshots restore presentation/combat fields but do not serialize opaque AI
pointers. Live Deathmatch continuation, graceful menu exit, repeated actual-game
handoffs, completion/rematch, physical Windows/Mac crossplay and outside-network
play remain acceptance requirements. Do not call this a finished migration release
until that evidence exists. See [shared relay validation](online-relay.md).

`TSFP_ARCADE_TRACE=1` enables bounded stderr capture/control diagnostics in the
native adapter; the isolated migration runner enables it and records sanitized
service counters/phase alongside score and timer evidence. Generated Windows
acceptance code uses the supported `--split 32` generator option and `_CL_=/MP4`
to stay within the local compiler memory limit. No generated guest C is edited.
