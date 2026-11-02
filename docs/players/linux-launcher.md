# Linux launcher

The Linux x64 launcher uses the same interface, settings, game-data importer,
save management and verified update transactions as Windows and Mac. It runs
natively; Wine, Python and a system .NET installation are not needed to run the
self-contained launcher download. Ubuntu 24.04 x64 with an X11 desktop is the
initial validation host. Wayland desktops need XWayland. Steam Deck acceptance
is still pending.

Extract the **whole** `TimeSplitters-Launcher-v<version>-linux-x64.zip` into a
writable folder you own. Keep `TimeSplitters.Launcher` and `Support` together.
If your archive manager loses permissions, run these commands in that folder:

```sh
chmod +x ./TimeSplitters.Launcher
./TimeSplitters.Launcher
```

Install your distribution's desktop libraries if needed. On Ubuntu these include
`libfontconfig1`, `libx11-6`, `libxrandr2`, `libxi6`, `libxcursor1`, `libice6`,
`libsm6` and `xdg-utils`. The game additionally needs its Linux Vulkan/runtime
dependencies; see [Linux development](../development/linux.md).

The launcher offers only published `linux-x64` game packages on the selected
channel. A launcher ZIP contains no game runtime or original game data. Until a
matching Linux game release is published, the launcher reports that none is
available. A source build can be used through the checkout workflow below.
Windows and Mac runtime ZIPs cannot be installed as Linux updates.

When a Linux runtime is installed, choose **Locate ISO** or **Use extracted
files** to import the supported US Xbox game. Then use **Play Game**. Settings,
saves and imported data remain in the installation folder across updates. Game
startup uses the system Vulkan driver; inherited Mac/software-driver overrides
are cleared. Desktop mode choices include common presets, Auto and any saved
custom mode. Automatic desktop refresh switching is unavailable.

## Source builds

Run in the native Linux checkout described in the development guide:

```sh
.venv-linux/bin/python tools/build/build-linux-launcher.py --install-sdk --test --package
build/linux-launcher/package/TimeSplitters.Launcher --root "$PWD"
```

The build verifies the pinned .NET SDK and locked Avalonia dependencies. `--test`
needs a display; use `xvfb-run -a` on a headless machine. To embed the original
launcher artwork, supply `--artwork-dir <folder>` containing `launcher-banner.png`,
`launcher-backdrop.png` and `launcher-logo.png`; `--require-artwork` makes those
files mandatory for release builds.

The checkout launcher uses `build/linux/game/futureperfect_recomp` and its
`BUILD.json`, `private/game`, existing `config` preferences, `private/saves` and
`private/native-settings`. Importing extracted files copies them into
`private/game`; it does not select an external data directory in place. Avoid
another import when the checkout already has its game data. Source checkouts
cannot replace themselves with published packages.

`--check --root <checkout>` validates the supported game-data hash, native ELF
x64 executable/hash and settings without launching the game. It does not prove
gameplay, audio, physical input or graphics correctness. Launcher package tests
use disposable synthetic data, then install, corrupt, repair and restart the
actual packaged launcher. Personal saves and settings are not test fixtures.

On Wayland the launcher cannot observe another process's game window. It
therefore retains the previous game's recovery cache rather than claiming the
new version was visually accepted. Game process monitoring still restores the
launcher when the game exits.
