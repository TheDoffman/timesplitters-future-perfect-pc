# Replacing the video library

Done 20 September 2026, on `feat/video-hle`, with Black as the case.

A title that opens with a logo movie decodes it with XMV, the Xbox video
library. Like every SDK library it is linked into the XBE, so it is lifted and
runs as the title's own code, and it is some of the least forgiving code in
any image: hand-written MMX inner loops fed by pointers that come from the
graphics layer. Black faults inside one of them within seconds of starting,
having reached nothing else.

**Refusing to open the file does not help.** Black treats a missing movie as
fatal. It calls its own error callback, which asserts it is not on the main
thread and halts. That is reasonable behaviour, because on a real disc the
file is always there. `RECOMP_SKIP_VIDEO` is still right for a title that
merely moves on; Black is not one.

So the movie is replaced rather than decoded, at the library's API boundary.

## Finding the boundary without a signature database

The database this project uses covers Direct3D, DirectSound and the
application library. It does not cover the video library, so there were
addresses and no names.

`scripts/section_calls.py` finds them another way. Each SDK library gets its
own named section, so the address range already says which functions belong
to it. What it does not say is which are **entry points**, and that is the
only part that matters. The tool reads the generated C, where every call site
records both the function it is written inside and the address it calls, and
reports the calls that cross into the section from outside.

```
XMV spans 0x0023EB40..0x00266874
5 entry point(s), most-called first

  entry        args   via      called from
  0x0023F291 1      call     0x000C4560 0x00209D40
  0x0023EC4D 3      call     0x000D3620
  0x0023F191 1      call     0x000DB190
  0x0023F275 2      call     0x000D3620
  0x0023F45C 4      call     0x000C4570
```

**Five functions out of a 163 KB library.** Argument counts come from each
callee's own `ret N`, read out of the binary, so the signatures are the
binary's rather than a guess.

The tool works on any library. It reports 39 entry points for DirectSound in
the same title, shows three of the video entry points calling into it, which
is what a movie with sound looks like, and tells you plainly when a title has
no such section at all, as TimeSplitters 2 does.

## Naming them

Reading the callers is what turns five addresses into five functions.

| Address | Name | How the caller gives it away |
| --- | --- | --- |
| 0x0023EC4D | `XMVPlaybackCreate` | Third argument is the address of the caller's own handle field. A non-zero return makes the title fetch a callback and invoke it. |
| 0x0023F191 | `XMVPlaybackDestroy` | The caller zeroes its handle field immediately afterwards. |
| 0x0023F275 | `XMVPlaybackGetStreamInfo` | Nine instructions: copies the object's +0x40, +0x44 and +0x48 into the caller's struct at +0, +4 and +0xC. |
| 0x0023F291 | `XMVPlaybackStart` | The caller sets its own "playing" flag straight after. |
| 0x0023F45C | `XMVPlaybackUpdate` | Called once per pump with a surface and an out-parameter the caller then switches on. |

The switch is the important part. The caller reads the status, rejects
anything above 3, and jumps through a four-entry table:

```
status 0 -> counts a frame and keeps going
status 1 -> sets the title's "movie done" flag
status 2 -> paused
status 3 -> paused
```

So **status 1 is the transition the whole thing turns on**, and the title's
video state machine advances to its next state as soon as it sees one.

## Where the names live

`config/extra_symbols/<title id>.json`, merged by `tools.xdk_symbols` into the
file it generates. From there the existing replacement machinery does the
rest: `src/hle/hle_xmv.c` marks five functions with `HLE_EXPORT`, and the
recompiler generates a thunk from each address to each implementation. No new
binding mechanism was needed.

**Addresses are per title, not per SDK version.** A statically linked library
lands wherever the linker put it, so two titles on the same SDK have it in
different places. Only the behaviour is shared, which is why the
implementation is in `src/hle` and only the addresses are per title.

## What the replacement does

Answers the five calls without decoding anything. It hands back a playback
object with the dimensions and frame rate the title reads through
`GetStreamInfo`, reports the movie playing for the first few polls and
finished after that, and frees nothing because the guest heap has no free.

`RECOMP_XMV_SECONDS` gives the movie a real length, and the default is zero.
Zero is deliberate: nothing is drawn while the title waits, and the waiting is
not free. Black polls the playback in a tight loop rather than once a frame,
and a one-second movie cost it **17.8 million polls** of a burning core to sit
through a black screen. Three polls report the movie playing so a title that
wants to see a beginning sees one, and the fourth ends it.

`RECOMP_HLE_XMV=0` gives the title its own decoder back.

It draws nothing. With `RECOMP_FMV_HOST` the host's own decoder plays the file
when the title opens it, and this keeps the title's state machine happy
alongside it.

## Result on Black

| | before | after |
| --- | --- | --- |
| outcome | access violation in the decoder | no crash |
| swaps in 40 s | 3 | **2005** |
| draws | none | **10,173** |
| draws skipped by the renderer | — | **0** |
| vertex shaders translated | — | 27 |

The title gets through its intro, loads, and runs a frame loop at roughly 50
frames a second with every draw reaching the renderer.

## What it did not fix

**The picture is black.** Frames dumped with `RECOMP_HLE_D3D8_DUMP` are a
single colour at zero brightness across the whole frame. The capture path is
not at fault: the same switch on TimeSplitters 2 gives 800 distinct colours
and a mean brightness of about 50.

So Black issues ten thousand draws that produce nothing visible, which is a
rendering problem and not a video one.

**There is a concrete lead.** Black is the first D3D8LTCG title here, and its
symbols carry fifteen functions whose names record that the linker gave them
register-passed arguments:

```
D3DDevice_LoadVertexShader_4__LTCG_eax1
D3DDevice_SelectVertexShader_0__LTCG_eax1_ebx2
D3DDevice_SetPixelShaderConstant_4__LTCG_ecx1_eax3
D3D_SetTileNoWait_0__LTCG_eax1_ecx2
...
```

**None of the fifteen is replaced.** The replacement layer matches on the
plain name, so `D3DDevice_LoadVertexShader` never matches
`D3DDevice_LoadVertexShader_4__LTCG_eax1`, and those functions run as lifted
code with no replacement. They are the shader path, which is exactly what had
to be replaced to make TimeSplitters 2 draw.

**That is now done**, and it was not the cause.

`tools/recomp/hle.py` parses the suffix, matches a plain implementation name
against a single LTCG variant, and the generated thunk lays an ordinary
argument frame just below the stack, fills it from the registers the name
records and from the caller's stack for the rest, and points `g_esp` at it for
the call. Implementations are unchanged and cannot tell the difference. Two
more functions bind in Black as a result, including
`D3DDevice_SelectVertexShader`, taking it from 46 replaced to 48.

The picture is still black. Of the fifteen register-argument functions only
two have implementations at all, so the rest still run as lifted code, but
none of the thirteen is a pixel-shader entry point and `SetPixelShader` was
already bound in Black exactly as it is in TimeSplitters 2.

The real symptom is narrower than it looked:

```
[HLE-D3D8] shadow pixel shader: 0 draws with a combiner, 2594 without
```

**Not one of Black's draws carries a register combiner.** In TimeSplitters 2
the combiners are what produce colour. So the title either never sets a pixel
shader, or sets one the combiner layer declines to parse, and that is where
the next session should start: instrument `D3DDevice_SetPixelShader` to say
what it is handed and how often.

## Playing the movies (planned, 23 September 2026)

Answering the calls without decoding was the right first step: it stopped the
crashes. The cost is that no title shows its intro, and six of the titles on
hand ship XMV:

| title | files | video | audio |
| --- | --- | --- | --- |
| XGRA | 68 | 640x480, 640x512 | PCM, 44.1 and 48 kHz |
| Breakdown | 24 | 640x480, 640x540 | Xbox ADPCM (0x69), a few PCM |
| Black | 17 | 640x480 | no audio track in the file |
| 007: Nightfire | 16 | 640x480, container version 3 | Xbox ADPCM |
| TimeSplitters: Future Perfect | 6 | 640x480, 720x480, 720x576 | PCM |
| Otogi | 1 | 640x480 | PCM |

(Bloody Roar Extreme and Gauntlet ship ASF `.wmv`, which Media Foundation reads
as it is. Doom 3 and THPS2X ship Bink, which nothing in Windows decodes.)

`RECOMP_FMV_HOST` does not cover these: it plays through Media Foundation,
which does not know the XMV container, and it owns a D3D8 device of its own,
which the shadow renderer excludes.

### The container

FFmpeg's `libavformat/xmv.c` describes it, and a parse to that description
walks every packet and frame of all six Future Perfect files to the byte:

- File header: next packet size, this packet size, max packet size, `xobX`,
  version (4; Nightfire's is 3), width, height, duration in ms, audio track
  count (u16) plus two bytes, then 12 bytes per track: codec tag (u16),
  channels (u16), sample rate (u32), bits per sample (u16), flags (u16).
- Each packet: next packet size, then a video word (low 23 bits the video data
  size, bits 23-30 the frame count, bit 31 "extradata follows"), then one word
  per audio track (low 23 bits its size). The video size is 4 bytes short per
  audio track, which FFmpeg notes and corrects for.
- Video data: optional 4-byte WMV2 extradata (`0x75000000` in every file seen),
  then frames, each a header word (low 17 bits the size in words minus one,
  the rest a timestamp delta in ms) and the frame. **The WMV2 bitstream is
  stored as little-endian 32-bit words**, so each word is byte-swapped before
  it goes to a standard decoder.
- Audio data follows the video data, track by track.

### The design

1. **Demux** in portable C (`src/video/xmv_demux.c`): open the file the title
   asked for, hand out video frames and audio blocks in order with their
   timestamps.
2. **Decode video** with the WMV decoder that ships with Windows (the WMV
   decoder MFT), fed WMV2 with the extradata as its user data. No FFmpeg.
3. **Decode into the title's own surface.** `XMVPlaybackUpdate` is handed the
   surface the title will present; the frame is converted into that surface's
   own format in guest memory and the update reports status 1, "new frame".
   The title then presents it exactly as it would on hardware (Future Perfect
   through `UpdateOverlay`, Black as a texture), and the shadow renderer picks
   the pixels up like any other texture upload. Nothing here needs to know how
   a given title shows its movie.
4. **Audio** goes straight to the host's XAudio2 output, PCM as it is and
   Xbox ADPCM through `src/hle/xbox_adpcm.c`. The title's own DirectSound
   stream for the movie stays NULL, as now.
5. **Timing** follows the frame timestamps against the host clock, so a movie
   plays at its own rate whatever the title's poll rate is.

What is still to be found, per title, before it can work: how `Create`'s
`source` argument names the file, and whether `UpdateOverlay` reaches the
shadow renderer.

### Finding: Windows' WMV decoder will not take WMV2 (23 September 2026)

Step 2 does not work on the machine this was tried on (Windows 11, build
26200). The WMV decoder MFT lists WMV2 among its input types and refuses every
WMV2 input type it is given (`MF_E_INVALIDMEDIATYPE`), with the file's codec
data in either byte order, with none, and with the full attribute set that a
real ASF file produces. Through its DMO interface the answer is the same
(`DMO_E_TYPE_NOT_ACCEPTED`). The WMV **encoder** refuses a WMV2 output type in
the same way. WMV1 works end to end through both with identical code, so the
calls are right and WMV2 specifically is unavailable.

So the decode is FFmpeg's `libavcodec`, loaded at run time (below).

### Decoding through FFmpeg

`src/video/xmv_decode.c` compiles against FFmpeg's headers only and loads
`avcodec-62`, `avutil-60` and `swresample-6` when the first movie opens: beside
the executable, then `RECOMP_FFMPEG_DIR`, then this checkout's
`third_party/ffmpeg/bin` (the BtbN LGPL shared build of 8.1, fetched, not
committed). Without them a movie is skipped exactly as before, so no title
needs FFmpeg to start.

Two things about the container that the prose above does not say, both found
by comparing against FFmpeg byte for byte:

- **The codec data is not WMV2's.** XMV packs the WMV2 flags into its own
  layout (mspel bit 0, loop filter 1, abt 2, j-type 3, top-left MV 4,
  per-MB RL 5, slice count bits 6-8), and they have to be moved to where WMV2
  keeps them (bits 15, 14, 13, 12, 11, 10 and 7-9) before a decoder sees them.
  `0x75` in Future Perfect's files becomes `0x0000AC80`. With the raw word
  every frame decodes to coloured blocks.
- **A packet whose frame count is 0 has no video**, only audio; its video bytes
  are padding. Reading one frame from it walks into the padding.

Checked on Future Perfect (`frd`, `eag_e`), Nightfire and Black: every frame
decodes with no errors and the pictures are right. Black's `02_n.xmv` gives
803 pictures from 1000 frames, the same count as FFmpeg's own tool.

### Result on Future Perfect (23 September 2026)

Both intro movies play in full with their sound, and the front end follows:
the EA logo (88 of 88 pictures) and the Free Radical logo (81 of 81), then the
player-count menu at 60 fps.

How the pieces ended up, where they differ from the plan above:

- **The picture goes two ways.** Future Perfect shows its movie on the video
  overlay (`UpdateOverlay`), a plane the Xbox scan-out puts over the frame
  buffer, so `hle_xmv_play.c` hands each picture to a host movie layer
  (`src/d3d/d3d8_movie.c`) and the shadow renderer draws it over the finished
  frame while the overlay is up. Black draws its movie surface as a texture
  instead, so each picture is also written into the surface `Update` is given,
  in that surface's own format (`LIN_A8R8G8B8` for Black, `YUY2` for Future
  Perfect's overlay surface); the shadow renderer's per-frame checksum sees the
  change and uploads it.
- **`EnableOverlay` must not run the XDK's body.** Turning the overlay off
  waits for the video scaler, hardware nothing emulates, and Future Perfect
  hung there the moment its first movie ended.
- **Pictures are timed by the wall clock, not the sound.** A packet's sound is
  read with its first picture, so a clock that follows the sound stops when
  the sound runs out and waits forever for a picture that is only read when
  the clock moves. The EA logo stopped at exactly 1033 ms that way.
- The sound has a voice of its own, `RECOMP_AUDIO_SLOT_MOVIE` (272).

Switches: `RECOMP_XMV_PLAY=0` goes back to reporting movies over at once;
`RECOMP_HLE_XMV=0` still hands the title its own decoder; `RECOMP_FFMPEG_DIR`
says where FFmpeg is when it is not beside the executable.

**Black**, the texture case: its EA logo and the rifle movie behind its front
end play (254 and 391 pictures); the frames it captures went from 0.4% lit to
82.5%. Its movies carry no sound track.

Still to do: the four XMV titles whose library entry points are not named yet
(Nightfire, Breakdown, XGRA, Otogi) -- `config/extra_symbols/<title id>.json`
through `scripts/section_calls.py`, as for Black and Future Perfect.

### Breakdown (23 September 2026)

Breakdown never reached its movie: it stopped after one frame, spinning on a
flag its `D3DDevice_InsertCallback` callback clears. The XDK writes that
callback into the push buffer for the GPU to raise, and no GPU here runs
push buffers, so `InsertCallback` is now replaced and runs the callback at
once (everything before it has been drawn by then). Breakdown now runs at
about 50 fps and opens `romdata\movie\namcologo.xmv`.

Its XMV entry points are named in `config/extra_symbols/4E4D0009.json`: the
six above plus `XMVPlaybackGetAudioStreamInfo` and
`XMVPlaybackGetCurrentTime`. Naming them showed that `GetStreamInfo`'s `+0xC`
is the **audio stream count** (`XMVVIDEODESC`: width, height, frames per
second, audio streams), not a frame rate: Breakdown read the old float 30.0
as 0x41F00000 tracks. It is 0 now, since the host plays the sound itself.

The movie opens, its Xbox ADPCM sound decodes, and each picture is written
into its YUY2 surface. **It is not visible yet**, for a reason that is not
the movie's: all of Breakdown's drawing goes through recorded push buffers
(`BeginPushBuffer`/`RunPushBuffer`, `BeginPush`/`EndPush`), which the shadow
renderer does not see, so it reports no draws on any screen. Its player also
calls `Update` only once: after `Start` it sits in state 3 and nothing seen so
far moves it on, which may be how this XDK's asynchronous playback works.
