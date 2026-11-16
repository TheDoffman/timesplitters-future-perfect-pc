# Hosting the launcher workshop on a Mac mini

The selected deployment target is the owner's Mac mini, accessible over the
internet, with no purchased domain. **Mods are browsed and searched inside the
existing launcher.** A separate public website is optional.

The production workshop is deployed through a confined gateway, with authenticated
creator uploads, isolated scanning, manual review, signed publication/revocation
and native launcher enforcement. The public origin is
`https://dans-mac-mini.tail7e8ab8.ts.net:8443`; separate private443 routing remains
private. Public native installed-game lifecycle checks pass on Windows/Mac/Linux.
Use the [operator checklist](mod-workshop-operations.md) for the live host and
[STATUS](../project/STATUS.md) for exact deployed identities and remaining gates.

The production public root and origin are source-pinned in
`config/mods/public-workshop-root.json` and `public-workshop.json`. The profile is
active after public acceptance. Fresh native clients use the compiled origin and
root without local Support/Mods files. To stage an explicit isolated profile, use:

```sh
python tools/mods/hosting/prepare-launcher-workshop-profile.py --output /new/private/client-profile --apply
```

The tool verifies the public fingerprint, threshold signatures, origin and current
root lifetime, then writes service.json and trusted-root.json for Support/Mods,
plus an operator receipt. It refuses an existing output. These are public data;
no signing keys are included. Pending profiles require the separate private
acceptance mode; the current active profile does not. Published client artifacts
containing these verified defaults remain a release gate.

Texture overlays and one restricted gameplay mode can be enabled by compatible
native launchers. The packaged First to Three example has completed real matches
through launcher-generated handoffs on Windows, Mac and Linux, with ordinary saves
preserved. Public upload/review/install/update/rollback/withdrawal gameplay also
passes on all three hosts. Original PCM audio and Lua audio commands pass native
launcher-to-game checks with the new audio capability; see
[audio scripting](mod-audio-scripting.md). Maps and unlinked additional Lua files
remain unsupported. See [workshop scope](mod-workshop.md)
and [acceptance evidence](../project/STATUS.md).

The current production runtime is root-owned and uses the locked UID/GID600
identity through nine system jobs. Private readiness, fresh scanner controls and
seven restarts pass; physical unattended boot and continued operational acceptance
remain open. Earlier owner-login agents and temporary download wrappers are
development history, not the current production service. Scheduled backups are
prepared and still awaiting protected admission; off-device automation is pending.
See [scanner operation](mod-scan-service.md) and [current status](../project/STATUS.md).

## Layout and public address

```text
Player's launcher -- HTTPS:8443 --> Tailscale Funnel
                                          |
                                  gateway127.0.0.1:8768
                                   |              |
                        static8766              creator API8767
                    approved snapshot       private account/upload data

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

The preparation/export/wrapper sections below describe isolated development or
new-host staging. They are not commands to rerun against the installed production
repository. Do not replace its sealed runtime, start duplicate owner-login agents
or change its routing using these earlier fixtures. See the operator checklist
and protected hosting/recovery contracts for the live installation.

Source is synchronized through Git into a separate `.nosync` task worktree. The
native Mac launcher build/sign/package/tests passed at f6770b2b, as did package/
reader checks and a real Caddy loopback/expiry test. These results do not establish
public HTTPS access or live modded gameplay. ClamAV1.5.4 is now installed with
official FreshClam databases in the private workshop scanner directory. No
scanner daemon was started. Tailscale1.104.7 is installed in /Applications; its
official ZIP checksum, strict/deep Developer ID signature and Apple notarization
were verified (Tailscale Inc., team W5364U7YZB). The app was opened in the
background. Later read-only CLI checks confirm signed-in/online operation.
An existing private Serve mapping occupies HTTPS443. Preserve it; the workshop
uses a separately selected free port such as8443. A temporary8443 Funnel passed public-relay/TLS/marker verification, then was
removed with the original routing restored; no production mapping is active. See the [official setup](https://tailscale.com/docs/install/mac).

On this FileProvider-managed Desktop, the generated app acquired FinderInfo and
code signing failed despite the builder's existing metadata cleanup. The same
source passed with generated outputs under Library/Caches:

```sh
python3 tools/build/build-mac-launcher.py \
  --dotnet /path/to/the/pinned/dotnet \
  --output "$HOME/Library/Caches/tsfp-mod-platform/launcher" --test --package
```

Use the SDK version pinned in `config/build/mac-launcher-sdk.json`. Keep source
on its task branch and generated app/package output outside cloud-managed paths.
Apple documents the
[FinderInfo/resource-fork signing restriction](https://developer.apple.com/library/archive/qa/qa1940/_index.html).

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
adapter enforces official signatures, a maximum2-day database age and a Mac
deny-default scanner sandbox. Pass `--clamav-database` explicitly for private
host databases and run `tests/mods/live_clamav.py` for real engine acceptance.

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

First follow the [offline signing ceremony](mod-catalog-trust.md). The
`trusted_root` and `catalog_key` variables below must point to the independently
verified bootstrap root and the host's dedicated private catalog key. Install the
hash-pinned signing dependencies in the selected host environment. No real root
has been provisioned yet. The launcher verifies signed metadata, but production
pin provisioning and public internet/client acceptance remain release gates.

```sh
python3 tools/mods/mod-workshop.py export \
  --repository "$workshop_storage/private-repository" \
  --output "$workshop_storage/public-snapshots/first-alpha" \
  --trusted-root "$trusted_root" --signing-key "$catalog_key"
python3 tools/mods/mod-workshop.py check-public \
  "$workshop_storage/public-snapshots/first-alpha" --trusted-root "$trusted_root"
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
  "$workshop_storage/public-snapshots/first-alpha" "$trusted_root"
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
- `/v1/trust/catalog` and `/v1/trust/roots/<version>`: signed metadata and public roots.
- `/v1/packages/<sha256>`: ZIP attachment for one published release.
- Everything else: unavailable. There is no directory browser or upload/admin API.

Do not run the Caddyfile by itself with an unset/changed root. The wrapper's
preflight and expiry supervision are part of this alpha's serving contract.
There is no dynamic web application, package extraction, scripting or scanning
in the public request path. Caddy's
[static file server](https://caddyserver.com/docs/caddyfile/directives/file_server)
and [loopback binding](https://caddyserver.com/docs/caddyfile/directives/bind)
provide that small serving surface.

The static server alone is not the complete workshop ingress. Prepare and verify
the [confined gateway](mod-public-gateway.md) and its private API backend first.
Inspect existing Serve/Funnel mappings before enabling public access. Do not
replace a private443 mapping or use a global reset. Select a free supported HTTPS
port and preserve all existing routes and their private/public visibility.

After production gateway acceptance, an example mapping on unused8443 is:

~~~sh
tailscale funnel --https=8443 --bg http://127.0.0.1:8768
tailscale funnel status --json
~~~

The command can require the owner's browser approval. Use the actual HTTPS
address it returns, including the port. Verify catalog/package/account behavior
from an independent internet connection and confirm existing443 routes remain
private before distributing the launcher. See the
[Funnel CLI documentation](https://tailscale.com/docs/reference/tailscale-cli/funnel).
To remove only this mapping, use its matching port flags:

~~~sh
tailscale funnel --https=8443 off
~~~

For a temporary connectivity test use a foreground, bounded process and only a
non-sensitive health backend; verify cleanup and the original routing afterward.
Do not expose the operator preview, quarantine, source checkout or personal apps.

## Connect the launcher

Current source clients use the compiled production origin/root by default. An
explicit custom service can use `Support/Mods/service.json` relative to the chosen
installation root, with its own explicitly configured trusted root. This public
URL contains no secrets:

```json
{"service":"https://ACTUAL-MAC-NAME.ACTUAL-TAILNET.ts.net:8443"}
```

Replace the example with the verified address returned by Funnel. HTTPS is
required for native workshop services. The Python loopback preview is a separate
developer tool. The address must be an origin, with no credentials,
extra path, query or fragment. The catalog cannot redirect package downloads to
another server. The client refuses HTTP redirects, bounds every response,
verifies signed metadata and saved trust state, filters expired/future scans,
rechecks availability before/after download, and verifies completed package hashes.

For development, create that file inside an isolated launcher build directory
and open **Mods → Refresh mods**. Production defaults are already source-pinned;
final publication must package the verified source through the normal three-host
release workflow. Players will then
open **Mods** and browse without entering a server address or installing Python,
Caddy, ClamAV or Tailscale. Only the Mac host needs those hosting tools.

The page searches names, creators and descriptions, shows scan/review context,
and downloads to `.launcher/mod-downloads/<sha256>.zip`. Install package writes
an immutable version under Mods/Packages, initially disabled. Installed view
can enable texture overlays and one restricted gameplay mode on capable game
builds; audio also requires `mod_audio_version:1`. Existing downloads
with a mismatched digest are preserved
and reported; cancelled/failed downloads remove only their own partial file.
HTTPS and a hash do not establish publisher authorship. Signed catalog metadata
and authenticated publisher ownership remain required before script execution
or automatic activation is enabled.

## Updating and revoking a publication

Use authenticated moderation to withdraw a release. The deployed publication
controller invalidates/rebuilds the signed approved snapshot; verify the new
signed catalog and package404 from outside the home network. Do not manually
edit the static snapshot or restart the earlier development wrapper on production.
See [withdrawal and incidents](mod-workshop-operations.md#review-reports-and-withdrawal).

Keep old snapshots outside all served roots for operator recovery. Previously
downloaded files cannot be recalled; the native active-mod manager enforces
signed revocations before modded launch. Do not mount old snapshots at
alternate public paths. Backup the private repository and event history without
making them public.

Ownership/authentication, quotas, confined scanning, periodic updates/rescans and
moderation/reporting are implemented in the full service. The old download-only
development wrapper supplies none of those API endpoints. The
[full completion evidence](../project/mod-workshop-acceptance.md) tracks remaining
operational, content and release acceptance.
