# Native multiplayer menus

Work branch `feature/native-multiplayer-ui` depends on the unmerged online-system
branch at `5c4df06a`. The user's layout reference is the supplied online flow
diagram. Private Server explicitly means a separately hosted directory/relay,
not an invite-only community room. Preserve the [client/server boundary](online-client-boundary.md).
See the [existing online system](multiplayer-ui.md) and [late joins](arcade-join-in-progress.md).

`pc_multiplayer_menu.inc` uses the original menu/button/font controls for the
network chooser, private hosting/connection, match browser, new-match options
and lobby-code join. It replaces the network chooser's `1A2730` builder only in
the new flow. LAN and final lobby/option screens retain original constructors.
`123F3..123F7` callbacks occupy verified INT3 padding. Original
`7D090(parent,buffer,byte-limit,width,changed-pointer,flags,done,cancel)` supplies
the on-screen keyboard and existing keyboard/mouse editor. Its limit is 255;
Paste handles a complete invitation separately.

`online.py --native` verifies the existing content/build identity, then runs
`online_menu.py` without creating a browser window. Its asynchronous controller
reuses Session/ArcadeSession and the authenticated relay bridge. Newer generations
cancel late results and close their sessions. Community settings and private
server endpoints are separate. A private server invitation (`tsfp-server:` URL/pin)
differs from a lobby code. Browse includes both game types; Story availability
retains existing release/platform policy. The desktop UI remains only for explicit
legacy test automation. The three platform adapters spawn without disabling the
game window or entering the desktop-browser wait.

`pc_online_menu_host.inc` owns bounded file IPC, validating version, length,
counts and monotonic sequence before accepting a bridge. No guest pointers or
service member tokens enter UI state. New starts clear stale per-process files.
Windows terminates an owned timed-out worker before reusing its paths.
SDL clipboard requests use `online_clipboard_sdl.inc` and asynchronous
`SDL_RunOnMainThread`; guest code never waits for a renderer waiting for its
frame. Menus poll paste completion. Upstream contracts:
[clipboard reads](https://wiki.libsdl.org/SDL3/SDL_GetClipboardText),
[dispatch](https://wiki.libsdl.org/SDL3/SDL_RunOnMainThread).

Optional private hosting is discovered in `Support/PrivateServer` or the user's
application-data `TimeSplitters Online/private-server` folder. Its implementation
and installer stay private, outside Git and normal client packages. The client
supervises public invitation/ready and stop handshakes, using loopback locally
while sharing the public endpoint. Stop files handle Windows venv launcher
children. Lobby leave keeps this directory alive; owner closure stops it.
The host screen shows configurable TCP/UDP ports; community relay players need
no forwarding.

After bridge acceptance, host creation uses original `1CBA50`; guest discovery
and selection use `352980`, `352990` and `1CBD40`. Native lobby Back retires its
native session, waits for the directory leave ACK and opens the same browser.
Closed connections need native feedback because no desktop dialog remains.
Return to Lobby retains the party through the original round-return path,
distinct from migration departure. Lobby code copy uses a native footer target,
keyboard F2 or the original Xbox right-stick press.

Validation in progress: Windows builds/captures verify network choices,
browsing, new-match settings, original text entry and host lobby creation. Two
Windows games discover/join the same lobby. First closure probe exposed a guest
still showing its old lobby after hidden-worker closure; explicit error-return
and native reset need rerunning. Controller/IPC/retained-return/FOV checks pass
24 tests (2 platform skips). Installed private hosting passes HTTPS/UDP creation,
lobby leave with retained directory and owner shutdown releasing both listeners.
Latest copy/paste, Story integration, gameplay return, detailed browser rendering
and physical Mac acceptance remain pending. No release/completion is claimed.
