# Pause Settings hub

`pc_display_menu_button` reuses the native pause Controls row at return PC
2F561C as **Settings**, without inserting an extra row. The former Audio row
at2F5666 keeps its native builder record/setters but is disabled/hidden and has
zero layout height. Native Continue, Restart and Quit/network leave behavior
remain owned by the game. Main-menu Settings is unchanged.

`pc_pause_settings.inc` replaces `pc_quit_menu.inc`. It builds the pause hub in
the same Audio Options / Video Options / Controls order as main Settings.
Children use existing callbacks2EC1F0,12008,2EC1A0. Their tab layouts, staged
edits, profile saves, Accept/Cancel and unsaved-change confirmations remain
native/existing shared implementations. Hub Back returns to the original pause
descriptor/context; it does not apply child changes or close the game.

The shared menu factory wrapper composes with the existing multiplayer/Story
factory policy. While the hub owns that player's pause context, native final
returns to the original pause descriptor are redirected to the hub. Other
parents, child dialogs and frontend descriptors are untouched. Opening a fresh
pause (capture bit80) or leaving gameplay clears the route; the next Start opens
the root pause menu. Child-return routing never replaces native save/discard
work or confirmation callbacks.

Unused callback padding12018/12019/1201B now serves hub open/build/back. The old
desktop-exit callbacks, confirmation code, menu rows and fixture are deleted.
`pc_display_menu_finish` still supplies main update notices and Story pause
rows/layout before/after original finalization. The legacy quit-desktop runtime
patch stays because those shared hooks are still needed; no new patch or guest
generation is required for this menu change.

Tests: `tests/graphics/pause_settings.c` through `test_quit_menu.py` checks all
four player contexts, native child destinations, Back, fresh pause, retained
finalization hooks and absence of desktop-exit rows on both adapter defines.
`fov_settings.c` checks the actual root button rewrite, unchanged main buttons
and hidden Audio's zero height. Existing child settings and multiplayer/Story
fixtures continue to exercise save/cancel/leave behavior.

Windows supported build and58 menu tests pass (two Mac-only compile checks
skipped). Actual1280x720 first-mission navigation passed: root has Continue/Settings/Restart/
Quit, hub has exactly Audio/Video/Controls, each child opens and Back returns
to the hub, hub Back returns to root, and a fresh pause reopens the root. Main
root has no Desktop row. Final screenshots/receipts remain ignored under
private/pause-glass/settings-*. The supported Windows build passes. Native Mac/Linux,
split-screen and network gameplay acceptance remain separate. The local preview
uses the existing0.6.0 build checkout; source changes on this branch are based
on20d0f5ef. Game data, screenshots, settings and saves remain local/ignored.


## Shared panel spacing and binding cells

Follow-up base6239ce95: `settings_panel_anchor` in
`pc_settings_spacing.inc` aligns Audio, Video and tabbed Controls to24% of the
logical canvas. Pages that exceed the available height move up to keep their
bottom at84%; invalid heights retain the existing anchor. The existing per-menu
anchor keeps tabs steady. Confirmation/capture dialogs keep native positioning.
Audio body leading1.0 and Controls body leading0.9 are set through native71B10
before layout; header leading and font sizes are unchanged. Never resize cached
row geometry after finalization: custom slider caches must remain consistent.

`kb_label_cell` measures fitted glyph bounds and uses `pc_ui_caption_y` to
center action labels and primary/alternate keycaps in their existing highlight
cells. Mini-keyboard labels, headings, hit targets and binding behavior retain
their existing paths. Both frontend and pause menus share this implementation.

Validation: supported Windows build passes;16 focused graphics checks pass,
two Mac-only compile checks skipped. Actual1280x720 frontend/pause Audio,
Video, Controls, Controller slider and keyboard binding pages1/2 were checked.
`settings_panel.c` covers shared anchor/footer bounds and unchanged cached row
geometry; `ui_layout.c` covers fitted caption midpoints with varied bearings.
Screenshots/receipts remain local under private/pause-glass/menu-spacing-final.
Native Mac/Linux visual acceptance remains pending. The desktop preview still
uses the existing0.6.0 build checkout with these changes selectively applied.

Right-edge follow-up (base64eb0399): KB_LIST row and paging endpoints use
native71B60 with77 canvas-percent for the78%-wide unified panel (84 for
legacy85%-wide panels). This leaves a1% inset before native row/highlight
construction; text, keycaps, header columns and pointer slot geometry all use
the same resulting bounds. Never trim only the drawn highlight. The previous85
endpoint clamped to the outer panel edge, allowing its native highlight to
protrude. Windows supported build passes; live acceptance receipts remain
ignored under private/pause-glass/keyboard-edge.
Live1280x720 acceptance: all16 binding pages (four groups, two pages, both
frontend/pause) pass endpoint bounds; captures confirm inset highlights and
Alternate columns. Existing centered captions remain intact.

The standalone hub fixture declares Story pause hooks/counters only on
Windows, matching the production finalization guard; Mac warning-as-error
compilation must not retain unused Windows-only stubs.
