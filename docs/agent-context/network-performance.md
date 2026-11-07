# Network eligibility and Vietnam profiling

Vietnam CPU regression: `stability_transitions_game.py --scenario crowd-vietnam`
uses the existing normal Arcade route, scene48, and checks ten configured bots.
Use `--profile-cpu` only for attribution; clean pacing requires `--no-captures
--no-native-trace --profile-presents`. `test_native_network.py` also runs the real
transport eligibility fixture: one million repeated queries cause only the two
startup environment reads, with offline/Online/LAN transitions and missing-file
coverage. This test travels with the existing native-network CI selection.

The startup environment is immutable after first eligibility query. Menu-selected LAN
uses `pc_network_enable_lan`; Online connect/disconnect uses `pc_network_set_online`.
The latter updates an atomic flag only after validating a successful configuration,
and resets it on explicit disconnect. Transport shutdown alone retains eligibility.
Do not restore per-dispatch getenv/file checks: guest dispatch calls this for arbitrary
game functions, even when there is no network match. The transport fixture covers
startup flags, missing startup files, explicit disconnect and a million offline calls.

## Existing crowd measurement contract

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

`multiplayer_exit_lookup` now rejects addresses outside its three owned callbacks
before any network or guest-state access. The existing multiplayer-exit fixture
checks a million unrelated dispatches with zero eligibility checks, then exercises
its real host/guest/lobby exit paths. This removes the sampled caller itself from
ordinary gameplay dispatch in addition to making other eligibility users cheap.
