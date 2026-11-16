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
US350C70 resolves language paths; US1504F0 hashes names. US27FA0/US28528 derive
frequency from event/table data. These are verified audit entry points, not a
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
Windows38 cases pass; Mac/Linux and sanitized native checks remain unverified
until their results are recorded in STATUS. This proves an input parser, not
asset replacement, sound quality, decoder coverage or a released audio SDK.
