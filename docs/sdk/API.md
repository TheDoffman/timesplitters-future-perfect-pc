# Creator API1

`scripts/main.lua` returns a callback table with integer `api=1`. Optional callbacks
are `on_start`, `on_tick`, `on_player_death` and `on_stop`. Closures retain state
for one match. The game dispatches authoritative offline events; scripts do not
decide authority, open files, connect to the network or access guest pointers.

Each callback receives copied `sequence`, `tick`, `tick_rate`, `actor_id`,
`target_id` and `authoritative` fields. Sequence increases strictly; tick never
decreases. Death uses the killer/world actor and victim target. Player IDs1..32
are opaque roster IDs;0 means world or a drawn match. Start/Stop occur once.
Tick callbacks cannot repeat a tick.

| Call | Behavior |
| --- | --- |
| `game.players()` | Copies current id/team/score/health/x/y/z fields; editing them changes no host state |
| `game.set_score(id, score)` | Queues a known player's integer score in[-1000000,1000000] |
| `game.end_match(winner_id)` | Queues one end;0 draws; subsequent score/end calls fail |
| `game.message(text)` | Queues1..512 bytes of printable valid UTF8 |
| `game.random(low, high)` | Deterministic seeded integer random; endpoints in[-1000000,1000000] |

All commands from one callback commit together after independent host validation.
An error or exceeded budget discards that callback's whole batch and faults the
mode. After requesting match end, only stopping sound and messages are allowed
within the ending callback; subsequent events stop. Invalid roster/resource IDs
cannot grant host access.

## Audio addon1

Declare `audio_api=1` alongside `api=1`. Call sounds only inside callbacks.
Every resource must be an installed, enabled, committed profile WAV named
`audio/xxxxxxxx.wav`, with its numeric key passed to `play`. Keys are shared
across an activated profile; conflicting assets are refused during activation.
These are custom resource keys, not original game sound-event IDs.

| Call | Arguments/defaults |
| --- | --- |
| `game.audio.play(voice, key, pitch, gain, loop)` | voice0..31; key0..fffffffe; pitch0 (authored) or0.25..4, default0; gain0..1 default1; boolean loop defaultfalse |
| `game.audio.pause(voice, paused)` | boolean paused defaulttrue; false resumes |
| `game.audio.pitch(voice, pitch)` | pitch0 resets authored rate; otherwise0.25..4; default0 |
| `game.audio.volume(voice, gain)` | gain0..1, default1 |
| `game.audio.stop(voice)` | Stops one voice |
| `game.audio.stop_all()` | Stops this mode's custom voices |

Numbers must be finite, integer fields must be exact integers, booleans must be
booleans and extra arguments are refused. Control of a finished/absent voice is
harmless. Stop/stop_all are permitted during Stop; new play/pause/pitch/volume are
refused after End or in Stop. Failure, binding/epoch changes and shutdown clear
voices. Device unavailability/mute does not change the gameplay transaction.
The game performs no file decoding from these callbacks.

## Modules and limits

`pack --modules` links up to32 local `.lua` files into `scripts/main.lua`.
`require("rules.scores")` maps to `scripts/rules/scores.lua`. Names are case
sensitive; missing/cyclic/ambiguous imports fail. Values are cached once per VM.
The loader reads no files, other packages or native libraries. All modules share
one VM and its budgets. Generated source must fit1MiB.

The restricted libraries are base/table/string/math/utf8. There is no IO, OS,
package, debug, coroutine, dynamic load, print/warn, rawset, metatable mutation,
pcall/xpcall or string.dump. Use `game.random`, not math.random/randomseed.

| Bound | Limit |
| --- | --- |
| Source |1MiB text; no bytecode or NUL |
| Lua allocator |16MiB |
| Loading |200000 VM instructions |
| Each callback |100000 VM instructions and64 host calls/commands |
| Roster |32 players |
| Wall watchdog |500ms per load/callback/close; trusted parent additionally supervises the confined child |
| Reply body |35104 bytes maximum; whole-batch validation |
| Package |2048 files;256MiB archive/expanded total;64MiB per file, with tighter type limits |

Read calls also consume host-call budget. Do not busy-wait or catch quota errors.
This API supports offline Arcade Deathmatch score modes and custom textures/sounds.
It does not implement arbitrary third-party Lua libraries, spawning new entities,
new playable map formats or online mod negotiation. Uploaded code is never run
inside the hosting service or antivirus validator.
