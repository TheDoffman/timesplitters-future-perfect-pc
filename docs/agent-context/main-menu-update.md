# Main-menu update notice

The game displays `UPDATE AVAILABLE - OPEN THE LAUNCHER` in its native footer
font only on the frontend main menu, above the bottom silver line and native
Select/Change Players prompts. It is informational;
updates remain installed through the launcher. No network requests run in the game.

Both WPF `Launcher.Check()` and Avalonia `NativeWindow.Check()` clear the result
before checking and publish the verified selected-channel catalog result afterward.
`GameUpdateNotice` in `GameSettings.cs` compares parsed versions, suppressing
same/older versions, unknown local versions, failed checks and other channels.
`GameSettings.StartInfo` passes absolute `TSFP_GAME_UPDATE_NOTICE` and
`TSFP_GAME_INSTALLED_VERSION` values after clearing inherited game variables.
The atomic UTF-8 sidecar `.launcher/game-update-notice.txt` is installation-local,
not a setting/save. A raw runtime launch without this environment has no notice.

The V1 record is `TSFP_GAME_UPDATE_V1 <UTC epoch seconds> <installed tag> <available tag>`.
`pc_update_notice.h` bounds input to511 bytes, validates tokens/current version,
rejects trailing data/NUL, and expires the result after24 hours (future tolerance5
minutes). The runtime trusts the launcher's release/version policy and renders
only constant text. The main menu polls the local result at most every2 seconds,
allowing a startup update check to finish after Play. Failed/absent results hide it.

`pc_display_menu_finish` records only the actual frontend builder's menu instance
(caller2D0EDF). `pc_display_menu_mouse_footer` draws through `pc_update_notice.inc`
only for that descriptor, owner0, active state3, no child, and a captured native
footer style. Submenus and pause/gameplay do not display it. Drawing measures and
fits the label to the guest canvas, preserves registers/font/alignment/UI placement,
and uses the same shared renderer on Windows, Mac and Linux.

Validation: `tests/graphics/test_update_notice.py` executes the portable parser;
`test_fov_settings.py` checks native drawing, font/register restoration, submenu,
pause/other-menu gating, disappearing and late-arriving results. Launcher shared
UI checks exercise version/channel suppression, clearing and launch routing on
WPF/Avalonia. Both fixtures are in runtime CI. Actual platform results belong in
STATUS; Windows results do not establish native Mac/Linux desktop acceptance.
