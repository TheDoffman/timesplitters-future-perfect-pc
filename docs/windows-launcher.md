# Windows release launcher

`TimeSplitters Launcher.exe` is the Windows x64 bootstrap. The upcoming Setup
EXE installs it for the current Windows user, creates shortcuts and registers an
uninstaller. The portable ZIP remains available: extract it into a writable
installation folder, keeping `Bin/TimeSplitters.App.exe` and
`Support/Launcher/manifest.json` beside the bootstrap in their subfolders.
Both forms install game runtime packages from the
public `TheDoffman/timesplitters-future-perfect-pc` GitHub Releases page. It never
pulls a branch, builds game code, requests a GitHub login or downloads original
game data. It reuses `tools/release/ImportGame.cs` for local ISO/folder import.

## Build and validate

Run `powershell -NoProfile -ExecutionPolicy Bypass -File tools/build-launcher.ps1 -Test`.
The build uses the Windows .NET Framework compiler, WPF, and built-in libraries;
no NuGet packages or .NET SDK are required. Supported systems are Windows 10/11
x64 with .NET Framework 4.8. The test executable uses tiny synthetic ZIPs in
`build/launcher/updater-tests`, never real game data, settings or saves, and
removes that isolated directory after the pass.

`build/launcher/launcher-tests.exe --live` reads GitHub release metadata without
installing anything. `launcher-tests.exe --verify-release` downloads, verifies,
and installs the published Preview ZIP into a disposable test folder, then
removes it; it never executes downloaded code or copies game data.
`launcher-tests.exe --live-launcher` reads launcher-only metadata without
installing anything. `"build/launcher/Bin/TimeSplitters.App.exe" --render-preview
preview.png` renders a sample installed-state UI without showing a window or
accessing the network. The sample version in that image is illustrative.
`"build/launcher/TimeSplitters Launcher.exe" --render-splash splash.png` renders
the startup splash without opening a window or checking the network.

`tools/package-release.py` builds and includes the launcher each time it creates
a Windows runtime ZIP. The checkout's developer `Play.cmd` is unchanged. The
packaged `Play.cmd` opens the launcher, with its previous direct-launch path as
a compatibility fallback when the EXE is absent.

## Windows installer (current source, not yet published)

Build with `powershell -NoProfile -ExecutionPolicy Bypass -File
tools/build-launcher-installer.ps1 -FetchCompiler -RequireArtwork -Test`.
This runs the normal launcher build/tests, verifies the two executable hashes
and versions against the current source/manifest, and wraps exactly the three
launcher payload files in Setup. `-LauncherDirectory <built-folder>` reuses a
verified existing launcher build; `-Output <folder>` selects the output directory.
With a supplied build, `-Test` runs installer lifecycle checks only; run the
normal launcher regressions separately. `-RequireArtwork` also verifies embedded
banner/logo resources in a supplied application.

The official Inno Setup 6.7.3 download and compiler files are SHA-256 pinned in
`tools/installer/compiler.json`. `tools/fetch-installer-compiler.ps1` downloads
and verifies it before execution, then installs its compiler in portable mode
under ignored `build/tools/innosetup-6.7.3`. It creates no compiler shortcuts,
file associations or uninstall registration. `-CompilerPath` must point to the
same pinned compiler. End users do not need Inno Setup or build tools.

Setup defaults to `%LOCALAPPDATA%\Programs\TimeSplitters Future Perfect` and
uses non-administrative current-user installation. The directory chooser permits
another writable local folder, including an existing portable game installation.
It never migrates another folder or copies game data automatically. Shortcuts
use the bootstrap and the selected working directory. Installer-owned uninstaller
files live in `Support/Installer`; the normal updater never owns these paths.

Setup and uninstall share the launcher's per-installation mutex (uppercase,
UTF-8 path SHA-256). They block while a game runs or an update needs recovery;
no process is terminated. Redirected destinations are refused. Setup rejects
newer executable file versions to prevent an older installer downgrading the
launcher. Existing settings, saves, ISO, imported data, game runtime, textures,
logs and caches are excluded from installer ownership and uninstall removal.
There are no recursive file-install or delete rules.

`InstallerIntegration.cs` updates only `DisplayVersion` in the existing per-user
`TimeSplitters.FuturePerfect.Launcher_is1` uninstall key after verified startup.
It requires that `InstallLocation` match the current root. Portable instances
never create a registration or update another installation's entry. Installer
registration errors are logged without blocking normal launcher use.

The build emits versioned and stable-alias Setup EXEs with `.sha256` files,
plus `launcher-installer-build.json` with version, compiler/download pin, source
commit/changes and launcher payload hashes. The original three-file ZIP,
checksums and `launcher-update.json` remain the self-update format. Publish the
installer assets alongside those ZIP assets in the next launcher release; no
release is created by building. These development Setup EXEs are unsigned;
Authenticode signing is not configured.

`tools/test-launcher-installer.ps1` compiles a test-only installer identity and
unique shortcuts, then installs/uninstalls inside its own output subdirectory.
It tests fresh install, reinstall/repair, retained game-folder reuse, running-game
and mutex guards, unfinished transactions, downgrade refusal, self-update and
uninstall preservation, and redirected destinations. It never uses an actual game
or personal saves. Failures retain their fixture/logs for diagnosis. After
inspection, `-WorkName installer-tests-<label>` selects a fresh test directory
without deleting retained failures. The build wrapper exposes `-TestWorkName`.
Five registration checks use a unique temporary HKCU key; five payload rejection
subtests run in `tests/test_launcher_installer_packaging.py`. GitHub's Windows
installer workflow uses fallback artwork and publishes no releases.

## Launcher artwork and colors

The v1.1.1 interface uses dark blue steel panels, silver text and icy blue
accents, with a matching startup splash and silver TS/blue portal icon. The primary Play button
uses a silver-blue metallic gradient.

`tools/build-launcher.ps1` embeds the optional
`private/launcher-art/future-perfect-banner-v1.png` into the main application.
Use `-BannerPath <image.png>` to supply a different banner (maximum 8 MiB).
The image travels inside `Bin/TimeSplitters.App.exe` for launcher-only updates;
no extra runtime image file or download is required. If the private image is
absent, the build retains the blue/silver vector header. Generated source
artwork and its provenance stay ignored under `private/launcher-art`.
The current private banner is the supplied 1600 x 1200 Future Perfect key art,
embedded unchanged. The full photo fits proportionally inside the right side
of the header, with no cropping or redraw. The original game wordmark and a
blue/silver panel fill the remaining width. Its source hash and the previous
generated banner's recovery copy are recorded in `banner-provenance.json`.

The optional `private/launcher-art/timesplitters-future-perfect-logo.png`
supplies the original title wordmark and subtitle lettering in the sidebar.
`-LogoPath <image.png>` accepts
the 500 x 187 title lockup (maximum 1 MiB); the sidebar subtitle crop depends
on that layout. The PNG is embedded unchanged, with its subtitle displayed
separately at a readable size instead of substituting a font. Without this private
asset, source builds retain the sidebar text fallback. Source URL and SHA-256 stay in
`private/launcher-art/logo-provenance.json`; no external image is needed at runtime.

Both EXEs use a dedicated compact silver TS monogram inside an icy blue temporal
aperture, independent of private artwork. `tools/launcher/IconBuilder.cs` draws
each transparent ICO entry directly at 16, 24, 32, 48, 64, 128 and 256 pixels;
the small sizes use heavier portal strokes for clarity. The build also writes
`build/launcher/icon-preview.png` to show all sizes. No image generator or
external asset is required to reproduce the icon.

## Publishing game updates

Publish a GitHub Release with a semantic version tag (`v1.2.3`, or
`v1.2.3-alpha.1` for a prerelease) and these assets from the existing packager:

- `TimeSplitters-Future-Perfect-PC-<tag>-windows-x64.zip`
- The corresponding `.zip.sha256` sidecar (publish both).

Do not attach game data, saves or personal settings. The ZIP contains one package
root and a `BUILD.json` with a matching `version` plus SHA-256 for every file
except the manifest itself. Existing release packages are supported even if
they predate the launcher. The standalone launcher remains installed in that case.

Both channels check the most recent 100 published release records and choose
the highest supported game version with the matching Windows asset. Stable
excludes prereleases; Preview includes them. Launcher-only releases, drafts and source-only releases are
ignored. Updates never downgrade the installed version; rollback is explicit.
An unpublished or checksum-less release is not offered. Network failures and
rate limits leave installed play available.

The launcher uses an asset's GitHub-provided SHA-256 digest when available,
otherwise its `.sha256` sidecar. This verifies download integrity from the
project's release channel; it is not an independent publisher signature.
Production code signing and Windows SmartScreen reputation are not included.

## Publishing launcher-only updates

Bump `LauncherVersion.Value` in `tools/launcher/LauncherVersion.cs` and its
assembly/file version attributes, then run
`tools/build-launcher-installer.ps1 -FetchCompiler -RequireArtwork -Test`.
For the chosen version, publish these assets together from `build/launcher`:

- `TimeSplitters-Launcher-v<version>-windows-x64.zip` and its `.sha256`
- `launcher-update.json`
- `TimeSplitters-Launcher-Setup-v<version>-windows-x64.exe` and its `.sha256`
- `TimeSplitters-Launcher-Setup-windows-x64.exe` and its `.sha256`
- `launcher-installer-build.json`

Use a GitHub Release named `launcher-v<version>`. Mark test builds as prereleases;
stable launcher versions must also have no semver prerelease suffix. Releases
may contain a game ZIP too; launcher versions and game tags are independent.
The main application lists/filter releases so a launcher-only release cannot
hide a game release. Nothing is read from a branch or an unpublished draft.
`Publish-Release.ps1` / `tools/publish-release.py` remain the game-release
pipeline; this installer build does not create releases, push tags or upload assets.

The 200-byte update manifest has `schema: 1`, `version`, `asset_name`, `size`
and the ZIP's `sha256`. The bootstrap derives the download URL from the matching
GitHub asset in that same release. If GitHub publishes an asset digest, it must
agree with the manifest. Every file is checked again after extraction and
immediately before installation. The launcher-only ZIP contains exactly the
bootstrap, application, and `Support/Launcher/manifest.json`; the latter hashes
the two executables. Extra runtime/user files and unsafe paths are rejected.

At startup a small themed window checks launcher metadata before opening the
main UI. The eight-second deadline also bounds an OS request that ignores
cancellation. The Open installed launcher button cancels the check/download;
closing the splash cancels startup. Downloads have a two-minute limit. Offline,
missing-manifest, rate-limit and verification failures fall back to the installed
application. An incomplete installation still requires the complete ZIP if no
usable release is available. Launcher updates never downgrade across channels.

Updates are automatic for the selected Stable/Preview channel. No game update
is implied. The bootstrap waits for neither a game build nor a large runtime ZIP.
When ready, `.launcher/self-helper.exe` waits for the bootstrap process to exit,
acquires the shared installation mutex, and replaces the three launcher files.
The new bootstrap skips its network check once after a helper handoff to avoid
retry loops, then starts the main application. The application waits for the
bootstrap to exit before acquiring the same mutex. Opening the companion EXE
directly routes back through the bootstrap.

Launcher updates use separate `.launcher/self-transaction`, `self-pending` and
`self-rollback` folders. Startup recovers an interrupted launcher transaction
before opening the main application. The game BUILD.json, game rollback snapshot,
Data, Settings, Saves and Textures are never launcher-update targets. Game
updates/rollback preserve a newer independently installed launcher. Keep the
complete `.launcher` folder for recovery. No launcher-only release is published
automatically by building these files.

Starting with launcher v1.2.4, after a normal launcher window loads successfully and both installed launcher
executables match their manifest, automatic cleanup removes the previous
launcher backup (`self-rollback`), obsolete or incomplete launcher staging
(`self-pending`), and unused `self-helper.exe` / `update-helper.exe` copies.
There is no retained history of older launcher versions after successful startup.
The backup remains available until that startup succeeds. An unfinished launcher
transaction or an invalid installation prevents cleanup. A newer prepared update,
an unreadable staging marker, and helpers needed by pending updates are retained.
Locked files are logged and retried at the next normal startup. Failed or cancelled
launcher downloads also discard their staging files immediately when possible.

Cleanup only targets these launcher-owned paths. It preserves game rollback,
game data, settings, saves, preferences, logs, and unrelated/manual downloads.
Developer render previews never perform startup cleanup. Run the isolated
cleanup regressions with `build/launcher/launcher-tests.exe --cleanup` after
building the test executable with `tools/build-launcher.ps1 -Test`.

## Startup argument troubleshooting

`Unknown bootstrap arguments` means the startup EXE received unsupported
command-line arguments before opening the main application. Normal startup uses
`TimeSplitters Launcher.exe` with no arguments. In launcher v1.2.2, developer
commands such as `--render-settings-graphics output.png` belong to
`Bin/TimeSplitters.App.exe`; passing them to the startup EXE causes this error.

The local v1.2.3 fix also accepts `--render-preview`, `--render-settings-preview`
and `--render-settings-graphics` at the startup EXE and forwards them to the
application without an update check. Relative output paths resolve against the
caller's current directory; the child exit code is preserved and preview time is
bounded to 30 seconds. `--render-splash` remains the startup EXE's own preview.
Unknown/malformed commands are still rejected. The startup log now records the
launcher version, argument count and bounded, JSON-escaped arguments, rather
than losing the arguments that caused a rejection. It remains capped at 1 MiB.

`launcher-tests.exe --bootstrap` runs the isolated preview-routing/diagnostic
checks without running game-update installation tests. Actual preview commands
operate on disposable output paths and do not launch the game or save settings.

## Pre-game settings

The sidebar **Game settings** page edits Player 1's keyboard/mouse bindings
(four alternatives per action), input device, sensitivity, mouse look and invert Y.
Click a binding to capture a key, mouse button or wheel direction. Reserved game
keys are refused. Reusing another action's key requires confirming the move.
Restore default controls affects Player 1 only and does not write until Save.

Launcher v1.2.2 opens both tabs inside the main launcher, with the sidebar
visible and usable. Binding capture, conflict reassignment and restore-defaults
confirmations also appear inside that page. **Home** retains your draft
and dismisses any active binding prompt; reopening **Game settings** restores the
same tab and edits. **Save settings** keeps the page open and reports validation
or save results in its footer. **Cancel** discards unsaved edits and returns to
Home; closing the launcher also discards unsaved edits. Existing
backup, rollback and external-edit protection are unchanged.

Graphics include output resolution (presets, attached display sizes and custom
dimensions), borderless fullscreen, supersampling (1–8), anisotropic filtering
(1–16), frame pacing, interpolation (1–4), VRR and the FPS overlay. Invalid
resolutions and non-finite sensitivity are refused. Desktop refresh is unchanged.

Launcher v1.2.1 adds
**Game settings → Graphics & resolution → Startup → Skip startup splash screens**:
skips the opening EA and Free Radical logo movies on the next launch. It also
disables the idle attract video; story cutscenes rendered by the game still play.
The checkbox defaults to off. Save settings to keep the choice, or uncheck and
save to restore the videos. The choice is stored under
`launcher.skip_startup_splashes` in `Settings/pc-controls.json`, preserved across
game and launcher updates. Launching sets the existing runtime's
`RECOMP_XMV_PLAY` switch explicitly to `0` (skip) or `1` (play), after clearing
inherited overrides. This works with the published alpha.4 game without a
runtime rebuild. New packages' `Launch.ps1` also honor this preference.

Controls and resolution use the same `Settings/pc-controls.json` and
`Settings/pc-display.ini` formats as the in-game settings. Graphics use a portable
`Settings/runtime.conf`; when it is absent, the UI reads the existing per-user
`xboxrecomp/titles/45410066.conf` as its starting point. Saving copies those
settings into the installation and never changes that per-user source. Unknown
JSON fields, other controller profiles, unknown runtime keys and comments survive.
Save detects external changes, makes `.bak` copies, writes atomically per file,
and restores earlier files if a later write fails. A running game blocks saves.

After portable graphics settings have been saved, Play starts the game directly
with explicit data/save/input/display/texture/online paths, sanitized environment,
mouse preferences and timestamped `Logs/*.out`/`.err`. This is necessary because
older `Launch.ps1` packages discard inherited display overrides. Installations
without portable graphics settings retain the existing script launch path.
Game updates preserve all three settings files. No runtime/game-code change is
needed for the existing supported Windows package.

`launcher-tests.exe --settings` runs isolated settings/preservation/rollback and
child-process environment checks. `TimeSplitters.App.exe --render-settings-preview
preview.png` and `--render-settings-graphics graphics.png` render the two tabs
without saving settings or starting a game. These previews now include the whole
launcher and its sidebar, at 1152 × 772, using the same embedded settings page.

## Installation and recovery

Starting with v1.1.2, Play keeps the launcher visible during startup and
minimizes it once the installed game's window appears. It restores the previous
normal/maximized launcher state when the launch script finishes. Startup failures
before a game window appears leave the launcher visible with status/log access.
Window detection matches this installation's executable path.
`tools/test-launcher-lifecycle.ps1 -LauncherRoot build/launcher-v1.1.2`
checks normal/maximized minimize-and-restore and failed startup using a bounded
synthetic executable/window. It uses no game data or personal files. The broader
updater suite requires real game processes to be closed before its install tests.

The installation directory is the folder containing the launcher. The updater
accepts only known root files, `Bin/*.exe`, `Bin/*.dll`, `Licenses/*.txt`, the
release-managed `Source` and `Support` trees, and the initial
`Settings/pc-controls.json` default. Existing settings are skipped.
`Data`, `Saves`, `Textures`, other settings and developer/source directories are
never release targets. ZIP traversal, duplicate/case-colliding paths, alternate
data streams, linked paths and unlisted/missing files are rejected. Archive
count/expanded-size limits and disk-space checks bound staging.

The application checks for game releases automatically but game installation requires an
Install/Update click. Downloading and verification happen before the launcher
closes. A copy of the launcher runs as `.launcher/update-helper.exe`, waits for
the old process, acquires the per-installation mutex, verifies the staged files
again, and installs. Both update and rollback refuse while any game process is
running. The launcher also blocks a second instance for the same folder.

Every replaced file is backed up before any installed file is changed. A
`.launcher/transaction/journal.json` records which files originally existed.
Failures restore old files and remove newly introduced files. On the next
startup an interrupted transaction is recovered before play/update is enabled.
If recovery files are missing or locked, the launcher stops and retains recovery
evidence. Successful updates retain one `.launcher/rollback` snapshot until the
new game opens a window and exits normally. Until then, Restore previous swaps
releases without replacing user settings. The downloaded ZIP is
discarded after successful extraction, and staging is discarded after install.

### Automatic game cleanup (launcher v1.2.5)

After a game session opens a window and exits with code zero, the launcher checks
that the installed runtime still matches its release manifest and removes the
previous game snapshot. The Restore previous button then becomes unavailable.
A failed launch, crash, incomplete installation or unfinished update keeps recovery
available. This check establishes startup/clean-exit behavior, not full gameplay
compatibility. Cleanup runs in the background; a locked file is retried on later
launcher startups only for the same confirmed build. Developer previews do not
confirm a game session or run startup cleanup.

Game updates also remove unchanged files listed in the old release manifest when
the incoming release no longer uses them, including old source archives and DLLs.
Those removals are transactional and restored by rollback before confirmation.
Modified files, untracked files, personal settings and independent launcher files
are retained. Missing or invalid old manifests skip this optional pruning.

Cleanup only removes updater-managed game backups and obsolete/incomplete staging.
Current game data, saves, settings, texture packs, and the original ISO remain in
place. An ISO found inside a managed folder prevents that folder from being
deleted; linked paths and personal backup directories are also refused. A newer
or unreadable staged release is retained. Normal ISO storage and manually
extracted game folders are never searched for cleanup candidates.

Run `build/launcher/launcher-tests.exe --game-cleanup` for the focused synthetic
game cleanup checks. The full updater suite additionally covers manifest-based
file removal, rollback restoration, and protected ISO handling during updates.
Do not delete `.launcher` while an operation is active.

The interface imports only into an absent `Data` directory. Existing data is
validated and retained; it is never overwritten or automatically deleted.
An incomplete/wrong existing Data folder requires the user to inspect/relocate
it before retrying import. Failed imports retain a named `Data-import-*` folder
for inspection. The ISO does not need to remain available after successful import.

Launcher errors appear in the status line and `.launcher/launcher.log` (bounded
to roughly 1 MiB). Game output continues through the release's `Launch.ps1` and
its `Logs` folder. Saves and config are not used for automated launcher checks.

Windows-specific source lives in `tools/launcher`; Mac release updating remains
queued as `MAC-014` in `MAC_PORT_BACKLOG.md`.

Launcher v1.2.3 keeps custom FOV disabled for installed game releases through
v0.2.0-alpha.4, which only read V1 display settings. Resolution changes continue
to save in that compatible format. FOV becomes available with a newer game
release; unpackaged development builds retain access for testing.

Launcher v1.2.6 filters resolution presets for the installed game release and
validates custom sizes before saving. Game releases through v0.2.0-alpha.4
support heights up to 2160 (maximum 7680×2160); newer games and unpackaged
development builds support up to 7680×4320. Both retain landscape and 4:1 limits.

### Preserve launcher artwork during game updates

A game package replaces the independent launcher only when its bundled version
is newer, or when an equal installed version fails its manifest checks. A valid
equal/newer launcher keeps its binaries and manifest together, including embedded
banner/logo artwork. Explicit launcher updates use their separate transaction.

Public game packaging requires the private banner and title logo via
`tools/build-launcher.ps1 -RequireArtwork`; missing artwork stops packaging.
Developer/CI builds may still omit the switch to use the fallback interface.
For an installation already affected by an older updater, close the launcher
and extract the complete verified launcher release ZIP into that installation.
An equal version with internally valid fallback files is not automatically
reinstalled by the existing launcher self-updater.

## Resumable downloads and recovery (current source)

Game updates, launcher updates and repair downloads keep matching saved bytes
across retries and launcher restarts. Home offers **Resume download** for a saved
game transfer with a matching published digest. **Pause download** keeps the saved
bytes; closing during a game download requests a pause and waits for the current
file operation to stop safely. **Cancel preparation** also retains a completed
cached download so the next attempt can verify it again without downloading it.

Progress shows bytes received, total size, measured download speed and estimated
time remaining. Speed measures this attempt's new bytes; previously saved bytes
are excluded from the rate. The launcher says **Estimating time remaining** until
it has a useful measurement, and shows verification/preparation separately.

Home failures offer **Retry** (or **Resume download** after a pause),
**Repair / Verify** and **View logs**. Failed repair operations offer retry and
logs in the same page. Interrupted startup launcher updates offer **Retry update**,
**View logs** and **Open installed launcher** when one is present. A failed initial
release check still opens the installed launcher automatically for offline use.

The cache is separate from installation staging, with one game slot and one
launcher slot under `.launcher/downloads`. It is retained for interruptions and
cancelled preparation, then cleaned when staging succeeds. A different release
URL, size or SHA-256 replaces its previous recognized cache. Unknown cache files
and linked paths are refused and kept; cleanup only removes the named owned
cache files and never recursively removes user folders.

Resume uses HTTP byte ranges and a strong If-Range validator when available.
Range boundaries and lengths must agree with the published asset. A server that
ignores Range restarts the file; an invalid range is rejected, and an unsatisfiable
saved range gets one fresh attempt. The complete file always passes the published
SHA-256 before it reaches staging, and package manifests are checked as before.
Checksum failures discard the damaged cache. A connection or stream that stalls
is bounded to 30 seconds; retry can resume the saved bytes.

Developer checks: `launcher-tests.exe --downloads` runs synthetic HTTP and
scratch-file recovery tests without requiring the game to close.
`--downloads-live` interrupts a small public standalone launcher ZIP after 64 KiB,
resumes it in a fresh client and verifies the checksum; it never installs or runs
the downloaded program and cleans its disposable files. Protocol reference:
[HTTP range semantics](https://www.rfc-editor.org/rfc/rfc9110.html#name-range).

## Game versions (current source, not yet released)

Select **Home → Updates → Game versions** to install a specific published
Windows runtime for comparison or bug reproduction. This main-window page
keeps the sidebar visible and shows the installed version, release selector,
stable/preview classification, publication date, download size and release notes.
The picker includes both stable and preview builds regardless of the normal
update channel. **Load older releases** reads subsequent GitHub release pages;
launcher-only, draft, source-only and unverified packages are excluded.

Choose **Install selected version** (or **Reinstall selected version**) and review
the confirmation. It replaces the installed game runtime only. It does not
change Git branches, rebuild a revision, or replace the launcher, even if the
selected game package bundles a different launcher version. Imported `Data`,
the ISO, `Saves`, existing `Settings` and textures are preserved by the existing
transactional installer. Save formats can differ across game versions: back up
your saves before comparing older builds. Unpublished development commits are
not downloadable from this list; the source checkout remains unchanged.

The same checksums, manifest validation, resumable downloads, pause/retry actions,
disk-space checks and rollback rules apply. Download progress appears on Home.
Retry retains the explicitly selected version even when it is older than the
normal update candidate. Close the game before switching; a running game blocks
installation. A failed refresh preserves loaded choices and leaves installed
files alone. Normal **Check updates** still offers newer releases only and never
silently reverts an installed game. No version is installed without an action.

`launcher-tests.exe --versions` runs the isolated catalog and native-page checks.
`TimeSplitters Launcher.exe --render-versions-preview preview.png` renders the
sample version picker without accessing GitHub or an installed game.

## What's new (launcher v1.2.11)

**What's new** in the sidebar and on Home opens an embedded release-notes page;
the sidebar stays visible. Choose **Game releases** or **Launcher releases**,
then **Available release** or **Installed release**. The version overview compares
the installed game or running launcher with the latest compatible release on the
Home update channel. It distinguishes newer updates, up-to-date installations,
versions newer than that channel and missing/unversioned installations. If Stable
has no game release, notes for an installed preview remain available separately.

The page reads published GitHub release descriptions; it never reads main or
invents a change summary. Latest game/launcher selection follows the existing
updater rules, including launcher metadata verification. Installed notes use the
exact version, looking up an older release directly when it is outside the latest
100 records. Unavailable or unpublished versions are not replaced with another
version's notes. Release dates and titles appear when published.

Headings, bullets and code blocks are displayed as native text. Images are omitted;
HTML, scripts, embedded XAML and automatic link navigation are not executed.
Descriptions are limited to 64,000 characters / 512 lines, with an explicit link
to read the complete release if shortened. **View release on GitHub** opens the
selected release only when clicked. **Known issues** displays a published heading
such as `## Known issues` or `## Known limitations`; if none exists, the page says
so rather than claiming there are no issues. Other sections remain in the change
notes. Open GitHub for the original Markdown formatting and linked information.

**Refresh notes** has a 25-second cancellation deadline. A failed refresh retains
previously loaded notes for the same version/channel for this session and marks
them with their last checked time; a first offline load shows retry guidance.
These notes are held in memory, not saved for the next launcher session. Switching
channel or installation version resets that view, and cancelled older requests
cannot replace its newer result. Updates are still installed from Home or the
startup updater; reading notes does not install anything or change settings.

Developer preview: `--render-news-preview output.png` works through the bootstrap
or app, with labeled sample releases and no network/filesystem maintenance.
`launcher-tests.exe --news` exercises synthetic release selection, exact-version
lookup, failures, safe text handling and embedded navigation without real game
files. The standard `build-launcher.ps1 -Test` includes these checks.

## Repair / Verify tabs (launcher v1.2.12)

The sidebar has one **Repair / Verify** entry with two tabs in the main window:
**Repair / Verify** for file checks and repairs, and **Diagnostics** for system
information, report copying, bug reports and enhancement requests. There is no
separate Diagnostics sidebar button. Opening the sidebar entry or a recovery
shortcut selects Repair / Verify by default; diagnostic developer previews still
open Diagnostics directly.

Both tabs keep **View logs** visible in the footer, including during an operation.
Switching tabs preserves verification results and the diagnostic report. The
report is collected on the first Diagnostics selection; use **Refresh diagnostics**
to update it after a file check or system change. File operations lock tab switching
while keeping **Cancel operation** and **View logs** usable. The sidebar remains
visible, and navigating away preserves unsaved Game settings drafts.

## Repair / Verify (launcher v1.2.8)

The **Repair / Verify** sidebar page stays inside the main window. Its **Open game
folder** button sits below the verification and repair cards and opens the
installation folder for inspection. Choose
**Verify files** separately for the game runtime or launcher. Verification works
offline, compares SHA-256 hashes with the installed manifest, reports missing,
changed or unreadable release-owned files, and creates or changes no files.
Game checks exclude personal settings and independent launcher files. A matching
installed manifest is an integrity check; it is not a fresh publisher signature.
Imported game data, ISOs, saves and texture packs are not scanned or repaired.

The current source layout uses separate game and launcher status cards. Each shows
**Not checked**, **Verified** or **Needs attention**, with version and checked-file
counts after a scan. **Verification details** stays collapsed until needed; a check
that finds a problem opens it automatically. Expand it at any time to inspect the
file-level results. The page scrolls at smaller window sizes and keeps operation
status and cancellation available below the content.

After verification, **Repair files** downloads the exact installed game release,
or the exact running launcher release, from GitHub. Draft, unavailable or
unpublished releases cannot be substituted with a newer/older version. The
archive size and published SHA-256, package layout and every staged file hash
must verify before installation. An unpublished development version cannot repair itself
from GitHub until its exact version is published. Repair is explicit, including for
internally consistent fallback artwork builds.

Close the game before repair. Downloads, verification and extraction can be
cancelled; installed files stay unchanged until the existing updater helper
applies the verified transaction and reopens the launcher. Game repair replaces
only changed/missing owned runtime files and the official BUILD manifest; it
preserves all settings, saves, extracted data, ISOs, textures, independent
launcher files and untracked files. Modified release-owned runtime files may be
replaced. The previous game rollback is retained. Repair has separate
`.launcher/repair-transaction` and `repair-rollback` folders; interrupted repairs
recover on startup, and repair backups use the existing confirmed-game cleanup
rules. Existing unknown/pending updates are retained, rather than overwritten.

## Diagnostics (launcher v1.2.8)

**Repair / Verify → Diagnostics** shows running/installed versions, Windows and process architecture,
CLR version, GPU/driver information when available, free space, embedded artwork,
saved graphics settings, last verification results and recent structured launcher
exception types. Graphics discovery runs in the background with a bounded wait;
missing system information does not block the launcher. Use **Refresh diagnostics**
after verifying files, then **Copy diagnostic report** to put the displayed text
on the clipboard. It is never uploaded automatically.

Choose **Report a bug** or **Request enhancement** to open the matching GitHub
issue form. The launcher converts the displayed filtered report into the optional
**Launcher diagnostics** field and also copies the complete text to the clipboard.
You fill in your description, select the requested version/platform choices,
review the diagnostics and submit on GitHub using your account. The launcher
needs no GitHub token and never creates or submits an issue automatically.
Clicking a report button opens a browser draft and sends the prefilled diagnostic
text to GitHub; review the displayed report before choosing it.

Long reports use a short browser link with a clearly marked summary. Paste the
complete copied text into **Launcher diagnostics (optional)** before submitting;
the full report is never silently truncated. If clipboard access is unavailable,
the report stays visible for manual copying. Refresh diagnostics after collecting
new verification results. Bug/enhancement description fields remain blank so the
launcher does not invent symptoms, reproduction steps or feature requests.
Both issue templates include the same optional plain-text diagnostics field;
they must be on the repository's default branch before publishing the launcher.
The prefill uses GitHub's documented [issue-form URL query fields](https://docs.github.com/en/issues/tracking-your-work-with-issues/using-issues/creating-an-issue#creating-an-issue-from-a-url-query).

The report excludes raw logs, personal/installation paths, arbitrary error
messages, file contents, save data and ISO information. Recent errors contain
only recognized UTC timestamps and exception types. **View logs** remains
available on Home and in both Repair / Verify tabs for local investigation.

Developer previews: `--render-repair-preview output.png` and
`--render-diagnostics-preview output.png` work through the bootstrap or app EXE.
They show sample UI state without network requests, repairs or hardware queries.
`launcher-tests.exe --maintenance` runs disposable maintenance fixtures;
`--maintenance-readonly` runs only the read-only/model/UI subset while an
interactive game must remain running. The normal `build-launcher.ps1 -Test`
includes the full maintenance suite and retains the production game-running guard.
