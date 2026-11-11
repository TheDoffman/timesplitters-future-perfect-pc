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
`123F3..123F8` callbacks occupy verified INT3 padding. Original
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
Closed connections return to the same native browser with a reason. Shared
online lobby descriptor `4E9594` ticks through `1A4510`, in addition to the
System Link `1A8D30/1A8F30` ticks. The stock transport `318A0` can wait forever
for callback `31680` after the directory has removed the room. The new frontend
only takes its already-detached cleanup branch when its owned controller
confirms terminal closure (guest role, teardown phase18, scene101). It still
executes the original local socket/session/library cleanup; gameplay, migration
and live-session shutdown retain their existing paths.
Return to Lobby retains the party through the original round-return path,
distinct from migration departure. Lobby code copy uses a native footer target,
keyboard C or the original Xbox right-stick press.

Validation (2026-11-11), development branch only:

- Windows build-t and physical Mac build-g/built-in checks pass with matching
  identity `ee1c8b0d51132305b9770715d57a94c2ad2e3a7a1562469a0f84a79515905d57`.
  Network/release regressions passed 322 tests (9 skips,10 subtests); focused
  Windows controller/IPC/guest-dispatch/FOV checks passed 32 (2 skips), Mac 31.
- `native-ui-cross-d` uses ordinary native input on Windows and the physical Mac:
  an invite-only room stays out of listings, code paste/copy works, original
  Options selects Deathmatch, both games play with advancing timers, Return to
  Lobby retains both players, and host closure returns both to native browsers.
  It also verifies LAN Back and reopening Community Browser on Windows.
- `native-ui-pair-n` exercises separate private hosting, server-invitation
  copy/paste, native create/browse/join, independent guest leave/rejoin and host
  closure. Installed private components on both OSes pass retained-directory
  and owner-exit tests, releasing both TCP/UDP listeners.
- `native-ui-grid-d` checks three authenticated lobbies, two cards/page,
  measured long titles, and forward/back paging. Captures show the full panel,
  thumbnails, mode icons and feedback fitting above the native footer.
- Earlier closure probes reproduced a genuine guest teardown wait and failed;
  the final Windows/Mac runs verify the fix. Early F2 clipboard probes conflicted
  with Settings; C/R3 replaces it. A separate Assault probe stopped at normal
  SELECT TEAM because the driver omitted confirmation; it proves no load defect
  and is not counted as full Assault gameplay acceptance.

The mini's display-metadata backend is deployed privately after an idle check;
health and authenticated host publication pass. Optional private hosting is
installed separately on both test machines. Story keeps its existing build and
platform limits; new Story gameplay, physical controller/mouse acceptance and
WAN reachability are not claimed by these keyboard/SDL tests. No main merge or
release was performed.

IPC version2 adds bounded native scene/mode identifiers and a separate `.match`
message; metadata publishing never consumes a UI request sequence. The host
publishes localized map/rules from the frontend selection `78AD58` in the lobby
and the active `7DE400` selection during gameplay (stock `353400` advertisement). The hidden client validates all fields and sends details only as current authenticated host. Backend display
support remains private. Details never control admission, epochs or restoration.
Two rows/page use the original `1CBDA0` painter for verified Arcade map/mode
pairs, briefly restoring a scratch advertisement after painting; unknown details
retain a text fallback. Native thumbnails never use remote resource pointers.
`crossplay_peer.py` accepts `native_ui=true` to omit legacy desktop automation
while keeping isolated saves, read-only probes and ordinary SDL input.

Private browsing tracks the selected server independently of an owned background
directory. Switching to a friend's invitation never reuses the owner's loopback
route; Host Server reconnects the existing owned directory. Background owner
shutdown cannot interrupt an unrelated community/private connection.

LAN retains native discovery/gameplay; its browser/lobby Back resets that native
transport and returns to the shared network chooser. Reopening Multiplayer
cannot remain stuck in the passthrough state after leaving LAN.

Native guest-dispatch fixtures use C11 on MSVC, matching production and the
compile-time IPC layout assertions exposed through `pc_online.h`.

Copy uses C/R3: F2 remains the reserved game Settings shortcut. Browser action
rows use the original normal font; measured card titles fit before thumbnails,
with full lobby/map/mode details in the selected-row footer. After the original `78980` rebuild restores selection, the adapter selects a
surviving button ID through native `77110`, avoiding orphaned focus after paging.
