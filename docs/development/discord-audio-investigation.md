# Discord audio/music investigation (unresolved)

Report: https://discord.com/channels/1566295768489263136/1567436747321647105
Branch: `fix/discord-audio-music`. Investigated main `831c324a` on Windows.
No runtime change or resolution comment: the complete report is not verified fixed.

The report includes broken victory music, minigun sound glitches, skipping music,
and a voice that begins normally then loses volume. The reporter says stable
0.5.0 still exhibits it. The earlier buffer completion/headroom/tail repairs
are already in that release; their existence is not a new fix for this report.
The supplied victory and minigun clips were retained locally as
`logs/audio-{victory,minigun}.mp4` and decoded PCM. The victory clip loses roughly
10 dB after its first 1.5 seconds. This observation does not distinguish an
intended mix/fade from a playback bug, and desktop listening was not performed.

`tests/media/test_audio_timing.py`: 59 passed (2.89 seconds). These cover existing
CPU/device-double contracts, not audible acceptance. The effects queue remains
bounded to 90 ms; arbitrary longer main-thread stalls can still cause gaps.

Actual main-baseline game: executable SHA256
`550f988c6ec45e9504d74bcecd9486874928b5272712683b7e9e8b9152ac5936`,
Classic textures, one sample, Original 60, isolated Test Lab, Chinese scene 0x19.
Through ordinary menus, Custom 5 slot 1 was set to Minigun, bots disabled and
a two-minute round selected. Normal mouse primary bursts consumed ammunition
from 200 to 9; secondary spin was also exercised. Read-only observation found
hand and linked weapon-state agreement (value 7). This run establishes actual
weapon use rather than a synthetic buffer-only fixture. The round then reached
the first-place Results screen, which explicitly named Minigun as Favourite Gun.
`logs/audio-live-observation/victory-results.bmp` preserves that end-of-round
trigger; it does not establish audible victory-theme correctness.

The existing diagnostics recorded 1,020 SetFrequency calls, 941 rate changes,
605 changes while playing, and 1,995 voice resets at one burst checkpoint.
The music stream reported zero dry events at that checkpoint. Neither number
establishes audible correctness. Rate-change handling in `hle_dsound.c`
`IDirectSoundBuffer_SetFrequency` calls `resync_output`; XAudio source voices
are created with `XAUDIO2_VOICE_NOPITCH`. Smooth backend rate changes are a
specific next investigation, not an implemented or proven cause. Removing
resync without updating queued-sample clocks would be unsafe.

Local evidence: `logs/audio-live-observation` (route, binary/XBE provenance,
bounded stdout/stderr and post-burst capture), `logs/discord-audio-baseline-tests.log`.
The probe caps each output stream at 4 MiB, so logs are not complete lifetime
health records. Source/build used the documented local validation compiler
workaround for seven oversized generated units; it was not a default-flags
Release build. Saves and fixture inputs were isolated from the user's profile.

Next acceptance: compare captured output against a console/reference for the
same victory sequence and minigun pitch ramp; add a causality test for the
rate-change hypothesis, cover both host audio adapters, then replay the report's
volume/music cases. No volume or queue tuning was made without that evidence.
