# Custom audio input boundary

[Immutable asset loading](mod-audio-assets.md) now owns hash-pinned validated
WAV snapshots. Current startup/launcher wiring and scripted audio playback pass
on all3 native hosts; see mod-audio-scripting.md and STATUS for exact acceptance.
`import-audio` now provides creator-side PCM preflight and exact copying. Its
Python parser is checked against this independent native parser, including
mutated headers, truncations, unsupported formats and metadata/PCM equality.

## Authored source snapshot

`TsfpModAudioSource` privately carries event ID, authored rate, channel count and
whole-source loop flag. The supported-US request adapter reads the event record
through fresh guest-RAM/OS span checks, bounds the35-entry rate selector and
compares the selected live Q12 value with the supported XBE's original table.
The authored rate uses the original integer48000/4096 conversion. Channels and
looping come from the already audited event flags. These are host binding metadata,
not a new script command, resource permission, package format or audio activation.

A zero authored rate means unavailable metadata; ID-only attribution can remain
valid. Invalid IDs mask the whole snapshot. Copies are held in the existing
eight-deep guest-thread scope, with the same overflow/unwind rules. The original
callee still runs once with unchanged register/stack effects. A successful native
buffer bind copies the snapshot; failed binds clear it, stops retain it, and model
retirement clears it. A later pitch change never rewrites the authored base.
The optional lifetime observer includes `authored`, `channels` and `looping`.

Native fixtures exercise valid22042Hz stereo/loop metadata and5859Hz mono
metadata, an unaligned/read-only event record, denied event/global pages,
guest-RAM bounds, overflow, invalid selectors and changed live Q12 values.
Separate scope cases cover copied values, nested masking and thread isolation.
Actual HLE fixtures verify preservation through pitch/play/stop, clearing at
retirement and clearing every field after native bind failure. Windows's full
audio/source/WAV selection passes161 cases; seven affected cases pass again
after the final guard/scope additions. No replacement playback is enabled.

The corrected host-page fixture at301767f7 passes all161 Mac cases5.09s; Linux's
earlier161-case pass9.60s is followed by all three corrected guard cases0.51s,
and Windows's three corrected guard cases pass2.35s. The initial Mac failure was
an unaligned mprotect request in the test:16KiB host pages require rounding the
metadata address to the actual page size. No production read check was relaxed.
All three normal301 game builds pass their native checks.
The standalone guarded-reader fixture also passes ASan/UBSan on actual Mac and
Linux hosts; leak detection is disabled. No sanitizer is injected into the game.

Real isolated six-bot Arcade scenes then exit0 (85 seconds Windows,100 Mac/Linux).
Every known Play snapshot in the bounded traces matches independently parsed
original-bank rate/channels/loop metadata:89 Windows,88 Mac,87 Linux. Playback
differs from the captured authored base in17/15/16 cases, while authored metadata
remains intact. Each host observes one authored looping Play; the four unknown
initial plays retain zero metadata. These are unmodded source-binding checks,
not custom audio, stream coverage or replacement-clock acceptance.
Evidence lives in private/test-saves/audio-authored-3017-{win,mac,linux}/evidence.json;
Windows analysis copies are logs/mod-audio-authored-*-proof.json. Source is pushed
and synchronized; CI43850646185 completes23 successful jobs/two intentional issue skips.

## Playback lifetime observation

Patch2715 adds a separate opt-in `TSFP_MOD_AUDIO_LIFETIME_TRACE=1`, capped at512
metadata records per process. It observes host buffer bind, play, frequency,
changed loops, pause/resume, stop and retirement under the existing audio lock.
Unknown event IDs are included: `4294967295` means no verified source identity.
The interface plus generation distinguishes a reused buffer from an earlier
binding. Records include raw/decoded sizes, original/current sample rates, decoded
loop bounds, play flags, playing/paused state, requested operation argument and
result. A loop end of0 means the full decoded source end. The observer reads no
guest memory or PCM, changes no attribution, clocks or samples, and invokes no
decoder. It neither enables nor authorizes replacements.

The actual HLE fixture binds event42, changes pitch48000->24000, sets loop8:24,
plays, pauses/resumes and stops. A later settings-rate change retires the old
model and starts a new unknown generation at22050. The observer faithfully records
that existing loss of identity instead of inventing a continuation. A600-call
probe confirms the512-record cap and byte-identical model/sample state. Separate
processes verify only the exact opt-in value1 enables logging. Windows's complete
audio/source/WAV selection passes161 cases. Native host fixtures and actual game
playback traces remain pending at this source checkpoint.

At source48465dd9 the same161 cases pass on the actual Mac/Linux hosts
(5.26s/9.49s), and all three normal game builds pass their native checks. Windows
uses the supported Windows PowerShell build environment: a PowerShell7 attempt
failed in existing icon generation before the game build; the normal retry passes.
No generated game code, optimization or system memory settings were changed.

Isolated unmodded six-bot Arcade scenes exit0 on all three hosts (85 seconds
Windows,100 Mac/Linux). Each captures512 records through roughly50..53 seconds,
including gameplay. Known Play counts are91/90/89 respectively, and every known
Play matches a prior bind of the same event/interface/generation. Four unknown
initial plays on each host have64-byte sources at48000Hz; their identity stays
unknown. Event5490 plays with the original whole-source loop flag,112768 decoded
bytes and16007Hz on all hosts; no explicit loop-region mutation was observed.

Every known Play differs from its buffer's initial44000Hz template rate. The
existing bank audit reads only280326 bytes of original metadata and the verified
XBE's35-entry frequency table. Comparing those authored rates with actual Play
gives69/80/71 exact matches and22/10/18 differences on Windows/Mac/Linux. Thus
the template rate is not the authored base for replacement pitch scaling. For
example, event3371's bank rate and actual Play are22042Hz, although the buffer
template says44000; event5490 similarly matches16007Hz and its bank loop flag.
No original audio payload is copied. This is evidence for the next source-metadata
binding step, not a replacement implementation or complete bank/stream coverage.

Ignored host evidence: private/test-saves/audio-lifetime-4846-{win,mac,linux}/evidence.json.
Windows analysis copies: logs/mod-audio-lifetime-*-proof.json,
logs/mod-audio-lifetime-analysis.json and logs/mod-audio-authored-frequency-proof.json.
Arrival timestamps are bounded pipe-capture times, not exact game ticks.
CI43846826893 at48465dd9 completes23 successful jobs/two intentional issue skips.

`src/mods/mod_wave.c/.h` validates an immutable caller-owned WAV snapshot before
its PCM could reach game audio. It performs no IO, allocation, conversion,
resampling, native decoder calls or playback. It is a shared input component,
not an enabled audio replacement feature. The game audio hook, creator import
command, publication/client policy and runtime/package capability remain open.
Existing staged audio packages remain inactive.

The initial native input contract accepts little-endian RIFF/WAVE with exactly
one `fmt ` chunk before one nonempty `data` chunk. PCM format1, signed16-bit,
mono/stereo,8..48kHz, exact frame alignment and matching byte rate/block alignment
are required. PCM's optional WAVEFORMATEX extension must be empty. RF64, RIFX,
compressed formats (including Xbox ADPCM), floating point, extensible formats,
duplicate format/data chunks, truncated chunks/padding, inconsistent lengths and
trailing bytes are refused. Unsupported input is never passed to an OS decoder.
The bounds are64MiB for the entire snapshot,64 chunks and600 seconds. These are
input limits; they do not establish a game voice/mix budget or promise arbitrary
duration replacements. Ancillary chunks are skipped and their loop/metadata
instructions are not interpreted.

`tsfp_mod_wave_parse` returns a borrowed PCM byte view, frame/rate/channel counts
or a fixed typed error; every failure clears its output. Callers must retain the
immutable authorized snapshot, check package identity/hash and enforce the active
profile before using the view. The view contains little-endian bytes, with no
promise of host sample alignment. No game addresses, asset paths or executable
instructions come from this format. No audio filename/key scheme is finalized.
See [RIFF](https://learn.microsoft.com/en-us/windows/win32/xaudio2/resource-interchange-file-format--riff-)
and [PCM format fields](https://learn.microsoft.com/en-us/windows/win32/api/mmreg/ns-mmreg-waveformatex)
for container and frame-size conventions; the smaller supported subset is ours.

## Existing loader evidence and next integration gate

The earlier original-asset audit in `tools/analysis/audio_bank.py` establishes
5703 sound events, filename/language conversion and CRC32 bank lookup from the
supported US loader. US27700 reads lists; US27290 converts extensions;
US350C70 resolves language paths; US1504F0 hashes names. US27FA0 computes an original duration using event/table data; the frequency path
in US28240 is not a codec inference. These are verified audit entry points, not a
new mod patch or permission to redistribute original assets.

Existing `pc_audio_mix_guest.inc`/patch0060 observes sound identity before
US299C0 starts a source on a pre-created stream. The ordinary bank has both
mono buffers and stereo streams; music uses another stream/rate table. That
scope alone does not prove a safe complete replacement route. Buffer/stream
completion clocks, packet ownership, pitch, authored loop regions and cleanup
must be mapped before switching source bytes. Do not replace an arbitrary
packet or infer codec/rate from a filename. Preserve original asset bytes and
neutral playback; require actual controlled replacement/fallback/loop/stop
checks on all three hosts before exposing audio activation in the launcher.

## Validation

`tests/mods/test_wave_input.py` compiles and calls the actual native parser.
It verifies exact PCM preservation, both channel layouts, sample-rate bounds,
optional empty extensions, ancillary chunks, malformed/duplicate/truncated input,
every truncation of a valid sample, padding, frame alignment, fixed chunk/byte/
duration budgets and zeroed failed output. `native_wave_fuzz.c` runs200000
deterministic mutations against misaligned source storage and checks every result.
`TSFP_MOD_SANITIZE=1` adds ASan/UBSan to that standalone mutation executable on
Clang/GCC; no sanitizer runtime is injected into Python or production helpers.
Windows38 cases pass; the actual Mac also passes the selected200 parser/scope/
suite/shard checks with the standalone mutation harness under ASan/UBSan. Linux
remains unverified until CI records its new source results. This proves an input parser, not
asset replacement, sound quality, decoder coverage or a released audio SDK.

## Source attribution before replacement

Patch2713 wraps verified US28240 once, reading its request pointer at entry ESP+4
and the event word at request+0 through guest-range guards. The pinned US XBE
was inspected before choosing this ABI. Direct music/stream paths remain unknown.
`mod_audio_source.h` tracks at most eight scopes per guest thread; invalid IDs
mask a parent's identity, overflow clears attribution, and returned/skipped guest
frames expire even when guest exceptions skip the normal wrapper leave. A valid
ID is0..5702; unknown is UINT32_MAX. This diagnostic identity is not a public
mod filename contract or a package permission.

SetBufferData stamps the buffer after the existing model update; new slots and
unattributed rebinds are unknown. Stop retains the bound source identity. The
optional TSFP_MOD_AUDIO_TRACE=1 emits at most128 metadata records per process
(event, interface/generation, byte/rate/alignment/loop bounds), under the existing
audio lock. It never dumps audio bytes. Source samples, decoding, frequency,
voice ownership, play/loop boundaries and completion clocks are unchanged.

The native source fixture checks both platform defines and the inactive path,
original-callee count/register/ESP effects, malformed and unaligned requests,
invalid IDs, nested masking, overflow, skipped leave via longjmp and actual thread
isolation. The real HLE buffer fixture also checks binding/rebinding, stop and
unchanged PCM/rate/alignment. Windows selected source/WAV/audio lifecycle checks
pass156 cases; the actual Mac passes the same156 cases4.64s. Runtime build,
controlled native source traces and Linux checks remain required; no
replacement/audio activation is claimed here.

Patch2714 additionally requires successful native SetBufferData completion before
assigning a requested event ID. A failed helper clears attribution to unknown
while preserving the original error result, registers, data and clocks. The
native-source-error fixture reproduces the prior incorrect stamp and passes with
the guard in a privately patched Windows HLE fixture. Actual runtime checks for
this follow-up are tracked separately in STATUS.

The request adapter now uses a fresh shared pc_guest_span read query for each
word, additionally fencing guest-RAM extent and host-pointer overflow. It accepts
read-only request storage and leaves denied/unmapped input unknown. The earlier
menu-local guard had no definition in the Windows manual translation unit; the
full link exposed that dependency. The new native fixture allocates real OS pages
and tests denied stack/request access, read-only request access and overflow,
without supplying a replacement guard. Windows final source/WAV/HLE selection
passes157 cases7.51s; final native linking and matching Mac/Linux checks remain
pending at this checkpoint. Replacement audio is still not enabled.

## Native Windows observation at source0ece5ff0

The normal optimized Windows build now links and passes its complete checks using
one compiler with UseMultiToolTask/EnforceProcessCountAcrossBuilds. The earlier
memory and unresolved-guard failures are superseded by this successful build;
no generated C or optimization was changed. Native source/WAV/HLE selections
pass157 cases on Windows and the actual Mac (5.21s on Mac).

An85-second isolated, unmodded six-bot Arcade scene exits0 and records exactly
the bounded128 observations spanning47 valid event IDs. Game SHA256 is
2a8528cd14c5a523a7e98f742582488751f800763872ebea203a805fd306a0d9.
Evidence SHA256 is
f2d5d172d7e5ec2d46caf7c0ca7d965e6a76a9b416a40dfe6f3f106354d7dd4b.
These prove the adapter is actually reached in this Windows scene. They do not
establish complete bank/stream attribution or custom replacement playback.

The traces also show a single event/buffer generation at different rates during
initialization (event3371:44000 then22042). An early SetBufferData snapshot must
not be treated as a final authored playback rate. Matching original frequency,
loop/stop/packet lifetime and source identity remains a prerequisite for replacement.
The full source0ece CI run passes Windows/Mac runtime and patch shards, Linux
Vulkan, GPU, native launcher and package checks; actual Mac/Linux audio-hook
scenes remain separate acceptance work.

Actual Ubuntu-24.04/WSL checks at bbed037f now pass the same157 source/WAV/HLE
cases in13.45s after applying the current runtime patch stack. The first run
started before patch application and failed fixture compilation; the rerun on
the patched runtime passes without a product change. Linux game compilation and
an actual audio-hook scene remain pending; the native test pass does not enable
replacement playback.

## Native Mac and Linux gameplay observations

The clean bbed Linux build passes its protected-memory/MMIO checks and a100-second
isolated unmodded six-bot match under Mesa lavapipe. It exits0 and records128
bounded observations spanning54 valid sound IDs. Its game SHA256 is
d621e75a138be1f5787c825d4f9a4b4ccdc809629563ed6205982913ae8c4b92;
evidence SHA256 bef2263d3b23886a2573a6e04f466b0a1c72640bf8f6fba565895a7a94269f97.
The later c662 release-media build is a distinct executable and needs package/
game acceptance; it must not inherit this scene's binary identity.

The clean c662 Mac Vulkan/MoltenVK build passes platform/MMIO/path tests and
renderer/input/settings/debug self-tests. A100-second isolated unmodded six-bot
match exits0 with128 bounded observations spanning60 valid sound IDs. A private
read-only observer supplies stage/bot evidence; no mod capsule is active. Game
SHA256 e4effbcab1a8703bf2e2c8c12e112905c8bb3e95fc0c9e4d93ad8ac0730c1ddd;
evidence SHA256 abca70b418fe6722cac0a590cb565c200b101e4238701ecb36bf2a584bf986ed.
Original asset trees are read without a second extract; only the supported XBE
and synthetic Test Lab fixture are copied into task-owned compiler/test paths.

Together with Windows these establish real source-attribution entry on all three
hosts. Complete source/bank/stream/rate/loop ownership and replacement playback
remain unfinished. No audio replacement is enabled by these checks.
