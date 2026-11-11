# Private mini service dashboard

`tools/network/online_dashboard.py` is an optional read-only observer in the lobby
service process. `lobby_server.py --dashboard-port PORT` binds a second HTTP server
strictly to127.0.0.1. It never adds an admin endpoint to the public HTTPS listener.
The static HTML/CSS/JS use no external packages or assets. The private HTTP server
limits eight workers and three-second idle reads, verifies Host, rejects cross-site
fetches, and supplies CSP/frame/no-cache headers. It has no write routes.

`Monitor` samples once/second with180 traffic samples and100 events retained in
memory. Snapshots whitelist room labels, peer/native IDs, role, native progress
age, remaining lease, relay registration/route class and checkpoint metadata.
They never include tokens, wire/relay keys, full endpoints, raw checkpoints or
request paths/bodies. The observer holds Directory.lock only while copying; it
never polls/renews/expires a room or changes gameplay. Brief transitions between
samples may be missed. Legacy v1/v2 sessions explicitly lack native telemetry.

`RelayServer` adds received/sent-byte counters; accepted forwarding/drop semantics
are unchanged. Optional server.monitor observes HTTP status/duration, worker use
and fixed-label timeout/TLS/disconnect/internal failure categories. CPU is service
process CPU as a share of logical CPUs; memory is peak process RAM (unavailable
where stdlib resource is absent), not whole-machine live memory. Dashboard polls
do not inflate game-directory request counts. Uptime/history reset on restart.
UI marks transport failure and delayed sampling separately; service health does
not prove public/WAN reachability. Lobby labels/events use textContent, not HTML.

`install_online_service.py --dashboard-port 41822` copies the observer and three
static assets alongside the existing server. Existing enabled dashboard ports
are preserved when omitted during an upgrade; certificate, public ports and
other settings must be explicitly retained during deployment. The installed
launch agent keeps the private dashboard alive across login/restart.

`view_online_dashboard.py --ssh HOST` owns a loopback-only SSH forward and opens
the browser. It uses existing SSH authentication and validates readiness without
HTTP proxies. Ctrl+C closes only its SSH child. `--no-browser` supports a hidden
operator session; nondefault local/remote ports are supported. No router mapping
or public dashboard listener is required.

Validation: `tests/network/test_online_dashboard.py` is registered in the native
runtime suite. It covers credential exclusion, read-only leases, host transitions,
bounded history, legacy labels, loopback/Host/method/path/CSP boundaries, real HTTP
and UDP counters and installed-server startup/certificate/port preservation.
Shared network tests still cover relay authentication, migration and persistent
HTTPS. Record physical Mac deployment and browser acceptance in STATUS/backlog.
