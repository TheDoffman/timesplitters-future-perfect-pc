# Operating the Mac mini workshop

This is the operating checklist for the deployed workshop. New-host development
commands in [Mac hosting](mac-mod-hosting.md) are not upgrades for the live host.
The [completion table](../project/mod-workshop-acceptance.md) records remaining
release gates; this checklist does not establish that those gates have passed.

## Installed service and custody

The public origin is `https://dans-mac-mini.tail7e8ab8.ts.net:8443`. It serves
approved snapshots and the authenticated creator/moderator API through the
confined gateway. The separate private443 mapping is unrelated and must remain
private. Keep the Mac powered and connected. Unattended operation after a physical
boot still needs acceptance; do not infer it from a process restart test.

Protected files live under `/Library/Application Support/TSFP Mod Workshop`.
The locked `_tsfpworkshop` UID/GID600 reads the private repository at
`data/scan/repository`. It cannot log in interactively. Root-owned sealed code
and fixed launchd bootstraps drop privilege before workers read private data.
Do not run an ordinary development API, scanner or publisher against that live
repository, change its ownership, grant SSH access to the service identity, or
replace a protected runtime with files from a checkout.

Nine existing system jobs supervise scanning/definitions, API, publication and
gateway operation. The additional backup job is **prepared, not yet admitted**
as of the current STATUS record. Use the recorded immutable release identities
in [STATUS](../project/STATUS.md) when assessing an upgrade. A source commit or
passing unit test does not mean the installed service changed.

The workshop owner password, Mac administrator password and recovery passphrase
are separate credentials. Keep them in their respective local prompts/password
manager. Invitations and session credentials belong in private operator storage,
never a mod ZIP, source commit, screenshot or support log. The portable encrypted
signing-key recovery file has passed the owner's Mac check; keep its passphrase
separately from the file. It does not back up the repository.

## Upgrading the deployed generation

The current installer can stage a new immutable release alongside the running
one. It does not switch active services. The initializer requires its exact
generation's configuration/marker, and the job registrar refuses existing system
job files and labels. Re-running those new-host commands against the deployed
generation is not a preserving upgrade.

Source4fc86283 is frozen in candidate descriptor
`14d9a007d249a52005cdf07bd3b2f5957a785b42fc11e7f7d3af56d63d7d515c`.
Its plan and actual disposable frozen-source/runtime acceptance pass on the Mac:
creator-only invitations, the SDK audio package, fresh isolated ClamAV clean/
EICAR results, exact PCM inspection, manual approval, report closure, revocation,
and empty-database/missing-engine refusal. These use UID501, private fixture data
and in-memory WSGI requests. They do not establish UID600 admission, TLS/public
acceptance, an installed release or active-generation migration. Exact receipts
and pins are in STATUS. Do not regenerate this frozen candidate merely because
later documentation or client commits change HEAD.

Before cutover, the preserving migration still needs to be implemented and
accepted: verify the current sealed generation and exact job/config bindings;
complete and verify a fresh recovery capture; stage and admit the candidate
under UID600; stop the fixed monitors/workers with proven child cleanup; migrate
only the declared generation-bound configuration, markers and job files; then
verify private readiness, scanner controls, persistent jobs and public signed
reads. Preserve the origin/root keys, accounts, uploads, audit, catalog version
floor and revocations throughout. A cutover journal must distinguish safe
rollback from uncertain partial state; never clear state to force admission.

The pending backup installation is bound to the old7be generation. Finish its
existing ceremony first, then explicitly account for that backup job/binding in
the upgrade. Installing a new host generation does not automatically migrate the
backup job. Keep the separate private443 routing unchanged. No active-generation
switch or new administrator prompt has been performed for this candidate.

The new private migration component (`mod_host_migration.py` and the installed
`migrate-private-generation-mac.py` step) implements only the locked-account
configuration phase. It derives the six allowed records from the v1 initializers
and stores a bounded private before/after journal. It supports explicit resume
or rollback only while all current bytes belong to those known generations.
Unknown files, links, partial journals or concurrent changes are preserved.
Signing files, API configuration and repository data are read/guarded or left
alone; they are not migration outputs. Root-update files require a separate
explicit migration contract.

Its default command is plan-only. Actual migration requires an immutable installed
step, locked UID/groups and a fresh root-owned `monitor/upgrade-quiescence.json`
binding the exact plan/nonce, all nine host jobs plus backup stopped, and a backup
verified after quiescence. The private worker does not create that authority,
stop services, migrate the backup binding or activate a release. The root
coordinator producing and validating that admission remains to be implemented;
this component must not be used as a standalone live upgrade procedure.

## Routine checks

At the start of a moderation session, and after a service or host restart:

1. Refresh **Mods** in a current native launcher. It must accept the pinned
   catalog with normal HTTPS verification. A browser showing a200 response alone
   does not verify the catalog authority, expiry or rollback protection.
2. Check free space on the Mac (`df -h /` is read-only). Preserve the game's
   originals, mods, saves, workshop folders and last good backups when freeing
   space. Do not clear quarantine or an incomplete snapshot to hide a failure.
3. Check the fixed workshop jobs in launchd. Their startup files are under
   `/Library/LaunchDaemons/org.timesplitters.workshop.*.plist`; copy the exact
   label from the relevant plist and use `sudo launchctl print system/LABEL`
   locally. Inspect only the workshop jobs. A one-shot job can be idle normally;
   check its last exit result and the relevant worker health/status as well.
4. Confirm scanner definitions are current and the scanner is healthy before
   approving releases. Stale/missing definitions and scanner failures must leave
   content unavailable for approval. Follow [scanner operation](mod-scan-service.md)
   rather than disabling its checks to clear a queue.
5. Once the backup job has passed protected admission, inspect its status and
   latest verified capture as described in [recovery](mod-host-recovery.md).
   A loaded label alone is not a successful backup. Check failure status before
   assuming an off-device copy contains a recent capture.

No automatic operator notification delivery is established by this checklist.
These checks require an operator; unattended alerting and incident acceptance
remain in the full completion table.

## Review, reports and withdrawal

Use the authenticated native Creator/Moderator screens and the
[creator service workflow](mod-creator-service.md). Maintain separate creator
ownership; do not publish another person's upload under the owner account to
bypass ownership or limits. Review the requested content, scan evidence and Lua
source before approval. A clean antivirus result is one check, not proof that
an addition behaves as intended. Native archive validation and script confinement
remain enforced independently.

Read player reports in the moderator queue, inspect the release and retain its
audit history. Close a report after recording the investigation's outcome; reopen
it if the issue remains. Do not delete private database rows to clear a report or
remove approval history. An account suspension and a package withdrawal are
different actions: verify the affected package is actually revoked when content
must stop being distributed.

For a harmful or invalid approved package, withdraw it through the authenticated
moderator workflow. The live publication controller creates a new signed snapshot;
the gateway must stop offering its approved download. Verify from outside the
home network that the new signed catalog retains the revocation and that
`/v1/packages/SHA256` returns404. Refresh an affected launcher and confirm active
modded launch is blocked; **Play without mods** remains the recovery path.
Previously downloaded bytes cannot be recalled, so catalog enforcement matters.

If publication or withdrawal does not become effective, treat it as an incident.
Keep the package unapproved, retain the audit/evidence and suspend only the public
8443 exposure if needed through the established Tailscale configuration. Do not
alter the separate private443 mapping or serve an old snapshot at another path.
Do not manually edit a signed catalog, replace the trusted root to suppress an
error, or reapprove withdrawn content just to make a test pass.

## Backups, capacity and recovery

The prepared managed policy captures daily, checks due time hourly and keeps up
to seven complete snapshots within an8GiB combined logical budget. It also needs
filesystem headroom. Seven snapshots are not guaranteed if they do not fit: a
new complete verified capture must fit before any old good capture is removed.
Capacity exhaustion preserves previous backups and reports an error. Plan
archival/capacity changes before growing the live repository to that boundary.
Unknown, partial or corrupt output stays private for inspection and stops growth.

Repository snapshots include password hashes, ownership and moderation history.
Keep them outside every served directory and protect off-device copies. The
existing Windows copy/return and unserved restore drill passed; **automated
off-device copies are not yet deployed**. A second folder on the same Mac is
not off-device custody. Check the copy's integrity and recoverability before
relying on it, and retain cumulative revocations/publication high-water evidence
that may be newer than a captured database.

Use [the recovery procedure](mod-host-recovery.md) to restore into a new private
directory. Keep its recovery barrier, revoked sessions, suspended accounts and
withheld approvals until explicit reconciliation. Do not point production at a
restored copy merely because SQLite opens. Production cutover needs current
trust/revocation reconciliation, fresh scan evidence and separate acceptance;
the completed clone drill did not perform that cutover.
