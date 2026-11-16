# Start a community mod project

Run these commands from the source checkout with its pinned Python dependencies
installed (`requirements.txt`). Windows can use `.venv/Scripts/python.exe`;
macOS/Linux use `.venv/bin/python`. Players use the launcher and need none of
these tools. A standalone source SDK can now be built without the game or server:
[SDK quick start and API reference](../sdk/README.md). Its creator-only command is
`python tools/mods/mod-creator.py`; the source workshop command below uses the
same parser/implementation. Publishing the SDK archive remains a release step.

Create a new project in an existing parent folder:

```console
python tools/mods/mod-workshop.py new build/my-mode --id my-mode --name "My mode" --author your-workshop-username --template gameplay
```

The gameplay template copies the original First to Three example into
`scripts/main.lua`, with an editable `mod.json`. It uses API1's restricted Lua
callbacks, score/message/end commands and isolated mode saves. It currently
targets offline Arcade Deathmatch. Edit the rules before packaging; the template
does not add entities, maps or multiplayer authority.

Choose `--template textures` for an empty texture project, or `mixed` for gameplay
source plus a texture folder. A texture-only project needs at least one imported
asset before packing. Existing destinations, including empty folders, are
refused. Project metadata uses the current draft package API; set the author
to your workshop username for your account's uploads.

## Import authored textures

Keep working PNGs and other source artwork outside the package project. Import
a PNG using the original texture's exact lowercase16-digit key:

```console
python tools/mods/mod-workshop.py import-texture build/my-mode private/my-art.png --key 0123456789abcdef --format DXT5
```

The key above is an illustrative placeholder. Use an actual key from your own
game's texture catalog, preserving its aspect ratio and channel semantics. The
importer does not discover game textures or redistribute original game content.
It writes `textures/<key>.dds` using the existing shared texture encoder, complete
down to the1x1 mip. The source artwork and project manifest stay unchanged.
`pack` computes the payload inventory and hashes later.

PNG input is bounded to32MiB, one frame and4096 pixels per axis. The default
format is DXT5; DXT1 and lossless BGRA8 (`--format RGBA8`) are also available.
The result must fit the renderer's24MiB per-texture limit. Output dimensions must
be powers of two; `--width` and `--height` explicitly resize both dimensions,
preserving the source aspect ratio. Resizing changes resolution, not the amount
of authored detail. `--channels luminance` stores opaque luminance;
`--channels alpha` preserves authored alpha with white RGB and requires DXT5 or
RGBA8. Choose these only when matching the original texture's channel semantics.
Other texture tooling offers separate original-alpha workflows.

A renderer-compatible DDS can also be imported; its bytes remain exact. PNG
encoding/resize/channel options are refused for DDS input. Unsupported files,
animated PNGs, missing mip levels, inconsistent DDS sizes, linked paths and
existing/case-colliding output keys are refused. To revise an imported texture,
preserve the prior project and create the next version in a new project folder;
there is no implicit overwrite flag.

The importer prints local provenance (source/output SHA256, dimensions and output
path). No local path sidecar goes into the package. It invokes no image decoder
for uploads: this local authoring command uses pinned Pillow only for the creator's
chosen PNG. Hosting keeps its existing structural checks, scanner, review and
publication gates.

## Import authored audio

```console
python tools/mods/mod-creator.py import-audio build/my-mode private/my-sound.wav --key 00000003
```

The key is8 lowercase hexadecimal digits below `ffffffff`, independent of original
game sound-event IDs. The importer preserves the exact WAV and source, refusing
links, existing/case-colliding keys and inputs inside the project. It checks the
complete RIFF container using the same PCM contract independently enforced by the
native game: PCM16, mono/stereo,8..48kHz, <=600 seconds/64MiB/64 chunks, exactly one
format followed by one nonempty aligned data chunk. No decoding/resampling or
playback occurs. Use `audio_api=1` and the key in your script; see
[sound commands](mod-audio-scripting.md). Native game audio acceptance now passes
through the actual launcher on all3 hosts. Older games without audio1 stay gated.

## Build and try the addition

```console
python tools/mods/mod-workshop.py pack build/my-mode build/my-mode-0.1.0.zip
python tools/mods/mod-workshop.py validate build/my-mode-0.1.0.zip
```

Keep ZIP output outside the project and choose a new filename for each build.
For package-local Lua modules, use `pack --modules` as described in
[the workshop guide](mod-workshop.md#package-a-modular-lua-project).
Upload through the launcher's creator page, complete scanning and moderation,
then install the approved release through Mods. Validation alone does not approve
or activate a mod. Current launcher/game capability and trust checks still apply.
Keyed PCM WAVs now run in audio1 games through the script addon. Map/OGG payloads
remain staged content without a playable integration.

`test_creator_projects.py` exercises new/import/pack/validate on all templates,
exact source preservation, PNG alpha/channel/mips, malformed/oversized/animated
inputs, key collisions and linked paths. It feeds imported DDS through the
actual shared native reader. `test_script_worker.py` packs both gameplay-bearing
templates and executes their rules in the real restricted VM through three kills
and the end-match command. These are tool/reader/VM checks; new live-game/public
creator acceptance and final SDK publication remain separate.

At source b0997ca9, Windows's creator/native-reader/packed-template VM/scope
selection passes172 cases; actual Mac/Linux selections including the linker
each pass187. Independent native authoring runs produce identical three-format
DDS payloads and the same mixed mod ZIP from matching authored pixels. The
actual native launchers inspect that exact package successfully on all3 hosts.
The proof ZIP is1847 bytes, SHA256
`441a092632eb8703b2e3816638f70a6901123f467662539bc3c847488328d598`.
These checks neither install nor publish the test package. STATUS records exact
launcher provenance and the remaining acceptance work.
CI43843861954 at that source completes23 successful jobs/two intentional issue skips.

The `audio` template creates a complete original sound-lab script plus two
deterministic PCM16 WAVs. See [sound scripting](mod-audio-scripting.md) for its
new/pack/validate commands and explicit audio_api=1 declaration. Current native
builds advertise audio1 after real launcher-to-game acceptance on all3 hosts;
creating a package does not bypass capability, scanning or moderation gates.

## Build the standalone SDK

From clean committed source, choose a new ZIP filename in an existing directory:

```console
python tools/release/package-creator-sdk.py --output build/TimeSplitters-Mod-Creator-SDK-v0.1.0.zip
```

The small deterministic source archive uses an explicit inventory, canonical LF,
fixed timestamps/permissions and uncompressed ZIP entries, keeping its identity
independent of zlib versions. `SDK.json` binds source/API versions and every input
hash; the adjacent `.zip.sha256` detects transfer corruption. The build refuses
dirty/untracked inputs, linked source and existing outputs. No game data, host
code, keys, private configuration or installed third-party dependencies enter it.
The SDK needs Python3.12+. Only PNG import needs pinned Pillow, listed separately
in `requirements-sdk.txt`. Detached CLI tests run standard-library commands under
`-I -S` from outside the repository, and separately exercise pinned PNG import.
The ZIP is a source SDK, not a bundled Python interpreter or a public release.
