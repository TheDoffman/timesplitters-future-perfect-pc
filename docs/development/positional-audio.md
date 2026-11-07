# Positional stereo audio

Audio Options exposes **Enhanced Audio (Beta)** in the shared frontend/pause
builder. It defaults OFF; the master switch gates separate choices for stereo
positioning, extended gunfire range, environmental filtering and background
ambience. Individual choices default ON and persist when the master is OFF.
General buffer, lifecycle, format and device-recovery fixes are always active,
including when this toggle is OFF; see [audio recovery](../agent-context/audio-buffers.md).
Changes apply live and save immediately, independently of the original
volume-slider Accept/Cancel draft.

The launcher exposes the same **Enhanced Audio (Beta)** preference under
Settings -> Audio. Shared C# `GameSettings`/`SettingsPage` serves
Windows/macOS/Linux and reads/writes the same sidecar. Reopen Settings to reflect
in-game changes; an already-open draft rejects an external edit rather than
overwriting it. Launcher Save retains its normal close-game requirement.

`src/interface/pc_audio_settings.h` defines the shared setting and its HLE-owned
implementation. All-default individual choices save `TSFP_AUDIO_V1 0|1`;
custom choices save `TSFP_AUDIO_V2 master spatial range environment ambience`
with strict 0/1 values, atomically to `pc-audio.ini` beside
`TSFP_DISPLAY_CONFIG` (or `RECOMP_INPUT_CONFIG`; default `config/`). The atomic
live value is published only after a successful save. A saved choice takes
precedence over diagnostic `RECOMP_ENHANCED_AUDIO`/`RECOMP_ACOUSTICS` overrides.
OFF restores ordinary routing and clears stream filtering; future gunfire
parameters retain the original range. Already queued PCM drains normally, so
SDL changes retain its documented queue delay rather than flushing sound.

Audio Options uses the same flat top tabs as Video and Controls in the main
menu and pause menu: **Volume**, **Mix**, **Enhancements**, and **Listening**.
Volume retains the original four profile controls and hidden subtitle record;
Accept/Cancel still save/discard the profile draft. Mix provides six percentage
controls, Enhancements the master and four individual choices, and Listening
Dialogue focus and Original/Reduced/Night impact range. These newer controls
still apply and save live; switching tabs preserves the profile draft.
The common compact font, silver dividers, rounded hover highlight, tab sounds,
Left/Right arrows, controller LB/RB and footer hints match the other settings.
Failed sidecar saves restore the previous selector. See the
[shared mix and listening contracts](audio-overhaul.md) and
[menu architecture](../agent-context/audio-options.md) for persistence and validation.

The shared HLE still asks the original XDK 5849 calculator for distance, cone,
mix-bin and headroom attenuation. `0040-dsound-positional-stereo.patch` also
projects its listener-relative source direction into stereo. This replaces
centered playback for spatial effects without changing music/movie stereo,
source PCM formats, byte cursors or packet completion clocks.

The supported US calculator at `0040897B` takes listener `ds+30` and source
parameters. Its disassembly passes listener/source positions at `+8`, stores
distance at `+68`, and uses listener front `+20` and up `+2C`. A read-only live
inspection resolved its selected position helper to `00409D06`: this 2D path
only writes X/Z of the cached relative vector at source `+5C`. Routing therefore
reads the full source/listener positions, not that partial cache. It runs after
the existing guarded calculator call. `src/gameplay/pc_spatial_audio.h` projects the vector onto
up cross front, normalized for orientation and distance. Head-relative mode
uses local X directly; disabled mode restores ordinary routing. Invalid or
coincident vectors center safely. Equal-power gains preserve energy without
amplifying a channel. This is stereo direction, not binaural HRTF or acoustic
occlusion. Original distance/cone falloff remains intact. The experimental
extra fade at the end of each range was removed after listening found distant
effects too quiet. Inside one game unit,
sounds are centered; from one to two units the stereo width increases smoothly.
This keeps the first-person dual-pistol sources (measured at 0.10 units) centered
without changing distant enemy/environment direction.

`0045-weapon-sound-range.patch` wraps the positional parameter builder
`00021FB0` only for the two original weapon-dispatch return addresses
`001B7216` (one-shot) and `001B71D6` (repeating). Its first float argument is
the maximum range, written to sound parameters `+2C`; the original weapon
dispatcher supplies 50, 70 or 200 units. The wrapper triples valid positive
ranges up to 200 and restores the caller's argument afterward. Other sound
callers, pitch, volume and timing are unaffected. This lets gunfire carry
farther without extending footsteps and ambience. It does not repair queue
starvation or other unrelated dropouts.

Buffers and streams retain their own routing state and clear it with their
ordinary lifetime. Only matching voice settings receive each 3D commit.
Windows XAudio2 uses a stereo mastering voice and updates source matrices for
queued sound too. Disabling spatial routing restores the source's captured
default matrix. SDL converts mono submissions to stereo in serialized host
scratch storage and applies the same gains to incoming samples. Its queued
samples retain their previous direction until played; buffer lead defaults to
40 ms, streams may hold longer. No stream is recreated when direction changes.
Native Mac listening and queue-latency acceptance remain pending (MAC-120).

Validation: `tests/media/test_audio_output_gain.py` executes the production
Windows routing functions against a silent source voice and the full SDL
adapter against a fake device, including actual PCM channel isolation. It
checks rotation, head-relative/disabled modes, invalid vectors, resets and
ordinary music/movie gain. `tests/media/test_audio_timing.py` checks the real
buffer/stream pumps for refusals, clock fallback, pause, flush, event completion,
headroom and voice reuse. These tests are already in the Windows/macOS shared
workflow. Host-device listening is a separate acceptance check.


## Experimental environmental acoustics

Patch `0050-environmental-acoustics.patch` and `pc_acoustic_scene.h` add an
opt-in beta, controlled by the saved Audio Options toggle; it is not a released
default. The original world ray `00088950` uses its static/door
query branch (`000A3500`), excluding the actor-query branch. It runs synchronously
on the guest commit thread, outside the output locks; the device callback never
executes translated code. The adapter preserves general, SIMD and x87 registers.
Room IDs come from the compact tree at `00702E00` (original `00157320`); bounds
are room-table `00708788`, stride B4, offsets 10/1C. Actual portal polygons use
`0070878C` and inclusive maximum `007B9754`; vertices begin at portal+18.
The bounded centre-portal search checks each leg against geometry. A closed
route stays muffled; an open indirect route steers sound toward its first
opening. Outdoor sources above the room bounds are tested from reachable
openings instead of automatically bypassing occlusion. Per-thread cached queries
run at most twice per 10 ms, with 100 ms refresh and at most 96 route rays.

Live Vietnam inspection exposed an essential coordinate boundary: the XDK
listener is at the origin and its sources are already listener-relative. They
cannot be ray-traced as map positions. Original `00028367` stores each positional
buffer's world XYZ at sound-record+8, linked through `005836C0`; record+0 is the
buffer interface, +30 the next link. `00026D20` subtracts camera+27C and rotates
by the camera's view matrix at +208; `00026EF1` reverses view Z for DirectSound.
The new adapter reads those original world positions and camera pose, then
converts only the resulting blocked/routed direction back to the XDK frame.
The first prototype used the wrong coordinate frame and is superseded.

Shared `pc_acoustic_dsp.h` applies a smoothed low-pass and at most 9 dB of wall
attenuation, plus a restrained early reflection on indirect routes. Both host
adapters consume the same PCM filter without changing source byte clocks. It
is not a full material model, tunnel reverberation solver or binaural renderer.
Disabled processing is byte-identical. Unknown sources and invalid geometry
retain the approved stereo behavior. The prototype resolves positional buffer and effect-stream world records for
one local player. Original `00029F68` and `00029DD0` store stream world XYZ at
record+6C, guarded by +50/+64 and active state +28; +4 is its stream object.
Split-screen and unknown sources retain their existing behavior.

Vietnam's remaining attached noise was traced to non-positional stream sound
28. The supported US bank metadata identifies it as `SFX_VIETNAM_WAR_AMB_STEREO`,
backed by `l48_vietnam_ambience22.MIB`; this is a mixed stereo ambience recording,
not separate positioned battle events. The ambience policy now detects non-positional, looping stereo effect streams
from their sound-bank flags (210 mask), across every map without sound-ID lists.
The title's dedicated music pool and non-looped dialogue are excluded; effects
with world emitters retain positional routing. Original stream selection at
00028450 derives stereo/3D capability from metadata bit 200; playback uses bit
10 for looping. Bounded asset names distinguish indoor/cave/sewer beds from
outdoor ambience and wind. Unknown bed names receive the background mix without
invented outdoor occlusion. The ambience
poll runs outside output locks every 150 ms, including non-3D streams which
never receive a 3D commit. It checks overhead geometry in five directions and
nearby visible, open-to-sky portals. All detected beds receive a smoothed 6 dB background reduction, a 4 kHz
high-frequency roll-off and linked stereo peak control above -14 dBFS (3:1,
120 ms release). This reduces the near/dry impact of explosions baked into the
recording while preserving its stereo balance. Outdoor beds also receive
stronger low-pass/attenuation beneath cover,
becoming clearer near a visible opening. Its authored stereo direction remains
unchanged: individual events baked into that recording cannot be independently
routed. There is no invented point source for a diffuse ambience bed.

SDL refuses a packet without consuming its filter history; reset clears it.
Existing gunfire range and near-field centering are unaffected.

Audio overhaul correction (2026-11-07, local): when targets are disabled,
the shared PCM filter clears its fading state below 1e-5 and leaves the remaining
packet bytes untouched. This ends the otherwise asymptotic processing and
requantization after an OFF transition. Bypass also clears reflection-only
delay history so a later ON cannot replay stale sound. The existing adapter
backups still restore history on refused submission. A new regression failed
on changed PCM bytes before the correction and passes afterward at 8 kHz
mono/8-bit and 48 kHz stereo/16-bit. The Windows build and native frontend pages
now pass; acoustic listening and native host acceptance remain pending in
[the overhaul plan](audio-overhaul.md).

The 29 media contracts plus the guest-menu fixture include wall/opening direction, closed-route behavior,
outside-room sources, filter frequency response, original ray ABI/register
preservation, compact-tree lookup, world-record lookup and the XDK Z conversion.
Windows Release compiled. User listening confirmed the corrected helicopter
roof/entrance behavior; remaining war-ambience listening is pending in the new
isolated build. Mac hardware remains unverified.

Beta toggle validation: Windows Release builds; 30 selected tests pass and two
native Mac compile checks are skipped on Windows. Tests cover default OFF,
restart persistence, saved-OFF precedence, live updates, failed-save isolation,
whole-feature range gating, row-height inheritance and callback register safety.
User testing confirms the visible selector and choice persistence; the isolated
file contains `TSFP_AUDIO_V1 1`. Native Mac UI/listening remains pending.

Launcher integration: `tools/build/build-launcher.ps1 -Test` passed on Windows,
including the three new shared UI cases and the existing updater/settings
checks. Portable launcher/shared-UI/Mac-adapter checks passed 31 tests and
26 subtests. These are not native Mac launcher execution or listening results.

## Acoustic cache correction

The overhaul's moving-emitter regression failed against the prior guest adapter:
a fully blocked ray retained its old direction until the next geometry query.
`pc_acoustic_cache.h` now separates cached geometry from current direction.
Blocked sources follow current world emitter/listener positions; indirect routes
retain their first opening as a world anchor and subtract the current listener.
The guest adapter then applies the current camera rotation and XDK Z conversion.
Clear paths retain the original guest direction. Source/listener room changes
invalidate cached obstruction immediately, including when no query budget remains;
the existing PCM filter still smooths target changes.
Scene identity compares both pointers and counts separately instead of XOR.

The budget remains two geometry queries per10ms. Refresh requests become due
after100ms and run in oldest-request order, so a stable voice iteration order
cannot permanently starve later voices. Requests absent for100ms cannot hold
the queue. Backlogged results may be reused for at most250ms; older or invalid
results fail open to the authored direction without obstruction. Hash collisions
probe for an entry before evicting the least
recently requested voice when all256 entries are occupied. This preserves
pending tickets for voices whose settings share low address bits. Geometry remains
an approximation: door movement and within-room geometry changes still await a
refresh, and an overloaded scene may temporarily fall back. It does not provide
per-sample ray tracing or a unique generation identity for reused guest objects.
Split-screen continues to bypass one-player geometry rather than guessing a
shared listener. Shared source serves Windows/macOS/Linux.

The real-HLE guest fixture proves moving emitter/listener pan and preserves its
ray ABI/room-tree checks. Deterministic scene fixtures cover doorway anchors,
maximum age, scene XOR collisions, room changes, slot reuse and inactive queue
entries. A64-voice fixed-order/hash-collision test checks the two-query budget and at least
three refreshes for every voice in1.2s. The focused timing/output suites pass123;
game/build and physical headphone/speaker acceptance are recorded separately.

The standard Windows Build/crash-symbol checks and219 media checks pass (one
opt-in device skip). GCC13.3/Clang18.1 also compile/run this scene/cache/DSP
fixture with warnings as errors in Ubuntu24.04/WSL, from isolated native-filesystem
copies. A bounded enhanced Vietnam bot match reaches gameplay with zero host
glitches/output failures and stable startup/loading counters. Its two process
captures have no packet flags/full-scale samples. This does not measure a
sustained frame rate or establish listening/device acceptance; see
[measured status](../project/STATUS.md#2026-11-07---spatial-cache-audit-and-runtime-check).

## Distance falloff audit

The overhaul's [native distance audit](audio-distance-audit.md) verifies the
live four-point curve and original distance routine across6643 cases. It retains
the current optional range extension; no additional fade or per-weapon factor
is supported by the evidence. The table in that audit distinguishes distance
attenuation from final output level and records remaining listening limits.

## Host format and optional multichannel evaluation

A read-only Windows query on2026-11-07 reports44100Hz, eight channels,
32-bit floating-point mix format and mask0x63F for each default render role.
IAudioClient3 reports441-frame default/minimum/maximum shared-engine periods
(10ms); GetDevicePeriod separately reports10ms default/3ms minimum. These are
OS capabilities, not measured game latency, physical7.1 acceptance or proof of
the user's headphone/per-app route. The query never initializes/starts a client,
submits samples, changes a device/volume or alters policy. Private receipts
retain source/executable hashes and results.

The production XAudio adapter explicitly creates a two-input-channel mastering
voice and applies source matrices to two destinations. SDL constructs stereo
input for positioned sources before its device conversion. Consequently the
current shared positioning policy supplies a stereo mix, not independently
positioned surround channels. Do not infer native surround or HRTF from an
eight-channel OS format. Microsoft's [mastering-voice API](https://learn.microsoft.com/en-us/windows/win32/api/xaudio2/nf-xaudio2-ixaudio2-createmasteringvoice)
distinguishes input channels from automatic speaker-configuration detection and
describes default-device virtualization. Its [XAudio2.9 guide](https://learn.microsoft.com/en-us/windows/win32/xaudio2/xaudio2-redistributable)
also documents conditional Windows spatial processing; its state has not been
inspected or changed here.

Optional multichannel work would need a shared source-direction/channel-map
contract and matching XAudio/SDL adapters, including original matrix restoration,
device changes, stereo content and split-screen fallback. A binaural path would
add directional filtering and its own reference/listening validation; a pan
value alone cannot recover front/back/elevation. Simply increasing master
channels would leave the current two-destination matrices inconsistent and
would not establish positional surround. Keep the current stereo baseline
until its listening checks complete; no surround/HRTF support is claimed or
enabled by this evaluation.
