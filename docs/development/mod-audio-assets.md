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

At source809bba7d, the native capsule/store/source/scope selection passes227
cases on each actual host (Windows46.58s, Mac22.54s, Linux30.98s). This includes
loading the maximum32-package/8192-resource capsule and refusing an extra byte.
Native launcher builds and shared checks pass on all three hosts (112 Windows,
113 Mac,114 Linux UI groups), with the actual fixture creator backend. Native
parent ASan/UBSan capsule checks pass31 each on Mac9.31s/Linux16.42s, with leak
detection disabled. CI43861454203 completes23 successful jobs/two intentional issue skips.

## Profile staging and startup

`ModLibrary` stages audio in the same unpublished generation as the script and
texture overlay. In this private integration, a script mode may include flat
`audio/<eight-lowercase-hex-key>.wav` inputs; these keys are mount-local resources,
not original-event replacement IDs. Ogg, nested names and standalone audio-only
packages remain inactive. The stable creator API and original-event bindings are
separate work; this naming rule does not establish those bindings.

`StageAudio` creates `audio/<payload-sha256>.wav` with CreateNew, hashing the same
bounded byte snapshot it writes, and deduplicates equal payloads. `audio.bin`
binds all selected package identities, including texture dependencies. The
profile is published only after the whole generation is staged and trust is
rechecked. Cancellation/failure retains the old profile. Known obsolete cache
files can be recycled; unknown files survive refusal. At launch,
`ConfigureAudio` recomputes the expected capsule, rechecks exact capsule/cache
bytes and the flat cache inventory, and emits only the explicit three-variable
handoff: `TSFP_MOD_AUDIO_CAPSULE`, `TSFP_MOD_AUDIO_CAPSULE_SHA256`,
`TSFP_MOD_AUDIO_CACHE`. Ordinary recovery clears inherited TSFP variables.

`mod_bootstrap` accepts these three variables only together with a verified
script capsule. It obtains the independent package set from that source capsule,
not from the audio capsule. It loads the complete immutable store before opening
the save namespace or starting the guest. Incomplete, extra-owner, changed,
unsupported-WAV or mismatched requests fail startup and poison the storage fence;
they cannot fall back to original saves. Both script and audio requests absent
keep ordinary startup. This initial bootstrap route requires a script profile;
standalone audio mounting is still integration work.

`pc_mod_bootstrap_audio(key)` is unavailable before a successful isolated-storage
commit and after runtime invalidation. Lookup is allocation/IO-free. Previously
borrowed PCM remains owned until process exit so draining voices cannot access
freed storage. It does not start any voice or select an original event.

The shared launcher requires both script capability1 and `mod_audio_version:1`
in the installed game manifest before enabling a script/audio package. Current
production game builds deliberately do not advertise the audio capability until
renderer integration and actual playback are verified. The launcher tests use a
fixture-only capability to exercise real install, stage, configure, tamper
refusal, disable and unknown-file preservation. Native bootstrap tests exercise
actual source/audio readers, PCM snapshots and storage commit with narrow host
storage adapters, including source-owner binding and retained views after
invalidation. These checks do not establish custom playback.

Resource/mixer binding, script audio API, playback clocks/pitch/loop/stop and
voice budgets remain open. No audio capability or replacement permission is
advertised by production builds yet.

The actual game source list is `src/mods/game.cmake`, separate from the helper/
component CMakeLists. It includes the store, capsule and WAV parser with strict
source warnings. The initial Windows startup build exposed missing loader symbols;
adding these common sources corrects the link on all game hosts. Normal native
game builds verify this integration independently of the component drivers.

Startup integration evidence: native bootstrap/capsule/store/source/scope tests
pass257 cases on Windows62.65s, Mac29.56s and Linux41.98s at implementation50c4dd69
(Windows tested the identical precommit source content). Normal game builds at
link-fixaf18d689 pass all three hosts and their normal native checks. Launcher
build/tests pass113 Windows,114 Mac and115 Linux shared UI groups plus the actual
fixture creator backend; Mac/Linux builds are ataf18d689, Windows uses the same
shared launcher code at50c4dd69. Actual native parent ASan/UBSan bootstrap/capsule
suites pass61 cases on Mac18.48s/Linux32.53s ataf18d689, with leak detection
disabled. No custom playback or game-audio sanitizer injection is claimed.
Sourceaf18d689 is pushed and synchronized; CI43863701975 completes23 successful jobs/two intentional issue skips.

The next host playback component is described in [custom audio voices](mod-audio-player.md).
Its private native controls and validated Lua sound commands are integrated.
Production activation and actual in-game sound acceptance remain pending.
