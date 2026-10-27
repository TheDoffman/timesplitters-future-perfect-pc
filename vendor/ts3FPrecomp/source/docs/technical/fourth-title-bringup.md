# Fourth title: Outrun 2, XDK 5849

Started 19 September 2026, after Marvel vs Capcom 2 was parked. A racing game,
so the nearest thing yet to Burnout 2 — and on **XDK 5849**, the version the
toolkit was originally built against, which should mean the fewest surprises
of any title so far.

## What it is

| | |
| --- | --- |
| Title ID | 0x53450036 |
| XDK build | **5849** (Burnout 2 is 5344, TimeSplitters 2 is 4721, Marvel vs Capcom 2 is 5028) |
| Graphics | D3D8, not LTCG |
| Engine | not RenderWare |
| `.text` | 1,540 KB — the smallest of the four |
| Sections | 22, **four demand-loaded** |
| Kernel imports | 152 |
| Entry point | 0x000F33CD |
| Notable libraries | XONLINES, XVOICE, VOICMAIL — it has Xbox Live and voice |

## Getting to a build

Again nothing in the pipeline needed changing:

```
py -3 -m tools.xdk_symbols "games/Outrun 2/default.xbe"
py -3 scripts/recompile.py "games/Outrun 2/default.xbe" \
    --work-dir games/_pipeline/outrun2/out --project titles/outrun2
py -3 scripts/regen_title_main.py outrun2 "Outrun 2" 0x000F33CD "Outrun 2"
cmake -S titles/outrun2 -B titles/outrun2/build -G "Visual Studio 16 2019" -A x64
cmake --build titles/outrun2/build --config Release --target outrun2_recomp
```

11,812 functions, 1,331,762 lines of C in 12 files, none failed, 73 call
targets stubbed as unresolved. **48 of 53 XDK functions replaced by name** —
the best of the four, which is what being on 5849 buys.

## Where it stops

Further than Marvel vs Capcom 2 in one way and less far in another: it reaches
`CreateDevice`, creates the shadow device at 640x480, mirrors the GPU time
fence, finds its frame buffer at physical 0x024E4000, presents its first
`Swap` — and then **faults**.

```
[CRASH] Access violation at RIP=0x7FF7CAF11DBF, fault addr=0xF861404C (read)
  Xbox regs: eax=0x0000000D ecx=0xF8604020 esi=0xF8604020 esp=0x00F7EF04
  Xbox VA of fault: 0xF860404C
  in sub_001BB846+0xCF
  guest stack: 0x00257360 <- 0x001BC6D2 <- 0x001BD546 <- 0x001BD668 <- 0x001BDCE5
```

`sub_001BB846` is a thiscall method — its first act is `esi = ecx` — so
**0xF8604020 is the object pointer itself**, and the fault is reading a field
of it. The address is in no aperture this runtime maps: the tiled aperture
covers 64 MB at 0xF0000000, and 0xF8604020 is 140 MB past its base, which is
more physical memory than the console has.

Two readings were possible, and they needed different work: an aperture this
runtime does not map, or a pointer that was never valid. **It is the second,
and the evidence is conclusive.**

`RECOMP_ALIAS_HIGH=1` maps 0xF8000000 as a second alias of the contiguous
window, purely to answer this. With it, the title reads that address happily
and then faults immediately at **0xFFC00000** — a different wild address, in
different code. Following a bad pointer looks exactly like that; a real
aperture does not.

What clinches it is what those numbers are. Searching the XBE:

| value | occurrences in the image | what it is |
| --- | --- | --- |
| 0xF8604020 | 0 | computed at run time, not a constant |
| 0xFFC00000 | **1548** | the bit pattern of a NaN float |

0xFFC00000 is `-NaN` in IEEE 754, and 1548 of them are sitting in the title's
data — uninitialised or "invalid" float slots. The second fault dereferences
one. So the code is reading **float data as a pointer**, which means the
structure it is walking was never filled in.

`RECOMP_ALIAS_HIGH` is kept, off by default and labelled an experiment,
because it is the probe that answered the question and the question will come
up again. It should never be turned on to make a title "work": all it does is
let a bad pointer read zeros instead of faulting, which trades a clear failure
for a mysterious one.

## What it actually was: a null std::map, and debris

**The C++ object reading of this was wrong, and is retracted.** An earlier
pass here read the vtable 0x00257360 out of the crash frame, found the two
constructors that install it, and concluded the object had been built by the
wrong one. None of that was the bug. The vtable was a leftover word on the
stack, and the value being dereferenced was never written by anything.

Three new switches settled it in three runs. They are general, they are
described in `docs/technical/memory-watchpoints.md`, and this was the case
they were built for.

**Run 1, who holds it.** `RECOMP_FIND_VALUE=0xF8604020` scans guest RAM at the
crash for that value. Exactly one word in 64 MB held it, at **guest address
4**. A value living at address 4 is not an aperture question. It is a null
pointer with a small offset added.

**Run 2, what wrote it.** `RECOMP_WATCH_WRITE=0x00000004` protects the page
and names every writer:

```
[WATCH] write to 0x00000004 ... eax=0x8239F620 edi=0x00000004
[WATCH]   0x008239F5 -> 0x8239F620
[WATCH] write to 0x00000005 ... eax=0x8239F740 edi=0x00000005
[WATCH]   0x8239F620 -> 0x39F74020
[WATCH] write to 0x00000006 ... eax=0x8239F860 edi=0x00000006
[WATCH]   0x39F74020 -> 0xF8604020
```

Three unaligned dword stores, one byte apart, each of an unrelated
contiguous-memory pointer. Byte 4 comes from the first, byte 5 from the
second, bytes 6 and 7 from the third. **0xF8604020 is debris** -- the tail of
one store and the head of the next, later read back as a pointer. There is no
instruction anywhere that wrote it, which is why searching for one got
nowhere.

**Run 3, where the mistake is.** `RECOMP_TRAP_NULL=1` makes guest page zero
unreadable, so the null access faults where it happens:

```
[CRASH] Access violation, Xbox VA of fault: 0x00000008 (read)
  ecx=0x00000000
  in sub_001BD1DA+0x3B
```

`sub_001BD1DA` loads `[ecx+8]`, then walks `+4` as a key and `+0x10` and
`+0x14` as children, comparing and branching left or right. That is a
red-black tree lookup: a `std::map::find`. Its `this` is null.

The chain is `sub_001C0D4C` -> `sub_001C051F` -> `sub_001BD97C` ->
`sub_001BD1DA`, and the null arrives as **`sub_001C0D4C`'s first stack
parameter**. `sub_001C0D4C` loads it with `ecx = MEM32(ebp + 8)` and passes
it straight down.

So the question is now a small one: who calls `sub_001C0D4C` with a null
first argument, and what should have produced that map. Worth checking early,
because it would be a toolkit problem rather than a title one: whether the
title's C++ static initialisers all ran. `tools/disasm/functions.py` already
carries a fix for exactly this in TimeSplitters 2, where initialiser table
entries pointed inside other functions and `_initterm` silently skipped every
one. A global `std::map` that was never constructed is the same symptom.

## Ruled out along the way

- **Our own D3D8 replacement.** `RECOMP_HLE_D3D8=off` crashes identically, at
  the same address in the same function. `RECOMP_AC97_READY=0` likewise.
  `RECOMP_VBLANK=0` does not crash only because the title then blocks forever
  on its presentation event and never reaches the code.
- **A missing memory aperture.** `RECOMP_ALIAS_HIGH=1` maps 0xF8000000 as a
  second alias purely to test this. The title then reads that address happily
  and faults at 0xFFC00000 instead, which is a NaN bit pattern occurring 1548
  times in the image. Following a bad pointer looks exactly like that; a real
  aperture does not.
- **File I/O and the cache partition.** The title opens the raw disk
  devices and sets its cache partition up successfully: `Partition5`
  opens, and it reads and writes sector 4 of `Partition0` (the partition
  images live under `%LOCALAPPDATA%` in the `xboxrecomp` folder). The one
  failed open in a whole run is the downloadable-content metadata file
  under `TDATA`, which does not exist and is not supposed to.
- **Uninitialised heap.** The debris is **0xF8604020 in every run**, and the
  three stores that build it are the same three every time. Determinism is
  what made the three-run loop above possible at all: each run could assume
  the last one's findings still held.

## Next

1. **Find who passes the null map.** `sub_001C0D4C` takes it as its first
   stack parameter and passes it down. Watch or read back from there.
2. **Check the static initialisers first**, because it is the cheap answer and
   it is a toolkit bug if it is true. `tools/disasm/functions.py` already
   handles one shape of this from TimeSplitters 2, where initialiser table
   entries pointed inside other functions and `_initterm` skipped them all. A
   global `std::map` that was never constructed has exactly this symptom.
3. Then the ordinary boot loop: `docs/technical/second-title-bringup.md` is the
   worked example, and `CLAUDE.md`'s debug table maps symptoms to causes.
4. The demand-loaded sections (four of them) have not been exercised yet and
   are worth remembering when something later is mysteriously absent.
