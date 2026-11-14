# Mod worker and native checks

Shared standalone Lua VM: src/mods/mod_script.c/.h, mod_worker.c and
mod_watchdog.c/.h. Native process boundary: mod_process.hpp, Windows/POSIX
adapters, mod_seal.c/.h and Darwin mod_guardian_mac.c. tsfp-mod-runner is a
bounded batch diagnostic tool, not the asynchronous game bridge. Build through
tools/build/build-mod-worker.py; C11 worker/C++17 parent, pinned CMake4.4.4.
The game is not connected and launcher script activation remains refused.

[API, wire, security gates and evidence](../development/mod-gameplay-scripting.md).
API1 queues score/end/message transactions from copied32-player snapshots.
Memory16MiB, instructions200000 load/100000 callback,64 calls/commands,
512-byte printable UTF8 messages and terminal faults discard partial commands.
The worker watchdog bounds C library operations at500ms. Production precreates
its VM/watchdog then seals before reading any protocol input; seed reset occurs
before loading, preserving deterministic game RNG.

## Process boundary

Process supplies private nonblocking pipes, resource accounting and owned-child
termination. The batch runner independently limits2s wall/1.5s CPU/128MiB resident,
4MiB input/1MiB output/4096 diagnostic bytes; output is withheld until exit.
It never interprets or applies gameplay commands. Per-request persistent game
supervision and native typed-reply validation remain required before activation.

Windows uses a new capability-free low-privilege AppContainer, explicit inherited
stdio allowlist, synthetic environment, dynamic-code/Win32k mitigations and a
one-process128MiB kill-on-close job assigned before resume. The production worker
verifies token/job/mitigation and refuses direct unconfined launch. The documented
LPAC creation attribute is mandatory: TokenIsLessPrivilegedAppContainer class46
is not a supported user-mode query on this host. An ALL_APPLICATION_PACKAGES file
grant is explicitly denied by the production LPAC test. Owned staged image/profile
ACLs are exact/protected (broker/system/admin full; package read/execute). Keeping
default profile grants alongside a deny failed a real profile-write canary.
Ancestor reparse paths are rejected. Normal shutdown removes owned stage/profile;
automatic recovery of stale crash profiles/cache remains pending. The abrupt-death
test cleans only its own recorded GUID profile after waiting for exact worker exit.

Linux x86_64 sets PDEATHSIG before exec, closes inherited descriptors, then the
worker applies non-dumpability/no-new-privileges,128MiB address space/core/FD limits
and thread-synchronized seccomp. It permits private stdio and necessary memory/
clock/wait/exit operations; files/network/spawn/exec/tracing/executable mappings
are denied. Unsupported architectures fail closed.

Darwin uses posix_spawn CLOEXEC_DEFAULT and explicit stdio actions. A separate
trusted guardian watches a private parent-liveness pipe and owns/reaps the worker
on normal or abrupt parent exit. An independent PID handshake lets the parent
meter worker CPU/RSS. Only stdio reaches the worker, which closes startup FDs,
lowers core/FD limits and applies deny-default Seatbelt (metadata reads only).
Darwin has no closefrom API. The paired deprecated Seatbelt APIs have narrowly
scoped warning suppression; no security check or fallback is disabled.

All helper images/paths must come from a verified installation, never manifests.
BUILD-worker.json schema2 hashes worker/runner and Mac guardian and records
source/revision/test/sanitizer modes. Signed game packaging is still unwired.

## Source and tests

Pristine Lua5.5.1 lives under vendor/lua-5.5.1, MIT LICENSES/Lua.txt. The shared
pin records official archive/original hashes, four attributed upstream fixes and
committed prepared hashes. Fixes apply only to verified new build copies with
Git ceiling set; reused copies cannot self-attest. Never force LUA_USE_C89, which
can silently select32-bit Windows integers. The VM asserts64-bit Lua integers.
Lua payloads are LF across hosts. No generated guest/pristine runtime edits.

TSFP_MOD_TESTING builds separate tsfp-mod-vm-test and native process/lifetime
canaries. The VM test binary skips sealing at compile time for focused fuzz/quota/
ASan checks. Production has no runtime bypass. Test binaries must never ship.
tests/mods/test_script_worker.py covers56 actual VM checks; test_script_process.py
covers production OS confinement/resources/lifetime. Both are in native CI; Linux
runs all mod tests. TSFP_MOD_SANITIZE=1 selects ASan/UBSan VM checks, separate from
production-process checks requiring ordinary memory layouts.

Source ac772502 full mod/CI suites pass Windows347/2 skips, Mac348/1 skip,
Linux347/2 skips. Mac/Linux56 ASan/UBSan VM checks each pass. Source6457f1fe expanded
production process tests pass Windows8, Mac7/1 Windows-only skip, Linux7/1 skip.
Actual denied native canaries include private file read/write, loopback connect,
child spawning, parent access/signalling permission, inherited descriptor and
secret environment; Windows also checks registry/executable ACL/profile writes.
Positive unconfined controls prove the capabilities existed. Native hangs/floods/
allocations are bounded and the original First to Three mode runs inside the
production OS policy. Abrupt broker death terminates the exact native worker.
Windows final process/CI recheck118 passes. Host details/logs are in STATUS.

## Remaining integration

No game event/command hooks exist yet. Original title script timing patches are
not a Lua mod VM; verify shared native interfaces/source/callers before wrappers.
Still required: persistent asynchronous native bridge and reply validation,
verified asset/game event adapters, multiplayer authority/hash agreement, mod save
namespaces, launcher script lifecycle, verified signed helper packaging/crash
recovery and actual custom-mode gameplay on all hosts. Full public workshop
acceptance remains mandatory. VM/process checks do not prove those requirements.
Unchanged Python/native trust vectors and live scanner/operator evidence remain
in mod-workshop.md and the catalog trust guide.
