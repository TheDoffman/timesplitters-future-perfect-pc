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
