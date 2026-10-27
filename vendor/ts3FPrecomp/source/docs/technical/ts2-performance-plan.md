# TimeSplitters 2: where the frame goes, and what to do about it

**Items 1-7 are done (19 Sep 2026); the results are at the end.** Everything
under "What was measured" is real and reproducible. The ranked plan was built
from those measurements plus code reading; the items marked *(not yet measured
in isolation)* had a mechanism identified in the source and a share of the
profile consistent with it, and were then done together and measured together.
Items 8-13 are still open (11 and 13, the TS2-specific ones, were done on the
bring-up branch: the title is lifted plain and has no unresolved calls).

Everything here is from one machine (Windows 10 19045, 20 logical cores,
Intel integrated GPU, `igd10um64xe.DLL`), Release build, shadow mode, lifted
**with** `--trace-all-entries`. The comparison lift without the trace hook was
not run.

---

## What was measured

Two runs, both `RECOMP_VBLANK=1 RECOMP_AC97_READY=1 RECOMP_HLE_D3D8=shadow
RECOMP_TRACE_BUDGET=0 RECOMP_FPS=5 RECOMP_SAMPLE=500`, the scripted menu path
from `second-title-bringup.md`, save folders cleared first, no other build
running. No `--profile`.

| Run | Length | Where it got to | Frame rate | stderr lines |
| --- | --- | --- | --- | --- |
| `trace_a` | 131 s, report every 30 s | Siberia at t≈72 s | front end 89-132 fps; **level 44-47 fps** | 64,202 |
| `trace_b` | 115 s, report every 20 s | never left the front end | 87-102 fps throughout | 7,802 |

`trace_b` took a different menu path despite the saves being cleared, so the
two are not a repeat of each other. That is itself worth recording: **the
scripted path is not deterministic**, and a fps comparison between two runs of
this script is only meaningful if both reach the same screen. Check the
`[HLE-D3D8] shadow:` draw counts before comparing anything.

### The level's frame, from `[HLE-D3D8] swap timing`

```
[HLE-D3D8] swap timing over 234 frames: gate wait 0.00 ms,
           title's Swap 0.00 ms, rest of frame 21.41 ms (per frame)
```

Five consecutive reports gave 21.23, 21.26, 21.41, 21.47, 21.70, 22.34,
22.56 ms. **Gate wait and the title's own `Swap` are both zero.** The whole
frame is the guest thread doing work between one `Swap` and the next. So:

- frame pacing is not the problem and the flip gate is not the answer here —
  there is nothing to pace, the title cannot reach 60 fps to be held at it;
- `nv2a_ack_thread`'s `Sleep(0)` spin is burning a core, but the title never
  waits on the fence in this state, so it is not adding latency to the frame.
  (It still competes for a core; on a 20-core machine that is invisible.)

### Draws per frame

From the differences between consecutive five-second `shadow:` lines in the
level: **≈356 indexed buffer draws + ≈32 non-indexed buffer draws per frame**,
and **zero skips of any kind** — no "program without layout", no declaration,
stride, primitive or failure skips. The 15% skipped draws in
`second-title-bringup.md` are gone in this lift. The front end in `trace_b`
draws ≈168 a frame.

### Sampler, level-weighted (`trace_a`, cumulative at 90 s)

The sampler's report is cumulative from process start, so this window is about
78% front end and 22% level. Guest main thread, 17,326 samples, 96.7% on-CPU,
87.2 s of CPU:

| Share | Category |
| --- | --- |
| 28.9% | lifted game code |
| 25.6% | OS (other) |
| 20.0% | host D3D11 / Intel driver |
| 10.6% | `d3d8_` (host renderer layer) |
| 9.1% | other |
| 2.3% | trace hook (`recomp_trace_enter`) |
| 2.2% | C runtime |
| 1.1% | `hle_` (HLE boundary) |
| 0.2% | kernel bridge |
| 0.1% | runtime (dispatch, icall) |

Leaf (exclusive), hottest first:

```
15.70%  igd10um64xe.DLL+0x77ddf          the Intel driver
 9.36%  NtWriteFile                      the runtime's own stderr logging
 7.43%  d3d8_combiners_get_shader        per-draw combiner cache lookup
 7.04%  sub_000A4C70                     lifted
 4.14%  RtlAcquireSRWLockExclusive  \    D3D11 device lock:
 3.09%  RtlReleaseSRWLockExclusive  /    resource creation on the draw path
 4.10%  level0_checksum                  texture change detection
 2.69%  D3D11CoreCreateDevice            (nearest symbol; d3d11 resource creation)
 2.33%  D3DReturnFailure1 [D3DCOMPILER]  runtime HLSL compilation
 2.19%  recomp_trace_enter               the trace hook
 2.04%  sub_001BC2A0                     lifted
 2.01%  RtlAllocateHeap
 1.73%  d3d8_vsh_prepare_draw            per-draw microcode hash
 0.99%  NtGdiDdDDIPresent                the host Present
 0.70%  getenv
   ... then a long tail of sub_ at <0.9% each
```

Inclusive (on the stack; unreliable through system DLLs, read as a hint):

```
88.43%  igd10um64xe.DLL+0x77ddf
54.77%  D3D11CoreCreateDevice [d3d11.dll]
28.11%  write [ucrtbase.dll]
20.52%  CreateDirect3D11SurfaceFromDXGISurface [d3d11.dll]
20.11%  dev_DrawPrimitiveUP
18.62%  host_DrawPrimitiveUP
16.91%  hle_d3d8_shadow_draw
13.89%  D3DReturnFailure1 [D3DCOMPILER_47.dll]
11.29%  d3d8_combiners_prepare_draw
```

### Sampler, front end only (`trace_b`, cumulative at 100 s)

Same shape with no level in it at all, which is the useful control: it says
these costs are **per draw**, not something the level uniquely triggers.
20,780 samples, 97.9% on-CPU, 96.7 s CPU.

| Share | Category |
| --- | --- |
| 30.7% | lifted game code |
| 21.6% | host D3D11 / Intel driver |
| 18.7% | OS (other) |
| 12.6% | `d3d8_` |
| 9.4% | other |
| 2.7% | C runtime |
| 2.5% | trace hook |
| 1.2% | `hle_` |

```
16.87%  igd10um64xe.DLL
 9.19%  d3d8_combiners_get_shader
 8.56%  sub_000A4C70
 4.46%  level0_checksum
 4.13%  RtlAcquireSRWLockExclusive
 3.86%  RtlReleaseSRWLockExclusive
 2.74%  D3D11CoreCreateDevice
 2.37%  recomp_trace_enter
 2.23%  d3d8_vsh_prepare_draw
 2.18%  D3DReturnFailure1 [D3DCOMPILER_47.dll]
 1.76%  NtWriteFile
 0.76%  hle_d3d8_shadow_apply_states
inclusive: 50.92% D3D11CoreCreateDevice, 27.13% dev_DrawPrimitiveUP
```

`NtWriteFile` drops from 9.36% to 1.76% between the two runs. That is the log
volume: **53,084 of `trace_a`'s 64,202 stderr lines are two lines printed on
every `D3DDevice_LoadVertexShaderProgram`** (26,542 calls in 131 s, ≈4-5 per
level frame), and the front end barely calls it.

### The headline

Both runs say the same thing, and it is not what the framing in `CLAUDE.md`
would predict for a title at 45 fps: **lifted code is under a third of the
guest thread and is spread over a long tail of `sub_` at well under 1% each.**
The register model (`CLAUDE.md` planned change 2) is not what these numbers
point at — same conclusion as the Burnout 2 investigation, now confirmed on a
real in-level frame rather than a front end.

What they do point at is that **the D3D8 HLE and the host renderer do
per-draw work that should be per-state-change or per-load**. Over half of the
guest thread's stack sits under D3D11 resource creation, in a frame that
creates no resources the title asked for.

---

## The plan, ranked

"Class" is A = xboxrecomp itself, every title benefits; B = specific to
TimeSplitters 2; C = a toolkit change TS2 happens to be the first to need.

| # | Change | Where | Class | Expected gain | Cost | Risk |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | Give the **indexed** UP draw a ring buffer, as the non-indexed one already has | `src/d3d/d3d8_device.c` `dev_DrawIndexedPrimitiveUP` | **A** | large — this is the bulk of the 54.8% inclusive under D3D11 resource creation | ~60 lines | low |
| 2 | **Cache sampler states** by descriptor hash | `src/d3d/d3d8_states.c` `d3d8_states_apply_sampler` | **A** | large — 4 `CreateSamplerState` + 4 `Release` per draw today | ~30 lines | low |
| 3 | Drop `c0`/`c1`/`final_c0`/`final_c1` from the **combiner shader cache key** | `src/d3d/d3d8_combiners.c` | **A** | large — should remove most of 7-9% `d3d8_combiners_get_shader` **and** most of 2.2-2.3% `D3DCOMPILER` | ~10 lines | low |
| 4 | **Budget the two per-call log lines** on the vertex-program path | `src/hle/hle_d3d8.c:1208`, `src/d3d/d3d8_vsh.c:1251` | **A** | ~9% of the guest thread in the level | 2 lines | none |
| 5 | Skip the **fixed-function pixel path** when a combiner shader is active | `src/d3d/d3d8_shaders.c` `d3d8_shaders_prepare_draw` | **A** | medium — one wasted `PSSetShader` + one constant-buffer `Map` per draw | ~10 lines | low |
| 6 | Don't rebuild the host vertex shader when `LoadVertexShaderProgram` reloads **identical** microcode | `src/hle/hle_d3d8.c`, `src/d3d/d3d8_vsh.c` | **C** | medium | ~40 lines | medium |
| 7 | **Precompute the microcode hash** at load instead of per draw | `src/d3d/d3d8_vsh.c` `d3d8_vsh_prepare_draw` | **A** | ~1.7-2.2% | ~10 lines | low |
| 8 | **Hash-index the texture cache** instead of a linear scan of 512 entries per `SetTexture` | `src/hle/hle_d3d8_texture.c` `host_texture` | **A** | small-medium | ~30 lines | low |
| 9 | Cheaper **texture change detection** than `level0_checksum` | `src/hle/hle_d3d8_texture.c` | **A** | ~4-4.5% | medium | medium |
| 10 | Hoist `strlen(name)` out of `trace_only_match`'s hot path | `src/kernel/recomp_trace.c:422` | **A** | part of the 2.2-2.4% trace hook | 3 lines | none |
| 11 | Ship TS2 lifted **without** `--trace-all-entries` | `titles/timesplitters2` build recipe | **B** | ~2.3-2.5% | none | none |
| 12 | Make `nv2a_ack_thread` sleep instead of `Sleep(0)` | `src/kernel/xbox_memory_layout.c` | **A** | ~0 here (one core back) | ~5 lines | low |
| 13 | Seed the last unresolved indirect call `0x001D35E4` | `config/seeds/4553000A.json` | **B** | correctness, not speed | — | — |

### 1. Ring-buffer the indexed UP draw — **class A**

`dev_DrawPrimitiveUP` (`d3d8_device.c:910`) uploads into a persistent 4 MB
ring with `MAP_WRITE_NO_OVERWRITE`, which is the right D3D11 pattern.
`dev_DrawIndexedPrimitiveUP` (`:965`) never got the same treatment: it calls
`ID3D11Device_CreateBuffer` **twice** (an `IMMUTABLE` vertex buffer and an
`IMMUTABLE` index buffer) and releases both, on **every draw**.

TimeSplitters 2's level is ≈356 indexed draws a frame, so that is **712
D3D11 buffer creations and 712 releases per frame**, ≈32,000 a second at
45 fps. Each takes the device lock, which is what `RtlAcquireSRWLockExclusive`
+ `RtlReleaseSRWLockExclusive` at a combined 7.2-8.0% leaf is, and what
`D3D11CoreCreateDevice` at 50-55% *inclusive* is (nearest-public-symbol for
d3d11's resource creation entry points). `RtlAllocateHeap` at 2.0% is the same
path.

What it takes: a second ring for indices (`D3D11_BIND_INDEX_BUFFER`), the same
discard/no-overwrite discipline, and `DrawIndexed` with the two base offsets
instead of 0. The vertex ring can be shared if it is created with both bind
flags, but D3D11 is happier with two.

Risks: the ring wraps with `WRITE_DISCARD`, which invalidates data still
referenced by draws already submitted in the same frame — the existing
non-indexed ring has that hazard today and 4 MB has been enough. TS2's per-draw
vertex range is a few kilobytes, so 356 draws is well inside 4 MB, but a title
with bigger ranges would need a bigger ring or a fence. Size the ring from
measurement, and log a wrap count.

Why this is first: it is the single largest identified cost, it is a pattern
already proven in the same file three functions up, and it cannot change what
is drawn.

### 2. Cache sampler states — **class A**

`d3d8_states_apply()` runs before every draw and ends with

```c
for (s = 0; s < 4; s++) d3d8_states_apply_sampler(s);
```

and `d3d8_states_apply_sampler` unconditionally releases the stage's sampler
and calls `ID3D11Device_CreateSamplerState`. **Four creates and four releases
per draw**, ≈1,400 a frame, ≈64,000 a second.

The blend, depth-stencil and rasterizer states in the same file are all
already cached — by hash for blend and raster, by `memcmp` of the descriptor
for depth-stencil. The samplers were simply never given the same treatment.
Do it the same way: build the `D3D11_SAMPLER_DESC`, compare it with the one
that produced the currently bound sampler, and only create and re-bind when it
differs. A small keyed cache (D3D11 dedupes internally anyway, but the call
still costs a device lock) is better than create/release.

*(Not yet measured in isolation. The mechanism is certain from the source; its
share is inside the same D3D11-resource-creation inclusive total as item 1.)*

### 3. Fix the combiner shader cache key — **class A**

`d3d8_combiners_get_shader` is 7.43% (level-weighted) / 9.19% (front end) of
the guest thread — the largest single `d3d8_` symbol, above every lifted
function. It FNV-1a hashes the **whole** `NV2ACombinerState` (≈1.7 KB, a
byte-at-a-time loop with a serial multiply dependency) and then `memcmp`s it,
once per draw.

Two separate faults:

**(a) The key contains data that does not affect the shader.**
`NV2ACombinerState` carries `c0[8]`, `c1[8]`, `final_c0` and `final_c1` — the
combiner constant colours. `d3d8_combiners_generate_hlsl` never reads them
(verified: the only reads of `state->c0`/`c1` in `d3d8_combiners.c` are the
parse at `:328` that fills them and the debug print at `:955`); they are
uploaded to the constant buffer in `d3d8_combiners_prepare_draw` at `:1187`.
So **every distinct constant colour the title uses makes a new cache entry and
a new runtime `D3DCompile(..., "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3)`**,
in a 128-entry table that then thrashes through LRU eviction and recompiles
the same shaders forever. That is what `D3DCOMPILER_47.dll` at 2.2-2.3% leaf
and 13.89% inclusive is: HLSL being compiled *in steady state*, in a level
that introduces no new materials.

**(b) The lookup runs even when nothing changed.** `prepare_draw` already
guards the *parse* with `g_dirty`. The hash and probe are outside that guard.

Fix both: hash and compare only the structural fields (stages, inputs,
outputs, `final_input`, `tex_mode`, `flags`, `num_stages`) — cheapest by
splitting the struct so the structural part is contiguous — and keep the last
`(state, shader)` pair so an unchanged state skips the lookup entirely.

Risk: if any constant is ever folded into generated HLSL, dropping it from the
key would bind the wrong shader. Grep says it is not, today. Guard it by
regenerating a captured frame and comparing images (`d3d8_replay`), which is
exactly what the capture/replay rig is for.

### 4. Budget the per-call vertex-program log lines — **class A**

```
src/hle/hle_d3d8.c:1208   "[HLE-D3D8] shadow vertex program at slot %u: ..."
src/d3d/d3d8_vsh.c:1251   "D3D8 VSH: Created shader handle 0x%lX (%d instructions)"
```

Both are unconditional, both are on the `LoadVertexShaderProgram` path, and
between them they are **53,084 of the level run's 64,202 stderr lines**.
`NtWriteFile` is 9.36% of the guest thread in that run against 1.76% in the
front-end run that barely takes that path. stderr is unbuffered, so each
`fprintf` is at least one syscall on the guest thread.

The rule the rest of the runtime already follows (`RECOMP_KERNEL_LOG_BUDGET`,
`RECOMP_TRACE_BUDGET`) is that nothing on a per-call path logs without a
budget. These two escaped it. Give them a budget, or log only when the slot's
microcode actually changed (which is item 6 anyway).

This is class A because it is the logging policy, not the path; but note the
two offending lines are only reached by titles that use the 4721-era
`LoadVertexShaderProgram` API, which is why Burnout 2 never showed it.

### 5. Skip the fixed-function pixel path when combiners are active — **class A**

Every draw calls `d3d8_shaders_prepare_draw()` and then
`d3d8_combiners_prepare_draw()`, and the comment at the call site says the
second "overrides PS if combiner shader is active". It does — after the first
has already computed `ff_ps_compute_signature()`, looked up and bound a
fixed-function pixel shader, and mapped and filled the whole `PSConstants`
buffer with `MAP_WRITE_DISCARD`. When `g_ps_token != 0` all of that is thrown
away. Check the token first.

*(Not yet measured in isolation; `d3d8_combiners_prepare_draw` is 11.29%
inclusive and the fixed-function half sits beside it.)*

### 6. Don't rebuild an unchanged vertex program — **class C**

`D3DDevice_LoadVertexShaderProgram` is called 26,542 times in 131 s — ≈4-5
times per level frame. Each call runs `host_vsh_create_shader` (a fresh VSH
slot, a `memcpy` of the microcode) and `host_vsh_delete_shader` on the
previous one. The compiled HLSL survives, because
`d3d8_vsh_prepare_draw` looks it up by microcode hash — but the slot churn,
the copies and the logging do not.

Keep the microcode that is already in the slot and compare; if it is identical,
do nothing at all.

**Class C, not B**: `LoadVertexShaderProgram` / `SelectVertexShaderDirect` is
how XDK 4721-era titles stream vertex programs, and the fix is entirely in
`src/hle` and `src/d3d`. TimeSplitters 2 is simply the first title in this
tree to take that path in a level.

### 7. Precompute the microcode hash — **class A**

`d3d8_vsh_prepare_draw` (`d3d8_vsh.c:1418`) FNV-1a hashes the whole microcode
(up to 136 instructions, 2,176 bytes) **per draw**, only to look it up in a
cache. The microcode changes when the slot is loaded, not when it is drawn
with. Hash at load and store it in `NV2AVshSlot`. 1.73-2.23% leaf.

### 8. Hash-index the texture cache — **class A**

`host_texture` (`hle_d3d8_texture.c:235`) walks up to 512 entries comparing
four 32-bit fields, once per `SetTexture`. A previous run of this title showed
554,000 binds. Key the cache by guest VA.

### 9. Cheaper texture change detection — **class A**

`level0_checksum` is 4.10-4.46% of the guest thread. It reads level 0 with a
stride chosen to take 4,096 samples, which for anything over 256 KB touches
essentially every cache line of the texture, once per bound texture per frame.

Options, in increasing order of work: sample fewer points (4,096 is arbitrary);
checksum only every Nth frame for textures that have not changed in a while;
or the proper answer, **write-watch the pages behind guest textures** and
re-upload only what the guest actually wrote — which is also what removes the
guesswork about a title that writes a texture between two checksummed frames.
The write-watch route is a memory-model change and belongs with `CLAUDE.md`
planned change 1.

### 10-13

`trace_only_match` computes `strlen(name)` before its `if (!list[0]) return 0;`
early-out, so every lifted function entry pays it even with
`RECOMP_TRACE_ONLY` unset (**A**, 3 lines). Shipping TimeSplitters 2 lifted
without `--trace-all-entries` removes the 2.3-2.5% hook entirely and is purely
a per-title build choice (**B**). `nv2a_ack_thread`'s `Sleep(0)` still burns a
core but, measured here, the title never waits on the fence (`title's Swap
0.00 ms`), so it buys no frame time on this machine (**A**, low priority). The
last unresolved indirect call is a TS2 seed (**B**).

---

## What this plan deliberately does not recommend

- **The register model.** Lifted code is 28.9% (level-weighted) / 30.7%
  (front end) of one thread, its hottest function is 7-8.6%, and the rest is a
  tail under 1%. The threshold set in `performance-60fps.md` — "more than half
  the guest thread and spread over hundreds of functions" — is not met.
- **The flip gate / `RECOMP_FPS_CAP`.** Gate wait is 0.00 ms and the title's
  own `Swap` is 0.00 ms in the level. There is nothing to pace until the frame
  is under 16.7 ms.
- **A Vulkan backend, for speed.** Nothing measured here is the backend's
  fault in a way a different API fixes; items 1, 2, 3 and 5 are the same
  mistakes in any API. Do Vulkan for portability, as `CLAUDE.md` says, after
  these.
- **Per-frame render/texture-stage state forwarding**
  (`hle_d3d8_shadow_apply_states`, 0.76% leaf). It diffs whole arrays per draw
  and is still cheap. Leave it.
- **`SetRenderState_Simple`** (1,384 calls a minute). Confirmed: it does not
  appear anywhere in either profile. Nothing to do.
- **DirectSound.** Does not appear in either profile.

---

## Reproducing

```bat
py -3 scripts\recompile.py "games\Time Splitters 2\default.xbe" ^
    --work-dir games\_pipeline\ts2perf\out --project titles\timesplitters2 ^
    --trace-all-entries
cmake -S titles\timesplitters2 -B titles\timesplitters2\build ^
    -G "Visual Studio 16 2019" -A x64
cmake --build titles\timesplitters2\build --config Release ^
    --target timesplitters2_recomp -- -m

rem clear the save profiles first, or the menu path changes
rmdir /s /q "games\Time Splitters 2\UDATA\4553000a\<12 hex chars>"

set RECOMP_VBLANK=1
set RECOMP_AC97_READY=1
set RECOMP_HLE_D3D8=shadow
set RECOMP_TRACE_BUDGET=0
set RECOMP_FPS=5
set RECOMP_SAMPLE=500
set RECOMP_SAMPLE_REPORT=20
set RECOMP_INPUT_SEQ=12000:start,16000:start,20000:start,24000:start,28000:a,34000:a,40000:a,46000:a,52000:a,58000:a,64000:a,70000:a
py -3 scripts\run_and_report.py ^
    titles\timesplitters2\build\Release\timesplitters2_recomp.exe ^
    --seconds 120 --out-dir runs --tag base
```

Never pass `--profile` for a speed measurement, and check `tasklist` for
`MSBuild.exe`, `cl.exe` and `*_recomp.exe` first. Confirm the run reached the
level by reading the `[HLE-D3D8] shadow:` draw counts before trusting its fps:
the level is ≈390 draws a frame, the front end ≈170.

---

## Results (19 Sep 2026)

Items 1-7 were done on `perf/ts2-items-1-7`, one commit each, and measured
together on the same machine, quiet, with the title lifted **without** the
trace hook, the same script, saves cleared, `RECOMP_FPS_CAP=0`, reading the
level phase (t = 75-125 s) of `[FPS]` and `[HLE-D3D8] swap timing`. The
baseline is three runs of the bring-up branch at 672e322.

| Build | Level fps, five-second windows | Rest of frame | stderr lines / 130 s |
| --- | --- | --- | --- |
| baseline (plain lift, cap off) | 79-89 | 12.4 ms | 130,003 |
| items 1-7 | 188-202 | 5.0 ms | 10,874 |

Two independent checks that nothing drawn changed:

- **Capture/replay.** Three level frames captured from the baseline
  (`RECOMP_D3D8_CAPTURE`, swaps 12500, 14000 and 15500, 359-389 draws each)
  replay **pixel-identical** (`cmp` on the BMPs) through the new renderer.
  This is the check item 3 asked for.
- **Replay as a renderer benchmark.** `d3d8_replay <capture> --loops 300
  --quiet` runs the host renderer alone, no game, present interval 0.
  Baseline 5.28-5.40 s for 300 loops (17.7 ms a frame); new 4.14-4.20 s
  (14.0 ms). The rest of the in-game gain is the HLE side (items 4 and 6)
  and what the D3D11 device lock was costing the guest thread.

Under the default flip gate (adaptive) the level holds 60.0 with 13.4 ms of
gate wait and 3.2 ms of frame, which is the headroom a slower machine will
spend. Burnout 2's front end is unchanged: 60.0 under the gate, 0.55 ms of
frame, no faults.

What each item turned out to be worth was not measured in isolation; the
profile said items 1-3 were the bulk, and the replay benchmark (renderer
only, items 1, 2, 3, 5 and 7) accounts for a fifth of the frame while the
in-game number halved again, so the HLE-side items 4 and 6 (log lines and
program reloads: `[HLE-D3D8] shadow vertex programs: N loads repeated a
slot's microcode` on the five-second report says how many) were worth more
than the 9% the sampler gave `NtWriteFile`.

Item 6 needed a second pass. The plan's "compare with what the slot holds"
matched 47 loads in a run: the title does not reload the *same* program
into a slot, it rotates a few through it (slot 0 takes 49-, 50- and
13-instruction programs in turn; 59,722 loads in the baseline's 130 s). Host
programs are now found by microcode content and kept for the run, shared by
every slot and every selected entry that names them: 41,103 of a run's loads
were answered from 12 cached host programs, and the five-second report says
so (`[HLE-D3D8] shadow vertex programs: N loads answered from the M cached
host programs`).

Still open, in the order the profile suggests: 9 (texture change
detection, `level0_checksum`, ~4%), 8 (texture cache scan), 10 (`strlen` in
the trace hook, moot for a plain lift), 12 (ack-thread sleep, no gain
measured). Take a new `RECOMP_SAMPLE` profile first: the frame is 5 ms now
and its shape has changed.
