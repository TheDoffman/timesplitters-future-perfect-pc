Launcher 1.8.0 introduces a redesigned interface, guided setup and simpler Linux installation.

## What changed

### A clearer home for your game, settings and tools
**Before:** Installation, versions, settings and recovery tools were spread across
the older interface.
**Now:** A redesigned TimeSplitters layout brings those actions together, with
clearer version and channel navigation, matching update windows and an explicit
Playing state. A deeper navy palette, brighter blue actions and serif typography
give the launcher a consistent new appearance while retaining its original artwork.

### Help with your first installation
**Before:** New players had to work out the installation steps and available tools
without a guided introduction.
**Now:** First-install setup walks through the required choices, and an optional
six-step tour explains the main controls. Clearer offline and retry screens help
you recover when a download or connection is unavailable.

### One Linux installer chooses the right architecture
**Before:** Linux launcher downloads assumed an x64 computer, leaving ARM64 users
without a matching native launcher or an automatic architecture choice.
**Now:** One Linux installer detects x64 or ARM64 and verifies the matching native
launcher before installing it. Updates select matching packages, and repair keeps
game data, saves and settings. Manual ZIP downloads remain available.
**How to use it:** Download TimeSplitters-Install-Linux.sh, inspect it if desired,
and run it with bash. Python 3 is required. An ARM64 launcher cannot install or
run the x64 game; a prebuilt ARM64 game is not included in this release.

### Better navigation on handheld Linux desktops
**Before:** Small controls, text entry and file selection made the desktop launcher
awkward to use with a controller or touch screen.
**Now:** Experimental SteamOS adaptations add controller focus and navigation,
larger touch targets, an inline keyboard and file/SD-card selection for Gaming
and Desktop modes. Physical Steam Deck acceptance is still pending; this is not
a Deck Verified claim.

## How to update

Close and reopen your existing launcher to check for its update. If it cannot
start, download the ZIP matching your platform and retain your existing game
installation, saves, settings and textures. Linux users can use the new installer.

[Windows x64](https://github.com/TheDoffman/timesplitters-future-perfect-pc/releases/download/launcher-v1.8.0/TimeSplitters-Launcher-v1.8.0-windows-x64.zip) · [Apple Silicon Mac](https://github.com/TheDoffman/timesplitters-future-perfect-pc/releases/download/launcher-v1.8.0/TimeSplitters-Launcher-v1.8.0-macos-arm64.zip) · [Linux installer](https://github.com/TheDoffman/timesplitters-future-perfect-pc/releases/download/launcher-v1.8.0/TimeSplitters-Install-Linux.sh)

## Known issues and compatibility

Launcher packages target Windows x64, Apple Silicon Mac, Linux x64 and Linux
ARM64. The Mac app is ad-hoc signed and is not notarized. Game downloads have
their own platform requirements; Linux ARM64 game support remains a developer
source build. Game 0.7.2 is released separately. Launcher 1.8.0 retains the
existing installation and update format and does not require a fresh game-data
import or a save conversion. Existing optional texture packs are retained.
