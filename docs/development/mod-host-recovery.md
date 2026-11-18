# Private workshop backup and recovery

`tools/mods/mod_backup.py` is an operator tool for the Mac/POSIX workshop host.
It does not enable public hosting. `mod_backup_pool.py` adds managed due-time
rotation that a protected scheduler can invoke; installing that scheduler remains
a separate deployment step. Windows players
do not need it. Keep backups outside every served publication directory. They
contain password hashes, account identities, ownership and private moderation
history; use private storage and an independently protected off-device copy.

## Off-device recipient custody foundation

`mod_backup_recipient` implements the independent recipient key needed before
encrypted off-device copies can be automated. This is an internal foundation,
not a deployed exporter, transport or scheduled copy. No production recipient
has been created. The existing signing-key portable recovery file is unchanged.

The recipient is X25519, separately generated from every RSA signing key. Its
public record binds the canonical HTTPS workshop origin, independently supplied
root SHA256, algorithm, raw public key and domain-separated key ID. Low-order
public keys, foreign profiles and mismatched private/public material are refused.
Only that public record is intended for the host; the private key must stay off
the host. Passing a signing private key as a recipient is rejected.

Windows creation/import uses a new private NTFS directory and account-scoped
DPAPI through the existing recovery ACL helper. Private bytes are never written
unencrypted. The public completion record is written last, and exact two-member
inventory is required when loading. Existing, partial or foreign stores remain
preserved; no adoption/repair or replacement occurs. Choose private local storage
outside cloud-sync folders when this component is integrated into operator tools.
Keep the portable envelope outside this exact two-file account store.

Portable recipient recovery uses a separate, bounded canonical envelope with
scrypt-n131072-r8-p1 and AES-256-GCM, fresh32-byte salt/12-byte nonce, and the full
public recipient as authenticated associated data. It is not interchangeable
with signing-authority recovery. Expected origin/root pin, format and size checks
precede the KDF. Wrong passphrases, tampering and changed public identity refuse
recovery. Secret payloads are bounded to4KiB and envelopes to8KiB. Private keys
exist only in memory, DPAPI ciphertext or this authenticated encrypted envelope.

Actual encrypted repository archive creation, host recipient provisioning, SSH
or other authenticated transfer, receiver retention, unattended scheduling,
production key custody and an off-device restore drill still need integration
and deployment. Component checks do not establish automatic backup operation.

## Streaming snapshot archives

`mod_backup_archive.encrypt_snapshot` connects verified POSIX snapshots to the
recipient file cipher. It first verifies SQLite/schema/references and every
snapshot file, then hashes the deterministic archive stream and encrypts a fresh
stream while rechecking every file and inventory. Bounded reads avoid creating
a second plaintext archive on the host. It returns private local metadata only;
transport, authenticated receiver receipts and scheduling remain separate work.

The framing is fixed magic `TSFPBA1\n`, a big-endian32-bit canonical JSON manifest
length (maximum1MiB), the strict existing snapshot manifest, and concatenated
payloads in sorted inventory order. There are no archive-provided file headers,
permissions, links or executable hooks. The existing4096-member/8GiB payload and
per-file ceilings remain; the encryption ceiling includes the bounded manifest
and12-byte framing overhead so a maximum-size snapshot remains representable.

On a POSIX recovery host, first call `decrypt_file` with the independently
authenticated ciphertext SHA. Pass its returned private plaintext SHA to
`unpack_snapshot`; never take this digest from an untrusted archive. Extraction
checks hash/framing/inventory/size before creating a new0700 directory, refuses
all existing destinations, checks2GiB headroom, writes only fixed allowed names
as0600 files, verifies each hash and rechecks the actual consumed archive digest.
The manifest is written after all members verify. Partial output is preserved
and must not be adopted. Full snapshot/database verification is still required
before success. This is a private snapshot, not a serving repository: the normal
`mod_backup.restore` authentication reset and publication-reconciliation barrier
remain mandatory. Windows retains ciphertext; extraction is POSIX host recovery.

`test_backup_archive.py` checks a real SQLite/blob snapshot through encryption,
decryption, extraction and conservative restore, plus traversal, duplicate JSON,
noncanonical/oversized metadata, truncation/trailing bytes, wrong receipts,
corrupt members, inventory mutation, existing targets and low-space refusal.
These are component checks, not production transfer/deployment acceptance.

## Recipient-encrypted file staging

`mod_backup_cipher.encrypt_file` and `decrypt_file` implement the bounded file
layer for future off-device archives. They do not build/extract an archive,
provision a host, transfer files or schedule copies. All paths must be private
and owned by the caller (private NTFS ACLs on Windows,0700 parent/0600-compatible
files on POSIX). Outputs are new-only. Linked/hardlinked/nonregular files,
unexpected identities and insufficient expected-output space plus2GiB headroom
are refused. Failure keeps private partial output for inspection; its existence
is not completion and it must never be restored or adopted automatically.

The version1 framing uses a canonical4KiB-bounded header with recipient/profile,
plaintext length (8GiB payload plus1MiB+12 bytes archive framing), ephemeral public key,32-byte salt and8-byte nonce
prefix. A fresh X25519 exchange feeds HKDF-SHA256 with a file-specific header
binding. Each at-most1MiB AES-256-GCM chunk uses the prefix plus a unique32-bit
big-endian counter; associated data binds domain/header/index/plain chunk length.
The next counter authenticates an encrypted32-byte plaintext digest as a mandatory
48-byte terminal frame, with a zero-length discriminator. Plaintext checksum is
not exposed in the public header. Exact file size/terminal/trailing-byte checks
refuse truncation or framing changes. See the primary [X25519/HKDF guidance](https://cryptography.io/en/stable/hazmat/primitives/asymmetric/x25519/)
and [AEAD nonce/tag requirements](https://cryptography.io/en/46.0.0/hazmat/primitives/aead/)
for the primitives; this application framing is described here.

Encryption requires the already-verified plaintext size and SHA256 and rechecks
content while reading. **Encryption does not authenticate who sent a backup:**
any holder of the public recipient can create ciphertext. Decryption therefore
requires a ciphertext SHA256 obtained independently through the future trusted
transfer channel, not one supplied by an arbitrary encrypted file. That digest
is checked before creating plaintext, and checked again over the actual bytes
consumed during decryption to reject a valid-file swap after the first hash pass.
Every chunk and the terminal digest must authenticate before success is returned.
Private staged prefixes may remain after a late failure; no success receipt is
returned. Returned plaintext hashes are local private metadata and must not be
copied into a public transfer manifest.

Windows path stat and held-file fstat report different ctime meanings; identity,
size and mtime checks are used there, with the same mandatory content digests.
POSIX also retains ctime checks. New-only private staging and the normal snapshot
verification/recovery barrier remain required when archive integration is added.
This layer has not been deployed and does not establish automated off-device
backups or a production restore drill.

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

## Backup-only protected worker source

`mod_backup_release.py` prepares an eight-file content-addressed bundle from a
clean committed checkout. Each file must equal its Git blob; the descriptor
binds the source revision, operator UID, payload hashes and an existing protected
host release. `hosting/prepare-backups-mac.py` runs as the ordinary Mac user and
creates a new bundle only. It does not install or start anything. Preparation and
installed-code validation reject linked ancestors/files, hard links, unsafe
ownership, writable shared paths, unexpected entries and mismatched hashes.
Privileged reads pin directory descriptors while walking each ancestor.

The proposed installed layout is `backup-releases/DESCRIPTOR_SHA256` beneath the
protected workshop root. A separate root-owned binding fixes the service UID/GID
and canonical Python inside the existing immutable host runtime. The fixed
`hosting/backup-bootstrap-mac.py` validates this inventory and reuses that runtime's
existing locked-service bootstrap to clear groups and irreversibly drop root
before executing `mod_backup_service.py`. It supplies no arbitrary executable,
identity, repository or command override. Existing API/scanner releases stay intact.

The worker uses only `data/scan/repository` and `data/backups/managed`, with seven
daily captures and the existing8GiB logical pool budget. It supervises one trusted
capture child in the same launchd process group, limits each output stream to8KiB
and wall time to30minutes, then terminates/reaps the child on failure. Child limits
are600 CPU seconds,1GiB data,64 descriptors and no core dumps. It never executes
uploaded archives. Private bounded atomic status preserves malformed, foreign or
differently bound previous state instead of overwriting it. The proposed launchd
plist runs hourly and at load; due-time policy determines whether capture is needed.

`tests/mods/test_backup_service.py` checks descriptor/runtime/code bindings,
committed preparation, refusal/preservation paths and real POSIX timeout/output
cleanup. These source checks do not prove protected installation or persistence.
`hosting/install-backups-mac.py --bundle PRIVATE_BUNDLE --sha256 RECORDED_SHA256`
prints an unprivileged plan. Adding `--apply` requires local Mac administrator
authentication and the matching preparing operator UID. It verifies the existing
sealed host/runtime/locked identity, exclusively copies the eight hashed files
and binding into a new root-owned read-only release, then re-execs its protected
`mod_backup_install.py` using that runtime. Existing unknown/partial code is
preserved; only an exact completed matching release can be reused.

The registrar checks that the fixed backup label/startup filename are absent,
creates the new UID-owned private parent and admits the privilege-drop probe,
initial capture and independent read-only verification. It then runs the hourly
job from protected staging twice, checks successful exit/status and only then
creates its startup plist and tests that persistent copy. A private root-owned
journal records admission and rollback. Failure unloads only the known job whose
reported path still matches, and removes only its newly created boot file when
inode/content still match (including a partial write). Replaced/unknown artifacts
and all captured backup data are preserved. Existing API/scanner jobs and routing
are never selected. `test_backup_install.py` exercises the new-only code and job
transactions with real POSIX files and simulated launchd outcomes; actual Mac
protected installation and startup acceptance remain separate required checks.
This does not supply off-device copies.

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
