# Mod worker, process boundary and native sessions

Authoritative source: src/mods/mod_script.c/.h (Lua API), mod_worker.c (wire),
mod_watchdog.c/.h, mod_seal.c/.h, mod_process.hpp + Windows/POSIX adapters,
mod_guardian_mac.c, mod_session.cpp/.hpp, mod_match.cpp/.h and pc_mod_guest.inc.
Build the worker with tools/build/build-mod-worker.py. VM C11; parent C++17.
The shared game build links the match/title bridge with NULL default binding;
verified bootstrap, save separation and launcher script activation remain unavailable.

`mod_saves.cpp/.hpp` provides the native save-namespace lease for that future
bootstrap. A strict64-hex profile identity chooses a sibling ModSaves/<identity>
directory; normal saves are neither copied nor modified. Versioned owner markers
must match exactly; unknown/incomplete directories survive refusal. The lease
blocks a second process, survives normal close with progress intact and releases
on abrupt process exit. Recheck rejects linked/hard-linked/special files, changed
markers, excessive depth/count and unsafe writer permissions. No guest storage
selects this root yet; bootstrap must hold the lease through storage shutdown and
abort scripted play on failure, never fall back to original saves.
guest_storage.c now provides a one-way storage fence, covering lazy defaults and
pre-init profile overrides. The kernel adapter applies it before filesystem
creation. Native bootstrap must arm it with the held lease before kernel init;
that caller/source capsule integration is still unavailable. See host-storage.md.
Windows creates protected user/System/admin ACLs and accepts owner-only parent
OWNER RIGHTS grants. POSIX requires current ownership and no other writers.
Recognized exFAT volumes use structural markers/link checks/locking because native
ACLs/mode ownership are unavailable; actual exFAT/sandbox acceptance is pending.
mod_files.hpp exposes the existing path validator; ImageStage retains its strict
private-directory default. Save leases apply their own volume-aware policy.

`mod_images.cpp/.hpp` stages immutable worker/guardian copies with expected SHA256
pins supplied by the trusted caller. Windows uses CNG, Darwin CommonCrypto and
Linux OpenSSL EVP. Local absolute paths reject linked ancestors; cache staging is
exclusive, bounded32MiB/image, cancellable between chunks and read-only afterward.
Windows also rejects trailing-dot/space and alternate-stream path aliases.
The digest covers the copied stream, not a path later reopened for copying.
Only owned files/empty directories are removed; unknown contents survive cleanup.
Session::open_verified runs staging on its existing background thread and keeps
the images until child cleanup ends. Match now requires image pins as well as
policy assertions and always uses that verified entry. Game builds additionally
compare caller pins against generated compiled pins; uploaded hashes cannot
select a different worker. Bootstrap remains unwired; launcher scripting is unavailable.
tools/build/mod_runtime_images.py now builds production-only helpers, verifies
source/configuration/image receipts, stages immutable content-addressed bundles
and emits a generated pin header. All three normal native game builders invoke
it before CMake, staging ModRuntime/<identity> beside the game executable.
Game CMake requires that header. Public game packagers preserve those exact
helper bytes and validate header/bundle/build consistency before copying; Linux
keeps helpers out of RPATH rewriting and records their source/Lua notice audit.
Native release-package assembly/signature/relocation acceptance remains pending.
Mac helpers explicitly target macOS11.0 (first Apple Silicon), rather than the
build host's OS. Production staging checks that CMake target; native tests inspect
both Mach-O minimum-version records. This does not claim execution on older hardware.
The relocated production-bundle test runs First to Three's start/three deaths/
stop under the real OS boundary and validates score1/2/3 plus End. It is a worker
protocol test; the game's default binding remains NULL.
No diagnostic runner, test image or VM library is distributed in those bundles.

[Full contracts and evidence](../development/mod-gameplay-scripting.md).
API1 provides on_start/on_tick/on_player_death/on_stop, copied32-player snapshots,
score/end/message transactions and deterministic RNG. Memory16MiB; load/callback
instructions200000/100000;64 calls/commands;512-byte printable UTF8 messages.
Terminal faults discard partial commands. Worker watchdog500ms includes C library
operations. Production precreates VM/watchdog, seals, then reads source. Source
seed reset precedes loading; Lua hash-state initialization is separate.

## Native session

Session links Process/shared POD types, never Lua. A background-owned thread
creates/meters/communicates/reaps; open/dispatch/poll/close do no child IO/waits.
Dispatch/poll try-lock and return Busy/Contended. The caller retains/retries the
same immutable event on a later iteration. One pending event and one unconsumed
result bound backpressure; there is no unbounded queue. Close signals/wakes,
never joins on the host thread; stopped() confirms owned child cleanup ended.
The executable/module must stay alive until stopped, including early-error teardown.
The test host has an explicit drain guard; this is not a dynamically unloadable ABI.

Host event checks cover lifecycle/sequence/ticks, roster uniqueness and finite/
bounded player fields/IDs. Native reply parsing checks exact protocol/status/
sequence/heap/count/length, all command structures/targets/score/end order, printable
UTF8 and bounded ASCII diagnostics. Load ACK has no commands; Stop has no score/
end. A full private result is validated before publication. Faults clear unconsumed
commands and kill/reap. Unsolicited stdout/stderr faults even while a result waits.
Load2s/event500ms wall;250ms CPU per request;128MiB RSS; idle50ms CPU per1s window;
session1s CPU per10s window. Renew idle windows after checking prior usage, so
legitimate paused watchdog overhead does not accumulate forever. A sealed fixture
spends25ms actual CPU in each of four idle windows as the positive control.

Background pipe waits use pending Windows IO events/POSIX poll, at most2ms;
submission/close wake the idle condition variable. No global system timer changes.
tsfp-mod-runner remains the separate batch diagnostic supervisor (2s whole-batch,
1.5s CPU/128MiB RSS,4MiB input/1MiB stdout/4096 stderr); it is not used for matches.
Neither Session nor runner applies guest commands.

## OS adapters

Windows: capability-free LPAC; explicit stdio inheritance; synthetic environment;
dynamic-code/Win32k mitigations; one-process128MiB kill-on-close job assigned before
resume. Worker verifies token/job/mitigation, refuses direct unconfined launch.
LPAC uses the documented creation opt-out attribute; TokenIsLessPrivilegedAppContainer
class46 is unsupported for user-mode queries here. An explicit ALL_APPLICATION_PACKAGES
file read grant is denied in real tests. Owned staged image/profile ACLs are exact/
protected: broker/system/admin full, package read/execute. Keeping default profile
callback/capability grants beside a deny failed the original profile-write canary.
Ancestor reparse paths are rejected. Normal cleanup removes owned cache/profile.
Automatic recovery of stale crash profiles/cache remains pending; abrupt-death
tests clean only their own recorded GUID identities after exact worker exit.

Linux x86_64: preexec PDEATHSIG, close_range, non-dumpable/no-new-privileges,
128MiB address space/core/FD limits, seccomp TSYNC over all threads. Private stdio
plus memory/clock/wait/exit primitives; no files/network/spawn/exec/tracing or
executable mappings. Unsupported architectures fail closed.

Darwin: posix_spawn CLOEXEC_DEFAULT/explicit stdio; a separate trusted guardian
watches parent liveness, owns/reaps the worker after normal/abrupt parent death,
and returns its PID over a separate private handshake so the host meters worker
CPU/RSS. Only stdio reaches the worker; startup FDs close, core/FD limits lower,
deny-default Seatbelt permits metadata reads only. No closefrom API exists. Paired
deprecated Seatbelt APIs have narrowly scoped warning suppression, no fallback.
Darwin rusage CPU totals are Mach ticks: checked128-bit mach_timebase_info conversion
is mandatory. /1000 alone undercounted Apple Silicon CPU, exposed by renewed idle
busy-spin checks. Actual corrected CPU/native idle tests now pass on the Mac mini.

## Source, builds and verification

Pristine Lua5.5.1 is vendor/lua-5.5.1, MIT LICENSES/Lua.txt. The pin records official
archive/original files, four attributed upstream fixes and committed prepared hashes.
Fix only verified build copies, with Git ceiling set. Reuse must match committed
hashes, never self-attested receipts. Never force LUA_USE_C89 (can select32-bit
Windows integers); the VM asserts64-bit integers. Lua payloads are LF on all hosts.
No generated guest/pristine runtime edits were made for this component.

BUILD-worker.json schema2 binds worker/runner/Mac guardian, source/revision and
separate testing/sanitize/host_sanitize flags. Helper paths/images must come from a
verified installation, never uploaded manifests. Signed game packaging is unwired.
TSFP_MOD_TESTING builds separate vm-test/native process/lifetime/session fixtures;
NEVER SHIP test binaries. The VM test binary skips sealing only at compile time;
production has no runtime bypass. Tests/mods routes are in native CI/Linux all-mod.

TSFP_MOD_SANITIZE=1 instruments the VM test. TSFP_MOD_HOST_SANITIZE=1 instruments
trusted Session/Process/driver with ASan/UBSan on Clang/GCC, while sealed children
remain ordinary (OS exec/address-space limits conflict with child instrumentation).
The builder --sanitize-host exposes the latter option; both are off by default.

Source46a34727:57 Session cases pass Windows/Mac/Linux;57 host-ASan/UBSan Session
cases also pass on each of Mac/Linux. Combined Session/Process: Windows65 pass/1
Mac-only skip, Mac65 pass/1 Windows-only skip, Linux64 pass/2 skips. Windows final
with CI selection/runtime-check cases:175 pass/1 skip. Full mod/CI suites at matching
production source: Windows404/3 skips, Linux404/3 skips (b2afea90), Mac406/1 skip
(f37ee9ba). The later change adds only test-host teardown/case.56 actual VM checks
are included in those full suites; prior Mac/Linux VM-ASan checks also pass.

Coverage: original First to Three mode past2s,100 stateful callbacks/Stop, full1MiB
source pipe transfer, backpressure, invalid source/events and malicious native wire
corpus, valid boundary/UTF8 controls, native hang/partial pipes/crash/stderr/idle
abuse, renewing idle CPU, nonblocking cancel and early-error teardown. OS canaries
have successful unconfined controls and denied file/network/spawn/parent/signal/
inherited descriptor/environment access; Windows adds registry/executable ACL and
private profile writes. Abrupt broker death terminates the exact native worker.
See STATUS/ignored logs for exact host versions and per-revision receipts.

Three empty Windows profiles from failed pre-drain test-host exits were individually
verified by known creation times, protected4-rule ACL/current broker owner/derived
package SID, then removed through the profile API. No broad package-folder deletion.
Ownership evidence stays ignored; manual test cleanup is not production crash recovery.

## Remaining game/public integration

`mod_match.h/.cpp` supplies a C ABI over Session: a64-event queue coalesces only
consecutive ticks, preserves death barriers and assigns sequence numbers only on
accepted dispatch. It revalidates epoch/offline authority/stable actor identity
before committing copied score/end/message commands. Score/rank reads also need
a fresh world. A fault closes the worker and disables the derived score overlay.
The trusted caller asserts verified images/source, separate saves and offline
policy before open; these assertions are not proof supplied by a mod manifest.

`pc_mod_guest.inc` initially accepts only offline non-team Arcade rules-index0.
It captures the supported US actor pool, preserving actor identity across body
respawn. The death wrapper calls the original once and requires its accepted
entity+258 serial increment. Derived actor+a6c/a68 and the sorted slot array use
committed scores; raw kills/deaths remain original. Patch2710 wraps641C0/64810,
674C0 and the existing681C0 Story frame adapter. End calls the stock67450 helper.
`mods/game.cmake` links only the C++ parent/OS adapter into the game, never Lua.
NULL binding preserves stock behavior. There is no file/environment activation
entry point; the future trusted bootstrap must own binding and worker lifetime.
The18 controlled-world/native bridge fixtures use real isolated Lua but
mock original title functions. They pass Windows/Mac/Linux, including Unix host
sanitizers. They do not establish actual gameplay or final
fault UX; messages still need a shared HUD adapter. Registered native route:
`tests/mods/test_mod_match.py`. Full game/play acceptance remains separate.

Sourceb8504672 ordinary bridge/Session/Process checks pass Windows83/one skip,
Mac83/one skip and Linux82/two skips. Host sanitizers caught an unaligned32-bit
read in the fixture's16-bit raw-stat assertion; the assertion now reads its two
bytes; corrected sanitizer checks pass as recorded below. Production was unchanged.
`test_original_mod_scores.py` locally lifts only pinned US score/rank/end routines
into a disposable fixture linked to the real isolated parent. Generated/private
instructions remain local. Missing private XBE skips; a wrong hash fails. Stock
death/frame/final finish side effects remain mocks, so it is not live gameplay.
The initial non-team adapter reports API team0 rather than exposing the title's
actor+84 identity comparison value; stock raw score calculation still uses it.

Multiplayer authority/hash agreement, mod save namespaces, launcher script
lifecycle, verified signed helper packaging/crash recovery, message rendering
and actual custom-mode gameplay remain required. The host
must establish image/package/authority/save policy before opening a session. Existing
original-title script timing patches are not Lua hooks; verify native source/callers
before adding wrappers. Full public workshop acceptance remains mandatory. These
component tests do not prove gameplay, public HTTPS, backups or a usable release.
Unchanged trust vectors and scanner/operator evidence are in mod-workshop.md.

Original-title follow-up source186c62ac: Windows19, Mac19 and Linux19 bridge plus
local original-routine checks pass. Mac/Linux also pass all19 under trusted-parent
ASan/UBSan (18 bridge and one original test run separately), without skips. The
fixture checks unbound original raw-score/ranking before isolated custom-mode
score/rank/End; final finish side effects, death and frame functions stay mocked.
The title snapshot/runtime/Capstone5.0.9 are required for this local-only test.
Full native builds and live gameplay are still separate pending acceptance.

Game parent build/stock-scene milestone: Windows full build/120-second native60
Story observer pass, Mac arm64 build/platform checks and reviewed post-skip
Story gameplay capture pass, Linux Clang build/MMIO and software-Vulkan state-
verified Story window pass. Sourcec5ed602d CI1399 aggregate passes. NULL mod binding
is used throughout; these checks do not activate scripts or replace full custom-
mode/save/helper/public acceptance. Exact hashes and limits are in STATUS.
