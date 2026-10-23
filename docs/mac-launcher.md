# Native Mac launcher

The native Apple Silicon launcher ports the Windows v1.2.18 interface using
Avalonia. Its home layout is generated directly from `tools/launcher/Launcher.xaml`;
artwork, placement, colors and labels come from that Windows source. The native
settings, release notes, version browser, maintenance and startup windows use
Mac controls. The local build includes native Discord, crash/hang capture, rich release notes,
and isolated end-to-end tests. [One release workflow](launcher-releases.md) builds
and publishes Windows and Mac launcher updates together; native game delivery
and Developer ID/notarization remain separate.

## Build and run

From the checkout root on Apple Silicon:

```sh
python3 tools/build-mac-launcher.py --install-sdk --test
open 'build/macos-launcher/TimeSplitters Launcher.app'
```

The SDK installs only under ignored `external/dotnet`, using the SHA-512 pin in
`config/mac-launcher-sdk.json`. Avalonia and transitive packages are locked in
`tools/launcher/macos/packages.lock.json`. The standalone app includes .NET;
users of the built app do not need to install the SDK, Python or CrossOver.
Source-checkout Vulkan builds additionally require their existing Python/SDK
verification helper. The SDK, NuGet cache, generated app and intermediate output
are build dependencies, not files to transfer between Windows and Mac.

Use `--dotnet /path/to/pinned/dotnet`, `--output path`, and `--artwork-dir path`
to select existing dependencies or the output location. The artwork directory
accepts the unchanged embedded resources `launcher-banner.png`,
`launcher-backdrop.png`, `launcher-logo.png`, and optional `launcher.ico`.
`--require-artwork` rejects missing artwork. Source builds without private assets
use the original vector fallback. The icon converter retains the original PNG
entries without redrawing or upscaling them. Keep original assets and provenance
under ignored `private/launcher-art`.

In a development checkout the app discovers the project by walking its parent
folders, or accepts `--root /path/to/checkout`. It runs
`build/macos/futureperfect_recomp` and preserves `private/game`, `private/saves`,
`config/pc-controls.json`, `config/pc-display.ini`, and
`private/native-settings/runtime.conf`. Startup does not create defaults or touch
those personal files. Play creates missing defaults only; settings saves are
explicit, backed up, checked for external edits, and rolled back on failure.
The app minimizes after process creation and restores after exit. It bounds the
two reused game logs to 4 MiB each. Existing 60 FPS interpolation preferences
are retained; absent native preferences default to 2x interpolation.

The native executable hash, Mach-O ARM64 identity, supported US XBE hash, and
nonempty `pak`/`xmv` folders are checked before launch. Vulkan checkout builds use
`tools/vulkan_sdk.py` to verify the existing pinned SDK receipt and select its
KosmicKrisp loader. No SDK download, renderer rebuild or switch occurs at launch.
A successful prerequisite check is not gameplay or graphics validation.

```sh
'build/macos-launcher/TimeSplitters Launcher.app/Contents/MacOS/TimeSplitters.Launcher' --check --root .
```

## Shared behavior and distribution

`LauncherPlatform.cs` contains package names, paths and Mac atomic replacement.
Version ordering, Stable/named-preview filtering, original-game import, checksum/path
validation, resumed downloads, update journals, rollback, settings models,
diagnostics filtering and issue-draft preparation use the existing Windows
source. Native keyboard/mouse events map to the shared binding codes, including
134/135 for the wheel and the shared reserved-key/conflict rules. The Discord
tab edits the same optional, off-by-default preferences as Windows. Native local
IPC uses the same bounded protocol, activity decoder, rate limit and reconnect
policy; the running Discord desktop client is required. No login/token is used.

The app requests only `macos-arm64` assets. Windows releases are never installed
or run by this launcher. The existing checkout stays usable when no compatible
published Mac release exists. Release installs and self-updates are disabled for
development checkouts so they cannot overwrite source or native build work.

Portable game packages use `Bin/futureperfect_recomp`, `Launch.command`,
`Settings/pc-controls.json`, `Licenses`, and a hashed `BUILD.json`; personal data
uses `Data`, `Saves`, `Settings`, and `Textures`. A new native runtime package,
including redistributable dependencies and provenance, is assembled on Apple
Silicon with `tools/package-native-mac.py`. It bundles the verified Vulkan loader,
MoltenVK, DXC and native dependencies, checks their hashes before launch, and
requires no Python, Homebrew or SDK on the player's Mac. Launcher 1.2.21 adds
this portable Vulkan loading path. The launcher alone cannot supply a game runtime.
See [native packaging](macos.md#native-release-packaging) and MAC-012 for validation.

`--package` additionally creates a launcher ZIP, SHA-256 sidecar and
`launcher-update-macos-arm64.json`. Keep the `.app` beside `Support/Launcher` in
a portable installation. Mac launcher updates use an independent release asset
and manifest, an eight-second startup check and bounded resumable downloads.
The previous launcher remains until a successful verified startup; helpers apply
updates after the old process exits. Original game data, saves and config are
never launcher-update targets. Building does not publish a release.

Game installation, repair and restoring a previous game also close and reopen
the launcher automatically. The copied `.launcher/mac-update-helper` waits for
the old process to exit, takes the installation lock, verifies/applies the staged
files, and starts the installed app with the same installation folder. This also
loads a newer launcher bundled with a game update instead of keeping old code in
memory. The shared `UpdateRestart` policy is used by Windows and Mac. A failed
game update reopens the retained installation and shows its failure result;
cancelling before installation leaves the current launcher open. Relaunch skips
one startup update check to avoid retry loops.

When macOS opens a downloaded app under `AppTranslocation`, its apparent parent
folder is temporary and read-only. Startup now offers **Choose installation
folder** before checking updates or writing launcher state. Select the real
extracted folder containing both the `.app` and `Support`, not the `.app` itself.
The launcher verifies the manifest, matching running executable and write access
before continuing against that folder. Invalid selections leave existing files
untouched; cancelling keeps the picker available. The selection applies to this
launch and does not move or copy an installation.

The existing v1.2.20 release predates this recovery screen. If it reports a
read-only `AppTranslocation/.../.launcher` error, quit it and use Finder to move
the `.app` itself out of the extracted folder and back, keeping `Support` beside
it, then reopen it. Moving only the outer folder may leave translocation active.
Source-checkout users can also continue using the documented explicit `--root`
path. This is separate from a Gatekeeper refusal to open the app; no quarantine
attributes or system security settings are changed.

The corrected builder signs the complete app bundle with a local ad-hoc signature.
Mac v1.2.19 shipped only a signed executable and requires a one-time manual
replacement when the corrected package is released; its updater rejects the new
bundle seal. See [signing and migration details](launcher-releases.md#mac-application-signing-and-the-v1219-migration).
Developer ID signing, notarization,
clean-machine installation and real published upgrade/repair/rollback remain
release acceptance checks. Local packaged install, helper-based repair/restart,
transaction rollback and native game lifecycle checks are covered.

Fatal signals notify a prestarted external reporter using preallocated shared
memory. Reports retain native ARM64/Xbox registers, executable SHA-256, matching
PC/LR symbols, a bounded native process sample and a 64 KiB log tail. The native
builder retains an adjacent dSYM with UUID/hash provenance. Hang observation uses
the shared 60-second/180-second policy, focus/modal suppression and sleep grace,
leaving the game running. Diagnostics opens/exports the latest local report;
export includes at most 256 KiB of native context. Nothing is uploaded. Native
context files replace the Windows-only minidump format. Unexpected exits still
use the same durable session records and uncertain-cause summaries as Windows.

## Validation

`--test` executes synthetic settings, native package, integrity, rollback,
download and platform-selection checks in a disposable directory alongside the
app. It never starts the game or uses personal saves/settings. The workflow runs
these checks on an ARM64 Mac runner alongside existing Windows launcher checks.

The executable also supports `--render-preview output.png`,
`--render-game-data-preview output.png` (expanded Home game-data section),
`--render-download-preview output.png` (sample transfer status),
`--render-download-compact output.png` (the same transfer at the minimum
1000 × 720 layout size), and `--render-download-compact-scrolled output.png`
(the compact view scrolled to the download controls),
`--render-settings-preview output.png`, `--render-settings-graphics output.png`,
`--render-settings-discord output.png`, `--render-news-preview output.png`,
`--render-diagnostics-preview output.png`, and `--render-repair-preview output.png`.
The modal previews `--render-confirm-preview output.png`,
`--render-confirm-long-preview output.png`, and `--render-alert-preview output.png`
render the actual confirmation/error dialog content and then close without
performing an operation. Long messages scroll above a fixed action row.
Pass an explicitly disposable `--root` when rendering settings. Preview mode
performs no network requests, startup cleanup or saves. The news preview renders
the actual page controls, including its toolbars and release selector. Run actual game tests
through `tools/test-native-mac.py` with its isolated paths and bounded duration;
do not automate Play against personal saves.

The completed adapter and end-to-end evidence is recorded in
[STATUS.md](../STATUS.md#native-launcher-parity-completion-2026-10-19).

The end-to-end wrapper uses `test-native-mac.py`'s locked, disposable Test Lab
profile and both isolated display configurations. It launches the real app/game,
checks native window observation, minimize/restore and exit handling, and
recycles only disposable virtual disks after all child processes exit:

```sh
python3 tools/test-mac-launcher.py --launcher 'build/macos-launcher/TimeSplitters Launcher.app/Contents/MacOS/TimeSplitters.Launcher' --mode quit
python3 tools/test-mac-launcher.py --launcher 'build/macos-launcher/TimeSplitters Launcher.app/Contents/MacOS/TimeSplitters.Launcher' --mode crash
```

`--discord` explicitly publishes this disposable session's activity to the running
Discord client and requires its acknowledgement. `--story --seconds 75` adds
synthetic SDL navigation into Time to Split and checks cutscene/gameplay/pause
activity; it is not physical input or campaign-completion validation.
`--seconds` bounds the run; `--runtime-root` selects an existing same-host checkout's local data/build.
The native builder accepts the same `--runtime-root` to reuse dependencies and
the single `build/macos` directory from a focused source worktree. It records the
source worktree revision/hashes and never copies game data or virtual environments.
Use the bounded wrapper, never automate Play against the personal installation.


## Stable and named game previews

Launcher 1.3.0 source adds independent game channels on Windows and native Mac.
New installations default to **Stable**; use the game update channel selector to
choose **Preview**, then select **Beta** or a named feature such as
**Story Co-op** under **Testing track**. Each track receives only its own updates.
The track selector appears only for Preview; switching through Stable remembers
the previously selected track. Old Preview preferences
migrate to Beta without enabling launcher preview updates. Historical alpha and
`-preview.N` versions remain available only in Game versions; no release-candidate type is required.
Select Stable and confirm the channel switch to install its latest game, even
when its version is lower than the installed preview. **Game versions** still
allows exact-version installation. Missing channels never silently substitute
another channel. Saves/settings/game data/textures are preserved; back up saves
before testing because preview save/network compatibility can vary.
See [versions, publishing and recovery](releasing.md) for the full process.
