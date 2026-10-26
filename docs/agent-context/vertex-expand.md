# Packed vertex conversion checks

`zz-vertex-expand-strides.patch` changes common HLE `hle_d3d8.c`. The parsed
declaration places converted attributes in a prefix before the unchanged
stream-zero tail. Attribute/width dispatch moves outside the vertex loop;
per-vertex attribute order remains, including overlapping prefix outputs.
Every draw reads fresh source bytes into a new zeroed allocation. No vertex
data, transformed pose or allocation survives that draw. Stream lookups and
their failure order, padding, SHORT/NORMPACKED3 arithmetic and width/format
fallbacks retain their existing behavior.

The selector tail-calls the unchanged generic fixed-function body or the packed
setup. A portable noinline compiler boundary preserves the generic compiler
scope; no OS rendering adapter or guest-clock change is added.

`tests/test_vertex_expand.py` compiles the full production functions against
`vertex_expand_reference.inc`, the frozen pre-change HLE body.48,000 comparisons
span four rounding modes, fresh/aliased streams, overlapping prefix outputs,
zero padding, lookup/allocation failure and source/destination guard pages.
Widened raw-copy, read-only extra-byte, wrong-stride, reversed-order and missing-
fallback mutations must compile and then fail. The extra-byte mutation preserves
output and must still fail at a guard page. Guard cases must reach conversion, rather
than accidentally passing because stream validation rejected their layout.
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
