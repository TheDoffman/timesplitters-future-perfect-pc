# Game releases: Stable and named previews

[Discord release notifications](discord-announcements.md): Stable posts to announcements; previews post to playtesting.

[Back to the README](../../README.md) | [Launcher releases](launcher-releases.md)

Game and launcher versions are independent. The next Stable game source version
is **0.4.0**, adding native Mac multiplayer and save-folder selection after `0.3.6`. This source change does not
publish a release or establish full campaign/platform validation. Historical
alpha and `-preview.N` releases remain available in Game versions; do not
relabel their assets or offer them as Beta updates.

## Versions and channels

| Purpose | Example tag | Channel | GitHub status |
| --- | --- | --- | --- |
| Stable game | `v0.3.2` | `stable` | Regular release, never Latest |
| Maintenance fix | `v0.3.3` | `stable` | Regular release, never Latest |
| Beta testing | `v0.4.0-beta.1` | `beta` | Pre-release |
| Named feature test | `v0.4.0-story-coop.1` | `story-coop` | Pre-release |
| Launcher | `launcher-v1.3.0` | Independent launcher updates | Stable launcher owns Latest |

Choose the intended version explicitly. Patch increments are fixes/maintenance;
minor increments are new features; major increments are deliberate compatibility
or project milestones. Before 1.0 the port remains experimental: document save,
settings and multiplayer compatibility in each release. Stable means the default
supported game channel, not a claim of complete original-game fidelity.

A preview's base number is its intended future release. Both a co-op branch with
recent main fixes and one without them can use `0.4.0-story-coop.1`; the commit
and notes identify their actual contents. Once published, the next changed build
is `.2`. New general tests use `beta.N`; the track is always named **Beta**. Named
feature tests keep their own suffixes, such as `story-coop.N`. New alpha,
preview and rc suffixes are rejected by version preparation, packaging and
publishing. Historical versions remain readable for exact-version history and
repair, and are excluded from Beta update selection.

Stable hotfixes such as `0.3.3` are not blocked by a published `0.4.0-story-coop.8`.
Ordering is checked within the stable major/minor maintenance line, or within a
preview's exact base version and channel. Independent experiments never compete
for the same update candidate. Prefer merging relevant Stable fixes into each
feature branch before a public test. If a feature slips to another target minor,
start the new series there and retain the original published tags.

## Prepare the version and source

Start independent release-tooling changes from main. For ordinary Stable game
releases merge the intended changes and VERSION into main. A release must have a
clean checkout exactly matching its pushed origin branch. No uncommitted game
changes or dirty source receipts are accepted.

```powershell
# Explicitly choose a version; this edits VERSION only, without publishing.
.\.venv\Scripts\python.exe tools/release/release-version.py 0.3.2
# Review and commit VERSION with the intended changes through a PR.
```

For an urgent fix after main has moved ahead, create `hotfix/0.3.3` from the
`v0.3.2` tag, apply the fix, prepare VERSION `0.3.3`, review and push the branch.
Stable publication accepts main, or an explicitly selected `hotfix/` or
`release/` branch descending from a published Stable release. Merge the fix back
into main through a PR; do not replace main's development VERSION with an older
maintenance version. The local publisher checks provenance and ancestry; normal
repository review/branch protection still applies.

Feature previews may publish an explicitly selected pushed branch without
merging unfinished work into main. On that branch:

```powershell
.\.venv\Scripts\python.exe tools/release/release-version.py 0.4.0-story-coop.1
# Review, commit and push VERSION plus the intended feature changes first.
.\Publish-Release.ps1 -Branch story-coop -ChannelName 'Story Co-op Preview' -PrepareOnly
```

Channel IDs are lowercase letters/digits/hyphens, start with a letter, and are at
most 40 characters; preview counters start at 1 without leading zeros. Display
names may vary independently and are limited to 80 printable characters. A custom
name is specified on each invocation. The tag, package and channel ID must agree.

For general Beta testing, prepare `0.4.0-beta.1` on the intended pushed branch:

```powershell
.\.venv\Scripts\python.exe tools/release/release-version.py 0.4.0-beta.1
# Review, commit and push the version and intended changes first.
.\Publish-Release.ps1 -Branch beta-testing -PrepareOnly
```

This appears as **Preview -> Beta**. Repeat the same branch argument without
`-PrepareOnly` to publish after reviewing the package and notes. Beta's display
name is fixed; `-ChannelName` customizes named feature tracks only.

## Build, inspect and publish

The complete regression suite uses up to four workers and reuses successful
compiler environment setup. No cases are omitted. Use `-TestWorkers 1` for a
serial diagnostic run, or `-TestWorkers N` to set the worker count. Any test or
worker failure still stops packaging. See [measured timings and safeguards](../development/release-performance.md).

Use the existing checkout, pinned upstream/dependencies and supported US Xbox
data. Validate its executable before generation/game validation. Keep personal
saves and settings intact. Check free space before a full build/package.
Install GitHub CLI and authenticate outside automated validation if necessary.

For main's Stable release:

```powershell
git switch main
git pull --ff-only origin main
.\Publish-Release.ps1 -PrepareOnly
# Review build/releases/<package>.notes.md and the package/receipts.
.\Publish-Release.ps1
```

For a named preview, repeat the same branch and display-name arguments when
publishing the prepared package:

```powershell
.\Publish-Release.ps1 -Branch story-coop -ChannelName 'Story Co-op Preview' -NotesFile logs/story-coop-release.md -PrepareOnly
.\Publish-Release.ps1 -Branch story-coop -ChannelName 'Story Co-op Preview' -NotesFile logs/story-coop-release.md
```

For the maintenance example use `-Branch hotfix/0.3.3`. `-Draft` uploads a draft
instead of making it public. `-PrepareOnly` creates no tag or GitHub release.

The publisher validates the full patch stack, rebuilds Release with native checks,
runs Python tests, verifies package hashes/manifests and exercises the production
launcher installer with disposable fixtures. It checks preservation of game data,
saves, settings and textures and tests the bundled Online runtime. Existing
prepared packages are reusable only for the exact commit, repository, version and
channel metadata; changed assets or a moved origin branch stop publication.

Publish **launcher 1.3.0 or newer for both Windows and Mac first**. The game
publisher checks that a compatible stable launcher with both ZIPs is already
published. Launcher 1.3.0 separates game channels from launcher self-updates;
older launchers' Preview selector includes unrelated experiments.

Publication pins the exact tested tag, creates a draft, uploads the ZIP, checksum
and BUILD receipt, verifies asset names/sizes, then publishes. Game releases
always use `--latest=false`; only previews set `--prerelease`. Tags/assets are
never overwritten or moved. If upload fails, inspect its draft before retrying;
the game publisher refuses an existing draft/release instead of replacing it.
Keep recovery/provenance records and use a new version for changed published code.

The local publisher currently builds/uploads the Windows game ZIP. The native
Mac packager produces a matching versioned package from the exact existing tag
and matching native build revision, with the same channel identity in BUILD.json.
Attach only verified Mac assets/checksums for that tag. A missing Mac package
means that release is unavailable on Mac; never substitute another channel's
package. Windows validation does not establish Mac runtime acceptance.

## Release notes and provenance

Stable notes compare against the highest earlier published Stable game ancestor,
even across minor versions. For the first Stable game release only, when no
Stable game has been published, use the most recently published earlier game
prerelease that is an ancestor. Thus `v0.3.2` compares against
`v0.3.1-alpha.2`; subsequent Stable releases compare against Stable releases,
even if newer previews exist. Launcher releases and drafts are never candidates.
Preview notes compare against the highest suitable ancestor in the same release
series, falling back to an ancestor Stable release. Nonancestor commits cannot
become a baseline. A maintenance branch still requires a published Stable ancestor.

The publisher includes merged game-related PRs from the exact commit range.
Launcher-only, documentation-only and release housekeeping PRs are excluded.
Feature-branch work may not have a merged PR: use `-NotesFile` to supply reviewed
Markdown describing the changes, known issues, test steps and compatibility.
The file is limited to 64 KiB. Notes are regenerated on every invocation; review
them before publication. Missing PR metadata fails rather than guessing.

A hidden `tsfp-release` JSON comment declares schema, channel ID/name, minimum
launcher, source branch/commit, initial package platforms, included Stable
ancestor and latest published Stable at preparation time. If a preview omits the
latest Stable release, visible notes say so. This proves ancestry of published
Stable releases, not inclusion of every unreleased main fix. BUILD.json records
source/package provenance and the canonical version-derived channel identity.
The launcher validates tag/channel agreement and hides the metadata from displayed
notes. Legacy releases without metadata remain installable.

Promoting work to Stable uses a new suffix-free version and fresh validation;
never turn an existing preview tag into Stable or rename its ZIP. A preview alone
does not reserve its target minor release for that feature.

## Launcher behavior and recovery

Fresh installs default to Stable. The top-level **Update channel** has only
**Stable** and **Preview**. Selecting Preview reveals **Testing track**, containing
**Beta** and published named features such as **Story Co-op**. The
launcher remembers the track when switching through Stable; checks stay within
the selected track. Beta releases use `-beta.N` and channel ID `beta`; named
feature IDs remain unchanged. Old Preview/general-preview preferences migrate
to Beta, preserving the remembered named feature when present. Historical alpha
and `-preview.N` releases stay in Game versions without being relabeled Beta.
This migration never opts the launcher itself into test updates. Launcher self-updates remain Stable unless a
separate explicit launcher preview preference is configured.

Select **Stable** and confirm **Switch game channel** to return from a newer
preview to the available Stable game. Same-channel checks never automatically
downgrade. **Game versions** remains available for exact-version comparisons;
installing a version also records its channel. An unavailable/withdrawn channel
stays selected without silently switching to another experiment or Stable.

Switching replaces the game runtime while preserving saves, settings, imported
data and installed textures. Test releases currently use the existing save
location: back up saves before testing or use a separate portable installation
with its own saves. No automatic save conversion or compatibility guarantee is
provided. State multiplayer version compatibility in each preview's notes.

## Optional textures and local packaging

Official game ZIPs omit enhanced textures. Ordinary source builds and publishing
need no texture LFS hydration. Players opt into the separate pack through the
shared launcher; updates, repairs and rollbacks preserve installed/custom packs.
Do not remove a pack because a new game ZIP omits it.

```powershell
.\.venv\Scripts\python.exe tools/release/package-release.py
```

This creates a versioned ZIP/checksum under build/releases using default settings,
without game data or personal files. It requires the matching successful Release
build and includes the portable Online/Python dependencies and licenses. An
explicit `--with-textures` offline bundle is separate from official publishing;
audit/hydrate its assets and preserve the 6 GiB pack budget. Optional pack
publication follows [the texture guide](../textures/textures.md), including hash/LFS checks.

## Issue-form versions and validation

Published game tags (`v...`) and launcher tags (`launcher-v...`) are refreshed in
the bug form by **Issue form versions**. Drafts and unshipped source bumps are
excluded; Stable and preview versions remain available. Required inputs remain
empty text fields. The workflow proposes reference-list updates through a PR;
it does not push to main. Launcher publication dispatches it explicitly because
GITHUB_TOKEN-created release events do not start another workflow.

Local checks use simulated GitHub and disposable data; they never publish:

```powershell
.\.venv\Scripts\python.exe -m pytest tests/launcher/test_publish_release.py tests/launcher/test_native_mac_package.py tests/launcher/test_launcher_release.py tests/repository/test_ci_runtime_scope.py -q
powershell -NoProfile -File tools/build/build-launcher.ps1 -Test
```

The release CI suite includes publisher/version/channel checks; both launcher
hosts run shared channel-selection/preferences/metadata fixtures. Run native Mac
launcher/package/lifecycle checks on macOS. A source version change requires no
gameplay test by itself; publishing still runs the actual build/package gates.
