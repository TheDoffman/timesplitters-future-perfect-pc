# Native Apple Silicon guide

[Back to the README](../README.md)

The Mac build produces an ARM64 Mach-O executable
from the verified game data and the same pinned runtime and Python dependencies
used on Windows. It executes the translated Xbox entry point, initializes the
kernel and graphics device, loads assets, and displays the game's **1–4 player
selection menu in a native SDL/Cocoa window**. This is an experimental
port: keyboard-driven menus now reach visible first-level gameplay. Basic
movement, mouse aiming, firing and pause/resume have been checked. Full-level
completion and rendering fidelity are not yet validated.
Double-click **Play-Native-Mac.command** to try the native build without a time
limit. It uses your normal `private/saves` and preserves existing controls and
PC display settings. Native runtime settings stay under `private/native-settings`.
Use `./Play-Native-Mac.command --check` for a read-only prerequisite check.

Keep one verified extraction in `private/game`, containing `default.xbe`, `pak`
and `xmv`. Transfer it once from the Windows checkout or extract your supported
US Xbox ISO with the project's own Python importer; preserve any existing copy:

```sh
python3 tools/import-game.py --iso /path/to/game.iso --output private/game
```

Python 3.12+ is sufficient for extraction, before runtime setup. Use
`--verify-only` instead of `--output` to check the image without writing files.
See [replacement status and import limits](independent-runtime.md).
Git does not transfer these ignored game files.

From the project root, use Python 3.12 or newer, Xcode command-line tools, and
an installed OpenSSL development library. The tested Mac has Apple Clang 21,
OpenSSL 3.6.3, and pinned SDL 3.4.16 built from its upstream submodule. No system
SDL installation is used. The script does not upgrade system dependencies.
Use a focused branch based on current main (or an explicitly documented
unmerged dependency); the native builder does not require a branch named
`mac-port`. Host, game hash and upstream revision checks still apply.

`--setup` prepares the runtime from tracked `vendor/ts3FPrecomp` source using
the same offline importer as Windows. It no longer fetches the original runtime
repository; SDL, the symbol database, Python packages and the selected SDK keep
their existing direct download pins. See [bundled runtime source](building.md#bundled-runtime-source).

Normal builds use the shared PC Xbox frontend through Vulkan on macOS 26+.
`--renderer auto` selects this route based on the pinned SDK's host requirement;
older Macs retain legacy OpenGL. `--renderer gl` explicitly selects the fallback.
`--setup` installs the verified checkout-local SDK when Vulkan is selected.
Without `--setup`, a missing or modified SDK stops before patching/generation;
it does not silently change renderer or overwrite an unknown installation.
Existing binaries keep their renderer until rebuilt. Both launchers use the
resolved renderer in `BUILD.json` and preserve personal settings.

```sh
# First native setup and generation; reuses the existing private/game.
python3 tools/build-native-mac.py --setup --generate --test

# Subsequent source changes.
.venv/bin/python tools/build-native-mac.py --test

# Bounded native execution, with isolated saves and configuration.
.venv/bin/python tools/test-native-mac.py --seconds 120 --capture

# Reusable unlocked Test Lab copy, when its local fixture is available.
.venv/bin/python tools/test-native-mac.py --unlocked-profile --seconds 120 --capture

# Frame timing without screenshot readback (navigate using isolated test input).
.venv/bin/python tools/test-native-mac.py --seconds 150 --test-keys --profile-frames
# Select a known gameplay window, excluding menus/loading and the opening animation.
.venv/bin/python tools/native_frame_stats.py logs/native-frame-times.csv --start 120 --end 145
```

Output stays in `build/macos`. `BUILD.json` records the source and runtime
revision, source/patch hashes, executable hash, compiler, Python packages and
linked libraries, plus working-tree changes and the shared source/content
identity. Native identity files stay in `build/macos/generated`; they do not
enable native multiplayer. Native changes to the upstream runtime are reproduced by
`patches/0400-mac-native-boot.patch`, `patches/0630-mac-renderer.patch`,
`patches/0660-mac-input.patch`, `patches/0740-mac-high-refresh.patch`,
`patches/0750-interpolation-layouts.patch` and
`patches/0780-shared-input-model.patch`, applied
after the shared feature patches. Thin Mac adapters stay in `src/native`; common Xbox rendering uses the same
patched runtime source as Windows. Legacy GL remains a compatibility exception.
Never edit generated game C. Regenerate when wrapper declarations change.

The native probe requires an ARM64 binary and the supported XBE hash. It strips
stale runtime overrides, uses `private/test-saves/native-mac`, isolates both
upstream and PC settings, bounds duration/log size, and records
`logs/native-boot-summary.json`. A successful probe means guest frames continued
until its time limit. The default is 15 seconds; an explicit `--seconds` accepts
up to 1800 seconds for sustained testing, with the same 8 MiB log limit.
An intentional early stop is recorded as `process_exit`, not a passed timed probe.
`--unlocked-profile` uses the shared [Test Lab fixture](testing-profile.md) in
a disposable nested save directory and skips startup movies. The default probe
retains its existing profile. Neither mode uses personal saves or configurations.
It separately reports sampled host presents/GL errors;
menu recognition still requires reviewing the captures. `--capture` retains two
rotating BMPs under `private/captures/native-mac`; it does not capture the desktop.
Selected visual evidence includes `private/captures/native-mac/first-native-menu.png`
and `private/captures/native-mac/menu-navigation.png`. Test
partition images are sparse, then recycled with validated paths; profiles,
configuration and logs remain. Native CTest uses `build/macos/path-tests` and
recycles its partition images after the pass as well.

`--profile-frames` uses the same recorder in GL and the common Vulkan
renderer. It records CPU wall-clock intervals between successful presentation
API returns and time spent in the call, capped at 20,000 rows (under 2 MiB).
Guest, interpolated and other presents are separate; failed/skipped calls do not
add samples, and their gap remains in the next successful interval. It adds
no GPU readback. The CSV and whole-run statistics are diagnostic data; loading
screens and menus must be excluded before describing gameplay FPS. These are
host-present timings, not measurements of monitor scanout. Compare the same
scene and build settings, and state whether `--capture` was enabled.

Native Mac uses fixed **60 FPS** rendering and the original **60 Hz** console
clock. Interpolation and prediction have been removed. Old timing preferences
cannot enable them; the launcher migrates those keys on Save while preserving
unrelated settings. The monitor retains its desktop refresh rate.
The interactive and isolated runners accept `--fps 60` only. The retired
`--capture-interpolation` mode is removed; ordinary `--capture` remains.
For separate profiling, `--profile-frames --profile-without-validation` disables
Vulkan validation only for that isolated run. Report this distinction and exclude
loading/readback from steady-scene timings.

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
- In menus, move the pointer to highlight a row, left-click to select, right-click
  to go back, or scroll to change rows. Keyboard/controller navigation remains
  available. Held menu clicks are suppressed when gameplay resumes.
- **Click inside gameplay** to capture the pointer, then move the mouse to look
  around. This first click is consumed; subsequent left clicks fire. Relative
  mouse counts now update camera angles directly using the shared PC model,
  including zoom sensitivity and pitch limits; they no longer emulate a stick.
- **Escape** pauses and releases the mouse. Resuming a menu automatically restores
  mouse look while the game stays focused. Focus loss releases controls; click
  inside gameplay to capture again. **F1** also toggles relative mouse capture
  (use Fn+F1 if macOS reserves the function key for brightness).
- **WASD** moves; left mouse fires; right mouse activates the right-stick action;
  **R** reloads, **G** uses the left trigger, **Tab** maps to Xbox Back.
- The first unassigned SDL controller also shares player 1. Explicit controllers
  assigned to other players are reserved. Physical controller play is unverified;
  SDL virtual-controller mapping/hotplug has regression coverage.

**Quit to Desktop** is available from the main and Pause menus. Its confirmation
starts on Cancel; Back returns to the current menu. The original Quit still
returns to the frontend.

Formatted menu/HUD hints use the current keyboard/mouse bindings and switch to
controller icons on deliberate controller activity. Text baked into artwork or
plain tutorial text is unchanged.

**Mac settings:** press **F2** (Fn+F2 if needed), or choose **Game Settings →
Keyboard, Mouse & Graphics…** in the Mac menu bar. The Keyboard & Mouse tab edits
up to four bindings per action, sensitivity and vertical inversion. Use **Record**
next to a binding to press a key or click/scroll inside the capture box. Escape
cancels capture; **Use Escape** and **Unbind** are explicit alternatives. If another
action uses the binding, **Move binding** reassigns it in the editor; Cancel keeps
the old assignment. Punctuation and keypad keys use the same portable codes as PC.
Unsupported saved keys are preserved and labeled unavailable on Mac.

**Save this tab** applies the edits and backs up the controls file; Cancel discards
unsaved edits. Capture and conflict changes remain drafts until Save.
The Graphics tab filters resolutions by aspect ratio and sets vertical field of
view (40–100°, default 55°), render resolution and fullscreen. Defaults and
changes remain drafts until Save; Cancel leaves settings unchanged. A failed
display save restores both resolution and FOV. **F11** toggles fullscreen
directly. Resolution and FOV persist in `config/pc-display.ini`; fullscreen
is a session setting. The original in-game Settings/Options menu also includes
**Display settings**, with a 15-second resolution preview and automatic rollback.

The lower-right host overlay shows the shared game version and generated build
identity independently of the performance display. It stays out of guest
screen-copy textures and capture files.

**Mac debug tools:** **F9** toggles the FPS overlay for the current session.
**F12** or **Game Settings → Debug & Performance…** opens options for FPS,
average guest frame interval, error/warning counters and optional performance
logging. Use Fn with the function keys if macOS reserves them. **Save & Close**
persists these options under `debug` in the controls JSON; Cancel discards drafts.
Clear counters resets the diagnostic history without deleting logs.

Game FPS counts guest presentations; Shown includes successful interpolated host
presentations. These measure application presents, not monitor scanout or the
console clock. The overlay stays outside guest screen copies and frame captures.
Log counters match diagnostic text; they cannot detect every rendering problem.
Polling runs on the main thread, and optional samples are limited to twice per
second. **Open logs folder** opens the launcher's bounded `logs/native-play.err`
location (or `native-boot.err` for isolated tests).

Higher resolutions render the scene at the selected size, preserve the original
menu proportions and widen the gameplay camera. Screen-space HUD draws retain
their proportions. Larger targets cost GPU time and can cause drops below 60 FPS. Four 2D texture inputs, projected/linear
texture coordinates, scissor clipping and 32-bit screen copies are implemented. Advanced texture modes, cube/volume
textures and movies remain incomplete; this is not complete Windows graphics parity.

Native builds prepare the same compiled DDS texture pack as Windows by default,
and the source launcher finds it automatically. See the [texture dependency and
publication status](textures.md#texture-build-dependency). Use
`--without-textures` on the builder for an explicitly lightweight build.
To override the pack for one Mac launch:

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
.venv/bin/python -m pytest tests/test_patch_updates.py tests/test_patch_tracking.py tests/test_native_mac_tools.py tests/test_native_mac_launcher.py -q
```

## Common renderer development probe

The common PC/Mac renderer uses the existing Vulkan backend and PC HLSL through
DXC. This is the normal build route on macOS 26+; the launcher follows the build
manifest and also supports explicit legacy GL builds. No game data or save fixture is needed
for this probe. Use the checkout's pinned Python/CMake/Ninja environment:

```sh
.venv/bin/python tools/test-rhi-mac.py --setup-sdk
.venv/bin/python tools/test-rhi-mac.py --driver moltenvk
.venv/bin/python tools/test-rhi-mac.py --driver kosmickrisp
```

The current SDK requires Apple Silicon macOS 26+ for this validation route.
`config/vulkan-sdk.json` pins SDK 1.4.363.0, its URL, byte size and publisher
SHA-256. The setup needs 5 GiB free for download/extraction, installs inside
ignored `external/vulkan-sdk`, uses `copy_only=1` and makes no system-wide SDK
installation. It preserves licenses/version manifests and records hashes of the
installed libraries and compiler headers. Unknown or changed installations fail
verification instead of being overwritten. The installed core takes about
2.5 GiB on the tested device; the verified installer/archive are disposable.
See the [SDK installation documentation](https://vulkan.lunarg.com/doc/view/latest/mac/getting_started.html).

The runner validates the runtime pin, applies the complete patch stack, initializes
only pinned SDL/Vulkan header/loader/allocator submodules, and builds a small test
under `build/macos/rhi-tests`. It bounds the executable to 60 seconds, isolates
cache/config paths and writes `logs/rhi-<driver>.log` and `.json`. A missing
validation layer or reported validation error fails the check. The shared shader
pixel checks also run through Windows D3D11 WARP in CI; Mac additionally checks
swapchain channel order and presentation. These checks are not gameplay or full
rendering certification. See [measured scope](../STATUS.md#common-vulkan-backend-on-mac-2026-10-19).


### Common game renderer

<a id="opt-in-common-game-renderer"></a>

With verified local game data/generated sources and the pinned SDK installed:

```sh
.venv/bin/python tools/build-native-mac.py --renderer vulkan --test
.venv/bin/python tools/test-native-mac.py --unlocked-profile --seconds 120 --capture --test-keys --fps 60
```

The builder reuses `build/macos` and records the renderer in `BUILD.json`; there
is no second game build tree. New Vulkan builds record the pinned MoltenVK
driver in `BUILD.json`; build with `--vulkan-driver kosmickrisp` to select the
alternative. Both launchers and the isolated runner honor that recorded choice.
Older manifests without a driver field retain KosmicKrisp. The Python launcher
and runner accept `--vulkan-driver moltenvk|kosmickrisp` as a temporary override
without changing the manifest or saved settings. The runner enables validation.
This path compiles the PC Xbox frontend directly. The Mac adapters supply the
window, SDL events and CoreText overlay bitmap. It has bounded first-mission
coverage, not full rendering, physical-input certification.
`--profile-frames` uses the common presentation recorder on both renderers and
separates accepted guest/interpolated API presents. These timings do not measure
GPU completion or display scanout. GL-specific `--trace-render` remains
unavailable on the common renderer. Use `--capture` for bounded frame evidence.
Use the isolated runner for automated checks. For interactive use, both the
native app and `Play-Native-Mac.command` read the renderer from `BUILD.json`,
verify the pinned SDK receipt, and configure the manifest-selected loader without
changing saved graphics preferences. `--check` remains read-only and now catches
missing or modified Vulkan dependencies before launch. Rebuild with
`--renderer gl --test` to select the fallback explicitly. Khallos sky distortion,
movie playback, full campaign fidelity and sustained 60 FPS performance
remain separate acceptance gaps; selecting a shared implementation does not
certify them.
See [results and remaining work](../STATUS.md#common-xbox-renderer-on-native-mac-2026-10-19).

## Native release packaging

Build the game from the exact existing game release tag, retaining its native
`BUILD.json`. On Apple Silicon, build the packaging branch's launcher with
`tools/build-mac-launcher.py --require-artwork --test`, then run:

```sh
.venv/bin/python tools/package-native-mac.py --version v0.3.1-alpha.1
```

The packager verifies the native executable and pinned SDK, bundles and rewrites
all non-system Mach-O dependencies, ad-hoc signs the modified binaries, includes
checksum-pinned third-party notices and corresponding source, and verifies the
actual ZIP with production launcher installation code on fresh/existing profiles.
It emits a `macos-arm64.zip`, SHA-256 sidecar and `BUILD.json` beside the ZIP.
Existing output paths are never overwritten. No original game data, personal
settings or saves belong in the public package. A bounded relocated runtime
probe is required before uploading those three files to an existing release.
For alpha.1 this passed on an Apple M4 Mac mini with macOS 27.0.1; its three
Mac assets are published alongside the existing Windows assets.

The portable Vulkan package requires Apple Silicon. Its minimum macOS version
is derived from the actual Mach-O deployment targets and recorded in `BUILD.json`;
the alpha.1 build requires macOS 27.0 or newer. It includes its runtime dependencies; the player needs no compiler, Python,
Homebrew or Vulkan SDK. The app and binaries are ad-hoc signed, without Developer
ID/notarization. The source tag identifies the game; `packaging_commit`, launcher
receipt and source archive identify any later packaging-only changes. Packaging
does not change the release tag or publish assets automatically.

Build/sign generated `.app` output outside cloud-synced storage when its provider
recreates Finder metadata. `--output "$HOME/Library/Caches/tsfp-mac-release/launcher"`
and a corresponding packager `--output` keep signing and ZIP staging local.
