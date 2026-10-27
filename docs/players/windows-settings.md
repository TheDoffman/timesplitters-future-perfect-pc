# Windows controls and display settings

[Back to the README](../../README.md)

## Launcher settings

The launcher, its startup window and the windowed game title bar follow
Windows' **app mode** (Light or Dark), including changes made while they are
open. This changes the title bar only; the launcher's interior keeps its own
theme. High-contrast mode uses the standard light title-bar setting. Windows
versions without the optional DWM theme attribute retain their normal title
bars. There is no separate game preference to save.

The game's **PC Settings** menu bar and dropdown, **Keyboard & Mouse** and
**Debug** windows (including the binding-capture dialog) also follow app mode.
Their backgrounds, text, lists and controls use a dark palette in Dark mode
and return to system colors in Light/high-contrast mode. The separate **Online**
window and its invitation dialog follow the same preference, checking for
changes about once a second. The game graphics and launcher interior retain
their existing appearance; Mica/Acrylic is not used.
Dark controls retain Windows-style typography and layout, with rounded surfaces,
blue checkbox accents and focus/hover feedback. Online uses Vista control
layouts with custom dark drawing rather than switching to the classic Clam
appearance. Light mode restores the original native styling.

The current source launcher groups **Game settings** into two tabs with a fixed
Save/Cancel footer. **Keyboard & mouse** contains Input & mouse and the key-binding
table, with four labeled slots per action. **Graphics & resolution** groups Display,
Camera & window, Quality & performance, and Startup into separate cards. Expand
**Custom resolution** to edit width and height; the preset picker and aspect filter
remain above it. Save applies the draft on the next launch; Home retains it and
Cancel discards unsaved edits. The sidebar stays visible while editing.

## In-game display settings

These instructions describe the current Windows source build. Downloaded releases may predate individual features.

Rendering uses fixed 60 FPS. High-FPS motion, interpolation and monitor-refresh
selectors have been removed. The desktop refresh rate stays unchanged.

Open **Settings → Display settings** from the main menu, or **Options → Display
settings** while paused. Choose an **Aspect ratio** group (All, 4:3, 5:4,
16:9, 16:10, 21:9 or 32:9), then use the resolution row's left/right arrows to choose
a mode, including **Original** for the game's default. Choose **Apply settings**, then
**Keep resolution** within 15 seconds. Back, Revert, or letting the countdown
expire restores the previous mode. Only confirmed settings are saved.

The **Vertical FOV** slider adjusts the gameplay view from **40° to 100°**, with a
numeric value and **Reset FOV to 55** for the original view. Select the row and
use left/right on the keyboard or controller, then **Apply settings**. Back
without applying discards the draft. FOV-only changes save immediately; changing
resolution too uses the same 15-second confirmation and rolls both settings back
if rejected. The launcher offers the same slider under **Game settings → Graphics
& resolution**; **Save settings** applies it on the next launch.

FOV is measured vertically, so the horizontal view also follows the selected
aspect ratio. Weapon zoom keeps its relative magnification. Cinematic framing,
terminal views and menu proportions retain their original camera behavior.
Existing display files keep the original 55° setting. Non-default FOV is stored
in the version 2 display format; both the runtime and launcher read versions 1
and 2. Resetting FOV to 55° writes the legacy version 1 format again.

The Windows launcher and in-game picker share a catalog of common render sizes,
from 640×480 through 7680×4320, including 16:9, 16:10, 21:9 and 32:9 presets.
They also include modes reported by the primary display. The aspect group filters
the list and stages a nearby resolution; it does not stretch one resolution to a
different ratio. The selected width and height determine the actual picture ratio.
Custom landscape sizes up to 4:1 remain supported.
Refresh-rate duplicates are removed, the console default appears first, and saved
custom modes remain available after a display change. The launcher provides
previous/next buttons beside its dropdown, an aspect-group selector, editable
custom dimensions, and a live current-ratio label. Selecting a group only changes
the draft; the existing Save/Cancel and in-game confirmation rules still apply.
These settings change the native
3D rendering resolution; the window still follows its existing size. Use
**Alt+Enter** for fullscreen. Render resolution does not change desktop
resolution or refresh.
Menu backgrounds fill the selected canvas, while text, buttons and panels fit
proportionally. Narrower layouts fit widgets vertically instead of cropping them.
Windows frontend menus reconstruct magnified background, logo and icon textures
with a bounded cubic filter. Main-menu row lettering also reconstructs glyph
coverage for crisper edges. These use the original artwork; they do not add
new high-resolution assets. Native-size/minified textures and gameplay keep
their existing sampling. For comparison or troubleshooting, launch with
`TSFP_MENU_FILTER=linear` to restore the previous menu filtering for that run.

In single-player widescreen, ammo counters, corner radar, status indicators and
scope zoom controls move toward their screen edges with consistent proportions.
World target markers and the central aiming graphics retain their own placement.
Split-screen retains its existing layout. Gameplay uses the selected aspect ratio
and an adjusted horizontal camera view. Cutscene bars adapt to the selected
aspect ratio: wide screens use thinner horizontal bars. Screens wider than
the cinematic picture fill the width by cropping vertically, preserving
proportions instead of adding side bars. The camera stays within the authored
horizontal aperture so cutscene backdrops still cover the view. The camera
and scripted bar animations adapt together. The recorded attract reel follows
the same proportional width-fill rule, excluding its encoded side padding
on wide screens. Startup logos also fill the width proportionally. Wider
displays crop the top and bottom of the original movie or logo picture.
Presentation changes do not alter animation speed, audio timing, or the
recorded videos' original frame rate.

Settings live in `config/pc-display.ini` beside the keyboard/mouse configuration
(`Settings/pc-display.ini` in packaged builds). `TSFP_DISPLAY_CONFIG` can override
the path for isolated tests. Delete that small file to restore the original
rendering resolution and FOV. Render targets are limited to 7680×4320 and
33,177,600 pixels. Higher choices need sufficient GPU resources; an unsuccessful
in-game allocation leaves the previous setting active.

## PC controls

Press **F2**, or choose **PC Settings → Keyboard & Mouse** from the game
window's menu bar, to open the separate settings window. F2 also works in
fullscreen. Pause a mission before editing: the settings window blocks gameplay
input but does not itself pause the simulation.

Double-click a binding, or select an action and a binding slot and choose
**Change binding**. Press a key, or click a mouse button / scroll inside the
capture box. Each action supports four bindings. A duplicate-key prompt lets
you move a binding from another action. **Clear binding** removes one slot;
**Restore defaults** stages the default PC layout. Mouse sensitivity, inversion,
and mouse input can also be changed here. **Save & Close** saves and applies the
settings; **Cancel** discards edits. The previous configuration is backed up to
`config/pc-controls.json.bak`. F1, F2, F9–F12 and the Windows keys are reserved.

Default bindings:

| Action | Control |
| --- | --- |
| Move / strafe | W A S D |
| Look | Mouse |
| Fire | Left mouse button |
| Aim | Right mouse button |
| Use / confirm | E, Space, or Enter |
| Reload | R |
| Crouch | Ctrl or C |
| Melee / menu back | F, middle mouse, mouse button 4, or Backspace |
| Throw grenade | G or mouse button 5 |
| Change weapon | Mouse wheel or left/right arrows |
| Weapon mode / scope zoom in | Q or Up / Page Up |
| Change grenade / scope zoom out | V or Down / Page Down |
| Pause and release cursor | Escape |
| Show / hide PC Settings menu bar | F1 |
| Keyboard & mouse settings | F2 |
| Debug settings | F12 |
| Fullscreen | Alt+Enter |

**PC Settings > Fullscreen** also toggles fullscreen; its checkmark reflects the
current window mode.

Press **F1** to show or hide the menu bar, including in fullscreen. You can also
choose **PC Settings > Hide Menu Bar** and press F1 to bring it back. The menu bar
starts visible each launch; F2, F12, and Alt+Enter still work while it is hidden.
Escape releases the mouse; click the game to recapture it.

Click the game window to recapture the mouse. The recapture click is consumed
so it does not fire a weapon. Alt-Tab releases the cursor and clears held
keyboard/mouse inputs. Aim and crouch activation follow the game's profile
settings. Space is an action key; the original game has no jump action.

The settings menu saves to `config/pc-controls.json`. Sensitivity is degrees per
raw mouse count (default `0.10`); lower it for
slower aiming. Mouse motion changes camera angles directly, scales down while
zoomed, and does not pass through the controller's stick deadzone or turn-speed
curve. WASD diagonals are normalized. The supplied configuration uses keyboard
input on player 1; it does not require a controller.

Controller sensitivity uses the original in-game slider, with 0.05 steps
between 0 and 1. Older builds could jump straight to an endpoint (issue #24).
The shared float-comparison fix in `837692d` restores intermediate values;
rebuild with `tsfp.ps1 -Action Build` to refresh older generated code and the
executable. This setting is separate from raw mouse sensitivity in F2.

For the original game's **Settings → Controls** page (for example, Inverse
look), **Accept** now writes the selected profile before returning to Settings.
You can then quit directly from Settings; returning to the main menu is no
longer required. **Cancel** discards the draft. If saving fails, follow the
game's save-error screen before quitting. This is
separate from the PC keyboard/mouse window's **Save & Close** and Display
settings' explicit save/confirmation controls.

In-game button prompts automatically show the **first assigned keyboard or mouse
binding** for each control, for example **[E] Select** and **[F] Back**. Saving
bindings in F2 updates the prompts immediately. Unassigned controls show
**[Unbound]**. Mouse-look prompts show **[Mouse]** while raw mouse look is enabled.

Press a controller button, move a stick past its deadzone, or squeeze a trigger
to restore the original controller icons. Keyboard presses, mouse clicks, wheel
input, or mouse movement switch back to keyboard/mouse prompts. An unchanged held
button, button releases, and stick drift do not switch the prompts back and forth.
The first connected controller not assigned to another local player can control
player 1 alongside the keyboard profile; plugging it in does not require a restart.
This applies to the game's formatted text prompts, including menu and HUD hints;
controller diagrams and labels baked into images retain their original artwork.

The Xbox action mapping was checked against the publisher's original
[Xbox manual, Complete Controls](https://www.gamesdatabase.org/Media/SYSTEM/Microsoft_Xbox//Manual/formated/Timesplitters_-_Future_Perfect_-_2005_-_Electronic_Arts.pdf).

### Debug settings

For the temporary five-second-stall investigation, **Play-Quiet.cmd** in the
source checkout launches
with diagnostic output discarded and Debug log polling/performance logging
disabled. FPS measurement and your saved debug options remain intact. The next
normal **Play.cmd** launch restores reporting. For terminal/System Link launches,
set `$env:TSFP_DEBUG_QUIET = '1'` before your usual launch command; afterward use
`Remove-Item Env:TSFP_DEBUG_QUIET` to restore normal launches. Restart both game
instances when comparing modes. Quiet mode tests reporting I/O; internal report
formatting still runs, and diagnostic logs/counters are unavailable for that run.

Press **F12** or choose **PC Settings > Debug**.
The shortcuts also work in fullscreen. Pause a mission first: settings block gameplay input but do not
pause the simulation. The Debug window offers:

- **Show FPS:** game frame rate, capped at 60. F9 toggles the FPS display for
  the current session. These are host submissions, not monitor scanout readings.
- **Show average frame time:** the average interval between game frames in
  milliseconds, measured over roughly half a second; this is not GPU execution time.
- **Show error / warning counters:** on-screen E/W counts of matching runtime log
  lines. The window lists the latest 12 matches and counts frames over 50 ms,
  including loading delays.
- **Write performance samples to log:** FPS and average frame interval, about
  twice per second. This is off by default to avoid growing logs unnecessarily.
- **Show network latency:** gameplay RTT in milliseconds, updated about once a
  second. Online guests measure their host; hosts and LAN players show the highest
  fresh peer RTT. Offline and waiting states are labelled.
- **Open logs folder** and **Clear counters / recent:** inspect the original
  logs or reset the session counters without deleting log files.

**Save & Close** persists the display/logging options under `debug` in the same
controls JSON file and applies them immediately. **Cancel** discards checkbox
edits; clearing counters takes effect immediately. All options default to off.
Existing controls files without a `debug` section remain supported.

A lower-right watermark always shows the PC version and a short multiplayer
build identifier, including when Debug options are off. `VERSION` is the shared
version source for builds and the default release package name.

Diagnostics match logged failures, unresolved calls, warnings, and unsupported
features; they do not detect every gameplay or visual anomaly. Counts cover the
current launch since startup or the last clear. Use `Play.cmd` or `tsfp.ps1 Run`
for live log monitoring; direct launches without `RECOMP_LOG_FILE` show
"Log unavailable". Log reading is bounded and runs outside the render thread.
The runtime's diagnostic frame capture uses **Ctrl+F11**; plain **F11** does nothing.


### Keyboard and Mouse menu

Main Settings and Pause Options now include Keyboard and Mouse, using the game's
controller-style menu theme. Each action has Primary and Alternate bindings,
with at most two keyboard/mouse bindings per action. All 34 actions are covered,
including ten inventory slots (1–9 and 0 by default). The internal 70 weapon IDs
are not additional settings slots. Bindings and mouse sensitivity update live.
Capture supports keyboard, mouse buttons and wheel, waits for opening input to
release and supports Escape cancellation. Conflicts can move the existing binding;
clear and defaults are available. Existing four-binding keyboard profiles use
the first two positions; controller mappings retain their capacity.

### Save drives

Create Profile → Select Storage Device lists mounted drives in the existing
screen, sorted by drive letter on Windows and volume name on Mac. Rows show just drive names and letters.
Highlight a drive
to see its free and total space. Choosing another writable drive immediately
continues saving the new profile there; no restart is needed. Existing profiles stay on
their original drive. New locations use TimeSplittersFuturePerfect/Saves on that
drive. The selected path is stored in save-location.txt beside controls.json.
If that drive is unavailable on startup, reconnect it or remove that preference
to restore the default save location. Automated Test Lab sessions lock location
changes. Explicit manual testing uses --allow-save-drive-changes.
