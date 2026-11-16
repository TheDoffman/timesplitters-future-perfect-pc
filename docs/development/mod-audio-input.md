# Custom audio input boundary

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
