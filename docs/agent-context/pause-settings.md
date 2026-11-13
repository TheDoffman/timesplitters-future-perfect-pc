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
