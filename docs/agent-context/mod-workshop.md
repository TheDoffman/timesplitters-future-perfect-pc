Native RuntimeResources now discovers OS-adjacent generated helper bundles and verifies private snapshots; PrivateDirectory creates strict-permission caches independently of saves. Bootstrap binding remains pending; see mod-scripting.md.

MatchManager now owns one active/retiring native match with reap-before-successor and no stale epoch reopen. Game auto-binding/resource discovery is still pending; see mod-scripting.md.

Bootstrap reads Windows handoff/root environment through the wide CRT and strict UTF8 conversion; empty/oversized pairs fail closed. Native fixtures cover Unicode capsule and save paths. See mod-source-handoff.md.

Native mod_bootstrap now consumes an explicit developer capsule before kernel init and publishes source only after isolated storage commits. The launcher still emits no scripts and no custom-mode manager starts yet. tests/mods/test_mod_bootstrap.py covers real capsule/lease plus shared storage state; see mod-source-handoff.md.

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
rollback; unknown personal files and links are preserved. Only texture-only
mods can be enabled. Maps/audio/scripts can be installed but never executed.
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
