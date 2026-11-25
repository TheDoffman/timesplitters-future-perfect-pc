# Ultrawide Arcade player labels and actor icons

Shared placement policy lives in src/interface/pc_display_ui.inc.
US sub_0012DBA0 draws the local actor's name from the viewport left plus the
HUD margin (7A6140). Patch 2810 wraps this complete label with LEFT placement,
including font measurement, tint and fade. This fixed HUD label must not
inherit centered 4:3 fitting.

US sub_0013C560 projects actor head positions, then emits two icon rectangles
through sub_0001BA70 at return PCs 13CF7A and 13D04E. These are the left and
right skull halves. Their third argument is the common projected actor centre;
the second draw reverses X bounds to mirror its texture. pc_ui_actor_icon_shape
fits both halves about that shared argument, preserving the reversal.
Fitting each half about its own midpoint separated them on wide displays (#603).
The primitive adapter uses STRETCH placement for emission. XY arguments are
floats at stack+4..+16; UVs and colours
stay unchanged. Every argument and enclosing placement are restored after the
batch flush. No whole actor loop shares a first-actor anchor. Crosshair Size
never scales actor icons. Ordinary weapon/zoom reticle behavior is unchanged.

Both changes retain the existing single-local-view guard: original 4:3 and
split-screen layouts remain intact. Shared source covers Windows D3D11/Vulkan,
Linux Vulkan and native Mac/legacy GL; this is not per-platform duplication.
No profile, player name, team colour, icon visibility or network state changes.

The historical controls-only upgrade fixture defers patch 2810 until its
graphics prerequisites are installed, then verifies the player-name hook in
the full upgrade. It does not omit the hook from current runtime validation.

Validation: tests/graphics/test_ui_layout.py executes the production adapter
with deferred batch spies, seven aspect ratios (including 3440x1440 and 5120x1440),
two independently positioned actors, both icon calls, unchanged UV/colour words,
Crosshair Size 150%, local-name text passes, restoration, and split/partial-view
fallbacks. Half textures must meet at the same projected edge, including lone
halves and consecutive actors. The old midpoint-fit negative control must fail.
The same suite checks production viewport/scissor math. These are
Windows-compiled regression checks, not visual gameplay acceptance on all hosts.

Windows Release build with D3D11/Vulkan and the standard post-build checks
passed on this computer at base1f6fa9da plus the local patch. Generation reports
zero failed functions. Matching executable identity and crash symbols are sealed.
The user confirmed both corrections in the isolated 3440x1440 Test Lab build.
Mac/Linux gameplay remains unverified.
