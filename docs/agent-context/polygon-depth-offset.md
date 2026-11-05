# Polygon depth offset investigation

Discord texture report 1567782763124363425 explicitly reproduces at locked
60 FPS. Keep this separate from material matrix arena lifetime issue #398.

At base 831c324a, HLE records NV2A methods 0384/0388 and offset enables
0330/0334/0338 in Xbox render-state slots 77..81, but the forwarding map omits
all five. The common rasterizer descriptor is zero initialized and never
receives either offset. During an isolated Original-60 Chinese match, read-only
sampling at render-state base 003E3CC0 observed enabled solid/point offsets,
including IEEE float slope -.4 and constant -3 (also -100 while disabled).
Sampling is asynchronous and does not establish exact per-draw correspondence.

Candidate additive patch 2601 carries the five raw values through unused host
render states 174..178, including frame capture/replay. Rasterizer translation
selects the enable for the authored fill mode, converts the constant float into
signed D24 depth units and forwards the slope. Disabled offsets are zero;
non-finite values are rejected and integer conversion is bounded. The existing
D3D11 and Vulkan adapters already consume these descriptor fields. Fractional
constant units are truncated by the integer RHI representation. This is not a
floating-depth-buffer emulation change.

The semantic reference is the NV2A method/state model and the original guest
values. For independent comparison, xemu documents the same float offset and
slope inputs in its [pixel shader implementation](https://github.com/xemu-project/xemu/blob/master/hw/xbox/nv2a/pgraph/glsl/psh.c).
No third-party implementation was copied.

Validation uses the existing CI routes:
- `test_state_layout_slots.py`: complete production HLE forwarding for all
  three known XDK layouts, live disable transition; omitted-map mutation fails.
- `test_render_state_cache.py`: complete rasterizer translation for every
  point/wire/solid enable combination, signed float interpretation and invalid
  values; dropped-offset mutation fails.
- `test_rhi_menu_sampling.py`: actual D24 GPU draws accept the offset coplanar
  pass, reject it when disabled and retain occlusion by nearer geometry through
  repeated state-cache hits. The old dropped-offset mutation must fail pixels.

Status: candidate only. These contracts verify the missing state path; they do
not yet confirm that the reporter's kitchen wall/ceiling or Plasma Autorifle
flicker is resolved. No resolution comment has been posted on this report.
Physical Mac/Linux acceptance remains unverified.
