# Build-Test guide continuation

[Main guide](build-test.md).

## Game release channels and maintenance publication (2026-10-22)

`tools/release/release_channels.py` owns canonical X.Y.Z[-channel.N] package identity and
the minimum launcher 1.3.0. `tools/release/release-version.py` writes an explicitly chosen
VERSION only; no automatic increment/tag/publication. The next Stable source
version is 0.3.2. Both game packagers validate identity before staging and emit
release_channel in BUILD.json. Official packages continue optional texture delivery.

`publish-release.py` defaults to exact origin/main; --branch permits pushed feature
previews and Stable release/ or hotfix/ branches with a published Stable ancestor.
It validates monotonic versions within stable major/minor lines or exact preview
base/channel series. Notes choose an ancestor in that series, falling back to
Stable; unrelated/nonancestor experiments never become a baseline. Channel JSON
records exact source and included/latest published Stable; a missing latest
Stable is disclosed. --notes-file supplies reviewed feature notes for unmerged
branches. Prepared package reuse includes the channel metadata; publication
retains latest=false and existing package/installer/hash/rollback checks.

The release CI scope selects release_channels.py and release-version.py and
existing test_publish_release.py covers their identity, preparation, branch,
hotfix, unrelated-baseline and prepared-retry contracts using fake GitHub.
LauncherSharedUiChecks covers shared channel selection/preferences/metadata;
launcher_versions.cs covers paginated discovery. Run the existing Windows launcher
suite and both-host CI. See docs/project/STATUS.md for measured local results; no runtime game
behavior or generated guest code changed, and no game build/publication is
established by these asset-free checks.

Grouped Preview UI: GameChannelPickerPage.cs uses both hosts' *Page.cs includes.
tests/launcher/launcher_shared_ui.cs covers tracks, Save state and launcher_saves.cs
(sparse backups, update protection, restore failures). Run
tools/build/build-launcher.ps1 -Test. --render-preview uses synthetic tracks.
No game build or publisher/tag-format change is required for this grouping.

Local Windows artwork selection: build-launcher.ps1 resolves explicit image
arguments first, then validates each build/launcher-art PNG against the shared
artwork pin, then uses the existing private legacy fallback if no cached file
exists. fetch-launcher-art.ps1 recovers the pinned banner/backdrop/logo without
executing downloaded code. -RequireArtwork checks the selected resources; normal
local rebuilds now reuse the same verified art as release CI when cached.

Beta publication (2026-10-23): release_channels.identity defaults to history parsing;
publishing=True rejects alpha/preview/rc/legacy suffixes. Version preparation,
both package entry points and publisher preflight enforce that mode. Beta's
canonical identity is beta/Beta; a custom Beta display name is rejected. Historical
identities remain readable for baselines without joining Beta's series. Publisher
fixtures now use beta.N, with explicit legacy rejection/history and independent
Beta baseline tests. Existing generic semantic-version ordering tests still cover
alpha/beta ordering separately from the new publication policy.

## Temporal Uplink regression route

`tests/support/rhi_screen_copy.h` exercises the production GPU screen-copy pass by
copying a saved texture region after clearing the source scene. Run
`python -m pytest tests/graphics/test_rhi_menu_sampling.py -q` on Windows. The shared
runtime patch stack is checked by `tests/repository/test_patch_updates.py`; a native GL
build and live Scotland the Brave Uplink view remain separate platform and
visual validation routes. Replay captures alone cannot verify this effect
because the host copies are not serialized into replay frames.

## Water and impact-spark checks

See [water tests](water-tests.md) for CPU/GPU validation and live limits; see
[spark timing](native-impact-sparks.md) for the original guest regression route.

CI discovers unittest in each owning test folder. Changes to project_paths.py
or conftest.py select all runtime and launcher suites.

[Request CI](kernel-objects.md).

Stage0219 device/name source and contracts use maintained src/runtime and tests/compiler
paths; see [device ownership](kernel-devices.md). Shared CI selects every new portable
contract and actual-header probe. The Windows real-host binding fixture skips on Mac;
that skip does not replace native Mac compilation or title gameplay.

[Unlock migrations](unlock-content.md).

Original sharing bridges and ordinal80 cleanup: [contracts and checks](kernel-sharing.md).

Stage0220 migration models: native-checkpoint, old-controls and old-Mac fixtures
omit0220 together with its0219 prerequisite while constructing historical inputs.
Each complete upgrade explicitly asserts the original sharing include/provider
and ordinal80/16byte cleanup. Full current stacks still include0220. No loader,
patch bytes, runtime behavior, strict warnings or final upgrade checks are weakened.

Online loopback CI synchronization: test_online.py's three-peer handshake waits
for the host's two authenticated latency replies as well as each guest's first
reply, matching its existing assertions. The same four-second deadline, nonce/
credential/replay checks and relay assertions remain. A partial host roster must
not end that wait. This test-only fix changes no transport or runtime behavior.

Original request lifetime/callback prerequisites: [contracts and ABI boundary](request-lifetime.md).

Stage0222: [original driver-call composition and opaque stack payload](driver-call.md).

Driver fault controls must compile under Clang/Werror before runtime rejection:
target expected-pointer weakening retains the expected operand; separate wrong
rollback location/pointer controls retain put. See test_io_target/test_io_call;
unused-mutant compiler errors are fixture failures, never passing fault evidence.

Stage0223: [private request construction and completion delivery](transfer-completion.md).

Stage0224: [host-owned FIFO request scheduling](transfer-completion.md#host-owned-request-queue).

Stage0225: [StartIo callback composition and acceptance](transfer-completion.md#startio-callback-composition); maintained maps include the new provider.

Stage0226: [exclusive waiting membership and device queue mirrors](transfer-completion.md#device-queue-mirrors).

Stage0227: [persistent device queue lifetime and scoped scheduler APIs](transfer-completion.md#persistent-device-queue-lifetime).

Stage0228: [read-only caller-owned request adoption and range lifetime](transfer-completion.md#caller-owned-request-adoption).

Stages0229/0231: [events and targets](transfer-completion.md). Stage0232: [symbolic links](symbolic-links.md).

Stages0233/0234: [routing](guest-paths.md), [original host storage](host-storage.md).

Vehicle mouse coverage: `tests/gameplay/test_mounted_mouse.py` exercises shared
mode `0x400` seat ownership and both host adapter bodies. Its optional supported-XBE
fixture (`vehicle_mouse_guest.c`) runs original seat dispatch, gun update and camera
math, and rejects the old mouse route. This is separate from live mission acceptance;
see [mounted and vehicle mouse contracts](mounted-mouse.md).

The mounted original-vehicle fixture additionally runs 1730E0 track output and
link restoration across camera headings, forward/reverse and steering. Original
CA040 camera coverage now includes nonzero driver pitch; its original custom
ABI saves ESI, while the pitch branch uses other registers as scratch. Existing
zero-pitch ABI expectations remain checked. See [tank controls](mounted-mouse.md).

Stage0235: [original pool imports and native surface retirement](kernel-pool.md).

Stage0236: [original absolute-time codec and guest calendar imports](kernel-time.md).

The0236 upgrade source checker reads UTF-8 explicitly: Windows cp1252 cannot
decode existing bridge box-drawing comments. CI4cb4697c found this fixture-only
error; native calendar source and the accepted game binary do not change.

[Native multiplayer and crossplay validation](multiplayer-ui.md).

[Strings](kernel-strings.md), [RTL](kernel-rtl.md), [formatting](guest-printf.md), [numbers](guest-numbers.md).

[Crash evidence and historical upgrades](build-test-details.md#crash-evidence-and-historical-upgrades).

Crossplay: [acceptance](multiplayer-ui.md#physical-windowsmac-online-acceptance).

Markers: test_ui_layout.py; test_patch_updates.py covers 2290/2291.

Mech/cockpit: test_mounted_mouse.py; [scope](mounted-mouse.md).

Physics: [checks](build-test-details.md#guest-physical-state-regression).

[Story flag and Clang fixtures](build-test-details.md#story-release-flag-regression).

[Languages](launcher.md#complete-game-language-packs-discussion-263).

[2x tests](original-upscale.md).

[Linux release/CI/POSIX](linux-game.md).

[RTL](guest-stack-trace.md), [host policy](execution-host.md), [context](context-execution.md).

GPU asserts stay on; AO logs scene/pixel failures.

Local-player tests: controls-options.md.
