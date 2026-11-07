# Unidentified-entry audio survey

`tools/analysis/audio_candidates.py` narrows review candidates without changing
the source census or asserting that compatible bytes are audio. It is portable
Python and reads the existing game data; no payload is extracted or rewritten.

```sh
python tools/analysis/audio_candidates.py --game /path/to/existing/game \
  --inventory /private/audio/inventory.json --output /private/audio/candidates-new.json
```

Use a decoded/copy-verified supported-US census and a new private output outside
the original game directory. The command validates the XBE identity and all
archive hashes. Before excluding a known CRC, it rechecks the complete payload
SHA-256 against the known decoded-content mappings, so a CRC alone cannot hide
different data. Invalid archive extents, changed sources and escaping paths fail.

## What is inspected

Each remaining uncompressed entry is checked for RIFF/WAVE, VAG, Ogg-container
and XMV magic/header candidates. Signature detection is not decoder validation:
for example XMV magic does not certify a version or audio track. Complete raw
Xbox ADPCM candidates must have integral36-byte mono or72-byte stereo blocks,
step indices0..88 and zero reserved bytes in every header. Stereo headers are
adjacent at offsets0/4, not separated by36 bytes. Large payloads are checked in
bounded chunks, including late headers. Both compatible layouts are retained
when channel ownership is ambiguous. All-zero data is explicitly marked.

Candidate payload hashes are compared with known bank content, independently
of their CRC/path. Identical bytes are recorded as content matches; channel
ownership, resource identity and playback use still require evidence. Compressed
entries stay explicitly unsurveyed. Raw PCM, malformed/different encodings and
audio embedded inside larger resources are not ruled out. `complete:false`
remains mandatory; this tool is not an audio/non-audio classifier.

## Current result

All403 archive hashes match the census. Of50,883 other-archive entries,11,115
known-CRC audio copies again match their complete known hashes. All39,768
remaining entries are uncompressed and surveyed. There are no detected container
signature candidates and no identical-content matches to the known banks.

Only two entries pass the raw ADPCM header filter. They are identical144-byte
payloads, CRC110A01D0, in `arcademem/l_25_arm.pak` and
`challengemem/l_70_chm.pak`. Their content hash is
`1e0f157d7e76ad3f0b972cc00ca48c2645bf7429cd473b0ed2e917f93c748622`.
Both mono and stereo header layouts fit. The payload consists largely of small
integers/zeroes and repeatedFFFFFFFF words. That is compatible with structured
configuration data as well as passing this permissive header filter; no audio
identity, channel count, duration or restoration is assigned.

This narrows standalone-format review to one distinct compatible payload. It
does not rename or remove the39,768 unidentified entries from the denominator.
The private survey retains every candidate's archive/index/offset/hash and all
limits. The initial known-bank census remains unchanged.
The actual command produces identical JSON on Windows and the independent
Linux/WSL checkout against the same original data. The full media suites pass
228 on Windows (real endpoint check enabled) and224 on Linux with four
Windows-specific skips, including all eight new survey regressions.

## Loader coverage remains open

Current direct-call analysis finds buffer creation at title sites27AA4/27B4F
through SDK4093C8, stream creation at2993E and388DA0 through40967C, and the
XMV library's4451EF stream call. The388DA0 caller's original descriptor specifies
PCM tag1, mono8000Hz/16-bit, and callbacks. Its asset/provider identity has not
been established. Do not label it as voice chat or associate it with the144-byte
candidate merely because the format or size seems suggestive. Static direct
xrefs do not establish that indirect calls or all runtime providers are covered.
No microphone or network path is activated by this investigation.

The eight synthetic regressions cover complete/late/stereo headers, truncation,
ambiguous zero data, bounded signatures, immutable sources, unknown CRC content
matches, known-CRC content mismatch, changed archives, unsupported compression
and escaping paths. Both media CI routes select them. They prove the survey's
contract, not resource identity. See the [acceptance ledger](audio-overhaul-acceptance.md).
