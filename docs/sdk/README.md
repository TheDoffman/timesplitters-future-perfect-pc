# TimeSplitters Mod Creator SDK 0.1.0

Build original texture additions and sandboxed gameplay modes, including custom
sounds. This source SDK runs on Windows, macOS and Linux with Python3.12 or newer.
It needs no game source checkout, Git, CMake, Lua installation or hosting service.
Players use the game launcher and do not install Python or this SDK.

Extract the ZIP to a writable local folder. `SDK.json` records its exact source,
API versions and file hashes. Keep the ZIP checksum supplied with the release.
The SDK and game are experimental; the package format identifier remains
`tsfp-mod-v1-draft`. The installed game must advertise script1 and, for sounds,
audio1. A validator cannot grant capabilities to an older game.

## Make a project

Run commands from the extracted SDK folder. On a system where Python is named
`python3`, substitute that name for `python` below. Create a `projects` folder:

```console
python tools/mods/mod-creator.py new projects/my-mode --id my-mode --name "My mode" --author your-workshop-username --template gameplay
```

Use your workshop username as `author`. Change the ID/name/description and
three-part numeric version in `mod.json`; retain its game/API fields and leave
`files` as `{}`. Packing generates the exact inventory. Change
`scripts/main.lua` to write your rules. The example awards one point per kill
and ends the match at three. Current gameplay modes run in a new offline Arcade
Deathmatch, use separate saves and cannot join online matches.

Templates are `gameplay`, `textures`, `mixed` and `audio`. The audio template
contains a working control sequence and two original generated WAVs. Texture
projects start empty; import at least one asset before packing. A mixed project
combines a gameplay script with a texture folder. The included
`examples/mods/modular-first-to-three` separates settings and score logic; copy
that folder into `projects` and edit its identity before using `--modules`.

## Import your artwork and sounds

PNG import needs the pinned Pillow dependency. Install it into your own virtual
environment using `python -m pip install -r requirements-sdk.txt`. Project
creation, DDS/WAV import, Lua linking, packing and validation use only Python's
standard library. The SDK never downloads dependencies automatically.

```console
python tools/mods/mod-creator.py import-texture projects/my-mode ../my-art.png --key 0123456789abcdef --format DXT5
python tools/mods/mod-creator.py import-audio projects/my-mode ../my-sound.wav --key 00000003
```

Keep authored inputs outside the project. Imports preserve their exact source
files and refuse existing keys. Texture keys are the game's existing16 lowercase
hexadecimal DDS keys; the SDK does not infer a key from an image. PNGs must be
static, at most4096 pixels per axis, with power-of-two output dimensions and
unchanged aspect ratio. DXT1/DXT5/RGBA8, mipmaps and channel layouts are supported;
use `import-texture --help` for resizing/channel options. Compatible DDS input
retains its exact bytes.

Audio keys are8 lowercase hexadecimal digits below `ffffffff`. Import accepts
exact little-endian PCM16 WAV, mono/stereo,8..48kHz, at most600 seconds/64MiB and64
chunks. It validates the complete container and preserves bytes, including
ancillary metadata. Export compressed/float audio to this format in your audio
editor first. In an `audio_api=1` script, `game.audio.play(0, 0x00000003)` plays
the imported sound. See [API reference](API.md) for controls and limits.

## Package, review and play

```console
python tools/mods/mod-creator.py pack projects/my-mode my-mode-0.1.0.zip
python tools/mods/mod-creator.py validate my-mode-0.1.0.zip
```

Add `--modules` when packaging multiple local Lua files. Keep ZIP output outside
the project and choose a new filename for each build. Only supported mod content
belongs inside a project: scripts, keyed textures and keyed WAVs. Keep editor
backups, personal files, executables and dependencies outside it.

For local testing, import the ZIP from the launcher's Installed mods view, review
activation, then Play a new offline match. Use Play without mods to recover from
a faulty mode. Disable/remove through the launcher; ordinary saves are separate.
For public distribution, sign in under Creator, upload the ZIP, wait for scanning
and moderator review, then use Mods search/install. Each changed release needs
a new project version. Dependencies use exact mod IDs/versions in `mod.json` and
are reviewed during installation and activation.

`validate` checks structure, types and hashes without executing Lua. It does not
prove a script is correct, authorize publication or replace antivirus/moderation.
Runtime budgets and process isolation remain enforced in the game. Maps/OGG and
arbitrary new entities do not have a playable creator integration in this SDK.
Original game assets are not supplied and must not be included in your upload.

Source and broader project documentation:
[TimeSplitters Future Perfect PC](https://github.com/TheDoffman/timesplitters-future-perfect-pc).
