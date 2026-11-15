# System-domain scan supervision

This is an implemented deployment component, not a running production service.
The native permission probe proved that `_tsfpworkshop` can inspect a system job
but cannot signal it through `launchctl`. The existing GUI scan prototype remains
separate. Root installation, permission/lifecycle acceptance of the new monitor,
locked-account configuration/definition initialization and boot operation remain
required before public deployment.

`tools/mods/hosting/system-scan-monitor-mac.py` runs a small privileged monitor.
It controls only the scanner label derived from the fixed configuration path
`/Library/Application Support/TSFP Mod Workshop/data/scan/service.json`. There is
no user-supplied target, executable, repository path or command interface. The
monitor never imports the worker or reads uploaded content or worker configuration.
It samples numeric process metadata, verifies the restricted owner throughout
the descendant tree, and uses the launchd job label for cancellation. It never
signals a sampled PID. Limits remain2GiB aggregate sampled RSS,8 processes and
480 seconds per observed worker lifetime. Sampling cannot promise an instantaneous
RSS ceiling; scanner guardian and resource limits remain independent controls.

Loss of observation, excessive processes, wrong owners, deadlines and memory
faults trigger a stop of that fixed job. A denied/failed stop produces an error
heartbeat, never a claim of termination. A crash or stale heartbeat prevents new
worker admission. Existing scans also retain their native lifetime guardian and
wall/resource limits; heartbeat expiry alone is not an immediate process kill.

The privileged entry point requires the immutable root-owned installed release,
its completed installation receipt, matching locked-account identity and installed
Python. `--generate` writes new root-owned plists and a deployment marker under
the fixed `monitor` directory. It refuses existing state and does not load jobs.
The generated Python path is canonical, inside the installed runtime. The monitor
and a short protected bootstrap start with `-I -S -B` and a minimal environment.
The bootstrap accepts only fixed scan/definition roles, or a nonce-bound inert
proof role; it accepts no command, executable, user or configuration path. It
clears groups, sets real/effective GID and UID, checks kernel credentials and
root/admin directory membership, and proves root cannot be restored before exec.
No worker module, private configuration, repository or uploaded content is loaded
as root. The same PID then runs worker code as the restricted account with `-I -B`.
The drop uses setgroups/setregid/setreuid, matching Python's
[subprocess credential primitives](https://docs.python.org/3.12/library/subprocess.html#subprocess.Popen),
whose explicit group clearing succeeded in the installer smoke on this Mac.

Real launchd tests with both InitGroups values retained extra kernel groups on
this Mac, while the installer's explicit subprocess drop produced only600.
The bootstrap uses an explicit drop rather than relying on that plist setting.
The monitor permits a new PID a maximum10-second root startup; its `starting`
heartbeat is unhealthy. Once the PID is restricted, a return to root fails closed.
Owner and RSS come from one numeric process snapshot. The system worker waits
boundedly for the monitor to observe its restricted phase before admitting work.
The label-based cancellation,480-second lifetime and independent native scanner
containment remain in force. Native bootstrap enforcement is still required.

The worker's `--system-worker` mode accepts no configuration argument: it reads the
fixed private scan configuration and requires a non-root identity without root or
admin supplementary groups. It ignores the older worker-writable GUI heartbeat.
On macOS, `mod_host_identity.py` reads effective groups through the fixed
libSystem `getgroups` entry point, with an explicit gid_t ABI and a bounded array.
Modern Python `os.getgroups()` instead returns directory memberships unaffected
by setgroups; see [Python's documentation](https://docs.python.org/3.12/library/os.html#os.getgroups)
and the host getgroups(2) manual. Kernel groups must be empty or the service GID;
directory membership separately must exclude root/admin. Both lists are retained
in smoke/proof reports. The native launchd and subprocess privilege-drop paths
must satisfy these checks independently.
It checks a root-owned, bounded, single-link, non-writable-by-others
heartbeat through held nofollow descriptors. Service/UID/policy/status/time must
match. The root directory chain is also verified. Health is published atomically
at most every5 seconds or on state changes. Its readable metadata contains no
credentials or upload paths; worker-owned directories are not privileged write
targets. Unknown root monitor records are preserved rather than adopted.

## Private initialization

`initialize-private-scan-mac.py` defaults to a read-only plan. Its installed
`--apply` entry runs only as the locked account with the installed runtime.
It creates the fixed private data/scan tree, empty repository/database/state
directories, exact scanner/updater configs and an initialization marker. The marker
becomes complete only after all writes finish. An unchanged complete layout is
reusable without rewriting config or application data. Incomplete/foreign/changed/
linked state and different source generations are preserved and refused.
It never downloads definitions, creates/migrates a repository or starts services.
Both system worker and system updater modes require matching completed state.

Root plist generation checks that state through a subprocess with UID/GID600 and
cleared groups. It emits three private artifacts: monitor, scan bootstrap and a
six-hour official-definition bootstrap. Generation does not register/load them.
Host upgrades still need generation/config migration with preservation of existing
repository/database/state, rather than automatic adoption of an unknown layout.

Do not bootstrap these plists until the fixed private configuration, official
definitions and fresh isolated scanner work under the locked identity, and the
new native system-monitor lifecycle probe passes. Only then perform the explicit
reviewed production cutover and unattended boot tests. Public TLS/publication,
backup/recovery and internet-to-native-game acceptance remain separate gates.

## Native permission proof

`probe-system-scan-monitor-mac.py` defaults to a read-only plan. Its explicit
`--apply` requires the immutable installed release and isolated Python, matching
the monitor's production installation gate. It creates a new root-protected
fixture and two inert nonpersistent system jobs, with distinct derived labels.
The target uses the actual protected bootstrap. The controller observes an unhealthy
root startup, then the restricted job reports real/effective UID/GID, kernel and
directory groups, denied root restoration and its
process group, reads root health, proves it cannot overwrite that health, and
starts one inert child. Refused identity/health checks retain numeric diagnostics.

The temporary root controller executes the installed monitor core against that
job. It first observes the actual process tree, then uses a controlled monotonic
clock only within this trusted proof to exercise the existing wall deadline.
It must receive successful label cancellation; the parent independently verifies
worker and child exit and that the restricted reader rejects terminated health.
The proof never changes the OS clock, production budgets or monitor CLI. It also
does not allocate2GiB merely to provoke a memory fault.

Cleanup attempts every owned job even after a command timeout, and acceptance
requires that all temporary labels are absent. A120-second parent alarm bounds
the explicit proof. Reports remain under the new root-owned fixture; existing
production monitor state, private scan configuration and uploaded content are
untouched. This proves the kernel permission/heartbeat path when run successfully;
it does not replace real persistent monitor-loop, crash/restart or boot acceptance.
