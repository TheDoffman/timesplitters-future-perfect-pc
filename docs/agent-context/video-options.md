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
and pause menus. 80–150% in steps of5 scales both first-person projection axes;
100% is the original aperture. Both FOV rows use the native sensitivity-bar
painter and pointer capture, with percentage text for Weapon FOV. Mouse drags
and keyboard/controller adjustments use 5% steps, retaining Apply/Back staging. Only caller US1B7482 in the existing perspective
wrapper applies it. Character previews, world FOV/culling, scope artwork,
cinematics, aiming input and projectile simulation retain their prior paths.
The adjustment preserves each weapon's authored lens/zoom. It is an aperture
percentage, not an absolute field angle. The value is staged across tabs,
applies without restart via Apply, is discarded by Back, and rolls back if
any batch save fails.

pc_display.c reads legacy V1/V2 with weapon100 and V3 width height world-fov
weapon-percent. Saving Original retains V1/V2 compatibility. Shared launcher
GameSettings reads/preserves V3 when changing other settings on all hosts.
Tests cover projection isolation/aspect/zoom inputs, persistence, staged menu
changes, save rollback and launcher round trips. Host visual testing is separate.

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
