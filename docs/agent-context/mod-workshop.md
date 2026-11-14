# Mod workshop foundation

Authoritative product scope, usage, trust boundaries and follow-up milestones:
[workshop development guide](../development/mod-workshop.md).

<!-- packet:start -->
Mod operator tools live in tools/mods/: bounded package validation, ClamAV
quarantine/review, immutable releases, scan expiry/revocation, loopback preview
and approved static exports. Public uploads/authentication, signed metadata,
scan isolation/freshness enforcement and scripting are still incomplete.
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
Actual modded gameplay requires a new game build and scene verification.

Checks: pytest tests/mods; Windows build-launcher.ps1 -Test; standalone
launcher-tests.exe --workshop (12 groups). Windows checks pass. Native Mac/Linux
execution, live ClamAV, public connectivity, full asset formats and gameplay
scripting remain acceptance work, not capabilities implied by these tests.
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
limits/timeouts fail closed. No CLI scan bypass exists. The scanner runs as the
operator and its database age is not enforced: production requires isolated
workers and freshness policy. Synthetic scanners exist only in tests.

Installations are separate immutable version directories. Texture-only mods can
be enabled in the native library; Lua, maps and audio remain unavailable for
activation. Public authenticated uploads, signed catalogs and distributed
revocation enforcement are incomplete. Shared Python has no OS adapter; native
Mac/Linux execution still requires actual host validation.

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
deployment, Tailscale bandwidth, native host acceptance and live ClamAV remain
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
Creator texture manifests now require keyed DDS names, a full mip chain, supported
BC1/2/3/BGRA8 formats, power-of-two dimensions up to 4096 and a 24 MiB payload cap.

Mac SSH access is now verified using an existing project alias. AGENTS.md hashes
match Windows. Its active checkout has unrelated STATUS edits and many untracked
cloud-conflict copies: preserve them and use a separate .nosync worktree. Caddy
and Python are present; Tailscale's command is a stale missing-app wrapper and
ClamAV is absent. No public hosting has been enabled.
