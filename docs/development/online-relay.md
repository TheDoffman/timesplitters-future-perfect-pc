# Shared Online lobby and relay

For the current native-menu protocol changes, see
[community directory update](online-community-update.md). Server source and the
installer below now live in the private service checkout, outside this client
repository; these older commands are deployment context, not a client updater.

Deployment on 2026-11-10: the Mac mini service uses HTTPS TCP **41820** and UDP
**41821**. `online-service.json` contains its public endpoint and certificate pin.
The router's UPnP service accepted permanent mappings for these two free ports to
the Mac; the original 30820/30821 rules were preserved. A public-address pinned
HTTPS query and bidirectional authenticated UDP probe passed from the home LAN.
This proves that route including router hairpinning, not an outside-network game
match. After router restart/IP changes verify the mappings, reserve the Mac LAN
address, and rerun installation/update public defaults as needed. UPnP rules may
need recreation after router reboot. The commands below use the generic default
ports; pass `--directory-port 41820 --relay-port 41821` for this deployment.

The Mac mini runs the HTTPS lobby directory and a UDP packet relay. The first
player still hosts the game's simulation, bots, rules and match start. All players,
including that host, open outbound UDP connections to the relay. Player routers
need no forwarding. This first implementation always relays; direct NAT traversal
and host migration are separate future work. A host leaving ends the session.

## Mac mini setup

Use Python 3.12 with SSL. The server uses the standard library; first-run certificate
creation uses `cryptography` when available or the system OpenSSL. No game assets,
game executable, Tk runtime or account database are needed on the server.

From the source checkout on the Mac, substitute the actual reachable addresses:

```sh
python3.12 tools/network/install_online_service.py \
  --address PUBLIC_IPV4 --lan-address MAC_LAN_IPV4 --install
```

For a LAN-only test use the Mac's LAN IPv4 as `--address` and omit `--lan-address`.
Without `--install`, this prepares the configuration without starting launchd.
The default installation is `~/Library/Application Support/TimeSplitters Online`.
It contains a five-file server, persistent certificate/key, public client settings,
source hashes, logs and `service.plist`. Existing certificates survive reinstallation.
The private key is mode 0600 on Mac; never distribute it. An unrelated nonempty
destination is refused. `--root`, `--directory-port` and `--relay-port` override
the installation location and listening ports.

The launch agent is `~/Library/LaunchAgents/com.timesplitters.online.plist`.
It starts at user login and restarts after a crash. It requires the Mac user to
remain logged in and the Mac to stay awake. The installer does not change power,
router or firewall settings, and does not replace a service from a different root.

Configure the **server network once**: reserve the Mac's LAN address and forward
TCP 30820 and UDP 30821 to it, allowing the Python service in the macOS firewall.
Both outside ports must match their inside ports. A server behind carrier-grade
NAT needs a public relay endpoint/VPS instead. The optional LAN override lets
local players reach the same service without relying on router hairpin support.
It applies to clients whose actual connection address is private; don't put a
reverse proxy in front without revisiting that trust boundary.

Check service state and stop/start it with:

```sh
launchctl print gui/$(id -u)/com.timesplitters.online
launchctl bootout gui/$(id -u) ~/Library/LaunchAgents/com.timesplitters.online.plist
launchctl bootstrap gui/$(id -u) ~/Library/LaunchAgents/com.timesplitters.online.plist
```

For foreground operation or bandwidth configuration, run the server directly:

```sh
python3.12 tools/network/lobby_server.py --bind 0.0.0.0 --port 30820 \
  --cert /path/to/certificate.pem --key /path/to/private-key.pem \
  --relay-address PUBLIC_IPV4 --relay-port 30821 --relay-lan-address MAC_LAN_IPV4
```

`--relay-bytes-per-second` defaults to 2 MiB/s per credential and
`--relay-total-bytes-per-second` to 16 MiB/s total, including envelope overhead.
These are traffic ceilings, not measured server capacity. Tune them to the actual
uplink. Tokens allow a one-second burst. Invalid packets receive no reply.

## Client and release setup

Copy the **public** `client-service.json` values into
`tools/network/online-service.json` before building matching Windows/Mac/Linux game
packages. The address and certificate fingerprint then ship with the companion;
new users see the common lobby list automatically. The file participates in build
compatibility hashing. Do not ship a LAN address as the internet default.

For source testing without editing defaults, use `TSFP_ONLINE_DIRECTORY` and
`TSFP_ONLINE_FINGERPRINT`. Saved directory choices take precedence; isolated tests
must use `TSFP_ONLINE_SETTINGS`. Users can also enter the server and fingerprint
in Directory settings, or paste a full invitation. Keep “Host directory on this
PC” off for shared relaying. That advanced option retains the old direct-host
workflow and its router requirements.

Shared hosting explicitly requires relay support. It refuses old directories
instead of silently falling back to a host that requires forwarding. A relayed
host uses an ephemeral local UDP port; the legacy UDP port field is ignored.
Existing direct sessions and System Link are retained. The companion's native
loopback handshake, guest virtual addresses and game protocol stay unchanged.
Changed companion sources require a normal rebuilt/sealed game identity; copying
Python scripts into an old release does not make a compatible game package.

## Protocol and validation

The HTTPS create/join replies give each member a separate relay credential.
Public listings and owner rosters never disclose those relay credentials. A
TSFR v1 HMAC envelope binds a room, sender, destination and monotonic sequence.
Only authenticated registration can learn/change a member's endpoint. One-second
registrations keep outbound NAT mappings alive. The relay forwards only host to
guest or guest to host; guest-to-guest game traffic still passes through the
player host. Directory membership, expiry, endpoint freshness and bandwidth
limits are checked before forwarding. Existing TSFO host/guest HMAC verification
remains inside the envelope. This authenticates packets; it does not encrypt
gameplay. HTTPS protects directory credentials in transit.

Disconnect/revocation stops forwarding immediately; abandoned directory entries
expire after 90 seconds. Missing relay replies disconnect the companion after
roughly 10–12 seconds. Missing initial relay/host handshakes time out after 12
seconds and remove the failed registration.

`tests/network/test_online_relay.py` runs real pinned HTTPS plus three real UDP
bridges with an outbound-only socket guard, occupied legacy host port, discovery,
host and guest traffic, RTT, forgery/replay/isolation/revocation/expiry/rate-limit
checks, old-directory rejection, invitations and the actual isolated installed
server command. `test_online_companion.py` additionally covers relayed browser
host/join/cancel and supervisor cleanup. Both suites are in runtime CI.
Socket tests establish transport behavior; full real-game WAN matches, loss/load
testing and a published compatible client release remain separate acceptance.
