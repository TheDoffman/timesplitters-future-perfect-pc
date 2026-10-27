# Rendering brief — 11 September 2026

Read this before touching graphics. It is the result of a review of yesterday's
five commits (`4225dbe` … `f04ee75`) against head, and it replaces the plan you
were following. The conclusion you reached at `7737dfe` is correct: the frontend
vertex program is the stock pass-through shader, its transform is identity, and
the white frame is not a transform problem. This brief says what the white frame
*is*, and how to work so that the next problem costs an hour rather than a day.

**One rule for today.** Do tasks 1 and 2 before anything else, dump the frame,
and only then decide what the next problem is. Do not open `d3d8_vsh.c` until
task 5.

---

## Ground truth, verified against head `f04ee75`

Each of these was read directly, not inferred.

1. **The pixel write has no alpha.** `raster_triangle` (`src/kernel/nv2a_pb_exec.c:1106`)
   writes the raw texel with `put_pixel` and never reads its alpha; the
   untextured path writes the vertex colour. `put_pixel` (`:1079`) stores the
   dword. There is no handler anywhere in the file for `NV097_SET_BLEND_ENABLE`,
   `NV097_SET_BLEND_FUNC_SFACTOR`, `NV097_SET_BLEND_FUNC_DFACTOR`,
   `NV097_SET_ALPHA_TEST_ENABLE`, `NV097_SET_ALPHA_FUNC` or `NV097_SET_ALPHA_REF`;
   all six fall to `note_unhandled`. The `LIN_A8` decoder (`:1002`) returns
   `0x00FFFFFF | alpha<<24`. A font page is an alpha texture, so every glyph
   quad paints a solid white box the size of its cell. That is the white band.
   The logo draws correctly because it is opaque RGB.

2. **The primitive constants are off by one.** `nv2a_pb_exec.c:1519–1523`
   defines TRIANGLES 4, STRIP 5, FAN 6, QUADS 7, QUAD_STRIP 8. The header at
   `src/nv2a/nv2a_regs.h:1160–1170` says 5, 6, 7, 8, 9. `s_gpu.prim` is the raw
   BEGIN_END parameter (`:1966`). So:

   | Title sends   | Value | Lands in executor case | Drawn as                          |
   |---------------|------:|------------------------|-----------------------------------|
   | TRIANGLES     | 5     | `NV_PRIM_TRIANGLE_STRIP` | a strip; every third triangle wrong |
   | TRIANGLE_STRIP| 6     | `NV_PRIM_TRIANGLE_FAN`   | a fan around vertex 0            |
   | TRIANGLE_FAN  | 7     | `NV_PRIM_QUADS`          | independent quads                |
   | QUADS         | 8     | `NV_PRIM_QUAD_STRIP`     | a strip; glyphs joined to neighbours |
   | QUAD_STRIP    | 9     | `default`                | not drawn                        |

   Your diagnosis in `f04ee75` that the text batch was being fanned was right.
   The text is hardware QUADS = 8, which the executor labels QUAD_STRIP, so it
   is now decoded as a strip and half the triangles still stretch between
   glyphs. The independent-quads code you wrote is correct and will be reached
   once the constants match the header. Fixing the constants also un-fans every
   strip and un-strips every list, so the triangle count and the frame will
   both change a lot. That is expected.

3. **The vertex program parser is documented wrong and left wrong.**
   `docs/technical/nv2a-vertex-program-encoding.md` now has the complete
   layout and the four structural errors. The rewrite was reverted. The same
   parser feeds the HLSL translator, so this is a shared defect. There is no
   test for it. It cannot affect this title's frame today, which is why it is
   task 5, not task 1.

4. **No tests were added yesterday.** 874 lines across 8 files, none under
   `tools/` or `tests/`. The 129 passing tests are the Python suite and cover
   none of this work.

---

## Tasks, in order

Commit each separately. Every commit body states what was measured before and
after, as you did for the first 80 commits. `f04ee75` did not; say in the next
commit body what it changed and what the `[PX]` counters showed.

### Task 1 — Primitive constants from the header

**Do.** Delete the five `NV_PRIM_*` defines. Switch on
`NV097_SET_BEGIN_END_OP_TRIANGLES`, `_TRIANGLE_STRIP`, `_TRIANGLE_FAN`,
`_QUADS`, `_QUAD_STRIP` from `nv2a_regs.h`. Add `_POLYGON` as a fan (that is
what the hardware does for convex polygons) and count `_POINTS`, `_LINES`,
`_LINE_LOOP`, `_LINE_STRIP` as unhandled by name.

**Test.** A C test that builds one synthetic batch per primitive type with
known indices, runs it through `raster_batch` with a recording `raster_indexed`
or `put_pixel`, and asserts exactly which triangles were emitted. Put it under
`tests/` with a `CMakeLists.txt`, and add `add_subdirectory(tests)` to the root
so it builds.

**Acceptance.** Test passes. Commit body records triangles per frame before and
after, and the frame dump after.

**Rule that comes with it.** No numeric constant that exists in `nv2a_regs.h`
is redefined anywhere in `nv2a_pb_exec.c` or `nv2a_pb_scan.c`. This is the
third wrong local copy (after 0x1D6C and 0x1808/0x0130/0x1B14). Add a test that
the scanner's name table agrees with the header for every entry.

### Task 2 — Alpha at the pixel

**Do.** Decode into `s_gpu`: `NV097_SET_BLEND_ENABLE`,
`NV097_SET_BLEND_FUNC_SFACTOR`, `NV097_SET_BLEND_FUNC_DFACTOR`,
`NV097_SET_ALPHA_TEST_ENABLE`, `NV097_SET_ALPHA_FUNC`, `NV097_SET_ALPHA_REF`.
The factor values are in the header as `NV097_SET_BLEND_FUNC_SFACTOR_V_*` and
`_DFACTOR_V_*`. The header has **no** `NV097_SET_ALPHA_FUNC_V_*` values; add
them from xemu's `nv2a_regs.h` (they are the GL compare enums, NEVER through
ALWAYS) with a comment naming the source. Do not type them from memory.

In `raster_triangle`, per pixel:

1. `src = textured ? texel * vertex_colour : vertex_colour` (per channel, 0–255
   scaled). Use the per-triangle colour you already pass in; interpolating
   per-vertex colour can wait.
2. If alpha test is enabled, compare `src.a` to `alpha_ref` with `alpha_func`;
   discard on failure.
3. If blending is enabled, read the destination pixel, apply
   `src*sfactor + dst*dfactor` for the factor pairs you implement. Implement
   `SRC_ALPHA / ONE_MINUS_SRC_ALPHA` (UI), `ONE / ONE` (additive) and
   `ONE / ZERO` (opaque). Count every other pair as unhandled, by name, so the
   report says what the title asked for.
4. Otherwise write `src` opaque, as today.

`put_pixel` needs a `get_pixel` twin for step 3, for both 32-bit and 16-bit
surfaces.

**Test.** A synthetic textured quad over a known destination colour with an A8
texel of alpha 0x80 and `SRC_ALPHA / ONE_MINUS_SRC_ALPHA`: assert the blended
value. One case each for alpha test pass and fail.

**Acceptance.** Frame dump shows legible text over the logo. Commit body records
the unhandled-factor list.

### Task 3 — Measure the frame properly

Before deciding anything else:

- Dump the frame (`RECOMP_FB_DUMP`). Look at it once. Then keep it: this is
  the first golden image, see task 4.
- Report from `nv2a_pb_exec_report()`: what is the top of the unhandled list
  now? That, not a theory, is the next problem.
- Record the numbers in the commit body: triangles, batches textured, batches
  no-texture, batches untransformed, vp_declined.

If the frame is still wrong after tasks 1 and 2, use task 6's modes before
forming a hypothesis.

### Task 4 — Frame capture and offline replay

This is the highest-leverage tool you can build and it is a few hundred lines.

**Do.** `RECOMP_PB_CAPTURE=<path>`: for one frame (from one CLEAR_SURFACE to the
next, or from FLIP to FLIP), write every `(subch, method, param)` the scanner
hands the executor, plus a snapshot of every guest memory range the frame
references: the surface, each vertex attribute's buffer span, each texture's
span, and the PRAMIN region the DMA resolver reads. A simple format: a header,
a list of `(guest_addr, length)` blobs, then the method stream.

`tools/pb_replay/`: a headless C program that loads a capture into a scratch
guest memory, feeds the methods through `nv2a_pb_exec_method`, and writes the
surface as BMP. No kernel, no title, no audio. It must build on Linux with the
kernel library's executor files only; if the executor has dependencies that
prevent that, that is a finding, note them and stub them.

**Acceptance.** Replaying the captured frontend frame reproduces the golden
image bit for bit. Check the capture and the image in under `tests/fixtures/`
only if they contain no game asset data; a frontend UI frame contains font and
logo textures, which are game assets, so keep the fixture local and check in
only the hash. Add a CI-shaped script that replays and compares.

From here on, **every executor change is tested against the replay first**, and
the title is booted only to capture a new frame.

### Task 5 — Vertex program parser, test first

**Do.** Write the test before the code: the twelve dwords in
`docs/technical/nv2a-vertex-program-encoding.md` must parse to exactly the
disassembly listed there (opcode, operands, banks, swizzles, masks, output
selector, final flag). Then rewrite `vsh_extract` and the field table by hand
against "The full layout" in that document. No scripted edits. The four
structural errors listed there are the checklist.

**Acceptance.** The twelve-instruction test passes. `d3d8_vsh_execute` on
`v0 = (100, 200, 0, 1)` with the documented constants writes `oPos` with
`out_written` including position and value `(100.53125, 200.53125, 0, 1)`. The
existing `test_lifter_fpu*`-style Python tests still pass. The HLSL generator's
output for the same program is checked into a test as text.

### Task 6 — Batch attribution modes

Two switches in `raster_triangle`:

- `RECOMP_PB_FALSE_COLOUR=1`: ignore textures; paint each batch a colour from
  its index (a small palette cycling on `s_gpu.draws`).
- `RECOMP_PB_ONLY_BATCH=N`: rasterise only draw number N.

Both dump the surface. "Which draw made this white area" becomes a ten-second
question.

### Task 7 — State at draw

At every BEGIN_END with a non-zero parameter, under `RECOMP_PB_STATE=1`, print
every field that decides a pixel, whether or not the executor uses it yet:
primitive, transform mode, viewport, surface format/pitch/offset, clip,
blend enable and factors, alpha test and ref and func, cull, depth test, all
four texture stages (format, size, address modes, filter, offset), the
combiner registers (`NV097_SET_COMBINER_*` as raw values), the program length
and constant range.

Half of these are not decoded today. Decoding them is the NV2A state model
beginning to exist; keep them in one struct with a dirty bit each. That struct
is what the GPU path will consume, so name the fields for what they are, not
for how the software rasteriser uses them.

### Task 8 — Move work out of the pixel and out of the vertex

- `fetch_position` runs the interpreter per vertex, and `batch_is_screen_space`
  runs it over the whole batch before `raster_batch` runs it again. Transform
  the batch once into a scratch array of positions, then rasterise from it.
- `put_pixel` calls `dma_resolve` and `surface_hits_image` per pixel. Resolve
  the surface once per batch.
- `getenv` at `:1687` and in `vp_dump_once` is called per method or per batch.
  Read every switch once at init.
- The `[PX]` counters print every 4,000 triangles unconditionally. Behind the
  verbose switch.

### After task 8: stop

The software rasteriser is a debugger. It gets no depth buffer, no perspective
correction, no combiners, no multi-texture, no interpreter beyond what task 5
needs. The next rendering feature goes into the state model from task 7 and a
GPU path driven from it, with the task 4 replay as its test harness. That plan
is in the graphics review; do not start it before tasks 1–8 are done and the
frontend is readable.

---

## How to debug rendering, from now on

These are the habits that would have made yesterday an hour.

1. **Attribute before you theorise.** When a frame is wrong, the first question
   is "which draw wrote these pixels", answered with task 6's modes. The second
   is "what state was set for that draw", answered with task 7. Only then form
   a hypothesis. Yesterday started with the most sophisticated possible cause
   and worked downward; the pixel write was the cause and a read of the fill
   loop shows it.

2. **Constants come from the header.** `nv2a_regs.h` is the single source for
   method numbers, primitive types, format codes and enum values. If a value
   you need is missing there, add it there with its source in a comment. Never
   define it locally.

3. **Pure functions get tests before they are wired in.** The parser, the
   interpreter, the primitive decode, the blend math and the texel decoders are
   all functions over dwords. Each takes minutes to test and hours to debug in
   a running title.

4. **Iterate on a capture, not a boot.** After task 4, a rendering change is
   checked in a second against a golden image. Boot the title to capture a new
   frame, not to check a fix.

5. **Measure what you claim.** "Drew something" is a claim only when the frame
   compare or the golden image says so. The white smear reported zero batches
   skipped and looked like progress for a day.

6. **Apply findings you already have before generating new ones.** The
   primitive off-by-one was on the audit page before yesterday's session
   started.

## What this is not

None of this is Burnout-specific. The primitive enum, the blend state, the
program encoding, the method numbers are properties of the NV2A and of the
D3D8 every title links. If a fix needs to know which game is running, stop and
say so; that is the smell the retired Burnout 3 scaffolding had.

## Report back

For each task, one commit, body with before/after numbers, and the frame dump
path. When tasks 1–3 are done, stop and report the frame and the top of the
unhandled list before continuing, so the order of 4–8 can be adjusted against
what the title actually asks for next.
