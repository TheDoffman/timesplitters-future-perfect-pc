# Audio overhaul acceptance ledger

This is the current gate list for the full objective. Older chronological notes
in the [overhaul plan](audio-overhaul.md) record milestones and may describe
states superseded later. A passing implementation/contract check does not imply
listening or hardware acceptance. The overall goal remains open.

## Ordered requirements

| Plan requirement | Authoritative evidence and scope | Current state / remaining work |
| --- | --- | --- |
| Audit archives, metadata, codecs, rates, channels and duplicates | Private schema2 inventory:403 archive/six movie hashes; all five known language banks and163 music entries mapped through original loaders. Production metrics for27,193 payloads,14,093 unique content/channel layouts;11,115 copies match. The [standalone survey](audio-coverage-survey.md) checks all39,768 other entries and finds two identical compatible payloads without assigning audio identity. | Known-bank census verified. Other entries remain unidentified and `complete:false` is retained. Embedded/raw PCM/other formats and one callback-stream provider remain open. |
| Loudness, peaks, loops and reference captures | Private loudness report27,253 base-rate rows/14,110 unique content/channel/rate inputs; original authored-loop lookup/rate conversion; controlled crane previews; owned campaign/combat/movie recordings. | Measurements/references exist. Source peaks/seams are candidates, not accepted defects; perception/reference listening remains open. |
| Repair verified playback defects | Shared patches2309/2310 repair original buffer layout/ownership and authored exclusive loop ends. `audio_buffer_layout.c`, `audio_buffer_regions.c`, timing/load/recovery fixtures use actual HLE/model/decoder. Original-code probes and game scenes corroborate the contracts. | Verified repairs implemented. Original and enhanced modes both receive correctness fixes. Runtime starvation counters alone do not prove audible failure; one Original match's increase remains for review. |
| Preserve clocks, queues and source bytes | Region fixtures cover fractional schedules, tiny/multiple wraps, one-shots, pause/seek/live updates, rates, refusal/hitch recovery and source immutability; native XMV cadence checks compare image/timestamp/audio hashes. | Automated contracts pass. Physical device changes and subjective lip/music synchronization need acceptance. |
| Separate enhancement choices and Original recovery | Shared `PcAudioOptions`, strict V1/V2 persistence and all32 feature combinations; default enhancement master OFF, children retained. Shared DSP exact bypass, default XAudio matrix restoration and SDL queue behavior are tested. | Implemented. OFF drains already queued PCM and smooths existing filter state; it does not undo unconditional playback repairs. Physical cross-host transitions remain unverified. |
| Independent Master/Effects/Music/Dialogue/Ambience/Movies | Shared packed `PcAudioMixOptions`, source scopes before first submit, queue gains and strict save-before-live; C/C# persistence and native-menu fixtures. Actual Music/Dialogue isolation and movie output/mute checks have separate receipts. | Implemented. Known script/actor caller ownership verified; speech baked into movie/music premixes stays in that bus. No claim of automatic separation of baked speech. |
| Dialogue focus and impact range | Shared activity/envelope logic, exact Original, partition invariance, linked stereo, authored attenuation, pause/drain/mute release and rejected-submit rollback; both adapters tested. | Implemented. Reduced/Night thresholds are initial audition values. Focus uses source-level accepted speech, not VAD within internal pauses. Per-source reduction is not a final summed limiter. Listening acceptance remains open. |
| Spatial direction and near field | Full listener/source coordinates, orientation/head-relative controls, equal-power projection and near-field centering; actual source records and original coordinate transforms; real-HLE moving-source regression. | Verified stereo baseline mechanics. Binaural/surround evaluation follows stereo acceptance under the plan; no HRTF support is claimed. |
| Distance, obstruction, room transitions and ambience | [Native distance audit](audio-distance-audit.md), current curve/optional range decision; bounded/fair acoustic cache, doorway/world anchors, room/scene invalidation; wall/door/sky/bed fixtures and observed continuous room13->6 walk. | Mechanics and representative scene verified. No extra range fade or speculative restoration. Reverb/cover balance and broader room listening remain open. |
| Split-screen policy | Real guest fixture enters0/2/3/4-player mode with a populated cache: authored direction, no one-listener obstruction/enclosure or ray calls; diffuse mix remains independent. | Shared fallback verified. Actual multiple-controller gameplay/listening unverified; no fabricated player-state write is used as acceptance. |
| Selective asset restoration | [Restoration review](audio-restoration-review.md) revalidates five candidates/four unique contents against encoded/PCM hashes and rejects mean subtraction that would worsen boundaries/range. | No replacement justified or selected. Original assets retained. Candidate listening and unknown asset identity remain open. |
| Integrated builds and supported rates | Windows Release/matching crash-symbol checks; Windows media228 pass including endpoint check; Linux224 pass/four platform skips. Movie/load contracts cover original/60/120/144/180/240 polling. Actual default/120 and144/180/240-target runs reach gameplay with process captures. The latter average143.00/177.26/205.39 guest updates/s during the sampled capture windows. | Audio sweep and contracts verified at selected targets. Display-present FPS remains unmeasured; sustained240 performance is not established. Targets and observed guest cadence are recorded separately. |
| Native UI, keyboard/mouse and saved settings | Shared launcher build/tests; offscreen/native4:3 and21:9 menus, pause navigation, selector/save/failed-save checks and isolated replayed keyboard/mouse runs. | Implemented/observed on Windows. Physical controller use and native Mac/Linux UI acceptance remain unverified. |
| Consolidated listening | A14-track,170.80-second private playlist packages current candidate recordings, controlled loop demonstrations and source previews with purpose/source/preset/hash metadata. All eight game-output tracks share the current candidate executable hash. A22-second room-walk clip was presented for user feedback. | Package ready; no answer or perceptual acceptance is assumed. Headphones and speakers remain in scope. |
| Preserve user data, other work and architecture | Focused local Windows worktree and independent Linux validation checkout/runtime; disposable checked Test Lab saves/settings; read-only original game assets; common feature/settings/DSP/model logic and thin host adapters. | Preserved by the executed workflows. No remote push, merge or publication. |

## Host and device evidence

| Host/path | What has actually run | What remains unverified |
| --- | --- | --- |
| Windows XAudio2.9 | Standard game build, owned campaign/combat/room/movie runs, process-only PCM capture; real default endpoint consumes silent PCM, invalidates on an injected callback and reopens successfully. | Physical unplug/default-device change, speaker/headphone judgement and multi-controller play. |
| Linux/WSL shared media | Independent checkout/runtime, GCC/G++13.3, media224 pass/four Windows-specific skips; controlled real-file XMV cadence using system FFmpeg. The new standalone survey produces JSON identical to the Windows run on the same source data. | Native Linux game/UI/performance and hardware listening. |
| Actual pinned SDL3 adapter/converter | Fresh independent Release static SDL build at `fa2c02bb6e21974a89ea9824bc53c9932abe5f9c`;16 dummy-driver cases through production `audio_sdl.c` cover22042/32000/44100/48000Hz, mono/stereo,8/16-bit, resampling tail drain, queued exact mute, source immutability, reset/shutdown. | These are converter/adapter checks with a paused dummy device, not physical playback or latency. The real PulseAudio attempt returned unavailable; `/mnt/wslg/PulseServer` is absent. |
| Native Mac | Common policy/source and portable shared tests; no duplicate Mac mix/DSP/model was added. | Native compilation/execution, launcher/game UI, actual SDL endpoint and physical listening on Mac. See MAC backlog. |

## Next acceptance work

Review the packaged current-candidate Original/listening presets and the
high-refresh recordings, retaining honest target/performance labels. Continue
provider/identity checks where evidence can resolve the remaining scope. Review
audible artifacts and preset balance from those recordings, then
change only a defect or tuning choice supported by that evidence. Retain the
unidentified-source and unavailable-device gaps in all completion claims. Do not
mark the full objective complete from build/test success or an unanswered
listening question.
