# Effects buffering under load

`patches/media/0055-dsound-busy-buffer-recovery.patch` extends shared runtime
`src/hle/hle_dsound.c`; see [runtime](runtime.md) for the buffering contract.
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
