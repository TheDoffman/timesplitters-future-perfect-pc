# Inclusive portal scissors (#521)

Xbox SetScissors uses inclusive last-pixel coordinates. The stored guest
rectangle and capture format retain those coordinates. Patch2725 converts at
`d3d8_GetScissor`, immediately before the common RHI consumes the bounds.
`src/graphics/guest_scissor.h` adds one to right/bottom **before** viewport scaling,
then rounds lower bounds down and exclusive upper bounds up. Double-precision
target ratios avoid an extra rounded row at nonintegral resolution scales.
The legacy GL adapter calls the same helper, then flips Y for glScissor.
The crane's alternate 640/480 full-canvas spelling is normalized to 639/479
before the common conversion; stored state remains unchanged.

A real Windows Vietnam capture showed a portal scissor `(201,110,375,321)`.
At 1280x720, the old conversion excluded row481, leaving sky visible between
the room floors. Corrected bounds include that row. Draw-level replay and vertex
inspection distinguished this from overlapping geometry/depth fighting. A
temporary precise-position shader experiment did not remove the seam and was
reverted; no depth bias, mesh expansion or camera change is part of this fix.

`tests/graphics/portal_scissors.py <d3d8_replay>` renders synthetic portal,
single-pixel and full-frame rectangles at 640x480, 1280x720, 1920x1080 and
3440x1440. It checks inside **and outside** pixels. The old Windows replay fails
the included bottom/right pixel check. `frame_replay_contract.run` includes the
suite in the maintained Linux/Mac Vulkan renderer checks.
`test_portal_scissors.py` checks fractional scales, unchanged guest coordinates,
empty rectangles, integer limits, and mutations removing inclusivity/rounding.
Existing `test_ui_layout.py` and `test_crane_overlay.py` exercise production
RHI/GL adapter bodies and viewport anchoring.

Local evidence is ignored under `private/issue521` and `logs/issue521-*`.
Windows D3D11 and Linux software-Vulkan replay both remove the captured seam;
Linux runs with validation enabled. This is not native Mac/Steam Deck hardware
verification. Private captured geometry/textures must not be committed. The
issue stays open until a downloadable release containing the fix is published.

Final local validation: Windows `tsfp.ps1 -Action Build` and its build checks
passed. The rebuilt executable completed the isolated `crowd-vietnam` route
with ten bots, all four gameplay checkpoints, clean exit and unchanged profiles.
The complete Windows frame-replay contract passed, including shared-effect depth
and all 12 portal coverage cases. These checks do not substitute for native Mac
or Steam Deck hardware validation.
