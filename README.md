# TimeSplitters: Future Perfect PC port workspace

Target: the original Xbox US release, title ID `45410066`, XDK 5849.
This workspace uses static recompilation: translate the original x86 executable
into C and link it to a native Windows runtime. This does not recover the original
C/C++ source. A build is an experimental port until gameplay has been verified.

## Download and play

Download the Windows x64 ZIP from [Releases](https://github.com/TheDoffman/timesplitters-future-perfect-pc/releases).
Extract the entire ZIP to a writable folder, then double-click **Play.cmd**.
On first launch, select your US Xbox ISO or an extracted game folder containing
`default.xbe`, `pak`, and `xmv`. Setup verifies the game version and copies the
files once. Afterward, **Play.cmd** launches directly; the ISO is no longer needed.

The release includes the PC executable, movie libraries, and default controls.
It requires Windows 10/11 x64 and about 5 GB free for setup. Python, Visual Studio,
and an emulator are not required to play. Original game data is not bundled.
This is an experimental prerelease; a complete campaign playthrough is unverified.
Keep the release's `Data`, `Settings`, and `Saves` folders when upgrading.

To produce the same public package after building:

```powershell
.\.venv\Scripts\python.exe tools/package-release.py --version v0.1.0-alpha.1
```

This uses an explicit file list and default settings. It excludes all game data,
personal settings, and saves, and writes the ZIP plus SHA-256 under `build/releases`.

## Local workflow

This repository contains the PC input code, runtime patches, and build scripts.
Game data, translated game code, binaries, personal settings, and saves stay local.
Keyboard/mouse bindings use a separate F2 settings window. Display settings
are available inside the game's own Settings and paused Options menus.
See [STATUS.md](STATUS.md) for tested behavior and current limits.

Run in PowerShell from this directory:

```powershell
.\tsfp.ps1 Status
.\tsfp.ps1 Test
.\tsfp.ps1 Generate
.\tsfp.ps1 Build
.\tsfp.ps1 SmokeTest -Seconds 30
.\tsfp.ps1 Run
```

`Run` starts `build/windows/Release/futureperfect_recomp.exe` with
`RECOMP_GAME_DIR` pointing to `private/game`. A smoke test captures output in
`logs` and ends the process after its time limit. Its report records crashes and
unresolved calls; a report command succeeding does not mean the game is playable.
Smoke tests use separate save folders in `private/test-saves` and capture frames
in `private/captures`. Interactive runs store saves in `private/saves`.

Double-click `Play.cmd` for the keyboard-and-mouse setup. Restart the game after
updating a build or editing the configuration file by hand. Changes saved from
the game's PC settings menu apply immediately.
`Play.cmd` closes its command window after starting a hidden launcher; game logs
continue to be written to `logs`, and launch failures appear in a dialog. The
portable release uses the same behavior after first-time game import. For terminal
diagnostics, use `tsfp.ps1 Run` (or `Launch.ps1` in a portable release) directly.

## In-game display settings

Available in the current local/source build.

Open **Settings → Display settings** from the main menu, or **Options → Display
settings** while paused. Select the resolution row to cycle forward, or use
**Previous resolution** to cycle backward. Choose **Apply resolution**, then
**Keep resolution** within 15 seconds. Back, Revert, or letting the countdown
expire restores the previous mode. Only confirmed settings are saved.

The picker includes 1920×1080, 2560×1440, 3840×2160, and modes reported by the
monitor nearest the game window, including ultrawide sizes. The first candidate
is that monitor's current desktop resolution. These settings change the native
3D rendering resolution; the window still follows its existing size. Use
**Alt+Enter** for fullscreen. Desktop resolution and refresh rate are unchanged.
Game menus retain their original proportions with bars; gameplay uses the
selected aspect ratio and a wider horizontal camera view.

Settings live in `config/pc-display.ini` beside the keyboard/mouse configuration
(`Settings/pc-display.ini` in packaged builds). `TSFP_DISPLAY_CONFIG` can override
the path for isolated tests. Delete that small file to restore the original
rendering resolution. Extremely wide modes are bounded at 7680×2160; modes above
2160 pixels high are outside this release's resolution range.

## Texture preview

Press **F10** during play to switch between **Classic** and **4K** textures without
restarting or reloading the level. **PC Settings > 4K Textures** shows the current
mode with a checkmark and also switches it. Held-key repeats are ignored; the key
works with the menu bar hidden and in fullscreen. The mode applies to texture
artwork independently of display resolution and lasts for the current session.

**Play.cmd** starts in Classic mode and makes the installed local pack available
to F10. **Play-Textures.cmd** starts in 4K mode with the same live toggle.
Standalone packages look for an optional pack in `Textures` beside `Play.cmd`.
For direct executable launches, set `TSFP_TEXTURE_PACK_DIR` to the pack directory;
`TSFP_TEXTURE_MODE=classic` or `4k` selects the initial mode. An explicit pack path
defaults to 4K if no mode is set. Without a pack, both modes use original artwork.

The current 4K option is a partial texture preview. The current pack contains
11 replacements: selected terrain, masonry, timber, concrete, flooring, metal
panels and a weapon atlas. Most of the game still uses original art. The pack is
about 44 MiB; its compiler enforces the requested **6 GiB** cap, including metadata.
Artwork and original game assets remain local under `private/texture-remaster`.

Two installed DDS files are 4096×4096, eight are 2048×2048 and one is 2048×1024,
all with complete mip chains. Square generated sources are 1254×1254; the stone
wall source is 1774×887. Compilation resamples these images; this is not native
4K generated detail. Character artwork that shifted UV details remains a draft
outside the active pack. Full level, tiling and material-consistency review remains.
The loader currently supports DXT1/DXT3/DXT5 replacements and limits resident
replacement textures to 512 MiB of GPU memory. Missing or invalid replacements
fall back to original textures. Both views stay cached within the existing
512 MiB replacement-memory limit, making repeated toggles allocation-free.
Restart after changing pack files on disk; switching the mode itself is live.

Run `.venv/Scripts/python.exe tools/texture-worklist.py` to audit installed DDS
hashes, dimensions and mip chains and refresh the local coverage/size plan.
The worklist tracks 5,547 unique supported keys and 753 unsupported payloads;
installed, unfinished artwork and unapproved drafts are counted separately.
Its roughly 2.5 GiB projection covers supported DDS payloads only, not source
art or undecoded formats, and does not mean those replacements have been made.
Exact new generation prompts are saved beside their artwork under `authored/`.

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
| Switch Classic / 4K textures | F10 |
| Debug settings | F12 |
| Fullscreen | Alt+Enter |

**PC Settings > Fullscreen** also toggles fullscreen; its checkmark reflects the
current window mode.

Press **F1** to show or hide the menu bar, including in fullscreen. You can also
choose **PC Settings > Hide Menu Bar** and press F1 to bring it back. The menu bar
starts visible each launch; F2, F10, F12, and Alt+Enter still work while it is hidden.
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

Press **F12** or choose **PC Settings > Debug**.
The shortcuts also work in fullscreen. Pause a mission first: settings block gameplay input but do not
pause the simulation. The Debug window offers:

- **Show FPS:** game frame rate; when interpolation adds presentations, the
  overlay shows game/shown FPS. F9 toggles the FPS display for the current session.
- **Show average frame time:** the average interval between game frames in
  milliseconds, measured over roughly half a second; this is not GPU execution time.
- **Show error / warning counters:** on-screen E/W counts of matching runtime log
  lines. The window lists the latest 12 matches and counts frames over 50 ms,
  including loading delays.
- **Write performance samples to log:** FPS and average frame interval, about
  twice per second. This is off by default to avoid growing logs unnecessarily.
- **Open logs folder** and **Clear counters / recent:** inspect the original
  logs or reset the session counters without deleting log files.

**Save & Close** persists the display/logging options under `debug` in the same
controls JSON file and applies them immediately. **Cancel** discards checkbox
edits; clearing counters takes effect immediately. All options default to off.
Existing controls files without a `debug` section remain supported.

Diagnostics match logged failures, unresolved calls, warnings, and unsupported
features; they do not detect every gameplay or visual anomaly. Counts cover the
current launch since startup or the last clear. Use `Play.cmd` or `tsfp.ps1 Run`
for live log monitoring; direct launches without `RECOMP_LOG_FILE` show
"Log unavailable". Log reading is bounded and runs outside the render thread.
The runtime's diagnostic frame capture uses **Ctrl+F11**; plain **F11** does nothing.

For a fresh checkout, install Git, Visual Studio 2022 with its C++ workload and
Windows SDK, and Python 3.12+, then run:

```powershell
git clone https://github.com/TheDoffman/timesplitters-future-perfect-pc.git
cd timesplitters-future-perfect-pc
.\setup.ps1 -Python 'C:\path\to\python.exe' -Iso 'C:\path\to\TimeSplitters - Future Perfect (USA).xiso.iso'
```

If you already have this checkout, run only the `setup.ps1` command. The script
validates the input paths and Python version, then runs Setup, Extract, Test,
Generate, and Build in order, stopping at the first failure. It requires internet
access for dependency downloads; it does not install Git, Python, or Visual Studio.
The default output is `build/windows/Release/futureperfect_recomp.exe`.
After setup succeeds, double-click `Play.cmd` to play from the repository.
The extracted files in `private/game` must remain; the ISO is no longer needed.
Use `-Configuration Debug` or `-Configuration RelWithDebInfo` for another build
configuration, and pass the same configuration to `tsfp.ps1 Run` to launch it.

The equivalent individual steps are still available for troubleshooting:

```powershell
.\tsfp.ps1 Setup -Python 'C:\path\to\python.exe'
.\tsfp.ps1 Extract -Iso 'C:\path\to\TimeSplitters - Future Perfect (USA).xiso.iso'
.\tsfp.ps1 Test
.\tsfp.ps1 Generate
.\tsfp.ps1 Build
```

Setup installs dependencies into `.venv`, clones the pinned toolkit, initializes
SDL and XbSymbolDatabase, builds the symbol matcher, and fetches FFmpeg 8.1 for
intro movie playback. The FFmpeg download has a pinned SHA-256; if its mutable
upstream URL changes, setup stops for review. All tool downloads stay local.
The launcher copies `config/pc-controls.default.json` to the ignored personal
`config/pc-controls.json` on first use. Existing personal bindings are preserved.

## Inputs and generated files

The supported `default.xbe` is 6,270,976 bytes with SHA-256:

```text
3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc
```

- `private/game/`: extracted game files.
- `private/pipeline/`: disassembly, function classification and translation reports.
- `external/ts3FPrecomp/titles/futureperfect/src/recomp/gen/`: generated C.
- `build/windows/`: MSVC build and executable.
- `logs/`: validation, build and bounded startup logs.

The original ISO is read without modification. Original assets, executable,
generated game code, tool checkouts and binaries are excluded from the outer Git
repository. Supply your own game dump when reproducing this setup.

## Upstream and licensing

The Windows runtime and Future Perfect target come from
[nemonicicon/ts3FPrecomp](https://github.com/nemonicicon/ts3FPrecomp), pinned to
`d90c92988a9f99d0580d2ca3f7efd6ec3a7b7845`. Its checkout includes its GPL-3.0,
MIT, third-party license and attribution files; retain them when distributing
any relevant runtime code. No game data is supplied by this repository.

`patches/save-routing.patch` fixes the runtime's device paths for Xbox profile
storage: Partition1's TDATA and UDATA now use the configured save directory.
Setup and Build apply it idempotently. Build also runs the native
`tests/save_paths.c` regression test against the actual runtime.
After all native build tests pass, Build recycles their temporary `build/path-tests`
data. Failed tests retain that directory for diagnosis; `private/saves` is unaffected.

`patches/pc-controls.patch` connects the window/input layer to `src/pc_controls.c`
and `src/pc_input.c`, and adds a manual wrapper around the US build's
`sub_000D2B80`. Its generated original body is retained as
`sub_000D2B80_gen`; the generated C is never hand-edited. Build refreshes the
generated code automatically when newly added wrappers are missing. The native
`tests/pc_controls.c` suite runs during Build.

`patches/pc-display.patch` adds the display menu and camera hooks separately
from the original controls patch, so existing checkouts can upgrade after a merge.
Setup, Test, Generate, and Build validate the complete patch stack in a temporary
source-only directory before changing runtime files. Conflicts name the failing
patch and leave the runtime checkout untouched. Existing patches should remain
stable; add subsequent changes as separate patches. Run the upgrade regression
tests with `.venv\Scripts\python.exe -m pytest tests/test_patch_updates.py -q`.

`patches/pc-input-vocabulary.patch` keeps the Python input UI's key names in sync
with the mouse-button and F23/F24 names added to the runtime by the PC controls
patch. Setup, Test, and Build apply local patches idempotently.

`patches/pc-debug.patch` connects the renderer's frame statistics and F9 shortcut
to `src/pc_debug.c`. Build runs `tests/pc_debug.c` to check persistence, log
monitoring, live display options, and Debug dialog Save/Cancel behavior.
`tests/pc_debug_game.py` checks the real game's menu and fullscreen access using
isolated settings/saves and captures the dialog and overlay for review.
`patches/pc-window-shortcuts.patch` moves the runtime's frame capture to Ctrl+F11
so plain F11 does not trigger a capture.

`patches/pc-prompts.patch` connects input-device detection and shared player-1
controller input to `src/pc_prompts.c`. It wraps the supported US executable's
text measurement (`0x0003FCF0`) and rendering (`0x00040FA0`) functions using
`src/pc_prompt_guest.inc`, translating prompt codes before text layout. The
original strings are preserved for controller input. Build runs
`tests/pc_prompts.c`, including a virtual SDL controller test and guest-stack
checks. The virtual-controller check skips when a physical controller is attached.
Build automatically refreshes an older generated checkout when these wrappers are missing.

`patches/pc-fullscreen.patch` disables DXGI's automatic exclusive fullscreen
switch so the game's borderless fullscreen handler owns Alt+Enter and the
menu command. This prevents failed frame presentation and frozen video when
entering fullscreen. New Direct3D presentation errors are logged once per change.
`tests/pc_fullscreen_game.py` checks actual screen animation before and after both
fullscreen controls, using isolated settings and saves. Run it with
`.venv\Scripts\python.exe tests\pc_fullscreen_game.py` on an unlocked Windows
desktop, leaving the game in front until it exits. Screen captures and logs are
saved under `private/captures/fullscreen`; temporary test saves are removed.

The camera-field research also consulted the open-source
[MouseInjector TS3 driver](https://github.com/regilopes/MouseInjectorDolphinDuck/blob/master/games/ts3.c).
All hook addresses used here were checked against the supplied Xbox executable.

An independent [tsfp-recomp project](https://github.com/NeilSeligmann/tsfp-recomp)
supports the same executable hash, but its documented tested host is Linux.
Its status claims do not establish the behavior of our Windows build.

See `STATUS.md` for results measured in this workspace.
