# Private online identity display

This is player-side connection metadata; the dashboard/service remain private.
See [the boundary](online-client-boundary.md). The feature branch is based on
the unmerged `dashboard-match-telemetry` work at `b26cf41a`.

`pc_online_roster.h` captures bounded, read-only names and character metadata
from a validated active Arcade host. Actor+70 supplies the same name used by the
native kill feed. The HUD portrait path (`13E230` → `2BE690`) resolves actor+48
character and signed actor+C0D skin. The original selector `2BE650` resolves
the localized name ID through `2BE600 -> 2BE300`, then `3D7B0` supplies text.
Native selector callers `2C0902/2C1F48` confirm the name path. Character-row+C
is biography text and must not be used as a name; the native live check caught
that distinction. Empty/unknown names remain explicit fallbacks. No guest
pointers, personal profile files or credentials enter the report.

`pc_arcade_migration_frame(after)` refreshes metadata on active game frames;
menu-only ticks stop during gameplay. Its guarded callback saves/restores guest
registers and retains the one-second throttle. The menu fixture verifies this
path without running the menu loop, including throttling and LAN exclusion.
The native control transport fixture verifies the post-frame callback dispatch.
`mp_publish_metadata` sends an independent `.players` record through the shared
Windows/SDL IPC adapter. This 1564-byte version1 record has sixteen bounded rows
and a publication serial, including when the match clock pauses. Existing menu
and checkpoint ABIs are unchanged. Startup/exit cleanup covers both owned files.

`online_menu.decode_roster` validates the entire binary record. The headless
controller installs only changed reports for the current host. `NativeArcade`
publishes optional `player_info` only as current host for five seconds after
capture and clears it on host/epoch changes. Older services can ignore it.
Private services must whitelist fields, authenticate the host and bind display
identities to the current round/scene/generation before using actor slots.
Identity data never enters public room listings or changes authority/leases.

`test_online_roster.py` compiles the production reader and checks the Python ABI,
vacant slots, pointer bounds, control stripping, host fencing, expiry and malformed
records. It is registered in the native CI suite. The native IPC and FOV/menu
fixtures retain their ordinary coverage. Updated matching game builds are needed
on Windows/Mac/Linux; the logic and IPC implementation are shared.

`TSFP_NATIVE_MENU_TRACE` reports capture/publication outcomes and refused read
extents without player labels, for live client-side identity diagnosis.

Windows and physical Mac builds/platform checks pass with matching `f818e717`
compatibility. Real native Windows-host/Mac-guest LAN acceptance verified player
and bot names, original portraits, localized character names, scores and six bots
on the authenticated private dashboard, then Return to Lobby and closure. Private
schema/diagnostic checks pass on both hosts; public artwork routes require login.
The public-address native gameplay run hit TLS handshake timeouts; do not infer
WAN gameplay acceptance from the passing LAN/public-dashboard checks.


Restored on `fix/dashboard-player-names`, based on native UI3542601b. The newer
native multiplayer branch omitted this previously validated reporting path,
while the private mini still expected optional host `player_info`. Selectively
ported dd870ca2/d24862b1/2c07a2ed/10556fe7/6a486c85 without replacing newer UI,
private hosting, separate admission-code behavior or local community routes.
The earlier native acceptance above applies to its recorded identity branch;
current integration validation is recorded in STATUS.md. No private service code
or dashboard deployment is required. Updated matching clients are required.

Host migration also retires owned replica bot gun models before stock AI overwrites their hand handles. See [migration cleanup](../development/arcade-host-migration.md#promoted-bot-weapon-attachments); test_arcade_promotion_weapons.py covers both host defines and fallback.

Current native-UI integration: matching Windows/Mac builds and real paired Deathmatch pass; authenticated dashboard labels remain populated through host loss, while six bots/scores/timer continue. Promoted-weapon cleanup leaves no visible orphan models. See STATUS.md for host-specific counts and scope.
