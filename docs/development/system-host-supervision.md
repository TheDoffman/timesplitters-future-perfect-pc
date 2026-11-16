# Persistent workshop host control

The API, signed publication controller and ingress gateway have fixed system-job
entry points. This component is implemented separately from deployment: no new
production jobs, private configuration, keys or public routes have been enabled.
The existing private Tailscale443 mapping must remain unchanged.
Release preparation verifies source against Git, allowing only exact bytes or
the exact CRLF rendering explicitly declared by HEAD attributes for Windows
operator scripts. It freezes those verified checkout bytes; mixed endings,
source changes and arbitrary filter transformations are refused.
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

## Protected setup and scanner admission

`prepare-installed-host-mac.py` defaults to a read-only plan. Its installed root-only
`--apply --input <private-operator-inbox>` accepts only three bounded0600 single-link
online inputs owned by the administrator identified by sudo. It matches their
origin/public root to the immutable source profile before creating the fixed
UID600 data/host-input directory. Existing exact inputs are reusable; changed,
aliased or unknown state is preserved and refused. Only fixed output names and
fixed child programs/modes are allowed; no generic command or UID is supplied.

The preparer runs both initializers and the official FreshClam updater after
explicit group/GID/UID dropping, using installed Python with isolated startup.
The root process does not parse online private keys or execute worker code.
`accept-installed-scanner-mac.py` then runs as the locked identity with installed
source/runtime provenance and matching private scan setup. It creates disposable
owned files and requires all four real-engine outcomes: benign content clean,
standard harmless EICAR test blocked, empty official database error and missing
engine error. The positive/detection controls must report actual ClamAV identity
and the required isolation policy; temporary files must be removed.

Only after scanner admission succeeds does preparation invoke the fixed root
scan/definition and three host-monitor generators. A final exclusive0600 protected
monitor/admission.json records release/UID/public pin/time and scanner admission.
The preparer neither registers/starts the jobs nor changes internet routing.
Generator outputs/partial state are new-only and preserved on failure. Job
registration, lifecycle acceptance, protected scanner execution and boot/public
verification remain distinct steps. Fixture tests establish policy/order/file
protections, not actual privileged engine operation.

## Private job registration and lifecycle

`register-installed-host-mac.py` defaults to a plan. Its immutable installed
root-only `--apply` requires a matching protected admission record no more than
300 seconds old, including all scanner controls. It derives exactly nine plists
from the immutable generators and compares every generated byte to that policy.
Existing persistent files or loaded labels are refused before starting anything.
There is no supplied job, process ID, command, user, destination or configuration.

All four monitors bootstrap before the three long-lived workers and the periodic
scanner/definition jobs. These initial jobs load directly from protected staging;
no new boot plist exists yet. `accept-private-host-services-mac.py` runs after
explicit credential dropping and accepts only fixed HTTP loopback8768 routes.
It verifies API health, the catalog's exact signed bytes against the SOURCE-pinned
root, freshness and protected role/PID/release health for all three host roles.
It does not download a bootstrap root or verify public TLS/internet connectivity.

Registration then crash-restarts each of the three long-lived workers and four
monitors using its fixed launchd label. Numeric owner/descendant snapshots bind
the observed old tree; all observed PIDs must disappear and a different service
PID must appear. Fresh private readiness is required again, with unchanged root/
origin and a nondecreasing catalog sequence. One-shot scan/definition scheduling,
queued package work and unattended boot still have separate acceptance requirements.

Only after live readiness and these restart checks pass are new root-owned0644
copies installed in /Library/LaunchDaemons. Any ordinary failure unloads only jobs
successfully loaded by this invocation, in reverse order, and removes only its own
created file inodes with unchanged expected content (or a partial write prefix).
An ambiguously accepted bootstrap is not claimed as owned; foreign/changed items
are preserved. Cleanup failures are reported rather than claiming rollback. Job
files are not installed before private acceptance, so an earlier interruption
cannot schedule unverified staging jobs for the next boot. Power loss during
the final persistence copy may leave an incomplete set of already verified jobs;
the journal must be inspected and repaired before claiming boot acceptance.

Root-owned bounded0600 deployment journals record preparing/starting/restart/
complete/failed states and whether rollback completed. No routing changes occur.
Tests exercise file protections, preflight collisions, rollback, uncertain job
ownership, owner/PID/child/restart/sequence refusal and signed loopback readiness
with disposable fixtures. Actual privileged registration and all seven restart checks now pass in the
14642b44 repair, with unchanged sealed source601. Independent checks confirm
nine loaded/persistent root-owned job files. Physical boot remains unverified.

This recovery depends on the Windows account's DPAPI profile/key material. Copying
the store alone to an unrelated computer is not a portable recovery backup.
The separate passphrase-encrypted portable export/check/new-Windows import is
implemented; see mod-host-recovery.md. Production export, independent passphrase
custody, off-device restore and operational backup acceptance remain incomplete.

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
The role directory is explicitly set0755 after exclusive creation. Merely passing
0755 to mkdir is insufficient under the setup/launchd077 umask: the actual native
cutover exposed0700 directories that correctly prevented unprivileged health
reads. Marker/plists stay0600, health stays0644 and no non-root writes are granted.
A regression exercises generation under077. Existing sealed code is preserved;
correcting already-created root-owned directory modes is an explicit setup repair.

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

The separate public8443 cutover now preserves the original private443 mapping
and passes Windows public-relay HTTPS/normal certificate and pinned catalog
verification, then native owner enrollment and SDK upload, isolated scan,
operator approval, signed download and inactive install; see STATUS. Root
registration itself never changes routing. Packaged Windows public-to-game and
signed withdrawal/launch refusal pass; other-host acceptance remains open. Do not interpret configuration persistence or controlled service restarts
as physical boot acceptance.
