# Packed vertex conversion checks

`zz-vertex-expand-strides.patch` changes common HLE `hle_d3d8.c`. The parsed
declaration places converted attributes in a prefix before the unchanged
stream-zero tail. Attribute/width dispatch moves outside the vertex loop;
per-vertex attribute order remains, including overlapping prefix outputs.
Every draw reads fresh source bytes into a new zeroed allocation. No vertex
data, transformed pose or allocation survives that draw. Stream lookups and
their failure order, padding, SHORT/NORMPACKED3 arithmetic and width/format
fallbacks retain their existing behavior.

`zz-vertex-expand-tiles.patch` follows that patch and bounds every attribute/tail
pass to32 vertices, rather than sweeping a large expanded buffer repeatedly.
Advance to `begin + min(vertices - begin,32)` to keep the final partial tile
within range without unsigned wrap. All vertices are still converted. Compare
tiny, partial-tile and large draws; a96-vertex benchmark alone missed the initial
whole-buffer kernel's slowdown above16,000 vertices.

The selector tail-calls the unchanged generic fixed-function body or the packed
setup. A portable noinline compiler boundary preserves the generic compiler
scope; no OS rendering adapter or guest-clock change is added.

`tests/test_vertex_expand.py` compiles the full production functions against
`vertex_expand_reference.inc`, the frozen pre-change HLE body.48,000 layout comparisons
span vertex counts0..1024, four rounding modes, fresh/aliased streams, overlapping prefix outputs,
zero padding, lookup/allocation failure and source/destination guard pages.
Widened raw-copy, read-only extra-byte, wrong-stride, reversed-order and missing-
fallback plus repeated tile-origin mutations must compile and then fail. The extra-byte mutation preserves
output and must still fail at a guard page. Guard cases must reach conversion, rather
than accidentally passing because stream validation rejected their layout.
Protected cases use4/132/260 vertices and16 usable pages plus one guard page;
they span multiple tiles while keeping destination allocations16-byte aligned.
Another128 cases compare complete large outputs near4,096/16,384/65,536 vertices
in both packed layouts, using NORMPACKED3 and SHORT4 under all four rounding
modes. Keep these checks distinct from performance timings/checksums.
Both native CI hosts run the fixture. It needs no game assets.

The optional executable arguments `<scenario> <enabled>` time600,000 draws of
96 vertices for a32-byte packed layout, fixed-function layout or50-byte packed
layout (scenario0/1/2). Compare seven alternating method pairs and retain the
source/patch hashes. This is CPU-kernel evidence, not game FPS, complete renderer
fidelity or physical Mac acceptance. Live measurements belong in STATUS.

## Controls fixture clock

The Windows `tests/pc_controls.c` expiry check uses GetTickCount64, matching the
adapter's clock. Sleep(40) can advance that coarse clock only31ms, short of the
34ms Reload pulse. Wait for at least40 tick-clock milliseconds before asserting
expiry. Optional `TSFP_TEST_RELOAD_PHASE_MS` (0..20) shifts the test poll and
prints elapsed/button diagnostics. Preserve the expiry and held-Fire assertions;
no production Reload/Fire policy changes are needed for this fixture failure.
