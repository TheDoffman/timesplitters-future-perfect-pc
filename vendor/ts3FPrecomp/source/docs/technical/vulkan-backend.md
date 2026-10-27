# A Vulkan backend for `src/d3d`

**Status: analysis, September 2026. No code written.** CLAUDE.md records the
decision ("Backend decision (Sep 2026): Vulkan, for portability — Linux, Steam
Deck, Android") and two of its costs (the Y-flip, the winding inversion). This
document is the work behind that one paragraph: where the seam goes, what
survives, what is genuinely new, what the frame costs, and what order to do it
in.

Everything numeric here was measured against the tree at
`docs/vulkan-backend-analysis` (off `feat/upstream-lifter-fixes`, at
`13afd4a`). Line counts and API inventories are reproducible with the commands
in the appendix.

---

## 1. The finding that decides the design

`src/d3d` is 11.3k lines of C. **814 of them touch Direct3D 11 or DXGI — 7%.**
The rest is Xbox knowledge: format tables, Morton swizzle, mip chains, P8
palette baking, cube and volume layout, NV2A microcode translation, register
combiner translation, FVF parsing.

| File | Lines | Lines touching D3D11/DXGI | |
|---|---:|---:|---|
| `d3d8_resources.c` | 2,467 | 182 | 7% |
| `d3d8_device.c` | 1,759 | 141 | 8% |
| `d3d8_vsh.c` | 1,512 | 95 | 6% |
| `d3d8_combiners.c` | 1,284 | 30 | **2%** |
| `d3d8_shaders.c` | 1,207 | 70 | 6% |
| `d3d8_states.c` | 435 | 108 | 25% |
| `d3d8_overlay.c` | 319 | 86 | 27% |
| `d3d8_screencopy.c` | 243 | 77 | 32% |
| `d3d8_internal.h` | 353 | 25 | 7% |
| headers, swizzle, FVF | 2,332 | 0 | — |

And the API surface behind those 814 lines is small: **58 distinct
`ID3D11*`/`IDXGI*` entry points in the whole project.** Not 58 calls — 58
distinct functions. Buffer/texture creation, map/unmap, five state-object
creates, the IA/VS/PS/RS/OM setters, `Draw`, `DrawIndexed`, `Clear*View`,
`ResolveSubresource`, `CopySubresourceRegion`, `Present`, `GetBuffer`.

Two corollaries worth stating plainly:

- **`src/hle`, `src/video` and `src/replay` contain zero references to
  D3D11 or DXGI.** Verified by grep. The graphics API is already confined to
  one directory; nothing above it knows which API is underneath.
- **The two shader translators are 94% and 98% pure string emission.** In
  `d3d8_combiners.c` the D3D11 references are 30 lines, all past line 900 —
  the compile/cache/bind tail. In `d3d8_vsh.c` they cluster at 700-800 (a
  `DXGI_FORMAT` used as a vertex-attribute enum) and past 950. CLAUDE.md's
  "~4k lines of translation survive the move" is not optimism; it is closer to
  4.3k and the coupling is at the end of the file where it can be cut off.

So the Vulkan backend is not a rewrite of `src/d3d`. It is a replacement for
7% of it, plus new code for the things D3D11 did implicitly.

---

## 2. Where to put the seam

Three candidates exist. Two of them are already in the tree.

### Seam A — the COM vtable (58 `IDirect3DDevice8` entries)

This is what the POSIX build does today. `src/d3d/CMakeLists.txt` selects
`d3d8_gl.c` on non-Windows: a second, independent implementation of the whole
Xbox `IDirect3DDevice8` interface, on OpenGL 3.3 via SDL2 and libepoxy.

**The in-tree evidence that this seam is wrong is `d3d8_gl.c` itself.** It is
1,121 lines against the Windows path's 9,226. Its own header comment lists what
it does not do — "real texture-format mapping, FVF-driven input layouts beyond
pos/diffuse/UV, Xbox VSH bytecode → GLSL, register-combiner pixel programs →
GLSL fragment" — and none of those have been done since. It has one fixed GLSL
program, `surfaces are minimal stubs in the first cut`, and `E_NOTIMPL` where
render targets should be. It cannot draw a title. Every one of the format,
swizzle, combiner and microcode fixes logged in CLAUDE.md since landed on the
Windows path only.

Duplicating at seam A duplicates 120 format cases, the swizzle header, P8
baking, cube/volume layout, and both shader translators. It is how you get a
backend that is permanently a year behind.

### Seam B — the 24 `host_*` wrappers (`src/hle/hle_d3d8_record.h`)

This is already a real, enforced, abstract renderer interface, and almost
nobody notices. `docs/technical/shadow-mode.md` states the rule: *"Nothing in
`src/hle` calls a device vtable entry or a `d3d8_vsh_*` / `d3d8_combiners_*`
setter directly."* Everything a title's graphics do reaches the renderer
through 24 functions:

```
host_Clear                  host_SetRenderTarget         host_vsh_create_shader
host_Swap                   host_CreateDepthStencilSurface  host_vsh_delete_shader
host_SetRenderState         host_CreateTexture           host_vsh_same_microcode
host_SetTextureStageState   host_CreateCubeTexture       host_vsh_set_constant
host_SetTransform           host_LockRect                host_vsh_set_declaration
host_SetViewport            host_UnlockRect              host_vsh_set_screenspace
host_SetTexture             host_DrawPrimitiveUP         host_vsh_set_vertex_data
host_SetVertexShader        host_DrawIndexedPrimitiveUP  host_combiners_set_pixel_shader
```

And `src/hle/d3d8_capture.h`'s 25 chunk kinds are that interface serialised —
by construction, since capture version 2 moved the boundary here precisely so
"the capture holds exactly what `src/hle` handed to `src/d3d`".

Seam B is the right place to **test** a backend. It is the wrong place to
**implement** one, for the same reason as seam A: below it sit the format
tables and the shader generators, and a Vulkan backend written at seam B
reimplements all of them.

### Seam C — an internal RHI inside `src/d3d` — **recommended**

Cut the 814 lines out into a narrow interface and let D3D11 and Vulkan both
implement it. Everything above the cut — the COM layer, format conversion,
swizzle, P8, cube/volume, FVF, NV2A microcode → HLSL, combiners → HLSL — is
written once and shared.

The reason this is cheap here, and is not cheap in most projects that try it,
is that **`src/d3d` is already shaped like a deferred renderer.** It does not
set GPU state when the title sets D3D8 state; it records into
`d3d8_render_state[]` and resolves at draw time:

- `d3d8_states.c` already computes `hash_blend_states()` and
  `hash_raster_states()` and rebuilds state objects only when they change.
- `d3d8_combiners.c` already hashes the full `NV2ACombinerState` and caches
  128 compiled shaders against it, because "most Xbox games use fewer than 20
  unique combiner configurations".
- `d3d8_vsh.c` already hashes microcode to reuse a compiled program.
- `d3d8_shaders.c` already computes a *texture signature* to select a
  fixed-function pixel shader variant.

A Vulkan pipeline key is the union of hashes this code already computes. The
deferred-resolve model that makes Vulkan awkward to bolt onto an immediate-mode
front end is the model `src/d3d` is already written in.

### What about DXVK? — less than it first appears

An earlier draft of this document floated DXVK as a way to prove the Linux
story before writing any Vulkan. **That was wrong, and the reason matters.**

DXVK supplies `d3d11.dll` *to Wine*. There is no native-Linux D3D11 for the
existing backend to run on, so "run it under DXVK" means running the
recompiled Windows executable under Wine or Proton — where **Wine provides
Win32 and `src/platform/win32_compat.c` is never entered at all.** The POSIX
memory mapping, the POSIX threading, the POSIX file layer: none of it runs.
It proves the title works on Linux GPU drivers. It proves nothing about the
port.

What it is still good for, narrowly: a performance and driver reference point,
and a second opinion on a Vulkan rendering bug ("does it also happen through
DXVK?"). It does not reach Android, and it keeps the D3DCompile/FXC dependency
in the form of Wine's `d3dcompiler`, which is not the same compiler.

**The genuinely cheap Linux probe is phase 1 below**: build the runtime
natively, supply the two missing POSIX host backends, and boot a title with
`RECOMP_HLE_D3D8=off`. That exercises exactly the shims DXVK bypasses, needs
no renderer, and is the project's own documented bring-up order — *"get to a
black screen with no crashes before caring about rendering"* — applied to a
new host.

---

## 3. The RHI

Roughly 45 entry points. Sketch, to be read as shape rather than as final
signatures:

```c
/* devices and frames */
rhi_device_create(window, &desc)      rhi_frame_begin()
rhi_device_destroy()                  rhi_frame_end(present_interval)
rhi_backbuffer_size(&w, &h)           rhi_wait_idle()

/* resources */
rhi_buffer_create(size, usage)        rhi_image_create(&desc)
rhi_buffer_destroy()                  rhi_image_destroy()
rhi_buffer_upload(buf, off, p, n)     rhi_image_upload(img, sub, p, pitch, rows)
rhi_ring_alloc(ring, n, &off)         rhi_image_readback(img, sub, dst, pitch)
                                      rhi_image_resolve(dst, src)
                                      rhi_image_copy_region(...)

/* shaders and pipelines */
rhi_shader_compile(hlsl, stage, &mod) rhi_pipeline_get(&key)   /* cached */
rhi_shader_destroy(mod)               rhi_sampler_get(&desc)   /* cached */

/* draw */
rhi_set_render_target(color, depth, level, face)
rhi_set_viewport(&vp)   rhi_set_scissor(&rect)   rhi_clear(flags, rects, ...)
rhi_bind_vertex_buffer(buf, off, stride)   rhi_bind_index_buffer(buf, off, fmt)
rhi_bind_uniform(stage, slot, buf, off, size)
rhi_bind_texture(slot, img)   rhi_bind_sampler(slot, smp)
rhi_draw(topology, count, first)
rhi_draw_indexed(topology, count, first, base_vertex)
```

Two design rules make the difference between an RHI that suits Vulkan and one
that fights it:

1. **Pipeline state is a key, not a set of independent objects.** D3D11's
   `OMSetBlendState` / `OMSetDepthStencilState` / `RSSetState` become fields of
   an `RhiPipelineKey` resolved once per draw. `d3d8_states_apply()` becomes
   "fill the key's blend/depth/raster fields from the render state array" and
   loses every `ID3D11Device_Create*State` call.
2. **The D3D11 adapter is written second, not first.** If the RHI is derived
   from what D3D11 does, Vulkan inherits D3D11's implicit barriers, implicit
   render passes and immediate context and has to fake all three. Derive it
   from what Vulkan needs; D3D11 then implements `rhi_pipeline_get` by pulling
   four state objects out of a cache, which is what it does today anyway.

---

## 4. The Vulkan backend proper

### 4.1 Target a modern core profile

Target **Vulkan 1.3 core**, and use three features that collapse most of the
difficulty:

| Feature | What it removes |
|---|---|
| `VK_KHR_dynamic_rendering` (1.3 core) | `VkRenderPass` and `VkFramebuffer` objects entirely. `SetRenderTarget` becomes `vkCmdBeginRendering`. No render-pass compatibility rules, no framebuffer cache keyed on attachment sets. |
| Extended dynamic state 1 & 2 (1.3 core) | Cull mode, front face, depth test/write/compare, stencil op and primitive topology leave the pipeline key. What remains is the shader pair, the vertex layout, the blend state and the attachment formats. |
| `VK_KHR_push_descriptor` | Descriptor pools, per-frame set allocation and set lifetime tracking. The binding set here is tiny (below), so push descriptors bind everything per draw with no bookkeeping. |

Vulkan 1.3 is available on every current desktop driver, on the Steam Deck, and
on Android 13+ devices with Adreno 6xx/7xx and Mali-G. Push descriptor is not
1.3 core but is near-universal; keep a descriptor-pool fallback behind the same
RHI call, about 150 lines.

If the 1.3 floor turns out to exclude a target that matters, the fallback is a
render-pass cache and a fatter pipeline key — several hundred lines and a
meaningfully worse bring-up. Decide this in phase 0, not phase 4.

### 4.2 The binding model is already small

Every constant buffer, texture and sampler slot the renderer uses today:

| Kind | Slots in use | What |
|---|---|---|
| VS constant buffers | b0, b1, b2, b3, b7 | b0 `TransformCB`; **b1 is `LightingCB` on the fixed-function path and `VSH_Constants` (192 × float4) on the programmable one**; b2 `VSH_Screenspace`; b3 `VSH_VertexData`; b7 overlay |
| PS constant buffers | b0, b7 | b0 `PixelCB` or `CombinerCB`; b7 overlay / screencopy |
| Shader resources | t0-t3, t8, t9 | four texture stages; t8 overlay glyph bitmap; t9 screencopy back buffer |
| Samplers | s0-s3 | one per texture stage |

Five uniform buffers, six sampled images, four samplers. One descriptor set
layout covers all of it with room to spare on every implementation's limits.

The b1 collision is the one thing to notice: `d3d8_shaders.c:1097` binds
`{ TransformCB, LightingCB }` at slots 0-1, and `d3d8_vsh.c:1481` binds
`VSH_Constants` at slot 1. They are mutually exclusive paths, so a single
binding declared at the larger size (3,072 bytes) serves both — but a Vulkan
validation layer will complain loudly if the fixed-function path leaves a
3 KB binding pointing at a 256-byte buffer. Bind the full range from a ring
allocation in both paths.

### 4.3 HLSL → SPIR-V through DXC

CLAUDE.md is explicit: *do not rewrite the generators to GLSL.* DXC is the only
path that honours that, and it is the right one.

- `D3DCompile(..., "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3)` becomes
  `IDxcCompiler3::Compile` with `-spirv -T ps_6_0 -E main -O3`. Four call sites
  in `d3d8_combiners.c`, `d3d8_vsh.c`, `d3d8_shaders.c` (×2), plus the
  `vs_4_0`/`ps_4_0` pairs in `d3d8_overlay.c` and `d3d8_screencopy.c`.
- **Binding shifts are mandatory.** HLSL's `b`, `t` and `s` register spaces are
  independent; SPIR-V's `binding` is one space. Without shifts, `cbuffer ...
  register(b0)` and `Texture2D ... register(t0)` both become binding 0 and the
  module is invalid. Use:
  ```
  -fvk-b-shift 0 0    b0..b7  -> bindings 0..7
  -fvk-t-shift 16 0   t0..t9  -> bindings 16..25
  -fvk-s-shift 32 0   s0..s3  -> bindings 32..35
  ```
  These three constants are the only new convention the shader generators need
  to be aware of, and they need to be aware of it only in the sense that the
  numbers must match the descriptor set layout. Put them in one header next to
  the layout, not in six command lines.
- **Do not also pass `-fvk-invert-y`.** It is an alternative to the negative
  viewport height in §4.4, not a companion to it. Using both flips twice and
  the bug presents as "everything is fine except it is upside down", which is
  indistinguishable from having neither. Prefer the viewport: it is one place,
  at draw time, visible at the call site, and it does not have to be re-decided
  for each new generated shader.
- **DXC is stricter than FXC.** HLSL 2021 tightens implicit truncation and
  operator overloads. The generated code is simple — arithmetic, `saturate`,
  `lerp`, `Sample`, `Load` — so the risk is low, but budget a pass of fixing
  generator output against DXC's diagnostics. Compile every generator's output
  offline first (`tests/nv2a_vsh_hlsl` already exists and already compiles the
  microcode decoder without the kernel layer; extend it).
- **Shipping DXC is a real cost.** `libdxcompiler` is ~18 MB, the same figure
  `docs/technical/ms-fusion-recompiler.md` records for `dxcompiler.dll`.
  Runtime compilation is not optional — combiner and vertex programs are
  generated from the title's own data while it runs — so the library ships.
  On desktop that is noise. On Android it is a third of a reasonable APK
  before any game data, and it is the single strongest argument for the
  SPIR-V disk cache below.

### 4.4 The Y-flip and the winding, precisely

CLAUDE.md names both and warns about one. Here is the whole of it.

**Clip space.** D3D and Vulkan both use a `[0, 1]` depth range. **No depth
remap is needed.** This is worth stating because it is the first thing everyone
changes, and it is the OpenGL problem, not the Vulkan one.

What does differ is the framebuffer Y direction: a D3D-authored shader writing
`SV_Position` renders vertically mirrored under Vulkan's default viewport. The
fix is a negative-height viewport, core since Vulkan 1.1:

```c
vp.y      = (float)height;
vp.height = -(float)height;
```

**The winding.** A negative viewport height inverts the effective triangle
winding. And `src/d3d` already carries one hand-annotated inversion, at
`d3d8_states.c:214-221`:

```c
case D3DCULL_CW:   rd.CullMode = D3D11_CULL_FRONT; break;  /* D3D8 CW = cull front in D3D11 convention */
...
rd.FrontCounterClockwise = FALSE;
```

CLAUDE.md: *"note `d3d8_states.c` already carries one hand-annotated winding
fix, so do not stack a second."* Concretely, the Vulkan backend must:

- **keep** the `D3DCULL_CW → cull front face` / `D3DCULL_CCW → cull back face`
  mapping exactly as it is — that inversion belongs to the D3D8 semantics and
  is API-independent;
- set `frontFace = VK_FRONT_FACE_CLOCKWISE`, which is what
  `FrontCounterClockwise = FALSE` means;
- and **do not invert it.**

> **Corrected 30 Sep 2026, by measurement.** This section originally said to
> invert `frontFace` a second time "to pay for the negative viewport height".
> That is wrong. The negative height makes the framebuffer image the one D3D
> draws, so every triangle winds on the screen exactly as it does under D3D11,
> and Vulkan decides facing in those same framebuffer coordinates (y down): its
> `CLOCKWISE` is D3D's clockwise. The inversion only applies against an
> *unflipped* viewport. With it, TimeSplitters 2's tunnel walls were culled away
> and its frames matched D3D11 at 21.8 dB; without it, 54 dB. The mapping is
> `raster_front_face()` in `src/d3d/rhi_vulkan.c`.

Anything else and geometry culls inside-out in a way that looks like a depth
bug and gets debugged as one for a day -- which is what the inversion did.

### 4.5 Pipelines

Key, after extended dynamic state has taken cull/front-face/depth/stencil/
topology out of it:

```
vertex shader module      (VSH microcode hash, or FFP variant)
fragment shader module    (combiner state hash, or FFP texture signature)
vertex input layout       (D3D8VshInput[] or the FVF-derived layout)
blend state               (hash_blend_states() — already computed)
color attachment format, depth attachment format, sample count
```

Expected population: the level in TimeSplitters 2 draws ~356 indexed + ~32
non-indexed a frame; Burnout 2 draws 690-910 in a race frame. Against that,
"most Xbox games use fewer than 20 unique combiner configurations" and the VSH
slot table holds `NV2A_VS_MAX_SLOTS` = 256. Distinct pipelines will be in the low
hundreds. A flat open-addressed hash table is sufficient; no need for anything
cleverer.

**Persist two caches beside the executable**, in the same place the log and the
F11 BMPs go (`setup_output` in the template):

- a `VkPipelineCache` blob, and
- a SPIR-V cache keyed by the generator's existing microcode / combiner-state
  hash.

Without them, the first appearance of every new shader in a level is a DXC
invocation plus a pipeline compile, mid-frame. That is exactly the "title
hangs after drawing" shape CLAUDE.md warns about diagnosing wrongly — it will
look like a stall in lifted code and it will not be one. Build the caches in
phase 3, not after the first bug report.

### 4.6 Buffers and the UP rings

`d3d8_device.c:851-931` already runs the right discipline: two 4 MB persistent
dynamic rings, `WRITE_NO_OVERWRITE` while there is room, `WRITE_DISCARD` at the
wrap, added because the indexed UP draw "used to create and release two
immutable buffers per call, ~700 a frame in TimeSplitters 2's level".

Vulkan wants the same shape with the fence made explicit: one ring per
frame-in-flight (three is right), each host-visible and coherent, each with a
`VkFence` or timeline value that must be waited on before the ring is reused.
4 MB × 2 rings × 3 frames = 24 MB, which is nothing.

This is one of the few places the Vulkan version is *simpler* than the D3D11
one. `WRITE_NO_OVERWRITE` is a promise the application makes to the driver and
which nothing checks; the per-frame ring makes the promise structurally true.

The same ring serves the constant-buffer traffic, with dynamic-offset uniform
bindings replacing the per-draw `Map(WRITE_DISCARD)` that
`d3d8_shaders_prepare_draw` and `d3d8_vsh_prepare_draw` do today.

### 4.7 Barriers and image layouts — the genuinely new work

This is the only part of the backend with no D3D11 analogue at all, and it is
where the first class of bugs will live. Three places need it:

1. **Texture upload.** Staging buffer → `vkCmdCopyBufferToImage` → shader read.
   `UNDEFINED` → `TRANSFER_DST_OPTIMAL` → `SHADER_READ_ONLY_OPTIMAL`. D3D11's
   `UpdateSubresource` and `Map` hid all of it.
2. **Render-target textures sampled later.** A title renders into a texture
   level and then binds it as a texture: `COLOR_ATTACHMENT_OPTIMAL` →
   `SHADER_READ_ONLY_OPTIMAL`, and back if it renders into it again. The
   tracking already exists in a usable form — shadow mode knows which textures
   are render targets (`d3d8_render_texture`, `d3d8_render_cube`,
   `d3d8_is_framebuffer`) and the surface-parent rule from
   `shadow-mode.md` already establishes which host object a guest surface
   names.
3. **Screen readback.** `d3d8_screencopy.c` copies the finished back buffer
   into a texture — three times a frame in TimeSplitters 2, and worth 62% of
   the picture's brightness. Under Vulkan that is a present-layout →
   transfer/shader-read transition inside the frame.

Design it as one `VkImageLayout` field per image plus a `transition()` helper
that no-ops when the layout already matches, called from `rhi_bind_texture`,
`rhi_set_render_target` and the upload path. Roughly 300-400 lines. **Run the
validation layers on by default in debug builds** — they catch essentially all
of this class and nothing else will.

### 4.8 Render passes

With dynamic rendering, `vkCmdBeginRendering` on each render-target change. The
naive mapping — begin and end around every draw — is correct and is how the
backend should start, but it forfeits the whole point on tile-based hardware
(Android; the Steam Deck's RDNA2 cares less).

The improvement, once correct: batch draws between render-target changes into
one rendering scope, and flush on a target change, a readback, or present.
A full-target `Clear` at a scope start then maps to `loadOp = VK_ATTACHMENT_
LOAD_OP_CLEAR`, which is the fast path; a partial `Clear` with rects maps to
`vkCmdClearAttachments` inside the scope. TimeSplitters 2's three full-screen
passes a frame make this worth having, but it is a phase-4 optimisation, not a
bring-up requirement.

### 4.9 Formats

Do **not** write a second Xbox-format table. Route through the existing one:

```
D3DFORMAT --(d3d8_to_dxgi_format, 120 cases, unchanged)--> DXGI_FORMAT --(new, ~45 entries)--> VkFormat
```

Keeping DXGI as the intermediate has a second payoff: `d3d8_capture.h` stores
`dxgi_format` in `D3D8CapVsInput`, so **the capture format needs no version
bump.** `D3D8CAP_VERSION` stays at 5 and every existing capture replays into
the Vulkan backend.

The mappings that are not mechanical:

| DXGI | Vulkan | Note |
|---|---|---|
| `B8G8R8X8_UNORM` | `VK_FORMAT_B8G8R8A8_UNORM` | No X variant. Force alpha with `VK_COMPONENT_SWIZZLE_ONE` on the image view. |
| `B5G6R5_UNORM` | `VK_FORMAT_R5G6B5_UNORM_PACK16` | **The names disagree and the bit layouts agree.** DXGI names packed formats high-bit-first, Vulkan low-bit-first. Getting this backwards swaps red and blue on every 16-bit texture and looks like a swizzle bug in the upload path. |
| `B5G5R5A1_UNORM` | `VK_FORMAT_A1R5G5B5_UNORM_PACK16` | Same reversal. |
| `B4G4R4A4_UNORM` | `VK_FORMAT_A4R4G4B4_UNORM_PACK16` | Needs `VK_EXT_4444_formats` (1.3 core). If a target lacks it, expand to `B8G8R8A8` at upload — `d3d8_convert_linear_pixels` already does exactly this kind of expansion for P8 and YUY2. |
| `D24_UNORM_S8_UINT` | `VK_FORMAT_D24_UNORM_S8_UINT` | **Optional in Vulkan.** AMD does not support it. Query at device creation and fall back to `VK_FORMAT_D32_SFLOAT_S8_UINT`; the format table must be built per-device, not statically. |
| `BC1`-`BC5` | `VK_FORMAT_BC*` | Universal on desktop, **not guaranteed on Android.** See below. |

**DXT on Android is a real blocker, and it is not this backend's to solve
cheaply.** Xbox titles are DXT-heavy and Adreno/Mali advertise ETC2 and ASTC,
not BC. Two options, both with costs: decompress at upload (the decoder already
exists — `d3d8_dxt_decode_texel` is already exported from `d3d8_resources.c`)
at roughly 4-8× the texture memory, or transcode to ASTC at upload, which costs
CPU time at load. Record this as an Android-phase item with a decision still
open; do not let it shape the desktop design.

### 4.10 Present, pacing and windowing

`IDXGISwapChain::Present(interval, 0)` → `vkQueuePresentKHR`, with the present
mode chosen from the interval. **This is a trap with a documented precedent in
the tree.** `d3d8_device.c:1530-1534`:

> A device run beside the title's own D3D8 (`hle_d3d8.c` shadow mode) must not
> block: the title paces its loader on frames presented, so a vsync wait here
> throttled Burnout 2 from about 136 frames a second to 27 and cut how far a
> run got by two thirds.

`VK_PRESENT_MODE_FIFO_KHR` is the only mode Vulkan guarantees, and it blocks.
So `xbox_D3D8SetPresentInterval(0)` must resolve to `MAILBOX`, else
`IMMEDIATE`, else `FIFO_RELAXED`, and only fall to `FIFO` when nothing else
exists — at which point the run log should say so, because that is the
Burnout 2 regression reappearing by a different route.

Two more items D3D11 gave away free:

- **Swapchain recreation.** Resize, minimise, and monitor change all produce
  `VK_ERROR_OUT_OF_DATE_KHR` / `VK_SUBOPTIMAL_KHR`, which every `vkAcquire
  NextImageKHR` and `vkQueuePresentKHR` must handle. DXGI handled this. ~150
  lines and a path through the frame loop.
- **Windowing.** The D3D11 path uses a raw `HWND` and pumps `PeekMessageA`
  inside `dev_Swap` and `d3d8_PresentFrame`. SDL2 is already a POSIX
  dependency and has `SDL_Vulkan_CreateSurface`, so SDL2 on every platform is
  the clean answer for the Vulkan backend. The coupling to untangle is that
  `win32_compat.c:1299` says the message pump exists for the *title's own*
  Win32 windows — it cannot simply be deleted on the Windows build. Expect the
  Windows Vulkan path to keep pumping messages even under SDL2.

Also needing ports, all `d3d8_*` callers rather than title code:

| Feature | Where | Note |
|---|---|---|
| F9 frame-rate overlay | `d3d8_overlay.c` | Renders text with GDI into a bitmap. Windows-only by construction; needs a portable text path (stb_truetype, or SDL_ttf) for Linux/Android. ~300 lines, and the natural moment to stop depending on GDI. |
| F11 frame capture | `d3d8_screencopy.c` + the BMP writer | Readback is `rhi_image_readback`. Straightforward. |
| FMV playback | `src/video/video_player.c` | Touches **no** D3D11 — it goes through `d3d8_CreateTextureImpl` and `d3d8_GetDevice`. Moves for free. Its Media Foundation decoder is a separate Windows dependency and a separate problem. |
| Flip gate / `RECOMP_FPS_CAP` | `src/hle` | Above the seam. Unaffected. |

---

## 5. The bring-up loop already exists

CLAUDE.md: *"frame capture and replay ... records one frame's host calls and
plays them back with no game running, so one frame can be drawn by two backends
and compared. That is the bring-up loop for a Vulkan backend."*

That is right, and it is the single largest reason this project is tractable.
A capture is deterministic where two runs of a title never land on the same
moment; it replays in seconds; it needs no guest memory, no recompiled title
and no game running. Every phase below has a pass/fail condition that is "this
capture replays and the image matches".

Three things stand between that and reality:

1. **`src/replay` is Windows-only.** `src/replay/CMakeLists.txt` says so in its
   header comment and links `user32`. It needs to build on Linux against the
   Vulkan backend. It already compiles `d3d8_capture.c` and `nv2a_vsh.c`
   directly rather than linking `xbox_hle`, so it has no kernel-layer
   dependency to untangle — this is a CMake change and an audit for Win32
   calls, not a port.
2. **A capture is game content and can never be committed** (CLAUDE.md, Legal;
   `.gitignore` covers `*.d3dcap`). So there is no golden-image corpus in CI,
   ever. What can be in CI is the existing synthetic round-trip
   (`tests/d3d8_capture`, already cross-platform) plus a new test that replays
   a synthetic capture through both backends and compares readbacks.
3. **CI needs a Vulkan implementation.** Mesa's **lavapipe** (`libvulkan_
   lavapipe.so` via `VK_ICD_FILENAMES`) runs headless on `ubuntu-24.04`,
   advertises Vulkan 1.3, and supports dynamic rendering and extended dynamic
   state. Verify push-descriptor support on it in phase 0; if it is missing,
   that is the reason the descriptor-pool fallback exists. This gives the
   project something it does not have today: **a renderer test that actually
   runs a GPU API in CI.**

The A/B discipline to hold to: for a given capture, replay through D3D11 and
through Vulkan, read both back, and diff. Not "does it look right" — the
frame-brightness investigation in `docs/technical/` is a standing record of how
expensive "looks right" is as an acceptance test.

---

## 6. What this does not do, and the honest risks

**It does not make anything faster.** `docs/technical/ts2-performance-plan.md`
is explicit, under *What this plan deliberately does not recommend*:

> **A Vulkan backend, for speed.** Nothing measured here is the backend's fault
> in a way a different API fixes; items 1, 2, 3 and 5 are the same mistakes in
> any API. Do Vulkan for portability, as CLAUDE.md says, after these.

That still holds and should be quoted at anyone who proposes this as a
performance project. The frame in TimeSplitters 2's level went from 12.4 ms to
5.0 ms through items 1-7, none of which were the graphics API. There is no
performance argument to fall back on if the Vulkan frame comes out slower, so
**keep D3D11 the default on Windows until Vulkan matches it on the same
capture**, and gate it on `RECOMP_D3D8_BACKEND=vulkan`.

**The strongest single recommendation in this document: delete `d3d8_gl.c` the
moment the Vulkan backend can replay a frame.** Two backends means two places
every future fix lands. Three means the third silently rots — and `d3d8_gl.c`
is the in-tree demonstration of exactly that, a 1,121-line skeleton that has
not tracked a single one of the format, combiner or microcode fixes CLAUDE.md
records. Removing it is also what turns the Linux CI job from "proves the build
compiles" into "proves the renderer works".

Ranked risks:

| Risk | Why | Mitigation |
|---|---|---|
| The phase-0 RHI extraction gets skipped | It produces no visible progress, and skipping it is how you end up with a fourth parallel implementation | Make phase 0's exit condition a byte-identical D3D11 replay. It is a refactor with a mechanical test. |
| Image layouts and barriers | The only genuinely new correctness burden; failure modes are corruption and device loss, not a clean error | Validation layers on by default in debug. One layout field per image, one helper. |
| The winding gets stacked | CLAUDE.md warns specifically; the symptom mimics a depth bug | §4.4. One inversion, on `frontFace`, with a comment naming what it pays for. |
| Shader compile hitches | DXC + pipeline compile on first appearance of a shader mid-level looks exactly like the lifted-code stall CLAUDE.md warns about misdiagnosing | Persistent SPIR-V and `VkPipelineCache` files beside the executable, built in phase 3. |
| DXC package size on Android | 18 MB before any game data | Named, not solved. The SPIR-V cache mitigates the runtime cost, not the size. |
| DXT unavailable on Android | Xbox titles are DXT-heavy; Adreno/Mali advertise ETC2/ASTC | Decision deferred to the Android phase. Decoder already exists. |
| Two backends to maintain | Every renderer fix lands twice | Delete `d3d8_gl.c`. Keep the RHI narrow. Make replay-through-both-backends the CI gate. |

---

## 6.5 What else stands between this and Linux / Android

The renderer is the reason this document exists, but it is not the only thing
in the way, and on Android it is not the biggest thing. Measured against the
tree, so that the Vulkan work is not mistaken for the whole port:

### Linux: four items, and the renderer is one

| Item | State |
|---|---|
| **Renderer** | This document. Real work. |
| **Memory model** | **Already shimmed, and better than CLAUDE.md's table says.** `src/platform/win32_compat.c` implements `CreateFileMappingA/W`, `MapViewOfFileEx`, `VirtualAlloc`, `VirtualProtect`, `CreateThread`, `QueryPerformanceCounter` and `AddVectoredExceptionHandler` on POSIX. `MapViewOfFileEx` uses `MAP_FIXED_NOREPLACE` and checks the returned address, with a long comment recording that bare `MAP_FIXED` silently unmapped live views and that `xbox_memory_layout.c` *depends* on a failed placement failing. Somebody has already debugged the hard part. What is untested is whether the 28 mirror views and the aperture layout actually place on Linux — nothing has ever linked a title there. |
| **Host audio output** | No POSIX implementation. `recomp_audio_output_*` (`src/hle/audio_output.h`) exists only as `audio_output_xaudio2.cpp`. |
| **Host input sampling** | No POSIX implementation of `recomp_input_host_sample` (`src/hle/input_host.h`). Note `src/input` itself *already* uses SDL2 on POSIX — it is only this one `src/hle` entry point missing. |
| **FMV** | `src/video` links Media Foundation, Windows-only, no POSIX path. |

`src/hle/CMakeLists.txt` already states the audio and input gaps and nominates
SDL2 for both: *"Until those exist the library compiles on POSIX, but a title
that links it there will be missing those symbols."* Everything in the
top-level `CMakeLists.txt` already builds in the Linux CI job. So the Linux
port is the renderer plus two SDL2 backends plus an FMV decision — the
renderer is roughly a quarter of it, and the other three-quarters are small
and already scoped in the build system.

### Android: the renderer is maybe a third of it

The host is ARM64, so **the recompiled x86 code itself has to compile and run
there**, and that is a bigger question than the graphics API.

The good news is measured: `templates/runtime/recomp_types.h` (1,315 lines, the
header every lifted title includes) was written for this. Its `xmmintrin.h`
include and its `_mm_cvttss_si32` fast path are both behind
`#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__x86_64__)`
with portable fallbacks; the MMX/SSE helpers are deliberately *"lane-wise C
rather than host intrinsics ... the header stays portable"*; atomics go through
`__sync_*` off MSVC. The lifter in `tools/recomp` emits no intrinsics and no
inline assembly at all.

The bad news is that CLAUDE.md's items **#1 memory model, #2 register model and
#3 x87 policy** all bite hardest on ARM64, and none of them is the renderer:

- **#2 register model** stops being an optimisation on ARM64. CLAUDE.md's own
  framing — *"ARM hosts (where x86-32 → ARM64 is no longer same-ISA and lifting
  genuinely pays)"* — is exactly the case where globals defeating aliasing
  analysis costs most, because there are 31 GPRs to allocate into rather than
  16 and no same-ISA free ride to fall back on.
- **#3 x87** is a correctness fork, not a performance one: `long double` is
  80-bit on x86 GCC/Clang and **128-bit on AArch64**. CLAUDE.md names this. The
  runtime already carries a per-thread x87 model, so what is missing is the
  policy decision, not the code.
- **#1 memory model**: the POSIX shim above works, but fixed-address placement
  of 32-bit guest VAs is more fragile under Android's allocator than under
  Linux's, and base+offset is what CLAUDE.md wants anyway.

Plus the renderer's own Android costs from §4.9 and §4.3: DXT decompression or
transcoding, and 18 MB of DXC.

### The scheduling consequence

CLAUDE.md puts items #1-#4 first and says why: *"The first four get baked into
generated code and are expensive to retrofit. Do these before any bulk
codegen."* The renderer is not in that category — it retrofits cheaply,
because nothing above `src/d3d` knows which API is underneath.

So if **Android** is the actual destination, Vulkan-before-#1-#3 is arguably
the wrong order: the renderer will still retrofit cheaply in a year, and the
register model and x87 policy will not. If **Linux and the Steam Deck** are the
destination, Vulkan-first is right — and phase 1 below proves the rest of the
Linux port before any Vulkan code exists, which is the cheap probe DXVK cannot
give you (§2).

---

## 7. Phases

Eight phases. Each has one pass condition a capture or a boot log can answer,
so "done" is never a judgement call. The Linux work that is *not* the renderer
runs as phase 1, in parallel with phase 0 and needing none of it — that is the
cheap probe §2 says DXVK cannot give you.

**Phase 0 — the seam, no Vulkan.** Extract `rhi.h` (~45 entry points) from the
814 D3D11-touching lines; rewire the six core files plus overlay and screencopy
through it; make pipeline state a *key* rather than four independent state
objects. D3D11 stays the only implementation, and its adapter is written
*second* so the RHI is derived from what Vulkan needs. Settle the Vulkan 1.3
floor (dynamic rendering, extended dynamic state, push descriptors) and stand
lavapipe up in the Linux CI job. *Pass: a captured frame replays through the
RHI'd D3D11 backend with a byte-identical readback.* This phase produces
nothing visible, which is why it is the one that gets skipped, and skipping it
is how the project acquires a fourth parallel renderer.

**Phase 1 — Linux boots a title with no renderer.** Runs in parallel with
phase 0. Write `recomp_audio_output_*` and `recomp_input_host_sample` for POSIX
(SDL2 — `src/input` already uses it there, so only the `src/hle` entry point is
missing), link a title on Linux with `RECOMP_HLE_D3D8=off`, and find out
whether `xbox_memory_layout.c`'s 28 mirror views and its apertures actually
place. Decide FMV: Media Foundation has no POSIX path. *Pass: a title reaches
its first `Swap` on Linux — CLAUDE.md's "black screen with no crashes", on a
new host.*

**Phase 2 — a Vulkan device on screen.** Instance, device, SDL2 surface,
swapchain, frames in flight, swapchain recreation on `OUT_OF_DATE`, present
mode from the interval (§4.10). Make `src/replay` build on Linux. Validation
layers on by default in debug. *Pass: a replayed capture's clear colour fills a
window on Linux, and the log names the present mode it chose.*

**Phase 3 — the fixed-function draw path.** Per-frame rings replacing the UP
rings, uncompressed textures with upload barriers, the per-device
`DXGI_FORMAT` → `VkFormat` table, the Y-flip and its single winding inversion
(§4.4), one pipeline key. *Pass: a replayed menu or loading frame matches the
D3D11 readback.*

**Phase 4 — the generated shaders.** DXC at six call sites, the three binding
shifts, one descriptor set layout, the `b1` collision, the microcode and
combiner programs, the persistent SPIR-V and `VkPipelineCache` files. Compile
every generator's output offline against DXC first, by extending
`tests/nv2a_vsh_hlsl`. *Pass: a replayed race or level frame matches.* This is
the phase that proves the "~4k lines survive" claim.

**Phase 5 — render targets, barriers, the rest of the frame.** One
`VkImageLayout` per image and one `transition()` helper; render-to-texture and
cube faces; `d3d8_screencopy.c`; batched rendering scopes with
`loadOp = CLEAR`. *Pass: TimeSplitters 2's three full-screen passes reproduce
and the frame brightness matches D3D11.*

**Phase 6 — a live title, then delete `d3d8_gl.c`.**
`RECOMP_D3D8_BACKEND=vulkan` beside D3D11 on Windows, a portable text path for
the F9 overlay (it is GDI today), F11 capture, the flip gate. Join with phase 1
for Linux. *Pass: TimeSplitters 2 plays on Vulkan on Windows within measuring
distance of D3D11, and plays on Linux.*

**Phase 7 — Android, and the two items that are not the renderer.** The lifted
output is already portable (§6.5). What is not settled is CLAUDE.md's item #2
(register model) and item #3 (x87 policy), and both bake into generated code,
so if Android is the destination they belong *before* phases 2-5, not after —
see §6.5's scheduling note. Plus DXT (decompress or transcode), 18 MB of DXC,
and touch input through `input_bindings.c`. *Pass: a title runs on an ARM64
device — not compiles, runs, with its physics and RNG matching the x86 build.*

---

## 8. Appendix — reproducing the numbers

```bash
# D3D11/DXGI-touching lines per file
for f in src/d3d/d3d8_*.c; do
  printf '%s %s %s\n' "$f" "$(wc -l < "$f")" \
    "$(grep -cE 'ID3D11|DXGI_|D3D11_|IDXGI|D3DCompile|ID3DBlob' "$f")"
done

# distinct D3D11/DXGI entry points in the whole project
grep -rohE '(ID3D11[A-Za-z]+|IDXGI[A-Za-z]+)_[A-Za-z0-9]+' src/ | sort -u | wc -l

# nothing above src/d3d touches the graphics API
grep -rc 'ID3D11\|D3D11_\|IDXGI' src/hle src/video src/replay --include=*.c

# the abstract renderer interface that already exists
grep -n 'host_' src/hle/hle_d3d8_record.h

# the binding slots in use
grep -n 'VSSetConstantBuffers\|PSSetConstantBuffers\|PSSetShaderResources\|PSSetSamplers' src/d3d/*.c

# the winding fix not to stack a second one onto
sed -n '205,225p' src/d3d/d3d8_states.c
```

## 9. Related reading in this repo

- `docs/technical/shadow-mode.md` — the `host_*` seam and its no-direct-vtable
  rule; the capture/replay loop this plan is built on.
- `src/hle/d3d8_capture.h` — the 25 chunk kinds; the serialised renderer
  interface, and why it moved to the host boundary in version 2.
- `docs/technical/ts2-performance-plan.md` — why this is not a performance
  project, with the measurements.
- `docs/technical/resolution-and-framerate.md`, `widescreen-and-resolution.md`
  — both already note that the Vulkan backend inherits their design points
  rather than their code.
- `src/d3d/README.md` — the format table and the D3D8→D3D11 feature matrix.
