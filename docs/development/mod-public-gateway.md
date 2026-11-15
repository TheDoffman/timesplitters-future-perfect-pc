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
origins without ports, paths or whitespace. No uploaded value selects proxy
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

This code/configuration alone is not public deployment. Actual Caddy/Gunicorn
routing, credential/origin isolation, body/framing limits, large downloads,
managed restart/resource behavior and real HTTPS need acceptance. Private API
client-IP rate accounting, public abuse/load behavior and production system-domain
service identity remain deployment gates.
