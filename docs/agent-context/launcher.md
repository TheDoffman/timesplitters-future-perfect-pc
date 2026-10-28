# Launcher internals

Build/release/CI helpers now live under their matching `tools/` folders.
Launcher implementation stays under `tools/launcher/`; installed package paths
remain separate from repository source locations.
NativeHost's inline Vulkan Python imports from `tools/build`; the native-launcher
suite executes that embedded code against an isolated grouped helper fixture.

The launcher compatibility selector follows the game-icon patch into
`patches/platform/`. Its scope and tests are unchanged; nested runtime patches
continue to select runtime CI jobs through the existing `patches/**` rule.

Compatibility scope matching now uses the numbered patch filenames documented
in [patch maintenance](../../patches/README.md); selection behavior is unchanged.
The obsolete generic README download SVG was removed; the Windows and Mac
buttons and launcher implementation remain unchanged.

<!-- packet:start -->
The desktop launcher is C# under `tools/launcher/`, NOT the guest recompilation
runtime. Windows uses WPF; native Mac uses Avalonia plus thin platform adapters.
Shared policy belongs in the existing C# models, not a second Mac copy.
`Launcher.cs` coordinates UI; `Launcher.xaml`, page classes and themes define
shared layout; `Ui.cs` maps controls/events. `LauncherPlatform.cs` owns OS/package
names and development-checkout versus installed paths. Read it before assuming
that `Data`, `Settings`, `Saves`, `Bin` and root checkout paths are interchangeable.

`ReleaseClient.cs`/`GameVersionsPage.cs`: published game releases and selection;
`SelfUpdate.cs`/`UpdateRestart.cs`: launcher updates and restart;
`DownloadTransfer.cs`: transfers; `InstallationMaintenance.cs`: repairs;
`GameSessions.cs`: game launch/lifetime; `GameSettings.cs`/`SettingsPage.cs`:
settings; `TexturePack.cs`/`TextureDownload.cs`: optional textures.
`GameSettings.NativeFps` stores original(0)/120/180/240 beta selection in a separate
`pc-frame-rate.ini` and exports `TSFP_NATIVE_FPS`; it needs a compatible game
build and does not revive interpolation or change the console interrupt clock.
Legacy144 sidecars load as120 without writes; explicit save uses the new preset.
Launcher version and game version are separate. A source commit is not a
published release. Use `tools/ci/launcher-compatibility.py --local` for API-free
shared checks, plus affected host lifecycle/build checks. Never start publishing
or change credentials just to validate a launcher edit.
<!-- packet:end -->

PR checks: `runtime-patches.yml` now calls the existing `launcher-build.yml`
when `launcher-compatibility.py scope` selects launcher inputs, then includes
that result in the candidate `PR validation` gate. The separate launcher
compatibility required check stays active until the branch ruleset migrates.

## Capture health diagnostics

Launcher v1.3.1 packages capture-health reporting and the 60/120/180/240 FPS
presets already on main. The shared version and numeric assembly attributes
advance together; package format, updater manifest names and optional-texture
delivery stay unchanged. Release validation uses the existing dual-host workflow.

Both launch adapters pass the persistent session ID as `TSFP_CAPTURE_SESSION`.
`GameSessions.CaptureSession` positively verifies the game PID/start-time pair.
`CrashReports.CaptureHealth` shares storage probing, bounded record parsing,
session/PID matching, 15-second freshness and helper lifetime checks across hosts.
It distinguishes missing helper, unverified old game, stopped/stale helper,
unwritable storage and live armed/unarmed capture. File presence alone never
establishes support. Diagnostics refresh is explicit; it does not run a test crash.
The ordinary Diagnostics report and Windows live-hang view include this summary.
Only fixed status text enters copied diagnostics; raw heartbeat fields stay local.
`breadcrumbs.txt` joins the bounded report export/retention allowlist (8 KiB).

## Shared page presentation

`PageTheme.Header` owns the category, 28px title, description and silver-blue
separator used by Settings, Saves, Maintenance, What's New and Game versions.
`PageTheme.Badge`/`SetBadge` share neutral/info/success/warning treatments; every
state includes a text label, so color alone is never the status indicator.
Repair badges use verification results; texture health uses completed checks or
installs and reports an incomplete check on errors. Release badges distinguish
updates from different channels and newer installed builds. Preview/Beta labels
remain separate from installation state.

`SettingsPage.Field` reserves the same caption height across paired columns and
retains labeled-by accessibility. Graphics lists display, camera, frame rate and
quality before texture management, with the existing fixed Save/Cancel footer.
Both hosts consume these page helpers; no duplicate native layout is required.
Visual-only validation uses the existing Windows `--render-settings-graphics`,
`--render-settings-preview`, `--render-repair-preview`, `--render-news-preview`
and `--render-versions-preview` modes plus shared UI checks. Native Mac layout,
DPI and keyboard acceptance still require a native host.

## Flow and ownership

### Save manager and archive safety

`SavesPage.cs` is shared WPF/Avalonia UI under `SavesHost`; both navigation adapters
route Saves there and lock other operations while its cancellable work runs.
`SavesPage` receives the native Mac window’s lifetime installation-lock ownership;
its busy state serializes save work without reacquiring `mac.lock`. Standalone
backup/restore callers still acquire the installation lock, as do Windows calls.
`SaveBackups.cs` archives only `LauncherPlatform.Saves(root)` into `Backups/Saves`.
It records UTC/date, game version and reason, hashes metadata and nonzero 64 KiB
blocks, omits empty regions and verifies the completed archive before publishing
its GUID-named `.tsfpsave` file. Limits are 1,024 files, 64 GiB logical, 512 MiB
nonempty data, 8,192 blocks and 4 MiB manifest. List shows the latest 200 valid
archives; invalid files are retained. No import/export or automatic pruning.

`Core.ApplyScope` calls `BeforeGameChange` only for game updates/repairs/rollback;
backup failure blocks installation. Launcher self-update is excluded. Restore
stages and checks all files, creates a pre-restore backup, then swaps save folders.
The bounded owner journal under `.launcher/save-restore` supports interruption
recovery. Cleanup failure after commit must never roll back partially deleted
old files. Recovery keeps the installed save folder and validates its undo
archive before discarding the old one. The Windows adapter marks staged files
sparse with FSCTL_SET_SPARSE; native SetLength leaves unwritten holes on Mac.
See [Microsoft's sparse-file contract](https://learn.microsoft.com/en-us/windows/win32/api/winioctl/ni-winioctl-fsctl_set_sparse).
Tests live in `tests/launcher/launcher_saves.cs` and run through shared UI checks
on both hosts. They use synthetic data only; Mac execution remains host-specific.

### Texture status and verification

`TextureDownload.InstalledVersion` reads the bounded local receipt;
`Verify` compares the schema-3 file index and computes required part bytes without
changing textures. `SettingsPage` shows installed/available versions and saved
Classic/Enhanced mode, labels drafts, and offers Verify plus confirmed repair.
Unknown custom files are outside verification and remain untouched.

The launcher publication job requires a full-history checkout (`fetch-depth: 0`)
for `launcher_pr_notes` to inspect the previous launcher tag and merged commit
range. Build-job history does not carry into the separate publication job.
`test_launcher_release.py` checks this prerequisite; tags and published assets
remain immutable during interrupted-publication recovery.

Read [launcher usage](../players/launcher.md), [Windows launcher](../players/windows-launcher.md),
[Mac launcher](../players/mac-launcher.md) and [release process](../release/launcher-releases.md)
only as needed. `Bootstrap.cs` handles startup; `Core.cs` contains shared
installation/path/transaction primitives. `LauncherPlatform.cs` chooses host
executable names, locks, restart/open behavior and file operations.

Release selection and UI do not own installation safety. Follow the existing
validation/staging/rollback code when updating downloaded content. Keep package
integrity, local data, settings and saves intact. Do not replace a whole settings
file to change one option. `ReleaseNews.cs`, release-note classes and
`WhatsNewPage.cs` handle displayed news; `CrashReports.cs` and
`DiagnosticIssueDraft.cs` handle diagnostics. Review redaction before collecting
or sending diagnostic data.

Root `Play.cmd` variants and `Play-Native-Mac.command` launch development builds;
they are not the packaged launcher's installer. `tools/build/build-launcher.ps1` and
`tools/build/build-mac-launcher.py` build the desktop UI. `tools/launcher/macos/` holds
native project/adapters; shared behavior stays above it. Do not run the
interactive launcher as an automated gameplay test.

## Checks

`tests/launcher/test_launcher_compatibility.py` checks local selection and the API-free
workflow; C# behavioral fixtures under `tests/launcher_*.cs` cover updates,
settings, sessions, startup, downloads and lifecycle. Python harnesses choose
appropriate compilation/host checks. See `.github/workflows/launcher-build.yml`,
`launcher-compatibility.yml` and [compatibility docs](../release/launcher-compatibility.md).
Use the harness's explicit result; a skipped compiler/native UI test is not a
pass on that host. A Windows build does not prove Mac UI or process behavior.

## GitHub issue automation

Issue forms: `.github/ISSUE_TEMPLATE/`; normalization/labels/version options:
`tools/ci/issue-forms.py` and issue workflows under
`.github/workflows/`. PR resolution notes: `tools/ci/issue-resolution.py`,
`.github/pull_request_template.md` and `issue-resolution-comments.yml`.
Use live GitHub issue/release metadata for a request that requires current data.
Local Git history and stale advisory files do not replace a live issue body.
Publishing a fix, merging, releasing and creating issues are different actions;
preserve the user's actual authorization.

### Published version references (PR #283)

The bug form's `BEGIN AUTO LAUNCHER VERSIONS` block now includes published
launcher `v1.2.23`, retaining the earlier release references. This is reference data in
`.github/ISSUE_TEMPLATE/01-bug-report.yml`, generated by the existing
`tools/ci/issue-forms.py` release-list refresh. Required version/platform inputs
remain empty for the reporter to fill in.
No launcher executable, installer, update policy or game behavior changes.
`tests/repository/test_issue_forms.py` covers bounded/sorted release references and empty
required inputs. The repository-context
check requires this owning guide for structural issue-form changes; validated
AUTO version-list-only refreshes are exempt.

### Enhancement requests in Ideas

The issue chooser's `config.yml` uses a contact link to
`discussions/new?category=ideas`; the former `02-enhancement.yml` form is removed.
Bug reports retain their form, release references and automatic labels. Existing
enhancement bodies remain recognized by the label helper for historical issues.
The shared `DiagnosticIssueDraft.Create(true, ...)` opens the same Ideas URL for
both launcher hosts without attaching diagnostics or changing the clipboard
payload contract. Launcher v1.2.23 is the first version carrying this link for
Windows x64 and native Mac ARM64. The shared version and numeric assembly
attributes advance together; publishing still requires the dual-platform
release workflow's successful builds, package checks and verified upload.
README, contribution and launcher guidance use the discussion route. The version
refresh test verifies that chooser links survive and the old form stays absent;
the existing launcher maintenance fixture checks destination and diagnostic isolation.
`publish_version_pr` describes continued Ideas routing in generated version-refresh
PRs; its mocked creation fixture checks this explanation alongside the uploaded files.

## Auto display selection

Shared `DisplayModes.cs` and `SettingsPage.cs` expose Auto in the aspect and
resolution selectors; custom numeric inputs are hidden for Auto. The stored
`1,1` mode is resolved by the game on the active display, not by the launcher
from its own primary monitor. `GameSettings.CanAutoResolution` gates it on the
installed `BUILD.json` boolean `auto_display_resolution` (local unpackaged
development builds allow it). Both release packagers advertise the capability;
older manifests keep their original default and manual choices. New compatible
settings default to Auto, while saved dimensions remain unchanged. A saved
Auto choice remains readable after installing an older game; its runtime falls
back to console output, and the settings page explains that a manual choice or
newer game is needed before saving. Opening settings never rewrites that file.

## Retired bundled character icons (#74)

`Core.InstallPending` reads `BUILD.json` â†’ `textures.retired`, emitted by the
shared pack auditor from `config/textures/texture-originals.json`. Each key records the
previous bundled DDS and public JSON hashes. `AddRetiredTextures` preserves
the entire pair if either existing member differs; absent members need no action.
Matching removals carry a hash precondition through `ApplyScope` and its normal
backup/recovery journal. Rollback restores removed originals. Backup cleanup
accepts only flat keyed files whose recorded retirement hashes still match;
unknown texture backups stay protected. This policy is shared by both hosts.
`tests/launcher/launcher_textures.cs` runs upgrade, custom-pair, partial-pair, rollback,
failure and late-change checks in both launcher builds. Already published
launchers require an update carrying this migration before upgrading the game.
Launcher 1.2.24 packages this migration for both hosts. Its shared version and
numeric assembly/file attributes advance together. The game publisher requires
a published stable 1.2.24+ with both platform ZIPs whenever the pack manifest
declares retired replacements; older bundled packs retain their 1.2.22 minimum.

## Optional texture delivery (2026-10-22)

`SettingsPage` owns both hosts' optional check/download/confirmation/cancel UI.
`TextureBusyChanged` locks the WPF/Avalonia host navigation and update actions.
Released launchers embed the `config/texture-download.json` URL; keep this public
endpoint and the publisher's default path stable during repository moves.
`TextureDownload` reads schema-3 `config/texture-download.json` (bounded to 8 MiB),
verifies per-part/per-file hashes and adopts matching files from existing packs
without receipts. Schema-1 receipts remain supported. Downloads merge keyed pairs
through `TexturePack`'s per-part recovery journal; identical pairs aren't rewritten.
Unrelated keys and settings remain. Completed parts survive interruption and are
rechecked on retry. Opt-in lives in `.launcher/texture-opt-in.json`; opening Settings
checks for updates, but downloads always require confirmation. Opt-out deletes only
that preference. Classic/Enhanced persistence is separate and defaults to Classic.

The catalog in `config/textures/texture-pack.json` declares optional delivery; normal
Windows/Mac game packages omit the assets. The download index must contain a
published schema-3 manifest, produced by `texture-release.py` after auditing and
uploading the committed pack. Committing DDS files alone does not make them
downloadable: the launcher reads this index, not Git LFS or the repository tree.
Merge each generated index after its remote assets verify; no launcher rebuild
is needed to discover it. No OS-specific texture protocol is needed.
Portable and host validation routes are recorded in build-test.md and MAC-057.

Texture operations disable Mac page navigation through the shared busy state,
keeping the Settings download progress and Cancel control accessible until completion.

## Release-reference refresh safety

`tools/ci/issue-forms.py` refuses an empty game or launcher result when that category
already lists versions, before writing either block. This covers empty, filtered
or category-incomplete responses; intentional removal of the last release needs
a reviewed manual reference edit. Initially empty categories remain supported.
PR #303 restored the references from the live published release API; the bad
refresh's upstream empty response has not been independently attributed.
`repo-context.py` exempts only valid version data inside both existing AUTO blocks;
question/schema, marker, publisher and workflow changes still require guides.

## Weapon-slot bindings

`GameSettings` appends weapon-slot actions from embedded
`config/pc-controls.default.json`, retaining the original 24 action indices
and adding exactly ten weapon slots (34 actions total).
Windows and Mac launcher binding views use the shared action count; native
in-game settings use `PC_ACTION_COUNT`. Missing bindings migrate in the draft
only, respecting custom keys and explicit Unbound. Stock White=1/Black=2 move
to Z/X if free; conflicting custom assignments are never stolen. Saving and
resetting use the existing transactional settings paths.

The binding table now exposes `GameSettings.BindingSlots = 2` on both launcher
hosts. Legacy four-source keyboard profiles retain their first two alternatives
and normalize on Save; loading does not rewrite the player's configuration.
The native runtime dialogs use the matching `PC_BINDING_SLOTS`. The in-game
themed editor applies bindings live through the shared runtime settings API;
launcher Save retains its existing close-game requirement.

## Stable and named preview releases (2026-10-22)

Launcher 1.3.0 source separates game channels from launcher self-updates.
`GameChannels` in ReleaseClient.cs owns canonical tag-derived IDs, optional
`tsfp-release` note metadata, channel options, preference migration and cross-channel
install eligibility. New game versions are X.Y.Z or X.Y.Z-channel.N; legacy
Beta tags map to the beta track; alpha/preview/rc tags remain history-only. New installs choose Stable; old preview
preferences affect only the game. Save preserves unrelated preference keys and
writes game_channel plus preview=false for older-client recovery. SelfUpdate
reads the separate launcher_preview preference, default false on both hosts.

ReadGameCatalog and CheckLauncher page through the release list, refusing a
partial catalog at their cap. Windows Launcher and Mac NativeWindow populate
channel options from verified platform packages and retain unavailable selections.
Game checks select exactly one channel; switching to Stable may install a lower
version after confirmation. Same-channel checks do not downgrade. Exact-version
installation saves its channel; normal installer/repair/rollback protections stay
in Core. Visible notes omit channel JSON and describe named game channels
separately from launcher updates. Saves remain shared and require backup before
preview testing; no save-format migration or profile copy is introduced.

Publisher and both game packagers record channel identity; publication requires
a published stable launcher 1.3.0+ with Windows/Mac assets. VERSION 0.3.2 is the
next Stable game source version, not a published release. The docs/release/releasing.md
process covers explicit feature/hotfix branches, version choice and rollout.
Shared UI fixtures execute channel isolation, preference preservation/migration,
metadata checks and return-to-Stable eligibility on both hosts; Windows versions
fixtures cover paginated discovery. Native Mac host acceptance remains pending
in MAC-062 and docs/project/STATUS.md.


Installation-copy follow-up: README's primary Windows installation row and
Windows launcher setup now explicitly keep Stable selected. Screenshot provenance
in docs/assets/README.md treats its old channel as illustrative. Older Windows
launcher release/update guidance now describes paginated channel-specific game
selection and independent Stable launcher updates; previews remain opt-in tests.
This follow-up changes documentation only, with no runtime or installer change.


Preview grouping follow-up: `GameChannelPickerPage.cs` is the shared WPF/Avalonia
Home selector hosted by GameChannelPickerHost. Its top-level choices are Stable
and Preview; Preview reveals Beta and named feature tracks. Canonical
channel IDs/tags and release selection stay unchanged. game_preview_track remembers
the last testing track across Stable and restarts; game_channel still stores the
actual selected ID for compatibility. Catalog refreshes suppress selection events,
and unavailable tracks remain selected. Both hosts lock both selectors while busy.
Shared UI checks exercise grouped options, remembered selections, silent refresh,
missing-track retention and disabled controls. Developer render previews show an
explicit synthetic Story Co-op sample; normal launchers list published tracks only.


Local artwork correction: the Windows builder now prefers hash-verified cached
resources in build/launcher-art (recovered by fetch-launcher-art.ps1 from the
config/build/launcher-artwork.json pin), then falls back to legacy private source art.
Explicit build arguments remain supported. Older private artwork is preserved;
the incorrect local banner came from 1.2.12 and lacked the pinned backdrop.
Top-right badge wording is restored to Stable Channel/Preview Channel on both
hosts; the grouped selector itself remains Stable/Preview. No release artwork
pin or published asset was changed.


Beta naming (2026-10-23): canonical general-track ID/name are beta/Beta and tags
use -beta.N. GameChannels.FromTag marks old alpha/preview/rc releases as legacy;
they remain repairable in Game versions, outside Choices and Beta candidates.
Old general-preview preference IDs/remembered tracks and the old boolean Preview
flag migrate to beta; named features and unrelated preferences remain intact.
GameChannelPickerPage shows Beta under Preview and retains the feature suffixes.
Old general-track note metadata remains readable without relabeling legacy builds.
Shared UI fixtures check Beta-only candidates, hidden legacy tracks/history,
preference migration and existing grouped selectors on both hosts.

Launcher changelog correction: tools/release/launcher-release.py launcher_notes pages the
published release history and explicitly supplies the highest earlier stable
launcher tag to GitHub's generated notes API. This prevents a game alpha from
becoming the implicit comparison baseline; drafts, launcher previews and later
versions are excluded. Initial launcher notes avoid implicit generation, and
incomplete pagination fails closed. tests/launcher/test_launcher_release.py covers mixed
paginated history, initial releases and the history cap in the existing dual-host
publication test route. No launcher binaries or OS adapters change.

launcher_pr_notes filters generated PR references to merges in the exact Git
range and launcher payload/build paths via launcher_release_path. Game-only,
docs and automation PRs are omitted; mixed launcher/game PRs remain eligible.
File pagination, rename sources and API truncation are validated (the 3,000-file
cap uses the local merge diff). The source comparison link explicitly includes
game history. Offline release fixtures cover filtering, range and complete files.
Mixed PRs touching game runtime/patch/texture source use a neutral launcher label
so their game feature titles do not become advertised launcher features.

Game changelog bootstrap: tools/release/publish-release.py preflight uses the most
recently published earlier ancestor game prerelease (including legacy alpha)
only when publishing the first Stable game and no Stable game exists. Later
Stable notes select the highest earlier Stable ancestor across minor versions;
preview series policy is unchanged. Maintenance branches still need a Stable
ancestor. Launcher tags, drafts, future versions and nonancestor previews are
excluded. tests/launcher/test_publish_release.py verifies baseline selection and generated
notes payload/range through the existing release test route. No host adapters change.
