# Linux x64 development

Linux support is being brought up using SDL3 and the same Vulkan renderer,
Xbox graphics frontend, shaders and runtime patch stack used by the native Mac
port. Shared gameplay, input, display/settings models and texture policy stay
in their existing source directories. Do not create a Linux copy of game logic.

## Current milestone

The Linux game target now compiles the same generated title, SDL input/audio,
shared settings models and Xbox/Vulkan frontend as Windows/Mac. Its Linux x64
fault handler and host adapters are under active validation. Compilation,
GPU contracts, boot, gameplay and full feature parity remain separate acceptance
steps; this is not a complete Linux release. Linux execution must be measured
on Linux; a Windows test cannot establish it.

The shared renderer/replay contracts now pass locally on Ubuntu/WSL with Mesa
software Vulkan. See [measured validation](../project/STATUS.md#linux-native-validation-after-restart-2026-11-01)
for source revisions, drivers and remaining game-integration limits.

The Linux SDK pin is `config/build/vulkan-sdk-linux.json`, at the same version as
the Mac SDK. Its publisher SHA-256/size is checked before extraction, installed
files are receipted, and licenses remain beside it under
`external/vulkan-sdk/linux`. The host's installed Vulkan ICD provides the GPU;
Linux does not use MoltenVK, Cocoa, the macOS SDK or a copied Windows environment.

## Windows development host

Use WSL 2 with Ubuntu 24.04 LTS. Enabling Virtual Machine Platform can require
a Windows restart. Install/register Ubuntu and verify `wsl --list --verbose`
reports version 2 before installing Linux build dependencies. Keep this check
separate from the game build; no script should restart Windows automatically.

Use a Linux virtual environment (`.venv-linux` when accessing a Windows checkout),
never the Windows `.venv`. Put the Linux working checkout, SDK and build tree
inside Ubuntu's native filesystem; DrvFS `/mnt/c` has substantial per-file
overhead during SDK/source verification. A local Git clone of the Windows
checkout can use its unpushed commits without copying ignored game data. Keep
the Windows editing checkout and Linux working checkout on the same committed
source revision; fetch new commits from the Windows checkout before building.
Preserve any Linux edits before switching revisions. Never apply patches/build concurrently on two
hosts sharing the same ignored runtime tree. No second game-data extract is needed
for these asset-free tests. Keep `build/windows`, `build/macos` and `build/linux`
separate, and preserve personal settings/saves.

On Ubuntu, install the development prerequisites. SDL's upstream
[Linux dependency guide](https://wiki.libsdl.org/SDL3/README-linux) includes the
XTEST headers required by the pinned SDL configuration:

```sh
sudo apt-get update
sudo apt-get install -y build-essential clang pkg-config git python3-venv \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxfixes-dev \
  libxss-dev libxtst-dev libwayland-dev libxkbcommon-dev libxkbcommon-x11-dev \
  libdrm-dev libgbm-dev libegl1-mesa-dev libdecor-0-dev \
  libasound2-dev libpulse-dev libudev-dev libvulkan1 mesa-vulkan-drivers \
  vulkan-tools xvfb xauth libssl-dev libfreetype-dev libfontconfig-dev python3-tk
python3 -m venv .venv-linux
.venv-linux/bin/python -m pip install -r requirements.txt
```

Build and execute the production contracts (no original game files required):

```sh
CC=clang CXX=clang++ .venv-linux/bin/python tools/testing/test-rhi-linux.py \
  --prepare-runtime --setup-sdk
```

`--prepare-runtime` reconstructs the pinned source snapshot and fetches only
the pinned SDL/Vulkan submodules. `--setup-sdk` is the explicit download boundary;
later runs verify the existing SDK and do not silently download or fall back to GL.
Use `--build-only` when no display/GPU is available. This does not mark GPU tests
passed. Logs/provenance are `logs/rhi-linux.json`, `logs/rhi-linux.log` and
`logs/replay-linux.log`. Probe configs/caches remain under `build/linux/rhi-tests`.
Ambient driver overrides are removed; `--driver-manifest /absolute/path/to/icd.json`
selects a particular driver explicitly and records its manifest hash.

On a VM/CI host without suitable Vulkan acceleration, use Mesa lavapipe under
`xvfb-run -a` and pass its installed ICD manifest explicitly. Discover that path
with `find /usr/share/vulkan/icd.d -name 'lvp_icd*.json'`; do not assume the filename
is identical across distributions. A software-driver pass verifies correctness,
not hardware acceleration or game performance. WSLg desktop/display variables
are retained; user save/settings paths and shader caches are isolated.

Run the same host-memory contracts on Linux:

```sh
.venv-linux/bin/python -m pytest tests/repository/test_linux_foundation.py \
  tests/compiler/test_linux_host.py tests/compiler/test_host_pages.py \
  tests/compiler/test_host_memory_stats.py \
  tests/compiler/test_native_vm_host.py tests/compiler/test_shared_views.py -q
```

## Direct WSLg windows and mouse capture

On this Windows host, WSL 2.7.14/WSLg 1.0.73.2 failed to paint direct game windows:
Weston logged `rdp_allocate_shared_memory` with `Input/output error` and
`use_gfxredir = 0`; the Windows window was transparent with a COPY MODE caption.
The fault was in WSLg's shared-memory transport, not the shared game renderer.
A restart alone did not reliably recover it. Updating the official WSL runtime
restored direct delivery with WSL 3.0.3, kernel 6.18.54.1, WSLg 1.0.79 and
MSRDC 1.2.7391. Windows was not restarted. Direct Wayland menus were visually
verified, and the user confirmed gameplay mouse capture and Esc release.

For direct play retain WSLg's desktop display variables. With SDL's Wayland
backend, point `XDG_RUNTIME_DIR` at the actual WSLg socket directory if the
user session's runtime directory has no `wayland-0`. Keep save/config/cache
paths isolated as documented below. Do not route the game to an Xvfb display
or a VNC viewer for mouse-capture acceptance. Esc releases capture, and focus
loss clears controls through the shared SDL adapter.

An invisible COPY MODE window should trigger inspection of
`/mnt/wslg/weston.log` and the installed `wsl --version` before changing renderer
code. Coordinate any WSL VM restart with running work; never restart Windows
automatically. Microsoft's [shared-memory failure report](https://github.com/microsoft/WSL/issues/41903)
describes the same transparent-window symptom. The local update result is
measured evidence, not a guarantee that every WSLg transport failure is fixed.

Direct window delivery does not establish GPU acceleration. The installed
Ubuntu Vulkan drivers still expose llvmpipe on this host: the RTX 4070 SUPER is
not used by that software Vulkan driver. Reviewed first-level play was roughly
15–20 FPS on the CPU. Keep Original 60 FPS when evaluating this baseline; beta
240 FPS is not appropriate evidence of hardware performance. Hardware-backed
WSL Vulkan requires separate driver and renderer-contract acceptance.

An isolated Mesa 26.2.5 Dozen probe detected the RTX 4070 SUPER as a hardware
Vulkan device, but advertised API 1.2.354 and no maintenance4 extension. The
production RHI contract rejected it before device creation because this shared
backend requires Vulkan 1.3 (including maintenance4 shader-interface rules).
Do not override the advertised API version or remove required feature checks
to make a driver appear compatible. System Mesa and the SDK pins remain unchanged.
The optional driver probe is local development evidence, not a supported game
configuration. Linux GPU performance acceptance still requires a compatible
hardware driver; software-rendered WSL timings cannot establish native Linux or
Steam Deck performance. See STATUS for the exact probe scope.

## Native game build and isolated tests

Use the existing supported US Xbox game directory, with `default.xbe`, `pak`
and `xmv`. It may be a Windows path mounted through `/mnt/c`; no duplicate
extract is needed. The scanner writes derived analysis JSON beside the XBE.
Original game assets and personal saves/settings stay local and unchanged.

```sh
CC=clang CXX=clang++ .venv-linux/bin/python tools/build/build-native-linux.py \
  --game-dir '/mnt/c/path/to/private/game' --setup --generate --test
```

Later incremental builds omit `--setup --generate`. Default compiler parallelism
is two workers to bound WSL memory use; `--jobs 1` through `--jobs 8` is explicit.
The ELF and source/binary/compatibility receipt are in `build/linux/game`.
The complete patch stack and current generation fingerprint are required.

```sh
SDL_VIDEODRIVER=x11 xvfb-run -a .venv-linux/bin/python \
  tools/testing/test-native-linux.py --game-dir '/mnt/c/path/to/private/game' \
  --seconds 120 --capture --driver-manifest /absolute/path/to/lvp_icd.json
```

The runner isolates input/display/runtime configs, XDG directories, crash reports
and saves in `private/test-saves/native-linux`. It uses original 60 FPS/60 Hz,
skips startup movies, bounds logs and keeps two rotating BMPs. `--test-keys`
uses the common synthetic SDL event file in that isolated directory, not physical
input. Only known disposable partition images are recycled. Presents/counters
require visual review before claiming menus or gameplay. Evidence goes in STATUS.
For routine gameplay, add `--unlocked-profile` with the existing local Test Lab
fixture under `private/test-fixtures/unlocked`. This prepares a disposable copy
of its five host save files; the canonical fixture remains unchanged.

Linux desktop settings/debug editing dialogs and movie decoding remain pending;
the manifest exposes these capabilities as unavailable. Shared in-game Video
Options, controls JSON and existing gameplay changes use the common models.
Portable packaging, audible output, networking and hardware acceptance remain
separate work. See [integration contracts](../agent-context/linux-game.md).

Then validate isolated boot/menus, movement/fire/pause, save/load, movies/audio,
fullscreen/resize/focus, networking, textures and supported refresh settings.
Do not use personal saves or overwrite installed controls for automated tests.

## Secondary Steam Deck validation

The Steam Deck is the planned secondary Linux x64 hardware test. First run the
asset-free probe in desktop mode using a separate development environment; do
not disable SteamOS filesystem protections or install into its system image.
After a runnable game package exists, test desktop and gaming mode, built-in
controls/Steam Input, suspend/resume, fullscreen/scaling and performance. WSL or
software-renderer results do not replace those hardware checks. No Deck has been
connected or modified by this initial work.
