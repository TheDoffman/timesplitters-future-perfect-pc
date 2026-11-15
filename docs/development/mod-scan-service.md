# Mac scan worker and watchdog

**Containment hold:** an actual sandbox probe demonstrated that setsid/setpgid
can escape process-group-only sampling/cleanup. Both installed private agents
are disabled and unloaded. New worker admission is held until the new parent-liveness
guardian and descendant-tree supervision pass actual adversarial host checks. Do not
enable these jobs for untrusted uploads yet. The earlier normal/group probes
remain scoped evidence, not proof against this escape.


`tools/mods/hosting/scan-service-mac.py` generates two private launchd login-agent
plists. The worker processes one queued upload and one due reviewed-release rescan
each minute. The independent watchdog samples that worker process group's total
RSS every half second, up to2GiB and eight processes, and enforces a480-second
observed wall deadline. These are sampled limits, not a kernel memory quota.
The worker also has a480-second alarm and launchd CPU/file/core limits; existing
scanner children retain their own180-second alarm and sandbox/resource policy.

launchd owns both job identities and cleans up the worker's remaining process
group when the job exits. Watchdog cancellation uses `launchctl kill` against the
owned service label, never a sampled PID. A worker must lead its process group
and refuses admission without a fresh, matching watchdog heartbeat. The watchdog
restarts after exit; the worker runs at load and on its60-second interval.
The jobs do not start a public server, update definitions or publish catalogs.

## Prepare private configuration

Use existing owner-only mode0700 repository, database and status directories.
The configuration parent must also be private. This is a shape example; replace
all paths with the actual private host paths:

```json
{"schema":1,"repository":"/private/workshop/repository","scanner":"/path/to/clamscan","database":"/private/scanner/database","state":"/private/workshop/scan-status"}
```

No uploaded fields or shell strings select these paths. The scanner's explicit
local installed alias may resolve to its canonical executable. Keep the selected
Python virtual environment and source installation available for the job lifetime.
Generate into a NEW directory under a private parent:

```sh
python tools/mods/hosting/scan-service-mac.py --config /private/workshop/service.json --generate /private/workshop/new-agent-bundle
```

The command returns the worker and watchdog labels. Inspect both plists before
installing them in the service user's `~/Library/LaunchAgents`; preserve existing
files/jobs. Load the watchdog and worker using `launchctl bootstrap gui/UID PLIST`
with the actual numeric UID. The first worker records a waiting state and leaves repository data untouched until the
watchdog has produced its healthy heartbeat; the next interval retries. The
operator may kickstart the worker after confirming health.

These are login agents: they need that user session, and they do not establish
unattended boot operation or a dedicated non-administrator production identity.
Public hosting still requires the intended service identity, stable pinned
installation/dependencies, definition updates, backup/retention, signed publication
refresh/invalidation, and restart/cutover acceptance. Do not expose an incomplete
prototype as a production workshop.

## Observe and stop

`worker.json` and `watchdog.json` in the private status directory are bounded,
atomically replaced files. Heartbeats bind the configured service label and expire
after10 seconds. The last fault survives later healthy updates. Unknown files,
links and unsafe directories are preserved and cause refusal. No request secrets
or unbounded launchd output logs are written; stdout/stderr go to `/dev/null`.
Inspect private repository states/audit history for scan results.

To stop, boot out the watchdog label first and then the worker label from their
GUI domain. Do not target unrelated jobs or PIDs. Remove only verified owned
installed plist files when uninstalling; preserve the private repository and
status/fault evidence. Logout/power loss, watchdog restart and forced cancellation
must be distinguished from completed scans. An interrupted upload/rescan remains
withheld and follows the existing lease/reclaim rules on a later worker cycle.

Build the native lifetime helper before any isolated scanner command:

```sh
python tools/build/build-scan-guardian.py
```

Its receipt binds the signed helper image to the current C source. Missing,
changed, linked or stale helpers refuse scanning. The helper owns and reaps the
actual scanner PID, watches a private parent pipe and a210-second fallback, and
survives controller process-group cleanup long enough to stop escaped scanners.
The process-tree watchdog samples numeric PID/PPID/RSS/UID metadata (no argv or
environment). This repair must pass the new actual escape/controller-death tests
and be included in the pinned installed release before lifting the hold.
