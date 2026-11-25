# Uplink and permanent color correction (#604)

Based on squatmaster420's [issue #604](https://github.com/TheDoffman/timesplitters-future-perfect-pc/issues/604)
and [implementation gist](https://gist.github.com/squatmaster420/82079fca34ae4aaf691c68ffac5b842e),
adapted to the shared renderer and deferred command recorder. Effective late
patch order is 3061 map, 3060 framebuffer, 3062 permanent grade, after 3040 V-Sync.

## Uplink map

The real CopyRects rectangle route supersedes the old no-rectangle shortcut.
Recognize the title's 640x480 snapshot and 128x128 handheld copies by both
rectangle pairs and texture dimensions. Preserve the composed top-left corner
before its aliased framebuffer is cleared; identify it by frame and sequence.
Use the shared HUD squeeze for the map's placed snapshot rectangle rather than
a fixed16:9 crop. The explicit destination rectangle is copied by value into
the deferred queue. The selective brightest shrink sampler preserves thin map
lines without changing ordinary screen-copy filtering. Legacy native GL uses
the same coordinate contract through its FBO adapter; its filtering is linear.

## Framebuffer color

Remember the actual render target's physical framebuffer. Retain the other
framebuffer's last completed image. Refresh stale bound current-framebuffer
textures before whole-target passes; successful draws advance an epoch.
Do not refresh partial glare/blur passes: repeated feedback amplifies their
legacy4:3 placement into bright rectangles. A cheap bound-texture guard and
64-vertex cap avoid scanning world meshes. RECOMP_HLE_D3D8_FB_REFRESH=0 is a
diagnostic legacy comparison, not a player preference.

## Permanent color grade

Patch3062-permanent-color-grade applies the former Console-style preset to
every final presentation: brightness +0.01, contrast 1.09, saturation 1.12,
and unchanged gamma. These are the contributor's comparison values; exact
original-console visual parity has not been independently established.
The user requested permanent adjustments and removal of all Picture controls.
There is no Picture page, slider, preset selector, settings model or persistence
API. Old pc-picture.ini files and TSFP_PICTURE_FILE are ignored and left intact.
Keep their ignore rule to protect any existing personal preview settings.

The shared resolve shader applies the fixed grade exactly once at final
swapchain output. Native-HUD intermediate resolves remain neutral; SMAA grades
its final color input once before filtering. No render-thread file access.

## Validation

Before removal, Windows gameplay confirmed the Uplink contours and position
marker; production copy fixtures passed D3D11, Vulkan and OpenGL recovery.
Those copy paths remain unchanged. The final permanent-grade change is checked
by test_color_grade.py: real WARP pixels for the fixed curve, alpha, black/white
clamping, neutral intermediates and no repeated grade. The pixel body is shared
in tests/support/color_grade_pixels.h with the native RHI GPU runner, so Mac and
Linux execute the actual resolve shader. Linux also runs the HLE framebuffer
fixture with native section garbage collection rather than Mac linker flags.
A contradictory preview
INI is present during the test and must have no effect or be modified.
Menu fixtures now check the original Graphics row order with no Picture entry.
The prediction-target fixture covers all five copy replay operations, including
the destination-scaled framebuffer copy, and checks that prediction cannot
write them into shared effect images.
Native checks on snapshot61218a4a: Mac Apple M4 passes10 HLE cases; copy/grade
pixels pass under KosmicKrisp and MoltenVK. Full KosmicKrisp passes; full MoltenVK
fails the separate visibility-query readiness assertion. Linux/WSL software
Vulkan passes the common suite and6 HLE cases. Record feature and full-suite
results separately; do not mask the MoltenVK failure or infer Steam Deck/RADV
hardware acceptance from software Vulkan. See STATUS.md for source/hash receipts.
Native mission gameplay and exact original-console color parity remain
unverified. Replay frames alone
cannot establish the map, because host copies are not serialized in captures.
