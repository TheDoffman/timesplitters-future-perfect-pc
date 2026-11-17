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

Before cutover, the preserving upgrade still needs integrated activation and
acceptance: verify the current sealed generation and exact job/config bindings;
complete and verify a fresh recovery capture; stage and admit the candidate
under UID600; stop the fixed monitors/workers with proven child cleanup; migrate
only the declared generation-bound configuration, markers and job files; then
verify private readiness, scanner controls, persistent jobs and public signed
reads. Preserve the origin/root keys, accounts, uploads, audit, catalog version
floor and revocations throughout. A cutover journal must distinguish safe
rollback from uncertain partial state; never clear state to force admission.

The internal backup/admission phase now binds the independent backup descriptor
and base runtime in the Root journal. Quiescence records its original timestamp;
rechecks preserve that timestamp and independently record their observation time.
A supervised UID600 child must force and independently verify a new snapshot
after shutdown. Its exact receipt is journaled before another live routing/job/
descendant check. Only then may Root issue the fixed0644 migration proof with
the backup manifest hash, boot identity and migration plan. The worker independently
enforces30-second proof and300-second backup freshness. Existing foreign proofs
and interrupted proof writes are preserved and refused. These are internal phases;
the public upgrade command still provides inspection/preflight only.

Preflight obtains a digest-only six-file migration plan from the locked installed
child and binds its hash alongside job and routing observations. Apply/rollback
transport admits only exact fixed helper results with unchanged keys/repository/
services. The migration helper alone uses isolated Python with the verified
runtime's signing packages; metadata/backup helpers retain isolated no-site Python.
Root never reads private configuration payloads through these reports.

The pending backup installation independently binds its sealed code to the old7be
runtime, not to the active host initialization marker. Finish its existing ceremony
first. Preserve that exact backup binding/job and sealed runtime during the host
upgrade. Its frozen code has captured and independently verified two snapshots of
the newer disposable repository schema; this is library compatibility evidence,
not scheduled UID600 admission. The first attempt refused the original fixture's
0755 repository; the successful run uses a separate private copy. Do not weaken
the backup directory checks or change the original evidence.

The scheduled worker respects cadence even for its initialization operation.
A quiescent upgrade needs an explicitly forced and verified fresh capture through
trusted locked-account code, rather than accepting a normal `not-due` response.
Preserving the old backup binding avoids changing its identity-bound status, but
does not waive that capture or subsequent job acceptance. Keep the separate
private443 routing unchanged. No active-generation switch or new administrator
prompt has been performed for this candidate.

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
verified after quiescence. Admission also binds the native boot identity so that
a pre-reboot record cannot authorize migration after boot jobs reload. The
private worker does not create that authority,
stop services, change the backup binding or activate a release. The root
coordinator's internal admission phase exists, but installed acceptance remains open;
this component must not be used as a standalone live upgrade procedure.

`host_release.installed(identity, uid)` verifies the protected installed code
tree independently of the operator's former staging directories. It derives
source/runtime inventories from the descriptor and bound runtime receipt, checks
all bytes/modes/links/directories, the guardian source/image receipt, exact tree
inventory and complete installation/intent receipts bound to revision/UID/path.
It never follows the descriptor's `source_root` or `runtime_root` staging paths
and never reads locked repository/configuration/signing data. Receipt identity
and boolean fields are typed; unknown artifacts, mismatched receipts or escaping
runtime links are preserved and refused. The existing staging-plan verifier uses
the same descriptor/component contract. File reads use nonblocking nofollow
descriptors so special files are rejected without waiting on a FIFO.

`upgrade-installed-host-mac.py` is the upgrade entry point's read-only inspection
phase. With no arguments it returns a plan without installed/private I/O.
`--inspect --from-release <exact-sha>` requires Darwin/root, installed Python
`-I -S -B`, the immutable current entry-point path and verified locked account.
It verifies both installed closures and requires their public origin/root pin to
match. New pending-public-acceptance status is allowed; changing authority is
refused. The resulting report proves installed code/profile compatibility only.
It does not stop jobs, create a journal, suspend routing, force a backup, migrate
private state or activate the candidate. There is no apply action yet, and no
current production ceremony invokes this new entry point.

The read-only `--preflight --from-release <sha> --backup-release <sha>` phase
additionally verifies the preserved eight-file backup closure and its independent
installed runtime. That runtime may be older than the active host. The coordinator
never imports old factory modules in its new interpreter. It invokes the new
immutable `upgrade-job-specs-mac.py` in separate old-bound isolated processes,
after dropping UID/GID/supplementary groups to the locked identity. One child
reads the old host's nine pure job factories; another reads the backup's one
factory. They validate their locked credentials, Python bindings and module
closures before returning bounded typed metadata, with no private configuration,
repository access or service operations. Parent validation binds identity,
retained base, UID/GID/groups, fixed labels and executable paths. The resulting
ten entries feed the protected twenty-plist and double live-job observations.
The preflight report contains a digest and counts, not migration authority.
`prepare_context` retains the verified old/new closures, preserved backup binding
and runtime, operator callable, fixed command transport, old/candidate job entries,
complete migration plan, private baseline, protected-file/process observation and
saved route for later orchestration. It also reads the candidate's nine pure
factories through the candidate-bound locked interpreter and validates their
combined ten-entry set with the unchanged backup. The preflight digest binds
these target specifications as well as the observed running old jobs. Internal
objects/callables are never serialized in the public report. Retaining context
does not authorize effects or replace later per-phase file/process/routing guards;
there is still no apply CLI or reboot-recovery path.

`mod_host_upgrade_process.py` bounds those metadata children to15 seconds/64KiB
stdout and the forced-backup child to1800 seconds/8KiB stdout; stderr is separately
bounded to8KiB and must be empty on success. It uses clean fixed environments,
no stdin or inherited descriptors, and owned Popen handles for termination/reaping
after timeout, excessive output, selector failure or interruption. These trusted
programs do not create descendants. The backup receipt must identify a new
captured/verified snapshot in the preserved binding with creation after root's
quiescence time; not-due, malformed, future or mismatched results are refused.
This transport does not itself attest code inventories: the installed entry point
must do so before choosing these fixed child operations. No apply action or
live upgrade is enabled by the read-only preflight phase.

`force-upgrade-backup-mac.py` is the locked-account capture phase for a future
quiescent upgrade. Its default is plan-only. Active use requires a root-owned
immutable step, isolated `-I -S -B` Mac Python, the locked identity and the exact
preserved backup descriptor/runtime binding. A fresh interpreter imports only the
preserved backup folder's modules; ambient mixed-generation modules are refused.
It checks the fixed repository/pool policy, forces a new capture despite cadence,
independently verifies the exact returned snapshot under the pool lease and
returns a bounded hash/time/identity receipt. It does not overwrite the scheduled
worker's status or change its runtime binding. Failed captures/verification leave
existing and incomplete artifacts for inspection; exception text from private
data does not become the operator report. Root must verify both immutable code
closures before launching this child through a bounded supervisor, after job
quiescence, and check the receipt before issuing migration admission. The internal
capture/admission phase now implements that ordering; installed Root/UID600
acceptance and full service switching remain unfinished.

The forced-backup core tests use `upgrade_backup_driver.py` in fresh `-I -S -B`
interpreters, matching the production closure boundary. A larger pytest process
can legitimately have other test/helper modules loaded; the production guard
must still reject that ambient state. The fresh driver tests actual capture,
policy refusal and verification failure, and deliberately inserts a foreign
module for the rejection case. Do not remove modules from pytest's global cache
or weaken the production closure guard to accommodate in-process fixtures.
The publication configuration test also respects the machine's credentials:
ordinary-user runs exercise the real catalog-key contract; root-run WSL tests
assert the signing layer's mandatory non-root refusal. The production guard is
unchanged. A successful root-run test is not locked UID600 signer admission.

`mod_host_cutover.py` implements job/process attestation for that
coordinator. Its `job_files` checks twenty generated/boot plists against the
immutable factory bytes, requiring root ownership, single-link regular files,
exact modes (host staging0600, backup staging0444, boot0644), safe ancestors and
stable held-file/directory identities. `preflight` repeats those checks around
the complete live observation. The caller must still verify sealed runtime/source
inventories before deriving specifications. The observer binds all ten fixed labels to the exact program
and argument sequence and accepts only the expected protected staging path or
installed boot-file path. Actual current jobs were loaded from staging paths;
requiring only the boot-file path would incorrectly reject them. It observes all
jobs twice around bounded PID/parent/process-group/UID sampling and refuses a
changed binding. Nested coalition fields cannot supply a top-level process ID.
It tracks descendants and groups without command-line/environment inspection;
exit requires those identities/groups and every dedicated UID600 process to be
gone, catching escaped or reparented descendants. Reused identities conservatively
refuse admission; these samples are never signal targets.

`mod_host_cutover_journal.py` v2 holds a nonblocking root-only global lease and
records bounded canonical stop history under root-owned0700 `upgrades`, outside
the monitor directory that an upgrade will replace. It binds before/after,
plan/preflight hashes, preserved backup descriptor/base runtime, UID, the original numeric PID/group snapshot, saved public
routing baseline, nonce and native boot session. The earlier undeployed v1
format is preserved/refused rather than silently adopted. Every stop intent
precedes its observation; four monitors must precede backup and the remaining
workers. A pending intent survives reopening and cannot be counted as a stop.
Unsettled prior transactions block new nonces; settled history remains bounded
to32 transactions. A caller records settlement only after verified restoration,
final activation, or an unused preparation. This storage primitive does not prove
that those actions occurred. Replaced leases/directories, malformed/foreign or
noncanonical histories, wrong boot identities and unknown/partial pending writes
are preserved and refused. Explicit `recover_pending=True` may promote only one
complete canonical append to the exact existing history/base in the same boot,
under the same global lease. It rechecks both files before replacement. Truncated,
foreign, duplicate or multi-event pending files stay untouched; no new nonce or
automatic cleanup bypasses them. Promotion recovers metadata, not an OS outcome.
This journal stores the coordinator's observations; it does not perform them or
issue migration authority. No deployed job uses it yet.

`mod_host_upgrade_layout.py` stages the new nine plists and four monitor markers
in Root-only `upgrade-layouts/<nonce>` (at most32 transactions), separate from the
global cutover journal's exact inventory. It attests the old factory-derived files and
marker generations first, bounds Root-owned history to128 records/1MiB/four levels,
and pins both directory inventories and the unchanged independent backup files.
The old monitor directory, including deployment and health history, is archived
whole. No old health record is copied into the fresh monitor directory.

Its internal exchange primitive announces intent before each of eleven effects:
archive/install monitor directories and replace nine boot files. Explicit resume
can observe a pending completed effect without repeating it. Reverse exchange
restores the original directory identity/history and old boot bytes. Unknown,
replaced or duplicate generations, mixed boot files without explicit recovery,
foreign manifests and partial temporary writes are preserved/refused. A live gate
rechecks suspended routing and all ten absent labels around numeric descendant/
group/UID absence; it performs no job stop or startup. Backup bytes and identities
must remain unchanged. The caller must hold the Root journal lease, verify sealed
factories/configuration migration, bind the prepared manifest digest and durably
record every effect. Full activation integration, post-activation rollback
and production admission remain open; no CLI exposes this phase yet. Reopening a
prepared layout requires its exact journal-bound digest and same boot identity.

The journal now records the prepared layout digest, private migration intent/result,
each fixed filesystem intent/observation and the complete layout result. It models
the monitor location and all nine boot-file generations; foreign/out-of-order or
duplicate effects are refused. `configuration_and_exchange` applies private
configuration before switching files, and restores files before rolling back
configuration. A lost private-child result retains intent and needs explicit
resume through the private migration's existing known-state journal. Partial
file effects retain their exact pending operation. Public access/restoration and
settlement must match the complete configuration/layout generation. Suspension
can still contain re-exposure while a protected effect is pending.

The original private file-flow bound was128 events/64KiB per transaction; a complete
apply/reverse/restoration exceeds the former64-event limit. Known proof refresh
is the sole exception to monitor inventory identity stability: a Root0644 proof
must match this transaction's recorded backup hash/time, shutdown, boot, plan and
nonce. Active and archived proofs are checked identically. This allows explicit
fresh recapture after proof expiry while preserving all other recorded files.
The locked worker still independently enforces its short admission lifetime.
Activation, post-start health-history checkpointing, reboot recovery and installed
acceptance remain unfinished; these internal phases are not a live upgrade CLI.

`mod_host_upgrade_activation.py` implements private startup only. Under the global
Root journal lease it retires the fixed active migration proof before any bootstrap.
Intent binds its hash or verified absence; existing authority moves into the
Root700 layout folder and becomes0600. Explicit resume recognizes only that
pending archive, including a crash between rename and permission tightening.
Foreign destinations or changed proofs are preserved/refused. Missing original
monitor proofs are recognized only through their retained, recorded archive.

The fixed installed AV helper runs as the locked identity under isolated Python
with the verified runtime's packages. Its900-second/8KiB transport returns exact
clean/EICAR/error-control results and removed-fixture status. The parent verifies
that helper/scanner descendants have exited or belong only to recorded job trees;
escaped dedicated-UID processes block further startup. A successful report is
journaled and must be fresh within300 seconds for every new startup intent.

Four monitors precede the unchanged backup job and remaining five workers. Each
bootstrap has a durable intent before the one fixed launchctl command; actual
loaded binding, rather than command exit status, governs observation. Explicit
resume can observe a previously loaded pending job without repeating bootstrap.
A pending absent/uncertain outcome is preserved for separate recovery. Already
recorded jobs, fixed policy identities and suspended routing are rechecked around
numeric process-tree observations. The four fixed monitor directories and13
policy files stay pinned while separate health metadata updates. No sampled PID
is signalled. The journal invalidates quiescence at the first startup intent and
blocks private-access/public-restoration/settlement after startup until admission
has verified fresh readiness and all seven restart checks.
This phase returns readiness_verified=false: private health/scanner readiness,
seven restart checks, controlled stop/rollback after startup, health-history
checkpointing, reboot recovery and final public admission remain unfinished.
There is still no installed apply CLI and no production job uses this phase yet.

`private_readiness` supervises the existing installed loopback/signature checker
as UID600 under isolated verified Python, with a110-second/8KiB budget. Its exact
report binds release/UID, origin/root, role health, catalog version and private
transport. The read-only preflight includes the old generation's verified report
in its digest and returns it as `private_baseline`. A coordinator must record
`baseline-private-verified` before shutdown; startup requires this baseline.

`admit_private_services` checks new readiness against that authority and a
monotonically increasing catalog floor. Three long-lived host workers then four
monitors each require a durable restart intent, one fixed-label launchctl SIGKILL,
a new PID and exit of the observed old tree/groups. The parent never signals a
sampled PID. Nonzero/timeout results are resolved by actual job/process observation;
an unresolved outcome stays pending. Explicit resume observes that same restart
without reissuing the signal. Every completed restart requires another fresh
signed loopback/role-health check. AV controls renew when expired.

Only a fresh report after all seven checks admits the exact private-access digest.
The selective routing primitive rechecks both AV/private freshness and the digest
at restoration intent and observation, preventing stale admission or changed
evidence from enabling public service. This function does not change routing.
Installed post-start shutdown/rollback acceptance, reboot recovery, the full
installed apply CLI and production acceptance remain open.

`mod_host_upgrade_shutdown.py` now implements controlled post-start shutdown and
monitor-history checkpointing. It snapshots current fixed labels/process trees
and retains any unresolved restart tree, then stops monitors before backup and
workers. Each bootout has durable intent and observed exact native absence.
Explicit resume observes a pending stop without repeating bootout. Changed PIDs,
bindings, reappearing labels and uncertainty preserve state; all tracked groups
and every dedicated-UID process must exit before shutdown completion. Unrecorded
UID processes prevent quiescence after the known jobs are safely stopped. A
pending startup that never loaded can be cancelled through this observed shutdown.

After shutdown, a new Root-only checkpoint binds the active monitor inventory to
the original layout digest. Static policy identities stay fixed; only typed
generation-bound health reports may change. The exact Root atomic-write namespaces
`.scan-<32hex>` and `<role>/.health-<32hex>` may remain as bounded opaque evidence
after a killed writer; those files are preserved without loading/deleting their
partial bytes. Other unknown/linked/changed history is refused. Reverse exchange
recognizes only the checkpoint hash recorded in the global Root journal and
archives the entire new directory before restoring the old one. Fresh backup/
proof after shutdown binds its new quiescence time; expired authority in the
restored old directory needs explicit fresh capture before private rollback.

Catalog floor is retained across shutdown/reverse/restart. The old generation
must again retire proof, pass AV, load fixed jobs, complete readiness/seven restarts
and pass public-admission freshness gates. The full history now allows256 events/
128KiB per transaction, at most32 transactions; a complete post-start reversal
exceeds128 events. These internal phases still require installed acceptance,
reboot recovery and full apply-CLI integration before live deployment.

The v2 routing callback is directly compatible with the selective transition
primitive. It persists intent before the action, permits only matching observed
results, and makes already-recorded recovery observations inert. Stops require a
recorded suspension; `quiesce(..., routing=<operator transport>)` also verifies
current routing before the stop sequence, before every next stop and at final
quiescence. Re-exposure halts progress. An explicit re-suspension is possible but
invalidates earlier private admission. Restoring public access requires a fresh
root-recorded private-access report digest for the before/after generation; the
after generation additionally requires all old stops observed. The report is
valid for300 seconds at intent and observation. Pending restore recovery may
refresh that verification. The coordinator must produce the actual verified
report; recording its digest is not itself proof. Settlement requires no pending
intent and confirmed public restoration (or an unused/cancelled preparation).
Cancelling an unapplied suspension requires the exact original public state and
no observed service stops. No CLI currently invokes this mutation pipeline.

The root-only `quiesce` primitive attests those job files, checks the journal's
UID/boot and addresses only the fixed `system/<label>` targets. It durably writes
each intent before `launchctl bootout`, then observes that same label. Neither
exit0, a nonzero exit nor a timeout establishes successful shutdown; absence
requires the exact native exit113/missing-label response. This response was
observed read-only on the user's Mac. Other observation errors preserve the
pending intent. It does not reissue bootout while polling an uncertain outcome.
Resume rechecks the known label binding; a changed PID outside the original
snapshot or a reappearing stopped label requires explicit recovery. Once all ten
labels are absent it checks numeric descendant/group exit and absence of all
UID600 processes, rechecks files and label absence, then records quiescence.
This primitive has no CLI and has not stopped production jobs. The root entry
point must still verify sealed code and suspend ingress before invoking it;
the internal forced-backup/proof phase is available, while integrated configuration/
monitor migration, rollback, private scanner/readiness/restart acceptance and
public restoration remain open.

The current public8443 route is Funnel-enabled HTTPS proxying127.0.0.1:8768;
private443 exists with Funnel disabled. Thus starting new private gateway jobs
while that route remains enabled exposes them publicly. The coordinator must
suspend only8443 before admission and restore it after acceptance, verifying the
unrelated configuration unchanged. A read-only current routing observation binds
the unrelated canonical configuration SHA256
`58f73772bcfc9eb3aa55d2ca29d970044a0194054b8ed3764e6258db06edd0a6`;
this is an observation, not a completed routing cutover or future expected pin.

For the pending route phase, the [official Funnel CLI reference](https://tailscale.com/docs/reference/tailscale-cli/funnel)
requires the original configuration flags when appending `off`; the target can
be omitted. Always select8443 explicitly and preserve its background mode. A
global reset is unsuitable because this Mac also serves unrelated private443.
The coordinator must verify the actual post-command configuration and unrelated
state, rather than treating an exit code as routing proof. No off/on command has
been executed for the candidate; exact selective suspension/restoration remains
an implementation and acceptance gate.

`mod_host_routing.py` implements that selective state contract. It accepts the
fixed8443 HTTPS proxy only, distinguishes public/private/absent states, refuses
foreground/partial/aliased or unknown managed routes and public443 exposure,
and hashes unrelated configuration without logging its private handler contents.
Empty/omitted/null known maps normalize identically; all other unrelated values
remain exact. Preflight reads status through the established operator and binds
both full and unrelated fingerprints into its digest. No routing mutation is
part of preflight.

The transition primitive requires a saved public baseline and the coordinator's
durable journal callback. Intent must succeed before one fixed off/on action;
success is established by the subsequent status, including unchanged unrelated
configuration. Timeouts/nonzero results are not retried or treated as absence.
Already-changed state needs explicit resume; unknown changes remain preserved.
Restore requires the exact original public configuration. The coordinator must
first prove private admission before requesting restore; this module does not
grant that authority. Its app transport drops to the verified original operator
UID and actual primary GID with empty supplementary groups and a clean environment
before executing the operator-writable app. It never runs that app as root or
uses reset/default443. Owner changes require a separate migration contract.
No CLI exposes these mutations; durable root routing events, recovery and actual
selective off/on acceptance still must be integrated before live activation.

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
