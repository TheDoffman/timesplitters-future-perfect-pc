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
