# Audio overhaul acceptance ledger

This is the current gate list for the full objective. Older chronological notes
in the [overhaul plan](audio-overhaul.md) record milestones and may describe
states superseded later. A passing implementation/contract check does not imply
listening or hardware acceptance. The overall goal remains open.

## Ordered requirements

| Plan requirement | Authoritative evidence and scope | Current state / remaining work |
| --- | --- | --- |
| Audit archives, metadata, codecs, rates, channels and duplicates | Private schema2 inventory:403 archive/six movie hashes; all five known language banks and163 music entries mapped through original loaders. Production metrics for27,193 payloads,14,093 unique content/channel layouts;11,115 copies match. The [standalone survey](audio-coverage-survey.md) checks all39,768 other entries and finds two identical compatible payloads without assigning audio identity. | Known-bank census verified. Other entries remain unidentified and `complete:false` is retained. Embedded/raw PCM/other formats and one callback-stream provider remain open. |
| Loudness, peaks, loops and reference captures | Private loudness report27,253 base-rate rows/14,110 unique content/channel/rate inputs; original authored-loop lookup/rate conversion; controlled crane previews; owned campaign/combat/movie recordings. | Measurements/references exist. Source peaks/seams alone are not defects. Reviewed loop/source references are accepted; unreviewed events remain unverified. |
| Repair verified playback defects | Shared patches2309/2310 repair original buffer layout/ownership and authored exclusive loop ends. `audio_buffer_layout.c`, `audio_buffer_regions.c`, timing/load/recovery fixtures use actual HLE/model/decoder. Original-code probes and game scenes corroborate the contracts. A fresh continuous loading-to-match capture covers reports increasing8->12, then stable through126s; a20s unchanged-gain excerpt supports review. | Verified repairs implemented. Original and enhanced modes both receive correctness fixes. Loading-to-match listening is accepted apart from the loading pause; starvation cause/per-voice attribution remains unresolved; counter/report times and loading silence alone do not prove an additional audible defect. |
| Preserve clocks, queues and source bytes | Region fixtures cover fractional schedules, tiny/multiple wraps, one-shots, pause/seek/live updates, rates, refusal/hitch recovery and source immutability; native XMV cadence checks compare image/timestamp/audio hashes. Callback-stream fixtures also check32 reentrantly refilled8000Hz/320-byte packets through normal/manual dispatch with exact PCM, ordered completion and stack restoration. | Automated contracts pass. Callback content identity remains unresolved. Physical device changes and subjective lip/music synchronization need acceptance. |
| Separate enhancement choices and Original recovery | Shared `PcAudioOptions`, strict V1/V2 persistence and all32 feature combinations; default enhancement master OFF, children retained. Shared DSP exact bypass, default XAudio matrix restoration and SDL queue behavior are tested. | Implemented. OFF drains already queued PCM and smooths existing filter state; it does not undo unconditional playback repairs. Physical cross-host transitions remain unverified. |
| Independent Master/Effects/Music/Dialogue/Ambience/Movies | Shared packed `PcAudioMixOptions`, source scopes before first submit, queue gains and strict save-before-live; C/C# persistence and native-menu fixtures. Actual Music/Dialogue isolation and movie output/mute checks have separate receipts. | Implemented. Known script/actor caller ownership verified; speech baked into movie/music premixes stays in that bus. No claim of automatic separation of baked speech. |
| Dialogue focus and impact range | Shared activity/envelope logic, exact Original, partition invariance, linked stereo, authored attenuation, pause/drain/mute release and rejected-submit rollback; both adapters tested. Original/Reduced/Night campaign references now exist; Reduced and Night include Dialogue focus. | Implemented. The user accepts Night as clearer/comfortable and Reduced as clear/comfortable, identifying headphones for the comparison. Retain tuning; broader scenes and speakers remain open. Focus uses accepted source-level speech, not VAD within pauses. Per-source reduction is not a final summed limiter. |
| Spatial direction and near field | Full listener/source coordinates, orientation/head-relative controls, equal-power projection and near-field centering; actual source records and original coordinate transforms; real-HLE moving-source regression. Initial [host/adapter capability review](positional-audio.md#host-format-and-optional-multichannel-evaluation) distinguishes the eight-channel OS format from the stereo mix and records shared multichannel requirements. | Verified stereo mechanics. The user accepts the Enhanced combat reference on headphones as clear positioning without excessive muffling; retain current tuning. Broader room/speaker/device acceptance remains open. Optional binaural/surround support is not implemented or claimed. |
| Distance, obstruction, room transitions and ambience | [Native distance audit](audio-distance-audit.md), current curve/optional range decision; bounded/fair acoustic cache, doorway/world anchors, room/scene invalidation; wall/door/sky/bed fixtures and observed continuous room13->6 walk. | The user accepts the headphone room-crossing reference as smooth with no obvious clicks/gaps. Retain filtering/ambience and the current range decision. Other rooms, speakers and device behavior remain unverified. |
| Split-screen policy | Real guest fixture enters0/2/3/4-player mode with a populated cache: authored direction, no one-listener obstruction/enclosure or ray calls; diffuse mix remains independent. | Shared fallback verified. Actual multiple-controller gameplay/listening unverified; no fabricated player-state write is used as acceptance. |
| Selective asset restoration | [Restoration review](audio-restoration-review.md) revalidates five candidates/four unique contents against encoded/PCM hashes and rejects mean subtraction that would worsen boundaries/range. The user hears no obvious unwanted defect in the four original quarter-gain previews. | No replacement justified or selected; retain originals. This review does not identify unknown archive entries or certify every source/event. |
| Integrated builds and supported rates | Windows Release/matching crash-symbol checks; final Windows coverage230 distinct passes including endpoint check (226 full-suite passes plus four explicit real-file follow-ups); Linux226 pass/four platform skips. Movie/load contracts cover original/60/120/144/180/240 polling. Actual default/120 and144/180/240-target runs reach gameplay with process captures. The latter average143.00/177.26/205.39 guest updates/s during the sampled capture windows. | Audio sweep and current contracts verified at selected targets. Display-present FPS remains unmeasured; sustained240 performance is not established. Targets and observed guest cadence are recorded separately. |
| Native UI, keyboard/mouse and saved settings | Shared launcher build/tests; offscreen/native4:3 and21:9 menus, pause navigation, selector/save/failed-save checks and isolated replayed keyboard/mouse runs. | Implemented/observed on Windows. Physical controller use and native Mac/Linux UI acceptance remain unverified. |
| Consolidated listening | A14-track,170.80-second private playlist packages current candidate recordings, controlled loop demonstrations and source previews with purpose/source/preset/hash metadata. All eight game-output tracks share the current candidate executable hash. A second range playlist adds Reduced; a short sequence preserves all four source-preview bytes. | Positive feedback covers loading, Night/Reduced, headphone combat/room transition, controlled loop and source candidates. The user accepts the original Music Volume full-level reference as comfortable. Native Accept and a fresh-process reload verify persistence in the isolated review profile. Movies gain is unchanged. Speakers are unavailable (only headphones). Physical device/controller/platform limits stay explicit. |
| Preserve user data, other work and architecture | Focused local Windows worktree and independent Linux validation checkout/runtime; disposable checked Test Lab saves/settings; read-only original game assets; common feature/settings/DSP/model logic and thin host adapters. | Preserved by the executed workflows. No remote push, merge or publication. |

## Host and device evidence

| Host/path | What has actually run | What remains unverified |
| --- | --- | --- |
| Windows XAudio2.9 | Standard game build, owned campaign/combat/room/movie runs, process-only PCM capture; real default endpoint consumes silent PCM, invalidates on an injected callback and reopens successfully. | Physical unplug/default-device change, speakers and multi-controller play; representative headphone clips are accepted. |
| Linux/WSL shared media | Independent checkout/runtime, GCC/G++13.3, final media226 pass/four Windows-specific skips; controlled real-file XMV cadence using system FFmpeg. The standalone survey produces JSON identical to the Windows run on the same source data. | Native Linux game/UI/performance and hardware listening. |
| Actual pinned SDL3 adapter/converter | Fresh independent Release static SDL build at `fa2c02bb6e21974a89ea9824bc53c9932abe5f9c`;16 standard dummy-driver cases through production `audio_sdl.c` cover22042/32000/44100/48000Hz, plus four160-frame8000Hz cases. Mono/stereo,8/16-bit, tail drain, queued exact mute, source immutability and reset/shutdown are covered. | These are converter/adapter checks with a paused dummy device, not physical playback or latency. The real PulseAudio attempt returned unavailable; `/mnt/wslg/PulseServer` is absent. |
| Native Mac | Common policy/source and portable shared tests; no duplicate Mac mix/DSP/model was added. | Native compilation/execution, launcher/game UI, actual SDL endpoint and physical listening on Mac. See MAC backlog. |

## Next acceptance work

The user explicitly retains full validation as the completion requirement.
Completed implementation, automated checks and accepted headphone references
stand. The goal is not complete. Required remaining physical acceptance includes:

- Speaker listening for speech/effects balance, positioning and room transitions.
- Actual output-device unplug/reconnect and default-device switching during playback.
- Multiple-controller split-screen gameplay and listening.
- Native Mac build, launcher/game UI, actual SDL output and listening.
- Native Linux game/UI, actual output and listening.

Record each on the actual host/device with revision, configuration and observed
result. A failure should lead to a supported repair and focused revalidation;
passing simulated recovery, WSL tests or headphone clips cannot close these gates.
Only headphones are currently available, so no completion date is established
for the remaining physical validation.

The user accepts the adjusted menu reference as "Comfortable now" on headphones.
The original Music Volume control at full raises the observed native Music stream
from-1737 to-1417 hundredths of dB (+3.2dB). A separate native Accept run returns
to Settings, then a fresh process reloads the same profile without reseeding and
shows full Music Volume with the final-1417 stream level. This is a saved original
music preference, applying to music generally after profile selection, not a
menu-only boost or a change to pre-profile title defaults.

The native-saved file is installed only into the unused isolated review profile,
after every file matches its initial snapshot and while its exclusive launcher
lock is held. The prior file is backed up. Launcher CheckOnly passes afterward
without changing profile hashes. Private persistence/validation receipts retain
both hashes and run provenance. Personal settings, source fixtures, PC mix100,
Movies gain, production code/binary and game-wide defaults are unchanged.

Only headphones are available. The remaining physical speaker/device/controller
and native Mac/Linux checks are unverified; no simulated endpoint or dummy-driver
result substitutes for them. On2026-11-07 the user explicitly chooses "Keep full validation open".
These remain completion gates, rather than deferred follow-ups. Unknown archive formats/provider identity also remain documented limits.

On2026-11-07 the user accepts the headphone room crossing as "Smooth transition;
no obvious clicks or gaps", the controlled corrected crane loop as "Continuous;
no obvious new artifacts", and the four original source-effect previews as
"No obvious unwanted defect". Retain current room processing, loop repair and
original assets. The crane is a pump demonstration, not an observed in-game
crane event; source previews use uniform quarter gain without restoration.

On2026-11-07 the user compared Original/Enhanced combat references on headphones
and reported "Clear positioning without excessive muffling". Retain the current
positioning/filtering for this reference. Different live bot matches do not
isolate individual effects or establish all-source/all-scene acceptance; other room
transitions, speakers and physical device changes remain separate checks.

On2026-11-07 the user compared the20-second Original/Night campaign intro clips
and reported "Clearer speech and comfortable effects" for Night. Retain its
current tuning. Night also enables Dialogue focus, so this is acceptance of
that combined reference, not an isolated compressor comparison. Playback
equipment was subsequently identified as headphones. Reduced was then reviewed
as "Clear and comfortable". Retain both settings; broader scenes and speakers remain unconfirmed.

On2026-11-07 the user reviewed the20-second loading-to-match excerpt and
answered: "Only the loading pause; otherwise clean". This accepts that excerpt's
perceptual result and supports retaining the current buffering. Equipment was
not specified, so it does not establish separate headphone/speaker acceptance,
per-voice counter attribution, the full listening package or device transitions.

Review the packaged current-candidate Original/listening presets and the
high-refresh recordings, retaining honest target/performance labels. Continue
provider/identity checks where evidence can resolve the remaining scope. Review
audible artifacts and preset balance from those recordings, then
change only a defect or tuning choice supported by that evidence. Retain the
unidentified-source and unavailable-device gaps in all completion claims. Do not
mark the full objective complete from build/test success or an unanswered
listening question.
