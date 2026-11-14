# Mod worker and native checks

Shared standalone VM: src/mods/mod_script.c/.h, mod_worker.c and mod_watchdog.c/.h.
Build with tools/build/build-mod-worker.py (pinned CMake); test through
tests/mods/test_script_worker.py, registered in Windows/Mac native suites and
the Linux all-mod step. No game assets required. TSFP_MOD_SANITIZE=1 enables
ASan/UBSan for its compiled Clang/GCC fixture. The worker is not linked to the
game yet, and launcher script activation remains refused.
GCC treats misleading guard indentation as an error; shared worker guards use
separate statements on separate lines. Warning policy remains enabled.

[API, wire, security gates and evidence](../development/mod-gameplay-scripting.md).
The exact First to Three example is executed by native worker tests. API1 queues
score/end/message commands and returns copied player snapshots, no guest memory.
Module/callback limits200000/100000 instructions,16MiB charged Lua heap,64 calls/
commands,32 players,512-byte messages. Protected calls and terminal transactional
faults discard partial commands. A500ms worker watchdog also bounds C functions.
Native Process adapters and a bounded batch diagnostic supervisor now exist;
Windows7 production-process tests pass, including positive controls and denied
file/network/spawn/parent/registry/handle/ACL/environment canaries, native hangs,
output/diagnostic floods, allocation bounds and the real original mode. Mac/Linux
validation is pending. See the development guide for OS policy/limits. This is
not yet an asynchronous game-parent bridge or a complete playable sandbox.

Pristine Lua5.5.1 is under vendor/lua-5.5.1 with MIT notice LICENSES/Lua.txt.
config/mods/lua-source.json pins official archive/original source/four upstream
fixes and final prepared hashes. src/mods/lua-fixes holds exact attributed fixes.
Only new verified build copies are patched. Git ceiling avoids silently skipped
patches inside an ignored repo build path. Reused copies must match committed
prepared hashes, not self-attested receipts. Never force LUA_USE_C89: it can
silently select32-bit Windows integers. Worker asserts64-bit Lua integers.
Lua source uses LF in Git checkouts to keep script payload bytes consistent across OS.

Game event integration is pending. Existing patches1640/pc_native_*script* refer
to original title timing/projectile behavior, not a Lua mod VM. No guest address,
generated C, pristine runtime vendor or gameplay wrapper was changed here. Plan
game hooks through the shared native interfaces and verify actual source/callers
before adding wrappers. All feature logic must remain common to all three hosts.

Unchanged workshop trust checks: tests/mods/test_catalog_trust.py and
test_native_trust_fixtures.py plus shared tests/launcher/workshop_trust.cs verify
Python-produced signatures on each native launcher; source/vector receipts and
cache inputs bind the corpus. See [trust](../development/mod-catalog-trust.md).
Producer/operator API and live scanner evidence are in mod-workshop.md/STATUS.

Source42828ec0:56 real worker checks pass on Windows/Mac/Linux; Mac/Linux each
pass56 ASan/UBSan worker checks. Selected suites Windows361/Mac363/WSL362 pass
(platform skips differ). Windows final worker recheck covers the GCC formatting
follow-up. SDK source packs/validates with README beside its payload tree. These
are VM/codec tests, not OS confinement or live gameplay acceptance.

Mac SDK has no closefrom: parent uses posix_spawn CLOEXEC_DEFAULT plus explicit
stdio dup actions/process group; worker closes startup FDs before lowering NOFILE.
Linux a668698f passes62 process/VM checks (one Windows-only skip); Mac initial
compile failure is being corrected and its validation remains pending.

Seatbelt deprecation suppression covers its paired error-free API only.
