# Marvel vs Capcom 2: renders, but its textures are refused

20 September 2026. The title boots, plays its intro audio and presents frames at
about 25 ms each. The screen is essentially the clear colour, because **59% of
its texture binds are refused**: its art is P8 and the shadow path drops P8.

**Read "Measured: the blocker is P8" at the bottom first.** It supersedes the
section immediately below, which was the first theory and is refuted — the
inline vertex path is real but carries only start-up work. The section is kept
because the reasoning in it was sound and only the conclusion was wrong.

---

## Superseded: the inline immediate-mode vertex path

MvC2 submits geometry the Xbox inline way:

```
D3DDevice_Begin(primitive)
  SetVertexData4f / SetVertexData2f / SetVertexDataColor    <- one vertex
  ... repeated per vertex ...
D3DDevice_End()
```

`src/hle/hle_d3d8.c` replaces `SetVertexDataColor` and `SetVertexData2f`, but
only to set a **persistent input register** — what a vertex program reads from
a register the vertex does not carry, which is Burnout 2's use of them. It does
not replace `Begin`, `End` or `SetVertexData4f`, and it never accumulates
vertices. So every vertex submitted this way is dropped.

The evidence is the draw count: **7.5 draws per frame** (5619 draws over 745
swaps) reach the renderer, while the summary reports `skipped 0 program without
layout, 0 declaration shader, 0 unknown shader, 0 stride, 0 primitive,
0 failed`. Nothing is being refused — it never arrives.

All five entry points are identified in this XBE, so they can be replaced by
name:

| symbol | address |
| --- | --- |
| `D3DDevice_Begin` | 0x002875A0 |
| `D3DDevice_End` | 0x002875E0 |
| `D3DDevice_SetVertexData4f` | 0x002874F0 |
| `D3DDevice_SetVertexData2f` | 0x002874B0 |
| `D3DDevice_SetVertexDataColor` | 0x00287550 |

**The work.** Record the primitive type at `Begin`, accumulate each vertex as
the `SetVertexData*` calls arrive, and issue the draw at `End`. The catch is
that the same two calls already mean "set a persistent register" outside a
`Begin`/`End` pair, and Burnout 2 depends on that, so the replacement has to
behave differently inside the pair than outside it. This is a standard Xbox
path, not an MvC2 quirk, so other titles will want it.

---

## It takes 80 to 175 seconds to reach its first frame

Unexplained, and separate from the blank screen. The main thread polls for that
whole time — 460 million `RtlLeaveCriticalSection` calls in 40 seconds, in the
shape

```
enter CS; if (get_work() == -1) { leave CS; }   repeat
```

while very little I/O happens. Worth its own investigation.

---

## Three things measured and found wrong

All three were stated with more confidence than they deserved. They are here so
the next person does not spend the same rounds.

**"Start-up is a race, about one run in three."** No. Runs were too short. A
good run needs 80–175 s of loading before its first frame, and the runs that
"failed" were 75 s — right on the threshold. Six consecutive misses looked like
strong evidence of a race and were an artefact of shortening the runs from 100 s
to 75 s.

The mistake underneath it: the shadow summary prints every 5 s **counting from
the first swap**, not from process start. Reading "2 swaps at the first summary"
as "rendering by 5 s" is wrong; a 100 s run with 4 summaries means rendering
began at about 80 s.

**"The guest spin starves the loader."** A switch that yielded on every *n*th
critical-section release changed loader throughput not at all across n = 0, 64
and 1024 — identical offsets, identical bytes. The switch was removed.

**"Yielding fixes the frame rate."** One run with it on looked like it took the
frame from 758 ms to 25 ms. Run in pairs:

```
CS_YIELD=0    swaps=0     no timing     <- a stalled run, not a slow one
CS_YIELD=256  swaps=643   24.98 ms
CS_YIELD=0    swaps=627   23.35 ms      <- the same, without yielding
CS_YIELD=256  swaps=601   24.89 ms
```

The 758 ms was an average over a run that spent most of its length loading.

**`RECOMP_FILE_SYNC=1` makes no difference** here either: 602 swaps against
747 without it. It is not the loader's problem.

---

## Two traps

**Counting swaps.** `grep "shadow: Swap"` counts a one-off notice about which
guest thread swaps, not swaps. Reading that made this title look like it
rendered a single frame when it was rendering hundreds. The number is in the
periodic summary:

```bash
grep -oE "shadow: [0-9]+ swaps" run.err | grep -oE "[0-9]+" | sort -n | tail -1
```

**Run length.** Give this title at least 200 s before concluding anything, and
never draw a conclusion from one run.

---

## Measured: the blocker is P8, not the inline vertex path (20 Sep 2026)

Both hypotheses in this file were tested with counters. One is refuted and the
other is now the lead.

### The inline vertex path is not it

`D3DDevice_Begin`, `End` and `SetVertexData4f` are counted (replaced by name,
each calling `HLE_CALL_ORIGINAL`, so the title's own code still runs). Two
runs that reached rendering, at 487 and 510 swaps:

```
317 Begin, 317 End, 1268 SetVertexData4f; peak per frame 77 Begin, 308 vertex data
```

**Byte-identical across both**, despite different frame counts. Per-frame work
would scale with frames; this does not move, so the inline calls all happen in
a fixed start-up phase and stop. Implementing the path would not change
gameplay output. `1268 / 317` is exactly 4, so they are quads — sprites or UI,
submitted once.

The mechanism is still real: those calls run the title's own XDK D3D8, which
writes the push buffer, and nothing reads the push buffer. It will matter for a
title that uses the path per frame. It is not MvC2's problem.

### P8 textures are refused, and that is 59% of every bind

```
texture format 0x0B refused (P8, no palette is forwarded); draws using it are untextured
shadow textures: 1106 binds, 1 cached, 0 uploads; skipped 656 format
```

**Exactly one format is refused, and it is P8.** 656 of 1106 binds. Whatever
geometry arrives is drawn untextured, which is what "a bold colour, not the
proper scene" looks like from a play session. MvC2 is a sprite fighter, so
palettised art is most of what it has.

`read_layout()` in `src/hle/hle_d3d8_texture.c` refuses it outright:

```c
if (t->fmt == XFMT_P8 || d3d8_format_bpp((D3DFORMAT)t->fmt) == 0) {
    g_skip_format++;
    return 0;
}
```

### Why this is a bridge rather than new code

The renderer already does P8. `src/d3d/d3d8_resources.c` has
`case D3DFMT_P8: /* expanded to BGRA through the palette */`,
`d3d8_convert_linear_pixels(fmt, w, h, src, dst, palette)` takes the stage's
palette, and the device exposes
`SetPalette(PaletteNumber, pEntries)`. Only the shadow path drops P8.

The work is therefore:

1. stop refusing P8 in `read_layout()` and let `upload()` run — the mip upload
   already converts through a palette;
2. forward the guest's palette, by replacing the XDK's palette entry point and
   calling the host `SetPalette` for the matching stage.

Point 2 is the part that does not exist yet, and is what the comment in that
file's header means by "P8 (no palette is forwarded)".

**Not yet verified**: that P8 is the *only* thing between here and a correct
picture. Draw traffic is about 6 per frame, which is low for a fighting game
even allowing for sprites, so there may be a second cause behind this one.
