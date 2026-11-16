# Private custom audio voices

`mod_audio_player.h` declares trusted native controls; `pc_mod_audio_host.inc`
implements them inside the shared HLE audio unit. Patch2717 includes it only in
game builds and services it under the existing buffer lock. It uses the existing
`RecompDsoundBufferModel` clocks and native audio-output interfaces. It does not
replace original events, buffers, streams, PCM or their clocks.

There are32 fixed custom voices. Their host output slots273..304 follow the
existing movie slot272; original buffer0..255/stream256..271 slots stay unchanged.
One shared header defines the count for XAudio2 and SDL. Voices and80KB scratch
are fixed, while the existing host backends lazily allocate their bounded copied
PCM queues. The player itself performs no allocation, file IO, decoding, guest
memory queries or worker communication. WAV snapshots are loaded before guest
startup and retained until process exit, as described in [audio assets](mod-audio-assets.md).

Play accepts a voice0..31 and a key in the committed bootstrap asset store. It
validates arguments and constructs the complete new voice before resetting an
old one. Unknown/unavailable resources or invalid controls leave existing voices
unchanged. Pitch is Q16 in0.25..4; zero selects the WAV's own authored rate. The
effective2..192kHz output range remains within the existing backend limits.
Volume is -10000..0 hundredths of a dB, followed by the existing effects bus and
master gain. Source PCM16 mono/stereo layout is preserved. Looping is whole-source;
subregions/3D routing and original-event replacement are separate integration work.

Custom voices have independent logical play and accepted-output models, using
the existing gameplay audio clock. Play primes40ms ahead; service runs at most
once per10ms and considers at most32 voices. Each copied submission is capped
at100ms/80KB, even at maximum rate/channels. Tiny source loops are
filled by doubling complete periods, avoiding a copy loop per source frame. A refusal never commits its output
cursor. As with the original buffer pump, a service gap over100ms discards stale
past audio and reanchors to the current logical timeline. Device discontinuity
does the same. These are host-clock semantics, not a promise of a sample-accurate
hardware playback cursor. Original game timing is untouched.

Pause freezes the logical cursor and discards queued output. Resume preserves
position/loop flags without replaying a FROMSTART instruction. Pitch changes
advance the old-rate timeline first, then reanchor output at the new rate;
zero resets to the WAV rate, never a template44000Hz value. Volume changes do
not flush output. Stop destroys only the selected custom output; stop-all clears
all custom voices. One-shot tails can drain after the logical source ends.
Bootstrap invalidation clears voices at the next service tick. The production
runtime also stops custom voices on binding/epoch changes, failure, closing and
shutdown; ordinary runtime shutdown does not initialize mod audio.

These functions are private native entry points. Their caller must enforce
offline match authority, validate resources and preserve callback transaction
rules. The confined VM has no direct native access. The versioned Lua/worker
audio-command route is still pending, so no production audio capability is
advertised and these controls alone do not establish in-game custom playback.

`tests/media/test_mod_audio_player.py` compiles the actual player plus the shared
buffer clock. It mocks only committed resource lookup, serialized host lock/time
and device responses. Exact synthetic mono/stereo PCM, slot/rate limits, refusal,
looping, pause/resume, pitch reset, tails, stale/discontinuous output, invalidation
and the32-voice/10ms budget are checked. Existing output-gain fixtures execute
the actual XAudio2/SDL adapters at slots273/304 and reject305. The production
factory driver separately checks voice reset on epoch loss and shutdown. These
component checks and normal game builds are distinct from hardware audibility,
actual Lua playback, device recovery under load and full release acceptance.

`src/mods/game.cmake` also sets the game-only implementation flag on the separate
`xbox_hle` static-library target. An executable PRIVATE definition alone does not
compile the HLE implementation; the normal Windows game link exposed that gap.
Component HLE fixtures and asset-free standalone builds retain their own flags.

Patch2718 uses the dedicated `TSFP_MOD_AUDIO_HOST` guard on that HLE target,
leaving unrelated `TSFP_MOD_GAME` display-menu branches unchanged. The initial
broad HLE flag exposed a missing menu include; the audio guard avoids expanding
that unrelated compilation path.
