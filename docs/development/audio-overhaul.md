# Audio overhaul

Goal started 2026-11-07. Work lives on `feature/audio-overhaul`, based on
`a24c3b60` (published game 0.5.3 baseline). Preserve original fallback, source
assets, settings and saves. Implement common policy once for Windows/macOS/Linux.
Merging and publication are outside this task's authorization.

## Sequence and acceptance

1. **Audit (census complete; reference listening pending).** Hash/index every game archive and movie without
   extracting another data copy. Identify sound-bank metadata and establish
   payload/codec mappings. Inventory rates, channels, durations and loop points
   only after verifying the format. Measure decoded peaks, loudness and seams;
   establish representative Original/Enhanced reference captures. Keep unknown
   and untested assets in the denominator.
2. **Playback repairs.** Establish the existing media-suite baseline. Investigate
   remaining high-refresh underrun counters, decoder accuracy, loop/tail handling,
   pause/skip and music/movie timing. Change only verified defects with regression
   evidence; retain guest completion clocks and bounded queues.
3. **Mix controls (implemented; game/listening acceptance pending).** Separate enhancements currently behind one beta switch;
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
The all-bank pass completed successfully: 27,253 payload/base-rate records,
14,110 distinct content/channel/rate combinations. There are 26,266 finite
integrated-loudness results and 987 below-gate/short records. The unique-source
median is -10.81 LUFS; finite values span -32 to -1.81 LUFS. Music's finite
median is -9.145 LUFS, spanning -15.1 to -6.13 LUFS; cinematic recordings are
included in this music pool. The highest measured source true peak is +6.03 dBTP.
These are source measurements before original event levels, music/effects
sliders, distance attenuation and the existing -6 dB output gain. They support
headroom/mix investigation, not global normalization or a claim of in-game
clipping. The decoded-sample and loudness inventories deliberately use different
deduplication keys: the latter includes base frequency.

FFmpeg's IMA rounding differs slightly from the production decoder: synthetic
four-block comparisons gave equal sample counts/channel order but up to 5 mono
or 8 stereo integer sample units of difference. Both use 64-frame Xbox blocks.
This does not establish a port defect: the production shift/add arithmetic
matches the pinned runtime's LLE arithmetic. Keep the production decoder intact
pending hardware/reference evidence; FFmpeg metering is not bit-exact proof.

## Independent enhancement controls (implemented; acceptance pending)

The shared preference now represents the master switch plus Stereo positioning,
Extended gunfire range, Environmental filtering and Background ambience.
V1 choices load with all four individual options selected, preserving existing
ON/OFF behavior. Custom choices serialize as
`TSFP_AUDIO_V2 <master> <spatial> <range> <environment> <ambience>`; all-default
children retain the legacy V1 representation. Values are strict 0/1 tokens.
An unknown/malformed file is preserved and cannot be overwritten by an in-game
save. Atomic publication follows successful file replacement; a competing save
is refused rather than spinning the reader. Master OFF preserves child choices.

Both the shared launcher page and native frontend/pause builder expose the
four choices. Launcher draft/conflict/transaction handling is retained; the
game saves live and restores selectors after failure. Native added choices use
32-unit row extents and scaled captions; actual in-game layout review is pending.
Windows offscreen Audio-tab rendering passed visual inspection at 1152x772.
Old game builds do not understand custom V2 preferences; the UI explicitly
requires an updated build. Publication/installation is not part of this step.

Shared patch0059 gates stereo, environmental routing and gunfire separately.
The ambience poll clears only non-positioned bed targets, leaving independent
positioned filters intact. Diffuse background mixing works without room geometry
and in split-screen; enclosure filtering remains neutral without one verified
listener. This is not multi-listener positional acoustics acceptance.

All32 combinations passed C disk round trips/master retention and production
C# serializer/Load round trips. Combined media/menu/portable launcher checks
passed170 tests,9 skips and31 subtests. Full Windows launcher build/tests then
passed in42.76 seconds, including41 shared UI groups and49 settings checks.
Earlier attempts stopped at the existing running-game guard; they were rerun
after that external session ended. No other game was closed or modified.
Native game build, physical Mac/Linux and listening checks remain pending.
Per-category gain sliders, ducking and
dynamic-range controls are still in progress, not supplied by these switches.

## Source ownership and mix controls (implemented; acceptance pending)

Patch0060 establishes source ownership before initial PCM submission. The
supported US executable's script-speech callers return to1643C2/164D20 from
46D80, independently of the subtitle preference check. Actor speech returns to
262700 from45820 and stores its opaque play handle at actor+E8C. The common
46D80 helper's nested45820 call inherits the outer speech scope. These contexts
have zero delay (45390 initialization); no deferred ownership is inferred.
Manual wrappers preserve original calls and register/stack behavior. Their
definitions and `_gen` declarations remain explicit in recomp_manual.c because
the compiler's source scanner does not recursively scan included `.inc` files.
Bounded TLS scopes require no host callback guest-memory access and discard
stale entries after guest stack unwinds. No subtitle setting is changed.

US299C0 starts sources on pre-created streams and calls SetFormat before their
later packet pumping. The music/cinematic record64BD98 takes Music precedence;
the ten effect records start5836D0 with stride14094. Established speech uses
Dialogue; the already verified looping-stereo flags210 select Ambience;
ordinary/unknown sources fall back to Effects. This does not isolate speech
baked into cinematic music. BufferPlay assigns new-source ownership before
the first submit; continuing playback retains it. Same-format stream reuse
retags at source start; unscoped frequency/format adjustments retain ownership.
Release/reallocation resets ownership. Scope/routing tests exercise actual
manual wrappers and real buffer/stream code, not executed game dialogue.

Patch0061 composes existing event/profile/headroom/distance levels with six
independent 0..100% settings: master, effects, music, dialogue, ambience, movies.
100% leaves the original level exactly unchanged. Master and category multiply
once (50% = -602 hundredths dB; 50% ×50% = -1204); zero maps to the existing
mute floor. The existing host output gain remains in place. Buffer/stream
pumps and the common XMV path update queued voices through the host volume
API, without rewriting PCM, queue counts or completion clocks.

`pc_audio_mix_settings.h` owns `pc-audio-mix.ini`, a separate sidecar so existing
enhancement V1/V2 preferences keep their format. Its strict ASCII form is
`TSFP_AUDIO_MIX_V1 <master> <effects> <music> <dialogue> <ambience> <movies>`.
Values are decimal0..100, without signs/leading zeros. Native reads are bounded
to254 bytes; malformed, embedded-NUL, future and unreadable files refuse saves.
Failed writes retain the live snapshot. Successful replacement publishes one
atomic six-field snapshot. Both audio sidecars resolve next to the isolated
display/input config via `pc_audio_config_path.h`.

The shared launcher exposes six sliders with draft/conflict/transaction
handling. The mix sidecar is the eighth tracked settings file; a failure there
rolls back earlier files. Offscreen Windows rendering is readable at1152x772
(`private/audio-overhaul/launcher-mix.png`). Native `pc_audio_menu.inc` replaces
the five appended enhancement rows with one Audio Mix link; its subpages hold
six exact-percent selectors and five enhancement toggles, respectively. This
retains the original profile controls and Accept/Cancel records. Independent
preferences save live; failed saves restore choices. Keyboard/controller/mouse
rendering, narrow aspect ratios, frontend/pause and split-screen need actual
game acceptance. Native volume selectors use arrows rather than a drawn bar.

Verification:146 media tests passed with5 explicit skips before the native
subpage addition; its production-code fixture subsequently passed. Real XMV
controlled-clock and Windows-timer tests passed all4 cases using the supported
game copy and pinned FFmpeg, preserving image/PCM hashes and cadence. The full
Windows launcher suite passed in40.76s with43 shared UI groups; malformed bytes,
external edits, exact category order and final-file rollback are covered. The
full production menu/FOV fixture passed2 checks (4 unrelated/data skips), and
56 portable launcher/menu checks passed with1 skip. These checks do not establish
audible fidelity, game source-ownership coverage or physical Mac/Linux acceptance.
Ducking, dynamic-range presets, further spatial work and selective restoration
decisions remain in the active goal.
The final combined media/full-menu/mouse/portable-launcher/context pass reports
226 passed,10 skipped (19.41s). The full Windows launcher suite was repeated
after the compact volume layout and passed in42.63s. Both exited successfully.

## Listening controls (implemented; tuning/acceptance pending)

Common patch2307 adds opt-in Dialogue focus and Original/Reduced/Night impact
range. MIX_V2 appends `<ducking0/1> <impact0..2>` to the six V1 percentages.
V1 loads both options neutral; saving neutral choices retains V1. Malformed or
future files remain protected. One atomic options snapshot carries every field.
The shared launcher and guest Listening subpage expose the same choices without
depending on the Enhanced Audio switch. Shared UI tests cover all6 combinations,
unrelated-save retention, V1 recovery and invalid values. Old builds preserve
an unreadable V2 rather than overwrite it; these options require this overhaul.

Dialogue focus samples accepted non-silent speech owned by the verified caller
paths. Paused/flushing, muted, drained or missing output does not trigger it.
The new host `audible_pending` query reads available SDL output without flushing
resampler input; XAudio uses its existing queued-voice state. This query never
selects a completion clock. A nonblocking guard limits sampling to once per5ms;
buffer and stream locks are held separately. Existing queue pumping occurs
before sampling. A20ms attack and300ms release approach6dB attenuation of Music,
Effects and Ambience; Dialogue and Movies are exempt. Device/queue availability
controls release even if the guest has not polled an old speech handle. Focus
OFF bypasses the additional level change immediately; release reaches exact0.
Speech already baked into a music/movie premix cannot trigger separate focus.

`pc_audio_dynamics.h` supplies a linked-stereo peak envelope once in shared HLE
code, before existing host acoustics. Reduced uses a0.35 linear threshold and
2:1 slope with120ms release; Night uses0.125,4:1 and250ms. These are initial
audition values, not perceptually accepted presets. The detector includes the
authored/current HLE level so already quiet sources are not compressed again.
There is no makeup, boost, lookahead, resampling or clock/queue change. Separate
Dialogue is exempt; effects, ambience and music/movie premixes are processed.
This is per-source impact reduction, not a limiter on the final sum of voices.
Original preserves exact bytes; fresh below-threshold input passes unchanged.
Speech activity is source-level, not VAD tracking within pauses in a recording.
Any option change affects newly
submitted PCM; existing queued processed PCM drains under the normal lead.

Buffers/streams retain small envelope state and restore it after a refused
submit. Existing source/reset/pause/format boundaries clear state. XMV copies
one bounded chunk before processing, retaining immutable pending PCM for retry.
The fixtures verify partition invariance, linked channels, signed16/unsigned8,
exact Original, authored attenuation, no PCM/source mutation, mute/pause/drain
duck release, resampler-held-input exclusion and rejected-submit rollback.
Windows media/full-menu pass154 with9 skips; the full launcher suite passes
44 shared UI groups in41.69s. Real XMV tests pass4 in40.32s, preserving Original
PCM/image hashes and cadence while also exercising synthetic movie DSP retries.
Game build, rendered native pages, listening/tuning and physical Mac/Linux output
remain unverified. No source audio asset has been rewritten.

## Process output capture

`tools/testing/capture-game-audio.cpp` is Windows developer instrumentation,
separate from the product audio path. It uses the Windows SDK's
[process-loopback activation](https://learn.microsoft.com/en-us/windows/win32/api/mmdeviceapi/nf-mmdeviceapi-activateaudiointerfaceasync)
with an explicit target PID and INCLUDE_TARGET_PROCESS_TREE. It has no endpoint
or microphone fallback. An agile completion object retains activation parameters
through asynchronous completion, including the bounded activation wait.

In an x64 MSVC developer shell, compile with
`cl /nologo /std:c++17 /EHsc tools/testing/capture-game-audio.cpp
/Fe:build/audio-tools/capture-game-audio.exe`. Run
`capture-game-audio <owned-game-PID> <new-private-output.wav> <seconds>`.
The allowed duration is1..120 seconds. Existing files are never overwritten;
invalid arguments and file preflight run before audio activation. The utility
writes stereo48kHz signed16 PCM WAV and reports frames, silent frames, packet
discontinuities, timestamp errors, elapsed time and early target exit. A parent
bounded runner must still check both game and recorder outcomes. A valid WAV
alone does not establish the correct scene, timing, audible fidelity or stability.

`test_audio_output_gain.py` compiles the actual utility on Windows and checks
argument bounds, preservation of existing evidence and preflight. It does not
activate audio in CI. Actual private menu captures succeeded at6/4/6-second
limits, with zero discontinuity/timestamp-error flags. FFprobe and Python wave
readers confirm the headers, PCM type and counts. These are game-only output
captures, not hardware headphone/speaker recordings or Xbox reference audio.
Mac/Linux need native capture and physical listening acceptance on those hosts.

## First Windows build and frontend checks

Generation completed:15,838/15,863 functions translated, zero failed,
6,595,934 lines. The three new wrapper `_gen` declarations are present. The
omitted/manual functions and existing unresolved stubs are not audio acceptance.
The isolated checkout uses a small compiler-input XBE and local derived caches;
the single complete game-data copy remains elsewhere. Runtime tests explicitly
use that complete data folder and a verified disposable Test Lab fixture.

The first normal compile stopped with C1002/C1060 heap exhaustion and an
MSBuild OutOfMemoryException; even a concurrent small settings fixture could
not compile its Windows headers. The x64 compiler/host were already selected.
The retry kept the same Release code/optimization and used
`cmake --build build/windows --config Release --target futureperfect_recomp
--parallel 1 -- /p:CL_MPCount=1 /p:UseMultiToolTask=true
/p:EnforceProcessCountAcrossBuilds=true`. One compiler process was observed at
about3.86GB working set, with system commit20.3GB of44.2GB. The previously
failing units then compiled and the game linked. The standard
`tsfp.ps1 -Action Build` subsequently passed its full build checks with
`CL_MPCount=1`, including matching crash symbols. No check or optimization was
removed, and no other checkout's process was stopped.

The first actual640x480 audio page exposed an oversized link and literal75
text that host fixtures had not caught. Unsupported `\vs75` text was replaced
with plain captions. Original profile rows use native absolute6/7.5-unit
metrics; inheriting that incoming value keeps the link visible without pushing
Accept/Cancel offscreen. The custom pages use automatic relative rows and the
existing descriptor-scoped0.65 glyph-size wrapper during measuring and drawing.
Native profile/Mix/Enhancements/Listening pages were subsequently inspected;
the keyboard changed and saved Music0, Dialogue focusON, impactReduced and the
enhancement master independently. Source font/geometry fixtures pass30 checks
with2 unrelated skips. Frontend coverage is not pause, split-screen, ultrawide,
mouse or full controller acceptance.

In the menu test, changing only Music from100 to0 lowered captured RMS from
-47.47 to-96.71dBFS, near the signed16 capture floor. The corresponding private
clips are `menu-pages-fixed-02/mix-before.wav` and `music-after.wav`. This confirms
runtime music ownership/gain; queued-update contracts are covered by timing
fixtures. It does not prove
every dialogue caller or final combat headroom. All menu sessions stayed live
through their bounded deadlines and reported zero XAudio engine glitches.
They were intentionally terminated by their owning runner (exit1), not claimed
to have exited naturally. Startup voice-starvation counters remain evidence to
investigate in combat; they are not automatically classified as audible loss.
The runner recycles only its six known disposable disk images and preserves
the test fixture, settings and selected evidence. No personal save was used.

## Planning estimate

Initial estimate, before codec/mapping and listening audit: 2–4 active hours for
the audit; 3–5 working days to an integrated Windows build; 1–2 weeks for the
full overhaul and cross-platform acceptance. These are cumulative estimates,
not deadlines or a claim of continuous background execution. Asset restoration
scope, unresolved runtime defects and access to physical devices may change them.
Re-estimate after the audit with measured scope. Track implementation and each
host's acceptance separately.
The completed census does not justify a bulk asset rewrite: proceed with mix
and playback controls, then targeted listening and restoration decisions. The
remaining estimate is2–4 working days to an integrated Windows candidate,
with1–2 weeks for full listening/cross-platform acceptance, conditional on
access to those devices and any defects discovered during gameplay.
