# TimeSplitters: Future Perfect PC port workspace

Target: the original Xbox US release, title ID `45410066`, XDK 5849.
This workspace uses static recompilation: translate the original x86 executable
into C and link it to a native Windows or Apple Silicon Mac runtime. This does
not recover the original C/C++ source. A build is an experimental port until
gameplay has been verified.

## Download and play

Download the Windows x64 ZIP from [Releases](https://github.com/TheDoffman/timesplitters-future-perfect-pc/releases).
Extract the entire ZIP to a writable folder, then open **TimeSplitters Launcher.exe**
(**Play.cmd** opens the same launcher). Choose **Locate ISO** or **Use extracted files**
to select your US Xbox ISO or an extracted game folder containing
`default.xbe`, `pak`, and `xmv`. Setup verifies the game version and copies the
files once. Afterward, use **Play Game**; the ISO is no longer needed.

The release includes the PC executable, movie libraries, and default controls.
It requires Windows 10/11 x64 and about 5 GB free for setup. Python, Visual Studio,
and an emulator are not required to play. Original game data is not bundled.
This is an experimental prerelease; a complete campaign playthrough is unverified.
The launcher checks **published GitHub Releases**, never pushes to `main`.
**Stable** excludes prereleases; choose **Preview** for published alpha/beta builds.
If no stable release has been published, Stable reports that explicitly.
Before the main window opens, a brief splash checks for **launcher-only updates**
and automatically installs newer launcher files. The check has an eight-second
timeout; **Open installed launcher** skips the wait. This does not update the
game. The selected Stable/Preview channel applies to both launcher and game.
Click **Install release** or **Update release** to download the matching Windows ZIP.
The launcher verifies the archive's SHA-256 and the package's file manifest,
then closes briefly while a helper installs it and reopens the launcher.
The game must be closed during an update. Offline checks do not prevent playing
an already installed game. Users need no Git, Python or compiler.

Updates preserve `Data`, `Settings`, `Saves` and optional `Textures`.
**Restore previous** returns to the preceding installed release, keeping personal
settings. Interrupted installs recover at the next startup. Keep `.launcher`
for recovery/rollback; its `launcher.log` contains bounded launcher diagnostics.
Do not manually replace application files while the launcher or game is running.

To build only the standalone Windows launcher:

```powershell
.\tools\build-launcher.ps1 -Test
```

Output: `build/launcher/TimeSplitters-Launcher-windows-x64.zip`. Extract the whole
ZIP into a writable folder and open `TimeSplitters Launcher.exe`. Keep its
`Bin/TimeSplitters.App.exe` and `Support/Launcher/manifest.json` companions.
This bootstraps a fresh installation from published releases. Its first run starts
with Stable selected. The interface uses original vector artwork and a temporal
scanner theme; no game art is bundled. See [launcher maintenance](docs/windows-launcher.md).

Launcher publishing assets are generated separately in `build/launcher`:
`TimeSplitters-Launcher-v1.1.0-windows-x64.zip`, its `.sha256` sidecar, and
`launcher-update.json`. Publish these together on a GitHub Release to enable
automatic launcher updates without downloading a game runtime ZIP.

To produce the same public package after building:

```powershell
.\.venv\Scripts\python.exe tools/package-release.py --version v0.1.0-alpha.1
```

This uses an explicit file list and default settings. It excludes all game data,
personal settings, and saves, and writes the ZIP plus SHA-256 under `build/releases`.

## Sharing fixes between Windows and Mac

Both builds apply the complete `patches/` stack to the same pinned runtime and
use the same generated game code and binding parser. Put common behavior in
shared source, with OS-specific event handling and rendering in adapters; the
tracked `AGENTS.md` makes this the rule for every device.

The portable input model in `src/pc_input.c` is compiled by both builds. Its
movement normalization now runs on both platforms, so fixes to diagonal speed
use one implementation. The Mac SDL input regression checks the actual binding
path, while Windows retains its PC input integration tests. Native build
manifests include shared source hashes as well as Mac adapter hashes.

This does not make Windows-only features automatically available on Mac:
Debug settings, advanced graphics effects and release packaging still need Mac
adaptation. Native Mac now shares the camera input model and button-prompt
formatter, but the graphics backends remain separate. Record and test adapter
work before claiming a feature works on both platforms.

New Windows PC features can be completed on PC first. Before completing a PC
feature, add or update its entry in [the Mac port backlog](MAC_PORT_BACKLOG.md)
with its behavior, source references, remaining Mac adaptation and acceptance
checks. Mac implementation happens in a later requested porting task; queued Mac
work does not block PC completion. Shared changes must still preserve existing
Mac functionality. Agents follow `AGENTS.md` and the pull-request checklist to
maintain this list; there is no automatic feature detector or CI enforcement.
Record measured validation in `STATUS.md` and link it from the backlog.

A change must reach the other checkout through Git and be rebuilt there. Keep
this chat's changes on `mac-port`; do not automatically merge it into `main` or
replace another device's uncommitted work. See `STATUS.md` for measured coverage.

## Native Apple Silicon setup (experimental)

Native work stays on **mac-port**. The Mac now builds an ARM64 Mach-O executable
from the verified game data and the same pinned runtime and Python dependencies
used on Windows. It executes the translated Xbox entry point, initializes the
kernel and graphics device, loads assets, and displays the game's **1–4 player
selection menu in a native SDL/Cocoa OpenGL window**. This is an experimental
port: keyboard-driven menus now reach visible first-level gameplay. Basic
movement, mouse aiming, firing and pause/resume have been checked. Full-level
completion and rendering fidelity are not yet validated.
Double-click **Play-Native-Mac.command** to try the native build without a time
limit. It uses your normal `private/saves` and preserves existing controls and
PC display settings. Native runtime settings stay under `private/native-settings`.
Use `./Play-Native-Mac.command --check` for a read-only prerequisite check.

Keep one verified extraction in `private/game`, containing `default.xbe`, `pak`
and `xmv`. Transfer it once from the Windows checkout or extract your supported
US Xbox ISO with the pinned upstream `tools.xiso`; preserve any existing copy.
Git does not transfer these ignored game files.

From the project root, use Python 3.12 or newer, Xcode command-line tools, and
an installed OpenSSL development library. The tested Mac has Apple Clang 21,
OpenSSL 3.6.3, and pinned SDL 3.4.16 built from its upstream submodule. No system
SDL installation is used. The script does not upgrade system dependencies.

```sh
# First native setup and generation; reuses the existing private/game.
python3 tools/build-native-mac.py --setup --generate --test

# Subsequent source changes.
.venv/bin/python tools/build-native-mac.py --test

# Bounded native execution, with isolated saves and configuration.
.venv/bin/python tools/test-native-mac.py --seconds 120 --capture

# Frame timing without screenshot readback (navigate using isolated test input).
.venv/bin/python tools/test-native-mac.py --seconds 150 --test-keys --profile-frames
# Select a known gameplay window, excluding menus/loading and the opening animation.
.venv/bin/python tools/native_frame_stats.py logs/native-frame-times.csv --start 120 --end 145
```

Output stays in `build/macos`. `BUILD.json` records the source and runtime
revision, source/patch hashes, executable hash, compiler, Python packages and
linked libraries. Native changes to the upstream runtime are reproduced by
`patches/zz-mac-native-boot.patch`, `patches/zzz-mac-renderer.patch`,
`patches/zzzz-mac-input.patch`, `patches/zzzzz-mac-high-refresh.patch`,
`patches/zzzzzz-interpolation-layouts.patch` and
`patches/zzzzzzz-shared-input-model.patch`, applied
after the shared feature patches. Mac renderer source stays in `src/native`;
Windows and the inherited Linux graphics backends retain their own sources.
Never edit generated game C. Regenerate when wrapper declarations change.

The native probe requires an ARM64 binary and the supported XBE hash. It strips
stale runtime overrides, uses `private/test-saves/native-mac`, isolates both
upstream and PC settings, bounds duration/log size, and records
`logs/native-boot-summary.json`. A successful probe means guest frames continued
until its time limit. The default is 15 seconds; an explicit `--seconds` accepts
up to 1800 seconds for sustained testing, with the same 8 MiB log limit.
An intentional early stop is recorded as `process_exit`, not a passed timed probe.
It separately reports sampled host presents/GL errors;
menu recognition still requires reviewing the captures. `--capture` retains two
rotating BMPs under `private/captures/native-mac`; it does not capture the desktop.
Selected visual evidence includes `private/captures/native-mac/first-native-menu.png`
and `private/captures/native-mac/menu-navigation.png`. Test
partition images are sparse, then recycled with validated paths; profiles,
configuration and logs remain. Native CTest uses `build/macos/path-tests` and
recycles its partition images after the pass as well.

`--profile-frames` records CPU wall-clock intervals between completed host
presents and time spent in swap, capped at 20,000 rows (under 2 MiB). It adds
no GPU readback. The CSV and whole-run statistics are diagnostic data; loading
screens and menus must be excluded before describing gameplay FPS. These are
host-present timings, not measurements of monitor scanout. Compare the same
scene and build settings, and state whether `--capture` was enabled.

Native Mac defaults to an **up-to-120 FPS** display target: the game still
simulates at 60 Hz, while the renderer draws intermediate frames using blended
camera/object transforms. GPU vertex shaders and optimized native game code
provide the extra rendering time. Menus, cinematics, scene cuts and expensive
frames can fall back to fewer display frames. This is experimental interpolation,
not 120 simulation updates, and is not a guarantee for every level or Mac.

Use `./Play-Native-Mac.command --fps 60` for the original presentation rate, or
`--fps 120` to explicitly select the new target for that launch. These options
do not rewrite saved settings. To retain 60 as a preference, put `frame_interp = 1`
in `private/native-settings/runtime.conf`; `frame_interp = 2` selects 120.
The Mac path clamps interpolation to 2x. A custom `frame_cap` is preserved;
use `frame_cap = 60` with interpolation, or the `--fps 120` override. Do not raise
`RECOMP_VBLANK_HZ`: that changes the console clock and can change game speed.

The isolated test runner also accepts `--fps 60` / `--fps 120`. Profile rows label
each present as `guest` or `interpolated`, and the statistics report both rates.
For motion evidence, `--capture-interpolation` enables a request file at
`private/test-saves/native-mac/capture-interpolation.txt`; write `1` while moving
to replace exactly two BMPs (`interpolation-guest.bmp` and
`interpolation-midpoint.bmp`) in the normal native capture directory. This GPU
readback is for visual validation; keep it outside measured performance windows.

The native implementation includes a reserved 4 GiB **virtual address range**
(not a 4 GiB game copy), aliased RAM/device mappings, host timers, an ARM64
register-access fault handler, sparse Xbox partition devices, and an SDL PCM
output backend. Unknown faulting instructions stop with diagnostics. SDL audio
submission has been observed, but output fidelity/latency and human hardware
playtesting have not been validated. Keyboard and mouse OS-event delivery has
been checked with automation. Audio position uses the existing wall-clock fallback.

The native renderer uses shared HLE state/texture/vertex hooks, GPU NV2A vertex
programs with the portable interpreter as fallback, and register combiners with
four 2D texture inputs. `--test` checks actual GPU pixels for topology, font
alpha/blending, distinct texture stages/coordinates, GPU/CPU vertex agreement,
scissors, screen copies, image surfaces, readback, offscreen state/orientation,
interpolation and worker-thread context handoff, alongside three platform suites.
These tests use no game data or saves. Advanced texture modes, cube/volume
resources, movies and full rendering fidelity remain incomplete. Complete
gameplay and save/load still need validation.

Native controls use the same JSON bindings as Windows. Focus the game window:

- Arrow keys navigate; **Enter**, **Space** or **E** selects; **Backspace** goes back.
- **Click inside the game** to capture the pointer, then move the mouse to look
  around. This first click is consumed; subsequent left clicks fire. Relative
  mouse counts now update camera angles directly using the shared PC model,
  including zoom sensitivity and pitch limits; they no longer emulate a stick.
- **Escape** pauses and releases the mouse. After resuming, click to capture
  again. **F1** also toggles relative mouse capture
  (use Fn+F1 if macOS reserves the function key for brightness).
- **WASD** moves; left mouse fires; right mouse activates the right-stick action;
  **R** reloads, **G** uses the left trigger, **Tab** maps to Xbox Back.
- The first unassigned SDL controller also shares player 1. Explicit controllers
  assigned to other players are reserved. Physical controller play is unverified;
  SDL virtual-controller mapping/hotplug has regression coverage.

Formatted menu/HUD hints use the current keyboard/mouse bindings and switch to
controller icons on deliberate controller activity. Text baked into artwork or
plain tutorial text is unchanged.

**Mac settings:** press **F2** (Fn+F2 if needed), or choose **Game Settings →
Keyboard, Mouse & Graphics…** in the Mac menu bar. The Keyboard & Mouse tab edits
up to four bindings per action, sensitivity and vertical inversion. Save applies
that tab immediately and backs up the controls file; Cancel discards unsaved edits.
The Graphics tab selects the render resolution and fullscreen. **F11** toggles
fullscreen directly. Resolutions persist in `config/pc-display.ini`; fullscreen
is a session setting. The original in-game Settings/Options menu also includes
**Display settings**, with a 15-second resolution preview and automatic rollback.

Higher resolutions render the scene at the selected size, preserve the original
menu proportions and widen the gameplay camera. Screen-space HUD draws retain
their proportions. Larger targets cost GPU time, so 120 presentations per second
remains a target, not a guarantee. Four 2D texture inputs, projected/linear
texture coordinates, scissor clipping and 32-bit screen copies are implemented. Advanced texture modes, cube/volume
textures and movies remain incomplete; this is not complete Windows graphics parity.

To use the same compiled DDS texture pack as Windows for one Mac launch:

```sh
./Play-Native-Mac.command --texture-pack private/texture-remaster/pack
```

Both adapters share content keys, full-mip BC1/BC2/BC3 validation, aspect checks
and a 512 MiB replacement residency limit. Missing, malformed or over-budget
replacements use original textures. This does not create artwork or increase the
6 GiB pack budget. Native texture replacement is verified with synthetic GPU
fixtures; the complete remaster pack has not been visually verified on Mac.

Mouse enabled/sensitivity/invert settings are read from the shared controls file
at startup and updated live when saved in Mac Settings. Mouse look uses direct
camera angles, independent of the controller turn-speed curve. With
`invert_y: false`, moving the mouse up looks up.
Losing focus releases keys, buttons and mouse capture and neutralizes
controller input. The bounded probe uses its own copy of controls under
`private/test-saves/native-mac`; edit only that copy for experiments. No personal
configuration is used by this test command. `--test` additionally checks native
key press/release, focus loss, mouse settings, click-to-capture without firing,
pointer release and SDL controller sharing.

For automated menu checks, add `--test-keys` and append SDL scancode names such as
`+Return` and `-Return` on separate lines to the isolated `test-keys.txt`, with a
short hold between writes. This optional path feeds SDL events through the same
binding reader; it does not establish physical keyboard/controller testing.
Native test input can append `+Return` / `-Return`, `motion 20 -10`, or
`button 1 1` / `button 1 0` to the isolated runner's `test-keys.txt` when
`--test-keys` is enabled. Use F1 or a capture click before motion. These inject
synthetic SDL events; they do not validate physical mouse or macOS event delivery.
`focus 1` / `focus 0` simulate focus changes within this test path without
changing normal interactive focus handling.

For a single bounded renderer trace, add `--trace-render` and write `1` to the
isolated `trace-render.txt`. The next frame logs up to 2,000 draw/clear samples;
normal play has tracing disabled. Interactive launch logs are capped at 4 MiB
per stream and reused on the next launch.

Portable patch, launcher and test-isolation checks:

```sh
.venv/bin/python -m pytest tests/test_patch_updates.py tests/test_native_mac_tools.py tests/test_native_mac_launcher.py -q
```

## Local workflow

The source build replaces Xbox Live with **Online** above System Link, with a
player-hosted HTTPS directory, compatible-only browser, private join codes and
a host-routed UDP bridge. **Online > Start New Match** detects your public IPv4,
starts a background directory and fills in its certificate automatically. Share
the copied invitation with friends. Router forwarding is still required.
LAN play is also available with `.\tsfp.ps1 Run -SystemLink`.
See [multiplayer setup and validation limits](docs/multiplayer.md).

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
