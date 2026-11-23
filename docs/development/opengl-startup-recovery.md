# OpenGL startup recovery and launcher selection

Implementation and available-host verification are complete, with the explicit
native-host deferrals below. This is local work on `feature/opengl-startup-recovery`, based on
`4c287926`. Nothing from this goal has been published or merged.

The user restored the launcher-selector requirement on 2026-11-22. It supersedes
the original goal text that removed the selector. Vulkan remains the default for
modern builds; **Settings > Graphics > Advanced > Renderer** offers Vulkan and
OpenGL, saves the choice and uses it on the next launch. Legacy Mac builds retain
their existing OpenGL default and frontend. D3D11 is not a launcher choice or an
automatic fallback.

## Platform decisions

| Platform | Automatic Vulkan-to-OpenGL recovery | Acceptance |
| --- | --- | --- |
| Windows | Enabled | Ordinary build, actual launcher/game, failure paths, graphics/window checks and bounded stability passed on Windows 11 / RTX 4070 SUPER / driver 617.42. Other vendors are not certified by this host. |
| Linux | Disabled | Shared GPU contracts and actual game recovery/cleanup/preservation passed under WSL. Manual choices both reach gameplay; WSLg does not report the launcher minimized, fullscreen checks fail, and measured GL gameplay is about 23 host presents/s. Physical Linux/Steam Deck acceptance is unavailable. Private X11 lifecycle verification is recorded separately below. |
| Native Mac | Disabled | Shared source/unit checks and Mac launcher compilation are available; a native Apple Silicon game/GPU host is not. Actual capability, driver, gameplay, window, performance and package acceptance remain deferred. Apple OpenGL is deprecated. |

`src/graphics/rhi_platform_acceptance.h` owns these defaults. Test fixtures can
explicitly exercise either gate. An environment variable or saved preference
does not enable automatic recovery on an unaccepted platform.

## Startup and preservation contract

`src/graphics/rhi_startup.h`, integrated by patch3052, owns the common transaction:

1. Create the selected backend. A successful Vulkan initialization stays Vulkan.
2. On failure, destroy partial primary resources before considering recovery.
3. Only an accepted Vulkan startup may try OpenGL, once. Select it only after
   creation succeeds. Explicit OpenGL selection starts OpenGL directly.
4. If OpenGL also fails, destroy its partial resources, log both failures and
   terminate game startup with a clear error. Later rendering/presentation
   failures do not enter this transaction.

The transaction does not write settings, saves or textures. Recovery leaves the
saved Vulkan choice intact. Shared launcher settings preserve Save/Cancel and
restart behavior, unrelated settings and unsupported choices on older packages.
Packages advertise their compiled `supported_renderers`; an unsupported choice
produces update/selection guidance instead of silently choosing another backend.

The OpenGL RHI, shader translation, resource/view policy and graphics algorithms
live in `src/graphics`. WGL and SDL adapters handle OS windows, contexts and
events. Native GL uses an owned guest worker and SDL owner-thread dispatch while
preserving guest TLS and queued input. Ordinary native Vulkan with recovery off
and the separate legacy Mac entry route retain their previous execution path.
No gameplay implementation or generated guest code is duplicated for OpenGL.

The common GL backend checks core4.1, full S3TC BC1/2/3 and mirror-once support
before allocating shared GPU resources. Unsupported hardware fails explicitly.
The restricted4.1 view/copy path can preserve requested MSAA while rendering
single-sample when the required view/stencil capabilities are unavailable.
That Windows compatibility probe is not certification of Apple's GL driver.

## Current evidence

Full provenance, command output and historical failures are indexed in
[the investigation ledger](../agent-context/opengl-recovery.md). Logs and private
game fixtures stay local and ignored.

| Requirement | Authoritative evidence |
| --- | --- |
| Primary success, recovery success, both failing, disabled/missing backend, explicit GL failure and cleanup ordering | Compiled resource-owning transaction spies; seven transaction/platform cases pass on Windows and Linux. Six combinations cover platform defaults and fixture overrides. |
| Real GPU resources, shaders, sampling, colors, depth/MSAA/views and cleanup | Windows accepted recovery matrix8/8, explicit GL and restricted4.1 contracts; native Linux matrix8/8 including window replacement and real SDL owner progress. Windows Vulkan validation was explicitly enabled with the verified SDK layer. |
| Ordinary accepted Windows build | Source `bfb73181`, game SHA256 `a8cf11b698b143a680a87e2766f75d4520b5ef5cb52ef1eee48082d00202ae9a`; supported Build/checks and matching symbols pass. No private CMake include or acceptance definition remains. |
| Actual Windows primary/recovery/terminal failure | `vk-enabled-default-01`, `gl-enabled-startup-recovery-01`, `gl-enabled-both-fail-01`: same normal game; primary stays Vulkan, recovered game runs OpenGL once, both-fail exits2 without presents. Settings/custom assets retained. |
| Saved Windows UI choice and rendered gameplay/effects/window changes | Actual launcher selector runs for both choices; reviewed menus/world/weapon/HUD/fire, active MSAA/SMAA/SSAO, Auto resizing, fullscreen/windowed/resize/restore and clean exits. |
| Bounded Windows stability | `gl-current-selector-soak-01`,300s: early/late movement, pause/resume, quality,60Hz guest clock and bounded resource growth. Private growth62,124,032 bytes, handles-6, dedicated GPU-593,920 bytes. This does not prove absence of slower leaks or measure scanout. |
| Native ordinary build and actual recovery | Source `2398062a`, normal ELF SHA256 `791eaca3b3dc7a5130f70545f4e55a0846435530832baf5cbef42c640ac442b1`; build/MMIO pass. Private trial `linux-real-recovery-01` reaches gameplay/movement after Vulkan failure. `linux-real-both-fail-01` exits2 with settings/assets/saves unchanged. Finally restores the exact normal ELF and disabled gate. |
| Native input and bounded progress | `gl-ready-controls-01`: validated local ownership,31.23 game units moved, weapon14 ammunition16→15, pause/resume, clean120s run and reviewed moved-world image. About23.37 host guest presents/s on the WSL GL bridge. Headless idle-dispatch CPU regression also passes. |
| Shared launcher persistence and integrity |55 native shared UI groups, Windows selector/game checks,45 SDK/Mac-launcher Python checks, five native fixture-isolation tests and Mac launcher compile. Native preflight follows the selected renderer, retaining compiler integrity checks without requiring Vulkan driver setup for explicit Mac common GL. |
| Fresh/historical runtime patch application | Four focused fresh/repeated, native-checkpoint, controls and Mac-stack upgrade tests pass against3058/3059. Subsequent acceptance-header work changes no patch text. |
| Package metadata and licenses | Windows/Linux/Mac writers preserve renderer capabilities, shader compiler provenance and the pinned SPIRV-Cross notice. Source/fixture tests cover the layout. A minimal launcher test installation is not clean package relocation evidence. |

The Linux selector probe uses the actual Settings page and Save event, restarts
the launcher between choices, preserves StartInfo's backend and forbids its test
environment from overriding that backend. WSLg full runs have valid rendering,
Story, persistence and clean-exit observations but remain **failed lifecycle
tests** because minimization is not observed. Short private Xvfb/Openbox checks
pass both choices and minimize/restore. Full private-X11 runs also pass both
120s choices, active Story, persistence, unrelated settings and clean exits
(`native-selector-x11-full-03`). Both ordinary-game renderer self-tests pass
resize/fullscreen there (`native-private-x11-display-01`). These remain virtual
desktop checks, with no physical-window or scanout approval.
Linux's cross-process game-window query remains unavailable; no update-recovery
or physical-window approval is inferred from a rendered frame.

All game probes use disposable Test Lab copies, owned settings/cache/capture
paths and child-only driver overrides. Original fixture integrity is checked.
Custom DDS preservation checks demonstrate file retention, not a complete
Enhanced texture-pack visual audit. No SDK file, global driver registration or
personal save/settings directory is modified; no host restart is issued.

## Exact deferred native acceptance

Linux automatic recovery remains disabled until a supported physical desktop
can complete Vulkan/OpenGL launcher and game checks, window/fullscreen behavior,
enhanced graphics settings and representative scenes, bounded GPU performance
and stability, plus a clean relocated package without checkout SDK dependencies.
Steam Deck remains a separate hardware target. WSL software Vulkan, the D3D12 GL
bridge and an isolated virtual X11 desktop cannot certify those targets. Failed
WSLg lifecycle/fullscreen results remain failed; they are not waived by another
environment passing.

Native Mac additionally needs an Apple Clang game build on the supported macOS
version, actual GL4.1/S3TC/mirror-once and shader/resource compatibility checks,
native SDL owner-thread/window/input tests, both startup failure paths, saved
selector behavior, settings/saves/textures, bounded performance and a relocated
package with the required compiler libraries. The legacy Mac build/frontend
must also be exercised on supported legacy hardware. Its source/build selection
is preserved; runtime compatibility is not certified from Windows/Linux tests.
Deprecated OpenGL support must not be presented as a guarantee for future macOS.

## Reproduce the available checks

Use the normal setup/build instructions and your own supported game data. The
bounded test runners enforce isolated profiles and unique evidence labels.

```powershell
.venv/Scripts/python.exe -m pytest tests/graphics/test_rhi_startup.py tests/graphics/test_opengl_build.py -q
.venv/Scripts/python.exe tools/testing/test-launcher-renderers-windows.py --label my-vulkan-check --backend vulkan --seconds 120
.venv/Scripts/python.exe tools/testing/test-launcher-renderers-windows.py --label my-recovery-check --startup-recovery opengl --seconds 120
.venv/Scripts/python.exe tools/testing/test-launcher-renderers-windows.py --label my-terminal-check --startup-recovery both-fail --seconds 120
```

```sh
.venv-linux/bin/python tools/testing/test-linux-launcher-renderers.py \
  --launcher build/linux-launcher/package/TimeSplitters.Launcher --label my-selector-check
```

The Linux runner accepts child-only `--driver-manifest`, `--gl-driver`,
`--gl-adapter`, `--sdl-driver` and `--wayland-display` controls. Keep software,
WSL and physical-driver results distinct. Do not package an accepted native
diagnostic trial; restore the ordinary disabled build and verify its receipt.
