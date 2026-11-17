The optional Fidelity Comparison controls were removed; normal display settings
and saved preferences apply. [Independent sun repair](fidelity-comparison.md).

# Video Options click feedback

Section selectors share `pc_settings_navigation.inc` with Controls: native
highlight sound on hover changes, switch sound only after a real section change,
global physical Left/Right arrows and controller LB/RB, and adaptive footer hints.
Native A/D value adjustment remains. Existing staged Apply/Back policy is retained.

The redesigned frontend and pause screen share `pc_display_menu.c`. Video
setting IDs are 0/1/2 (aspect/resolution/FOV) and 7–11 (texture, frame rate,
subtitles, Kill Feed, post-process AA). Graphics choices use IDs100/102–104;
FPS/Ping is ID105. Advanced navigation is ID108, return is ID107, clear cache ID106.
The three tabs share click feedback; confirmation/restart prompts are excluded.
Post-process AA (Off/FXAA/SMAA) is on Graphics and keeps staged Apply/Back behavior
across tabs. MSAA and shader cache moved to `DISPLAY_ADVANCED`; Back returns to
Graphics and that tab stays highlighted. Apply is ID3 and keeps its original feedback.
See [SMAA and staged settings](smaa.md) for persistence/rollback contracts.

`menu_video_click_sound` handles fresh mouse clicks on these enabled setting
rows only, scoped to the live Video descriptor and owning menu. It respects the
guest menu-sound enable flag US7BA16C and validates sound table US7BA178 before
calling original US21780 with its +8 switch sound. Original Previous/Next choice
helpers US755D0/756A0 use that same sound (+8 at US75644/75713); their native arrow
feedback remains untouched.

`pc_menu_mouse_guest.inc` invokes this helper only for a fresh non-arrow left
click. Hover/held buttons do not replay it. `pc_menu_sliders.inc` also invokes it
once when a fresh click captures the FOV slider, without repeating while dragged.
Other menus, disabled rows and resolution confirmation retain their existing
feedback. Shared guest sound dispatch needs no new OS audio adapter or assets.

Checks: `tests/input/test_menu_mouse.py` covers routing, silent hover, held-click
suppression, arrow deduplication and FOV capture; `test_fov_settings.py` checks
actual Video scope, all selector IDs, disabled/confirmation/muted/missing-table
guards and guest ABI. These dispatch tests do not establish physical audibility
on a native Mac host.

Combined graphics flash coverage includes SMAA Off/On alongside MSAA, AO
and FXAA in `tests/graphics/graphics_transition.h`, using isolated runtime
preferences and restoring SMAA Off at completion. Native validation is separate.

## Weapon aperture

Video Options > View & HUD includes Weapon FOV (row12), shared by frontend
and pause menus. It now displays a reference angle,40-100 in one-degree steps,
matching Vertical FOV.55 is the original aperture. Projection scale is
 tan(angle/2)/tan(55/2), preserving each weapon's authored lens/zoom rather than
replacing it with a fixed absolute projection. Both rows use native slider
painting and pointer capture; Weapon FOV no longer displays a percentage.
Only caller US1B7482 applies the aperture adjustment. Character previews,
world FOV/culling, scopes, cinematics, aiming and projectiles keep their paths.

pc_display_weapon_fov_degrees/set_weapon_fov_degrees serve the slider. The
legacy percentage APIs retain exact V3/V4 aperture values; rounded degree text
and unrelated Apply/launcher saves do not quantize those existing preferences.
Changing the value writes V5: width height worldFov weaponDegrees crosshair.
Returning to55 restores original sizing and legacy V1-V4 serialization. Apply
still saves without restart; Back discards. The menu snapshots the old percent
mode for exact failed-save rollback. Arm visibility still uses effective aperture.
Shared launcher GameSettings preserves V5 angles/crosshair and rejects invalid
or extra fields. Legacy files keep their original semantics. Tests exercise
bounds, staged input, V5 round trips, legacy no-op preservation, rollback and
projection isolation. See [crosshair sizing](hud-sizing.md) for V4 migration.

## Window mode

Display now starts with Window mode (ID13): Windowed / Fullscreen. Both native
frontend and pause menus stage this value across tabs. Apply previews the window
and any resolution change together; Keep saves the batch. Revert, Back in the
confirmation, and the15-second timeout restore the original mode. Leaving before
Apply does not alter the host window or saved preference. Save failures restore
the previous window mode and retain a dirty draft with an inline error. No restart.

pc_display_fullscreen/set_fullscreen are thin window adapters. Windows sends
PC_FULLSCREEN_SET_MESSAGE to the owning shadow-window thread (additive patch2620)
and reuses its borderless transition; SDL marshals both queries and changes to
its main thread and synchronizes the window. No new refresh-rate choice or override is introduced.
The launcher already had fullscreen persistence; its labelled Fullscreen control
now explains borderless/windowed behavior and next-launch Save semantics.

The common pc_runtime_settings.h writer edits only the named boolean in
runtime.conf, preserving unknown keys and comments. Fullscreen uses the existing
fullscreen key, so launcher and game share startup preferences. SMAA reuses this
writer while keeping its atomic live publication. The helper bounds input at1MiB,
rejects binary/read-only files and publishes a same-directory temporary atomically.

fov_settings.c exercises the actual menu transaction against a disposable host
window: staging, tab retention, discard, Keep, Back, timeout, failed save, failed
window request and recovery. test_pc_display_modes.py supplies the expanded SDL
contract; test_smaa.py carries the shared writer into its isolated source fixture.
Existing launcher settings/dirty-Save tests cover its fullscreen checkbox.

Linux acceptance at7a6fea3c: native build and actual X11 frontend/pause window
transitions, Keep persistence and timeout restoration pass under Ubuntu/WSL
software Vulkan. Linux launcher UI checks pass. Wayland/WSLg self-tests pass on
sequential retry after an initial transient; physical desktop/Deck and native
Mac acceptance remain pending. See linux-game.md and STATUS for measured scope.

## First-person shoulder visibility

Additive patch2720 extends the existing US18EA90 node-submission wrapper.
`src/graphics/pc_viewmodel_visibility_guest.inc` matches the submitted instance
against the active player's separate first-person arms entity (context+E84,
created by US D35B6/D35C0). At Weapon FOV above100, it omits rigid shoulder
roots: parent0, bone1/10, type0, with a validated root and bounded node table.
It does not alter asset data, transforms, animation, projection or world actors.
US8F5E0 still traverses child nodes, so skinned upper arms, elbows, wrists and
fingers remain submitted. Default/reduced Weapon FOV retains original behavior.
Other players' arms are selected through the active viewport context, not P1.

`tests/graphics/test_viewmodel_visibility.py` exercises the production predicate
with captured rig topology, unrelated ownership, retained child/skinned nodes,
original FOV and invalid pointers/counts. It is registered in runtime-patches CI;
visual acceptance and native-platform execution are separate checks.

Crosshair Size uses a staged View & HUD slider and V4 preferences; HUD Scale
removed. See [crosshair sizing](hud-sizing.md) for migration and validation.

Weapon-degree validation: Windows supported game build and full launcher-Test
pass (52 settings checks); focused graphics21pass2Mac skips, repository context
21pass. Actual2560x720 main/pause: mouse plus keyboard fine adjustment, Back
discard, Apply/V5 persistence, live rendering and original55 fallback pass.
The local build/launcher preview binary matches the final supported game build.
Mac/Linux visual acceptance remains pending.

View & HUD slider readouts: fov_label keeps names only; fov_value_draw paints
Vertical FOV, Weapon FOV and Crosshair Size values right-aligned10 logical units
before the existing bars. Native row font, glyph bearings, selection colour and
fade are reused; font/alignment restored. Shared main/pause layout; slider hit
geometry, staging and serialization unchanged. Windows supported build and focused
callback tests pass (4pass2Mac skips). Actual1280x720 main/pause captures confirm
labels/readouts, live mouse/keyboard updates, Apply, Back and gameplay. Preview
binary refreshed; Mac/Linux visual acceptance remains pending.

Tutorial control-hint columns: pc_tutorial_ui.inc measures each native prompt
caption and centres keyboard/mouse icons in one shared column. Normal
US147CAE action icons are centred; descriptions follow the shared row gaps.
Multi-view layouts retain native behavior. All scratch geometry and
column scope restore on exit; tutorial_layout covers unequal widths/rebinds.
Tutorial-column validation:12 layout tests pass, including mixed-width column
centres, rebinds, controller/split-screen guards and scope/ABI restoration.
Windows supported build and actual1280x720 Story Scifi Handgun capture pass.

The standalone tests/input/menu_mouse.c fixture supplies the Crosshair Size
callback beside Weapon FOV; all five mouse hover/drag mutation cases compile
without depending on the complete display-menu translation unit.

Tutorial icon spacing: normal keyboard/mouse icon captions use80% of their
card text scale, with measured vertical centring inside the original row pitch.
Description fonts and wrapping stay native. Both scale axes and x/y arguments
restore after each icon draw; controller glyphs retain their original size.

Hint-row spacing follow-up: tutorial_rows adds six logical units between each
control row and expands the native panel by the matching total. The painter
recalculates its unpadded height cache at the end; padding never accumulates.
Icon and action-description y positions move together; headings, font sizes,
split-screen paths and wrapped text remain unchanged.
Scope and frame arguments restore on return. Windows supported build,12 layout
tests and actual1280x720 Story captures pass; repeated captures confirm stable
panel height and visible keycap gaps. Native visual checks remain pending.

Controller tutorial parity: tutorial_columns now measures controller button and
trigger captions as well as keyboard/mouse captions. Both use the same centred
column and six-unit row gaps. Controller glyphs retain native size; keyboard
captions keep80% sizing. Split-screen remains native.
Controller-face/trigger widths and per-device split-screen guards are covered by
tutorial_layout. Windows supported build and26 layout/read-scope/icon tests pass.
Isolated1280x720 and2560x720 Story captures verify native controller glyphs,
common column centres, row gaps and stable panel height. Physical-device input
selection retains its existing path; native Mac/Linux visual checks pending.
