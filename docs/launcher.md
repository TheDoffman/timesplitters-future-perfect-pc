# Play with the Windows launcher

[Back to the README](../README.md)

Open the [latest Windows launcher release](https://github.com/TheDoffman/timesplitters-future-perfect-pc/releases/latest)
and download its `TimeSplitters-Launcher-…-windows-x64.zip` asset. Extract it
completely into a writable folder and open **TimeSplitters Launcher.exe**.
Keep the companion files in `Bin` and `Support` with it. Windows 10/11 x64 and
.NET Framework 4.8 are required; no developer tools or GitHub login are needed.

## Windows security prompts at startup

The recovery controls below are in current source for the next launcher update.

If Windows cancels opening the launcher application, the startup window stays
open with **Retry**, **Open launcher folder** and **View logs**. A cancellation
message does not prove that you clicked Cancel: SmartScreen or another security
prompt may need attention. The launcher cannot determine the exact prompt from
Windows' cancellation code alone.

Choose **Open launcher folder**, then open `Bin\TimeSplitters.App.exe` directly
to check whether Windows shows a prompt. Review that prompt and confirm the
download came from this project's official release before deciding whether to
allow it. Return to the startup window and choose **Retry**. Retry opens the
installed launcher again without repeating the update download. You can also
close the startup window and open the launcher normally afterward.

Access-denied errors and missing application files have separate guidance.
**View logs** opens the selected installation's `.launcher` folder, containing
`launcher.log`. Restoring missing launcher files requires the complete launcher
package, with its `Bin` and `Support` folders together. This recovery UI does not
change Windows security settings or unblock files automatically.

## First-time setup

1. Keep **Stable** under **Update channel** for the default game release.
   Launcher 1.3.0+ also offers named opt-in previews for testing.
2. Select **Install release**. The launcher downloads a published Windows game
   runtime from this project's GitHub Releases and checks its integrity before
   installing it. It does not download original game data.
3. Select **Locate ISO**, or **Use extracted files**, and choose your supported
   US Xbox game. An extracted folder must contain `default.xbe`, `pak` and `xmv`.
4. Wait for the import to finish and select **Play Game**.

The launcher verifies and imports the data once. The ISO is no longer needed
for subsequent play; the imported data must remain installed. The launcher
minimizes after detecting the game window and restores itself when the launch
finishes. A failed startup leaves status and logs accessible.

Use **F2 inside the game** to edit keyboard/mouse controls. Resolution is under
**Settings → Display settings**, or **Options → Display settings** while paused.
See the [Windows settings guide](windows-settings.md) for the full controls.

## Everyday play and updates

Open **TimeSplitters Launcher.exe** and select **Play Game**. An installed game
can still be launched when a release check fails or you are offline.

**Stable** offers stable game releases; general **Preview** and named previews
follow their own testing channels in launcher 1.3.0+. The launcher checks published releases, not development branches.
A newer GitHub commit does not become an installed game update until a compatible
release is published. Launcher and game versions are separate.

Use **Check updates**, review **Release notes**, then select **Update release**
when offered. Close the game before installing updates or restoring a previous
release. The launcher downloads and verifies an update before applying it and
preserves imported data, saves and existing settings. On Windows and Mac, the
launcher closes after preparation and automatically reopens after installation,
so Play always uses the newly installed launcher. Repairs and restoring a previous
game use the same restart flow. Cancelling a download does not restart it.
Leave room for staging and one rollback snapshot in addition to
the installed game. Optional texture updates also need temporary space for one part.

Launcher updates are checked separately at startup. **Open installed launcher**
lets you continue with the installed launcher while a startup check is underway.

## Enhanced textures (upcoming launcher update)

Windows and Mac use the same optional pack. Under **Game settings → Graphics &
resolution**, select **Check texture downloads**, then **Download / update textures**.
Review the confirmation. Progress supports cancellation; retry resumes verified
parts. A download enables **Check for texture updates when opening settings**.
Checks never start an update without confirmation. Turning checks off keeps the
installed pack. Downloads and this preference take effect immediately.

Save **Use enhanced textures (Beta)** to use installed replacements next launch.
Classic is the default. The toggle does not download or delete files. F10 and
in-game Texture style changes affect only the current session. Missing replacements
always use originals; this incomplete Beta may contain visual issues.

Existing packs from bundled game releases are recognized by file hashes. Matching
files require no download; only parts containing new or changed files are fetched.
Unchanged pairs within those parts are not rewritten. Official updates replace
matching keys and preserve unrelated custom keys, saves and settings. Each part is
validated before installation and rolls back on failure; completed parts survive
cancellation. Game upgrades, repair and rollback preserve the installed pack.

The optional pack must be published before downloads become available. Until then,
the launcher explains its availability and continues to support installed packs.

## Compare specific game versions (upcoming launcher update)

Under **Home → Updates**, select **Game versions**. Choose a published stable or
preview Windows release, read its notes and select **Install selected version**.
Use **Load older releases** if the version is further back in the list. Selecting
the installed version offers **Reinstall selected version** instead. Review the
confirmation before the download begins; progress and pause/retry appear on Home.

This replaces the installed game runtime while keeping your launcher, ISO,
imported game data, saves and settings. Close the game first. Back up saves before
testing older builds because save compatibility can vary. You can choose a newer
release again from the same page or use the normal update controls to return to
the latest release. Published versions need no Git checkout or rebuild; unpublished
development commits are not included. Your development source checkout is unchanged.

## What's new (launcher v1.2.11)

The sidebar's **What's new** page shows game and launcher release notes inside
the same window. It compares installed and available versions for your selected
channel. Switch between **Game releases** and **Launcher releases**, then choose
**Available release** or **Installed release** to read that exact version's notes.
Published known issues appear in their own section when supplied by the release.
**Refresh notes** checks GitHub again; a failed refresh keeps notes already loaded
for this session and shows their last checked time. **View release on GitHub**
opens the original release. Use **Home** in the sidebar to return to update controls.
This page reads release information; installing an update remains a separate action.

## Saves, logs and recovery

- **Saves** opens the local save folder.
- **Repair / Verify → Open game folder** opens the installation directory.
- **Repair / Verify** contains file-check/repair and **Diagnostics** tabs in launcher v1.2.12; Diagnostics includes system reports and GitHub report buttons.
- **View logs** opens the local launcher log folder from Home or either maintenance tab; game logs are also under `Logs`.
- **Support** opens this project's GitHub issues.
- **Restore previous** swaps back to the retained game release when available,
  preserving your data, saves and settings.

Keep the whole installation together, especially `Data`, `Settings`, `Saves`
and `.launcher`. The latter contains updater recovery and rollback records;
retain it throughout updates and recovery. Normal updates do not require manually
copying game files or reimporting your ISO.

If **Play Game** is unavailable, check that both a game release and verified US
Xbox data are installed and that another game instance is not already running.
An unsupported or incomplete existing `Data` folder is retained for inspection,
not overwritten. Check the status message and logs before changing files.

## Startup error after a successful ISO import

Launcher 1.2.12 can report `CannotCreateTypeConstrainedLanguage` from `Launch.ps1`
on a fresh installation. If the log already says **Game data imported successfully**,
the failure is in the launch script, after import. Open **Settings**, save your
current settings once, and try **Play Game** again; this selects that version's
direct launch path. Reimporting the same ISO does not address this error.

Launcher 1.2.13 fixes [#128](https://github.com/TheDoffman/timesplitters-future-perfect-pc/issues/128)
by using direct game startup for fresh installations too. This change needs a
published launcher update before installed copies receive it; it does not require
changing Windows security policy. The legacy standalone `Launch.ps1` still uses
PowerShell; use **Play Game** in the launcher.

## Report a problem or request an improvement

Use the [bug report form](https://github.com/TheDoffman/timesplitters-future-perfect-pc/issues/new?template=01-bug-report.yml)
for problems or [start an Ideas discussion](https://github.com/TheDoffman/timesplitters-future-perfect-pc/discussions/new?category=ideas)
for enhancements. Search existing issues or [Ideas](https://github.com/TheDoffman/timesplitters-future-perfect-pc/discussions/categories/ideas)
first and report one problem or suggestion at a time.

For a bug report, the version and platform text fields start empty and require
an answer. Copy your **game version** from the installed version shown in the launcher,
then copy your **launcher version** from **LAUNCHER v...** at the top of its
window. These can be different. If you cannot find either one, choose
**I don't know**. You can enter an older or custom build directly. Enter your
operating system under **Platform**. The collapsed **Published versions** list
is a reference that refreshes automatically as releases are published; it does
not fill in an answer for you.

Tell us what happened and how to make it happen again, or describe your idea.
Screenshots and logs are optional; **View logs** in the
launcher opens its log folder. Remove personal information before attaching
anything, and keep game files and saves private.

Improvement suggestions belong in Ideas, with one post per suggestion. Versions,
platform and launcher diagnostics are not requested. Bug reports receive type
and platform labels automatically.

The next launcher update routes **Request enhancement** directly to a new Ideas
discussion on Windows and Mac. Until that update is installed, use the Ideas link
above if your launcher still opens the old enhancement issue URL.

## Older portable packages and source builds

A standalone game runtime ZIP is an alternative for manual installation. Extract
the entire package and use its **Play.cmd** entry point; older packages may open
the importer and game directly instead of the graphical launcher. On first use,
choose the supported ISO or extracted game folder. Preserve `Data`, `Settings`
and `Saves` when manually upgrading, and keep `Support` and the licenses with
the package. This is separate from the recommended launcher-managed setup above.

The repository's developer **Play.cmd** starts an existing local build. Follow
the [build guide](building.md) if you are working from source; downloading the
source repository alone does not install a playable game.

## Native frame-rate selection (upcoming launcher update)

Open **Game settings → Graphics & resolution → Frame rate** to select
**60 FPS (Original)**, **120 FPS (Beta)**, or **144 FPS (Beta)**, then save.
The original 60 FPS path is the default and the recovery option. High-refresh
mode requires a game build containing the `feature/native-144-fps` runtime;
installing this launcher alone does not add that capability to older games.
It targets fresh game frames without reviving the retired interpolation engine.
Actual FPS depends on the game build, hardware and display. The desktop refresh
rate is unchanged.

Native high refresh remains beta. The development branch has a reported Arcade
movement/firing regression, and complete gameplay validation is still pending.
Save and restart the game to apply changes; return to **60 FPS (Original)** if
problems occur.

The launcher shares `pc-frame-rate.ini` with the new in-game frame-rate menu,
beside `pc-display.ini` in the installation's settings folder. Its versioned
format is `TSFP_FRAME_RATE_V1 0`, `120`, or `144` (zero selects original timing).
Reopen launcher settings after changing the choice in the game. Concurrent
changes are rejected instead of overwritten, and the preference participates
in the existing backup and multi-file rollback transaction. Cancel leaves it
unchanged. Launch uses the saved choice as `TSFP_NATIVE_FPS`; zero explicitly
overrides older native-FPS runtime overrides. Legacy presentation keys retain
their safe fixed-60 migration values for compatibility with older game packages.
The common settings model and page serve both Windows and Mac.


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
