# Video Options click feedback

The redesigned frontend and pause screen share `pc_display_menu.c`. Video
setting IDs are 0/1/2 (aspect/resolution/FOV) and 7–11 (texture, frame rate,
subtitles, Kill Feed, SMAA). Graphics choices are IDs100–104; FPS/Ping is ID105.
The three tabs share click feedback; confirmation/restart prompts are excluded.
SMAA is on Graphics and keeps staged Apply/Back behavior across tabs. Apply is ID3 and keeps its original feedback.
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
