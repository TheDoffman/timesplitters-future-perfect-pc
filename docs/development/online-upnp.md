# Automatic port mapping for player hosting

Hosting a private server starts its HTTPS directory and shared UDP relay, then
attempts to open both ports through a local UPnP Internet Gateway Device. The
defaults remain TCP30820 / UDP30821. The existing in-game flow and invitation
codes are unchanged. Any connected player can host an independent Arcade or
Story lobby through that relay without opening an inbound UDP port.

The game keeps its mappings alive while hosting and removes its own mappings on
normal exit. Leaving a lobby keeps an owned directory, its relay and both port
mappings alive. Only closing the private server releases those mappings.
Existing matching manual forwards are reused; conflicting forwards are left
untouched. A router restart can be recovered when its IGD stays reachable at the
same control endpoint. UPnP failures leave hosting available for LAN use and
manual forwarding. The controller records mapping availability for diagnostics.
If automatic mapping fails, only the private-server owner needs to forward the TCP directory and UDP
relay ports. The server must remain running for its lobbies; their traffic uses
its bandwidth and may take a longer route than a direct connection.

UPnP must be enabled on the router. It cannot traverse provider CGNAT, an upstream
second router, or a blocking host firewall. A successful mapping is not proof of
Internet reachability. Community-relayed matches already use outbound connections
and do not require a player-side inbound mapping. UPnP applies only to
the private-server owner. Older private directories without relay support retain
the direct-lobby fallback, where that lobby host needs its own UDP mapping. The
existing community service is unchanged.

Routers receive a one-hour lease renewed while hosting. Permanent-only routers
are supported with ownership-checked cleanup on normal exit; after a crash or
power loss, those routers may retain the mapping until manually cleared. Short
leases on other routers expire automatically. The game never deletes or renews
another application's mapping.

Set `TSFP_UPNP=0` before launching to disable automatic mapping. Automated tests
and isolated gameplay runners always disable real router changes. The shared
Python helper has no additional dependency and runs on Windows, Mac and Linux.
Simulated router contracts and Windows integration are tested; physical-router,
Mac/Linux device and outside-network acceptance remain to be verified.

Protocol: [UPnP WANIPConnection](https://upnp.org/specs/gw/UPnP-gw-WANIPConnection-v1-Service.pdf).
