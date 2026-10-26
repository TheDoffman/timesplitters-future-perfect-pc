# Vulkan and low-level graphics: an audit, and the route there

Scope: everything in the repo that produces a pixel. What exists, what it is
actually capable of, and what has to happen — in what order — to get a Vulkan
backend and a low-level NV2A path that gives full control over the GPU at the
highest performance the design allows.

Measured against `eb5355a` (17 September 2026). Every number below came from
the tree, not from a README; where a README disagrees, the tree wins.

---

## 0. The short version

There is **no Vulkan code in this repository.** `grep -ri vulkan` returns four
hits, all of them prose. The decision recorded in `CLAUDE.md` has not been
started.

There is, however, considerably more to build on than the two READMEs suggest,
and one piece of existing architecture — the `host_*` seam in
`src/hle/hle_d3d8_record.h` — that makes this much cheaper than it looks.

The single most important structural finding is this:

> **The project has two graphics frontends and no graphics backend.**
> `src/d3d` is not a renderer with a swappable backend; it is a D3D11 renderer.
> `d3d8_gl.c` is not an OpenGL backend; it is a second, far less capable
> *frontend* that re-implements the same COM interfaces over GL and is selected
> at CMake configure time by `if(WIN32)`. Adding Vulkan the same way produces a
> third fork of the frontend and three divergent implementations of the same
> 12,000 lines.

The second most important finding is that **Vulkan and LLE are not one project.**
They are orthogonal axes that share exactly one artifact — a backend command and
resource layer — and that shared artifact should be built first, once, for both.

The third is that **a meaningful amount of the performance the project is
leaving on the table today has nothing to do with the backend API**, and can be
recovered in the existing D3D11 path this week. See §6.

---

## 1. Inventory: what exists today

### 1.1 The D3D8 HLE frontend — `src/d3d`, 12,066 lines

| File | Lines | D3D11/DXGI touchpoints | What it is |
|---|---:|---:|---|
| `d3d8_resources.c` | 2,440 | 180 | Buffers, textures, cube/volume, ~120 `D3DFMT_` mappings, format conversion, mip chains |
| `d3d8_device.c` | 1,662 | 141 | Device, swapchain, draws, render targets, present |
| `d3d8_vsh.c` | 1,502 | 90 | NV2A vertex microcode → HLSL, input layouts, 64-entry cache |
| `d3d8_combiners.c` | 1,203 | 25 | NV2A register combiners → HLSL, 128-entry cache |
| `d3d8_shaders.c` | 1,199 | 64 | Fixed-function VS/PS in HLSL, per-texture-signature PS cache |
| `d3d8_xbox.h` | 1,174 | 0 | Public D3D8 types and vtables |
| `d3d8_swizzle.h` | 467 | 0 | Morton/Z-order decode, 2D and 3D |
| `d3d8_states.c` | 355 | 97 | Render state → D3D11 state objects |
| `d3d8_gl.c` | 1,076 | 0 | **Parallel POSIX frontend** (see §2.1) |

Total D3D11/DXGI references across `src/d3d`, `src/hle`, `src/replay`:
**~600 in ~13,000 lines, about 7% of the code.** Of those 600, **93 are the
single token `DXGI_FORMAT`** — it has become the project's de-facto "host pixel
format" type and has leaked outward as far as the capture container
(`d3d8_capture.h:209`, `D3D8CAP_VS_DECLARATION`).

That ratio is the good news: the overwhelming majority of `src/d3d` is Xbox
knowledge, not Direct3D knowledge, and Xbox knowledge is backend-neutral.

### 1.2 The host seam — `src/hle/hle_d3d8_record.h`

**24 functions.** Every call `src/hle` makes into the renderer goes through one
of them, and the header states the rule that keeps it true: nothing in
`src/hle` calls a device vtable entry or a `d3d8_vsh_*` / `d3d8_combiners_*`
setter directly.

```
host_Clear                  host_CreateTexture          host_vsh_create_shader
host_Swap                   host_LockRect               host_vsh_delete_shader
host_SetRenderState         host_UnlockRect             host_vsh_set_constant
host_SetTextureStageState   host_ReleaseTexture         host_vsh_set_declaration
host_SetTransform           host_SetRenderTarget        host_vsh_set_screenspace
host_SetViewport            host_CreateDepthStencil…    host_vsh_set_vertex_data
host_SetTexture             host_DeviceDepthSurface     host_combiners_set_pixel_shader
host_SetVertexShader        host_DrawPrimitiveUP        host_DrawIndexedPrimitiveUP
```

This is already a backend interface. It was built for frame capture, but the
property that makes capture work — one enforced choke point, after all Xbox
conversion — is exactly the property a backend abstraction needs. **This is the
project's biggest asset for the Vulkan move and it was built for another
reason.**

Its one flaw for this purpose: the signatures are typed in the project's own
D3D8 COM pointers (`IDirect3DDevice8 *`, `IDirect3DTexture8 *`), which binds
the seam to the frontend's object model rather than to a backend's.

### 1.3 Frame capture and replay — the A/B harness

`d3d8_capture.h` (307 lines) defines a versioned container, **format version 3,
22 chunk kinds**, holding one frame of host calls plus every byte they read,
opened by a full state snapshot. `src/replay/d3d8_replay.c` (1,001 lines)
replays it with no game running, no guest memory, no recompiled title.

I compiled `src/hle/d3d8_capture.c` with GCC on Linux with no Windows and no
Direct3D headers: it builds clean. The container is genuinely portable and
`tests/d3d8_capture` round-trips a synthetic capture in the Linux CI job.

The replay *tool*, however, is Windows-gated in CMake, for one stated reason:
it links `xbox_d3d8`, which on POSIX is `d3d8_gl.c` and has no equivalent entry
points. That gate disappears the moment a real backend interface exists.

**This harness is the correct bring-up vehicle for a Vulkan backend and it
already exists.** One capture, two backends, two images, diff. No game needed,
deterministic, seconds per iteration.

### 1.4 The LLE path — better than documented

`src/nv2a/README.md` says PGRAPH does no rendering, and it is right, but it
points at the wrong file. The register-level `nv2a_core.c` `pgraph_method()` is
**a counter**: it stores the parameter into a flat `pgraph.regs[]`, increments
statistics, and logs unhandled methods. The old PGRAPH → D3D11 translator was
removed in September 2026 (it handled 21 methods and discarded transform
programs, constants, vertex formats and combiners outright).

The real low-level work lives in `src/kernel`:

| File | Lines | What it does |
|---|---:|---|
| `nv2a_pb_exec.c` | 2,726 | Decodes the title's push buffer and **carries out a subset**: surfaces, clears, and a CPU rasteriser for screen-space flat-shaded geometry, written straight into guest RAM |
| `nv2a_pb_scan.c` | 205 | Read-only survey of the same stream; counts methods per subchannel, tracks parse health |
| `nv2a_vsh.c` | 681 | NV2A vertex microcode **decode and CPU execution** — pure C, builds everywhere (verified) |

Coverage: **~46 of 228 top-level `NV097_*` methods, about 20%.** (Counted as
`NV097_*` defines in `nv2a_pb_exec.c` against `NV097_*` defines in
`nv2a_regs.h` whose value is a method address rather than a field mask.
`gap-analysis.md` says 36 of 215 by a slightly different count; the ratio is
the same and neither is worth reconciling.) Enabled with
`RECOMP_PB_EXEC`; surveyed with `RECOMP_PB_SCAN`; and `scripts/pb_unhandled.py`
already turns the executor's unhandled list into a ranked, named work queue with
`NOOP` / `INERT` / real buckets.

Two shared components already cross the HLE/LLE line and prove convergence is
possible: `nv2a_pb_exec.c` includes `../d3d/d3d8_swizzle.h` rather than owning a
second Morton decoder, and `nv2a_vsh.c` was split out of `d3d8_vsh.c` precisely
so both paths could use one microcode parser.

Presentation for this path is `src/video/fb_present.c` — **GDI**, deliberately,
Windows-only, off unless `RECOMP_FB_WINDOW`.

The source is also honest about its own hazards in a way worth preserving.
`surface_hits_image()` and `dma_resolve()` exist because
`NV097_SET_SURFACE_COLOR_OFFSET` is an offset *inside the colour DMA object*
and the executor has always treated it as a guest VA. `NV097_SET_CONTEXT_DMA_COLOR`
is parsed but its base is never resolved. The Dashboard case wrote 4.9 MB of
black over its own code before the guard went in.

### 1.5 Shader generation

All three generators emit **HLSL, Shader Model 5.0**, compiled with
`D3DCompile`, with fully explicit binding:

```
cbuffer TransformCB  : register(b0)     Texture2D/TextureCube/Texture3D texN : register(tN)
cbuffer LightingCB   : register(b1)     SamplerState                  sampN : register(sN)
cbuffer VSH_Constants: register(b1)     192 float4 (3 KB)
cbuffer VSH_Screenspace : register(b2)
cbuffer VSH_VertexData  : register(b3)
```

That every generated shader already declares explicit, disjoint, low-numbered
`b`/`t`/`s` registers is a genuinely lucky property: **one Vulkan descriptor
set layout covers every shader this project will ever generate**, and DXC's
`-fvk-b-shift` / `-fvk-t-shift` / `-fvk-s-shift` maps them mechanically.
`CLAUDE.md` is right that these ~3,900 lines survive the move and should not be
rewritten to GLSL.

### 1.6 Build and CI

Three jobs: Python (pytest), Linux GCC (cmake + ctest), Windows MSVC (cmake +
ctest). No job renders anything, and nothing compares an image. Backend
selection is `if(WIN32)` in `src/d3d/CMakeLists.txt` — **configure-time, not
runtime**, which means no build can currently contain two backends and
therefore no build can A/B them.

---

## 2. Findings

### F1 — `d3d8_gl.c` is a fork of the frontend, not a backend. It is a liability.

1,076 lines implementing every `IDirect3DDevice8` vtable slot over GL 3.3, with
one fixed GLSL program supporting "position + diffuse + UV". It has no format
table, no swizzle, no mip chains, no cube or volume textures, no register
combiners, no vertex microcode, no render targets. Its own header comment lists
all of that as "next iterations".

It cannot be finished cheaply, because finishing it means porting ~7,000 lines
of Xbox logic that already exist thirty centimetres away in the same directory.
Meanwhile it is the only thing `xbox_d3d8` resolves to on Linux, which is why
`src/replay` — the one tool that could make a Vulkan backend tractable — is
Windows-only.

**It should be deleted, not extended.** Not immediately; it is the only thing
keeping the Linux CI job compiling a graphics library. But it is not the
Vulkan starting point and treating it as one is the main way this goes wrong.

### F2 — `DXGI_FORMAT` is the portability blocker, and it has escaped into the capture format

93 of ~600 D3D11 touchpoints. It appears in `d3d8_internal.h` struct fields, in
`d3d8_vsh.h`'s `D3D8VshInput`, and — critically — in the capture container's
documented field list. The cross-backend A/B format is therefore currently
D3D11-flavoured.

This is the cheapest high-value change in the whole audit and it gates
everything else.

### F3 — There is no pipeline cache, only a depth-1 memo that destroys and recreates

`d3d8_states.c`:

```c
if (hash == g_last_blend_hash && g_blend_state) return;
g_last_blend_hash = hash;
if (g_blend_state) { ID3D11BlendState_Release(g_blend_state); g_blend_state = NULL; }
... CreateBlendState(...)
```

Same shape for depth-stencil and rasteriser. A frame that alternates between
two blend configurations creates and destroys a state object **per draw**. In
D3D11 that is wasteful; in Vulkan the equivalent is `vkCreateGraphicsPipelines`
per draw, which is not a performance problem but a stall.

A related latent issue: `hash_blend_states()` packs each field into a fixed
4-bit slot (`SRCBLEND << 4`, `DESTBLEND << 8`, `BLENDOP << 12`). Every value in
this header's `D3DBLEND` enum is ≤ 11, so it is currently sound — but the Xbox
constant-colour blend factors (0x8006/0x8007), which this header does not yet
define, would alias straight through `COLORWRITEENABLE`. Worth fixing while the
state layer is being rewritten anyway.

### F4 — `DrawIndexedPrimitiveUP` allocates two GPU buffers per draw call

`dev_DrawPrimitiveUP` does the right thing: a ring buffer with
`MAP_WRITE_NO_OVERWRITE` and a `DISCARD` on wrap.

`dev_DrawIndexedPrimitiveUP`, thirty lines below it, does not:

```c
bd.Usage = D3D11_USAGE_IMMUTABLE;  ... CreateBuffer(&tmp_vb);
bd.BindFlags = D3D11_BIND_INDEX_BUFFER; ... CreateBuffer(&tmp_ib);
... draw ...
ID3D11Buffer_Release(tmp_ib); ID3D11Buffer_Release(tmp_vb);
```

`shadow-mode.md` records Burnout 2 drawing **690–910 times in a race frame**.
The capture format has exactly two draw chunk kinds, `D3D8CAP_DRAW_UP` and
`D3D8CAP_DRAW_INDEXED_UP`, and `host_*` exposes only the two UP draws — so
**every draw in the HLE path is a UP draw**, and every indexed one is two
buffer creations and two destructions. Up to ~1,800 per frame.

`convert_fan_or_quad` additionally `malloc`s and `free`s per fan or quad draw.

This is backend-independent and fixable today.

### F5 — PGRAPH state is modelled twice, half each

`nv2a_core.c` keeps `pgraph.regs[]`, a flat array with no structure and no
consumer. `nv2a_pb_exec.c` keeps `s_gpu`, a purpose-built struct that knows
about surfaces and clip rectangles but only what its CPU rasteriser needs.
Neither is a PGRAPH state model. Any LLE path that reaches a GPU needs one, and
it is the piece that was deleted in September 2026.

### F6 — The two frontends produce different representations of the same draw

HLE produces: a D3D8 vertex shader handle, a `D3DPIXELSHADERDEF`, D3D8 render
states, host-converted vertex bytes.
LLE produces: NV2A transform program microcode, register-combiner register
values, vertex attribute array descriptors pointing into guest RAM.

These are the *same information* in two encodings, and both already feed HLSL
generators that consume the NV2A forms. `d3d8_vsh.c` takes microcode.
`d3d8_combiners.c` takes a `D3DPIXELSHADERDEF`, which is itself a packed
description of NV2A combiner registers.

**Giving those two generators a second entry point that takes PGRAPH register
state is the single highest-leverage reuse available in this project.** It is
what turns LLE from "a second renderer" into "a second frontend".

### F7 — Three windows, no owner

`src/hle` opens a window for shadow mode. `src/video/fb_present.c` opens a GDI
window for the guest framebuffer. `video_player.c` has the FMV player's own.
With two frontends feeding one backend this becomes a swapchain ownership
problem. Presentation needs a single owner before, not after.

### F8 — The `host_*` rule is enforced by convention only

`hle_d3d8_record.h` states the rule that makes capture correct, and nothing
checks it. One `grep` in CI would. This matters more, not less, once there are
two frontends and two backends.

---

## 3. The shape of the answer

Two axes, not one project:

```
                     D3D11          Vulkan
                  ┌──────────────┬──────────────┐
   D3D8 HLE       │   TODAY      │   Phase V    │   ← fast path, per-XDK boundary
   (src/hle)      │   (Windows)  │              │
                  ├──────────────┼──────────────┤
   NV2A LLE       │  (removed    │   Phase L    │   ← full control, per-title-proof
   (pb_exec)      │   Sep 2026)  │              │
                  └──────────────┴──────────────┘
                          ↑
              one backend serves all four cells
```

The honest tension, stated plainly, because it is the crux of the request:

**LLE at the push-buffer boundary will never be faster than HLE at the D3D8
boundary.** `CLAUDE.md` is correct about this and it is worth not arguing with.
The LLE path must parse a command stream, decode state changes, resolve DMA
objects, and track guest memory coherency — per frame, per draw — and the HLE
path skips all of it by receiving the same information pre-digested as
arguments. That cost is real and it is where emulator frame time goes.

**But "highest performance" and "full control" are not the same request, and
they are answerable at different layers.** LLE buys correctness on titles that
hand-roll push buffers or defeat signature matching, and it buys the ability to
see and change everything. Vulkan buys explicit control over submission,
memory and pipeline state — and that is where the performance actually is.

So: do not choose. **Make both frontends emit the same backend command stream,
and make the backend the fast part.** Then LLE costs what push-buffer decode
costs — bounded, measurable, and paid only by the titles that need it — rather
than costing a whole second renderer.

---

## 4. Phase V — Vulkan

### V0 · Delete `DXGI_FORMAT` from everything above the backend — 1 week

Introduce `src/d3d/rhi_format.h`: an `RhiFormat` enum, `d3d8_to_rhi_format()`,
`rhi_format_bpp()`, `rhi_format_is_depth()`, `rhi_format_is_compressed()`, plus
per-backend `rhi_format_to_dxgi()` / `rhi_format_to_vk()`.

Replace `DXGI_FORMAT` in `d3d8_internal.h`, `d3d8_resources.c`, `d3d8_vsh.c`,
`d3d8_vsh.h` and `d3d8_capture.h`. Bump the capture format to version 4 (it is
checked exactly, not ranged — by design).

Gate: CI stays green on both jobs, `tests/d3d8_smoke` and `tests/d3d8_capture`
unchanged in behaviour.

This is mechanical, it is the prerequisite for everything below, and it is also
what makes `src/replay` portable.

### V1 · Extract the RHI; port D3D11 behind it — 3–4 weeks

Define the interface from what the code already does, not from what a graphics
API offers. Measured from the current call sites, the necessary surface is:

- **device** — create, resize, present, acquire backbuffer, capabilities query
- **buffer** — create (vertex/index/uniform), destroy, map-discard, map-no-overwrite, update
- **texture** — create 2D/cube/3D with mip chains, update subresource, create view, destroy
- **sampler** — create from filter/address/LOD, cached
- **target** — bind colour + depth, clear, resolve MSAA, readback via staging
- **pipeline** — `(vs blob, ps blob, input layout, blend, depth-stencil, raster, topology, RT formats)` → cached immutable object
- **shader** — compile HLSL source → backend blob, with a persistent on-disk cache
- **draw** — `draw`, `draw_indexed`, dynamic viewport and scissor

Port `d3d8_device.c`, `d3d8_resources.c`, `d3d8_states.c`, `d3d8_shaders.c`,
`d3d8_vsh.c` and `d3d8_combiners.c` onto it, with D3D11 as the only backend.

**Nothing new renders in this phase.** The frame must be pixel-identical.
That is exactly what the capture harness is for: take a set of captures before
the refactor, replay them after, diff the BMPs.

Also in this phase, because the state layer is being rewritten anyway: replace
the depth-1 memo with a real hash-keyed pipeline cache (F3), and give the
indexed UP path the same ring buffer the non-indexed one has (F4).

### V2 · The Vulkan backend, brought up entirely against `d3d8_replay` — 4–6 weeks

No game. No guest memory. One `.d3dcap` file, two backends, two images.

```bash
d3d8_replay caps/race.d3dcap --backend d3d11  --out ref_
d3d8_replay caps/race.d3dcap --backend vulkan --out vk_
compare ref_000.bmp vk_000.bmp
```

Bring-up order, each step a visible image:
clear + present → UP draws with the fixed-function shaders → textures and
samplers → register combiners → vertex programs → render targets → MSAA and
readback.

Specifics that will come up:

- **Y-flip**: negative viewport height (`VK_KHR_maintenance1`, core since 1.1).
  Do **not** flip the projection matrix. `CLAUDE.md` warns that `d3d8_states.c`
  already carries one hand-annotated winding fix (`D3DCULL_CW → D3D11_CULL_FRONT`,
  `FrontCounterClockwise = FALSE`); stack a second and you get inverted culling
  that only shows on two-sided geometry, which is the worst possible failure to
  debug. Compensate the front-face winding once, in the backend, and validate
  against the existing mapping rather than re-deriving it.
- **HLSL → SPIR-V**: DXC with `-spirv`, profiles `vs_6_0` / `ps_6_0`. Map the
  existing explicit registers with `-fvk-b-shift 0 0 -fvk-t-shift 8 0
  -fvk-s-shift 16 0` (or similar) so `b0..b3`, `t0..t3`, `s0..s3` land in one
  descriptor set with disjoint bindings. One `VkDescriptorSetLayout` for every
  shader the project generates.
- **Constants**: the VSH constant buffer is 192 × float4 = 3 KB. That is a
  dynamic-offset UBO fed from a per-frame ring allocator, not a
  `vkUpdateDescriptorSets` per draw.
- **Shader compilation cost**: `dxcompiler` is ~18 MB (the MS Fusion teardown
  in `docs/technical/` notes the same figure). Precompile the *fixed* shaders to
  SPIR-V offline and ship them; run DXC at runtime only for generated combiner
  and vertex-program variants. Then add a persistent on-disk cache keyed by
  `(combiner token | microcode hash, backend, generator version)` so the second
  run of a title compiles nothing. The current caches are 128 and 64 entries,
  in memory, discarded at exit.
- **Pipeline cache**: serialise `VkPipelineCache` to disk per title, same
  rationale.
- **Threading**: `CLAUDE.md` keeps a cooperative single-thread guest model. Keep
  every `vkQueueSubmit` on one thread and do not let the guest threading model
  leak into the backend.

### V3 · Make Vulkan the default on Linux; delete `d3d8_gl.c` — 1 week

At this point the GL frontend is strictly worse than the Vulkan backend on
every axis and its continued existence is a source of divergence.

Also: make backend selection **runtime**, not `if(WIN32)`. A build that can
contain both is what makes the A/B permanent rather than a one-off.

### V4 · The performance work Vulkan exists for — 3–4 weeks, then ongoing

- Render-pass grouping; `LOAD_OP_DONT_CARE` after a full clear; don't reload
  attachments that are about to be overwritten.
- Batch the per-frame UP uploads into one large host-visible ring, one
  `vkCmdBindVertexBuffers` per state group rather than per draw.
- Descriptor indexing / bindless for the four texture stages, to cut per-draw
  descriptor churn.
- Asynchronous pipeline compilation, so a newly-seen combiner token does not
  stall the frame that first uses it.
- Timeline semaphores, 2–3 frames in flight.

---

## 5. Phase L — low-level NV2A

Phases L0–L2 are independent of Phase V and can run concurrently. L3 requires
V1.

### L0 · Inventory, per title — days

The tooling exists. Run it, on the second and third title, not just Burnout:

```bash
RECOMP_PB_SCAN=1 ./game_recomp.exe 2> runs/scan.err
RECOMP_PB_EXEC=1 RECOMP_PB_UNHANDLED_ALL=1 ./game_recomp.exe 2> runs/pb.err
python3 scripts/pb_unhandled.py runs/pb.err
```

**Implement nothing before this list exists.** 228 methods is a lot to
speculate about and `pb_unhandled.py` already sorts them into "genuinely
nothing to do", "a D3D11 backend would want this", and real work.

### L1 · DMA objects — 1–2 weeks

The correctness floor, and the blocker for everything downstream. Implement
RAMIN DMA-object lookup properly: `NV097_SET_CONTEXT_DMA_COLOR` (and zeta,
semaphore, vertex, texture) name context objects with a class, a base and a
limit. Resolve them.

Then `dma_resolve()`'s heuristic and `surface_hits_image()`'s guard can be
**deleted rather than tuned**. A guard that prevents a catastrophe is not the
same as a fix, and the source says so itself.

### L2 · A PGRAPH state model — 3–4 weeks

One struct, filled from the method stream, replacing both `pgraph.regs[]` and
`s_gpu`:

- **surfaces** — colour and zeta: DMA object, offset, format, pitch, clip,
  swizzle/tiling, anti-aliasing
- **vertex attributes** — 16 slots: format, component count, stride, DMA offset
- **texture units** — 4: offset, format, size, mip levels, filter, address mode,
  palette, border
- **shader stage** — transform program, 192 constants, execution mode, viewport
  scale and offset
- **combiners** — the RC register set, in the form `d3d8_combiners.c` can consume
- **raster** — blend, depth, stencil, alpha test, fog, cull, scissor

This is state, not a translator. The translator that was removed in September
2026 was removed because it *was* a translator: it mapped 21 methods directly to
D3D11 calls and had nowhere to put the things that did not map.

### L3 · LLE onto the RHI — 4–6 weeks (requires V1)

With L2 and the RHI in place, a draw becomes: resolve vertex arrays out of
guest RAM → look up or build a pipeline from the transform program and combiner
state → issue an RHI draw.

The reuse here is the point (F6). Give `d3d8_vsh.c` and `d3d8_combiners.c` a
second entry point taking PGRAPH state instead of D3D8 shader objects. Both
already consume the NV2A forms; what they lack is a caller that speaks
registers rather than handles.

At this point `nv2a_pb_exec.c`'s CPU rasteriser stops being the renderer and
becomes what it should be: a reference oracle and a last-resort fallback.

### L4 · Guest memory coherency — 2–3 weeks

The part that is easy to forget until it dominates the profile. An LLE GPU
reads textures and vertices out of guest RAM, and the guest rewrites them
without announcing it.

Start with **hash-on-bind** — `src/hle` already does exactly this, caching
textures "by VA + data + format + size". It is simple, it is correct, and it
costs a hash of bytes you were going to read anyway.

Move to write-watch (`GetWriteWatch` on Windows; `userfaultfd` or
`mprotect` + `SIGSEGV` on POSIX) **only if measurement says to**. `CLAUDE.md`
names write-watch traps as one of the places emulator frame time actually goes;
reproducing that by default would undo the reason for doing any of this. Note
also that planned change #1 (base+offset memory model) changes what is
available here, so L4 should follow it, not precede it.

### L5 · The fallback switch — 2 weeks

Per title, and ideally per draw: HLE by default, LLE where the signature
database cannot reach.

The concrete first target is named in `shadow-mode.md`: Burnout 2 fills its own
push buffers through `BeginPush` at two call sites, and those draws currently
reach no replacement at all. Decoding that content and routing it to the *same*
RHI as the HLE draws around it, in the same frame, on the same render target,
is the proof that the two frontends have genuinely converged.

---

## 6. What to do first, if the goal is frames per second

Three of these need no Vulkan, no LLE, and no refactor. They are available in
the current D3D11 path and they transfer to every backend afterwards.

| # | Change | Where | Why |
|---|---|---|---|
| 1 | Ring-buffer the indexed UP draws | `d3d8_device.c`, `dev_DrawIndexedPrimitiveUP` | Up to ~1,800 buffer create/destroy pairs per frame today (F4) |
| 2 | Real pipeline/state cache | `d3d8_states.c` | Currently destroys and recreates a state object per alternation (F3) |
| 3 | Persistent on-disk shader cache | `d3d8_combiners.c`, `d3d8_vsh.c` | 128 + 64 entries, in memory, thrown away at exit |
| 4 | Batch draws by state group | above the RHI | 690–910 draws/frame, each one re-applying everything |
| 5 | Everything in V4 | Vulkan backend | Needs V1–V3 first |

Measure before and after with the same capture, replayed. That is what the
replay tool is for and it removes the "was the game in the same place" problem
entirely.

---

## 7. Test strategy — the gap that will hurt most

**No CI job renders anything, and nothing compares an image.** With one backend
that is survivable. With two backends and two frontends it is not: a silent
divergence in the Vulkan path or the LLE path will be found by a person looking
at a screen, weeks later, or not at all.

Captures cannot fill this gap on their own, because a capture contains the
title's own textures and vertices and is therefore game content that must never
be committed (`.gitignore` covers `*.d3dcap`, correctly).

So: **extend the synthetic capture generator.** `tests/d3d8_capture` already
round-trips a synthetic capture with no game data. Extend it to emit a capture
that exercises combiners, vertex programs, render targets and a representative
spread of texture formats, and add a CI job that replays it on every available
backend and compares against a committed reference image. Synthetic pixels are
not game content.

That single test is worth more than any other piece of infrastructure named in
this document, and it is a few days of work.

Two more, cheap:

- A CI `grep` enforcing the `host_*` rule (F8) — nothing in `src/hle` calls a
  device vtable entry or a `d3d8_vsh_*` / `d3d8_combiners_*` setter directly.
  Extend it to the LLE frontend once that exists.
- A documented procedure for regenerating the local capture corpus after every
  format version bump, since the version is checked exactly.

---

## 8. Risks

| Risk | Consequence | Mitigation |
|---|---|---|
| Vulkan added as a third frontend, like `d3d8_gl.c` | Three divergent copies of 12k lines; permanent | V0/V1 before any Vulkan code is written. Non-negotiable. |
| Refactor changes pixels while claiming not to | Silent regressions blamed on the new backend later | Capture before, replay after, diff. Every phase. |
| Winding/Y-flip fixed twice | Inverted culling visible only on two-sided geometry | One compensation, in the backend, validated against `d3d8_states.c`'s existing note |
| LLE reintroduced as a translator, not a state model | Same failure as the September 2026 removal | L2 before L3, and L2 produces state, not calls |
| Write-watch coherency by default | Reproduces the cost the project exists to avoid | Hash-on-bind first; write-watch only on evidence |
| Three windows, two frontends | Swapchain ownership conflict | Single presentation owner, decided in V1 (F7) |
| CPU and GPU vertex programs disagree | Divergence with no fault, the hardest class in this project | Pick one per frame; `nv2a_vsh.c`'s CPU path becomes an oracle, not a fallback |
| DXC runtime dependency (~18 MB) | Ship size, and a stall on first use of each variant | Offline-compile fixed shaders; persistent cache; async compile |

---

## 9. Sizing

Wide error bars — these are shapes, not estimates, and V2 in particular depends
heavily on how much of the capture corpus turns out to exercise paths nobody
has looked at.

| Phase | | Depends on | Rough |
|---|---|---|---|
| V0 | Remove `DXGI_FORMAT` above the backend | — | 1 wk |
| V1 | Extract RHI, port D3D11, fix F3/F4 | V0 | 3–4 wk |
| V2 | Vulkan backend to replay parity | V1 | 4–6 wk |
| V3 | Default on Linux, delete `d3d8_gl.c`, runtime selection | V2 | 1 wk |
| V4 | Vulkan-specific performance work | V3 | 3–4 wk + ongoing |
| L0 | Per-title method inventory | — | days |
| L1 | DMA object resolution | L0 | 1–2 wk |
| L2 | PGRAPH state model | L1 | 3–4 wk |
| L3 | LLE onto the RHI | L2, V1 | 4–6 wk |
| L4 | Guest memory coherency | L3, memory model rework | 2–3 wk |
| L5 | HLE/LLE fallback switch | L3 | 2 wk |
| T1 | Synthetic cross-backend image test in CI | V0 | days |

Critical path to "Vulkan renders a title on Linux": **V0 → V1 → V2 → V3**,
roughly 9–15 weeks, with T1 running alongside from the start.

Critical path to "full control over the GPU": **L0 → L1 → L2 → L3**, which
cannot finish before V1 does.

---

## 10. The one-paragraph recommendation

Do not write Vulkan code yet. Spend the first month removing `DXGI_FORMAT` from
everything above the backend and extracting the RHI that `hle_d3d8_record.h`
has already half-designed, keeping D3D11 behind it and proving pixel-identity
with the capture harness that already exists. That single refactor makes the
Vulkan backend a contained piece of work brought up against a replay tool with
no game running, makes the LLE path a second frontend rather than a second
renderer, makes `d3d8_gl.c` deletable, and makes the three performance problems
in §6 fixable in passing. Everything asked for here — full Vulkan, full control,
highest performance — routes through that one object, and nothing else can start
until it exists.
