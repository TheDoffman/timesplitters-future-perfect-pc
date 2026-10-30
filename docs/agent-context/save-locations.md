# Save-folder selection (discussion 401)

The installed default remains `<game folder>/Saves`. Development fixtures retain
their explicit roots. `save-location.txt` beside RECOMP_INPUT_CONFIG is shared
by the game and launcher. Existing absolute choices are retained; unavailable
configured folders fail visibly. Selecting default removes the preference so
moving a portable installation does not pin it to the old absolute location.

Game: pc_storage_routes.inc owns selection/validation/persistence/UDATA routing;
pc_storage_menu.inc offers current/default/Choose folder in the original profile
screen, not a second drive menu. Native picker adapters live in pc_controls.c
(Windows window message / Shell picker) and settings_macos.m (Cocoa main queue).
They publish bounded paths to a guest-thread completion poll, release input and
inhibit the hang watchdog while the picker is open. Cancel changes nothing.
New picker targets must be empty or contain UserData; drive roots are rejected.
The existing TSFP_SAVE_LOCATION_LOCK still protects isolated test saves.
Future profile opens switch immediately; existing handles/cache images stay put.

Launcher: SaveLocations.cs resolves the same preference. SavesPage shows the path
and offers Use this folder or Copy current saves. Copy requires an empty target,
keeps originals, copies sparse regions and checks SHA-256 before committing the
preference. Cancellation/failure keeps the old preference; partially copied
files may remain at the destination, never replacing existing files. A dedicated
save directory is required because explicit restore replaces that directory.

SaveBackups follows the selected location. Custom-folder restore stages on its
own volume in the parent's `.launcher/save-restore-<installation hash>` directory;
an installation-local save-restore-location.txt locates that transaction during
recovery. Default-folder transactions retain the previous layout. Unknown files
stop recovery; commit cleanup retains the existing verified undo-backup contract.
Open saves and pre-launch validation also use the shared resolver. Backup archives
remain in the installation's Backups/Saves folder.

Checks: tests/storage/test_storage_menu.py covers live route/persistence,
folder rows, root/non-save rejection and preserving existing files. Shared
launcher_saves.cs covers copy/default/legacy missing path and custom restore;
launcher_shared_ui.cs checks location controls and cancellation. Run Windows
build-launcher.ps1 -Test and tsfp.ps1 -Action Build. Native Mac must validate its
Cocoa chooser and Avalonia page on a Mac; Windows results do not prove that UI.

Picker completion is consumed only by the menu that opened it; unrelated profile
updates leave it pending. The guest callback fixture in test_fov_settings.py
covers owner isolation, success, cancel and rejected-folder feedback. Bounded
SAVE-FOLDER log events record status without logging the chosen private path.
