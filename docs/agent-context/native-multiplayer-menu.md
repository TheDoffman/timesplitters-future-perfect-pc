# Native multiplayer menus

Work branch `feature/native-multiplayer-ui` depends on the unmerged online-system
branch at `5c4df06a`. The user's layout reference is the supplied online flow
diagram. Private Server explicitly means a player-hosted directory,
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

Explicit private hosting uses `online_private.LocalDirectory` in the hidden
companion, restoring the former window's HTTPS directory startup. It reuses the
existing certificate/key without replacing either, detects the public IPv4 and
returns a pinned server invitation plus a separate loopback URL for its owner.
Private invitations may additionally carry up to4 RFC1918 IPv4 local_addresses.
`online_host.local_ipv4_addresses` gathers the routed interface and hostname's
IPv4 interfaces without sending discovery traffic. `PRIVATE_CONNECT` tries local
loopback, those numeric LAN hints (same TCP port,1s each), then the public URL;
every candidate must verify the same certificate pin before any request. Server
HTTP errors stop rather than changing endpoint. Old invitations remain valid.
The selected endpoint is retained for browse/create/join. The public pair stays
canonical, so an owned local directory cannot replace a different server.

`Handler` passes the accepted socket's actual local IP into Directory.request.
For loopback or RFC1918-to-RFC1918 requests, replies return a lobby host's LAN
origin; for a lobby hosted by the directory owner through loopback, they return
the actual LAN listener IP. This applies to listing, join and heartbeat; WAN
responses retain public addresses. Neither same-PC loopback acceptance nor LAN
System Link proves that a private invite's public address supports router hairpin
NAT. Tests explicitly refuse loopback/public and complete pinned TLS and direct
UDP via the real host RFC1918 interface; physical cross-device acceptance is
separate. Both peers require matching rebuilt companion identities; copy a new
server invite to include the optional LAN hints.
Native hosting preserves the original desktop window's forwarded port pair:
TCP30820 for the private directory (`MP_DIRECTORY_PORT`), UDP30821 for a default
direct match (`MP_MATCH_PORT`). `mp_ui.port` is the UDP match port; only the
PRIVATE_HOST request substitutes the independent directory port. A chosen UDP
port cannot move the TCP listener. The initial native prototype incorrectly used
41830 for both protocols, bypassing existing30820/30821 forwards. Tests exercise
actual preparation and IPC for hosting and match creation, including a custom UDP
port. The unused port editor now labels/validates a UDP port1024..65535. Incoming
server invites retain their explicit port; copy a fresh invite after restarting
hosting with the corrected defaults. Community41820/41821 is configured separately
and is unaffected by private defaults.

The directory begins empty. Connected players create independent v2 direct-UDP
Arcade/Story matches, with16/4 limits; its256-room and32-worker limits are separate.
Leaving a match keeps the directory alive. Closing its owner stops it. Public
community rooms retain their v3 relay/migration implementation; private matches
use the old direct path and its host-UDP forwarding requirement. A private lobby
host's departure ends that match; there is no newly claimed private migration.
Metadata publishes through the existing Session heartbeat. Normal hosting needs
no external component. Explicit TSFP_PRIVATE_SERVER_COMMAND remains test-only
integration support. The native Host Server page retains Invite Code / Copy / Next, with startup
progress and copy feedback; Join Server supplies an explicit Paste action.

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

## Field transition and code-panel follow-up (2026-11-11)

Checkpoint378fab64 commits visible private-directory admission and preceding UI
work. `mp_code_draw` now requires its own active menu/descriptor and native open
phase3, with native transport handoff inactive. Row callbacks also run during opening/closing and while an outgoing child
remains attached; using global destination-page text there exposed fields before
the native panel. All four owned field pages use the same guard. Caption/input
colours use154FE0 with menu+F4 fade;72560 already fades the native gray bevel.
Native keyboard text editing remains the original7D090 path. Its temporary
edit buffer now uses slot20, separate from the panel heading in slot30, for both
accept and cancel. Opening a keyboard must not replace the underlying heading.

Host/Join Server and Join Match headings use native vs85/vs100 size escapes,
with uppercase conversion disabled on their heading row (the authored labels
are already uppercase). Do not use the identifier small: Windows headers define
it as an RPC type macro. MP_JOIN_CODE now shares the7-unit field, separate heading /
divider, Paste and Join rows with MP_JOIN_SERVER. Its text remains ENTER LOBBY
CODE / ENTER CODE..., and selecting the field still opens the virtual keyboard.

`mp_privacy_draw` uses the original padlock sprite147 from descriptor4D4C04
(table4D2D5C, runtime texture80F20), as the locked mission/challenge painters at
2BEC4B/2BEEFD/2BF1B3 do. It draws with native1ED50/1F090 and the actual texture
width/height at descriptor+26/+2A, preserving aspect within24x30. It sits at
row-left+10 / row-midpoint minus8, below mode text and above population, followed
by small native face3 Invite Only text at x+32 in muted red7F50507F. Both use
154FE0(menu+F4) fade; the icon keeps its original artwork. Native material/text
state and font are restored; there is no drawn-outline replacement. Keep saving
bounds before the stock advertisement painter reuses its rectangle. Titles use
the original full width and the stock mode icon remains clear.

Tests cover separate compact headings, Paste/Join presence and zero field
rendering on transition/stale-owner callbacks. The community service update is
separate from these shared menu fixes; see the operator/developer
[update guide](../development/online-community-update.md). It explains the v2/v3
contract and installation/verification steps without claiming an external service
implementation or deployment from this client checkout.

## Visible Invite Only admission and third revised notes (2026-11-11)

Checkpoint8c81d558 commits the preceding transition/input/layout fixes. The next
notes restore native79710 dividers under Matches Available, Lobby Options and
server invite headings. `mp_divider` gives decorative type9 rows IDs249/248;
FFFFFFFF belongs to LAN Start New Match and must not identify a separator.
Heading rows use the original33006 flags with automatic height, rather than a
size escape plus a forced minimum. Server headings are separate, nonselectable
rows; their7-unit field contains only the invitation. Name captions select the
Game Type row's authored face and leave at least16 units before the gray field.
Custom text explicitly selects the original text render mode and half-intensity
RGB/alpha palette; full255 channels had made the same authored face look heavy.
The lobby footer
now says LOBBY CODE: followed by the code.

Native IPC version3 adds `invite_only` to PcOnlineMenuLobby (244 bytes; state7828).
Request remains2468 bytes; JOIN's address field carries the selected public lobby
ID, other actions retain its address meaning. A locked row clears the input and
opens MP_JOIN_CODE. Wrong-code errors return there for retry; an open row keeps
its existing direct-select flow. Native thumbnail rows paint the original padlock plus Invite Only caption, saving row bounds
before the stock painter reuses its rectangle for the map/mode icons;
unknown-map fallback rows label Invite Only. Capacity and mode admission remain
independent of privacy. IPC validates the new boolean and rejects old versions.

Player-owned v2 directories now list both privacy types and publish
`invite_only: true` with a public `code` identifier. CREATE returns a separate
`lobby_code` only to the host; locked-room listings/admission/heartbeat replies
never publish it. POST /v2/lobbies/{public-id}/join requires `lobby_code` for locked
rooms and verifies it before allocating any peer. Code-only invitations still
resolve within this directory; the reply's canonical code controls subsequent
heartbeat/leave. IDs and secrets cannot collide across rooms. TLS/owner/peer
credentials, expiry and Arcade16/Story4 bounds remain unchanged.

Session.join / join_arcade / ArcadeSession.open accept an optional lobby_id.
Native selection sends that public ID in the path and the entered secret in the
join body; authentication errors never fall back to a different protocol. A full
invitation cannot redirect a selected locked row to a different directory. Host
and guest native footers/invitations use lobby_code, retaining public IDs only
for service RPCs. The legacy invitation window also shares the real lobby code.

Directory GET /v2/lobbies advertises `invite_only_listing: true` when it supports
this contract. Native locked creation requires that capability rather than
silently retaining the old unlisted behavior. The external community v2/v3
service must implement the same listing/admission/owner-code contract and flag;
its private source/deployment is not in this checkout. Until it is updated,
community Invite Only creation reports that the server needs an update. Open
community rooms and LAN keep their existing protocol. Do not claim the community
requirement complete from private-directory tests or display-only fixture rows.

## Second revised notes (2026-11-11, local)

Each custom page has its own16-byte descriptor and32 text slots in one bounded
allocation. `mp_show` clears its active-menu pointer before79B70 and only rebuilds
a child whose descriptor is the destination.79B70 can retain its outgoing child
during closing; rebuilding that child caused the Online/Start New Match flash
and reused browser footer state. Pending responses can update state while a new
page opens; its actual builder publishes the active pointer. Native saved focus
is repaired after opening when it refers to a disabled/header/deleted row, using
the same type4/7/8 and flags&5 rules as mouse navigation.

The custom LAN browser's Back is MP_BACK, not just the original1A4350 callback.
Both paths now invoke original1A4350, opening4E9974. Its1A8440 builder requests
1CC360 shutdown;1A8410/1CC3C0 waits for76B1A4==0 and completes35E960/3565B0
release before the chooser returns. Native validation previously found a chooser
with76B108 still1 and selectionFFFFFFFF; after this repair it returns with0 and
selection1, and both community/private joins after LAN work. LAN selects match
transport without killing an owned private directory's companion; directory
leave acknowledgements do not drive LAN's native UI.

The browser title uses the original33006 heading flags and no extra separator;
a separator with IDFFFFFFFF conflicted with native LAN Start New Match. The
creation heading uses the existing native vs75 size escape with uppercase
conversion disabled, as small_note already does. Creation labels use native large
uppercase controls. Custom fields select their authored font from item+1C, as
stock76193 does, rather than a guessed face ID. Their measurement buffers and
caption strings are separate. Host/Join Server use a centred heading above a
full-width entry field, retaining Copy/Paste/Next/Join and the in-panel share hint.

Guest selection requests directory admission while retaining the browser or
code-entry page and its existing Connecting status. Back cancels admission in
place; errors retain that page for retry. Only native discovery opens MP_JOINING:
1A43E0 /1A4780 builders present the Joining lobby row; its hidden selection contract
retains index0 and1CBD40 after352990 reports an actual advertisement. The original
native browser is not shown between directory admission and the real lobby. Back,
native discovery errors and timeout return to the connected directory browser. No transport shortcut
or fabricated advertisement is used.

The browser keeps its80-unit width and full-canvas placement, with a70-unit
frame and65-unit scroll area reserving a clear status band.

Lobby-code feedback uses the native footer font, a [C] keycap or original right-
stick glyph M, and native hover colour. The hit region uses the same aspect-aware
pointer mapping as other menu controls; click/C/right-stick still copy and show
COPIED. Browser info baseline is raised19 native units to avoid the footer.

Final supported Windows build/built-ins pass (identity a9d7a839), and70 focused
checks pass with2 Mac-only skips. Native c396d8c4 host/guest tests cover LAN ->
Back -> community join/rejoin, and a fresh guest's LAN -> Back -> private pinned
invitation -> Refresh -> direct native lobby. Joining progress and hover/keycap /
COPIED are captured. A fixed-delay leave/switch driver was invalid while native
phase18 cleanup was still running; it was replaced with state-aware navigation.
Final59f4e6c4 captures verify native fonts and stacked panels. Repeated Online /
Private switching completes LIST replies at sequences1/2,4/5,6/7,9/10; Online
fixture GETs advance3/6/9/12. The owned directory remains available across LAN
entry/exit (owner1 at sequences11/12/13). Final a9d7a839 capture confirms browser
messages below the listing panel and above the footer. See STATUS for evidence
and validation limits; physical Mac/Linux acceptance remains pending.

## Revised online notes (2026-11-11, local)

Checkpoint d98a021d saves the restored private directory and prior menu work.
The chooser now has Online / Private Server / LAN without a separate Online/LAN
row; Online's native ticker says Browse matches online via the community server.
The browser uses the original80x75 frame and70-unit scroll region, with the native
thin inset Matches Available row. Browser flags40020000 select the original
full-canvas placement; clear4002a000 style bits before75B70 because it ORs flags
into reused pages. Native745C9/74651 and the original42020102 browser flags
confirm that positioning contract; captures verify the logo is removed.
Start New Match keeps a smaller nonselectable Lobby Options row.

Mouse Refresh is latched by native_multiplayer_refresh_click before the stock
dispatcher consumes the footer edge, then serviced on the next safe online tick.
A click during a pending refresh queues one subsequent refresh; held clicks do
not repeat. Keyboard/controller paths remain. Host Server shows Starting private
server while pending; its invitation field/actions appear once ready. Copy reports
success/failure. Join Server has Paste, and selecting its field pastes the complete
invitation instead of opening the255-character native keyboard. Lobby codes and
names retain their existing editing paths.

PRIVATE_CONNECT first probes the invitation's port on loopback with a one-second
limit and the same pinned certificate, then falls back to its public URL if the
local probe fails. The canonical public identity stays separate from this verified
connection endpoint, including private hosts created by another local instance.
Switching directories cannot reuse a prior local route. This avoids relying on
router hairpin NAT without trusting an unrelated local server or changing remote
friends' invitations. Tests cover actual public invitations against real local
TLS/direct UDP, wrong-pin fallback and switching directories.

Windows Build/built-ins pass; final focused selection70 passes with2 Mac-only
skips. Real community host/guest native lobbies show2 players, and guest leave /
rejoin succeeds. The reported20-second timeout was not reproduced; no transport
root cause is established. A second private game pasted the public invitation,
connected through verified loopback and joined the host's direct match. Final
frame capture b55efb24 shows two complete rows and a third partially visible;
consecutive synthetic mouse footer clicks each made3 fresh fixture GETs (3/6/9).
This fixture proves refresh/UI behavior, not community transport. Test instances,
companions and disposable disks were cleaned. Mac/Linux native acceptance and
visible Invite Only admission remain unverified/unfinished respectively.

## Private directory restoration (2026-11-11, local)

The user's clarification supersedes the separate-component assumption in the
historical notes below. Default Host a Server invokes the built-in former
LocalDirectory flow, not a service installer. `online_private.py` derives the
bounded v2 registry/TLS certificate handling from the pre267a53f4 implementation;
community service modules remain excluded. The native controller remembers the
selected directory's explicit direct capability, including after joining by
invitation and switching back to its owned directory. Only that private path
creates Session direct matches. New code is in the shared companion tuple and
network identity; private service source is not needed to host a player directory.

Isolated Windows hosting/client/identity checks45 pass with real pinned HTTPS and direct UDP:
empty directory before creation, two independent Arcade/Story hosts, guest ACK,
lobby leave retaining the directory/other room, owner shutdown, certificate reuse,
credential rejection, limits, expiry and metadata. The initial sandbox run could
not load Git OpenSSL DLLs; the normal-host rerun passes. Supported Windows Build
and built-ins pass with identity8f208ead. Native input verified Host Server, a
generated invitation, Next to empty browser, Create Lobby to the original match
lobby, and Leave to the retained browser. The same owned41830 TCP listener
survived lobby leave and disappeared on game/companion exit; isolated certificate
state and disposable saves were used. Release/package/context checks191 pass
with10 platform skips; other host build/device acceptance remains unrun.
Invite Only public-ID/secret admission
remains a separate unfinished requirement; restored v2 privacy is historical.

## Online notes follow-up (2026-11-11, local)

Rebased onto main e7cb1016; upstream controls, release records and code are
retained. The local checkpoint commit was replayed as a40c8e7a. Header/link
conflicts were adapted without changing upstream behavior. Context guide byte
limits required compact references after combining their newer headers.

Main row3 at verified builder callsite2D0DAF now says Multiplayer; its original
ID, selection dispatch and descriptions remain. The chooser's outer heading is
Multiplayer and its nonselectable inner title is Online / LAN. Creation uses
Start New Match outside and Lobby Options inside. Browser headings are Match
Browser / Matches Available, with one shared fixed80-wide/60-high native frame
and50-high scrolling region for community, private and LAN. Title and separator
sit outside the scrolling region. LAN builders1A43E0/1A4780 retain stock352990
advertisements and1CBA50/1CBD40 creation/join callbacks. Empty and populated lists
use identical frame dimensions. Footer text has separate storage from fallback
match captions. Tests exercise two headings, fixed dimensions, retained LAN
callbacks and all20 rows. Windows Build/built-ins pass on rebased version0.6.1.
101 focused/client-boundary/context checks passed with2 platform skips. Native
captures confirm the main label, separate titles and equal populated Community /
empty LAN frame bounds. LAN Start New Match opens the original lobby; Back returns
to the chooser. The isolated probe cleaned its processes and disposable disks.
Invite Only listing/admission remains unfinished. The later private-directory
restoration above removes the separate hosting-component dependency.

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

Fifth revised notes: production menu fixtures verify open/protected admission
requests retain their source page, selected public ID/secret semantics, duplicate
activation suppression and in-place cancellation. Existing discovery tests retain
index0/callback1CBD40. Native initialization1A0A60/1CB950 and advertisement
enumeration352980/352990 remain required; do not remove them to merge screens.

Community update verification must bypass the optional personal LAN route.
The operator guide sets Client.local_address=None before its pinned public
probe, requires an outside-network caller, and records old/deployed TCP/UDP
listeners around service installation. Generic30820/30821 installer defaults
must not replace the deployed community41820/41821 pair. Host-local success
alone is not Internet acceptance; actual private updater behavior remains
outside this checkout and needs operator-side confirmation.


## Automatic player-host UPnP (2026-11-12)

`online_upnp.py` is a shared standard-library IPv4 IGD adapter, bundled and hashed
with both host companions. LocalDirectory starts HTTPS and a SharedRelay UDP
listener before mapping TCP30820 and UDP30821. Both handles close with the
directory, independently of lobby lifetime. Legacy direct Session.create retains
its separate UDP mapping until Session.close; admission failure rolls it back.
A maintenance-worker startup failure cleans up its new mapping before fallback.
Joining and relay hosting do not map ports. Directory/lobby lifetimes stay
independent. PRIVATE_READY reports mapping availability without blocking hosting
when UPnP is unavailable. No guest IPC or game-menu layout is added.

Discovery binds the default RFC1918 interface, uses bounded SSDP, inspects up to4
LAN device descriptions and supports WANIP/WANPPP IGD1/2. XML size/entity limits,
no proxies/redirects, and same-responding-private-IP description/control URLs keep
discovery local. SOAP queries precede AddPortMapping; a matching existing forward
is borrowed unchanged, a conflicting one is never overwritten. Each new mapping
has a unique description, one-hour lease and periodic recheck/renewal; missing
entries are recreated while that gateway remains reachable. Error725 permits
permanent-only routers; normal exit still checks ownership before deletion.
Deletion/renewal never touches another handle's/manual mapping. Failed or killed
processes rely on finite expiry; permanent-only routers may need manual cleanup.
UPnP does not establish outside-network reachability or bypass CGNAT/double NAT.

Tests in test_online_upnp.py simulate discovery/SOAP, leases, errors, restarts,
conflicts and lifecycle/rollback, with real isolated TLS/UDP for host/guest flows.
All pytest tests and isolated Windows/Mac game runners force TSFP_UPNP=0 so tests
never configure real routers. Router-unit tests explicitly inject their fake IGD.
CI registers the new tests; package/identity tests cover the helper. Native Mac /
Linux physical-device and actual supported-router/WAN validation remain separate.
New private directories advertise `private_relay: true` with their existing
private v2 capability. Controller.private_relay selects Session.create(relay)
for both Arcade and Story; private_direct remains the legacy v2 API selector,
not a statement that UDP is direct. Older directories still select direct UDP.
The community v3 Arcade / v2 Story paths are unchanged.

SharedRelay uses the existing TSFR HMAC envelope from online_transport, with
independent random per-member relay keys supplied only over admitted TLS
create/join responses. Lists never expose keys. Registration binds an authenticated
endpoint, rejects replays and permits authenticated NAT rebinding. Only registered
host/guest pairs in the same live room can forward; guests cannot bypass the
host. Keys are removed on leave/expiry; closed rooms cannot forward. Registration
expires after5s; admitted traffic has a bounded per-member packet budget.
The relay forwards opaque existing wire payloads and re-signs the outer envelope
for its recipient. It does not add migration to private v2 rooms.

Private relay addresses use the accepted TLS socket's local address for LAN /
loopback connections and the directory owner's public address for WAN callers,
never the individual lobby host's address. A busy UDP listener rolls back HTTPS
startup. Ephemeral test-directory TCP ports also select ephemeral UDP ports.
Tests cover mixed modes, Invite Only, multiple hosts, real three-peer gameplay
packet delivery, owner-only mappings, room isolation, replay/spoof rejection,
NAT rebinding, expiry, rate limits and listener rollback/cleanup.
The existing community service is unchanged; relayed player lobbies need no
inbound client mapping. Physical router/WAN and Mac/Linux acceptance are pending.


### Router SOAP interoperability (2026-11-12)

Live Windows CR1000A discovery succeeded but GetSpecificPortMappingEntry returned
402 for self-closing NewRemoteHost. The same query with expanded empty tags
returned714 (no mapping); encodingStyle/XML declaration alone did not help.
Gateway.call now serializes all empty arguments with explicit start/end tags.
Do not treat402 as missing: that would bypass conflict/ownership checks.

This router also rejects DeletePortMapping with empty NewRemoteHost (402), yet
accepts a literal wildcard `*`. PortMapping.close retries only that error and
only after querying again to confirm the unique description/destination still
belong to this handle. Permission/other errors and replaced/manual entries never
trigger the fallback. Regression cases model all three actions and replacement
between deletion attempts. Corrected helper physically created, queried and
removed both hosting ports on this Windows router; WAN gameplay and native Mac /
Linux behavior remain unverified. Mobile-hotspot inbound reachability is a
separate upstream NAT capability, not established by home-router mapping success.


### Directory lifetime ends at Multiplayer (2026-11-12)

Returning to MP_NETWORK (the Multiplayer Online/Private Server/LAN selector)
now calls mp_close_directory via mp_show; the stock mp_root_build entry does the
same. pc_online_disconnect signals companion shutdown through each existing host
adapter, reaching Controller.close and LocalDirectory.close for HTTPS/relay and
owned UPnP cleanup. Native pending/autojoin/host/sequence and cached server state
are cleared so later hosting cannot reuse an obsolete invitation.

Back from Host Server to Private Server, or lobby options to the browser, does
not close the directory. Lobby leave/Return to Lobby retain their existing
semantics. The fov_settings production fixture exercises both retained navigation
and private/browser Back teardown. Existing controller/private/UPnP tests verify
independent lobby/server lifetime and owner-only TCP/UDP cleanup. No new adapter,
IPC action or menu layout is required. Cross-network Windows-host/Mac-hotspot
joining was reported working by the user before this lifetime change; native
Mac/Linux acceptance of the new return boundary remains unverified.


### Host startup notice and profile lobby default (2026-11-12)

After syncing main c1e67e42 through merge a9340161, Controller.PRIVATE_HOST
reports the exact unavailable TCP directory / UDP relay ports in its existing
PRIVATE_READY message. A missing UPnP handle is not proof that manual forwarding
or WAN access fails: the short message asks the player to check Internet forwarding and keeps LAN
hosting available (under128 characters for native-font readability). Listener /
certificate/startup failures still use ERROR. Native completion now copies host
PRIVATE_READY messages to the existing native status bar instead of discarding
them. Success reports active mappings without claiming verified WAN reachability.
The existing IPC structure/version, hosting flow and server lifecycle are intact.

Opening MP_CREATE refreshes the automatic name from the local P1 profile using
original356870(0) -> local index ->69720(index). Verified original69720 returns
profile metadata+4 (or body+4 when no persistent metadata); names are bounded to
31 characters. Existing profiles/readers are not modified. Guest pointer bounds
and NUL termination are checked, unsupported/control bytes become question marks
to satisfy the existing printable-ASCII lobby contract, trailing spaces are
trimmed, and a missing/invalid name falls back to My Lobby. Normal names produce
Dudeman's Lobby. Saved guest registers survive those accessor calls.

Accepted custom lobby names set name_custom and survive later page visits;
clearing the field restores the profile default. The default changes when the
profile changes while no custom name is set. Tests cover profile/peer selection,
renaming, custom retention, invalid index/unterminated fallback; production native
tick delivery to the status bar; both/single/no missing mappings and the serialized
controller message. Platform behavior is shared; no new native adapters or
community-service changes are required.


Lobby-name field text uses70% native scale after painting its label/background.
Only MP_CREATE input/placeholder is reduced; label, field geometry, other code
fields and stored name stay intact. Measurement/ellipsis and vertical centering
use the reduced scale; original scales are restored at the end of mp_code_draw.
This is a shared cosmetic change; existing native painter fixtures cover its
ordinary rendering/transition routes. Native Mac/Linux visual review is pending.


Beta package acceptance imports LocalDirectory from online_private, preserving
online_host's player-only boundary. The actual bundled-runtime probe creates a
private shared-relay room and delivers guest-to-guest packets through it, without
an artificial host address rewrite. Its child explicitly disables physical UPnP
after stripping inherited TSFP variables, and has a120s subprocess bound.

Windows/Mac bundle TLS probes import certificate from online_private, matching
the private-directory split. The standalone companion check generates a temporary
certificate and loads it into SSLContext to cover the exported package contract.
