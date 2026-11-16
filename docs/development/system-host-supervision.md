# Persistent workshop host control

The API, signed publication controller and ingress gateway have fixed system-job
entry points. This component is implemented separately from deployment: no new
production jobs, private configuration, keys or public routes have been enabled.
The existing private Tailscale443 mapping must remain unchanged.
Actual f099 native component checks pass258 cases, including live ordinary-user
Gunicorn auth and complete group cleanup. The matching release is prepared only;
protected new-role jobs, private provisioning and restart/boot remain unverified.

`locked-service-bootstrap-mac.py` accepts api/publication/gateway roles in addition
to scan/definitions and its inert proof role. These are fixed arguments to
`system-host-service-mac.py`, never a supplied command, executable, user, target
or configuration path. The root prelude retains explicit group/GID/UID dropping,
irreversible privilege-loss checks and the immutable installation gate before exec.
All host configuration, account/upload operations, signing and networking run as
the restricted identity. The monitor runs with isolated, site-disabled Python.

`mod_system_host.py` derives the three labels and fixed paths from the installed
host root. Configurations live in data/host/{api,publication,gateway}.json with
separate private {role}-state directories. The API and publisher use the same
data/scan/repository. All origins must match the API and verified catalog root.
Public server executables must be the installed runtime Caddy; publication uses
trusted-root.json and catalog-1.pem through catalog-4.pem in data/host, with
optional root updates in its root-updates subdirectory. Only authorized catalog
keys may be loaded. Offline root private keys do not belong on this host.
These files must be provisioned before generation; this component creates none.
Completed matching private scan initialization is also required. A changed source
generation needs explicit preserving migration rather than rewriting live data.

`system-host-monitor-mac.py --role <api|publication|gateway> --generate` validates
configuration in a child with explicitly dropped credentials. It reads no worker
configuration or signing material as root. It requires the matching scan monitor
generation first, then writes new-only root-owned worker/monitor plists and a
deployment marker under monitor/{role}. Existing/partial state is preserved and
refused. Generation neither registers jobs nor starts services. The fixed worker
bootstrap runs with KeepAlive, a60-second restart throttle and group cleanup;
its independent root monitor has a5-second restart throttle. Use only the immutable
installed source/runtime, not a mutable checkout, to generate or run these jobs.

The monitor samples numeric PID/owner/RSS metadata for the fixed launchd label.
It allows at most10 seconds of unhealthy root bootstrap, then requires the locked
UID across the process tree and forbids returning to root. It limits the observed
tree to8 processes,2GiB aggregate sampled RSS and3600 seconds per observed lifetime.
After15 seconds it requires a fresh, matching controller heartbeat. This bounded
inert JSON record is the only worker-owned input the privileged monitor reads;
held nofollow descriptors, private owner/mode checks, single-link regular-file
requirements, stable metadata and an8KiB cap reject aliases, pipes and oversized
or changed data. Nothing in this record selects a path, command or job target.
Loss of observation, wrong ownership, stale health or limits stops only the fixed
system label. Failed cancellation never claims successful termination.

Root health is published atomically as readable0644 metadata in the protected
directory. Role, label, UID, release, actual controller PID, policy, status and a
10-second freshness bound must match. Idle, starting, stale, failed and terminated
health does not admit a host worker. Gateway/publication adapters ignore their
older writable GUI watchdog files in system mode and cannot generate/control jobs.
The older login-session prototypes retain their separate behavior.

The API controller waits for root health, owns a checked private lock and starts
the installed Python/Gunicorn child through the verified native lifetime guardian.
It checks root health while serving and publishes its heartbeat every5 seconds.
The child receives fixed configuration and binds only127.0.0.1:8767; the existing
trusted Gunicorn settings retain2 synchronous workers and bounded requests.
The optional Gunicorn control socket is disabled; protected job control has a
single fixed interface and no additional local management socket.
The guardian has a3600-second deadline, and the parent retires after3300 seconds
so launchd can renew the service. Parent death closes the private guardian pipe
and stops/reaps the API process group. Root observation is an additional control,
not a substitute for that independent lifetime containment.
The guardian's explicit terminate-group mode is for this trusted forking server;
the scanner default still stops its wait-owned direct child, even after group
escape. Server mode uses waitid WNOWAIT to retain child/PID/group ownership until
remaining workers are stopped, including a master that exits before its workers.

Gateway/publication controllers retain their confined Caddy children, bounded
scratch leases, private locks, signed refresh/revocation behavior and root-health
checks in system mode. Independent native system-job lifecycle, locked-account
clean/EICAR/error/rescan, restart/boot, trust recovery/backup and public internet
acceptance remain required before claiming a deployed workshop.

`test_system_host.py` also starts the real child factory under the native guardian
with a disposable private account, verifies HTTP origin/scheme and bearer checks,
observes the configured Gunicorn process tree and closes the parent pipe to check
group cleanup. This ordinary-user fixture tests child transport/lifetime, not the
installed root-to-UID600 system-job path.
