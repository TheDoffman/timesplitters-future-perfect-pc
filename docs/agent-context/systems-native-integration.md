# Native network and UI integration

POSIX startup retains its original UTF8 boot labels. Online-menu publication
keeps failed-write cleanup and its unconditional return on separate lines so
GCC's misleading-indentation warning remains enabled under -Werror.

[Sync telemetry](network-sync-telemetry.md).
[Pad](controller-deadzones.md), [Rollback](control-presets.md), [UI](main-menu-update.md).
[Actors](network-actor-lifetime.md).
[Tex](texture-preference.md); [arms/V-Sync](video-options.md).

[Uplink](native-uplink.md#throw-impulse); [IDs](online-player-identities.md). [UI](native-multiplayer-menu.md).
[Network startup](network-performance.md): one-time config read, atomic dispatch;
LAN enables explicitly, Online publishes connect/disconnect. No stale re-import.
Shared Windows/Mac/Linux; fixture: `tests/network/network_eligibility.c`.
[Sun/visibility](fidelity-comparison.md): optional preset removed; normal settings restored.

[Tools](developer-tools-integration.md).
SDL self-tests use click/Escape capture; F1 opens optional Developer Tools.
Only synthetic events in src/native/input_sdl.c change. Linux passes;
Mac execution pending.

PC bar: [state](interaction-input.md#pc-settings-bar-visibility).

[Clicks](interaction-input.md#fresh-weapon-clicks-434).

[Story](systems-details.md). [Rejoin](multiplayer-exit.md).

[Mech](mounted-mouse.md).

Minigun: [policy](native-minigun.md).

[Kernel](kernel-objects.md).

Memory: [retired APIs](retired-memory-compatibility.md).

Guest storage: [allocation](independent.md#guest-allocation-ownership), [threads](thread-memory.md), [virtual memory](virtual-memory.md).

Image/state/ABI: [image](independent.md#native-image-loading), [execution](independent.md#guest-state-and-abi-reports).

Wind: [time scopes](native-wind.md).

Aim/animation: [diagnostic limits](native-local-trace.md).

GPU cache fixture/config: [runtime](runtime.md).

Native player contact: [clear-slide policy/trace](native-player-contact.md).
GPRs: [scoped aliases/tests](gpr-refs.md).
Script/audio clocks: [contracts](native-script-clocks.md).

Mod preflight also rejects restored flags on a previously unsupported fresh epoch.
