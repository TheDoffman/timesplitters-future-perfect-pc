# One launcher release for Windows and Mac

The next Stable launcher source version is **1.4.2**, including the merged
community links, language selection and Story Co-op track updates. The source bump does not
publish either platform; the workflow below must pass before release.

The **Launcher release (Windows + Mac)** workflow builds the Windows x64 launcher
and native Apple Silicon Mac app from the same source revision and shared version.
It publishes both packages in one GitHub release only after both hosts pass their
launcher regressions and package checks. Each installed launcher selects its own
platform's update manifest. Game releases remain independent. [Game Stable and named preview channels](releasing.md) do not control launcher self-updates. Publish stable launcher 1.3.0+ before publishing games using the new channel strategy.

## Publish an update

1. Change `LauncherVersion.Value` in `tools/launcher/LauncherVersion.cs`, plus its
   assembly/file attributes, to a new unused version. Keep the attributes numeric
   (`X.Y.Z.0`); a preview may use `X.Y.Z-beta.1` for `Value`.
2. Merge that change and the desired launcher updates into `main` through a PR.
3. In **Actions → Launcher release (Windows + Mac) → Run workflow**, select `main`,
   enable **publish**, and run it once. The workflow creates `launcher-vX.Y.Z` at
   the tested commit and publishes both packages. There is no second Mac release
   step and no locally installed SDK or artwork needed on the maintainer's device.

Alternatively, push a `launcher-vX.Y.Z` tag on the merged commit. Its version must
match the source, and the commit must be contained in the default branch. Do not
create a separate manual GitHub release for that tag: let the workflow own its
draft and upload sequence. GitHub's standard Actions token supplies publication
access; no personal access token is needed.

Leave **publish** off for a build/test run with downloadable Actions artifacts.
Relevant launcher PRs run this validation automatically, with no publication
credentials. Windows is built first so both hosts receive the same original
artwork and newly generated shared icon. Mac uses the pinned SDK and locked NuGet
dependencies. Source artwork remains ignored; `config/build/launcher-artwork.json` pins
the archive and three embedded PNG checksums from the existing Windows release.
The artwork extractor reads that assembly in reflection-only mode. Changing art
requires updating those shared pins after the new assets are available.

## Published assets and failure handling

New releases contain one versioned ZIP and one direct-download alias per platform,
two SHA-256 sidecars, two build receipts and two update manifests (ten assets):

- Windows: `launcher-update.json`, selecting `windows-x64`.
- Mac: `launcher-update-macos-arm64.json`, selecting `macos-arm64`.

The workflow verifies internal manifests, archive hashes, permissions, source
receipts and uploaded GitHub digests before publishing. Failed builds leave no
release; failed uploads leave a draft that can be resumed. Published assets and
tags are immutable: fixes require a new version.

Stable launcher releases become **Latest**. README buttons download the matching
`TimeSplitters-Launcher-windows-x64.zip` or `TimeSplitters-Launcher-macos-arm64.zip`
through GitHub's `/releases/latest/download/` route. A separate link opens release
notes and checksums. Previews do not replace Latest.
Versioned ZIP names and update manifests retain their existing format, so
installed launchers can still discover, verify and repair their matching package.
Aliases are uploaded from the verified versioned ZIP paths without creating
local copies. Stale aliases in build folders are ignored. Both public ZIP names
have identical bytes and SHA-256 digests; the versioned checksum sidecars also
verify the aliases. All ten assets must verify before a draft becomes public.

Publication explicitly refreshes the issue-form version workflow because releases
created using `GITHUB_TOKEN` do not trigger its release event.

Generated changelogs explicitly compare against the highest earlier published
stable `launcher-vX.Y.Z` release, including for launcher previews. Game tags,
drafts and launcher prereleases cannot become the baseline. Release history is
paged; incomplete history stops publication. With no previous stable launcher,
the notes describe an initial launcher release without an unrelated comparison.
The publication job checks out full Git history and tags so the changelog can
inspect that range. A successful build job does not supply history to this job.

The change list includes only merged PRs in that commit range touching launcher
source, packaging, artwork/SDK pins or embedded default controls. Game-only,
documentation-only and issue/release housekeeping PRs are excluded. Mixed PRs
remain included when they change launcher inputs.
Mixed PRs touching game source, patches or texture assets use the neutral
"Launcher updates" label instead of advertising a game feature in their title.
Changed-file lists are paged and checked for completeness, including renamed paths. The full source comparison
link is explicitly labeled because GitHub's compare view also shows game changes.

The Mac target is ARM64, not Intel. The app retains its existing ad-hoc signing;
Developer ID signing/notarization and clean-machine Gatekeeper acceptance remain
separate delivery work. This workflow releases the launcher only; native game
packages and original game data are not bundled. CI package tests use disposable
fixtures and never use personal saves or settings.

## Mac application signing and the v1.2.19 migration

The Mac builder signs nested native libraries, then the completed application
bundle, using a local ad-hoc identity. It strictly verifies the complete signature
before generating file hashes. The release manifest includes the canonical
`TimeSplitters Launcher.app/Contents/_CodeSignature/CodeResources` seal. Mac CI
also verifies the extracted/installed app, rejects a missing seal and a changed
Info.plist, then verifies the repaired bundle before accepting the package.

This fixes a packaging gap in v1.2.19: the executable was ad-hoc signed but the
assembled .app was not sealed. Direct executable tests did not check Finder's
bundle signature. An intact ZIP or a successful command-line launch therefore
was not evidence that a downloaded app would pass macOS launch assessment.

Ad-hoc signing does not establish an Apple Developer ID or notarization. A browser
download can still be blocked by Gatekeeper. Do not claim a warning-free install,
notarization or clean-machine acceptance from `codesign --verify` alone. Never
remove quarantine or disable Gatekeeper in the build, updater or automated tests.
Developer ID signing and notarization require separate credentials/setup.

**One-time Mac migration:** v1.2.19's updater rejects the newly required signature
file. Users of that version must manually install the first corrected Mac package;
its automatic updater cannot perform this transition. Close the launcher, keep
the existing game installation, saves, settings and ISO, and replace only
`TimeSplitters Launcher.app` and `Support/Launcher/manifest.json` with the corrected
release's copies. Subsequent corrected versions accept and preserve the seal.
Windows update layout/discovery is unchanged. Release notes must disclose this
Mac migration; do not replace the immutable v1.2.19 download in place.
