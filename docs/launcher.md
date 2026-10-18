# Play with the Windows launcher

[Back to the README](../README.md)

Open the [latest Windows launcher release](https://github.com/TheDoffman/timesplitters-future-perfect-pc/releases/latest).
Installer support is available in current source for the next launcher release.
Choose the Setup EXE when available; older releases still use the portable ZIP.
Windows 10/11 x64 and .NET Framework 4.8 are required; no developer tools or
GitHub login are needed to install or play.

## Install with Setup (upcoming launcher release)

Download `TimeSplitters-Launcher-Setup-windows-x64.exe` (or the versioned
`TimeSplitters-Launcher-Setup-v…-windows-x64.exe`) and run it. Setup installs for
your Windows account without requesting administrator access. The default is
`%LOCALAPPDATA%\Programs\TimeSplitters Future Perfect`; you can choose another
local folder that you can write to. The game runtime, imported data and personal
files use that same installation folder.

Setup adds a Start menu shortcut and an optional desktop shortcut, plus an entry
in Windows **Installed apps**. Open the launcher from either shortcut and follow
the first-time setup below. Automatic launcher and game updates continue working;
you normally only need Setup once. Windows' displayed launcher version follows
verified launcher self-updates.

For an existing portable installation, select its existing folder in Setup to
reuse the installed game, settings, saves and imported data. Nothing is moved or
copied from another installation automatically. Rerunning the same installer
repairs launcher files; an older installer refuses to replace a newer launcher.
Close the game and launcher first. An unfinished updater transaction must be
recovered by opening the installed launcher before running Setup again.

Uninstall through Windows **Installed apps**. It removes the launcher, its
installer registration and its shortcuts. Downloaded game files, imported `Data`,
the ISO, `Saves`, `Settings`, textures, logs and download/recovery records are
kept in the selected folder. Reinstalling into that folder reuses them. Uninstall
does not wipe the installation folder recursively.

## Portable ZIP

Download `TimeSplitters-Launcher-…-windows-x64.zip`, extract it completely into
a writable folder and open **TimeSplitters Launcher.exe**. Keep its `Bin` and
`Support` files together. This option remains supported and does not add shortcuts
or an Installed apps registration. The launcher's automatic updater still uses
verified ZIP packages internally for both kinds of installation.

## First-time setup

1. Choose **Preview** under **Update channel**. Current game releases are
   prereleases, so they do not appear in Stable.
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

**Stable** offers stable game releases; **Preview** also includes published
prereleases. The launcher checks published releases, not development branches.
A newer GitHub commit does not become an installed game update until a compatible
release is published. Launcher and game versions are separate.

Use **Check updates**, review **Release notes**, then select **Update release**
when offered. Close the game before installing updates or restoring a previous
release. The launcher downloads and verifies an update before applying it and
preserves imported data, saves and existing settings. It may restart during
installation. Leave room for staging and one rollback snapshot in addition to
the installed game; optional textures also need their own space.

Launcher updates are checked separately at startup. **Open installed launcher**
lets you continue with the installed launcher while a startup check is underway.

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

## Report a problem or request an improvement

Use the [bug report](https://github.com/TheDoffman/timesplitters-future-perfect-pc/issues/new?template=01-bug-report.yml)
or [enhancement request](https://github.com/TheDoffman/timesplitters-future-perfect-pc/issues/new?template=02-enhancement.yml)
form. Search existing issues first and report one problem or suggestion at a time.

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

Improvement suggestions only ask for your idea. Versions, platform and launcher
diagnostics are not requested. Reports are labeled
automatically by type; bug reports also use the selected platform.

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
