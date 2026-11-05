# Native effect targets and shared world depth (#497)

Patch2610 changes the live host_SetRenderTarget boundary in
src/hle/hle_d3d8_record.c and the production replay boundary in
src/replay/d3d8_replay.c to include TSFP_RHI_RENDERER, alongside Windows.
Both call xbox_D3D8PrepareSharedDepthTarget before obtaining a level-zero
surface when the effect uses the automatic scene depth. The helper already
exists in common RHI code; patch2600 exposes its declaration on every host.
Legacy Mac GL retains its existing path, since it does not define that macro.

Without this preparation, smaller effect color targets cannot attach the
larger scene depth image. SetRenderTarget fails, so deferred draws may execute
on the previous target. This is a candidate cause of the electrical glow
reported through walls in Electro Chimp/The Russian Connection. No exact
reported gameplay capture has yet been validated; do not equate the synthetic
occlusion reproduction with a completed encounter verification.

The Linux software-Vulkan production replay failed before the change with
SetRenderTarget depth128x96/color64x64 mismatch. With the change,
tests/graphics/ghost_targets.py passes at128x96,160x90,344x144 and160x90 scale2.
It checks a foreground wall occluding the effect, refreshed feedback samples,
and depth writes surviving the return to scene. frame_replay_contract.run now
runs these checks with the caller's SDK/driver environment and validation on.
The live adapter fixture in test_ghost_targets.py compiles the actual preparation
block without _WIN32, checks unrelated/null depth and mip exclusion, and failure
propagation. A Windows-only-guard mutation must fail.

Visual follow-up (2026-11-05): the native Linux game built successfully from
772a38df with this candidate, including the protected-MMIO check. An isolated
older-build run reached Electro Chimp Discomatic and recorded a real electrical
beam frame. Software-Vulkan production replay with and without patch2610
produced pixel-identical output, with zero failed host calls in both runs.
That frame did not reproduce the reported occluded glow, so it neither proves
nor disproves this candidate for #497. Actual through-wall encounter validation
remains outstanding. Local evidence is logs/issue497-visual in the Linux checkout;
these private game captures must not be committed. The temporary baseline
replay mutation was restored and the corrected replay rebuilt.

The historical-controls upgrade fixture predates patch1130's shared-depth
helper and patch2600. Exclude patch2610 from that old starting subset, then
assert both native guards and helper calls after applying the full current
stack. This preserves upgrade coverage without inventing an impossible old
renderer state.
