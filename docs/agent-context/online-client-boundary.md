# Online client/server boundary

The user requires this game repository to contain only player-side online code.
The mini's directory, UDP relay, Arcade migration coordinator, dashboard, service
installer and their server integration tests are maintained privately outside
Git. Do not reintroduce them in packages, CI, build identities or source lists.
The deployed service is independent of the game source checkout.

Keep `online-service.json`: its public service URL and TLS certificate fingerprint
are connection information, not server administration credentials. Keep the
generic `view_online_dashboard.py` SSH viewer; keys and SSH host configuration
stay outside Git. To connect, run `python tools/network/view_online_dashboard.py
--ssh YOUR_SSH_ALIAS` using the locally configured dashboard access account.

On Windows, install the desktop connection shortcut with:

```powershell
powershell -NoProfile -File tools/network/install-dashboard-shortcut.ps1 -SshAlias YOUR_SSH_ALIAS
```

The installer uses the checkout's Python environment by default (`-Python` can
select another installed interpreter). It copies only `dashboard_shortcut.py`
and the generic SSH viewer into LocalAppData and creates **Mac Mini Dashboard**
on the current Windows desktop. The shortcut reuses a healthy loopback dashboard
or starts the viewer without a console, then opens the default browser. Connection
failures show a dialog pointing to the local log. SSH aliases and keys must already
exist locally; the installer does not configure SSH, the mini or the router.
The shortcut works independently of this checkout after installation, provided
the selected Python installation remains available. This is explicitly a Windows
desktop convenience; Mac users retain the existing cross-platform SSH viewer.

`online_host.py` now only handles player invitation/connection helpers. The
browser always creates rooms through its configured service; the former local
directory checkbox and automatic-directory launch path are removed. Saved
`automatic_host` values cannot start a directory. Match simulation, authenticated
UDP transport and native checkpoint capture/restore remain player-side. The
checkpoint schema string is defined in `online_arcade_client.py` without importing
the private coordinator. Windows/Mac companion lists and network identities cover
client files only. Existing binaries need rebuilding to match this source change.

`Bridge.quality_snapshot()` reports authenticated host/guest round-trip times and
completed latency-probe outcomes over 60 seconds. RTT expires after five seconds;
the missed-probe fraction is not gameplay packet loss. `NativeArcade.body()` adds
this optional display-only `connection_quality` field to authenticated member
updates. Migration/key/roster changes reset affected samples. These values never
renew native engine progress, alter migration eligibility or route traffic.
Older servers can ignore the extra field. Server validation, checkpoint display
decoding and dashboard UI remain private. See `tests/network/test_online.py` for
authenticated nonce, timeout, expiry, reset and RPC-report regression coverage.
Read-only name/character reporting: [identity contract](online-player-identities.md).

Client tests cover invitations, certificate validation, UDP authentication,
native lifecycle and native Arcade checkpoint/adapter contracts without a local
server. Server-backed acceptance drivers and tests are preserved with private
source. Legacy manual integration tools require that external fixture explicitly;
normal game play does not. Historical gameplay acceptance results remain evidence
for their recorded revisions; this separation is not a new gameplay test.

The [native multiplayer flow](native-multiplayer-menu.md) uses a headless client.
Optional separate private hosting is an externally installed, supervised component;
its backend source remains private too.

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
