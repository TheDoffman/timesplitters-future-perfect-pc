# Documentation

Start with [install and play](../README.md#start-playing) for released builds,
or [repository layout](development/repository-layout.md) for source and local file locations.

| Task | Guides |
| --- | --- |
| Install and use the launcher | [Launcher](players/launcher.md), [Windows implementation](players/windows-launcher.md), [Mac launcher](players/mac-launcher.md) |
| Configure the game | [Windows settings](players/windows-settings.md), [Mac setup and controls](players/macos.md), [high refresh rates](analysis/native-high-refresh.md) |
| Play online | [Multiplayer](players/multiplayer.md), [Discord presence](players/discord-presence.md) |
| Use textures | [Textures](textures/textures.md), [alpha handling](textures/texture-alpha.md), [menu textures](textures/main-menu-textures.md) |
| Troubleshoot | [Known issues](players/known-issues.md), [Windows crash logger](players/windows-crash-logger.md), [high-refresh regressions](analysis/native-high-refresh-regressions.md) |
| Build and contribute | [Building](development/building.md), [contributing](../CONTRIBUTING.md), [shared platform](development/shared-platform.md), [independent runtime](development/independent-runtime.md) |
| Test and release | [Test profiles](development/testing-profile.md), [CI](development/ci.md), [releasing](release/releasing.md), [launcher releases](release/launcher-releases.md), [launcher compatibility](release/launcher-compatibility.md) |
| Investigate internals | [Subsystem map](agent-context/README.md), [patch maintenance](../patches/README.md), [online bot investigation](analysis/online-bots-investigation.md) |
| Check progress | [Measured status](project/STATUS.md), [Mac backlog](project/MAC_PORT_BACKLOG.md) |

Player guides live in `players/`; update callers and relative links when moving
a guide. Historical external links may refer to its previous repository path. Technical subsystem notes belong in `agent-context/`;
documentation screenshots and download buttons belong in `assets/`.
