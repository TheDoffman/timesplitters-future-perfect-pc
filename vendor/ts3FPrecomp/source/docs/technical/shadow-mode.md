# Shadow mode: drawing the title through `src/d3d`

`RECOMP_HLE_D3D8=shadow` makes a recompiled title draw. Each replaced XDK
function runs the title's own body first (`HLE_ORIGINAL`), then repeats the
call on a host device (`src/d3d`, Direct3D 11) in a second window. The title's
own frame buffer is untouched, so the two can be compared.

Burnout 2 renders its logos, menus, loading screens, HUD and race this way.

---

## The layers

```
title (XDK D3D8, statically linked)
  |  40 functions replaced by name (tools/recomp/hle.py, HLE_EXPORT)
src/hle          Xbox -> host conversion
  |  host_* wrappers, one per host call
src/hle/hle_d3d8_record.c        <- the seam
  |                        \
src/d3d (D3D11)             .d3dcap capture file
                                  |
                            src/replay/d3d8_replay.c
```

**Nothing in `src/hle` calls a device vtable entry or a `d3d8_vsh_*` /
`d3d8_combiners_*` setter directly.** A direct call is one the capture cannot
see, and a replay that silently differs from the run. The single exception is
reading the back buffer for frame dumps, which changes no state.

## What reaches the host, and from where

| Host state | Source |
|---|---|
| Clear, Swap, viewport, transforms | the replaced call's arguments |
| Render and texture stage states | the title's own D3D8 state arrays, read by name (`HLE_IMPORT_VAR`, XDK 4627+ layout only) |
| Vertex programs | the `X_D3DVertexShader` object: microcode plus its parsed declaration |
| Vertex data (UP and buffer draws) | guest memory, with NORMPACKED3 expanded on the CPU |
| Textures | the guest pixel container, cached by VA + data + format + size |
| **Pixel shaders** | the `D3DPIXELSHADERDEF` inside the object `SetPixelShader` selects (at +0x0C, with a pointer to it at +0x08) |
| Combiner constants | the render state array (`SetPixelShaderConstant` is assumed to land there; a mismatch with the definition's own C0 is logged once) |
| Render targets | the Xbox surface's parent: a texture's level 0 becomes a host render target texture, a cube container's face becomes that face of a host cube, the frame buffer becomes the back buffer, anything else a scratch target |

The frame buffer and the automatic depth buffer are recognised by identity:
they are the surfaces the XDK itself passes to `SetRenderTarget` inside
`CreateDevice`. Recognising them by size alone sends a title's full-screen
offscreen passes to the screen.

## Frame capture and replay

A capture records every host call for one frame, plus the vertex, index and
texture bytes those calls reference, and opens with a snapshot of the host's
state read back from `src/d3d`. Replay plays it into the same renderer with no
game running, no guest memory and no recompiled title.

That is the project's fast loop: **change the translation, replay the same
frame, compare the images.** A capture is deterministic, where two runs of the
title never land on the same moment.

```
# take captures of the frames that actually draw a scene
RECOMP_D3D8_CAPTURE=caps/c.d3dcap RECOMP_D3D8_CAPTURE_SWAP=2000 \
RECOMP_D3D8_CAPTURE_EVERY=150 RECOMP_D3D8_CAPTURE_MINDRAWS=300 your_game_recomp.exe

# replay one, both ways, and look at the two images
d3d8_replay caps/c_02000.d3dcap --out with_
d3d8_replay caps/c_02000.d3dcap --out without_ --no-combiners

# find which draw is at fault
d3d8_replay caps/c_02000.d3dcap --list-draws        # every draw, its state, targets, clears
d3d8_replay caps/c_02000.d3dcap --draws 690         # stop after 690 draws
d3d8_replay caps/c_02000.d3dcap --skip-draw 703     # leave one out
```

Captures hold the title's own textures and vertices, so **a capture is game
content and is never committed** — `.gitignore` covers `*.d3dcap`.

`tests/d3d8_capture` round-trips a synthetic capture (no game data) and runs
on Windows and Linux.

## Switches

| Switch | Effect |
|---|---|
| `RECOMP_HLE_D3D8=shadow` | draw the title through `src/d3d` in a second window |
| `RECOMP_HLE_D3D8_PS=0` | stop forwarding pixel shaders; use the host's fixed-function pixel path |
| `RECOMP_HLE_D3D8_PS_PROBE=1` | dump the first shader objects `SetPixelShader` is given |
| `RECOMP_HLE_D3D8_DUMP=<prefix>` | write the host back buffer as BMPs (at most 24) |
| `RECOMP_HLE_D3D8_DUMP_EVERY=<n>` | swaps between dumps |
| `RECOMP_HLE_D3D8_DUMP_MINDRAWS=<n>` | dump only frames with at least n draws |
| `RECOMP_D3D8_CAPTURE=<path>` | record a frame (see above) |
| `RECOMP_D3D8_CAPTURE_SWAP`, `_EVERY`, `_MINDRAWS` | which frames to record |
| `RECOMP_D3D8_PS_SHOW=<reg>` | draw one combiner register instead of the result: `v0`, `t0`, `r0`, `fog`, or `<reg>a` for its alpha |
| `RECOMP_D3D8_PS_DUMP=1`, `RECOMP_D3D8_VS_DUMP=1` | print the generated HLSL |
| `RECOMP_HLE_D3D8_TRACE_SWAPS=<from>-<to>` | one line per SetRenderTarget, SetTexture, Clear, GetBackBuffer2, CopyRects and Swap while the swap count is in the range: the order of a frame, including what the XDK does inside Swap's own body |
| `RECOMP_HLE_D3D8_FB_PROBE=1` | after each copy of the host frame into a title's frame-buffer texture, what the texture now holds |
| `RECOMP_D3D8_SCREENCOPY_PROBE=<n>` | for the first n copies, what the *source* held and whether drawing was going to the scene target |

## A frame that never reaches the back buffer (Future Perfect, 22 Sep 2026)

TimeSplitters: Future Perfect drew 69,000 times a minute and the screen stayed
pure white. Five things were wrong at once, and each needed a measurement:

1. **Its scene goes into a texture over the frame buffer.** The title wraps
   frame buffer A (data 0x00204000) in a texture of its own with
   `XGSetTextureHeader`, takes a surface of that texture each frame and sets
   it as the render target. Judged by its parent, that surface was "a render
   target texture" and every draw went into a host texture nothing presented.
   Now a surface whose data pointer is one of the swap surfaces' (the target
   CreateDevice set, plus whatever `GetBackBuffer2` has returned) is the
   screen, whatever it hangs off. Read the log's `data` fields, not the
   parent.
2. **Its device is `D3DSWAPEFFECT_COPY` with one back buffer**, and it calls
   `Swap(D3DSWAP_BYPASSCOPY)` then `Swap(D3DSWAP_FINISH)` every frame. The
   trace shows the XDK's Swap body itself setting the front buffer as the
   target and drawing a full-screen quad -- the title's swap callback, doing
   the back-to-front copy through a five-stage colour-grading combiner. The
   `[TRACE]` lines that appear *between* "Swap flags 0x2" and the next
   frame boundary are that callback. Both swaps present here; the second
   shows the same frame again.
3. **The callback binds the back buffer's *surface* object as its texture**
   (common type 5, not 4). The texture layer refused it as "not a texture"
   -- 3,296 skipped binds, one per frame -- and the quad sampled the 1x1
   white placeholder. A D3DSurface is a pixel container with the same
   Format, Size and Data fields, and the hardware reads it as a texture, so
   it is accepted now.
4. **The copy of the host frame into that texture must happen at bind time,
   inside Swap's body**, when the scene is still in the scene target. A
   snapshot taken at the frame boundary was tried and pinned a stale black
   copy: by then the target held the graded output, not the scene.
5. **Linear textures are addressed in texels on the NV2A.** The quad handed
   the sampler coordinates in 0..640 x 0..480 for a `LIN_A8R8G8B8` frame
   buffer; a normalised sampler clamped every one to an edge texel and the
   whole frame came out one flat grey. Both pixel paths now scale a stage's
   coordinates by 1/size when its texture's Xbox format is linear
   (`d3d8_format_is_linear`, `tex_scale` / `TexScale`).

The white screen that preceded all of this was not any of them: with nothing
drawn to the host back buffer, a frame dump shows whatever the buffer last
held, and it had last held a white fade frame. A dump is only evidence about
the frame if something drew that frame.

`MINDRAWS` matters more than it sounds: a title's 3D frames can be rare among
its 2D ones. Burnout 2 draws 2-6 times on a loading frame and 690-910 in a
race frame, so a fixed interval samples loading screens almost every time.

## What is per-title here

Very little. The replacement boundary is per XDK build, and below it everything
is NV2A behaviour that every title shares — shader translation, texture
formats, render target rules. What is per-title is which XDK the signature
database has to identify, the discovery seeds, and titles that fill their own
push buffers through `BeginPush`, whose draws no replacement ever sees.

## Known gaps

- A cube the title uploads from guest memory binds white. Only the cubes it
  renders into are mirrored, and the cache holds eight of them; past that the
  run log says so.
- `CopyRects` is not handled.
- `SetVertexData4f` / `SetVertexData4ub` are not replaced; `SetVertexDataColor`
  and `SetVertexData2f` are.
- A render target texture replays as zeros: its contents are drawn, never in
  the guest's memory, so a capture that samples one without drawing into it
  first shows nothing.
- Lights and material are not forwarded, so lighting stays off.
- Three render state layouts are read: XDK 4627+ (deferred states at slot 92,
  complex at 136), XDK 4034-4431 (82 and 117) and XDK builds before 4034 (82
  and 116, with the older texture stage order); anything else turns state
  forwarding off and says so. 4432-4626 (DEPTHCLIPCONTROL present: 83 and 118)
  has no title to prove it and is not read.
