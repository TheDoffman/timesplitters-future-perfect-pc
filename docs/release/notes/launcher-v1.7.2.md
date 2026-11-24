Launcher 1.7.2 improves startup reliability and makes the console's Errors only
view more useful.

## What changed

### More reliable startup after an update on Mac
**Before:** On Apple Silicon Mac, the launcher could close unexpectedly when it
reopened after an update with optional texture update checks enabled.
**Now:** The launcher finishes its initial game update check before starting the
optional texture check, avoiding the overlapping checks that triggered the
reproduced crash. Texture checking remains optional and runs in the background.

### Healthy console messages stop looking like errors
**Before:** Routine messages reporting zero errors or no failures could appear
in Errors only, making a healthy session look as though something was broken.
**Now:** The filter respects the message's severity and ignores routine zero-error
reports. Error messages remain visible, and the full console log is still available.
**How to use it:** Open Tools & Logs, choose Console, then turn on Errors only.

## How to update

Close and reopen your existing launcher to check for its update. If it will not
stay open, use the download links below to get the new launcher ZIP for your
platform. Keep your existing game installation, saves, settings and textures.

[Windows x64](https://github.com/TheDoffman/timesplitters-future-perfect-pc/releases/download/launcher-v1.7.2/TimeSplitters-Launcher-v1.7.2-windows-x64.zip) · [Apple Silicon Mac](https://github.com/TheDoffman/timesplitters-future-perfect-pc/releases/download/launcher-v1.7.2/TimeSplitters-Launcher-v1.7.2-macos-arm64.zip) · [Linux x64](https://github.com/TheDoffman/timesplitters-future-perfect-pc/releases/download/launcher-v1.7.2/TimeSplitters-Launcher-v1.7.2-linux-x64.zip)

## Compatibility and limits

Available for Windows x64, Apple Silicon Mac and Linux x64. The Mac app is
ad-hoc signed and is not notarized. This is a launcher-only update: it does not
replace your game or change multiplayer compatibility. Your installed game keeps
its own system requirements and version.
