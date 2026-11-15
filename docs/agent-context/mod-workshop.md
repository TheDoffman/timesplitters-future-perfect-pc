Workshop discovery (feature/mod-platform, basebc67c1c9): shared ModWorkshopPage
adds content/creator filters, name/creator/numeric-version sorting, latest release
per ID by default and explicit older-release browsing. Browse renders at most20
cards per page; changed filters/catalog reset the page. Categories, expanded
payload bytes and exact dependencies come from the same strict manifest parser
used by native installation. Mixed packages match each content category. The
model supports all three launchers without a new platform adapter. Windows
behavioral tests pass; native Mac/Linux builds and visual checks pending. Public
HTTPS, creator UI, map/audio activation and the full release remain unfinished.

Gateway native canaries now verify it cannot directly read the public snapshot
tree, can connect only to fixed proxy destinations and denies source-bound TCP,
connectx/UDP connections to other ports. Shared publication supervision is bound
to both actual PID and repository/origin identity after extraction.

Public gateway preparation: hosting/Gatewayfile and gateway-service-mac.py bind
localhost8768, canonical HTTPS origin, fixed static8766/API8767 destinations and
known workshop routes. Download requests strip credentials/cookies; API scheme
headers are controlled by the trusted gateway. JSON and ZIP body bounds differ.
CaddyServer gateway role receives no snapshot/repository/key paths and its own
sandbox permits only those two proxy destinations. mod_service_watch.Supervisor
shares prior publication PID/origin-or-repository health, sampled RSS, fault and
agent generation; publication adapters preserve existing contracts. Actual private gateway routing, authenticated creator/reviewer flow, official
ClamAV clean/EICAR decisions, signed publication/revocation and native isolation
pass. Transport checks enforce header/framing/fixed/chunked body bounds. Actual
managed gateway/publication restart, memory/stall cancellation and scratch
recovery pass. Public TLS/internet and production identity still need acceptance.

Public-server native canaries also prove process-environment denial against an
owned ready native target with fabricated environment and a successful control.

Darwin large-file responses need a narrow network-outbound exception for local
server port8766 with NO supplied remote endpoint (sendfile response context).
Plain local-port allowance would also permit hostile source-bound connections;
the new rule requires NOT remote-ip. Native controls cover bound connect,
connectx and UDP sendto; integrated large catalog/package acceptance is required.

Public static-server confinement: CaddyServer now requires Mac Seatbelt and
mod_public_sandbox.profile (public tree/fixed config/system runtime reads, private
scratch writes, self-signals, hardware sysctls, explicit process-info denial and
localhost8766 inbound/bind only). New outbound destinations/fork/private-data access stay denied. Per-server
scratch is owner-marked and leased outside the writable work directory; guardian
inherits its lease while the server inherits only stdio. Reclaim requires exclusive
lease after guardian/server exit. Running scratch is sampled at256 entries/32MiB,
bounded cleanup unlinks owned output without following links; unknown/incomplete
stores are preserved. Actual confined Caddy signed flow,8MiB/range/HEAD, crash
recovery and managed restart/resource/stall acceptance pass with fixture keys;
production deployment remains pending (STATUS).

Service identity preparation: hosting/provision-service-mac.py defaults to a
read-only Directory Services plan. Explicit local administrator --apply creates
only a new locked _tsfpworkshop user/private group and root-owned service layout.
It starts no service/endpoint and migrates no data/secrets. Shell is false,
authentication disabled before numeric UID assignment, no admin membership, and
no password/token ceremony is performed. Unknown/partial installations are
preserved; completed receipts can verify repeat invocation. Privileged application
still needs real administrator execution and acceptance before deployment.

Publisher faults before server construction (including key admission) are now
recorded by the lock owner. A competing lock loser preserves the active owner
status. Diagnostic metadata never includes signing exception/key details.

Managed publication: live-publication-mac.py now generates private controller and
independent watchdog login agents. Managed startup waits for fresh matching
publication-descendants-v1 health bound to its actual PID before key admission.
The watchdog samples2GiB/eight-process descendant bounds and15s controller
heartbeat, retains the exact bounded fault reason and cancels by owned launchd label; controller also withdraws if watchdog
health expires. mod_host_watch.py shares the existing bounded numeric sampler
with scan-service-mac.py; scanner limits/protocol remain unchanged. Actual
temporary launchd memory/stall/restart acceptance passes with fixture keys and all
test jobs unloaded. Production deployment/identity/boot/public checks remain open.

Live publication: mod_live_publication.py/hosting/live-publication-mac.py poll only
approved availability plus cumulative revocations, withdraw old Caddy before
refresh, sign monotonically newer immutable exports and recheck moderation after
export. A30s scan-validity margin avoids expiry churn;30min refresh renews metadata.
Failures/recovery barriers stop downloads; no stale-export fallback. Private
hash-inventory receipts retain current/previous outputs, preserve unknown/changed
files and recover complete receipts after interrupted retention. Caddy gets only
a static tree/minimal environment and an owned parent-lifetime guardian lease.
Guardian default210s is unchanged; trusted publication callers can explicitly
select1..86400s leases. See [live publication](../development/mod-live-publication.md). This controller
passed actual private Caddy HTTP revocation/error/barrier and controller-death
acceptance with disposable keys. Persistent production service and internet
acceptance remain unfinished; see STATUS.

Definition maintenance: hosting/definitions-service-mac.py generates a six-hour
private login-agent job and fixed official FreshClam config. No upload-controlled
arguments, proxy, shell hooks, external mirrors or daemon mode are accepted. Native
lifetime guardian,180s controller timeout,1MiB output limit/hash-only diagnostics,
private non-overlapping lock and storage preflight bound maintenance. Definition
checks never bypass scanner freshness or approve uploads. Pinned88c9b3f7 private login-agent deployment and its initial official check plus
subsequent isolated scan passed. The six-hour interval has not elapsed; production
identity/boot acceptance remains pending. See mod-scan-service.md and STATUS.

Worker admission now also requires exact macos-seatbelt-v3; v2 is held even with
a healthy guardian heartbeat. Legacy approved v2 reports are withheld until rescan.

The v3 Seatbelt policy explicitly denies process-info* and permits only public
hardware/runtime sysctls. A native positive-control canary proved the prior
same-user environment exposure; v3 denies it with EPERM. Named sysctl denial
alone was insufficient on this Mac. Real ClamAV and launchd queue/review/rescan/
watchdog restart passed before pinned27b01f86 private re-deployment. Old holds and
source/plists/data remain preserved. Public operation is still unfinished.

Scan-service config-only tests use a private inert executable: hosted CI Python
can be group-writable and is correctly rejected by the production scanner policy.
The fixture fix preserves that policy and never executes the fixture.

Scanner controllers now require default SIGCHLD handling so another handler cannot
steal guardian exit status and turn an engine error into apparent success. The
native guardian resets its own SIGCHLD disposition before spawning its child.

Darwin process sampling accepts unrelated system UID -2 while enforcing the exact
service UID throughout the managed descendant subtree; live host exposed this
OS-format difference before agent re-enablement.

Scanner containment repair is implemented and the private Mac login-agent
installation upgraded to pinned27b01f86 after actual escaped-session/root-death,
RSS/wall cancellation and real AV acceptance. mod_scan_sandbox.py routes all
run/scan calls through receipt-verified scan_guardian_mac.c, built by
build-scan-guardian.py. Parent-pipe EOF/cancellation kills and reaps the owned
scanner PID across session changes; guardian runs outside the controller group,
passes only stdio and has its own210s fallback. Capture streams are bounded.
The watchdog counts the numeric PPID subtree and advertises guardian-descendants-v1;
legacy components fail admission. Historical disabled installation/evidence is
preserved. This remains private login-only; public service operation is unfinished.
See STATUS and the scan-service guide for evidence and deployment limits.

Mac scan service: hosting/scan-service-mac.py generates a scheduled worker and
independent launchd watchdog from private local config. Worker queue/rescan cycles
require matching fresh health and their own process group. Initial missing health
records a waiting state without touching repository data. Sampled descendant RSS/
process/wall budgets cancel by owned launchd service label, never sampled PID.
The native guardian survives child-group cleanup; worker/scanner alarms and CPU/fd/file limits
remain. Health files are bounded/atomic and retain last faults; unknown files
are preserved. See [scan service](../development/mod-scan-service.md). Login-agent
mechanism is separate from public deployment, boot/dedicated identity, definitions
and signed publication refresh; actual host acceptance must be recorded in STATUS.

Periodic maintenance: mod_rescan.py/rescan-next revalidates one due reviewed
immutable package under the Uploads worker lock. Fresh clean renews the prior
review; detection revokes; transient error withholds/retries; changed bytes need
manual repair/review. Slow AV holds no SQLite writer lock. Lease/state compare
preserves concurrent moderation and allows interrupted-scan reclaim. Backup
refuses active leases; restore clears renewal records. See
[scan maintenance](../development/mod-scan-maintenance.md). Private login-agent
supervision is installed; automatic signed refresh and public operation remain pending.

Private host recovery: tools/mods/mod_backup.py captures a reserved-lock SQLite
snapshot plus referenced immutable/queued bytes, with private ownership and bounded
file/disk budgets. Restore only creates a fresh root, revokes auth, suspends accounts,
expires open tickets and withholds previous approvals. .recovery-required blocks
API/catalog/approved bytes/approval/exports. Explicit local resume consumes an
operator reconciliation record for current origin/version floor and cumulative
revocations; unknown hashes persist in recovery_revocations and signed exports.
See [recovery workflow](../development/mod-host-recovery.md). No keys/secrets,
public endpoint, scheduled retention or automatic service cutover are supplied.

Installed now exposes optional gameplay mod storage through ModCacheLocation
(ModScriptLaunch.cs). This trusted local preference validates absolute local,
unlinked, existing UTF8 paths and is emitted only for a verified active mode.
Unavailable configured folders fail visibly; blank restores OS cache selection.
Native permission/filesystem checks remain authoritative. Uploaded packages cannot
write this Settings preference. Native shared UI checks cover Unicode round trips,
bad/missing locations, default restore and original-game recovery bypass.

Launcher gameplay activation: ModLibrary accepts one scripts/main.lua mode with
optional flat DDS textures, alongside texture packages, only when the installed
BUILD.json advertises mod_script_version=1. It rechecks workshop availability,
archive/profile bindings and source payload hashes, builds a canonical capsule
binding all selected package identities, and rechecks its active-generation hash
before each launch. GameSettings supplies the explicit ordinary save root; native
bootstrap selects separate ModSaves and offline-only mode. Enable never runs code.
Missing capability leaves scripts inactive; Play without mods bypasses the profile.
Native UI/source/capability/tamper/preservation checks are in launcher_shared_ui.cs.
Actual packaged-mode launcher handoffs now finish native offline matches on all
three hosts with ordinary saves preserved; evidence is in STATUS.md. These are
private fixtures, not public upload-to-play or release-package acceptance.

ScriptRuntime uses bounded background preflight and epoch-specific approval before MatchManager binds. Production startup/title/shutdown are wired; see mod-scripting.md.

Native RuntimeResources discovers OS-adjacent helper bundles and verifies private snapshots; strict private caches and bootstrap binding are wired. See mod-scripting.md.

Preflight also consumes unsupported fresh identities before any worker admission;
restoring their flags cannot start a mid-round script. See mod-scripting.md.

MatchManager owns one active/retiring match with reap-before-successor and no stale epoch reopen. Game auto-binding/resource discovery are wired; see mod-scripting.md.

Bootstrap reads Windows handoff/root environment through the wide CRT and strict UTF8 conversion; empty/oversized pairs fail closed. Native fixtures cover Unicode capsule and save paths. See mod-source-handoff.md.

Native mod_bootstrap now consumes an explicit developer capsule before kernel init and publishes source only after isolated storage commits. The launcher now emits verified generation capsules and the production manager auto-binds. tests/mods/test_mod_bootstrap.py covers real capsule/lease plus shared storage state; see mod-source-handoff.md.

# Mod workshop foundation

Authoritative product scope, usage, trust boundaries and follow-up milestones:
[workshop development guide](../development/mod-workshop.md).

<!-- packet:start -->
Mod operator tools live in tools/mods/: bounded package validation, ClamAV
quarantine/review, immutable releases, scan expiry/revocation, loopback preview
and approved static exports. Private WSGI accounts/owned quota uploads/serial
scan queue/moderator review are implemented. Public deployment, native creator UI,
production trust pins, persistent supervision and scripting remain incomplete. ClamAV enforces official-only
databases at most2 days old; legacy scan reports require a rescan/review.
The selected public host is the user's Mac mini; no domain purchase is required
for the planned Funnel trial. SSH works, but no public service has been deployed.

Shared launcher ModWorkshopPage.cs owns browsing/search/verified download and
installed controls. ModLibraryPage.cs owns immutable Packages, strict JSON/ZIP/
DDS validation, dependency/cycle/conflict guards, profile hashes and active
texture generations. Mutations reject a running game. Old versions support
rollback; unknown personal files and links are preserved. Texture-only mods and one main.lua mode can be enabled on capable builds.
Maps/audio/additional Lua remain inactive.
GameSettings.StartInfo(root,withoutMods) verifies the overlay for normal play
and bypasses even a broken profile for recovery, without changing that profile.

src/textures/texture_pack_io.h tries TSFP_MOD_TEXTURE_DIR before the official
keyed DDS path, on the existing bounded worker. Reader rejection falls back to
the base pack; GPU/alpha rejection retains the original view. Both RHI and
legacy GL share it. Enhanced/Classic gating, originals and saves stay intact.
Windows native install/version rollback/removal-to-profile-scene texture loading
and visual restoration pass. Story gameplay and Mac/Linux game scenes remain unverified.

Checks: pytest tests/mods; Windows build-launcher.ps1 -Test; standalone
launcher-tests.exe --workshop (19 groups). Current Windows/Mac/Linux launcher
checks pass; actual Mac Gunicorn-to-ClamAV/review and real signing/key-ceremony tests pass.
Public connectivity, full asset formats and scripting remain acceptance work.

Native Lua has VM/OS/session limits and original-title fixtures on all three hosts.
The game parent builds and stock Story scenes run; NULL mod binding remains the
default. Native match creation now requires trusted helper pins and verifies
private copies before launch. Pins must come from the game build, never uploaded
manifests. Production helper packaging, bootstrap/save policy and actual custom-
mode play remain required. See [worker and game bridge](mod-scripting.md).
<!-- packet:end -->

Production native helper staging lives in `tools/build/mod_runtime_images.py`;
see [image contracts](mod-scripting.md). Native builders compile pins and stage
helpers beside the game; public packagers copy verified helper-only trees.
Native release-package acceptance and bootstrap remain pending.
Mac worker builds pin the Apple Silicon OS floor; see the image contracts above.
Relocated production helpers run the original First to Three example in native
protocol tests; this is not a bound in-game custom match.
Native mod_saves.cpp leases persistent per-profile namespaces and rejects aliases
to personal saves. Bootstrap now fences storage; actual custom-mode and exFAT acceptance remain pending;
see [save-lease contracts](mod-scripting.md).
The shared storage fence prevents lazy default/profile escape once armed; trusted
mod bootstrap now registers it before kernel init. See [host storage](host-storage.md).
Production-helper tests find CMake beside a virtualenv interpreter or on PATH,
matching process tests; system Python installs place pip tools under Scripts.
ModScriptLaunch.cs/mod_capsule.cpp define a bounded source handoff and canonical
save identity, verified against one C#/native corpus. Caller package authorization,
storage fencing and actual activation remain required; see mod-source-handoff.md.
Local source: `tools/mods/mod_package.py`, `mod_repository.py`, `mod_server.py`,
`mod-workshop.py`. The CLI is directly executable; peer imports resolve from its
directory. Tests add `tools/mods` explicitly and use no private game files.
`examples/mods/workshop-demo` is original source, not recovered game content.

`tests/mods/test_mod_workshop.py` covers hostile archives, approval/expiry/
revocation, and real loopback HTTP downloads into inactive installations. It is
registered in the Windows/macOS native suite and Linux job. No live scanner or
game assets are needed. The ClamAV adapter has exit-code/limit/timeout fixtures;
these are not evidence of real malware detection.

Quarantine stores immutable digest-addressed blobs and SQLite release/event
records under the explicitly supplied private repository root. Only fresh clean
scans plus manual review appear in search/download. Missing scanner or scan
limits/timeouts/stale databases fail closed. No CLI scan bypass exists. The
scanner uses official signatures, a2-day maximum database age and no symlink
following. --clamav-database selects the same directory for identity and scan.
Reports without that age-policy marker require rescanning before publication.
mod_scan_sandbox.py uses a Mac deny-default Seatbelt process: only this content,
definitions, public runtime/certs and private temp are readable; only temp is
writable. No network or child exec. scan_child.py sets hard CPU150s, file512MiB,
fd128/core0 limits; parent enforces180s and kills diagnostic output over1MiB,
retaining at most64KiB. Any clean-exit diagnostic is withheld. Policyv2 invalidates
earlier scans; file bounds must accommodate ClamAV's512MiB scan budget.
Unsupported hosts fail closed, no bypass.
RAM supervision, service identity and production job queues remain incomplete.
Synthetic scanners exist only in tests; tests/mods/live_clamav.py explicitly
exercises real clean/EICAR/error/quarantine/review/revocation behavior.
tests/mods/live_scan_isolation.py checks disposable read/write/symlink/exec/
network canaries and inherited limits on the actual Mac host.
Its shell canary invokes /bin/bash directly: macOS27 /bin/sh execs that variant,
which the intentionally single-executable profile correctly rejects.
test_scanner_process.py uses real harmless children for flood/timeout/exit-code
bounds; Windows/Mac native suites and the Linux mod step register it. Scanner
tools are CPU-only scope inputs; this does not skip actual renderer tests.

Installations are separate immutable version directories. Texture-only mods can
be enabled in the native library; Lua, maps and audio remain unavailable for
activation. Public authenticated uploads, signed catalogs and distributed
revocation enforcement are incomplete. Shared Python has no OS adapter; native
Mac and WSL Linux shared UI/install contracts now pass; game sessions still need
validation on all three hosts.

## Creator service and parsing budgets

[Service contracts and operator commands](../development/mod-creator-service.md).
mod_accounts.py owns scrypt/invitations/revocable sessions/recovery/suspension and
rate/audit bounds. Audit actors use user:/system: namespaces to prevent name
collisions. mod_uploads.py reserves quotas, checks ownership again on
finalization, streams private uploads and serializes idempotent scanning/retries.
Moderation authorization, release approval/revocation and audit share a SQLite
transaction via Repository.approve_in_transaction/revoke_in_transaction. Failed
audit cannot publish. Existing operator releases require explicit owner adoption.
mod_api.py is a strict origin/scheme/Host/bearer WSGI API, with no quarantine file
route. mod_operator.py writes one-time invitations only to new private0600 files.
Gunicorn27.0.1 is hash-pinned separately; its profile binds127.0.0.1:8767 only.
Production factory rejects UID0/unsupported hosts before touching storage.
It is not wired to public Caddy. Actual Mac loopback Gunicorn/upload/ClamAV/
moderator/snapshot/revocation checks pass, then the owned listener closes; no
persistent API service or real accounts have been deployed.

Python/native preflight ZIP EOCD/actual central records before eager library
allocation; maximum2049 entries,180-byte names,4096-byte extras,1024-byte comments,
single disk, no SFX/global ZIP64. JSON is4MiB maximum,32 levels,4096 members per
collection and65536 value/key tokens; preflight precedes Python object creation.
Lua source is1MiB/file and map descriptors4MiB. Native ModJson numeric matching
uses the original string/start position instead of quadratic suffix copies.
New creator/API/parser tests run in Windows/Mac native suites and all tests/mods
under Linux CI. live_creator_service.py checks real Gunicorn+ClamAV on Mac;
live_scan_isolation.py compiles a C probe to verify hard limits/alarm across exec.
Public scheduling, worker RSS supervision, native credentials/UI, signed trust,
backups and public internet acceptance remain pending.

Producer trust: mod_trust.py signs exact-byte catalog digests with RSA3072/SHA256,
offline2-key root thresholds, sequential cross-signed rotation, expiry, monotonic
versions and permanent bounded revocations. mod_signing.py supplies Unix-private
key loading/ceremony and catalog-role-only online signing. Export reserves its
SQLite sequence before publishing. Host now requires an out-of-band root and
refuses unsigned previews. Native enforcement is described below; real production
keys are still absent. Wire/commands/bounds:
[catalog trust](../development/mod-catalog-trust.md). Real-crypto tests:
tests/mods/test_catalog_trust.py, registered in native suites; hash-pinned wheels
in hosting/requirements-signing.txt. Caddy allows only numbered public roots and
catalog trust metadata in addition to approved package routes. At b01ad2a5,
Windows282/Mac284/WSL283 selected checks pass (platform skips differ). Actual
Mac encrypted-root ceremony, private permissions, online export, Caddy signed
HTTP GET/private-route/write rejection and owned listener shutdown pass; no
public TLS or native-client trust is established.

Native trust: ModTrustPage.cs shares RSA3072 exact-byte verification and locked
atomic signed-state/root-chain persistence on all hosts. Initial roots are
compiled or explicitly installed out of band, never downloaded as a new pin.
ModWorkshopClient requires HTTPS, fetches bounded metadata/sequential root updates,
rejects expiry/replay/equivocation/forgotten revocations and commits before listings.
It rechecks after package transfer. Schema2 workshop receipts bind origin;
ModLibrary batches fresh availability/revocation checks per origin before install,
enable and normal game mount. Local/schema1 creator mods retain their existing
path. Disable all/uninstall/Play without mods remain recovery. Compiled root and
service remain blank; no production pin/public release. Shared workshop_trust.cs
checks public Python-signed vectors, persistence/corruption/rotation and actual
native model activation/revocation/expiry; producer also validates the vectors.

`mod_publication.py` exports approved/fresh content to a new separate static
snapshot (4MiB catalog, 1,000 releases, 512MiB package cap). Preflight rejects
unexpected files, links, changed packages and expiry. `hosting/host-mac.py`
preflights, starts Caddy on loopback8766 and stops it at snapshot expiry.
`hosting/run-mac-mod-host.sh` is the Mac entry point; `hosting/Caddyfile` exposes
only catalog/package GET/HEAD. No quarantine/database/upload/admin is public.
Manual revocations require stopping/switching an already exported snapshot.

Shared launcher source `tools/launcher/ModWorkshopPage.cs` owns both page and
bounded HTTPS client. Service is the compiled DefaultService or installation
`Support/Mods/service.json`; blank means unavailable. Native transport requires
HTTPS; redirects, external package URLs and script execution are refused.
Downloads recheck availability and write only verified hash-named ZIPs under
`.launcher/mod-downloads`, preserving changed cache files and removing owned
partial files. Shared UI checks cover catalog/transport/search/preview/digests/
removal. `launcher-tests.exe --workshop` runs19 focused Windows groups. The same
groups are included in ordinary shared UI checks on Windows/macOS/Linux. The
latest full Windows launcher command passes.

`--render-mods-preview` displays explicitly labeled example listings without
network or gameplay. WPF/Avalonia navigation uses ModsNav/ModsHost. Setup and
the no-purchased-domain Funnel trial: [Mac hosting](../development/mac-mod-hosting.md).
Mac SSH is now verified; its existing working files were preserved. Public
deployment, Tailscale bandwidth and game host acceptance remain
unverified.

## Texture library and runtime overlay

`ModLibraryPage.cs` owns the shared native-client package model (strict bounded
JSON with duplicate-key rejection, ZIP paths/types/count/size limits, payload
hashes and format validation). Install snapshots the ZIP before validation and
publishes an immutable `Mods/Packages/id@version` directory only when complete.
Its receipt and original ZIP manifest bind installed metadata. Non-texture
content can be installed but cannot be enabled yet.

`ModWorkshopPage` now has Browse/Installed views, Install, Enable textures,
Disable, Disable all, Uninstall and Play without mods. Model mutations use a
per-library exclusive lock and reject a running game. Exact dependencies,
cycles, duplicate enabled IDs, texture conflicts and a 256 MiB active-overlay cap
are checked before publishing an atomic profile. Version switches preserve old
packages; stale reproducible overlays are removed, while unknown personal files
and links block cleanup. Cancellation/failures keep the prior active profile.

Profiles store key+package-hash bindings and a generation under Mods/Active.
`GameSettings.StartInfo(root,withoutMods)` verifies active hashes before passing
TSFP_MOD_TEXTURE_DIR. The one-argument overload retains its prior contract.
Play without mods bypasses even a corrupt profile, clears inherited TSFP
settings through the existing environment sanitation, and does not rewrite it.

`src/textures/texture_pack_io.h` resolves flat lowercase 16-hex DDS names from
that overlay first, on the existing worker. Reader rejection/missing files
falls back to the official pack; renderer upload/alpha/resource rejection still
uses the existing original-view fallback. No official pack or game archive is
overwritten or duplicated. Existing Enhanced/Classic admission remains intact.
The RHI and legacy GL use this same reader. Actual in-game activation still
requires a newly built mod-capable game and live scene verification.

`test_texture_mod_mount.py` compiles the actual reader and checks precedence,
missing/invalid/aspect fallback, no-pack operation and creator DDS validation.
`launcher_shared_ui.cs` adds native package/activation/rollback/dependency/cycle/
conflict/tamper/cancellation/personal-file and recovery tests; 12 focused workshop
groups run with --workshop and with the ordinary three-host shared UI suite.
The POSIX C11 reader fixture declares `_POSIX_C_SOURCE=200809L`, matching its
use of the production trace clock; Linux CI exposed this missing test-build flag.
Creator texture manifests now require keyed DDS names, a full mip chain, supported
BC1/2/3/BGRA8 formats, power-of-two dimensions up to 4096 and a 24 MiB payload cap.

Mac SSH access is now verified using an existing project alias. AGENTS.md hashes
match Windows. Its active checkout has unrelated STATUS edits and many untracked
cloud-conflict copies: preserve them and use a separate .nosync worktree. Caddy
and Python are present; Tailscale's command is a stale missing-app wrapper and
ClamAV1.5.4 is installed with official databases in the workshop's private
Application Support/scanner directory. No background service or public hosting
has been enabled. Live repository acceptance is run separately from unit tests.

## Mac validation checkpoint f6770b2b

Source was pushed and checked out through Git in a separate .nosync worktree;
the primary Mac branch/edits were left intact. Native launcher build/sign/package/
tests passed using its existing pinned dotnet8.0.426 SDK, with60 shared UI groups
including the12 workshop groups.186 Python/C workshop/layout/CI checks passed
under the host's Python3.12.12 environment. Real Caddy2.11.2 loopback serving,
private/write-route rejection, expiry and listener shutdown were exercised.

Desktop output acquired FinderInfo/FileProvider metadata and failed signing
despite the builder's existing xattr cleanup. The same code passed with output
under ~/Library/Caches/tsfp-mod-platform/launcher. Keep generated Mac bundles out
of cloud-managed folders; no change to signing policy/keys was needed. This
verifies native launcher/service components, not actual game mods or public TLS.

Native trust source24236283: Windows66/Mac67/Linux68 shared UI groups pass.
Mac signing/package/post-test signature verification and Linux actual installed
package repair/restart/render preservation pass. Mac288/WSL287 selected Python
checks pass (one WSL non-root ceremony skip). Native source/vector receipts and
Windows build-cache inputs bind the golden data. Production pins/HTTPS identity
and public client acceptance remain unprovisioned.

Creator ZIPs use explicit Unix creator-platform metadata on every host. The SDK
round trip exposed different Windows/Unix archive hashes from ZipInfo defaults
despite identical payloads. Existing packages remain valid; new packs canonicalize
this header along with the existing fixed timestamp/file mode.

Native script process boundaries are implemented and checked on Windows/Mac/Linux;
see mod-scripting.md for exact scope, host evidence and remaining game integration.
The diagnostic runner is not a playable asynchronous game bridge. Script activation
stays unavailable; public release acceptance and helper crash recovery are pending.

Persistent native Session supervision/transaction validation is implemented;
see mod-scripting.md for bounded async API and current host evidence. Actual
game event/command adapters and script activation remain pending.

Session idle CPU uses bounded1s windows so legitimate paused-worker overhead
does not accumulate forever. Host-only sanitizer checks preserve sealed children;
see mod-scripting.md for actual host evidence and pending checks.

Renewed idle windows exposed Mac CPU undercounting: proc_pid_rusage CPU totals
are Mach ticks, not nanoseconds. The adapter now uses checked128-bit Mach timebase
conversion. Linux404 full checks/3 skips and56 host-ASan/UBSan Session checks pass;
Windows404 full checks/3 skips pass. Updated Mac rerun remains pending.

Corrected Mac Mach CPU conversion passes full/sanitizer checks. Test-host early
error paths now drain Session cleanup before exit; production crash-profile
recovery remains pending. See mod-scripting.md/STATUS for exact host counts.

Production startup/title factory now connects compiled helpers, bootstrap lease
probes, offline-only network policy, automatic match binding and HUD/shutdown.
See mod-scripting.md#production-game-factory for measured checks and pending scenes;
public launcher script activation and deployment remain unfinished.

Actual native Mac tick/First-to-Three and Linux tick matches now exercise the
production bridge. Explicit local cache selection and exec metering: mod-scripting.md.
Windows also completes First-to-Three; HUD and window-thread drain: mod-scripting.md.
