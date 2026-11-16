# Sound commands for a custom mode

Sound commands are an explicit addon to Lua API1. Declare `audio_api=1` in the
returned callback table. Existing API1 scripts keep their behavior and original
reply format. The package API remains `tsfp-mod-v1-draft` until release acceptance;
production game manifests do not advertise audio support yet.

```lua
return {
    api = 1,
    audio_api = 1,
    on_start = function()
        game.audio.play(0, 0x00000001, 1, 0.25, false)
    end,
    on_stop = function() game.audio.stop_all() end,
}
```

Include `audio/00000001.wav` in the same script package. A resource key is the
eight-digit lowercase hexadecimal filename, not an original sound-event ID or
guest address. Only committed profile assets are available. Files must satisfy
the [PCM16 WAV contract](mod-audio-input.md); filenames, host paths, decoder calls,
network access and dynamic native libraries are never passed to the worker.

| Method | Parameters |
| --- | --- |
| `game.audio.play(voice, key, pitch, gain, loop)` | Voice0..31 and integer key0..0xfffffffe. Optional pitch0.25..4 (default/zero restores authored WAV rate), gain0..1 (default1), boolean loop (defaultfalse). Replaces only that voice. |
| `game.audio.pause(voice, paused)` | Boolean, defaulttrue. False resumes the existing cursor. |
| `game.audio.pitch(voice, pitch)` | Changes playback rate; omitted/zero restores the WAV rate. |
| `game.audio.volume(voice, gain)` | Gain0..1, default1; zero is silent. Existing effects/master controls also apply. |
| `game.audio.stop(voice)` | Stops one voice. |
| `game.audio.stop_all()` | Stops the mode's custom voices. |

All methods return no value. Controls on an absent/finished voice are harmless;
playing an unknown resource refuses the entire transaction and stops the mode.
Pitch/gain must be finite numbers; keys/voices must be integers and flags actual
booleans. Extra arguments are refused. Lua API calls share the existing64-call
and64-command callback budgets, instruction watchdog and16MiB Lua heap ceiling.
Calls outside callbacks or without the explicit addon declaration fail. Callback
errors publish no earlier score, sound or message commands from that callback.
After `game.end_match`, and inside `on_stop`, only sound stop/stop-all are allowed.

The parent verifies canonical fields, sequence, event state and resources before
committing any gameplay model changes. Resource authority comes from the trusted
source/profile-bound store, not from Lua. A match without a trusted resource
validator refuses every sound command. Production dispatch also requires the
selected, active, nonfailed offline runtime and its simulation thread. Native
device unavailability/mute can prevent audible output without invalidating a
valid gameplay transaction. The [host player](mod-audio-player.md) describes
voice, queue, timing and lifecycle limits.

## Creator example

```console
python tools/mods/mod-workshop.py new build/audio-lab --id audio-lab --name "Audio lab" --author your-workshop-username --template audio
python tools/mods/mod-workshop.py pack build/audio-lab build/audio-lab.zip
python tools/mods/mod-workshop.py validate build/audio-lab.zip
```

The template creates an original callback script and two deterministic, generated
triangle-tone WAVs. It plays a one-shot, then a loop, pauses/resumes it, lowers
pitch/volume and stops. It uses no original game assets. The source remains an
editable project; the packer inventories and hashes the exact distributable files.
The current launcher keeps audio packages inactive until the installed game
advertises the verified audio capability. A successful package/VM test alone
does not enable that capability or establish actual in-game playback.

## Private worker wire

Legacy-only replies retain version1 and their16-byte command header. Batches
containing audio use version2: every command has the existing kind/target/value/
text-length u32 fields, then resource/pitch/flags u32 and signed volume i32,
then the declared UTF8 message bytes. Only message commands carry text. The
additional fields must all be zero for legacy commands; each audio kind has an
exact shape and unused fields must be zero. Gain is converted to bounded
hundredths of a dB; pitch is bounded Q16. No pointer or file path crosses the wire.
Version2 requires a nonempty batch; load/error replies remain version1.
The reply-body ceiling is35,104 bytes; source/input limits are unchanged.

Actual VM tests cover every sound control, declarations, mixed score/sound/message
batches, stop semantics, invalid types/ranges/NaN, full-budget replies and callback
rollback. A confined hostile worker exercises the actual native session parser's
canonical-field checks and legacy-wire refusal. Production factory tests cover
source resource authorization, unknown-resource rollback, simulation-thread
dispatch, invalidation and a reply exceeding the former frame limit. Creator
tests pack the audio template, run its full sequence in the real VM and load the
generated WAVs through the native immutable store.

At clean fdab5e79, normal native game builds and real offline six-bot audio-template
games pass on Windows, Mac and Linux/WSL software Vulkan. Private read-only
observers record accepted PCM and the authored control sequence; originals and
the synthetic fixture remain unchanged and games exit0. This establishes actual
game execution and backend acceptance, not physical audibility. Linux records
retryable full-game queue pressure, without device/voice failure. A prior Mac
attempt faults in MoltenVK finishQueries; its root cause remains open despite
the later passing run. STATUS retains counts, provenance and harness corrections.
Launcher-to-game audio acceptance, capability advertisement and release publication
remain required before general activation.
