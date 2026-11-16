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
These files must be provisioned before generation. The private initializer below
creates the fixed layout only from explicitly staged online inputs.
Completed matching private scan initialization is also required. A changed source
generation needs explicit preserving migration rather than rewriting live data.

## Private configuration initialization

`initialize-private-host-mac.py` defaults to a read-only plan. Its installed
`--apply` runs as the locked identity after matching scan setup. The fixed private
data/host-input directory must contain exactly three0600 single-link regular files:
origin.json, trusted-root.json and catalog-1.pem. The origin file contains only
schema1, the canonical HTTPS origin and trusted_root_sha256. The fingerprint must
come from the operator's off-server ceremony and also be checked by the protected
cutover driver against its frozen input receipt; an internet response never
establishes this pin. No offline root private keys belong in this input directory.

All inputs are bounded and read through held nofollow/nonblocking descriptors.
Before writing anything, initialization verifies the root fingerprint, current
threshold signatures, matching origin and an unencrypted RSA3072 catalog-role
private key with threshold1. Root-role, foreign and encrypted offline keys are
refused. It generates no keys and performs no network access. Directory enumeration
has a fixed entry budget rather than reading an unbounded set of names.

New-only data/host setup creates the three exact configurations, copied online
inputs, separate0700 role states, an empty root-updates directory and a0600 marker
that becomes complete only after all writes. It does not initialize a repository
or register/start jobs. The marker binds source generation, origin, public pin and
all payload digests. Unchanged completed setup is reusable without staged inputs
and without rewriting files or live role state. Partial/foreign/changed/aliased
state is preserved and refused. Rotation/new source generations require explicit
preserving migration rather than automatic adoption or reset.

Every installed host worker, including the API child and generator's private check,
requires completed matching host setup. This is additional to scan initialization.
The input directory remains private and is preserved; it can be retired through a
separate verified operator step after recovery copies are checked.

## Off-server Windows trust ceremony

`prepare-workshop-trust-windows.py --directory <new-private-folder> --origin
<canonical-HTTPS-origin>` defaults to a plan; `--apply` creates a new store and
`--check` verifies it without rewriting files. Choose a Windows folder outside
cloud-synced/workshop directories. The tool creates two RSA3072 root keys with
separate random32-byte PKCS8 encryption passwords and a separate online catalog
key. The root requires both root signatures, while catalog signing uses only its
distinct online role. Only host-input/origin.json, trusted-root.json and
catalog-1.pem belong on the Mac. Root-a/root-b and recovery.dpapi remain off-server.

`mod_windows_recovery.py` uses current-account DPAPI with application entropy and
UI forbidden; it does not enable machine-wide decryption. Its fixed PowerShell
helper creates directories atomically with protected ACLs allowing only the
current user and SYSTEM, and verifies inherited private file access. Existing
ACLs are never relaxed/replaced. Aliased paths, hard links, changed files and
unknown/incomplete stores are refused. See Microsoft's
[DPAPI interface](https://learn.microsoft.com/windows/win32/api/dpapi/nf-dpapi-cryptprotectdata)
and [directory ACL creation](https://learn.microsoft.com/dotnet/api/system.io.directory.createdirectory?view=netframework-4.8.1).

Ceremony verification checks every payload digest, public root and online key,
decrypts the capsule and both encrypted roots in memory, then proves their root
threshold signatures over a fresh challenge. No root plaintext, passwords or
capsule contents enter CLI arguments, output, logs or Mac inputs. The CLI returns
public identities/fingerprint and its verification result. Expired public metadata
can still be checked for root recovery; it is not considered ready for deployment.
The private host initializer separately requires a currently live root.

This recovery depends on the Windows account's DPAPI profile/key material. Copying
the store alone to an unrelated computer is not a portable recovery backup.
Independent encrypted export and Windows-profile recovery still require explicit
operational backup/restore acceptance before the full workshop goal is complete.

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
