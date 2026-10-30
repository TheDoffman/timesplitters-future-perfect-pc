# Halo: Combat Evolved (USA), XDK 3925

Brought up 1 October 2026 on this fork's shadow renderer, and re-verified on
`main` 012e356 (after #34) with the toolkit changes below on 2 October 2026.
It is the first title here on an XDK before 4034, which is why most of what
it needed is XDK-version support rather than anything Halo-specific.

Result: the main menu, the opening cinematic and the first level's cryo bay
up to the "look around" prompt, lit and textured, at 30 frames per second
(Halo's own cap). Scripted input drives the menus, creates a profile, starts
the campaign and turns the view with the right stick.

## What it is

| | |
| --- | --- |
| Disc | Halo: Combat Evolved, USA retail 01.10.12.2276 |
| `default.xbe` | 2,535,424 bytes, SHA-1 198f094065eedf392908a643117004e270e4930a |
| Title ID | 0x4D530004 |
| Entry point | 0x00015CC9 |
| XDK libraries | XAPILIB 3911, D3D8 3925, D3DX8 3911, DSOUND 3936, XNETS 3911 |
| Symbols recovered | 359 (`tools.xdk_symbols`) |
| Replacements bound | 49 of the 87 HLE exports resolve in this XBE (main 012e356 with the changes below) |
| Kernel imports | 134, all routed |

Code sits in `.text` and in the library and Bink sections (D3D, D3DX,
DSOUND, XNET, BINK*, XPP). Two more sections, DOLBY and BINKDATA, are
flagged executable but hold no x86 code.

## Building it

`titles/halo` is the template project. Seeds are in
`config/seeds/4D530004.json` (91: 18 observed in runs, the rest from static
scans of the same XBE).

```
py -3 -m tools.xdk_symbols "games/Halo - Combat Evolved (USA)/default.xbe"
XBE="games/Halo - Combat Evolved (USA)/default.xbe"
W=games/_pipeline/halo/out
py -3 -m tools.xbe_parser "$XBE" --json "games/Halo - Combat Evolved (USA)/default_analysis.json"
py -3 -m tools.disasm "$XBE" --text-only \
    --extra-sections D3D,D3DX,DSOUND,XNET,BINK,BINK32,BINK32A,BINK16,BINK4444,BINK5551,BINK16MX,BINK16X2,BINK16M,BINK32MX,BINK32X2,BINK32M,XPP \
    -o $W/disasm --seed-functions config/seeds/4D530004.json -v
py -3 -m tools.func_id "$XBE" --functions $W/disasm/functions.json \
    --strings $W/disasm/strings.json --xrefs $W/disasm/xrefs.json -o $W/func_id -v
py -3 -m tools.recomp "$XBE" --game-only --split 250 \
    --disasm-dir $W/disasm --func-id-dir $W/func_id -o $W/recomp \
    --icall-sites $W/recomp/icall_sites.json --gen-dir titles/halo/src/recomp/gen \
    --exclude-manual titles/halo/src/recomp_manual.c --game-name Halo \
    --hle-symbols "games/Halo - Combat Evolved (USA)/default_xdk_symbols.json" -v
cmake -S titles/halo -B titles/halo/build -G "Visual Studio 17 2022" -A x64
cmake --build titles/halo/build --config Release
```

These are the stages `scripts/recompile.py` runs, with one difference: the
disassembler is given `--text-only` and the code sections by name, because
the default sweep would also take DOLBY and BINKDATA as code. The default
was not tried. Seed 0x001CF6AC lands inside an instruction the sweep
decoded; it is accepted only because it is marked observed (the disasm
change below, also in #30).

Lift: 7,797 functions; 7,791 of 7,812 translated, 0 failed; 25 unresolved
call stubs; 1.25 million lines in 32 files.

## What it needed from the toolkit

Each of these is its own commit, with the evidence in its message; the
evidence is Halo's. One came from main itself: #34 builds the cubes a title
fills from memory (`static_cube`), which this bring-up had needed as a patch.

1. **Contiguous allocator and pinned ranges** (kernel). Halo frees its first
   D3D device's memory and then pins its game state at physical 0x61000 and
   its tag cache at 0x3A6000 with `XPhysicalAlloc`. The pinned request was
   answered without telling the arena, so the 22 MB texture cache was carved
   across both and texture uploads overwrote the game state: a crash 23 s
   in, before the menu.
2. **DirectSound** (hle). Main's `IDirectSoundBuffer_SetBufferData` and
   `SetFormat` replacements (#28) write one XDK's settings layout. Halo's
   DSOUND 3936 keeps another (none of its buffers is modelled), so the
   replacement corrupted the Bink intro's audio buffer and skipped the
   game's own code: the intro movie hung 22 s in. Such buffers now get the
   game's code, as before #28.
3. **XDK 3925** (hle). The render-state arrays put the deferred states at
   slot 82 and the complex ones at 116 (4627+: 92 and 136), with the older
   texture-stage order; state forwarding was off, so nothing blended. The
   shader object's parsed declaration is laid out differently, so every
   program's draws were skipped as "no host layout"; the title's own D3DVSD
   tokens are read instead. The main menu waits on a visibility test with no
   timeout; with no push buffer executed nobody writes the result, so
   `GetVisibilityTestResult` now answers "complete, 0 pixels". That last one
   is a shim, not an occlusion query.
4. **Textures** (d3d8, hle). Cubes the title fills from memory (Halo's
   normalisation and environment cubes) got the 2D white placeholder and
   sampled zero: every surface lit through one was black; #34's
   `static_cube` now mirrors them (11 by the cryo bay, 0 refused). L8, AL8
   and A8L8 were stored as R8/R8G8, so alpha read 1: Halo's atmospheric fog pass
   then replaced every wall of the cryo bay with the room's black fog
   colour.
5. **Combiners** (d3d8). The final combiner read the last stage's constants
   instead of its own (the ring and every character black); the title's own
   texture modes were reduced to sampler kinds, so bumped cube-map
   reflections never reached a surface; and several stage details differed
   from the NV2A (mux direction, shared constants, read-before-write, clamps,
   blue-to-alpha, final-combiner settings).
6. **Captures** carry the texels of memory-filled cubes and the stage
   palettes, so Halo's world shaders replay faithfully (diagnostics only).
7. **Disassembler** trusts seeds a run observed (also in #30).

## How it was tested

Headless runs: background window, muted, a fresh `RECOMP_SAVE_DIR`, and
the pad owned by `RECOMP_INPUT_SEQ`:

```
46000:down:100,49000:down:100,52000:up:100,55000:up:100,58000:a,66000:a,74000:a,110000:a,118000:a
```

That walks the main menu, creates a profile, starts the campaign and skips
the opening cinematic into the cryo bay; without the last two presses the
cinematic plays (Keyes on the bridge at about swap 5700).

| Stage | Result |
| --- | --- |
| Start-up | XAPI, two D3D devices, DirectSoundCreate and 16 streams, XInput replaced |
| Intro movies | drawn through the inline Begin/End path |
| Main menu | the ring, nebula, planet and logo, lit and textured, 30.0 fps |
| Opening cinematic | the Pillar of Autumn's hull textured; Keyes lit and textured |
| Cryo bay | walls, floor, pods and crew lit and textured; the "look around" prompt; the right stick turns the view |

On main 012e356 with these changes (2 October 2026) the menu, the cryo bay at
the prompt and the cinematic's Keyes shots draw as they did on 5963e67 with the
same changes: the cryo bay frame at swap 6000 differs from that run's by 0.05
of 255 on average (0.03 % of pixels by more than 32: the crewman's idle
animation), the cinematic matches shot for shot about 150 swaps earlier, and
the menu and the level hold 30 frames per second.

## Known gaps

- Rendering approximations: BUMPENVMAP without the bump offset, CLIPPLANE
  kills nothing, DOT_ZW does not replace depth (logged once), PROJECT3D
  samples a placeholder (no volume textures), L16 / G8B8 / R8B8 channels.
- The first cinematic shot's ship is a near-black silhouette. That matches
  Halo's data as drawn (dark lightmap times dark base map) but has no
  reference frame.
- Visibility tests answer 0 pixels: lens flares and coronas that test their
  own visibility do not draw.
- A rare crash while a new thread's TLS is set up (once in 24 runs).
- Sound: in the first play test most of it was missing (a few effects). With
  these changes the streams take packets (65 done in a run to the cryo bay,
  471 through the cinematic), but Halo also submits packets to a stream object
  the HLE did not create (`stream Process on unknown object`, 4 a run) and
  signals an event by guest address (4 `SetEvent failed` a run). Every test
  run was muted, so none of this is checked by ear.
- Only scripted input; GAME DEMOS needs the disc's XDemos folder beside the
  XBE and would start a separate program.
- The first copy of each map waits out two 10 s timeouts.
