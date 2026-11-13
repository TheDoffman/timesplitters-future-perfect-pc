# Native pause glass

The original US title captures the scene in `sub_002E55F0`, downsamples/filters
through scratch sprite -2, then copies a subrectangle into the player's pause
picture (`pause + 4`) with `sub_00018790` / D3DDevice_CopyRects. The painter
`sub_002F3460` uses that image under the original animated glass/grid/rings.
The guest owns capture eligibility, picture lifetime and split-screen rectangles.
No replacement artwork, new setting or generated guest edit is needed.

Shared patch `2730-pause-glass-texture-copy.patch` adds the missing GPU-to-GPU
offscreen CopyRects route. Only an existing rendered level-zero source with
matching guest allocation/layout is authoritative; CPU-only uploads retain their
native result. Surface parent/data checks reject other levels and self copies.
The native XDK body consumes its stack count/pointers; snapshot those arguments
before calling it. Reading them afterward silently became a full corner copy.
Rectangles retain their source pixels and destination offsets; no-rectangle
copies use the overlapping dimensions. Bounded unsupported routes retain native
fallback. The destination cache is shared with normal texture binding.

The common RHI copy pass uses the source texture's logical dimensions rather
than the current gameplay viewport when converting to physical GPU pixels. It
preserves untouched destination pixels and restores output/viewport state. The
record wrapper queues the copy behind pending draws and retains prediction's
shared-image write guard. The legacy GL adapter retains its existing unsupported
screen-copy fallback. No Windows-only gameplay policy is added.

The loader orders2730 after temporal-map, surface precision and MSAA copy-sync
extensions. Keep the original patch bytes and loader dependency together.

Validation routes: `test_framebuffer_sampling.py` exercises actual HLE routing
and rendered-source identity, `tests/support/rhi_screen_copy.h` checks real GPU
pixels through `test_rhi_menu_sampling.py` (Windows WARP) and the native RHI
runner, and `test_prediction_targets.py` verifies all four copy replay operations
respect prediction. Repository context/patch-stack tests cover migration/order.

Local Windows executable is built using the existing runtime checkout based on
9554e8d (0.6.0); the relevant renderer/menu sources match task base c1e67e42.
This is not a current-main release build. Native Mac/Linux and split-screen
visual acceptance remain pending. Actual Story screenshots and private test
receipts stay ignored under `private/pause-glass` and `logs/`.

Windows acceptance (2026-11-13): supported Build -SkipTextures and built-in
checks pass. Twelve focused HLE/cache/WARP/prediction checks pass, including a
mutation that reproduces consumed native arguments. Twenty-one context checks
and three pristine full-stack/repeat/CRLF/conflict checks pass; stack fixtures
reuse the existing runtime Git archive read-only, with current-main patches and
loader from this worktree. First-mission gameplay, animated pause, resume and
reopen were captured at1280x720 with disposable Test Lab saves. The captured
scene is now visible through the moving glass. The final NULL-offset edge case
was verified by WARP pixels after the live capture. Other scenes/resolutions,
split-screen and native Mac/Linux visual acceptance remain unverified.

Aspect-ratio follow-up: patch2731 wraps native2E55F0 capture and2F3460 backdrop
through shared `pc_display_pause_glass` in pc_display_ui.inc. Capture/filter draws use STRETCH/full-canvas placement; the backdrop scopes only
its native picture blit (return PC2F3592) so the OPTIONS heading keeps its
original draw scope. Native viewport, UVs,
projection, colors and animation remain untouched. Flush before entering/leaving
and restore the prior placement/tag so menu labels retain proportional layout.
This avoids a centered HUD transform moving partial scratch pixels away from
native CopyRects regions as the display aspect changes. No pixel-size preset
or OS-specific path is used. ui_layout.c covers capture/backdrop scopes at seven
ratios, deferred boundaries and parent restoration. Runtime hooks are shared
with native-display builds; Mac/Linux visual acceptance remains pending.

Scaling acceptance (2026-11-13): final Windows Build -SkipTextures and built-in
checks pass. Eleven UI placement/viewport checks pass, including seven display
ratios for capture taps, the backdrop picture and preserved header placement.
Twenty-one context checks and two pristine-stack/repeat/CRLF checks pass.
Actual first-mission gameplay/pause/animation/resume/reopen was visually checked
at640x480 (4:3),3440x1440 (ultrawide),3840x2160 (4K16:9); image dimensions and
menu-state receipts match each requested mode. The glass now fills the viewport
and OPTIONS heading remains visible. Screenshot paths are private/pause-glass/
scaled-WIDTHxHEIGHT-*.png. Final executable SHA256:
DD0166A4B985105B5E0C9020B8EA5E1EE0B7F8A0FDDB636AD7A6B4081A30A187.
The existing Pause Glass Preview desktop shortcut uses that executable. Personal
saves/settings were retained. This local preview remains based on0.6.0; no PR,
push or release was made. Other scenes, actual32:9/split-screen, live resize while
paused, and native Mac/Linux visual acceptance remain unverified.
