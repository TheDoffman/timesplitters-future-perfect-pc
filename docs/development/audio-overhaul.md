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
- English bank's first entry begins with a bounded 5,406-name list (5,355 `.vag`,
  51 `.mib`). Trailing metadata and mapping to payload CRCs remain unverified.
  Filename extensions alone do not establish Xbox audio encoding or sample rate.
- The runtime name table used by acoustics is not file-backed at that address
  in the XBE. Offline analysis must resolve its loaded bank origin rather than
  read runtime addresses as executable file offsets.
- Existing audio guide records residual high-refresh combat underrun counters;
  audible loss/root cause remains unproven. See [buffer evidence](../agent-context/audio-buffers.md).

## Reproducing the first census

Run `python tools/analysis/audio_audit.py --game <existing-game-folder>
--output <private-report-folder>/inventory.json`. It checks the supported XBE,
validates all archive index extents and hashes archives/movies. It inspects
candidate sound/music banks without decoding, extracting or modifying assets.
Other archives stay explicitly unclassified. Reports contain original names and
stay ignored/private; never commit them with source.

## Planning estimate

Initial estimate, before codec/mapping and listening audit: 2–4 active hours for
the audit; 3–5 working days to an integrated Windows build; 1–2 weeks for the
full overhaul and cross-platform acceptance. These are cumulative estimates,
not deadlines or a claim of continuous background execution. Asset restoration
scope, unresolved runtime defects and access to physical devices may change them.
Re-estimate after the audit with measured scope. Track implementation and each
host's acceptance separately.
