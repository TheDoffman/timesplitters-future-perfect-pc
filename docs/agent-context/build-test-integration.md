# Workshop and native test routes

Game link/lease: mod-scripting.md; backups/SDK/recovery: [details](build-test-details.md).
Upgrade admission/recovery: test_upgrade_command.py/test_upgrade_context.py.
Public service acceptance: test_public_service_readiness.py uses real fixture
signatures and local checkpoints, fake HTTP transports/role observations;
test_upgrade_process.py checks the fixed UID600 child and exact bounded reports.
Certificate verification, redirect/size refusal, floor/version reuse and retained
revocations are required. Native CI selects both the helper and its tests.
Workshop: [host, backend, scanner, cleanup and worker checks](build-test-details.md).

Epoch/GCC/dependency reuse: build-test-details.md.
[Sync telemetry](network-sync-telemetry.md).
[Pads](controller-deadzones.md). [IDs](online-player-identities.md). [UI](native-multiplayer-menu.md).
[Actors](network-actor-lifetime.md).
[Rollback](control-presets.md); [Tex](texture-preference.md).
[JIP](arcade-join-in-progress.md); [Routes](online-client-boundary.md).
[Arms](video-options.md).
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
