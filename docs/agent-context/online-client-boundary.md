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

The private copy retains the previously deployed source and test versions.
Earlier Git revisions already contained server code; removing it from the current
tree does not rewrite published history. Future changes must respect this split.
