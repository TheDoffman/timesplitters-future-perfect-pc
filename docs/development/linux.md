# Linux x64 development

Linux support is being brought up using SDL3 and the same Vulkan renderer,
Xbox graphics frontend, shaders and runtime patch stack used by the native Mac
port. Shared gameplay, input, display/settings models and texture policy stay
in their existing source directories. Do not create a Linux copy of game logic.

## Current milestone

The initial target builds the existing asset-free common-renderer pixel/resource/
presentation contract and production frame-replay tool on Linux x64. Linux host
page queries and available-memory snapshots extend the existing platform adapters.
This is **not yet a Linux game build or a playable Linux release**. Compilation,
GPU contracts, boot, gameplay and full feature parity are separate acceptance steps.
Linux execution must be measured on Linux; a Windows test cannot establish it.

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
  vulkan-tools xvfb xauth
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

## Remaining game bring-up and secondary Steam Deck validation

Before advertising a Linux game executable, implement and test Linux x64 fault/
MMIO decoding and fatal-signal reporting, wire the shared SDL input/audio and
title entry into the Linux build, supply text/settings/Online host adapters, and
verify movie/dependency loading. Keep unsupported capabilities explicit and
preserve their saved values. Continue to use the supported US XBE hash, complete
patch stack, reproducible generation, original 60 FPS defaults and 60 Hz console
clock. Settings/UI support must consume the common definitions, not duplicate them.

Then validate isolated boot/menus, movement/fire/pause, save/load, movies/audio,
fullscreen/resize/focus, networking, textures and supported refresh settings.
Do not use personal saves or overwrite installed controls for automated tests.

The Steam Deck is the planned secondary Linux x64 hardware test. First run the
asset-free probe in desktop mode using a separate development environment; do
not disable SteamOS filesystem protections or install into its system image.
After a runnable game package exists, test desktop and gaming mode, built-in
controls/Steam Input, suspend/resume, fullscreen/scaling and performance. WSL or
software-renderer results do not replace those hardware checks. No Deck has been
connected or modified by this initial work.
