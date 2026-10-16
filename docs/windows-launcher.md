# Windows release launcher

`TimeSplitters Launcher.exe` is the portable Windows x64 bootstrap. Extract the
whole launcher ZIP into a writable installation folder, keeping the application
`Bin/TimeSplitters.App.exe` and `Support/Launcher/manifest.json` beside it in
their subfolders. It installs game runtime packages from the
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
assembly/file version attributes, then build. The build produces these assets
in `build/launcher`; for version 1.1.0, publish all three together:

- `TimeSplitters-Launcher-v1.1.0-windows-x64.zip`
- `TimeSplitters-Launcher-v1.1.0-windows-x64.zip.sha256`
- `launcher-update.json`

Use a GitHub Release such as `launcher-v1.1.0`. Mark test builds as prereleases;
stable launcher versions must also have no semver prerelease suffix. Releases
may contain a game ZIP too; launcher versions and game tags are independent.
The main application lists/filter releases so a launcher-only release cannot
hide a game release. Nothing is read from a branch or an unpublished draft.

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

## Installation and recovery

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
evidence. Successful updates retain one `.launcher/rollback` snapshot; Restore
previous swaps releases without replacing user settings. The downloaded ZIP is
discarded after successful extraction, and staging is discarded after install.
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
