# Experimental gameplay script worker

The native worker now executes real Lua and an original First to Three mode with
synthetic authoritative game events. This is not yet a playable mod release.
The launcher still refuses Lua activation. An asynchronous native game supervisor,
verified game event/command adapters, multiplayer agreement,
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

The standalone CMake targets use ISO C11 for the worker and C++17 for the trusted
process adapter/diagnostic runner on Windows, macOS and Linux. Feature policy is
shared; OS process creation, isolation, resource queries and pipes use thin adapters.
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
actual host. Production OS adapters and a batch diagnostic supervisor are implemented below;
actual host tests pass for the named process cases; the asynchronous game bridge remains pending.
There is no unsandboxed public activation option in the launcher.

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
never guessed addresses), persistent asynchronous game supervision, native reply validation,
crash-profile recovery, verified helper-image game packaging, script profiles/launcher lifecycle, mod saves, multiplayer
authority/hash checks, richer creator tools and actual custom-mode gameplay on
Windows/Mac/Linux. The full mod-system acceptance matrix remains authoritative.

## Native process boundary (in progress)

`mod_process.hpp` supplies trusted-parent nonblocking pipes, process usage and
owned-process termination. Windows uses a new capability-free LPAC identity and
read-only staged executable/profile, no ordinary AppContainer package-group access,
a one-process128MiB job attached while suspended, kill-on-job-close, dynamic-code
and Win32k restrictions, private inherited stdio handles and a synthetic environment.
The broker requires absolute local paths; Windows rejects reparse ancestors. Only owned temporary cache/profile objects
are modified. Exact protected ACLs grant broker/system/admin full access and the
package read/execute; merging a deny into default profile grants was insufficient
and failed the added profile-write canary. Both the profile-write control and
confined denial now pass. The production worker verifies its token/job/mitigation before input;
it refuses direct unconfined launch. LPAC is established by the documented creation
attribute: TokenIsLessPrivilegedAppContainer is not a supported user-mode query on
the tested Windows build. The positive package-group file canary checks the practical
difference. A fixed installed worker image must be verified by the future game
bridge; uploaded manifests must never choose an executable path.

POSIX uses an empty sensitive environment and private nonblocking pipes/process
groups. Linux forks and closes all inherited descriptors. Darwin uses posix_spawn
with CLOEXEC_DEFAULT and only explicit stdio dup actions; it has no closefrom API. Linux sets parent-death SIGKILL before
exec. The trusted worker precreates its VM/watchdog, then sets no-new-privileges,
non-dumpability,128MiB address-space/core/descriptor limits and a seccomp filter
synchronized over all threads. Only stdin reads/stdout-stderr writes and bounded
memory/timing/thread-wait/exit primitives remain; executable mappings, files,
network, child creation, exec and tracing are denied. The initial adapter supports
Linux x86_64 only; other architectures fail closed. Mac applies a deny-default
Seatbelt policy before untrusted source, with metadata reads but no file contents,
network or child creation. Mac parent-death supervision uses the trusted guardian described below.
Actual Windows/Mac/Linux process tests pass for the named cases.

`tsfp-mod-runner <trusted-worker-image> <owned-cache-root>` is a developer batch
supervisor, **not** the final game session bridge. It accepts at most4MiB stdin,
independently caps wall2s/CPU1.5s/resident128MiB/stdout1MiB/stderr4096, kills/reaps on
failure, and withholds output until the child exits. It never interprets or applies
game commands. These whole-batch limits cannot be used for a long-running match;
the future asynchronous game bridge must budget each request and validate entire
replies before applying transactions without blocking the render/simulation thread.
Mac normal and abrupt parent shutdown signal the independent lifetime guardian
through its private pipe; that guardian kills/reaps its owned worker. Release packaging and worker-image verification remain
unwired. `BUILD-worker.json` schema2 binds both production worker and runner hashes
and records test/sanitizer build modes.

`tests/mods/test_script_process.py` builds production binaries and a separate
native escape/resource canary. Its unconfined controls prove the same operations
work before checking that confinement denies them. Windows8 process checks pass (Mac/Linux7 each, plus one Windows-only skip): file
read/write (including an ALL_APPLICATION_PACKAGES read grant), own profile writes, loopback connection,
child spawn, parent process access, inherited file handle/environment secret,
private registry read, executable ACL rewrite, native infinite loop, output floods,
allocation bound, and real First to Three protocol round trip. Cache sentinels
survive and owned temporary files are removed. The native hang has no VM watchdog,
so it tests the independent parent. Windows strict handle policy can raise an
invalid-handle exception; the test canary catches only STATUS_INVALID_HANDLE.

`TSFP_MOD_TESTING=ON` additionally builds `tsfp-mod-vm-test`, which skips OS sealing
at compile time for focused VM/fuzzer/ASan checks, and `tsfp-mod-process-probe`,
which includes a baseline-control mode. Neither test executable may be shipped.
Production has no bypass argument, environment variable or protocol message.
The normal builder leaves testing OFF. Existing56 VM checks execute the test VM;
production-process checks are separate and never run a sanitized binary whose
instrumentation requires incompatible executable mappings/address-space limits.

### Abrupt broker death

The Darwin Process adapter now starts a separately built trusted lifetime guardian
beside the verified worker. A private FD3 pipe closes when the game parent exits,
including SIGKILL; FD4 returns the owned worker PID before input dispatch. The
guardian forwards no data and reads no mod protocol. It launches the worker with
only stdio, meters no untrusted values, watches parent EOF, and retains waitpid
ownership until that worker exits/is killed. It therefore cannot accidentally
signal a recycled PID. The game parent queries worker CPU/RSS, not the guardian.
Normal shutdown closes liveness and waits for the guardian to kill/reap. The
trusted guardian binary must be included in future signed game-image verification
and is hashed by BUILD-worker.json. Mac production tests pass, including abrupt parent death.

A separate test parent/native canary exercises abrupt broker death without the VM
watchdog: Windows8 process checks pass, including actual kill-on-job-close exit.
Linux PDEATHSIG and the Darwin guardian also pass the actual expanded host checks.
The Windows test waits on the exact worker process handle before deleting only
its owned GUID profile/cache via the profile API. Crash-time automatic stale-cache
and profile recovery is not implemented yet; successful abrupt process termination
does not mean the dead broker ran its ordinary cleanup destructor.

## Persistent asynchronous host API (in progress)

`mod_session.hpp/.cpp` now provides Session::open/dispatch/poll/close/stopped.
It links the OS Process adapter, not the Lua VM. A self-owned background thread
per session performs creation, pipe IO, process metering and cleanup. Public API
calls do no process IO or child waits. Dispatch/poll use try-lock: Busy/Contended
means the host must retry on a later host iteration, preserving the same immutable
event. One pending event and one unconsumed result bound memory/backpressure.
Do not convert Busy into an accepted or dropped death/tick event. The future title
adapter needs a bounded event queue/coalescing policy and must fault the mod if
it cannot preserve required event order; never stall simulation/rendering.

The parent independently validates event lifecycle, sequence/ticks, unique roster,
finite bounded coordinates, player/team/health/score and actor/target IDs. Replies
must match request sequence, protocol/status/heap/count/length budgets, complete
command structures, known player IDs, score bounds, end ordering, printable UTF8
and exact bounded ASCII diagnostics. Load ACKs have zero commands; Stop cannot
change score/end. A full private transaction is published only after validation.
Faults clear unconsumed commands and terminate/reap the worker. Close is an atomic
cancellation plus wake notification, not a game-thread join; stopped() becomes true
only after owned process cleanup. A detached execution thread holds shared state,
not a Session pointer, until cleanup ends. The game must keep its executable/module
alive through shutdown; this is not a dynamically unloadable plugin ABI.

Per-request independent wall budget: load2s, event500ms. CPU budget250ms/request,
50ms while idle,1s per10s session window; RSS128MiB. The session rejects unsolicited
stdout or any stderr, including while a prior result awaits consumption. Source
stays1MiB. Process::wait_for_activity is background-only and waits at most2ms on
pending Windows IO events or POSIX poll; submission/close wake the idle condition
variable. This avoids repeated Windows Sleep rounding without changing global
system timer settings. No response or resource fault blocks the host API.

The session does **not** apply guest memory or commands. Before creating it, a
trusted game adapter must verify installed helper images and package provenance,
select separated saves, and enforce host/multiplayer authority/hash agreement.
Before applying a returned transaction it must revalidate the live match epoch and
current roster, then apply all commands together or reject all. Those adapters,
real title hooks, script profile activation and signed helper packaging are still
required; source-level Session tests do not establish playable game scripting.

`tests/mods/test_script_session.py` compiles actual Session/Process workers and a
separately sealed hostile native fixture (no VM watchdog). Windows55 new checks
pass, alongside8 process checks (one Mac-only skip). Coverage includes the original
mode beyond the diagnostic runner's2s whole-batch limit,100 stateful callbacks,
Stop delivery, unconsumed-result backpressure, maximum-source chunked pipe writes,
invalid source/events, strict malformed/truncated/status/command/text/sequence
corpus, native hangs/crashes/stderr, idle CPU/output and nonblocking close. Valid
native score/draw/end/UTF8 transactions provide positive controls. Test helper
executables are compiled only in TSFP_MOD_TESTING and must never ship. Actual
Mac/Linux Session checks are pending at this checkpoint.
