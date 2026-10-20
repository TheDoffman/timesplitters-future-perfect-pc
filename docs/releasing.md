# Packaging and publishing Windows releases

[Back to the README](../README.md)

Run these commands from the repository root. Players can use the ready-made ZIP linked in the README.

Release regression tests can run from ordinary PowerShell. They discover Visual
Studio or standalone Build Tools and load the MSVC x64 environment automatically.
Install the C++ x64 toolchain; a Developer PowerShell session is not required.

## Package locally

To produce the same public package after building:

```powershell
.\.venv\Scripts\python.exe tools/package-release.py
```

This uses an explicit file list and default settings. It excludes all game data,
personal settings, and saves, and writes the ZIP plus SHA-256 under `build/releases`.
The game ZIP includes the audited compiled textures in `Textures/` by default.
They stream directly from the single active pack into the ZIP, with sanitized
metadata and per-file hashes in `BUILD.json`; no texture staging copy is made.
Use `--texture-pack <folder>` to select another audited pack, or
`--without-textures` for a lightweight distribution. The staging folder alone
does not contain the streamed textures; use the finished ZIP to test delivery.
The launcher seeds missing replacement keys while preserving existing texture
pairs and personal settings. Updating an already installed artist pack remains
a separate texture-pack operation. Old launcher versions that reject `Textures/`
need the launcher update containing this shared installer change first.
The separate launcher bootstrap ZIP stays small; this applies to the game ZIP.
Launcher 1.2.22 adds bundled-texture installation; publish that shared launcher
update before distributing a game ZIP that contains textures. The standard
launcher release workflow validates both Windows and Mac before publication.
Both Windows and native Mac packaging use the same streaming texture helper.
The package includes an isolated Windows Python/Tk runtime and the Online
companion, including automatic hosting certificate support; players do not need
Python or Git installed. Packaging downloads the pinned Windows wheels in
`requirements-release.txt` and includes their licenses. A matching successful
Release build is required. `Source/pc-port-source.zip` records the local project
source and runtime patch, including changes not yet committed.
Use `.venv\Scripts\python.exe tests/portable_online.py --stage <extracted-package>`
to test the bundled runtime, HTTPS hosting, invitations, and three-peer UDP relay
without system Python/Git on PATH. Real router forwarding still needs validation.

## Publish a release from main

The optional texture pack is delivered independently from game and launcher
packages. For new artwork, run `python tools/texture-release.py publish --version
<new-pack-version>` from the checkout holding the authoritative pack, then commit
the resulting `config/texture-pack.json` before merging the delivery update.
This uploads the DDS files to an immutable GitHub release without putting them
in Git history. Every main push and release publication verifies that the
manifest still references uploaded assets with matching sizes and SHA-256 hashes.
An ordinary code release reuses the existing pack; it does not force users to
download it. See [texture publication](textures.md#publish-pack-updates).

Commit the intended `VERSION` and merge the changes, including these release
scripts, into `main`. Install [GitHub CLI](https://cli.github.com/) and authenticate
once with `gh auth login` using an account allowed to publish this repository.
Use the existing workspace and game data; setup/build dependencies must already
be installed. Then run in PowerShell:

```powershell
git switch main
git pull --ff-only origin main
.\Publish-Release.ps1
```

The script requires clean source and an exact match with `origin/main`. It reads
`VERSION`, validates the runtime patch stack, rebuilds Release with native checks,
runs Python tests, packages the public ZIP and verifies its manifest/checksum.
It then installs that exact ZIP with the launcher's production unpack/install
code in a disposable folder, checking fresh installs and preservation of existing
settings, saves, game data and textures. A launcher rejection stops publication.
It tests the bundled Online runtime with the existing game data, without copying
the game or using personal saves. It creates a GitHub draft for the exact tested
commit, uploads the ZIP, checksum and build metadata, checks the uploaded asset
names/sizes, then publishes it. Versions containing a prerelease suffix (such as
`0.2.0-alpha.3`) become prereleases. All game-runtime releases, including stable
versions, use `--latest=false` during draft creation and final publication.

### Keep the launcher download current

Use short release titles with the product and version first: **TSFP v0.2.0-alpha.5**
and **Launcher v1.2.7**. The game publisher sets this title automatically. For a
launcher release, the [Windows and Mac workflow](launcher-releases.md) sets
`Launcher vX.Y.Z` automatically. Keep the existing
`vX.Y.Z` and `launcher-vX.Y.Z` tag formats; titles do not change download URLs.

The README download button uses GitHub's permanent `/releases/latest` URL.
Reserve the repository's **Latest** designation for the newest stable launcher
release containing both Windows and native Mac packages. The shared workflow
uploads and verifies both before publishing, then assigns Latest for stable
versions only. This moves the README destination automatically; no README
version edit is needed.

Game releases published through this script retain the launcher destination.
If publishing a game release manually, also leave **Set as the latest release**
off or use `--latest=false`. The launcher discovers game updates from the release
list, so game releases do not need the Latest badge. See GitHub's
[permanent release links](https://docs.github.com/en/repositories/releasing-projects-on-github/linking-to-releases).

### Issue-form versions and labels

The **Issue form versions** workflow refreshes the bug report's reference lists from published GitHub
releases after publication, editing, deletion or unpublishing. Game tags use
`v...`; launcher tags use `launcher-v...`. Draft releases and unreleased source
version bumps are not offered as installed versions. Both stable and preview
releases are included, newest version first, with up to 30 versions of each
in a collapsed Markdown reference. Game version, Launcher version and Platform
are required text inputs without prefilled values. GitHub counts an empty
dropdown option as a selection, so blank dropdown entries do not enforce an
answer. Users can type older/custom, unknown or uninstalled values explicitly.
Release refreshes must update only the reference lists and never prefill inputs.
The improvement form
asks only for the idea, has no version, platform or diagnostic questions, and is
excluded from version refreshes.

The workflow also refreshes on relevant pushes to `main` and can be run manually
from Actions. It proposes only the bug form's reference lists through a PR on
`automation/issue-form-versions`, with `contents: write` and `pull-requests: write`;
it never pushes directly to the protected default branch or force-pushes.
Approve workflows to run if GitHub requests it on the bot-created PR, then let
the required checks pass before merging. Auto-merge can be enabled for that PR.
See [CI and branch protection](ci.md) for repository settings and check names.
The game publisher uses the maintainer's GitHub CLI authentication. The launcher
workflow uses `GITHUB_TOKEN` and explicitly dispatches **Issue form versions**
because token-created release events do not start another workflow.

Form submissions receive `bug` or `enhancement` immediately. On issue creation,
editing or reopening, the separate **Automatic issue labels** workflow synchronizes that type plus
`platform: windows` or `platform: macos` when a platform answer is present. New
improvement suggestions receive only `enhancement`. It creates
missing platform labels, removes outdated managed labels after edits, and keeps
other labels such as priority, accessibility and triage decisions. Unrecognized
reports and pull requests are left alone. Issue text is read as data, never
inserted into shell commands. The labeling job has `issues: write` and no
repository-write permission.

Offline checks: `python -m unittest discover -s tests -p test_issue_forms.py -v`.
Manual refresh: `python tools/issue-forms.py versions --repository owner/name`
(optional `--check` reports stale choices without writing).

### Release notes and preparation

The description includes merged game-related PRs since the most
recently **published game** release (including prereleases) and installation
instructions, with numbered launcher steps followed by a portable ZIP alternative.
The generated "What's Changed" heading is removed beneath
"Changes since"; source commit lists and build/validation sections are omitted.
Build provenance remains in the verification assets. The comparison does not use
the last Git tag or skip prereleases by asking for GitHub's latest stable release.
Launcher releases, drafts and unrelated tags are excluded from the comparison.
GitHub's [release notes API](https://docs.github.com/en/rest/releases/releases#generate-release-notes-content-for-a-release)
provides candidate PRs. The publisher verifies each PR is merged and its merge
commit is within the exact game-release commit range, then reads all changed
files (including renamed paths). Game source, shared patches/configuration,
game build inputs and runtime/import tools qualify; launcher-only,
documentation-only, version-only and CI/release-note housekeeping PRs do not.
Mixed game/launcher PRs remain because they change the game. Classification uses
files rather than title keywords or labels. Incomplete file metadata stops note
generation instead of guessing. PRs are deduplicated; an empty range gets an
explicit message. The full changelog link still opens GitHub's repository-wide
comparison, which can contain launcher commits.

To prepare and inspect the ZIP/notes locally without creating a tag or release:

```powershell
.\Publish-Release.ps1 -PrepareOnly
```

Run normally afterward to publish the same validated package. `-Draft` instead
uploads a draft for review on GitHub. Notes are regenerated on each invocation.
Artifacts and the preparation record stay under ignored `build/releases`;
Disposable staging and Python test fixtures are permanently removed using
validated paths; packaged archives and release metadata are retained. Test
fixtures are also cleaned when tests fail. No Recycle Bin support is required.
No branch switching, version bump, overwrite of existing release files, or tag
rewriting occurs. Changed prepared assets or a changed main commit stop the run.
Existing drafts/releases with the version also stop it; inspect a failed upload's
draft on GitHub before retrying. Failed package stages are retained for diagnosis.
Do not rename a package to reuse an already published version: update `VERSION`,
commit and merge it before preparing the next release.
