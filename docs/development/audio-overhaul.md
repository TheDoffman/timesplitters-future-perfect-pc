# Audio overhaul

Goal started 2026-11-07. Work lives on `feature/audio-overhaul`, based on
`a24c3b60` (published game 0.5.3 baseline). Preserve original fallback, source
assets, settings and saves. Implement common policy once for Windows/macOS/Linux.
Merging and publication are outside this task's authorization.

## Sequence and acceptance

1. **Audit (in progress).** Hash/index every game archive and movie without
   extracting another data copy. Identify sound-bank metadata and establish
   payload/codec mappings. Inventory rates, channels, durations and loop points
   only after verifying the format. Measure decoded peaks, loudness and seams;
   establish representative Original/Enhanced reference captures. Keep unknown
   and untested assets in the denominator.
2. **Playback repairs.** Establish the existing media-suite baseline. Investigate
   remaining high-refresh underrun counters, decoder accuracy, loop/tail handling,
   pause/skip and music/movie timing. Change only verified defects with regression
   evidence; retain guest completion clocks and bounded queues.
3. **Mix controls.** Separate enhancements currently behind one beta switch;
   provide shared persisted controls and original recovery. Identify trustworthy
   dialogue/effect/ambience/music ownership before adding independent gains,
   optional dialogue ducking and dynamic-range processing. Neutral settings must
   preserve existing PCM and saved volume behavior. Avoid cumulative gain stages.
4. **Spatial/acoustic refinement.** Validate listener/source frames, pan changes,
   distance and occlusion against reference scenes. Tune gunfire range by evidence
   rather than a blanket multiplier. Review ambience individually, room reverb
   and transitions, split-screen policy and SDL routing latency. Evaluate optional
   binaural/surround output after the stereo baseline is accepted.
5. **Selective asset restoration.** Process only catalogued defects, retaining
   originals, loop identity, hashes and reversible per-asset fallback. No global
   synthetic enhancement or sample-rate increase presented as recovered detail.
6. **Integrated validation.** Build; run shared media/settings/recovery tests;
   consolidate campaign/combat/ambience/movie listening at supported frame rates.
   Check headphones/speakers, device changes and physical Windows/macOS/Linux
   behavior. Automated host doubles do not establish listening or device acceptance.

## Initial evidence

- Supported US `default.xbe` SHA-256 verified against the repository pin.
- Existing local data contains 403 PAK archives. Five `sounds_<language>` banks
  each have 5,407 entries; `musicts.pak` has 163 entries. These are container
  counts, not a count of unique audible effects.
- English bank's first entry contains 5,406 filenames and 5,703 variable-length
  event records. Original US27700/27290/350C70/1504F0 establish platform extension
  conversion, localized paths and standard CRC32 lookup. All language-bank
  payloads map without missing or ambiguous CRCs. The English bank has 5,355
  mono buffers and 51 stereo streams. Their original filename extensions do
  not describe PlayStation encoding: the Xbox loader uses ADPCM tag 0x69.
- US27FA0/28528 derive base playback frequency from the original 35-word table
  at 4CA9B8, scaled by 48000/4096. Record measured base frequency rather than
  assume rounded rates from filenames; dynamic pitch/randomization is separate.
  Seven English assets have events at more than one base frequency. Original
  explicit loop subregions and the extra metadata word remain unverified.
- US28890/350C40 select 163 unique music paths through the original 202-entry
  path/rate tables. All 163 CRCs map. Music is stereo: 120 payloads use 32 kHz,
  43 use 44.1 kHz. Some music contains mixed cinematic dialogue/effects, so
  changing that stream's level cannot isolate speech already baked into it.
- Production decoding measured all 27,193 language/music payloads, representing
  14,093 distinct encoded-content/channel combinations. No partial blocks,
  invalid step-index headers or nonzero reserved headers were found. Sample
  peaks, RMS, DC, sample counts, base-rate durations, seam steps and source/PCM
  hashes are in the private inventory. Full-scale samples are common and are
  review evidence, not automatic proof of clipping or grounds for normalization.
- A further 11,115 known audio entries in 74 level archives are complete
  SHA-256-identical copies of bank assets; they need no duplicate decoding.
  Other unknown paths and embedded formats remain unclassified.
- FFmpeg 8.1 probed and decoded all six XMV audio streams: signed 16-bit PCM,
  stereo at 44.1 kHz. The opening logos and attract movie have different authored
  RMS levels. These offline observations do not establish in-game A/V sync.
- The runtime name table used by acoustics is not file-backed at that address
  in the XBE. Offline analysis must resolve its loaded bank origin rather than
  read runtime addresses as executable file offsets.
- Existing audio guide records residual high-refresh combat underrun counters;
  audible loss/root cause remains unproven. See [buffer evidence](../agent-context/audio-buffers.md).
- Rejected category shortcut: event flag 0x2 also occurs on vehicles, guns and
  other effects. It is not a verified dialogue identifier; mix routing must not
  use it as one. Similar caution applies to cinematic stereo recordings.

## First verified playback repair

`pc_acoustic_dsp.h` previously kept a tiny nonzero smoothing state indefinitely
after turning enhancements OFF, so neutral samples continued to be requantized.
The new finite release reaches exact bypass below 1e-5 and preserves remaining
packet bytes, clearing stale reflection/filter history. A byte-preservation
regression fails before the fix and passes afterward. Reflection-only re-enable
also cannot replay previous sound. Both adapters use this same code and retain
rejected-submit rollback. The Windows media suite passes 139 tests with five
explicit skips; native build/listening and physical host acceptance are pending.

## Reproducing the first census

Run `python tools/analysis/audio_audit.py --game <existing-game-folder>
--output <private-report-folder>/inventory.json`. It checks the supported XBE,
validates all archive index extents and hashes archives/movies. It inspects
candidate sound/music banks without decoding, extracting or modifying assets.
Other archives stay explicitly unclassified. Reports contain original names and
stay ignored/private; never commit them with source.

For decoded metrics, build `tools/analysis/audio_decode_probe.c` together with
the prepared runtime's `src/hle/xbox_adpcm.c` as a shared library, including its
`src` directory; add `--decoder-library <library>` to the census command.
`test_audio_measurements.py` builds this bridge with the available host compiler
and verifies signed predictors/deltas, channel interleaving and output bounds.

Run `python tools/analysis/audio_loudness.py --game <existing-game-folder>
--inventory <private-report-folder>/inventory.json --ffmpeg <pinned-ffmpeg>
--scope all --output <private-report-folder>/loudness.json` for gated LUFS,
true peak and loudness range. It meters original input; processed output is
discarded. Short/quiet sounds can legitimately have no integrated LUFS.
The first all-bank pass is running; results are not yet accepted.

FFmpeg's IMA rounding differs slightly from the production decoder: synthetic
four-block comparisons gave equal sample counts/channel order but up to 5 mono
or 8 stereo integer sample units of difference. Both use 64-frame Xbox blocks.
This does not establish a port defect: the production shift/add arithmetic
matches the pinned runtime's LLE arithmetic. Keep the production decoder intact
pending hardware/reference evidence; FFmpeg metering is not bit-exact proof.

## Planning estimate

Initial estimate, before codec/mapping and listening audit: 2–4 active hours for
the audit; 3–5 working days to an integrated Windows build; 1–2 weeks for the
full overhaul and cross-platform acceptance. These are cumulative estimates,
not deadlines or a claim of continuous background execution. Asset restoration
scope, unresolved runtime defects and access to physical devices may change them.
Re-estimate after the audit with measured scope. Track implementation and each
host's acceptance separately.
