# Multiplayer companion presentation

Server source stays private. See the [client/server boundary](online-client-boundary.md).

Arcade game-frame metadata continues while native menus are inactive; see
[identity capture and checks](online-player-identities.md).

## Arcade host migration

Shared Arcade uses authenticated v3 rooms through `ArcadeSession`; Story and
legacy direct tests retain v2. Native control uses a separate authenticated
loopback socket, pointer-free checkpoints, engine progress/restore ACKs, dynamic
host routing and per-epoch key rotation. The native game-thread adapter preserves
scores, game tick and bot numeric state before committing mapped native peer IDs.
`3000-arcade-migration-build.patch` enables the shared game adapter. Production
native Windows Deathmatch tests retain scores/countdown and six fighting,
respawning bots after two/three-player normal exit/crash and repeated handoff.
Authenticated native-ID routes let every survivor follow the new host. Pause
caption/confirmation describe leaving and transferring the match. Physical Mac
Vulkan crossplay now passes normal/crash/repeated handoffs, results and rematch;
other modes require their own acceptance. The Mac relay
service is deployed; client executables require matching rebuilds.
See [implementation and validation](../development/arcade-host-migration.md).

`pc_arcade_migration_guest.inc` publishes active preparation/graceful-departure
status through `pc_match_overlay_migration`. The shared final-frame canvas paints
a centred Migrating host card, independent of HUD/Tools preferences, and clears
it on commit, closure or leaving gameplay. It does not retain guest pointers,
capture input, or add a network message. Windows and native GL use the same layout.
Its visual theme follows the native pause confirmations: black panel, pale
rounded frame, blue light bars and bold uppercase lettering. Ten draw slots
include the border, interior, separator, bars and three labels.
For native common-renderer visual tests, `3010-present-frame-capture.patch`
captures the swap-chain image immediately before presentation, including late
host overlays. Raw HLE shadow dumps precede that composition and cannot verify
the migration card. `pc_present_capture.h` is opt-in, render-thread-only and
writes two rotating BMPs; the crossplay observer selects it for Vulkan builds.

## Shared relay

`lobby_server.RelayServer` authenticates TSFR envelopes against the live directory
roster, learns endpoints only on registration and forwards host/guest pairs with
replay, expiry and token-bucket limits. Separate relay keys never enter public
listings or owner rosters. `online_transport.Bridge` encapsulates the unchanged
TSFO game transport and retains player1 routing/authority. Both roles use outbound
ephemeral sockets; the native loopback handshake is unchanged. `Session.create`
accepts explicit `transport='relay'`, refuses unsupported directories and waits
for relay registration before publishing a game handshake. Guests still require
the actual player's authenticated ACK. Lost relay replies stop the session.

The browser defaults new installations to a shared service, refreshes listings
on opening and retains saved legacy automatic hosting choices. Public defaults
come from bundled/identity-hashed `online-service.json` (deployed Mac mini) or env;
saved user choices win. `install_online_service.py` prepares the five-file
standalone service and a per-user Mac launch agent without game assets. Optional
LAN advertisement avoids hairpin routing. See [setup](../development/online-relay.md)
and `test_online_relay.py` for actual HTTPS/UDP, authorization, isolated installer
and outbound-only validation. Real-game WAN/release acceptance is separate.

`tools/network/online_theme.py` owns the launcher-colored Tk browser and invitation
view (`build_browser`, `build_invitation`). `online.py` supplies its existing
variables and callbacks; identity verification, session/bridge lifetime, port
validation, invitation parsing and persistence remain in that controller. The
Play online tab separates joining/browsing and hosting; Directory settings holds
the shared directory URL and fingerprint. The navy/blue interior is independent
of Windows app light/dark preference, with native colors in high contrast.
Small viewports scroll and focus traversal reveals offscreen controls. The shared protocol is unchanged; Windows and Mac use the same controller.

## Validation

`tests/network/test_online_theme.py` now exercises the real browser/invitation
widgets at 100/150/200% Tk scaling, native accessibility theme fallback, action
callbacks, tab switching, minimum bounds and keyboard focus visibility in a
short scrolled viewport. These checks run in both desktop runtime CI suites.
Use `python tools/network/online_theme.py` (optionally `--invitation`) for an
offline preview without game data, saved settings or network operations.
The companion is already included in bundling and compatibility hashes; source
changes need a normal matching game build/package before multiplayer use.


## Project icon

`OnlineTheme.track` loads `multiplayer.ico` beside the packaged companion or from
`build/online-icon` in a checkout. It assigns the icon to both windows and as the
root's default for later popups. `build-online-icon.ps1` renders the vector source
in `IconBuilder.cs` at 16/24/32/48/64/128/256 pixels. Both the normal game resource
build and `bundle_online.bundle_icon` generate it; packages copy it beside the
companion. It requires no private artwork or additional Python libraries. The
header now reads “Find your friends. Start a match.” without the extra tagline.

The native wrapper also receives explicit `WM_SETICON` handles: the shell gets
the full 256px artwork, while the caption gets the system small-icon size.
`iconbitmap` alone supplied 32px for both roles on Windows Tk 8.6. Handles are
cached, restored after wrapper recreation and released when the root closes.
The packaged-icon test checks actual HICON bitmap dimensions for both windows,
including withdraw/show; icon-file resolution alone is insufficient evidence.

Rounded control images use transparent corners instead of a navy matte. Card
entries/buttons use background styles matching their parent in all states;
page/popup controls retain the page background. Both scrollbar orientations
use centered, scale-sized image arrows and flat thumb/trough elements, retaining
ttk's scrolling bindings. Tests cover corner transparency, state backgrounds,
arrow hit targets and real unit-scroll callbacks at 100/150/200% Tk scaling.
The custom elements stay inside the branded theme; high contrast uses the
original native theme and its controls.


PR #399 CI exposed clipped directory fields at 150/200% scaling on a 1280x768
Windows runner. `scrollable_panel` now serves the Play tab, Directory tab and
invitation popup, including focus-driven scrolling. The real-widget checks cap
the reported screen size to that runner geometry even on large developer
monitors and verify that the certificate field and popup Return action can be
brought into view. The original assertions remain in place.

## Mac hosting and packaging (2026-10-30)

The same browser now runs on Mac. Windows caption/HICON/accessibility calls stay
behind the platform adapter; Mac uses native window framing and the shared navy
controls. Tk iconphoto receives the 512px multiplayer PNG generated from the same
IconBuilder artwork. `online_platform.py` owns process monitoring and native error
presentation. `online_paths.py` accepts the game's explicit handshake/manifest
paths, including builds whose runtime lives outside the source checkout.

Mac packages contain Support/Online and Support/Python; their shell entry execs
the relocated interpreter with local Python/Tcl/Tk/CA paths and no user site or
PYTHONPATH. `bundle_online_macos.py` checks native loading and real Tk/TLS before
returning. The shared widget tests run on both hosts; DWM and HICON tests remain
Windows-only. Native game host/join/cancel supervisor checks and a real two-device
transport round trip passed; both-direction LAN match/rematch evidence is in STATUS.

## Native multiplayer adapter (2026-10-30)

`src/network/pc_network_platform.h` owns Winsock/BSD sockets, monotonic time,
locks, entropy, local IPv4 selection and error mapping. `pc_network.c` retains
one packet/session/bot implementation. The Xbox 16-byte sockaddr has a 16-bit
family; Darwin's sin_len/family layout is converted explicitly at the boundary.
Guest error numbers and ioctl/setsockopt constants stay Xbox values. Guest memory
validation uses the shared Mach/VirtualQuery adapter, including high Mac addresses.

`src/native/online_macos.m` spawns the shared browser directly, monitors its
handshake, pumps Cocoa while waiting, restores SDL3 mouse state, and stops/reaps
the relay on disconnect/exit. `--online-self-test` exercises this actual adapter
using isolated `TSFP_ONLINE_TEST_COMMAND` host/join/cancel commands. Python's
`online_platform.GameProcess` detects POSIX reparenting or a Windows process
handle exit. This supersedes the Windows-only network fallback notes above for
Mac builds defining TSFP_NATIVE_NETWORK; measured bot gameplay scope is in STATUS.

## Physical Windows/Mac Online acceptance

`test-online-bot-motion.py --mac-host <SSH-alias> --mac-root <checkout>
--lan-address <Windows-IPv4>` uses the existing bounded menu/gameplay/results
scenario with a native guest on the physical Mac. `crossplay_peer.py` supervises
that isolated child over SSH; SDL file events supply its inputs. The separately
loaded `native_read_probe.c` exposes read-only guest observations through a private
Unix socket, never an external listener or game memory writes. It is a test
artifact, not linked or packaged into a release. Both peers must have matching
sealed identities. Observations remain asynchronous; captures and results are
recorded separately. Full-match evidence is recorded in STATUS after execution.

Compatibility hashing sorts final manifest entries by portable path strings.
Windows Path ordering folds case while POSIX preserves it; identical file hashes
previously produced different build IDs. The ordering regression executes both
path policies and checks an independently assembled canonical digest.

The physical-peer runner uses a temporary pinned TLS certificate for its LAN
directory and places both automation commands under network-validation. The
shared testing-profile disk recycler uses native Trash on Mac, retaining the
same resolved six-partition boundary as Windows. No personal save is recycled.

`--mac-role host` reverses the host/guest assignment without changing the shared
scenario. Native observations are batched locally into read-only pages to avoid
per-field SSH delays. Menu snapshots/captures distinguish input failures from
protocol failures, and match evidence is saved before disposable-disk cleanup.
Migration tests wait for a naturally scored checkpoint acknowledged by a survivor.
`--migration-results --short-match` checks matching final records, an elected-host
rematch with reset scores/clock and a new round identity, then matching results
again. Retired player rows may remain in the first result; live native IDs and
bot rows must be present and unique. `--points` selects a stock point limit;
team-mode runs confirm the native Select Team screen before sampling gameplay.
`--rpc-trace` adds a test-only `sitecustomize` exception observer to companion
Python threads (32 exceptions and512 active progress samples maximum).
It preserves native compatibility and logs RPC failure classes/status/messages.
The observer chains the environment's existing Python bootstrap, retaining Mac
Tk support; enabled/disabled subprocess fixtures check bootstrap preservation,
the record limit and exclusion of local credentials.
GUI Python writes to a dedicated test log; only native counter/epoch/phase fields
are sampled. An exception hook records fatal companion thread failures as well.
New-round native leases begin when the host starts that round; pre-load progress
timestamps cannot immediately retire a guest awaiting its first post-load RPC.
This grants one ordinary lease only: unchanged counters do not renew it, and
inactive guests remain ineligible to host. Service tests cover resumed/stalled peers.
Arcade heartbeat RPCs use one-second socket deadlines, so one failed TLS handshake
can retry inside the four-second engine lease. Recovery sends current native
progress while omitting stale checkpoint/ACK authority; repeated transport failure
closes after a bounded3.5-second failure window. Other directory calls retain5s.
Normal-play acceptance now requires both peers still playing at the window end.
Arcade heartbeats reuse a per-thread pinned HTTPS connection (HTTP/1.1 opt-in
keep-alive), avoiding a new handshake for every pulse. Ordinary calls still close.
Errors discard the connection; reconnects verify the pin before credentials.
Heartbeat exit closes its cached socket; server idle deadlines/32-worker cap stay.
Original bot-result/limit CPU fixtures enable TSFP_NATIVE_NETWORK, retaining the
real compiler OS for the appended divide-error helper rather than forcing Win32.

## Native multiplayer build and validation (2026-10-30)

The native builder passes its actual same-host runtime to `network-identity.py
--runtime` and seals the finished executable. This prevents `--runtime-root`
builds from omitting the runtime/generated source from compatibility identity.
`test_native_network.py` compiles the production transport on Windows and Mac;
its portable fixture checks real UDP, Xbox sockaddr/error translation, invalid
packets, high host-memory pointers and bridge registration. The extended Windows
fixture still checks latency, bot/session ownership and lifecycle.
`test_online_companion.py` opens real Tk windows with isolated content/settings,
hosts/joins/cancels, and verifies relay shutdown and lobby removal. Shared UI,
Online, identity and pinned certificate tests now run in both host native suites;
Windows DWM/HICON checks remain platform-specific.

`bundle_online_macos.py` packages the build host's CPython/Tk, pinned ARM wheels,
CA trust and native dependency closure, rewrites loader paths, includes notices,
and signs/checks each binary. It copies the actual framework interpreter rather
than its respawning launcher, preserving direct-parent supervision. A real Tk/TLS
probe must pass before packaging succeeds. The package minimum OS includes Python
and extension libraries. Source development needs Python 3.12 with tkinter and
`pip install -r requirements-release.txt`. Generate `build/online-icon/multiplayer.png`
with the shared Windows icon builder and transfer that generated asset for Mac
packaging, as with launcher artwork. No game-data copy is required.

Formula notices use flat `Licenses/Online-<formula>-<notice>.txt` paths. The
installer accepts only top-level `.txt` license files, so names such as LICENSE
and sbom.spdx.json gain a `.txt` suffix without changing their bytes. The
packager's `copy_notices` fixture checks multiple formulae and notice formats;
the shared launcher tests retain rejection of nested or extensionless notices.


Results Options uses a short Down tap before Rematch; holding it triggers menu
repeat and can select Leave Match. The runner captures the selected option before
confirmation. A Windows-only control reproduced that input failure independently
of Mac networking; it is not evidence of a failed network reset.

The paired runner waits for actual directory registration (bounded 45 seconds),
including cold-cache content verification. Remote cleanup requests explicit JSON
shutdown before draining SSH, rather than relying on Windows pipe EOF. Provenance
is saved before cleanup and marks cleanup completion separately.

Native captures freeze each candidate frame into memory before decoding. If the
newest rotating BMP is incomplete, use the previous complete frame or retry within
two seconds; never enable truncated-image acceptance. The regression exercises
a valid prior frame alongside a partially written new frame.

The native peer records cleanup ownership only after creating its run lock
exclusively. Failed preparation or lock acquisition leaves the existing owner's
lock/disks alone. Later startup failures still clean up the acquired run, and a
second start is rejected so it cannot replace that cleanup target.
The owned lock remains present while disks are recycled and is released only
after success; a recycling failure leaves the run blocked for inspection.

Source Online builds use `generated/online-python.h`, written by the native
builder with its actual runtime-root interpreter. The Cocoa adapter consumes
that path while portable packages retain their bundled interpreter. The source
checkout therefore needs no duplicate venv when `--runtime-root` is in use.

## Layout convergence

`scrollable_panel` remembers the applied content size, background and scrollbar
visibility. Repeated Configure notifications at that same state no longer
submit another canvas-window geometry request. Real resize, content-height and
theme changes still update the layout. The deterministic notification-queue
fixture fails against the old callback and settles after each real resize with
the guard. Existing real-widget layout/focus/action tests stay intact.
Child UI probes now emit delayed Python stack traces if they stall, retaining
the same 60-second timeout. This repair was prompted by repeated hosted Mac
layout timeouts during PR432; physical Mac Tk9.1 and Windows checks pass, while
the original hosted timeout must still be validated by CI.

The child-test wrapper now includes TimeoutExpired's captured stdout/stderr in
pytest failures (decoding captured bytes when necessary). This preserves the
20-second faulthandler stacks under CI's short traceback mode; previously only
the command and 60-second timeout appeared. A synthetic timeout regression
checks trace retention. The timeout, widget assertions and product layout are
unchanged; improved diagnostics alone do not establish a timeout root cause.

The browser layout probe now uses Tk mainloop with 25 ms geometry samples,
requiring three unchanged samples within three seconds before each existing
assertion. Hosted traces stalled in the first update(); a disposable Mac Tk8.5
probe sampled repeated native image redraws, without a Python Configure loop.
The event-loop probe avoids waiting for an empty native redraw queue. Geometry,
focus, button and timeout assertions remain enforced. Windows (12 tests) and
physical Mac Tk9.1 (11 plus one Windows-only skip) pass; hosted Tk acceptance
is recorded separately in STATUS. Product layout is unchanged.

## Online opening feedback

PR534's extra Python loading panel was removed: it delayed the Multiplayer
options and did not explain the game's intentional wait. Original strict
pre-browser compatibility verification and browser lifecycle are restored.
The shared native Online callback returns immediately with a pending flag.
`pc_online_notice.inc` draws a native-font opening/setup instruction; after
two draws the next menu input tick enters the original blocking companion call.
The presented notice remains on the game while setup owns input.
Completion or cancellation clears the pending state. Windows/Mac/Linux use the
same guest menu path with their existing process/window adapters.
`fov_settings.c` checks deferred launch, one-shot dispatch and cancellation;
existing companion tests retain actual host/join/cancel coverage.

Builds without a desktop networking adapter provide a no-op handoff tick; the
native syntax-only regression covers this disabled-feature path.
