# Hosting the launcher workshop on a Mac mini

The selected deployment target is the owner's Mac mini, accessible over the
internet, with no purchased domain. **Mods are browsed and searched inside the
existing launcher.** A separate public website is optional.

This work prepares a download-only hosting alpha. SSH access to the Mac is now
verified; public access has not been enabled. Creator scanning/review stays private;
authenticated public uploads and isolated scan workers remain future work.
Downloads remain inactive until explicitly installed/enabled. Texture-only
activation is implemented in the native library and shared reader, with live
game acceptance pending. Maps/audio/scripts cannot be enabled yet.

## Layout and public address

```text
Player's launcher -- HTTPS --> Tailscale Funnel address
                                  |
                              Mac mini
                                  |
                         Caddy on 127.0.0.1:8766
                                  |
                   approved static publication snapshot

Private repository + quarantine + scans + moderation stay outside that snapshot.
```

[Tailscale Funnel](https://tailscale.com/docs/features/tailscale-funnel) supplies
a public HTTPS address under the Mac's `*.ts.net` name. Players do not need to
join the private network to use a Funnel address. No custom domain or router
port forwarding is required for this trial. Check the current account/plan's
eligibility before enabling it. Funnel has non-configurable bandwidth limits;
measure mod download demand before committing to it as the long-term delivery
service. A later HTTPS endpoint can replace it without changing mod IDs or
package hashes.

Keep the Mac powered on and connected while hosting. Home upload bandwidth,
storage space, service availability and moderation capacity limit this approach.
If the Mac is off or a publication expires, the launcher shows an unavailable
workshop while the installed game remains available.

## Prepare the Mac

Use a dedicated non-administrator service account and an explicit workshop
storage location. Keep personal files, game assets, credentials, saves and
quarantine outside the public directory. Do not expose the source checkout or
the local Python preview server through Funnel.

Install Python 3.12+, [Caddy](https://caddyserver.com/docs/install),
[ClamAV](https://docs.clamav.net/manual/Installing/Packages.html), and a
[Tailscale macOS variant with CLI/Funnel support](https://tailscale.com/docs/features/tailscale-funnel#requirements-and-limitations).
Follow the official installer instructions for the chosen packages. Do not copy
the Windows virtual environment to the Mac. Set up ClamAV's normal
[FreshClam database updates](https://docs.clamav.net/manual/Usage/SignatureManagement.html)
and verify a live scan before accepting community submissions. The project
adapter still does not enforce database-age policy or OS-isolate the scanner.

Get this branch onto the Mac through Git after it is committed/pushed. A shared
Codex account does not transfer local source files. Use the same root AGENTS.md;
preserve any Mac working-tree edits. The current Windows-only source edits do
not establish that the Mac has received or validated them.

The commands below run from the source checkout, with `python3` pointing to a
host-local Python 3.12+ environment. Select your own repository and snapshot
paths. The example locations are under Application Support and never contain
game assets:

```sh
workshop_storage="$HOME/Library/Application Support/TSFP Workshop"
mkdir -p "$workshop_storage"
python3 tools/mods/mod-workshop.py search --repository "$workshop_storage/private-repository"
```

This creates an empty private repository. Follow the
[creator and moderation workflow](mod-workshop.md#try-the-workflow) to submit,
scan, inspect and approve packages locally. An empty approved catalog can be
published to test connectivity; there is no need to fake a scan approval or
include game files for that test.

## Export only approved content

```sh
python3 tools/mods/mod-workshop.py export \
  --repository "$workshop_storage/private-repository" \
  --output "$workshop_storage/public-snapshots/first-alpha"
python3 tools/mods/mod-workshop.py check-public \
  "$workshop_storage/public-snapshots/first-alpha"
```

Choose a **new** output directory for each export. Existing snapshots are never
overwritten. The exporter copies only fresh, approved, hash-verified ZIP bytes
and public catalog metadata. No database, scanner logs, moderation notes,
unapproved packages, passwords or local filesystem paths enter the published
catalog. Limit: 1,000 releases, 4 MiB catalog and 512 MiB of package storage per
alpha snapshot. Public snapshots must be outside the private repository.

The publication expires when its oldest included scan reaches 24 hours, or
after 24 hours for an empty catalog. Rescan/review releases and export a new
snapshot before expiry. This is intentionally a manual alpha workflow; service
startup does not automatically approve, rescan or publish anything.

## Start the approved-download service

```sh
sh tools/mods/hosting/run-mac-mod-host.sh \
  "$workshop_storage/public-snapshots/first-alpha"
```

The wrapper validates the exact publication tree and starts Caddy on loopback
port 8766. It rejects links, extra/missing files, changed packages and expired
publications. It stops Caddy when that snapshot expires. It does not start
Funnel, purchase anything or change login/power/router settings. Use Ctrl+C to
stop it; if the supervising terminal is closed, verify the Caddy process stops
before assuming public access is disabled. A service-manager configuration with
reboot/recovery testing remains production follow-up work.

The supplied [Caddyfile](../../tools/mods/hosting/Caddyfile) serves GET/HEAD only:

- `/v1/catalog`: JSON catalog for the launcher.
- `/v1/packages/<sha256>`: ZIP attachment for one published release.
- Everything else: unavailable. There is no directory browser or upload/admin API.

Do not run the Caddyfile by itself with an unset/changed root. The wrapper's
preflight and expiry supervision are part of this alpha's serving contract.
There is no dynamic web application, package extraction, scripting or scanning
in the public request path. Caddy's
[static file server](https://caddyserver.com/docs/caddyfile/directives/file_server)
and [loopback binding](https://caddyserver.com/docs/caddyfile/directives/bind)
provide that small serving surface.

Check `http://127.0.0.1:8766/v1/catalog` on the Mac. Then, in a separate terminal,
authenticate/configure Tailscale as described in its documentation and run:

```sh
tailscale funnel --bg 8766
tailscale funnel status
```

The Funnel command can prompt for the owner's account approval. Use the
**actual HTTPS address it returns**; do not invent a hostname. Verify the
catalog from another internet connection before distributing it. See
[Funnel CLI documentation](https://tailscale.com/docs/reference/tailscale-cli/funnel)
for account setup, status and removal commands. To stop this specific mapping:

```sh
tailscale funnel 8766 off
```

## Connect the launcher

The shared Mods page reads `Support/Mods/service.json` relative to the chosen
installation root. This contains a public URL only, with no secrets:

```json
{"service":"https://ACTUAL-MAC-NAME.ACTUAL-TAILNET.ts.net"}
```

Replace the example with the verified address returned by Funnel. HTTPS is
required for internet services. HTTP is accepted only for the explicit
`127.0.0.1` local preview. The address must be an origin, with no credentials,
extra path, query or fragment. The catalog cannot redirect package downloads to
another server. The client refuses HTTP redirects, bounds every response,
filters expired/future scans, rechecks release availability before download,
and verifies the completed package's SHA-256.

For development, create that file inside the isolated launcher build directory
and open **Mods → Refresh mods**. For the eventual public release, set
`ModWorkshopClient.DefaultService` to the verified endpoint and rebuild/package
the launchers through the normal three-host release workflow. Players will then
open **Mods** and browse without entering a server address or installing Python,
Caddy, ClamAV or Tailscale. Only the Mac host needs those hosting tools.

The page searches names, creators and descriptions, shows scan/review context,
and downloads to `.launcher/mod-downloads/<sha256>.zip`. Install package writes
an immutable version under Mods/Packages, initially disabled. Installed view
can enable texture-only overlays; scripts cannot be enabled. Existing downloads
with a mismatched digest are preserved
and reported; cancelled/failed downloads remove only their own partial file.
HTTPS and a hash do not establish publisher authorship. Signed catalog metadata
and authenticated publisher ownership remain required before script execution
or automatic activation is enabled.

## Updating and revoking a publication

1. Stop the hosting wrapper or disable its specific Funnel mapping.
2. Revoke affected releases in the **private** repository, or rescan/review
   releases for a fresh publication. Revoking in SQLite alone does not update
   an already exported static snapshot.
3. Export to a new snapshot path, preflight it, and restart the wrapper against
   that new path. Re-enable the mapping if it was disabled.
4. Check from outside the home network that removed releases are absent from
   `/v1/catalog` and their package URLs return unavailable.

Keep old snapshots outside all served roots for operator recovery. Previously
downloaded files cannot be recalled; a future active-mod manager must enforce
revocations separately before loading code. Do not mount old snapshots at
alternate public paths. Backup the private repository and event history without
making them public.

Before opening public creator submissions, implement ownership/authentication,
upload quotas, isolated scan workers, database freshness enforcement, moderation
and report handling. The download-only server supplies none of those endpoints.
The full [workshop implementation plan](mod-workshop.md#full-product-implementation-plan)
tracks these remaining milestones.
