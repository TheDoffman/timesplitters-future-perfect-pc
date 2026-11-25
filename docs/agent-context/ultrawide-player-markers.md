# Ultrawide Arcade player labels and actor icons

Shared placement policy lives in src/interface/pc_display_ui.inc.
US sub_0012DBA0 draws the local actor's name from the viewport left plus the
HUD margin (7A6140). Patch 2810 wraps this complete label with LEFT placement,
including font measurement, tint and fade. This fixed HUD label must not
inherit centered 4:3 fitting.

US sub_0013C560 projects actor head positions, then emits two icon rectangles
through sub_0001BA70 at return PCs 13CF7A and 13D04E. These are the left and
right skull halves. Their third argument is each half's inner edge;
the second draw reverses X bounds to mirror its texture.
When a name is visible, the guest deliberately separates the inner edges:
13CDA9 stores the measured text's left edge, and13CF9C advances it by its width
before the other half. The measured width is local+28, or stack+60 at either
rectangle entry (hex offsets). pc_ui_actor_icon_shape adds/subtracts half that
width to recover this actor's projected centre, then fits each rectangle about
it. The name opening shrinks by the same aspect factor as the text; without a
name its width is zero and the halves join. There is no cross-actor cache.
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

Patch2815 extends the existing41E30 outlined-label wrapper after the final
Developer Tools wrapper. pc_display_actor_name scopes only return PC13CE13,
preserving prompt/read scopes and all five original text passes. Its per-label
tag initializes PcUiWorldAnchor with both X and Y from the first complete text
batch. The RHI viewport and scissor use that same anchor; the native GL adapter
uses the same shared policy. Tall output retains projected Y instead of pulling
the name toward screen centre. Existing objective-tag142D10 X-only behavior,
unrelated text, partial views and split-screen retain their prior behavior.

Historical controls/Mac/native/texture/graphics snapshots defer2815 alongside
its later Developer Tools and viewport prerequisites. The complete upgrade
installs it; the Mac transition explicitly checks both the outlined-name wrapper
and projected-Y viewport helper. The controls fixture also defers2810 until its
graphics prerequisites arrive. Current runtime validation includes both hooks.

Validation: tests/graphics/test_ui_layout.py executes the production adapter
with deferred batch spies, seven aspect ratios (including 3440x1440 and 5120x1440),
two independently positioned actors, both icon calls, unchanged UV/colour words,
Crosshair Size 150%, local-name text passes, restoration, and split/partial-view
fallbacks. Named cases cover three text widths and mirrored geometry. Production
viewport/scissor checks compare text and skull edges at18 display shapes,
including square, portrait and unusual window sizes. A685872-case sampled
dimension sweep checks physical-pixel size, centres and name openings. Controls
restore the former midpoint fit or omit projected Y; both must fail. The existing
outlined-text scope fixture checks the new wrapper on PC/native macro paths,
retaining the helper's PC-controls/native-display guard. Prompts-only builds
must not compile the display helper as an unused static function under -Werror.
Six asset-free D3D11 renders verify actual name position and pixel size at640x480,
1280x1024,1920x1080,1920x1200,3440x1440 and5120x1440. These are Windows renderer
and shared-policy checks, not physical Mac/Linux or every-monitor acceptance.

Windows Release build with D3D11/Vulkan and the standard post-build checks
passed on this computer at base1f6fa9da plus the local patch. Generation reports
zero failed functions. Matching executable identity and crash symbols are sealed.
The user confirmed both corrections in the isolated 3440x1440 Test Lab build.
Mac/Linux gameplay remains unverified.
