# Hunter: The Reckoning (USA), XDK 4361

Brought up 2 October 2026 on `main` 5963e67 with the toolkit changes from the
Halo and Future Perfect bring-ups and three changes of its own (below), and
re-verified the same day on `main` 012e356 (after #34).

Result: the title boots, plays its Bink movies, the main menu, character
select and the opening in-engine cinema, and plays the first level (the
subway): the tutorial box, the hunter on the platform, enemies, a fight,
damage, death and the level restarting, at 60 frames per second outside
loading. All of it with scripted input; nobody has played it with a pad yet.

## What it is

| | |
| --- | --- |
| Disc | Hunter: The Reckoning, USA retail (2002) |
| `default.xbe` | 2,899,968 bytes, SHA-1 39735b530002b4e2da4182042591ef6e68ee08a0 |
| Title ID | 0x56550007 |
| Entry point | 0x000A27C1 |
| XDK libraries | XAPILIB, D3D8, DSOUND, DMUSIC, XBOXKRNL, LIBCMT, D3DX8, XGRAPHC, all 1.0.4361 |
| Symbols recovered | 416 (`tools.xdk_symbols`; no DMUSIC, D3DX8 or LIBCMT coverage in the database) |
| Replacements bound | 53 of the 87 HLE exports resolve in this XBE |
| Kernel imports | 123 |

Code sits in `.text` (game, CRT, DirectMusic and Bink) and in the library
sections D3D, DSOUND, D3DX, XGRPH, WMADECXM, WMADEC and XPP. DOLBY is flagged
executable but holds the Dolby encoder's DSP image, not x86 code.

## Building it

`titles/hunter` is the template project (`scripts/new_title.py hunter "Hunter -
The Reckoning (USA)"`, then `scripts/regen_title_main.py hunter "Hunter: The
Reckoning" 0x000A27C1 "Hunter - The Reckoning (USA)"`). Seeds are in
`config/seeds/56550007.json`.

```
py -3 -m tools.xdk_symbols "games/Hunter - The Reckoning (USA)/default.xbe"
XBE="games/Hunter - The Reckoning (USA)/default.xbe"
W=games/_pipeline/hunter/out
py -3 scripts/recompile.py "$XBE" --work-dir $W --project titles/hunter --only parse -v
py -3 -m tools.disasm "$XBE" --text-only \
    --extra-sections D3D,DSOUND,D3DX,XGRPH,WMADECXM,WMADEC,XPP \
    -o $W/disasm --seed-functions config/seeds/56550007.json -v
py -3 scripts/recompile.py "$XBE" --work-dir $W --project titles/hunter --only identify -v
py -3 scripts/recompile.py "$XBE" --work-dir $W --project titles/hunter --only lift -v
cmake -S titles/hunter -B titles/hunter/build -G "Visual Studio 17 2022" -A x64
cmake --build titles/hunter/build --config Release
```

The disassembler runs by hand, with the arguments `recompile.py` gives it plus
`--text-only` and the code sections by name, because `recompile.py` would also
sweep DOLBY and has no option for a section list. The default was not tried.

Lift: 17,516 functions detected, 17,676 of 17,701 translated (0 failed), 32
unresolved call targets stubbed, 1.84 million lines of C.

## What it needed

- **Seeds (10).** Three a run observed (a thread start at 0x000A2752 and two
  XPP indirect-call targets). Seven are out-of-line tails: blocks a function's
  conditional jump reaches, parked after its `ret`, which the function splitter
  cut off as functions of their own that only pop the return address. One
  (0x0018F20E) failed at boot as a `[STUB]`; the other six have the same shape
  and were found by reading. Each pops registers its parent saved, so the seed
  gate's unsaved-pop test does not apply to them; each was checked by hand.
- **`recomp_manual.c`: Bink's MMX probe.** sub_001E6CC0 answers "is MMX
  there?" with `pushfd`/`popfd` and `cpuid`, which the lifter emits as no-ops,
  so it said no; Bink then picked a blitter table entry that is null for
  non-MMX, and the first movie hung on null calls (from 0x001D4B05). The
  override returns 1, as the console does.
- **Render states in the XDK 4034-4431 layout.** Deferred states at slot
  82 and complex at 117; without it state forwarding is off and nothing blends.
- **The translator's re-sync drops every out-of-phase decode it covers.**
  The CRT memcpy and memmove lifted with junk decoded from their inline jump
  tables; one piece, `or byte ptr [esi+0x5F], bl`, ran after every copy that
  ended in the zero-trailing-bytes case and corrupted the byte 0x5F past the
  copy's source. In Hunter it hit DirectMusic wave objects (caught with a write
  watch) and, by the shape of the bad values, the heap's own links: the heap
  failed in the switch from character select to the opening cinema in every run
  that got there.
- **DirectSound stream completion events.** Streams without a callback
  complete a packet by setting its event; the HLE passed the guest handle token
  to the host, which failed every time, so Hunter's music streamer only ever saw
  timeouts.

## How it was tested

Headless runs with a background window, sound muted, a fresh save folder and
the game's own copy of the disc folder (default.xbe and scratchimg.bin copies,
Media a link), a scripted pad (RECOMP_INPUT_SEQ: START through the logos and the
title, A through the menus and character select, then both sticks held in turn),
at most 240 s each.

| Milestone | Result |
| --- | --- |
| Movies | the Bink logos and intro play; START skips them |
| Main menu | the logo and NEW GAME / CONTINUE / OPTIONS / SPECIAL FEATURES, blended, 60 fps |
| Character select | AVENGER's card with its five stat rows, three empty stone cards |
| Opening cinema | the in-engine scenes before the first level |
| First level | the Hunter-Net tutorial box over the subway platform; the hunter by the tracks with the HUD; zombies, a fight, blood, health falling; the hunter moves on the sticks; he dies, the game returns to character select and restarts the level |

No crash, no unresolved indirect call and no stub was reported in the
240-second run that reached the level. On `main` 012e356 with these changes,
lifted and built again, the menu frame differs from the 5963e67 run's by 1.0
of 255 on average (the backdrop's grain) and the level frames at swaps 2880
and 4320 by 0.03 and 0.21; the level holds 60 frames per second outside
loading.

## Known limits

- Played only by script, one player. A pad or the keyboard, and players 2-4,
  are untried. Sound was never listened to (every run was muted).
- Not compared with the console: the level is dark, which may or may not be
  how it looks there, and one cinema frame shows a figure smeared or doubled.
- Bink's background I/O thread polls a mutex in a loop (about 17 million
  NtReleaseMutant calls a run, one host core busy). Bink creates that thread
  suspended and the runtime starts it at once; it copes, because the loop
  re-reads its handles.
- Guest thread priorities do not reach the host (KeSetBasePriorityThread is
  handed a guest object, not a host handle); Hunter raises its audio thread.
