# Vertex shader constant uploads

The tail `vsh-constant-dirty.patch` changes common RHI `d3d8_vsh.c`. Each
incoming constant register is compared by bytes with the current CPU bank.
Scalar copy order stays intact, including overlapping source ranges, and
Hor+ handling/queries still run. Changed bits (including signed zero/NaN payloads)
mark the bank dirty; unchanged sets never clear pending dirty data. Empty or
clipped non-null ranges keep their original invalidation. Initial/new buffers
remain dirty. This is current API state, not an animation/pose cache: every
incoming value is read and every changed pose still uploads before drawing.

The draw path retains screen-zoom invalidation and binds the same buffer. Only
a successful map/copy/unmap clears dirty state and commits the zoom state. A
failed map leaves pending constants dirty for retry, including an identical
next set or a zoom transition. The previous unconditional clear could lose it.
No interpolation, guest timing, texture, or shader-program policy changes.
Both Windows and Mac common RHI use this code; legacy GL keeps its existing
direct uniform path, which has no RHI dirty buffer to update.
`camera_zoom()` caches its numeric setting on first use; the per-draw screen
target decides whether to apply it. The existing screen/non-screen transition
check remains necessary even when all incoming constant bytes repeat.

`test_vsh_constant_dirty.py` extracts the production setter and complete upload
block, comparing them with the frozen pre-change runtime reference. Controlled
host services test 8,192 random/edge configurations across four rounding modes,
full bank/GPU-mirror bytes, overlapping aliases, clipping/invalid inputs, setter
FP flags, initial zero upload, signed zero, repeated sets, fresh poses, target/
zoom changes and failed-map retry. Always-dirty/lost-change/clear-after-failure
mutations must compile and fail. Both native CI hosts run the fixture; services
are mocks, not physical GPU/scene evidence. Retain separate actual renderer
replay/readback, live pacing, source/build hashes and physical-host limits in
STATUS before claiming an experience improvement.
Selected retained Windows GPU replays also compare pre/post-change pixels and
hand/weapon coverage, fresh adjacent poses and the constants-only frozen-pose
control. Record rounding differences and repeat controls separately; those
offline images do not establish live framebuffer parity or whole-game fidelity.
