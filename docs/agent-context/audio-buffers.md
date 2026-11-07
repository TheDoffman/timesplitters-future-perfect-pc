# Effects buffering under load

## Callback-fed PCM acceptance

`test_audio_timing.py` additionally runs `callback-pcm8k` and
`callback-pcm8k-manual` through the production stream adapter. The supported
executable's original388C7D descriptor uses mono8000Hz PCM16;38880F constructs
320-byte packets with an index context. Completion callbacks return with ret12
and can call Process to refill before returning. The synthetic provider retains
two pending packets and completes32 in order, requiring exact output PCM,
unchanged source bytes, status/size before callback, no early completion at the
last unheard frame, restored guest stack and no context-index event signalling.
The second case resolves through the manual dispatch fallback. The actual title
codec/provider is not executed by this fixture and its content identity remains
unresolved; linked XVOICE metadata is insufficient to label it as voice chat.
These checks establish adapter behavior, not hardware playback or latency.

`patches/media/0055-dsound-busy-buffer-recovery.patch` extends shared runtime
`src/hle/hle_dsound.c`; the buffering contract is documented below.
It is independent of native-high-refresh timing and of the music stream path.

`tests/media/test_audio_timing.py` compiles `audio_buffer_load.c` against the
actual patched effects pump. Its 64-voice fake-device cases cover steady
17 ms service, repeated 60 ms service, a 150 ms hitch, refused recovery,
finite tails/expired sounds, fixed lead and legacy zero. The pre-fix pump fails
busy, hitch and refused-recovery cases. The existing Windows/macOS runtime
workflow already selects this suite. The fixture checks immediate initial
submission, bounded queued duration, exact recovery source offset and finite
sound completion. Empty-queue counts include refused submission attempts.

These are deterministic CPU/device-double checks, separate from audible
busy-match and physical Mac acceptance. Buffering cannot cover arbitrary
stalls: the cap remains 90 ms and a longer hitch can still cause a gap.
No new audio thread, guest clock, stream policy or host API is introduced.

## Invalid buffer retirement

`media/0056-dsound-invalid-buffer-retirement.patch` retires known buffers before
unusable-settings exits in `model_for_known`. Previously cleared settings
holders, invalid rate or zero alignment could return NULL while retaining a
slot indefinitely. The normal DoWork walk now releases the host voice and
clears all slot state, including for buffers that are not playing.
Valid playing, stopped and paused buffers retain their slots. A full pool of
valid buffers still refuses another allocation; this is not voice stealing.
Patch 2300 additionally intercepts final guest Release, as described below.

The existing `test_audio_timing.py` CI route compiles `audio_buffer_lifecycle.c`.
It checks four complete 256-slot allocation/invalidation rounds and successful
PCM submission from a recovered slot, counted host resets, idempotent cleanup,
playing-buffer invalidation, null/unreadable settings, bad rates/alignment/size,
and retention of a fully occupied valid pool including paused/stopped buffers.
Five original cases fail before the patch; preservation controls already pass.
Windows fixture results do not establish Mac or busy online listening behavior.

## Format changes and partial source tails

`media/0057-dsound-format-and-tail-recovery.patch` includes guest sample rate
and source alignment in buffer identity, independent of SetFrequency's playback
rate. A changed format rebuilds both clocks and drops the old host voice,
following the existing format-change lifecycle. Raw source size remains the
identity while only complete PCM frames or ADPCM blocks enter the clocks.
Incomplete trailing bytes therefore no longer cause immediate retirement.

Stream SetFormat with the same encoding/channel/bit/alignment layout keeps
packet timeline boundaries. Resetting the host voice then resumes at the exact
unheard sample, including inside an ADPCM block and after pause. SetFrequency
keeps the same behavior. Changes to the byte layout still use the existing
relayout policy; queued encoding/channel conversions are outside this repair.

The existing CI-selected `test_audio_timing.py` adds five buffer format/tail
cases and four stream rate/frequency cases. Seven regression cases fail before
0037; two frequency controls pass before and after. Captured stream PCM must
match the unheard suffix, and completion reports the original source size.
These adapter fixtures mock guest stream format validation and host playback;
they do not establish the title's frequency of use or audible hardware behavior.

## Device and voice recovery

`media/0058-audio-device-recovery.patch` implements bounded XAudio recovery and
shared stream/movie discontinuity handling. `audio_output.h` owns the one-second
retry interval and `recomp_audio_output_take_discontinuity(slot)` notification.
Unsuccessful initial output also marks a discontinuity, so restoration skips
elapsed sound rather than submitting a stale pending prefix. Normal queue
backpressure does not reset a voice. Explicit reset clears the notification;
explicit mute and shutdown never reopen output.

XAudio recreates its engine after critical/device-invalidated errors and closes
all old voices before freeing their PCM. Non-device failures retire only the
affected voice and back off that slot. Public output operations are serialized;
the engine callback only stores an atomic error. COM initialization/cleanup is
thread-local, independent of engine destruction. Reconnect reads the same gain
and mute configuration. SDL default-device migration remains owned by SDL;
`src/native/audio_sdl.c` retries failed initial opens and individual streams
with the same interval and signal, without repeatedly initializing a successful
SDL audio subsystem or destroying unrelated voices.

Streams keep the existing wall-clock fallback while output is absent, reanchor
each refused recovery attempt to that clock, then use the new device cursor
origin once accepted. Movies retain their video clock and discard pending PCM
that is already due during recovery. Effects retain their existing bounded
stale-data catch-up. Playback cannot recover sound already missed during an
outage. No operating-system device settings are changed.

`test_audio_device_recovery.py` compiles complete XAudio/SDL adapters against
failure-injecting host doubles on both CI hosts. `test_audio_timing.py` checks
stream origins and PCM after a loss before/after the first device snapshot;
the native movie-update fixture checks the exact unheard movie suffix. The
Windows-only opt-in `TSFP_TEST_AUDIO_DEVICE=1` test opens the real default device,
submits silent PCM, injects a critical callback and checks reopened playback.
It is distinct from physical unplug/replug, Bluetooth switching and listening.

Primary API references: [XAudio critical-error recovery](https://learn.microsoft.com/en-us/windows/win32/api/xaudio2/nf-xaudio2-ixaudio2enginecallback-oncriticalerror),
[XAudio 2.9 endpoint migration](https://learn.microsoft.com/en-us/windows/win32/xaudio2/xaudio2-redistributable),
and [SDL default-device migration](https://wiki.libsdl.org/SDL3/SDL_OpenAudioDevice).

## Integration with Enhanced Audio

General recovery patches 0055-0058 apply after the existing 0040/0045/0050
beta patches. The earlier local 0035-0038 names were reordered for integration;
the recorded patch ledger still migrates that local stack. No recovery path
checks `pc_audio_enhanced_enabled`: buffering, invalid-slot retirement, format
repair and device recovery apply in either mode. The toggle continues to own
only spatial routing, gunfire range and environmental/ambience processing.

XAudio serializes the beta setters with recovery and preserves routing/filter
targets across unexpected voice/engine loss; explicit reset clears them.
Both adapters restore filter history after rejected PCM submission. The XAudio
backup is shared under the output lock and copied only for active filtering.
`test_audio_device_recovery.py` adds enhanced-on/off cases for both adapters,
checking recovery, ordinary PCM preservation and explicit-reset cleanup.
These deterministic adapter checks do not establish audible whole-game or
physical Mac acceptance.

## Runtime contracts

Effects buffering: `media/0055-dsound-busy-buffer-recovery.patch` extends the
shared `hle_dsound.c` pump. Its default 40 ms lead grows to the
observed active service interval plus 10 ms, capped at 90 ms; patch2305 shares
that measured headroom across fresh effect slots. Explicit
`RECOMP_DSOUND_LEAD_MS` values stay fixed, including legacy zero. Play still
submits immediately; pause/stop resets prevent learning inactive intervals.
After a >100 ms output-model interval, stale data is skipped only up to now,
then the future lead is submitted in the same call. Refused submissions keep
the output cursor unchanged; expired one-shots stay expired. Guest clocks,
stream/music policy and host audio adapters are unchanged. The existing audio
timing CI suite now exercises 64 voices under delayed service and recovery.

`media/0056-dsound-invalid-buffer-retirement.patch` makes every unusable-buffer
exit in `model_for_known` retire an existing slot/host voice, including null
settings holders and invalid rate/alignment. DirectSoundDoWork therefore reclaims
invalid stopped buffers as well as playing ones. Valid stopped/paused buffers
are retained; no live voice is evicted to admit another sound. See
[audio lifecycle tests](audio-buffers.md). Guest Release remains unchanged.

`media/0057-dsound-format-and-tail-recovery.patch` includes rate/alignment in
buffer identity, retains raw source size while clocking complete frames, and
preserves stream packet boundaries for rate-only SetFormat. Exact unheard PCM
and ADPCM suffixes survive voice resets/pause; encoding layout changes keep the
previous policy. See [format regression coverage](audio-buffers.md).

`media/0058-audio-device-recovery.patch` adds one-second XAudio engine/voice
retries and a shared per-slot discontinuity signal for streams and movies.
Their wall-clock fallback skips elapsed audio and establishes a fresh playback
origin after reconnecting. Explicit reset clears the signal; mute/shutdown
remain terminal. See [device recovery and tests](audio-buffers.md).

## Final buffer Release and dropout diagnostics

`media/2300-dsound-release-diagnostics.patch` preserves the original
`CDirectSoundBuffer_Release` and retires the matching host slot only when its
returned reference count is zero. The object argument maps to interface +1C.
The supported US constructor 00408028 installs vtable 004A1CE0, whose +8 method
is 00406FF0. Public IDirectSoundBuffer_Release 00406AA0 subtracts 1C and calls
that method. Its 0040688A helper decrements object+4 and invokes destruction
only on zero; these inspected bytes justify the added extra symbol. Generation
preserves the original body and derives the thunk's four argument bytes from
its ret instruction. No guest destructor, refcount, callback or memory is
replaced. Failed/nonfinal releases retain the model; unmodelled objects still
call the original. A generation identity prevents reentrant slot/address reuse
from being retired by the outer Release. The shared HLE covers both hosts and
never consults Enhanced Audio.

The five-second DSOUND health report now shows occupied slots, failed pool
allocations, release calls/retirements, refused PCM submissions and starvation
episodes. A dry queue remains one episode across repeated refused retries;
accepted PCM permits counting the next episode. Invalid-slot retirement remains
a separate fallback. Refusal counts describe attempts, not sounds proven lost.
`RecompAudioEngineStats` retains lifetime counts for queue-full, invalid PCM,
output unavailable, voice retry waits, allocation failures, voice/device
failures, output-open attempts and successes. XAudio/SDL snapshots retain them
across reset, recovery and shutdown, including when hardware statistics are
unavailable. SDL still has no hardware playback-position/glitch measurement;
its adapter-owned failures exclude migrations handled internally by SDL.

Existing CI routes `test_audio_timing.py` and `test_audio_device_recovery.py`
cover nonfinal/error/final Release, playing/paused/stopped slots, still-valid
freed settings, four complete 256-slot reuse rounds, reentrant replacement,
unmodelled objects, continued PCM output, starvation episode counting and
failure-category snapshots on both adapters. These fixtures do not prove
long-session gameplay or physical headset switching/listening.

## Exit reporting and combined lifecycle stress

Patch 2301 registers normal-exit reports when HLE/output first initializes.
The HLE report snapshots only host-owned counters and occupancy under its lock;
it does not read guest settings, which may already have been freed. Both
adapters register their idempotent shutdown and emit the same final lifetime
failure/recovery fields, even after failed initialization or deliberate mute.
Explicit shutdown followed by process exit prints once. This complements the
five-second report for short sessions; abrupt termination/crashes cannot be
promised an exit summary. Patch 2302 makes the Windows RHI WM_QUIT path use
standard exit rather than ExitProcess. Patch 2303 also changes xbox_HostExit,
the actual shadow_wndproc WM_CLOSE target, to run these handlers while retaining
its feedback/profile flushes. The native SDL path already uses standard exit.

`audio_lifecycle_stress.c` links the actual shared buffer HLE/model to each
complete XAudio/SDL adapter. Only host APIs and guest services are doubled. The fake pthread header is
scoped to the SDL driver; the XAudio C++ driver needs native libc++ pthreads.
Each Enhanced Audio on/off case performs 32 eight-buffer scene teardowns,
combining actual Play/Pause/Resume/Release calls, output loss/refusal, release
while recovery is pending, surviving-voice recovery and same-address reuse.
It checks all host voices retire, no released sound resurrects, paused state
and DSP/routing/discontinuity state clear, and final summaries appear without
calling the periodic reporter. Existing recovery cases additionally cover
failed-start/muted exit and repeated explicit shutdown. The real RHI host
fixture posts WM_QUIT; a separate test compiles the real xbox_HostExit body
and checks diagnostics and exit handlers. These
are controlled lifecycle checks, not a real level transition or listening test.

## One-shot completion follows queued output

`media/2304-dsound-buffer-completion.patch` extends GetStatus after natural
wall-clock completion of a non-looping buffer. If that buffer has accepted
PCM and its host voice still has queued buffers, status remains PLAYING.
This prevents a status-driven Stop/reuse from discarding the last samples
after an underrun. The guest cursor clock, lead, stale-data policy and explicit
Stop/Release semantics are unchanged. Explicit reset clears `fed`; paused
buffers retain PAUSED; unavailable output retains wall-clock completion.
XAudio supplies queue depth. Patch2306 adds an explicit SDL finite-tail drain
without claiming a device cursor. Physical listening remains a separate check.

`test_audio_timing.py::test_buffer_audible_completion` executes the real pump
and GetStatus with a 100ms PCM one-shot and a 60ms service interval. The initial
40ms queue runs dry for 20ms; all 9600 source bytes are submitted but the last
20ms remains queued at guest completion. Status now waits for that tail, then
finishes. Stop/reset, pause and device-unavailable controls retain their status.

## Fresh effects inherit busy-service headroom

`media/2305-dsound-burst-headroom.patch` shares observed active effects service
headroom across buffer lifetimes under the existing HLE lock. Previously each
fresh slot started at 40ms even after other voices had learned 70-90ms was
needed. Repeated short effects could therefore suffer their own initial
underrun before adapting. The shared value only grows from an active, fed
buffer's service gap, remains capped at 90ms, and never overrides explicit
fixed/zero lead. Play still submits immediately. Guest clocks, sound priority,
slot capacity, stream buffering and explicit stops are unchanged. No audio
thread or host adapter change is introduced. The higher learned lead persists
for the process, including across scenes; queued SDL routing changes can take
up to that bounded lead to reach playback.

The existing `test_effects_under_load` route adds `burst`: an active voice
learns a 60ms service interval, then eight rounds replace 63 simultaneous
non-looping voices. Each fresh voice must start with 70ms of PCM, retain its
own queue, and avoid another underrun at the next 60ms service. This regression
fails before patch2305. Existing fixed/zero, steady, long-hitch, refused-output,
finite-tail and expiry cases retain their previous behavior.

## FPS validation and remaining runtime limits

The load fixture additionally runs 64 independent voices at millisecond-
quantized 60/120/144/180/240 service cadences, with repeated 37ms delays. It
requires zero steady-service underruns, bounded queues, and no recurring
starvation after the first delayed service teaches the lead. These CPU/device-
double tests cover requested cadences independently of physical render speed.
They do not establish audible whole-game acceptance.

The 2026-11-03 Windows ten-bot Hotel matrix exercised all four current presets
and the legacy diagnostic144 rate. Windowed60/120/180/240 had no increase in
combat underruns; windowed144 added8. Verified fullscreen/VRR repeats added0
at180,12 at240,0 at144. All runs had zero effects submission refusals and zero
slot-exhaustion failures. The live machine did not sustain180/240 rendering;
see STATUS for measured present rates. Additional counter events remain
unexplained and must not be called fixed or attributed to long render hitches:
the profiled render gaps did not exceed90ms in these residual cases. Counter
events alone do not establish audible loss. Native Mac/network listening and
other scenes remain separate acceptance work.

### Loading-to-match reference

A later126-second Original run records the whole loading-to-match interval,
not just settled combat. Counter reports arrive with8 at65.594s and12 at70.969s,
then remain12 through126s. A continuous67.98s process capture starts after the
controller's55.016s launch request. Its near-silent10ms windows span13.97..16.06s
of the recording, around loading/first-person countdown. Capture discontinuity,
timestamp-error and full-scale counts are zero; XAudio reports zero engine
glitches/refusals/failures. A gain-preserved20s excerpt is ready for listening.

These are host-pipe report arrivals, not exact per-voice event timestamps. Input
replay logs print the scheduled `r->at`, not actual execution time; they cannot
time-bound the older counter increase against its capture. A single read of the
four initial64-byte PCM buffers finds shared all-zero data but does not attribute
the later increase. Individual gaps may be masked by other voices. The loading
silence, counter cause and listening acceptance remain distinct claims; no new
buffering/gain repair is justified from this observation alone.
The user subsequently reviewed the20-second excerpt on2026-11-07 and reported
only the loading pause, otherwise clean. Retain current buffering. This clears
that excerpt's listening concern, not the counter's per-voice cause or broader
preset/headphone/speaker/device acceptance; their equipment was unspecified.

## SDL finite-tail drain (review correction)

The original completion change incorrectly assumed SDL's position stub exposed
queue depth. Patch2306 introduces `recomp_audio_output_tail_pending(slot,finish)`
in the shared output contract and uses it for natural one-shot completion.
XAudio delegates to its existing queue snapshot. SDL retains position()==0,
so streams and movies still use their existing wall-clock fallback. Its new
drain operation checks both queued input and available converted output;
once the producer has accepted the entire tail, it flushes the resampling
remainder once. Successful new input/reset clears the flush marker. Ordinary
chunks, looping sounds, pause and explicit Stop do not flush through this path.
Query/flush failure retires only that voice and uses existing retry/discontinuity
handling; absent/reset/shutdown output reports no pending tail.

The complete shared HLE/model is now linked to each complete XAudio/SDL adapter
in a tail regression, with only host APIs doubled. A late natural completion
must retain PLAYING across pending input and converted output, then finish on
drain. The SDL case fails before2306 and passes afterward. Direct SDL tests
also verify no invented device cursor, one flush, reset/reuse, unavailable
output, query/flush failures and preservation of unrelated voices. These tests
are in the existing dual-host `test_audio_device_recovery.py` CI selection.

API semantics: [SDL queued input](https://wiki.libsdl.org/SDL3/SDL_GetAudioStreamQueued),
[available output](https://wiki.libsdl.org/SDL3/SDL_GetAudioStreamAvailable),
[end-of-input flush](https://wiki.libsdl.org/SDL3/SDL_FlushAudioStream).
