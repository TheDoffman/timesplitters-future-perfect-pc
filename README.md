<div align="center">

# TimeSplitters: Future Perfect

Direct mouse aiming · 4K & ultrawide · Custom controls · Online & LAN

<a href="https://github.com/TheDoffman/timesplitters-future-perfect-pc/releases/latest"><img src="docs/assets/download-launcher.svg" alt="Download the latest Windows launcher" width="280" height="52"></a>

**[Quick start](#start-playing)** · [Launcher guide](docs/launcher.md) · [Multiplayer](docs/multiplayer.md) · [Build from source](docs/building.md)

<sub>Windows 10/11 x64 · Experimental PC port · Supported US Xbox game required</sub>

</div>

![TimeSplitters launcher with Play Game, game import, update channel and release controls](docs/assets/launcher-preview.png)

<p align="center"><sub>Launcher interface preview from the current source. Version and channel shown are sample state; choose <strong>Preview</strong> to install current game prereleases.</sub></p>

> [!IMPORTANT]
> You need your own supported **US Xbox ISO or extracted game data**. Import it
> once through the launcher; afterward, play from the launcher without the ISO.
> Original game files are not included. No emulator or developer tools are needed.

## Start playing

| Step | Installation |
| :---: | --- |
| **01** | **Download the launcher.** Open the [latest launcher release](https://github.com/TheDoffman/timesplitters-future-perfect-pc/releases/latest). Choose `TimeSplitters-Launcher-Setup-windows-x64.exe` when available. Installer support is in current source; older releases provide the portable ZIP. |
| **02** | **Install and open.** Run Setup, choose a folder and optional desktop shortcut, then open **TimeSplitters Launcher** from Start. For a portable ZIP, extract the whole archive to a writable folder and open **TimeSplitters Launcher.exe**. |
| **03** | **Install the game runtime.** Set **Update channel → Preview**, then choose **Install release**. The launcher downloads and verifies the published PC runtime. |
| **04** | **Import your game once.** Choose **Locate ISO**, or **Use extracted files** for a folder containing `default.xbe`, `pak` and `xmv`. |
| **05** | **Press Play Game.** The launcher minimizes when the game window appears and returns when you finish. |

**Next time:** open **TimeSplitters Launcher.exe → Play Game**. Installed play
remains available offline.

The installer uses your Windows account without requiring administrator access.
Existing portable users can select their current game folder during Setup to
reuse downloaded files and imported data. Uninstall removes the launcher and
its shortcuts while keeping game files, saves, settings, the ISO and imported
data. See the [installer guide](docs/launcher.md#install-with-setup-upcoming-launcher-release).

<details>
<summary><strong>System requirements, channels and keeping your saves</strong></summary>

- **Windows 10/11 x64** with **.NET Framework 4.8**.
- Your supported **US Xbox game**, title ID `45410066`; [exact executable hash](docs/building.md#inputs-and-generated-files).
- Internet access for the initial runtime download; about **5 GB** for the game,
  plus space for update staging, rollback and any optional textures.
- **Preview** includes published game prereleases. **Stable** excludes them.
  Launcher and game versions are separate.
- Keep the whole installation together, especially `Data`, `Settings`, `Saves`
  and the launcher's recovery files. Normal updates preserve existing data,
  settings and saves.

Python, Git and Visual Studio are not required to play.

</details>

## PC features

<table>
<tr>
<td width="50%" valign="top"><strong>Mouse aiming</strong><br>Direct camera control, zoom-aware sensitivity and normalized WASD movement. Rebind actions and adjust sensitivity with <strong>F2</strong>.</td>
<td width="50%" valign="top"><strong>Resolution and aspect ratio</strong><br>Native <strong>1080p, 1440p and 4K</strong> rendering, monitor-reported ultrawide modes and proportional cutscene and startup-video framing that fills ultrawide displays without side bars.</td>
</tr>
<tr>
<td valign="top"><strong>Input prompts</strong><br>Formatted menu and HUD prompts follow your bindings and switch back to controller icons when you use a controller.</td>
<td valign="top"><strong>Online and LAN multiplayer</strong><br>Experimental player-hosted Online matches, invitations, private join codes and compatible-build browsing, plus LAN System Link.</td>
</tr>
<tr>
<td valign="top"><strong>Launcher</strong><br>Install releases, import game data, check updates and press <strong>Play Game</strong>. Roll back an update when a previous release is available. Current source adds a <a href="docs/launcher.md#compare-specific-game-versions-upcoming-launcher-update">game version picker</a> for testing published older builds.</td>
<td valign="top"><strong>Performance diagnostics</strong><br>Optional FPS, frame-time and network-latency displays. Use <strong>F9</strong> for the FPS overlay and <strong>F12</strong> for debug settings.</td>
</tr>
</table>

Recent fixes improve pickup-range checks, controller sensitivity adjustment,
story audio transitions, Khallos Express scenery rendering and multiplayer
movement updates. [See measured progress →](STATUS.md)

### Controls and settings

**Game settings** in launcher v1.2.0 lets you set keybinds and graphics before
playing. Resolution selectors use your primary display's reported modes, include
the console default, and provide left/right arrows. Saved custom modes are retained.

Launcher v1.2.11 adds **What's new** for game/launcher release notes,
installed and available versions, and published known issues inside the main window.

| Controls | Display | Launcher |
| --- | --- | --- |
| **F2** opens keyboard/mouse settings. Save bindings, sensitivity and inversion. Pause before editing. | **Settings → Display settings** (or paused **Options**) selects aspect-ratio groups, resolution and vertical FOV (40–100°, default 55°). Confirm resolution changes within **15 seconds** or it reverts. | **Check updates → Update release** installs game updates. **Repair / Verify → Open game folder** opens the installation folder. Its **Diagnostics** tab provides system reports and GitHub reporting in launcher v1.2.12. **Saves**, **View logs** and **Support** open the save folder, logs and issue tracker. |

In the main, pause and Tab/Objectives menus, hover to highlight, **left-click** to select and
**right-click** to go back. Use the **mouse wheel** to move up/down through
menu rows, including the level-selection list; click to activate a level.
Scrolling takes priority over hover until you briefly pause the gesture; clicks
remain immediate.
Visible, enabled prompts along the bottom (Select, Back, Player Progress and
other shortcuts) turn light blue on hover and accept left-click. This also applies
to the profile picker's Select, Back and Delete Profile prompts. Disabled prompts
keep their normal appearance. The main-menu footer, decorative strips and animated
blue backdrop fill widescreen and ultrawide displays while labels and buttons
stay proportional. The in-game Objectives header/footer bars also fill wide
screens. Full-screen transition fades, menu background tints and story blackouts
also cover the output without changing their original opacity or timing. Loading-picture
fades and their top/bottom borders span the same full width. The loading label
uses proportional UI scaling and follows the left screen edge on wide displays.
Older, conventional and futuristic weapon-scope masks extend to the output edges while the
aperture and reticle keep their proportions. The Sci-Fi Sniper extends its translucent tint instead
of adding opaque margins. Shared aim reticles keep their projected aim point while their shapes fit
the display; zoom gauges follow the left/right edges and full-view aiming lines span the output.
Lock-on indicator pieces scale around their target. Special scope gradients and screen effects
reach the display edges while their ring artwork retains its proportions.
The main Story/Arcade menu uses a smaller font so its list
clears the logo and scrolling description. Click the Options/Status headings to switch pause or Objectives
tabs; available headings highlight on hover. Keyboard and controller navigation
still work.
Closing Pause or Objectives restores gameplay mouse control automatically;
no extra click is needed. **Escape** pauses and releases the pointer. Use
**Alt+Enter** for fullscreen. Settings saved in-game remain in place for your
next launcher session.
**Quit to Desktop** is available below Extras in the main menu and at the bottom
of Pause. It opens a confirmation with **Cancel** selected. The pause menu's
existing Quit still returns to the game menus.

<details>
<summary><strong>Default keyboard and mouse controls</strong></summary>

| Action | Windows control |
| --- | --- |
| Move / look | W A S D / mouse |
| Fire / aim | Left / right mouse button |
| Use / confirm | E, Space or Enter |
| Reload / crouch | R / Ctrl or C |
| Melee / menu back | F, middle mouse or Backspace |
| Throw grenade | G or mouse button 5 |
| Change weapon | Mouse wheel or left/right arrows |
| Weapon mode / scope zoom in | Q or Up / Page Up |
| Change grenade / scope zoom out | V or Down / Page Down |
| Keyboard and mouse settings | F2 |
| Fullscreen | Alt+Enter |
| Show / hide the PC menu bar | F1 |
| FPS overlay / debug settings | F9 / F12 |

The original game has no jump action; Space is a confirm/action binding. Aim
and crouch behavior follows your in-game profile settings. F2 blocks gameplay
input but does not pause the simulation. Prompts baked into artwork retain their
original appearance. [Full controls and display guide →](docs/windows-settings.md)

</details>

## Multiplayer

Everyone needs the same game build and matching game data. In the game, choose
**Online / System Link → Online**.

- **Host:** forward **TCP 30820** and **UDP 30821** to your PC and allow them
  through Windows Firewall. Leave automatic hosting enabled, click **Start New
  Match**, and send friends the full invitation. Return to the game and select
  **Start New Match** to choose the map and mode.
- **Join:** paste the host's invitation into **Invitation / lobby code**, click
  **Join by code**, then select the host's match in the game and **Ready**.
  The host selects **Go**. Guests do not need to forward ports.
- **On the same local network:** choose **System Link** instead.

Experimental bots are configured through the host's **Options → Bot Set**.
The lobby shows players, admitted bots and the selected map/mode capacity.
Humans take priority; excess selected bots wait for space without changing
the saved bot set. The host sees how many are waiting.

> [!NOTE]
> Online is experimental. Router setup is not automatic; real internet routing
> and sustained cross-machine play still need validation. Campaign co-op is not
> yet supported.

**[Host or join a match →](docs/multiplayer.md)**

## In development

| Texture overhaul | Native Apple Silicon |
| --- | --- |
| Modern materials and surfaces, with **up to 4K** on major surfaces. A **6 GiB** active-pack budget and **512 MiB** replacement-residency limit keep storage and memory bounded. | A native **ARM64** build with SDL/Cocoa and OpenGL, shared PC controls, native settings and an experimental **up-to-120 FPS** presentation target. |
| **Partial local preview.** Much of the game still uses original art. Development artwork is not bundled with the public release; 4K output does not imply native 4K source detail. | **Experimental.** Visible first-level gameplay and basic combat checks have been reached. Interpolation keeps the game simulation at **60 Hz**; movies, advanced rendering, multiplayer and packaging still need work. |
| [Texture guide →](docs/textures.md) | [Mac setup →](docs/macos.md) · [Feature backlog →](MAC_PORT_BACKLOG.md) |

> [!NOTE]
> This is an experimental port. A full campaign playthrough and complete
> rendering fidelity remain unverified; periodic multiplayer frame-rate drops
> remain under investigation. Mac full-level completion and save/load are also
> unverified. Features describe current source and the published launcher—check
> **Release notes** for your installed game version.

## Guides & support

| Player guides | Developer guides |
| --- | --- |
| [Windows launcher](docs/launcher.md) — installation, updates and recovery | [Build guide](docs/building.md) — prerequisites, setup and patch workflow |
| [Controls & display](docs/windows-settings.md) — bindings, resolution and diagnostics | [Release publishing](docs/releasing.md) — package, validate and publish |
| [Multiplayer](docs/multiplayer.md) — Online, invitations and LAN | [Measured progress](STATUS.md) — evidence and remaining work |
| [Native Mac](docs/macos.md) — Apple Silicon setup and controls | [Project rules](AGENTS.md) — reproducibility and data preservation |

**[Report a bug](https://github.com/TheDoffman/timesplitters-future-perfect-pc/issues/new?template=01-bug-report.yml)**
or **[request an enhancement](https://github.com/TheDoffman/timesplitters-future-perfect-pc/issues/new?template=02-enhancement.yml)**.
For bugs, enter your installed versions and platform, then describe the problem. For an
improvement, just share your idea. Screenshots and logs
are optional. Published version references stay up to date automatically; both forms
receive labels automatically.
[Find your build version and logs](docs/launcher.md#report-a-problem-or-request-an-improvement);
keep personal saves and game data private.

---

### Faster gameplay testing

Use the local Test Lab profile for unlocked levels, characters and challenges,
with collectible progress left fresh. A bounded isolated Windows session also
skips startup videos: `python tools/testing_profile.py --play --seconds 120`.
See [testing profile setup and isolation](docs/testing-profile.md).

### Credits

Built on **[nemonicicon/ts3FPrecomp](https://github.com/nemonicicon/ts3FPrecomp)**,
with local runtime patches and platform adapters. This community project
statically recompiles the Xbox executable into native code; it does not recover
the original C/C++ source. [Pinned upstream and licensing →](docs/building.md#upstream-and-licensing)

This repository contains port code, patches and tooling. Original game assets,
the Xbox executable, generated game code, personal settings and saves remain
local. Retain the applicable upstream and third-party licenses with builds.
