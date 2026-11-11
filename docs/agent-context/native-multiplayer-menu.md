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
`123F3..123FA` callbacks occupy verified INT3 padding. Original
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
The host screen shows Invite Code / Copy / Next; component configuration owns
its ports. Community relay players need no forwarding.

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

## Flow alignment in progress (2026-11-11, uncommitted)

The current task follows the user's diagram and ignored interactive mockup.
Browser layout follows stock `1A4780`: flags2000/width60/spacing2.5,
7-unit Start New Match and20-unit `1CBDA0` advertisements inside its35-unit
`780E0`/`781A0` scrolling region, menu flag100 and stock panel-height constant
47DAC4. All20 bounded results remain in one list. Refresh is footer action2
(keyboard R/native X and existing footer mouse handling); Select/Back are also
explicit. No paging, refresh or code-entry action rows are inserted.
`75400`/`77140`/`77190` finish geometry and `75E70` publishes bounds. Do not copy
`1A42E0`'s `70F30`: it hides a different network-dialog parent; applying it to
this list makes the browser disappear. Native captures caught both missing
scroll containment and that incorrect finalization assumption.
Verified Arcade metadata uses the original painter; unknown metadata keeps a
safe text fallback.

Story previews use a scoped verified mission index in `pc_story_browser.inc`,
independent of the active connection's mode. A valid local Arcade-map placeholder
keeps the stock painter's initial lookup bounded; the existing Story adapter
replaces its thumbnail and chapter caption. Scratch advertisements, preview
scope and fonts are restored after each row. Additive patch3030 admits the two
existing Story painter wrappers on native-display builds. This is shared UI
adaptation, not native Story gameplay support or a guest/network-mode change.

Game Type and Privacy are actual native type1 choices using the common
`7D480`/`717D0`/`71CF0`/`75430` interfaces and callback123FA, rather than text
containing angle brackets. Their values survive changes to another field.
Selection-specific chooser/private descriptions attach through the original
`71730(item,text)` API, so the game's ticker clips them beneath its header/logo.
The initial custom strip overpainted the logo and was replaced after native
capture review. Successful controller replies no longer add generic prose.
Host a Server starts the separate component before its Invite Code / Copy / Next
page; Join Server and Join Match retain field / Join without extra Paste rows.
The Story Online entry returns to the same Community / Private / LAN chooser.

The present backend still implements unlisted rooms and returns join codes as
listing identifiers. Do not describe that as the agreed visible Invite Only
model or expose an unlisted code in a public listing. Completing public IDs,
secret admission and the locked-row prompt depends on its private source/API;
the user has been asked for that source location. This repository remains
player-side only. Native Story release/platform eligibility stays intact.

Focused controller/menu/Story checks passed79 with2 Mac-only skips; repository
context fixtures passed21. Subsequent browser-region checks passed4 with2 skips.
Supported Windows Build/built-in checks pass. Isolated native captures confirm
contained list scrolling, mixed Story thumbnail/chapter rendering and privacy
left/right changes. Choice labels use native left alignment (4100), with
right-aligned native value widgets (8400), keeping Invite Only clear. This is
display/input acceptance using local HTTP metadata, not live multiplayer or
private hosting acceptance. Earlier branch evidence below is historical.

## Earlier branch validation (2026-11-11), development branch only

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

Join Match paints a lobby-code field beside its label (`123F9`), with
an Enter code placeholder. It reuses the original gray keyboard-field background
and bevel through `72560` (the stock `7C6D0` painter calls it), including native
palette and fade; no custom border/fill is drawn. The whole native row remains clickable
and opens the existing text editor; accepted/pasted values appear in the field.
Native rectangle/font primitives retain the game theme and original hit testing.
The field receives initial focus on Join Match, and smaller native text keeps
the label separate from the box while fitting a complete 16-character code.
Windows menu compilation/FOV checks pass (4 passed,2 platform skips); the native
empty-field capture confirms spacing, placeholder and focus appearance.
The gray-field native Windows probe uses ordinary window focus/text/key events
(no replay masking or guest writes): entry opens, sixteen characters are accepted,
and the resulting full code fits in the field. Empty and populated captures verify
the original gray bevel, placeholder, spacing and restored font scale.

The same native gray field (`123F9`/`72560`) now paints lobby name and private
server invitation rows; raw names/invitations retain their existing limits and
paste behavior. These pages initially focus their editable field. Page switches
reuse one descriptor, so an already-open page re-enters stock phase2 through
`75980`, restoring the original opening animation/cue; initial phase1/2 opens
and same-page refreshes do not restart it. Activation/Back use native table slot0
(as stock `1A4350` does), and creation choices use slot8, via the existing guarded
sound helper. Muted/missing sound tables stay silent; keyboard-editor sounds
remain owned by the original dialog. The compiled fixture covers transition
restart versus initial-open/same-page suppression.
Windows `fields-sounds-c` captures verify both fields, creation/privacy navigation,
Back, and opening the original lobby-name editor. Menu/FOV fixture checks pass
(4 passed,2 platform skips), including actual activation and Back dispatch,
blocked/muted activation, and transition restart suppression. Tests a/b of the
foreground-only driver stopped on a focus guard; they are not product failures.
