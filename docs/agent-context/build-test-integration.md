# Workshop and native test routes

Game link/lease: mod-scripting.md; backups/SDK/recovery: [details](build-test-details.md).
Upgrade admission/recovery: test_upgrade_command.py/test_upgrade_context.py.
Public service acceptance: test_public_service_readiness.py uses real fixture
signatures and local checkpoints, fake HTTP transports/role observations;
test_upgrade_process.py checks the fixed UID600 child and exact bounded reports.
Certificate verification, redirect/size refusal, floor/version reuse and retained
revocations are required. Native CI selects both the helper and its tests.
test_upgrade_coordinator.py composes real journal/files with simulated OS/children:
forward/reverse public restoration, lost observation/settlement and expired resume
must not repeat routing/jobs. Replay mutations reject typed-time/identity/role/
floor changes, unrecorded exposure, missing public evidence and stale settlement.
test_upgrade_command.py checks existing-only authenticated restoration dispatch.
Pending forward configuration/archive/install/boot/completion cases now request
explicit rollback resume: finish only the known forward effect, refresh authority
when needed, then reverse without starting any candidate jobs. Native fixtures
cover intent/result loss and expired proofs for all these boundaries.
test_host_reboot_observation.py uses real protected boot/backup files with fake
launchd and numeric process samples. Mixed verified generations and absent jobs
are observations only; pre-boot PIDs cannot enter its API. Unknown origins,
escaped service processes, file/PID/boot changes and backup rebinding are refused.
The module/tests select native CI; cross-boot journal takeover is not implemented.
test_reboot_observation_lease.py exercises original-history/boot preservation,
global lease exclusion, explicit one-append metadata promotion, refused upgrade
events, changed selection/boot and new-only/existing-only constructor races.
Observation leases do not enable operational cross-boot recovery.
test_upgrade_context.py is registered explicitly for native CI and verifies
post-reboot metadata reads, exact fresh factory matching, missing/foreign context
preservation and refusal of ordinary reconstruction, sealing or directory creation.
Historical observation comparisons use canonical JSON bytes: persisted file
stamps are arrays, while a fresh in-memory protected-file fixture uses tuples.
This normalization changes the test comparison, not the stored metadata contract.
test_reboot_prepare.py composes original metadata with fresh boot/process samples,
selective suspension and ten fixed-label stops. Lost observations resume under a
new lease without duplicate actions; unknown files/orphan workers block cleanup.
Another restart replaces PID authority, preserving the original history. Typed
replay mutations reject changed bindings/order, exposure and ordinary settlement.
test_reboot_layout.py exercises read-only active/archive/install/boot positions,
typed updated health, opaque partial-write preservation and refusal of policy/
backup/process/routing changes. Observation Layout cannot prepare/move/exchange.
checked_monitor_history is shared with ordinary shutdown checkpoint validation.
test_reboot_checkpoint.py composes actual private upgrade files/history with
simulated restart/quiescence. Known complete checkpoint recovery is explicit;
partial/aliased/changed/capacity artifacts stay preserved. Hash/epoch/journal
bindings and refused ordinary effects remain mandatory after checkpoint sealing.
Standalone composed checkpoint tests explicitly import simulated_boot as well as
machine/flow/prepared, so the initial private startup retains its strict boot check.
test_reboot_backup.py checks the preserved runtime binding, explicit fresh reuse/
renewal, lost-receipt recovery, post-child quiescence/checkpoint changes and typed
receipt replay. Reboot backup does not update ordinary frame/migration authority.
Early-reboot checkpoint cases require original policy/backup file identities,
factory markers, typed health, no nonce staging directory and no protected effect
history. Partial/linked staging, wrong store mode, replaced policy, mixed boot,
foreign proof and dedicated-UID orphans stay preserved/refused. The composed
backup fixture uses the new epoch's later quiescence, including unstaged recovery.
test_reboot_admission.py composes the typed fresh-backup/checkpoint handoff with
ordinary forward/rollback/public restoration, historical inventory after moves,
event-time freshness and original/current lease refusal. New boot readiness must
be established afresh; original transaction/directions/catalog floor stay bound.
The shared reboot command fixture validates reboot stop intent before admission;
after admission it delegates to the ordinary shutdown fixture's own intent checks.
Applied-upgrade/reboot/forward/rollback/public settlement exceeds the old256-event
limit;512 events remain bounded by the unchanged128KiB journal cap. The composed
case checks successful complete history, and ordinary flow still tests exhaustion.
Proof metadata rejects malformed/cross-boot capture bindings; a reused old PID
owned by an unrelated user must not become current cleanup authority.
Composed pending cases interrupt migration, archive/install/boot exchanges, proof
retirement, startup, restart and public observation before a simulated reboot;
fresh boot jobs are reconstructed from actual boot files, including known mixes.
Each interrupted phase also reverses directly: new startup intents must select
only the restored old generation, without an intermediate candidate startup.
An observation-only state without an admission field refuses operational boot
and tracking with ModError; it cannot reach an effect through a missing field.
test_upgrade_command covers --recover-boot argument/auth ordering and dispatch
under the actual existing exclusive lease, with phase effects simulated. New and
interrupted/prior-boot admission require preparation; public restore requires
this boot already admitted. Missing/foreign/same-boot history cannot be adopted.
test_upgrade_process requires every reboot module in the sealed inventory.
test_reboot_command uses fresh-bound fixture metadata with an actual preflight
digest and sealed original context, exercising real admission/reconstruction/
coordinator files and public restoration. Its adapter reuses the fixture's held
lease; transport, closure admission and system processes remain simulated.
Unstaged reversal prepares only a manifest, then starts old services directly,
without applying candidate configuration. It still needs fresh AV/readiness and
seven restarts; ordinary unprepared rollback without reboot admission is refused.
test_backup_recipient.py covers independent X25519 identity, canonical origin/
root pin and private/public binding, low-order rejection, pre-KDF size/schema/
profile refusal, tamper/wrong-passphrase failure, and portable recovery. Actual
Windows cases use DPAPI/private NTFS ACLs, preserve existing/partial stores and
refuse a different valid encrypted key or unknown member. Other hosts verify the
portable format and refuse Windows-only import. No production recipient exists;
archive encryption, transport, retention and scheduling are not implemented here.
test_backup_cipher.py checks private new-only file encryption: empty/1MiB edges,
tampered/reordered chunks and encrypted terminal digest, independent receipt
before plaintext and actual consumed-byte hash after decryption, bad headers/
keys/profile, private retained failure output, changing source and disk headroom.
Windows stat/fstat ctime differ (creation versus change time); portable identity/
mtime checks retain content-hash verification. No sender trust is inferred from
recipient encryption. Archive building, export/transport and scheduling remain.
Cipher fixtures canonicalize the host temporary directory before creation;
Mac /var aliases remain refused by production private-path guards.
Workshop: [host, backend, scanner, cleanup and worker checks](build-test-details.md).

Epoch/GCC/dependency reuse: build-test-details.md.
[Sync telemetry](network-sync-telemetry.md).
[Pads](controller-deadzones.md). [IDs](online-player-identities.md). [UI](native-multiplayer-menu.md).
[Actors](network-actor-lifetime.md).
[Rollback](control-presets.md); [Tex](texture-preference.md).
[JIP](arcade-join-in-progress.md); [Routes](online-client-boundary.md).
[Arms/V-Sync](video-options.md).
[Throw](native-uplink.md); [Beams](native-projectiles.md).
[Vietnam](network-performance.md).
[Tools](developer-tools-integration.md).
Warnings: [launcher](launcher.md).
[Save/checksum tests](multiplayer-exit.md#validation).

[Online](multiplayer-ui.md); [sun](fidelity-comparison.md).
[Effect depth](native-effect-depth.md).
[Update tests](main-menu-update.md).
[HUD/MSAA](../development/graphics-options.md).
[SDL/input fixtures](linux-game.md).
[Font/escape tests](audio-options.md).
`test_lfs_checkout.py`: pointer clones, opt-in LFS pull, preserved packs.
[Kill feed](../development/arcade-kill-feed.md)
