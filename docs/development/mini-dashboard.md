# Mac mini server dashboard

The dashboard shows the lobby/relay service in a browser. It refreshes each second:

- Lobbies, connected player IDs, current host and migration phase.
- Engine progress age, relay registration and the latest backup's age and ACKs.
- Relay traffic, rejected packets, HTTP errors, TLS failures and timeouts.
- Service uptime, CPU use, peak process memory and HTTPS worker use.

The first player still runs the match. The mini observes/relays it; the dashboard
does not run game simulation or change a lobby. Legacy sessions show unavailable
native telemetry explicitly. Events are observed state changes, not a complete
packet or gameplay audit; changes shorter than the sampling interval can be missed.
History is bounded and resets with the service. A reachable dashboard does not
prove that public internet game connections work.

## Open the dashboard

On the mini, visit `http://127.0.0.1:41822` after enabling the dashboard.

From the Windows source checkout, using the approved dashboard-only SSH connection:

```powershell
.venv/Scripts/python.exe tools/network/view_online_dashboard.py --ssh timesplitters-dashboard
```

The command opens the browser and keeps an SSH tunnel running. Leave its terminal
open while viewing; Ctrl+C closes that tunnel. Use `--local-port 41823` if41822 is
already occupied, or simply revisit the existing dashboard. On macOS/Linux use
the host's Python path instead of `.venv/Scripts/python.exe`.

The equivalent manual command is:

```sh
ssh -N -o ExitOnForwardFailure=yes -L 127.0.0.1:41822:127.0.0.1:41822 timesplitters-dashboard
```

Then open `http://127.0.0.1:41822` on that computer. Replace the SSH alias with the
mini's configured SSH name/address when using another setup. The dashboard's
access panel can generate a command for a different SSH name.

The current mini's build key is marked `restrict` in `~/.ssh/authorized_keys`,
which denies TCP forwarding. It remains unchanged. With the user's approval, a
separate `timesplitters-dashboard` alias/key now permits access only to the private
dashboard port. Shell commands, game-port forwarding and other remote listening
ports were tested and rejected. The alias retains strict host-key checking using
the mini's existing pinned host-key file. Private key material stays on the PC,
outside the repository; only its public key was installed on the mini.
An SSH “administratively prohibited” message means forwarding permission was
denied, rather than a dashboard outage. Other computers need their own approved
SSH access; the dashboard key is not part of the game distribution.

## Enable it on the service

The dashboard is an optional separate loopback listener. For this deployment,
retain the existing public/LAN addresses and certificate and enable it with:

```sh
.venv/bin/python tools/network/install_online_service.py \
  --address PUBLIC_IPV4 --lan-address MINI_LAN_IPV4 \
  --directory-port 41820 --relay-port 41821 --dashboard-port 41822 --install
```

Back up the existing server/configuration before an installed-service upgrade.
The installer reuses its existing certificate. An enabled dashboard port is
preserved if omitted during a later upgrade. The launch agent starts the service
and dashboard at login; it requires the mini to remain awake.

Do not forward the dashboard port on the router. It only listens on127.0.0.1;
remote access uses SSH. The public HTTPS game listener does not expose dashboard
routes. Snapshots omit authentication tokens, game/relay keys, raw checkpoints,
private keys and player IP addresses. Browser pages reject cross-site requests
and framing and have no write controls.

The same observer can be enabled on a standalone service with
`lobby_server.py --dashboard-port 41822` alongside its existing flags. No game
assets or native game rebuild are needed to install this service-only view;
the existing v1/v2/v3 game protocols and client compatibility matching are retained.
