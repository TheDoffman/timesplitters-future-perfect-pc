# Native audio asset snapshots

`src/mods/mod_audio_assets.cpp/.hpp` implements a private immutable WAV store.
The trusted launcher/bootstrap must first authenticate the catalog/packages,
resolve the active profile and supply its authorized package digests and asset
pins. This component verifies the supplied bytes and policy; hash equality alone
does not authenticate a publisher. Uploaded manifests and Lua cannot supply
their own authority or directly invoke this native API.

Each pin contains a private resource key, package digest, absolute UTF8 data path,
SHA256 and exact byte count. Keys are mount-local integers, not guest addresses,
filenames or a public mod naming scheme. Zero is valid; UINT32_MAX is reserved.
The loader accepts at most32 unique authorized packages,8192 unique resource keys,
64MiB per file and256MiB of total WAV snapshots, plus bounded container overhead.
Every declaration and the aggregate budget are checked before any asset read.
Duplicate resource keys, unauthorized owners and invalid digests are refused.

`read_pinned_data_file` hashes the same opened regular file that it copies into
memory. Files and ancestors cannot be symbolic links/reparse points; the opened
file must have exactly one hard link. Actual and declared sizes must match.
Growth, truncation, changed bytes, missing/special files and cancellation fail
the whole load. Reads are bounded to32KiB chunks. Cooperative cancellation is
checked before/between files and reads. No file is written or replaced.
The existing `read_pinned_file` interface keeps its32MiB source/helper policy;
the new64MiB path is for data, not executable helper staging.

The actual shared [PCM WAV parser](mod-audio-input.md) validates each snapshot.
No OS/third-party decoder runs. A failed file never publishes a partial store.
On success, each entry owns its bytes and borrowed PCM view; sorting moves owned
pointers rather than the underlying entries. Later file replacement cannot
alter captured audio. `find` uses binary search and performs no IO, allocation,
decoding or locking. The caller must keep the store alive until every borrowing
voice has drained. Load/destruction belong on initialization/control threads,
never an audio/render callback. This is separate from per-voice/mix budgets.

The production parent library compiles this component and the existing WAV
parser with its normal warnings/hardening. The OS-confined Lua worker receives
no file access or new commands. `TSFP_MOD_TESTING` adds a private native driver;
that driver is excluded from production helper bundles.

`tests/mods/test_audio_assets.py` exercises the actual native loader with distinct
original PCM vectors, unsorted/zero/high keys, multiple authorized owners,
invalid policy/hash/size/content, partial-load refusal, cancellation, links,
Unicode paths and source replacement. A native allocation counter checks repeated
lookup. A valid64MiB WAV and four-resource256MiB mount pass; a fifth resource
is refused. The test uses one disposable source file, not copied game assets.
`test_mod_capsule.py` checks the existing shared reader/capsule behavior too.
At source ef78a73c, the combined asset/capsule/CI-selection suite passes196 cases
on Windows29.41s, Mac14.77s and Linux20.52s. Two final Windows handshake/Unicode
checks also pass14.07s. Shared image/staging regression passes34 Windows cases
with five intentional platform skips. All three normal native game builds pass
their checks. Actual host ASan/UBSan runs pass23 asset-store cases each on Mac
8.61s and Linux15.45s, with leak detection disabled; the large256MiB boundary
case is excluded from sanitizer runs and passes normally on all three hosts.
These are actual loader/parent-build checks, not custom game-audio acceptance.
CI43857620225 at that source completes23 successful jobs/two intentional issue skips.

## Private launcher capsule

`ModAudioCapsule` in `tools/launcher/ModScriptLaunch.cs` encodes the data-only
handoff. `src/mods/mod_audio_capsule.cpp/.hpp` reads it and calls the actual
immutable loader above. This is an internal format, not a public mod manifest.
The trusted caller must authenticate packages and resolve the active profile
first. The native reader requires that independent active package set; capsule
contents cannot authorize themselves. Neither hashes nor this wire authenticate
a publisher by themselves.

All integers are unsigned little endian. The header is eight ASCII bytes
`TSFPAUD1`, a four-byte package count (1..32) and a four-byte asset count
(1..8192). Exactly that many 32-byte package digests follow in strictly ascending
byte order. Each asset is exactly48 bytes: resource key u32, package index u32,
exact WAV byte count u64 and payload SHA256 (32 bytes). Assets are strictly
ascending by key; UINT32_MAX is reserved. No trailing bytes are permitted.
The maximum capsule is394256 bytes (16+32*32+8192*48). Per-file and aggregate
limits match the loader; repeated payloads still count per resource snapshot.

No record contains paths. From the separately supplied absolute private cache
root, the native reader derives only `<payload-sha256>.wav`. It checks the root
with `checked_path`, verifies a single-link capsule snapshot against the expected
hash, matches the exact sorted independent owner set, then validates all records
before the loader reads any WAV. The capsule itself uses the cancellable data
reader. Malformed/truncated/overlarge capsules, duplicate/out-of-order keys,
unknown owners, mismatched digests, aliases and cancellation publish no assets.
The C# encoder owns its serialized bytes and copies input records before sorting.

`tests/launcher/mod-launch-vectors.json` carries the same original synthetic WAV
and exact capsule bytes/hash for the C# launcher and native tests. The shared UI
suite checks the actual encoder; `tests/mods/test_audio_capsule.py` checks the
actual native capsule-to-store path, malformed policy, independent authority,
links and Unicode paths. CI registers this native suite alongside the store.

Launcher cache staging/startup wiring, resource binding and actual custom
playback remain integration work. Audio packages remain inactive in the launcher.
No new audio capability or replacement permission is advertised by this component.
