# Private workshop backup and recovery

`tools/mods/mod_backup.py` is an operator tool for the Mac/POSIX workshop host.
It does not enable public hosting. `mod_backup_pool.py` adds managed due-time
rotation that a protected scheduler can invoke; installing that scheduler remains
a separate deployment step. Windows players
do not need it. Keep backups outside every served publication directory. They
contain password hashes, account identities, ownership and private moderation
history; use private storage and an independently protected off-device copy.

## Capture

The repository and backup parent must already be owned by the current service
account with mode0700. Paths through links are refused. Choose a new destination:

```sh
python tools/mods/mod_backup.py backup /private/workshop/repository /private/backups/snapshot-001
```

A reserved SQLite writer lock prevents new upload/scan claims and moderation
writes while a second connection captures the committed database, including WAL
pages. An active receiving/processing lease refuses the snapshot before creating
output; quiesce uploads/scanning and retry. Requests may wait or report storage
busy during capture, so schedule a maintenance window for larger repositories.

The snapshot includes the database, every referenced release ZIP (approved,
quarantined, rejected or revoked), and available queued/retry/held ready uploads.
Required missing bytes or a changed digest refuse completion. Operator secrets,
signing keys, scanner databases, temporary partial uploads, unreferenced files,
personal data and public exports are excluded and remain untouched. Keep signing
keys/bootstrap trust and the current publication/revocation journal in separate
protected recovery storage. This snapshot alone cannot reconstruct changes made
after its capture.

Limits are4096 files,256MiB database,256MiB per package and8GiB total. Capture
requires an additional256MiB disk headroom. Larger repositories currently require
an operational archival/capacity plan before using this tool. Files are mode0600,
directories0700. `backup.json` is written last and committed to disk; an incomplete
output is preserved privately for inspection and cannot be restored as complete.
Existing manual backups are never replaced or automatically deleted. Off-device
storage remains a separate responsibility. A new managed pool may rotate only
its own snapshots under the explicit policy below.

## Verify and manage routine captures

Read-only verification checks all file hashes, SQLite schema/integrity and stored
package references without restoring or revoking authentication:

```sh
python tools/mods/mod_backup.py verify /private/backups/snapshot-001
```

It opens only the immutable snapshot database with `mode=ro&immutable=1`, avoiding
WAL/shared-memory sidecars. Live databases always retain ordinary SQLite locking.

Initialize a **new** private managed pool, separate from the live repository and
every served directory. Existing paths/manual backups are never adopted:

```sh
python tools/mods/mod_backup_pool.py init /private/workshop/repository /private/backups/managed --keep 7 --interval 86400 --maximum-bytes 8589934592
python tools/mods/mod_backup_pool.py run /private/backups/managed
```

The policy records the exact repository and a unique pool identity. Invoke `run`
periodically as the same locked service account; it captures only when due.
`--force` requests a fresh capture immediately. A nonblocking lock serializes
runs. Clock rollback, unknown entries, aliases, corrupt metadata and changed
file sizes stop work without replacing existing data. The interval is1 hour to7
days; retention keeps1..32 complete snapshots. Defaults are daily/7 snapshots and
8GiB logical snapshot bytes, including manifests, plus bounded policy/lock files.
Filesystem allocation/headroom is checked separately during capture.

The entire new snapshot must fit the remaining pool budget, including a4MiB
manifest reserve, before it is created. Previous good snapshots are not deleted
to make capture fit. After full verification of the replacement, rotation hashes
each removal candidate and unlinks only its declared fixed-layout files through
directory descriptors; there is no recursive delete or link traversal. The newest
verified snapshot always survives. Manual backups outside the pool stay untouched.
An interrupted complete capture is recognized and finishes rotation on a later
tick. Incomplete or unexpected output stays private for operator inspection and
blocks further captures rather than consuming more space. Exit2 reports failure;
successful JSON distinguishes `captured` from `not-due` and includes the next due
time, retained bytes/count and names removed under the policy.

This source implementation does not install/edit production launchd jobs, upgrade
the protected601/7be runtime, copy backups off-device or alter public routing.
Protected scheduling and off-device retention need their own deployment and
host acceptance; do not point old installed code at a new source configuration.

## Stage a restore

Stop the affected services and restore into a new private sibling, leaving the
original repository and the backup untouched:

```sh
python tools/mods/mod_backup.py restore /private/backups/snapshot-001 /private/workshop/recovered-001
```

Every file is bounded and hash checked. Unknown paths, extra files, links,
hardlinks, executable database schema, corrupt database relations and missing
required packages are refused. Failed partial output retains its recovery barrier.
No ZIP or Lua code is extracted or executed during recovery.

The restored database preserves account identities/roles, password hashes,
ownership, upload records and audit history. All accounts are suspended; sessions
and invitations are deleted, and open upload tickets expire. Previously approved
or pending-review releases require a new scan and review. Existing revocations
remain revoked. The recovery barrier blocks the creator API, approvals, catalog,
package reads and static exports until reconciliation completes.

## Reconcile before resuming

The operator must inspect the latest trusted publication and revocation journal,
including changes since the backup. Do not guess the current origin or version.
Prepare a private JSON file using the verified current values; this is an example
shape, not a production recovery record:

```json
{"schema":1,"origin":"https://workshop.example.test","version_floor":12,"revoked":[],"note":"Reference the latest trusted publication and cumulative revocation review"}
```

`revoked` must include cumulative post-backup revocations, including hashes whose
release records do not exist in the backup. The list supports4000 entries. The
origin must match any saved publication identity; the floor cannot precede the
saved version and must be the latest actual published version (or higher).
The tool cannot infer external publications from an old backup. If that evidence
is unavailable, keep recovery blocked.

```sh
python tools/mods/mod_backup.py resume /private/workshop/recovered-001 /private/recovery/reconciliation.json
```

This records the reconciliation, preserves unknown revoked hashes as tombstones,
updates the publication floor and removes the barrier. Subsequent signed exports
start above that floor and include the tombstones. Reconciled revoked packages
cannot be approved. Keep scan/API workers stopped while restoring account access. Accounts stay
suspended; review current roles/suspensions and
use the existing local `recover-account` invitation workflow individually. Install
fresh verified scanner definitions, rescan/review eligible releases, and verify a
new signed export before changing service configuration to the recovered path.
Old static exports and signing identities are not reactivated by this command.

A disposable command-line restore drill now passes on the actual Mac (see STATUS).
Production service cutover, off-device recovery, supervision,
retention, and service/endpoint cutover acceptance are separate release gates.
The tests use disposable repositories and signing identities, not production data.

## Portable recovery of the signing authority

The Windows root ceremony's `recovery.dpapi` depends on that Windows account's
profile keys. `tools/mods/hosting/portable-workshop-recovery.py` adds a separate
passphrase-encrypted export for losing the original account or computer. This is
an operator tool, not part of a player installation. It contains both encrypted
root keys, their recovery passwords, the online catalog key and exact public
origin/root bytes inside one authenticated encrypted envelope. Never place it on
the public host, in Git, or in a served/download directory. Keep the passphrase
separately from the encrypted file and record the public root SHA256 independently.

Use a new private export directory outside the checkout and cloud-sync folders.
On Windows, create it with `hosting/private-recovery-windows.ps1 -Mode Create`;
on a POSIX verification machine, use a private directory owned by the operator
with mode0700 and file mode0600. Existing files/stores are never replaced. Run
these commands in an interactive local terminal, substituting the actual private
paths and independently recorded pin:

```text
python tools/mods/hosting/portable-workshop-recovery.py export --store ORIGINAL_STORE --file PRIVATE_EXPORT/recovery.bin --trusted-root-sha256 RECORDED_SHA256
python tools/mods/hosting/portable-workshop-recovery.py check --file PRIVATE_EXPORT/recovery.bin --trusted-root-sha256 RECORDED_SHA256
python tools/mods/hosting/portable-workshop-recovery.py import --store NEW_WINDOWS_STORE --file PRIVATE_EXPORT/recovery.bin --trusted-root-sha256 RECORDED_SHA256
```

Export/import use Windows private ACLs and current-account DPAPI. `check` also
runs on macOS/Linux and proves recovery without the original Windows profile.
All operations prompt locally with hidden input; no password argument, pipe or
environment variable is accepted. Export confirms the passphrase and decrypts
its newly created envelope before committing the output. The 16-256 character
passphrase protects AES256-GCM using a random32-byte salt, random12-byte nonce
and fixed scrypt parameters N131072/r8/p1 (about128MiB); untrusted parameters
cannot request more work. Envelope/plaintext budgets are64/32KiB.

Recovery verifies the supplied out-of-band public pin, exact inventory, public
root signatures, both distinct root keys and threshold signatures, online signer
and origin before creating a new store. Private key PEM files remain encrypted;
their passwords exist only in memory or inside DPAPI/the encrypted envelope.
No archive paths are extracted. Errors and proof JSON contain no secret values.
Expired roots may be recovered for a later signed rotation; `root_valid_now:false`
does not authorize serving with expired trust. Import rebinds the recovery capsule
to the new Windows account and preserves incomplete output for inspection on IO
failure. It starts no services and changes no launcher or public routing.

The final portable/scope selection passes154 cases on Windows and153 with1
Windows-only skip on the actual Mac. A disposable encrypted Windows export was
also transferred to and verified on the Mac without Windows DPAPI. Production
export, independent custody of its passphrase, off-device storage and a restore
drill must still be performed before claiming disaster-recovery acceptance.
This export does not include the workshop database/packages or post-backup
revocations; retain and reconcile those using the workflow above.
