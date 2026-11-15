# Live signed workshop downloads

`mod_live_publication.py` and `hosting/live-publication-mac.py` keep the private
repository and public static exports separate. The controller polls approved
availability and cumulative revocations every half second. Account/login/upload
audit writes do not trigger restarts. It withdraws the old server before signing
a new monotonically numbered export, validates it, rechecks moderation and starts
Caddy against only that tree. Changes after export cause withdrawal and retry.

Refresh/copy/signature/root/retention errors, a recovery barrier or an unexpected
server exit stop serving. There is no automatic fallback to an old export. Polling
is sampled invalidation, not instantaneous revocation: existing requests can
complete before withdrawal. Independent internet revocation timing still needs
acceptance. Clients continue to enforce signed catalog expiry and revocations.

The controller excludes releases with less than30 seconds of scan validity,
refreshes metadata every30 minutes and gives Caddy a bounded native guardian
lease of at most one hour and never beyond catalog expiry. Parent-pipe loss
kills/reaps its owned server PID; uploaded content cannot choose this lease.
Existing scanner callers keep the guardian's default210-second deadline. Caddy
gets a minimal environment and static root, never signing keys or database paths.
The existing Caddyfile binds only127.0.0.1:8766; this does not establish public TLS.

Prepare private mode0700 repository and state directories outside one another.
Use a separate online catalog-role key and the operator-supplied pinned root;
follow [the signing ceremony](mod-catalog-trust.md). Never put offline root keys
in the controller configuration. An example shape is:

```json
{"schema":1,"repository":"/private/workshop/repository","state":"/private/workshop/publication-state","caddy":"/path/to/caddy","trusted_root":"/private/workshop/root.json","catalog_keys":["/private/catalog-key/private.pem"],"root_updates":[]}
```

The config parent must be private and all paths absolute. Catalog keys retain
their private0600/non-root ownership requirements. The controller serializes
through a private lock. Run only one configured controller for a repository and
listen address; preserve existing jobs and unknown files:

```sh
python tools/build/build-scan-guardian.py
python tools/mods/hosting/live-publication-mac.py --config /private/workshop/publication.json
```

Use the pinned signing dependencies for this host runtime. A persistent production
service, production root/endpoint configuration and independent public acceptance
are still required. The source CLI alone is not a deployed service.

The private publication store retains current and previous snapshots. Receipts
bind every file hash/size and directory; only verified owned outputs are pruned.
Complete receipts left before retention can recover on restart. Changed/unknown
files, aliases and an export interrupted before its receipt are preserved and
block serving until operator reconciliation. Do not manually point Caddy at such
outputs. At most three complete snapshots are admitted during replacement, with
an additional bounded staging space requirement. These copies are disposable
exports; the separate private repository and signing/recovery procedure remain
authoritative. Status/fault metadata is private and bounded; server output goes
to the null device.

Private Mac loopback acceptance atdf6c9eed passed actual Caddy revocation to HTTP
404 in0.612 seconds, scan-error withholding, recovery-barrier withdrawal and
controller SIGKILL cleanup of guardian/server. The actual CLI loader was used with
disposable fixture signing identities. This does not prove public internet timing,
production pins or persistent service deployment; see STATUS for the evidence hash.

## Managed login agents

Generate a new private bundle from the same config:

```sh
python tools/mods/hosting/live-publication-mac.py --config /private/workshop/publication.json --generate /private/workshop/new-publication-agents
```

The bundle contains a KeepAlive controller (`--managed`,60s restart throttle) and
independent watchdog (`--watchdog`,5s throttle). Both use the selected interpreter
and source path, null output, private umask and core/file limits. Inspect/pin the
source/runtime and preserve existing labels/plists before loading the returned
job identities in the intended GUI domain. This does not install/bootstrap jobs.

Managed startup records waiting and admits no keys or public bytes until matching
watchdog health watches its actual PID. The controller checks that health each
cycle. The watchdog samples the whole numeric PID/PPID subtree regardless of
sessions, with2GiB/eight-process bounds. After a15-second startup grace it requires
a fresh matching controller heartbeat. Missing/bad samples or stale health cancel
the owned launchd controller label; the native parent-lifetime guardian then stops
Caddy. No sampled PID is signalled. Status/faults are private and bounded. A busy
or stalled operation may cause safe withdrawal and require operator inspection.

These remain login agents, not unattended-boot services or dedicated production
identities. Actual launchd lifecycle, memory/stall cancellation and public uptime
acceptance must be recorded separately. Existing `--config` without a mode keeps
manual foreground operation for explicit private acceptance; generated agents
always select the supervised managed mode.

Temporary Mac launchd acceptance at6b245a9f passed watchdog/controller restart,
scaled128MiB RSS cancellation (256MiB private allocation), and actual15s stalled
heartbeat cancellation. All cases reaped guardian/Caddy and withdrew downloads;
test jobs were unloaded. These used disposable signing keys and synthetic reports,
not production configuration. Fault-owner tests at54989350 additionally verify
pre-server/key-admission errors are recorded privately and lock losers preserve
the active owner status. See STATUS for the evidence hash and remaining gates.

## Static-server confinement and temporary storage

The managed and foreground CaddyServer paths now both require Mac Seatbelt;
there is no unconfined runtime fallback. The policy allows only its current public
snapshot, fixed configuration, executable/system runtime and private work area.
It denies private contents, new processes and other-process information. Inbound
binding is limited to localhost8766. A narrowly matched response exception handles
Darwin sendfile without authorizing destination-bearing new connections, including
calls explicitly bound to that source port. Native positive controls cover TCP,
connectx and UDP, file/alias access and fabricated process environments.

Each server receives fresh work under `caddy-runtime/units/ID/work`. Ownership
markers and the exclusive lease remain outside that writable directory. The
native guardian retains the lease while its child inherits only standard IO;
recovery cannot reclaim a still-owned server's output after controller death.
Running scratch is sampled at256 entries,32MiB and depth8. Cleanup walks at most
4096 entries/1GiB/depth16, verifies ownership and unlinks aliases themselves without
following targets. Unknown/incomplete markers and foreign-owned data are preserved
and cause refusal. Treat this directory as disposable server output; store no
personal data there. These are sampled bounds, not a filesystem quota.

Actual private Mac acceptance at7c6d403c passes signed refresh/revocation,
large-body transfer/ranges/HEAD, parent-death cleanup, crash scratch recovery and
managed restart/memory/stall cancellation. Initial stricter rules exposed a
512-byte sendfile failure and were corrected using native destination-denial
controls. All test jobs are unloaded. Public internet timing, production identity,
boot operation and deployment remain separate gates; evidence hashes are in STATUS.
