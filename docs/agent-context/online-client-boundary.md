# Online client/server boundary

The community mini's directory, UDP relay, Arcade migration coordinator,
dashboard and service installer remain private, outside this client repository.
Do not copy that deployed service into client packages or CI. The service is
independent of the game checkout.

The user clarified on2026-11-11 that Host a Server must start the former local
player directory directly, through in-game menus. `online_private.py` restores
that bounded v2 HTTPS/direct-UDP player-hosting capability from the former
LocalDirectory path. It ships with the companion and needs no separate hosting
component. It does not implement the community relay/coordinator/dashboard.
The default native private launcher imports it only on an explicit host action;
old saved automatic_host values still cannot silently start a directory.

Keep `online-service.json`: its public service URL and TLS certificate fingerprint
are connection information, not server administration credentials. Keep the
generic `view_online_dashboard.py` SSH viewer; keys and SSH host configuration
stay outside Git. To connect, run `python tools/network/view_online_dashboard.py
--ssh YOUR_SSH_ALIAS` using the locally configured dashboard access account.

`online_host.py` now only handles player invitation/connection helpers. The
browser always creates rooms through its configured service; the former local
directory checkbox and automatic-directory launch path are removed. Saved
`automatic_host` values cannot start a directory. Match simulation, authenticated
UDP transport and native checkpoint capture/restore remain player-side. The
checkpoint schema string is defined in `online_arcade_client.py` without importing
the private coordinator. Windows/Mac companion lists and network identities cover
client files only. Existing binaries need rebuilding to match this source change.

Client tests cover invitations, certificate validation, UDP authentication,
native lifecycle and native Arcade checkpoint/adapter contracts without a local
server. Server-backed acceptance drivers and tests are preserved with private
source. Legacy manual integration tools require that external fixture explicitly;
normal game play does not. Historical gameplay acceptance results remain evidence
for their recorded revisions; this separation is not a new gameplay test.

The [native multiplayer flow](native-multiplayer-menu.md) uses a headless client.
Explicit private hosting starts a supervised local directory in the hidden
companion. Lobby departure preserves it; owner/companion shutdown closes it.

The private copy retains the previously deployed source and test versions.
Earlier Git revisions already contained server code; removing it from the current
tree does not rewrite published history. Future changes must respect this split.

Validation (2026-11-10): Windows network, Story launcher and release-tool checks
passed 266 tests with 9 skips; 13 focused client tests passed again after import
cleanup. On the physical Mac the same selection passed 261 with 13 skips and one
missing-xdist failure; installing the test dependency and rerunning that check
passed. The isolated companion imports without private server modules. Private
archive hashes and all 11 installed service files matched before/after source
checkout synchronization, and the live loopback dashboard remained healthy.
No new native build or gameplay run was performed for this repository split.
