# Linux acceptance before release

A passing WSL build does not establish support on every Linux installation.
The current preview targets Linux x64 with the Ubuntu24.04/glibc2.39 baseline,
a working desktop display and a system Vulkan driver. No Steam Deck transfer or
SteamOS modification is part of this work.

## Reproducible package checks

Build with the supported US game directory, then build/test the Linux launcher.
Run the native settings/debug/input/renderer self-tests with isolated settings.
Package with `tools/release/package-native-linux.py --version TAG --output PATH`.
The packager verifies source/binary receipts, relocates libraries and Python/Tk,
checks hashes and tests fresh installation, upgrade preservation and executable
corruption rejection using the actual launcher. Keep failed stages for diagnosis;
use a fresh output directory for another attempt.

Extract the finished ZIP into a new folder containing spaces and non-ASCII
characters. Make that folder read-only and run as an ordinary user, with no
checkout SDK, system Python packages or developer loader paths available:

```sh
./Launch.sh --game --settings-self-test
./Launch.sh --game --debug-self-test
./Launch.sh --game --input-self-test
./Launch.sh --game --renderer-self-test
./Launch.sh --game --data '/path/to/your/game'
```

Direct game mode writes config/data/state/cache under the corresponding XDG
user directories; relative XDG overrides fall back to the home-directory default.
First-run controls are published atomically without replacing existing settings.
The launcher itself currently expects a writable installation for updates and
installation locks. Game data is always supplied separately and is not packaged.

Use `tools/testing/test-native-linux.py --package-root PATH` to run the same
bounded, isolated Story observer against a relocated package. Add `--movies`
for separate startup-video checks. Routine Story performance runs skip startup
movies; disable captures while measuring performance, and validate pixels in a
separate captured run. Keep60/120/180/240 preset selection separate from measured
sustained frame rate. Linux capture-to-bind remains pending; dropdown binding
editing is available in the desktop settings adapter.

## Hardware and session matrix

Record the package hash, distribution, kernel, glibc, GPU/driver, display session,
resolution, preset and observed frame intervals for each result. Unrun checks
stay pending; none should be inferred from another operating system's pass.

| Environment | Required acceptance |
| --- | --- |
| Native AMD/Mesa desktop | Wayland and X11, movies/audio, resize/fullscreen, save/reload, gameplay and frame pacing |
| Native Intel/Mesa desktop | Same checks plus integrated-GPU memory pressure |
| Native NVIDIA desktop | Driver loading, Wayland/X11 focus/capture and fullscreen transitions |
| Fresh Ubuntu24.04 desktop | Extract/import/play with no SDK, development checkout or system Python setup |
| Fedora/current desktop | Verify compatible ABI and dependency loading; do not infer from Ubuntu |
| Steam Deck, later | Desktop/Gaming Mode, Steam Input, text entry/scaling, battery/thermal behavior and resume |

For each available native host, test controller unplug/reconnect while a button
is held, focus loss during movement, audio-device switching, display changes,
and suspend/resume. Run multiple Story missions, Arcade and matching-build
multiplayer, then repeat save/load and level transitions in a longer session.
Use --resources to track owned-child memory, threads and descriptors across repetitions; a shorter first-mission pass is
not full-campaign or multiplayer acceptance. Do not suspend a user's computer
or alter its driver/system image automatically.

Before public distribution, finish dependency source/license auditing, complete
clean-machine and native-hardware checks, and exercise the actual release/update
path. Local preview ZIPs and configured CI jobs are not a published Linux release.
See [STATUS](../project/STATUS.md) for measured results and remaining work.
