# Community mod workshop

The intended product lets creators package additions, submit them to a hosted
workshop, and lets players search, download, manage and play those additions
through the launcher. Both assets and gameplay scripting are in scope.

## Current implementation

This branch contains a **local developer prototype**, not a released mod system.
It provides creator packaging, bounded archive validation, a ClamAV adapter,
quarantine/review states, a searchable catalog, a local browser, downloads,
revocation and atomic inactive installation. The existing launcher now has a
shared **Mods** page for search, verified downloads, native installation and an
Installed view with texture enable/disable, version switching and uninstall.
Players use its
existing .NET runtime and do not need Python. Operator tooling uses the Python
standard library for packages and requires Python 3.12 or newer; publication
signing uses pinned PyCA cryptography. It has no paid API.
The private creator/moderator API additionally uses a separately pinned Gunicorn
host runtime. Accounts, ownership, quota-reserved uploads, a serial scan queue
and authenticated review are now implemented. The native Creator tab supports
sign-in/invitations, reviewed ZIP uploads, status history and optional OS-vault
sessions; native moderation/reporting are implemented. Windows vault proof passes;
Mac/Linux persistence acceptance and public deployment remain incomplete. See [creator service](mod-creator-service.md).
Signed static exports, offline root rotation and monotonic publication versions
and shared native signature/checkpoint/revocation enforcement are implemented.
Production pins and public acceptance are still pending. See
[catalog trust](mod-catalog-trust.md) before hosting or generating any real keys.

Texture-only packages can now be enabled in a separate active overlay. The
shared runtime reader loads those DDS replacements before the existing pack,
with reader fallback and the existing Enhanced/Classic switch. Windows profile-
menu visual loading/update/rollback/removal is verified; Story gameplay and
other game hosts remain unverified. One gameplay mode containing exactly
`scripts/main.lua` and optional textures can now be enabled when the installed
BUILD.json advertises `mod_script_version: 1`. Additional enabled texture packages
are included in the mode's isolated save identity. Each launch revalidates the
packages, workshop trust and source capsule. Choose offline Arcade Deathmatch
and start a new match; scripts cannot access online play and use separate saves.
Only one gameplay mode can be selected. Older builds keep scripts inactive.
Installed also provides an optional Gameplay mod storage folder. Leave it blank
for the normal OS cache. If that cache has unsafe permissions, choose an existing
local folder writable only by your account (plus system administrators). The
native runtime still rejects shared, linked or unsupported filesystems; selecting
a folder does not change its permissions. Helper files use this location; mode
saves remain in their separate namespace. Missing configured folders stop modded
launches; Play without mods still works. This preference is stored locally in
Settings/mod-cache-location.txt and never comes from a downloaded package.
Maps, audio and unlinked additional Lua files remain inactive; formats/APIs remain
drafts. Creators can link a modular Lua project into the supported single entry
source using `pack --modules`, described below.
Native game bootstrap, helper confinement and gameplay hooks have been tested on
all three hosts through developer handoffs. Packaged First to Three has completed real offline matches through launcher-
generated handoffs on all three hosts with ordinary saves preserved; public
creator upload-to-launcher-to-game acceptance remains pending. No public infrastructure,
production signing keys or usable scripting release has been deployed. The selected host is the owner's Mac
mini, over the internet without a purchased domain. See the
[Mac hosting setup](mac-mod-hosting.md) for the approved static-download alpha,
public address and launcher connection. The local Python preview remains local.
The full goal's [acceptance matrix](../project/mod-workshop-acceptance.md) tracks
unfinished scripting, asset types, hosting/security and release requirements.

## Use the native launcher library

Open **Mods → Browse**, select **Install package**, review the exact required
versions, and confirm the download. Existing matching dependencies are verified
and reused. Cancellation can leave completed packages installed but inactive;
retrying verifies those packages rather than downloading them again.

In **Installed**, choose Enable on a supported texture package or gameplay mode.
Review the selected package and its installed dependencies together, including
any active versions that will be replaced, then confirm. Unrelated active mods
are kept. Missing dependencies, conflicting payloads, incompatible retained mods
and stale reviews stop the change. Installation does not automatically enable a
mod. Gameplay modes need a compatible game build, offline Arcade Deathmatch and
a new match; they use separate progress.

Use **Installed → Check for updates** to refresh the signed workshop catalog.
A newer version offers **Review update** on the newest installed version from
that workshop. Local packages and packages from another workshop address are
not automatically associated. Review and install the update's dependencies,
then enable the new version through its separate activation review. The old
active profile and installed version are kept until that switch. To roll back,
enable a retained older version and review its dependency changes. Uninstall
is available for disabled versions that other installed packages do not require.
Textures require Enhanced mode and a game built with this branch's overlay
reader; an existing published game does not gain support by updating only its
launcher. Public creator-to-launcher-to-game acceptance is still pending.

Enabled packages are pinned by their ID, exact version and SHA-256. Conflicting
texture keys, dependency cycles, missing exact dependencies and two active
versions of one ID are rejected. Changing versions retains older installed
packages for rollback. Texture activation uses a bounded separate cache and
never copies or overwrites the official texture pack. Unknown personal files
inside a version directory stop uninstall until the owner moves them aside.

**Disable all mods** preserves installed packages. **Play without mods** skips
even a corrupt profile without rewriting it. Maps and audio can be installed
but cannot be activated yet. Gameplay modes remain inactive on older game builds.
The CLI `install` below is an operator/testing staging command; the player UI
uses `Mods/Packages`, `Mods/profile.json` and `Mods/Active` instead.

## Package a modular Lua project

The SDK can link up to32 creator-owned Lua files into one `scripts/main.lua`,
using a package-local `require("rules.settings")` for
`scripts/rules/settings.lua`. Build the original modular sample with:

```console
python tools/mods/mod-workshop.py pack examples/mods/modular-first-to-three build/modular-mode.zip --modules
python tools/mods/mod-workshop.py validate build/modular-mode.zip
```

Keep the output outside your source folder and use a new filename for each build.
Set the manifest author to your workshop username before uploading. Without the
flag, `pack` preserves the original package layout; it does not silently link
files. The generated source contains all modules and stays inside the existing
1MiB source limit. It uses the existing version1 capsule and restricted VM;
players need no extra interpreter. A lone main source retains its original bytes.

Imports are exact, case-sensitive dot names relative to `scripts/`, with no
filesystem, dependency-package, native-library or network search. Values are
cached per VM; nil becomes true. Missing/cyclic imports fail the mode. Modules
share one VM and its existing budgets/permissions. Names beginning `__tsfp_` are
reserved by the linker. Packaging never executes Lua; syntax/logic still require
VM/game validation. See the [modular sample](../../examples/mods/modular-first-to-three.md).
Actual all-host gameplay of the new sample and the full public flow remain
acceptance work. Maps/audio loading, richer game APIs and a stable SDK release
remain part of the full goal.

## Try the workflow

Run from the source root. Use `.venv/Scripts/python.exe` on Windows or your
host-local Python environment on macOS/Linux. These examples use `python` for
readability. The example contains original source only, with no game assets.

```console
python tools/mods/mod-workshop.py pack examples/mods/workshop-demo build/workshop-demo.zip
python tools/mods/mod-workshop.py validate build/workshop-demo.zip
python tools/mods/mod-workshop.py submit build/workshop-demo.zip --repository private/mod-workshop --clamav-database /path/to/official/databases
```

Submission requires a trusted `clamscan` executable and working virus databases.
Use `--clamscan PATH` if it is not on PATH. There is **no skip-scan option**.
The isolated scanner currently runs on the Mac host. Use `--clamav-database PATH`
to select the required operator-owned database directory for
both version identity and scanning. Official signatures are required; databases
older than2 days cause an error. Directory/file symlinks are never followed.
Maintain definitions with the scanner's normal updater. The CLI returns exit 2
for detections, scanner errors, missing databases or timeouts. A clean scan
produces `pending-review`; it never automatically publishes the package.

Inspect the returned SHA-256 and review the actual package contents before
approving that exact immutable release. Replace `DIGEST` below with that hash:

```console
python tools/mods/mod-workshop.py inspect DIGEST --repository private/mod-workshop
python tools/mods/mod-workshop.py approve DIGEST --repository private/mod-workshop --note "Reviewer name: content and authorship reviewed"
python tools/mods/mod-workshop.py search demo --repository private/mod-workshop
python tools/mods/mod-workshop.py serve --repository private/mod-workshop
```

Open the printed loopback URL, normally `http://127.0.0.1:8765`, to search and
download. The browser renders creator text as text rather than HTML. The HTTP
preview is deliberately read-only and bound to loopback. It refuses unexpected
Host headers and has no CORS or HTTP upload/admin endpoint. Do not expose it
through a tunnel or reverse proxy as a production service.

In another terminal, download and stage an approved package:

```console
python tools/mods/mod-workshop.py fetch-install --server http://127.0.0.1:8765 --sha256 DIGEST --mods private/mod-install-test
```

For a package downloaded through the browser, use:

```console
python tools/mods/mod-workshop.py install build/workshop-demo.zip --sha256 DIGEST --mods private/mod-install-test
```

The installer writes `MOD-ID@VERSION/package.zip`, `content/` and `receipt.json`
under the chosen mod directory, then renames the completed staging directory
into place on the same filesystem. It does not overwrite an existing version.
It preserves old versions for rollback, verifies dependencies' package hashes,
and requires their exact versions to be staged first. It never executes an
installer, modifies Data/Settings/Saves/Textures, or enables a mod. A hash verifies
the bytes against the chosen digest; it is not proof of safety or authorship.

Installation roots and repository storage must be owned by the operator/player,
without untrusted concurrent filesystem writers. Local administrators can alter
their own database/receipts; this prototype does not defend against that.

```console
python tools/mods/mod-workshop.py revoke DIGEST --repository private/mod-workshop --note "Reviewer: reason for removal"
python tools/mods/mod-workshop.py rescan DIGEST --repository private/mod-workshop
```

Revocation removes search/download availability and cannot be undone with
approve/rescan. Already downloaded copies remain on disk. Rescanning a
non-revoked release returns it to review, even when clean. Approvals, search
results and downloads require a clean scan less than 24 hours old; expired
releases disappear until rescanned and reviewed. The report records the scanner
version/database identity and enforced2-day definition-age policy. Legacy
reports without that policy and the current isolation marker require rescan and review. Automated database
updates and rescan scheduling remain production prerequisites.
Search matches ID, name, author and description. The native launcher paginates
results and includes older approved versions when selected; the operator preview
retains its simple list. Relevance ranking and version-channel selection remain
future work.

## Package contract

A ZIP contains `mod.json` at its root and only the payload files declared in
`files`. `pack` fills that field with SHA-256 and uncompressed size per file.
The example manifest is [here](../../examples/mods/workshop-demo/mod.json).
IDs use lowercase words separated by hyphens. Versions are exact
`major.minor.patch`; the initial dependency format maps IDs to exact versions.
An ID/version pair is immutable, including rejected submissions. Updates use a
new version. The prototype trusts its local operator to assign IDs; it does not
establish publisher identity or enforce creator ownership.

| Directory | Accepted payload | Current game support |
| --- | --- | --- |
| `textures/` | Flat lowercase 16-hex `.dds` keys; supported formats, bounded dimensions/bytes and full mip chain | Native library activation and shared reader implemented; actual modded gameplay verification pending |
| `audio/` | `.wav`, `.ogg`, with container signature checks | Staged only; replacement hooks not implemented |
| `maps/` | `.json` objects | Draft container, not a recovered/importable game map format |
| `scripts/` | UTF-8 `.lua` source | Stored only; no Lua VM or gameplay API implemented |

Unknown manifest fields, undeclared files, duplicate JSON keys, duplicate/case
colliding names, path traversal, absolute/drive/UNC paths, NTFS alternate streams,
reserved Windows names, special files, symbolic links, directory records,
encrypted ZIPs, unsupported compression and compiled Lua are rejected. Native
executables are excluded by extension policy and common executable-header checks.
Type sanity is intentionally separate from full decoder/game-format validation.

Limits: 2,048 payload files, 64 MiB per payload (Lua1MiB, map JSON4MiB), 256 KiB
manifest, 256 MiB expanded payload/archive. ZIP central-directory and JSON
allocation budgets apply before general decoders; see the
[bounded parsing contract](mod-creator-service.md#bounded-parsing). Actual decompressed sizes, CRCs and hashes
are checked. Native EXE/DLLs, shell scripts, Python, installers and nested ZIP
payloads have no accepted role. No package path can name a game/save/config file.

## Upload and trust model

```text
Creator source -> deterministic package -> private snapshot -> format/hash checks
  -> normalized temporary files -> antivirus scan
     -> detection: rejected
     -> unavailable/error/timeout/limit: scan-error
     -> clean: pending-review -> operator review -> approved catalog/download
Approved release -> scan expiry or rescan: withheld until fresh review
Any release -> revocation: permanently unavailable from this repository
Download -> expected hash -> validate again -> install inactive beside prior versions
```

The scan adapter scans normalized extracted contents, invokes the scanner
without a shell and treats any exit except the scanner's clean result as a
block. It explicitly enables scan-limit alerts; otherwise large files can be
skipped. A 180-second scan timeout blocks publication. SQLite keeps release
state and an append-only operator event history; blobs are content-addressed.
The repository directory, database and scanner configuration are trusted.
The Mac scanner runs in a deny-default Seatbelt profile. Its readable inputs are
the extracted upload, official database/certificates and public runtime libraries.
Only a private temporary directory is writable. Network and child execution are
denied; inherited environment variables and file descriptors are removed. Hard
CPU150s, per-file512MiB, descriptor128 and core0 limits apply before exec; the
parent kills the engine after180s or diagnostic output over1MiB, retaining only
64KiB in memory. The file limit accommodates the engine's512MiB scan budget;
engine diagnostics with an otherwise clean exit also require operator repair.
Isolation policyv2 requires older reports to be rescanned. Each scan uses a fresh process. Unsupported
hosts or broken sandbox startup fail closed, with no unsandboxed CLI fallback.
Memory supervision, queue/global concurrency limits and a dedicated production
service identity still need integration. A parser sandbox does not prove that
the contents of a clean package are safe to execute.
Only local trusted operators should use this prototype. Production uploads must
run in a disposable worker with bounded memory/CPU/disk and no credentials or
network access to internal services.

Antivirus is one layer. It cannot certify a mod free of malicious behavior or
protect a vulnerable game decoder by itself. These boundaries follow the
[OWASP upload guidance](https://cheatsheetseries.owasp.org/cheatsheets/File_Upload_Cheat_Sheet.html).
Scanner limits/alerts are defined in the
[ClamAV scanner source/manual](https://github.com/Cisco-Talos/clamav/blob/main/docs/man/clamscan.1.in).
Do not label results “virus-free” in the eventual player UI; show scan time,
engine, review state, permissions and the report button.

## Full product implementation plan

### 1. Shared game loader and creator SDK

Define a versioned native mod API in shared source and connect it with
reproducible patches, not edits to generated guest C or the pristine snapshot.
First integrate texture-key overrides through the existing `src/textures/`
contract. Establish evidence for audio/map/gameplay entry points through
`tools/repo-context.py find` and bounded isolated game tests before specifying
their formats. Asset imports need decoded-size, dimensions/channel-count and
format validation before exposing native decoders to community files.

Ship a creator CLI, starter projects, API documentation, map/content importers,
validation diagnostics, local test profiles, and redistributable examples.
Creator source and compiled packages must contain only distributable additions;
the player's imported original game remains required. Map importing, characters,
weapons, animations and game-mode creation are separate acceptance milestones,
not capabilities implied by accepting an archive directory.

### 2. Sandboxed gameplay scripts

Select and pin the scripting engine after a small shared runtime proof. Lua
source is the proposed creator-facing format, not a committed working ABI.
Expose game operations through a narrow versioned API: session/map lifecycle,
bounded events, entity handles, validated spawn requests, scoring, game-mode
state and HUD messages. Never expose host pointers or arbitrary guest-memory
reads/writes. Reject stale handles and validate every host call's arguments.

Remove filesystem, shell/process, networking, native modules/FFI, debugger and
dynamic code-loading access. Allow persistence only through a quota-limited
namespaced mod-storage API. Bound memory, instructions per tick, event queue,
entity/spawn count and host-call time; budget exhaustion must terminate execution
even if a script catches errors. Disable an offending mod with a useful error
and keep the original game launchable. Script failures must not corrupt saves.

Run untrusted code in an isolated worker wherever practical; use host adapters
for Windows/macOS/Linux restrictions and a shared message/API contract. Benchmark
the effect on frame timing before choosing synchronous gameplay calls. A process
alone is not a sandbox: prohibit inherited handles, credentials and unrestricted
OS access. Complete a hostile-script test suite before enabling any script by
default. Static keyword blacklists and antivirus do not implement this sandbox.

For multiplayer, the server owns gameplay state. Negotiate the exact ordered
mod/version/hash set and API version before joining; distinguish cosmetic-only
mods from gameplay changes. Do not grant remote players automatic script
execution or downloads. Handle dedicated-server compatibility, deterministic
rules and separate modded save namespaces explicitly.

### 3. Player mod manager in the existing launcher

**Install package** first checks the current signed catalog and previews every
exact dependency. Confirmation installs dependencies first and reuses only
matching workshop-origin packages whose archive and extracted bytes verify.
The preview reports expanded payload bytes, not compressed download size.
The operation shares a512MiB package-download budget, rechecks the entire required
set before publishing each package and leaves the active profile unchanged.
A cancelled/failed batch keeps completed packages installed and inactive; review
the plan again to retry. A later enable remains explicit. Automatic dependency
activation and update/rollback previews are still unfinished.

Browse offers content and creator filters, name/creator/numeric-version sorting
and20 results per page. It shows the latest approved available release per mod
by default; **Include older releases** exposes retained reviewed versions for
manual version selection. Search and filters apply to the displayed version's
metadata. Mixed packages match every included content kind. Cards show expanded
payload size and exact dependencies from strictly validated manifests; this size
is not a compressed download estimate. Filtering and catalog refresh reset paging.
Only the supported US Future Perfect game and draft API appear in this catalog.

The shared `ModWorkshopPage.cs` supplies catalog browsing, search, bounded
hash-verified downloads and native installation on the common WPF/Avalonia page.
`ModLibraryPage.cs` implements immutable installed versions, exact dependencies,
cycle/conflict guards, atomic active profiles, retained-version rollback,
texture-only enable/disable and safe uninstall. It checks hashes before launch;
Play without mods bypasses a broken profile without changing it. Native Mac/Linux
and actual game acceptance are tracked separately. Native services require HTTPS
and signed metadata; the standalone Python preview uses loopback HTTP. Redirects
and external package URLs are rejected. Every download rechecks the signed
catalog before and after transfer; expired scans are hidden. Workshop-origin
receipts enforce fresh availability/revocation before enabling and mounting.

Add **Browse mods**, **Installed**, and **Creator tools** to the shared launcher
model with the usual Windows/macOS/Linux adapters. Browse needs name/category/
creator/game-version filters, screenshots, release history, permissions, scan
and review dates, required dependencies, reports and an install action.

Before enabling a package, resolve exact dependencies, cycles, incompatible game/
API versions, conflicting asset keys and gameplay conflicts. Preview the required
download size and changes. Maintain ordered enabled-mod profiles, atomic
activation, enable/disable, safe uninstall and per-mod settings. Preserve the
existing Enhanced texture pack and Classic recovery path. Keep game files,
official updates, mods and saves separate. Lock activation while the game runs;
recover interrupted installs on restart. Add a “launch without mods” action.

Production networking needs bounded downloads, cancellation/resume, verified
hashes, authenticated catalog metadata and revocation checks. Keep the local
prototype's loopback transport separate from that production client. Installed
unmodified games must remain playable offline; decide the explicit policy for
stale mod trust metadata before offline mod execution is enabled.

### 4. Public hosting and moderation

Use a dedicated authenticated API, relational metadata store, private quarantine
object storage, isolated scan queue/workers and a separate approved-package
bucket/CDN. Public uploader identities must own their mod IDs. Use short-lived
upload authorization, hard per-user storage/upload/rate quotas, idempotent job
processing and immutable versions. Approve only the exact digest scanned and
reviewed; keep quarantine objects inaccessible through public URLs.

Use TLS, reviewed authentication/session handling, publisher permissions,
moderator roles, abuse reporting, takedowns, audit history and backup/restore.
Publish signed catalog metadata with pinned trust roots, expiry and rollback
protection; plan key rotation/recovery before release. Hashes alone do not
authenticate a catalog. Rescan on fresh signature updates and schedule periodic
rescans. Enforce scanner database freshness; an engine outage must stop new
approvals. Revoke compromised releases and tell installed clients what changed.
The adapter now enforces definition age and Mac process isolation; scheduled
updates/rescans, queue policy, memory limits and service identity still need
production integration.

The first public version can use one scanner plus structural/content validation
and manual review; additional engines or isolated behavioral analysis can be
added without treating consensus as a guarantee. Do not send creator uploads
to third-party scanning services without an explicit disclosure/retention policy.
Avoid serving uploaded HTML/SVG or arbitrary screenshots directly from the app
origin. Re-encode images in isolated workers and serve downloads as attachments.

### 5. Release gates

| Milestone | Required evidence before claiming support |
| --- | --- |
| Local repository foundation | Adversarial package/state tests; current branch provides this |
| Asset mod alpha | Texture/audio/map integration tested separately in isolated game sessions; enable/disable preserves originals |
| Script alpha | Real game hooks, hostile-script/host-call tests, memory/time isolation, crash containment |
| Launcher beta | Search to install to play to rollback/uninstall on all three hosts; no external Python requirement |
| Public workshop beta | Real auth/ownership, isolated scan workers, fresh definitions, signed catalog, revocation, backup restore and abuse handling |
| Multiplayer support | Server authority, ordered hash negotiation, mismatched-mod handling, modded-save separation |

The operator selected a Mac mini with public internet access and launcher-based
browsing. A no-purchased-domain trial using Tailscale Funnel is prepared in the
[hosting guide](mac-mod-hosting.md); it has bandwidth limits. Account/age policy,
moderation staffing and production service management remain operator decisions.
No hosting purchase or public deployment has occurred. Build and test each
milestone before advertising it.

## Validation

```console
python -m pytest tests/mods/test_mod_workshop.py -q
```

The suite uses synthetic scanner results only within tests, plus missing-engine
and subprocess-result/timeout fixtures. It exercises actual loopback HTTP search,
download, staging and revocation. It does not claim live antivirus detection,
real game loading, sandbox security, public hosting or performance acceptance.

Run `python tests/mods/live_clamav.py --database PATH --clamscan PATH` explicitly
on the host with real official databases. It uses disposable private packages:
clean content stays private until review, EICAR is rejected, empty databases and
missing engines fail closed, and revocation removes availability. EICAR is the
harmless standard antivirus test file, never executed. This test does not prove
scanner OS isolation or detection of all malicious content.
Run `python tests/mods/live_scan_isolation.py --database PATH` on the Mac for
explicit filesystem/symlink/network/child-execution canaries and inherited hard
limit checks. It creates disposable canaries and never probes personal files.
