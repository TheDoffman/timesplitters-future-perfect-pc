# Mod workshop foundation

Authoritative product scope, usage, trust boundaries and follow-up milestones:
[workshop development guide](../development/mod-workshop.md).

<!-- packet:start -->
Mod operator tools live in tools/mods/: bounded package validation, ClamAV
quarantine/review, immutable releases, scan expiry/revocation, loopback preview
and approved static exports. Private WSGI accounts/owned quota uploads/serial
scan queue/moderator review are implemented. Public deployment, native creator UI,
signed metadata, persistent supervision and scripting remain incomplete. ClamAV enforces official-only
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
launcher-tests.exe --workshop (13 groups). Current Windows/Mac/Linux launcher
checks pass; actual Mac Gunicorn-to-ClamAV/review and141 Mac/WSL mod tests pass.
Public connectivity, full asset formats and scripting remain acceptance work.
<!-- packet:end -->

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

`mod_publication.py` exports approved/fresh content to a new separate static
snapshot (4MiB catalog, 1,000 releases, 512MiB package cap). Preflight rejects
unexpected files, links, changed packages and expiry. `hosting/host-mac.py`
preflights, starts Caddy on loopback8766 and stops it at snapshot expiry.
`hosting/run-mac-mod-host.sh` is the Mac entry point; `hosting/Caddyfile` exposes
only catalog/package GET/HEAD. No quarantine/database/upload/admin is public.
Manual revocations require stopping/switching an already exported snapshot.

Shared launcher source `tools/launcher/ModWorkshopPage.cs` owns both page and
bounded HTTPS client. Service is the compiled DefaultService or installation
`Support/Mods/service.json`; blank means unavailable. No HTTP except explicit
127.0.0.1, redirects, external package URLs or script execution.
Downloads recheck availability and write only verified hash-named ZIPs under
`.launcher/mod-downloads`, preserving changed cache files and removing owned
partial files. Shared UI checks cover catalog/transport/search/preview/digests/
removal. `launcher-tests.exe --workshop` runs 12 focused Windows groups. The same
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
