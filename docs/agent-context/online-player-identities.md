# Private online identity display

This is player-side connection metadata; the dashboard/service remain private.
See [the boundary](online-client-boundary.md). The feature branch is based on
the unmerged `dashboard-match-telemetry` work at `b26cf41a`.

`pc_online_roster.h` captures bounded, read-only names and character metadata
from a validated active Arcade host. Actor+70 supplies the same name used by the
native kill feed. The HUD portrait path (`13E230` → `2BE690`) resolves actor+48
character and signed actor+C0D skin; the verified character table also supplies
the localized label. Empty/unknown names remain explicit fallbacks. No guest
pointers, personal profile files or credentials enter the report.

`pc_arcade_migration_frame(after)` refreshes metadata on active game frames;
menu-only ticks stop during gameplay. Its guarded callback saves/restores guest
registers and retains the one-second throttle. The menu fixture verifies this
path without running the menu loop, including throttling and LAN exclusion.
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
