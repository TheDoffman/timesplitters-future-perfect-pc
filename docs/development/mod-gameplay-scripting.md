# Experimental gameplay script worker

The native worker now executes real Lua and an original First to Three mode with
synthetic authoritative game events. This is not yet a playable mod release.
The launcher still refuses Lua activation. OS confinement, an independent native
game supervisor, verified game event/command adapters, multiplayer agreement,
mod save namespaces and live game acceptance remain mandatory before activation.
VM library restrictions alone are not a security boundary against a native engine
bug. There is no public upload-to-game scripting claim from the tests below.

## Engine and build provenance

Lua5.5.1 is vendored unmodified in `vendor/lua-5.5.1` under its MIT license
(`LICENSES/Lua.txt`). The official [download and archive checksum](https://www.lua.org/download.html)
are recorded in `config/mods/lua-source.json`, together with every original file
hash. The four currently listed [upstream5.5.1 fixes](https://www.lua.org/bugs.html#5.5.1)
are retained with exact commit URLs/patch hashes in `src/mods/lua-fixes`.
The builder verifies original files, applies the fixes to a new build copy, and
verifies every prepared file against committed post-fix hashes. Pristine vendor
files are never patched. Reused prepared sources must still match those hashes.
Git discovery is bounded so patch application behaves identically inside an
ignored checkout build tree and in a standalone temporary directory.

```sh
python tools/build/build-mod-worker.py --output build/mod-worker
python -m pytest tests/mods/test_script_worker.py
```

The standalone CMake target uses ISO C11 on Windows, macOS and Linux. All feature
logic is shared; only the worker watchdog uses thin Windows/POSIX adapters.
Build concurrency is2. `--cmake` selects the repository-pinned CMake executable;
`--sanitize` enables AddressSanitizer/UndefinedBehaviorSanitizer on Clang/GCC.
Native CI installs the shared CMake pin before testing. No online source fetch
is required to build, and players will not need Lua, Python, Git or CMake once
the worker is integrated and bundled with the game.

Lua's native integer width is asserted to be64 bits. Do not force
`LUA_USE_C89` externally: that can suppress the upstream Windows selection and
silently reduce Lua integers to32 bits on Windows. Tick values up to2^53-1 must
remain exact on every host. `BUILD-worker.json` binds worker binary/source/fix
hashes and the repository revision. Release packaging must include the Lua license
and corresponding project source/build information; it is not yet wired into the
game release packagers.

## Draft API1

A source-only `scripts/main.lua` returns a table with integer `api=1` and any of:
`on_start`, `on_tick`, `on_player_death`, `on_stop`. Unknown callback names,
non-functions and wrong API versions fail loading. State in Lua closures persists
for that match. Each callback receives a copied event table:

| Field | Meaning |
| --- | --- |
| sequence | Strictly increasing host event sequence, starting1 |
| tick | Nondecreasing simulation tick, at most2^53-1 |
| tick_rate | Host simulation ticks per second,1..1000 |
| actor_id | Acting/killer player;0 for a world event |
| target_id | Target/victim player; nonzero for death |
| authoritative | True; only the authoritative game bridge may dispatch |

The host must dispatch Start once, then Tick/Death, then Stop once. Tick callbacks
cannot repeat a tick. After a script requests match end, only Stop is allowed.
Player IDs are opaque integers1..32, not guest pointers. Actor/target/winner IDs
must refer to the current event roster (except world/draw ID0). Roster IDs must
be unique; positions must be finite and bounded. The worker cannot certify the
host's authority itself: the future game bridge must enforce local/host ownership
and multiplayer mod-hash agreement before spawning/dispatching.

| Call | Behavior |
| --- | --- |
| `game.players()` | Returns copies with id/team/score/health/x/y/z; modifying them changes no host data |
| `game.set_score(id, score)` | Queues an integer score in[-1000000,1000000] for a known player |
| `game.end_match(winner_id)` | Queues one match end;0 means draw; score/end calls afterward fail |
| `game.message(text)` | Queues printable valid UTF8,1..512 bytes; no C0/C1 controls |
| `game.random(low, high)` | Deterministic seeded integer generator; endpoints in[-1000000,1000000] |

Every game API call counts against the callback's host-call quota, including
read-only calls. Commands accumulate in a private transaction. An error,
allocation/instruction/host-call limit or invalid API argument discards **all**
commands from that callback and permanently faults that worker. The host must
validate the entire reply, matching sequence and current player roster, before
applying any command. A success reply cannot authorize arbitrary pointers, file
paths, console commands or function addresses. Native adapters must treat messages
as text, never as printf format strings or executable commands.

The current libraries are base, table, string, math and utf8. No IO, OS, package,
debug, coroutine, require, file loaders, dynamic load, collectgarbage control,
print/warn, metatable modification or rawset is available. `string.dump` and
platform-seeded math.random/randomseed are removed. Use `game.random` instead.
pcall/xpcall are deliberately absent so scripts cannot catch quota errors and
continue indefinitely. Host-side fault state is sticky as another guard. This is
a restricted Lua API, not a promise to run arbitrary third-party Lua libraries.

## Budgets and containment layers

| Resource | Worker bound |
| --- | --- |
| Source |1MiB; no NUL or binary chunk header; text-only Lua loader |
| Lua allocator |16MiB including actual allocation-header/reserved-block charges |
| Module execution |200000 VM instructions |
| Callback |100000 VM instructions,64 host calls/commands |
| Roster |32 players; finite positions within10000000 world units |
| Message |512 bytes, valid printable UTF8 |
| Reply |34080 bytes maximum; no partial commands on failure |
| Wall watchdog |500ms per load/callback/close, including expensive C library operations |

The instruction hook runs every1000 instructions. The allocator tracks its own
actual block sizes and preserves shrinking blocks when required by Lua's allocator
contract. A budget breach is terminal even if Lua's allocator retries after GC.
All script/module/event work executes through protected Lua calls. The watchdog
uses a separate thread and terminates only the worker with exit124 if an operation
does not return, including C pattern matching that instruction hooks cannot stop.

The watchdog is a defense inside the worker, **not** the final trusted supervisor.
An exploited worker might defeat it. The external game parent must independently
bound wall time, RSS/CPU, pipe reads/writes, stderr/output, process lifetime and
children, sanitize inherited environment/handles, and disable that mod on failure
without stalling/crashing the game.
It must create a confined process (Windows AppContainer/restricted handles/job,
Mac Seatbelt, Linux no-new-privileges/seccomp/descriptor isolation) before executing
uploaded source. File/network/child-process escape canaries must pass on each
actual host. Those adapters/supervisor are pending; there is no unsandboxed public
activation option in the launcher.

## Binary protocol1

All numbers are little endian. A frame begins with uint32 body length. Inputs
are at most1048584 bytes; replies are at most34080 bytes. Truncated, oversized,
unknown or trailing data fails closed. Stdin/stdout are binary mode on Windows.
The worker accepts no arguments, source paths or filesystem commands.

Load: uint32 type1, uint32 seed, then source bytes. Only one load is accepted.
Dispatch: uint32 type2, uint64 sequence, uint32 kind (Start1/Tick2/Death3/Stop4),
uint64 tick, uint32 tick_rate/actor/target/player_count, then roster entries.
Each entry is uint32 id/team, int32 score/health, IEEE754 float32 x/y/z.
Quit: uint32 type3 only. EOF also shuts down; it is not a successful Stop event.

Reply: uint32 protocol1/status, uint64 sequence, uint32 memory_bytes/command_count;
then each command: uint32 kind/player, int32 value, uint32 text_length, text bytes.
Kinds: Score1, End2, Message3. Finally uint32 diagnostic_length and at most255
printable ASCII diagnostic bytes. Status0 succeeds;1 faults the script/request;
2 rejects a frame. Error replies have no commands. Load sequence is0. A hard wall
termination has no event reply. The reference `mod_script_protocol.py` codec
validates bounds/types/text/sequence/roster/end order and never applies commands.
It is developer/test code, not the future native game supervisor.

## Original mode and evidence

`examples/mods/first-to-three` contains an original manifest and Lua mode. The
actual native-worker test loads that file, starts a synthetic match, delivers
three deaths, verifies scores1/2/3 and the winner command, then stops the match.
It does not modify real game data or saves. Change the author field to your
creator account before eventual authenticated submission. Packaging a script
does not make it eligible for activation in the current launcher.

`tests/mods/test_script_worker.py` builds the actual native worker and exercises
forbidden libraries, load/callback infinite loops, memory exhaustion, command
rollback, invalid player/score/message values, lifecycle/sequence/tick validation,
duplicate IDs/nonfinite positions, independent snapshot copies, deterministic RNG,
64-bit tick fidelity, upstream numeric fixes, binary/NUL source refusal, hostile
frames/replies, C-function wall timeout, and fixed-source build verification in
both checkout and standalone output directories. Windows, Mac and Linux runs
pass; actual ASan/UBSan runs on Mac and Linux also pass. STATUS records exact
source and evidence.

Still required: real game hooks/command adapters (use verified source evidence,
never guessed addresses), independently supervised OS confinement on all three
hosts, game packaging, script profiles/launcher lifecycle, mod saves, multiplayer
authority/hash checks, richer creator tools and actual custom-mode gameplay on
Windows/Mac/Linux. The full mod-system acceptance matrix remains authoritative.
