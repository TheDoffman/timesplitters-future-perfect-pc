# Steam Deck launcher (experimental)

The redesigned native Linux launcher includes a Steam Deck layout for SteamOS
Gaming Mode and Desktop Mode. Physical Deck acceptance is still pending: the
automated Linux checks do not establish built-in input, Gamescope focus,
suspend/resume or game performance on a Deck.

## First setup

1. In Desktop Mode, extract the complete Linux x64 launcher ZIP into a writable
   folder you own, such as `~/Games/TimeSplitters`. Keep `Support` beside
   `TimeSplitters.Launcher`. Do not install into the SteamOS system image or
   disable its read-only protections.
2. Add `TimeSplitters.Launcher` to Steam as a non-Steam game. Keep its target and
   Start In folder pointing at this installation. This is a native Linux build;
   leave forced Proton compatibility off.
3. Select a **Gamepad** Steam Input layout for the shortcut and start it through
   Steam. Touch and trackpads also work, including during setup. Desktop Mode's
   mouse/keyboard controller layout can otherwise keep the controls in mouse mode.
4. Follow the setup guide to install the Linux game and import your supported
   US Xbox ISO or extracted game folder. Original game data is not included.

In Gaming Mode the launcher fills the screen. In Desktop Mode it remains a
normal window and remembers its desktop size and position. Gaming Mode does
not overwrite that desktop placement. The launcher stays alive while the game
runs, then becomes available again after the game exits.

The Linux game package currently has its own driver and glibc baseline; see
[the Linux launcher requirements](linux-launcher.md). A launcher layout alone
does not establish that every SteamOS release can run the game package.

## Controls

| Control | Launcher action |
| --- | --- |
| D-pad or left stick | Move focus; left/right adjusts a focused slider |
| A | Select, toggle, open a list, or confirm the list selection |
| B | Back or cancel; on Home, ask before closing the launcher |
| LB / RB | Previous / next control |
| Right stick | Scroll the current section |
| Y on a text field | Open the built-in keyboard |
| Y on keyboard binding capture | Choose a keyboard key without an external keyboard |

The focus outline follows controller navigation. Steam and Quick Access buttons
remain SteamOS controls. Launcher input pauses while the launcher is inactive
or the game is running. Held buttons must return to neutral after regaining
focus or reconnecting, so returning from a game does not accidentally select Play.

Controller input uses standard SDL mappings from the installed game's SDL3 or
the host's SDL3/SDL2 runtime. If no compatible runtime or controller is available,
touch/trackpad navigation stays available and the Home hint explains the state.
The launcher does not install system packages or change Steam Input profiles.

## Files, SD cards and text

The Deck layout keeps ISO, extracted-data, save-folder and report destination
selection inside the launcher. Browse Home, Downloads, or mounted drives/SD
cards; use Up a folder to navigate. Select the folder containing `default.xbe`,
`pak` and `xmv` for extracted game data. Imports still validate the supported
executable and preserve existing data.

The built-in keyboard applies text only when you choose Done. B or Cancel keeps
the original field unchanged. **Deck help** in Launcher tools opens these basic
controls and setup tips inside the launcher.

## Testing and remaining acceptance

`--deck` requests the handheld layout for testing on another Linux PC.
`--gaming-mode` requests its fullscreen behavior; `--desktop-mode` keeps a
normal window even under Gamescope. These options survive updater restarts.
They do not alter game settings or select a performance preset.

Before calling this hardware-supported, verify on a physical Deck: LCD/OLED
1280×800 readability, touch, built-in/Steam Input controls, keyboard entry,
SD-card import and saves, Gaming/Desktop launch/return, launcher/game updates,
Steam overlay/Quick Access, suspend/resume, docked output, and reconnecting input.
See Valve's [launcher and input guidance](https://partner.steamgames.com/doc/steamhardware/compat).
