# PC multiplayer bring-up

The source build replaces Xbox Live with **Online** above System Link. Every PC
runs its own game, using competitive multiplayer with a player-hosted directory
and UDP bridge. No Steam, paid service, account, or centrally operated server is
required. Campaign co-op is a separate future effort.

Two local processes have used Online, an HTTPS directory, and separate virtual
network addresses to join a match and enter gameplay. Real routers, different
internet connections, and full-match stability remain unverified. This feature
is experimental and is not in the existing downloadable portable release.

## Set up Online

Build the same source and game content on all PCs with `.\tsfp.ps1 Build`, then
launch with `.\tsfp.ps1 Run` or `Play.cmd`.

Choose **Online / System Link > Online > Start New Match**. Automatic hosting
is enabled by default. It detects your public IPv4, starts an HTTPS directory in
the background, creates/reuses a certificate under `private/online-directory`,
and fills in the local directory address and fingerprint. No PowerShell window
or certificate copying is needed. Copy the invitation from the hosting window
and send it to friends, then return to the game and select **Start New Match**
to choose match settings.

The host still forwards **TCP 30820** and **UDP 30821** to their PC and permits
both through Windows Firewall. If a different game UDP port is selected, forward
that number instead. This does not configure the router or firewall automatically.

Public IPv4 detection uses the free, IPv4-only [ipify HTTPS endpoint](https://www.ipify.org/)
with a five-second timeout. No lobby details, credentials or game data are sent.
The address is refreshed each time automatic hosting starts; **Detect** can also
refresh it explicitly. If lookup fails or a VPN gives the wrong hosting address,
disable automatic IP detection and enter your public IPv4 manually. The directory
and bridge otherwise have no dependency on ipify. Detection does not solve
carrier-grade NAT or port forwarding.

Friends paste the full invitation into **Invitation / lobby code** and click
**Join by code**. It fills in the directory address and fingerprint and joins
the compatible lobby. Invitations contain connection details, not owner or peer
credentials; share them with the intended players. A bare code still works when
the directory address/fingerprint are already configured. The usual compatible
public browser is available to everyone using that directory.

The automatic directory stops when its host chooses System Link, opens Online
again, or exits the game.
If you want several hosts to share a directory, keep a standalone directory
running instead and disable **Start directory automatically on this PC** in the
host's Online window. Automatic hosting reports an occupied directory port;
it never stops or replaces another service.

### Optional standalone directory

One player starts a persistent directory separately. Replace the example with their public IPv4:

```powershell
.\Start-OnlineDirectory.ps1 -PublicAddress '203.0.113.42'
```

The script creates a reusable self-signed TLS certificate under ignored
`private/online-directory`, prints its SHA-256 fingerprint, and runs until stopped.
It uses OpenSSL included with Git for Windows and the workspace's Python
environment. Keep this window open. It makes no certificate store changes.

- Forward **TCP 30820** on the directory player's router to that PC. Friends use
  `https://<directory-public-IP>:30820` and the printed certificate fingerprint.
- On the directory PC use `https://127.0.0.1:30820` and that same fingerprint.
  `-PublicAddress` makes its own game-host registrations advertise the reachable
  public address instead of loopback.
- The game host also forwards **UDP 30821** to their PC and allows it through
  Windows Firewall. The browser can select a different port; forward the same
  number externally and internally. Directory and game host can be the same PC.
  A different game host forwards only its selected UDP port.
- Guests need no forwarding. A host behind carrier-grade NAT needs a reachable
  player to host instead. IPv6-only hosting is not implemented.

Choose **Online / System Link > Online** in the game and enter the directory
address and certificate fingerprint. These non-secret preferences are saved in
ignored `config/pc-online.json`.

To host, enter a name and UDP port, select **List publicly** or leave it private,
and click **Start New Match**, with automatic directory hosting disabled.
Share the displayed join code for a private lobby.
Return to the game and select **Start New Match**.

Friends click **Refresh**, select a compatible lobby and **Join selected**, or
enter a private code and click **Join by code**. The bridge requires an
authenticated reply from the host before opening the game's match list. Select
the host's match there and Ready; the host selects Go. Browser counts represent
connected PCs; the game's own lobby shows and limits actual players.

The browser/bridge companion has no console and exits when the game closes,
including after a crash. Choosing Online again closes the previous connection.
Choosing System Link disconnects it and uses LAN transport. Cancel returns to the
game without connecting. Keep the directory running during online sessions;
registrations expire after 90 seconds without a heartbeat. No database or copies
of game assets are created. Hosts sharing a router need separate UDP forwards;
devices on that LAN can use System Link instead of relying on router hairpin NAT.

## Try System Link on a LAN

Build the same source and game content on both PCs, then run in PowerShell:

```powershell
.\tsfp.ps1 Build
.\tsfp.ps1 Run -SystemLink
```

Select **Online / System Link > System Link**. One player selects **Start New
Match**; the other selects that match, then Ready. The host starts the match.
The native transport uses UDP 1000 (gameplay), 3000 (session information), and
9999 (discovery). Allow the game through Windows Firewall on the private LAN.

If LAN broadcasts do not reach the other PC, explicitly name its LAN IPv4:

```powershell
.\tsfp.ps1 Run -SystemLink -Peers '192.168.1.25'
```

`-Peers` duplicates discovery broadcasts to the supplied addresses. It does not
solve NAT, translate addresses embedded in Xbox session data, or make internet
play ready. Selecting System Link enables native LAN transport.
There is no Xbox console interoperability or Xbox Live emulation.

## Compatibility

Build generates `build/windows/generated/network-identity.json` and a compiled
SHA-256 identity from the protocol, native source, runtime source, lifted game
code, patches, and game data. Paths are relative and source line endings are
normalized. Online companion source is included too. User settings, saves,
diagnostic metadata and external texture
packs are excluded. Other source changes are conservatively incompatible.

Every discovery and gameplay datagram carries the compiled identity. Traffic
with a different identity, an unknown header, or no header is discarded before
the game receives it. This covers connections that bypass the browser.
The launcher also verifies the executable against a successful-build stamp,
hashes game content before a System Link launch, and refuses content changed
since the build; rebuild after editing game data. A failed build cannot authorize
an old executable against a new manifest. Launching the
EXE directly bypasses that launcher check. Online also checks game content and
compares the running executable's compiled identity with the manifest on opening.

The directory only returns public lobbies matching the requested identity.
Looking up an incompatible lobby by its private code returns HTTP 409. This is
compatibility enforcement for cooperating builds, not authentication, encryption,
or protection against a deliberately modified client.

## Online transport

Online presents `10.254.0.0/24` addresses to the title, preserving addresses inside
Xbox session data across different networks. Game sockets talk to their own
bridge on loopback using a random local token. Physical socket ports are
ephemeral, allowing two instances on the same PC without port conflicts.

Guests open an outbound UDP path to the host. The host delivers packets addressed
to itself and relays guest-to-guest traffic. This is host-routed multiplayer; it
needs no third-party relay or direct guest-to-guest hole punching. Relayed traffic
consumes the host's bandwidth.

Directory v2 provides filtered listing, private/public registration, code joining,
rosters, peer leases and removal. Credentials travel over HTTPS. Each guest has
a separate key shared only with the host. UDP uses HMAC-SHA256 authentication,
room/build identities and sequence numbers to reject replayed, wrongly signed,
wrong-room and wrong-build packets. Game payloads are authenticated but not
encrypted. The host remains trusted; this is not an anti-cheat system.

The service retains v1 for the earlier foundation; Online uses v2. Requests,
concurrent connections, room counts, packet sizes and receive work are bounded.
Do not put it behind a proxy that hides caller IPs: remote game-host addresses
come from the TCP connection. HTTPS uses normal certificate verification unless
an exact fingerprint is supplied; it is checked before credentials are sent.
The client accepts HTTP only on localhost for development.

Remaining validation includes real internet routing, different NAT behavior,
disconnect/rejoin in a full match and sustained play across modes. Packaging
Python/Tk and the companion for portable releases is a separate release step.

## Validation and storage

Launch both local test instances from the repository folder:

```powershell
.\Test-SystemLink.ps1
```

The script verifies the existing build, launches a host and client with separate
test saves and display settings, and waits until both games close before recycling
their temporary virtual disks automatically. Leave its PowerShell window open
until cleanup finishes. Test profiles are retained for the next run; logs are
overwritten in each role's `interactive.out` and `interactive.err`. The launcher
restores the calling shell's environment and prevents overlapping test sessions.
It uses the fixed test controls, so personal bindings and settings are preserved.
In both windows choose **Online / System Link > System Link**. Host a new match
in the first window, join and Ready in the second, then select Go on the host.

Native tests cover UDP round trips, default-protocol sockets used by discovery,
XNADDR/key mapping, source ports, invalid guest pointers, and incompatible/raw
packet rejection. Python tests cover deterministic identities, changed game and
runtime data, directory filtering, private codes, owner authorization, full
lobbies, heartbeat compatibility, and expiry.

Online native tests also check bridge registration, tokens and virtual source
routing. Python tests use real sockets for the directory, handshake, three-PC
relay, broadcast routing, credential rejection and version filtering. The local
HTTPS game run exercised the Online-menu-to-gameplay path and certificate
pinning; a wrong certificate fingerprint was rejected.

`TSFP_ONLINE_TEST_COMMAND` enables automated host/browse/code actions only from a
file inside `private/network-validation`. `TSFP_ONLINE_SETTINGS` redirects UI
preferences for isolated tests. Neither is needed for normal play.

Manual two-process validation uses `private/network-validation/host` and `client`
with separate saves and loopback addresses 127.0.0.2 and 127.0.0.3. It reuses the
same game data and build. `TSFP_PC_INPUT_STATE` is an opt-in test input file with
`<virtual-key-number> <0-or-1>`, updated atomically; it permits background input
without changing desktop focus. Never point either test process at personal saves.

After both processes stop, run `tools/cleanup-network-tests.ps1`. It recycles only
the six known disposable `Partition*.img` files per test save directory, rejects
reparse points and out-of-root paths, and preserves test profiles and evidence.
