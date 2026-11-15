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
uses `-I -S -B` and a minimal environment; the scanner uses `-I -B` under
`_tsfpworkshop`. Both jobs specify `InitGroups=false`, disable core dumps and retain
launchd process-group cleanup. The local macOS launchd.plist manual confirms that
InitGroups controls supplementary group initialization when UserName is set;
actual launchd identity/group acceptance is still required.

The worker's `--system-worker` mode accepts no configuration argument: it reads the
fixed private scan configuration and requires a non-root identity without root or
admin supplementary groups. It ignores the older worker-writable GUI heartbeat.
Instead it checks a root-owned, bounded, single-link, non-writable-by-others
heartbeat through held nofollow descriptors. Service/UID/policy/status/time must
match. The root directory chain is also verified. Health is published atomically
at most every5 seconds or on state changes. Its readable metadata contains no
credentials or upload paths; worker-owned directories are not privileged write
targets. Unknown root monitor records are preserved rather than adopted.

Do not bootstrap these plists until the fixed private configuration, official
definitions and fresh isolated scanner work under the locked identity, and the
new native system-monitor lifecycle probe passes. Only then perform the explicit
reviewed production cutover and unattended boot tests. Public TLS/publication,
backup/recovery and internet-to-native-game acceptance remain separate gates.
