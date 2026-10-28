# Positional stereo audio

Audio Options exposes **Enhanced Audio (Beta)** in the shared frontend/pause
builder. It defaults OFF; one switch controls stereo projection, the gunfire
range wrapper, environmental routing and the background ambience mix.
Changes apply live and save immediately, independently of the original
volume-slider Accept/Cancel draft.

The launcher exposes the same **Enhanced Audio (Beta)** preference under
Settings -> Audio. Shared C# `GameSettings`/`SettingsPage` code serves both
hosts and reads/writes the same sidecar. Reopen launcher Settings to reflect
in-game changes; an already-open draft rejects an external edit rather than
overwriting it. Launcher Save retains its normal close-game requirement.

`src/interface/pc_audio_settings.h` defines the shared setting and its HLE-owned
implementation. It saves `TSFP_AUDIO_V1 0|1` atomically to `pc-audio.ini` beside
`TSFP_DISPLAY_CONFIG` (or `RECOMP_INPUT_CONFIG`; default `config/`). The atomic
live value is published only after a successful save. A saved choice takes
precedence over diagnostic `RECOMP_ENHANCED_AUDIO`/`RECOMP_ACOUSTICS` overrides.
OFF restores ordinary routing and clears stream filtering; future gunfire
parameters retain the original range. Already queued PCM drains normally, so
SDL changes retain its documented queue delay rather than flushing sound.

The menu inserts a native OFF/ON choice before the hidden subtitle record,
retaining that record for the original menu save logic. It inherits Audio
Options' explicit row height and scales the long beta caption to 75% to avoid
overlap. Failure to save restores the selector's previous value.

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
Native Mac listening and queue-latency acceptance remain pending (MAC-119).

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
