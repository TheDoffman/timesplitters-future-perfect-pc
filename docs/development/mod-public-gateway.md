# Workshop ingress gateway

The gateway is a separate confined Caddy process on127.0.0.1:8768. It forwards
only known workshop routes to static downloads on8766 or the private creator API
on8767. Its canonical HTTPS origin must match the API and signed publication.
It receives no repository, publication-tree or signing-key path. Public HTTPS
termination and independent internet acceptance remain separate requirements.

`Gatewayfile` rejects other Host values, restricts methods/path shapes, limits
JSON bodies to16384 bytes and archive PUT bodies to268435456 bytes, and bounds
headers/read/write timeouts. Download requests strip Authorization and Cookie;
download responses strip Set-Cookie. API requests retain bearer credentials but
replace the trusted forwarded HTTPS scheme and discard untrusted forwarding
headers. The Gunicorn backend trusts that scheme only from loopback. Do not
expose8767 directly or treat forwarded HTTPS as proof that public TLS is configured.

Gateway sandbox rules permit only localhost8768 binding, established responses
and outbound proxy connections to localhost8766/8767. Static-server rules remain
separate and cannot open those new API connections. Both roles use the native
parent-lifetime guardian, private scratch leases and bounded cleanup. The gateway
cannot directly read static snapshots or private host files. It must have a
separate private state directory from the publication controller.

Example trusted local configuration:

```json
{"schema":1,"origin":"https://workshop.example.test","state":"/private/workshop/gateway-state","caddy":"/path/to/caddy"}
```

The config parent/state must be owned and mode0700. Hostnames are canonical DNS
origins with the default port or explicit8443/10000, without paths or whitespace.
The signed origin and API Host/Origin include that port; Caddy matches the DNS
hostname separately because its Host matcher removes the request port. No uploaded value selects proxy
destinations, source/config paths, arguments, credentials or filesystem roots.

```sh
python tools/mods/hosting/gateway-service-mac.py --config /private/workshop/gateway.json
python tools/mods/hosting/gateway-service-mac.py --config /private/workshop/gateway.json --generate /private/workshop/new-gateway-agents
```

Generated login agents use managed admission plus an independent watchdog. Shared
`mod_service_watch.Supervisor` binds health to role, service identity, origin (or
repository for publication), actual controller PID, freshness and supervision
policy. Existing publication adapters preserve its metadata/agent contract.
Missing health, stalled controllers, excess sampled descendant memory or child
exit withdraw ingress. A guardian lease is renewed by bounded restart every30min.

Persistent system jobs use the separate fixed-role [system host controls](system-host-supervision.md).
Their protected root health replaces the GUI watchdog admission; the unprivileged
gateway/publisher cannot control system jobs. This component still requires native
protected installation/lifecycle acceptance and does not enable public routing.

Private Mac acceptance at sourcea8743bba verifies real Caddy routing, stripped
static-download credentials, controlled API scheme headers, route/method/Host
restrictions, a SHA-verified1MiB transfer and oversized JSON rejection. A separate
real Gunicorn/accounts/uploads/official ClamAV/signing fixture verifies creator
and reviewer roles, clean review, EICAR rejection, automatic signed publication,
logout and revoke-to-404 in0.549s. Keys/accounts are disposable fixtures.
Transient static refresh can return502 while downloads are withdrawn.

Raw HTTP checks reject conflicting lengths400, unknown transfer encoding501,
oversized headers431 and both fixed/chunked16385-byte JSON413 before completed
backend requests. Exactly16384 chunked bytes succeed. A request supplying both
Content-Length and chunked encoding is normalized to chunked framing with no
Content-Length at the backend; it consumes exactly the decoded body.
The actual-Caddy test in test_gateway_service.py preserves these checks and is
registered in native CI (skips without macOS/Caddy). It also exercises every
launcher API route, including report creation/history/resolution, digest lookup,
scan evidence and POST-only Lua/DDS previews. These private routes retain bearer
credentials and the16KiB JSON cap; wrong methods, malformed IDs and unknown paths
remain unavailable. Recording loopback backends establish transport routing;
separate API tests establish account roles and package authorization.

Temporary launchd jobs for both gateway and publication pass watchdog/controller
restart, scaled128MiB memory cancellation with256MiB allocation, actual15s stalled
heartbeat cancellation, child reaping and scratch recovery. All test jobs were
unloaded. See STATUS for receipt hashes.

These checks do not establish public deployment. Real HTTPS/internet, private API
client-IP rate accounting, public abuse/load behavior and production system-domain
service identity remain deployment gates.
