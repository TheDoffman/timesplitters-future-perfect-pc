# Windows Vulkan default and MSVC build policy

Normal Windows packages build both backends and prefer Vulkan. Patch
`graphics/rendering/3050-windows-vulkan-primary.patch` changes initial RHI
selection while keeping D3D11 first in the recovery table. An explicit
`RECOMP_D3D8_BACKEND=d3d11` remains available. Linux/native Mac already prefer
Vulkan; legacy Mac OpenGL is unchanged.

Before Vulkan allocates GPU resources, `rhi_vk_dxc_available` initializes the
verified dynamic shader compiler. Missing DXC returns through the same D3D11
recovery path as a missing Vulkan driver. Partial initialization also destroys
the instance/surface/debug messenger, and allocator destruction is guarded.
`rhi_d3d11_adopt` explicitly selects the backend owning the adopted device;
WARP fixtures must not keep the initial Vulkan dispatch table.

`tests/windows-rhi` and `tools/testing/test-rhi-windows.py` exercise default
Vulkan, explicit D3D11/Vulkan, missing driver/compiler/device recovery and
explicit D3D11 adoption. The no-device case uses the loader's vendor filter in
the child process only. With a verified SDK path, ordinary Vulkan cases require
active validation and reject any Vulkan diagnostic or silent fallback.
`native_refresh_game.py --backend auto` leaves the backend override absent and
requires actual Vulkan guest presents. `--backend d3d11` provides the reference.
Tagged JSON reports record requested and observed backend; saves/config/cache
paths remain isolated. Frame profiling and capture/validation are separate runs.

The normal Release build applies maintained MSVC compatibility rules through
`src/runtime/msvc_generated_code.cmake` and `tools/build/msvc_generated_code.py`.
`config/build/msvc-generated-code.json` identifies the eight measured compiler
exceptions by guest definition, not generated chunk filename. Three units use
`/Od /Ob0`; the others limit inlining with `/Ob1`. Other source keeps Release
optimization. Debug is unchanged. This is a supported host-compiler policy,
not a generated C edit or a change to guest instructions. It preserves bounded
builds after source regrouping and fails on missing/duplicate policy definitions.
The output JSON records affected units and symbols.

MSBuild uses fresh per-source compiler processes, with `-CompileJobs` controlling
their bound (default two). Tests cover regrouped chunks, unchanged source bytes,
stronger rules when definitions share a unit, invalid/absent definitions,
and CMake's actual Debug/Release option expansion. Do not restore handwritten
VCXPROJ exceptions or overwrite another checkout's pending compiler work.

Standard build replay checks explicitly use D3D11, so building does not require
a validation SDK. Dedicated Vulkan acceptance uses the verified SDK. Release
packaging lists Vulkan as the default and includes the pinned DXC DLLs/notices.
The historical controls-upgrade fixture defers patch 3050 until its full graphics
prerequisites are restored, then verifies that the upgrade installs the Vulkan
preference, compiler preflight and maintained MSVC policy.
See [player/build commands](../players/windows-vulkan.md) and measured results
in [STATUS](../project/STATUS.md).

## Launcher renderer selection

`GameSettings.Renderer` defaults to vulkan; the shared controls JSON launcher
object stores renderer as vulkan/d3d11 using the existing atomic save/rollback
and stale-file checks. `Validate` rejects other values before saving or launching.
Windows `SettingsPage.GraphicsRenderer` offers Vulkan (Default)/Direct3D 11 in
Graphics > Display; draft tracking and Save/Cancel follow other settings.
`StartInfo` scrubs inherited runtime variables, then explicitly maps the saved
choice to RECOMP_D3D8_BACKEND only when CanSelectRenderer is true. A package must
advertise both graphics_backends, or be an unpackaged development build.
Legacy packages retain their built-in choice and get an update hint.
`LauncherPlatform.SupportsRendererSelection` is false for MACOS/LINUX; their UI
hides the selector and unrelated saves preserve the Windows preference without
applying it to the native process. Automatic Vulkan startup fallback is unchanged.

Validation: `tools/build/build-launcher.ps1 -Test` passed all existing suites plus
two shared renderer cases. Coverage includes reload, unrelated saves, invalid
values, stale-file protection, inherited overrides, legacy/new package metadata,
UI Save/Cancel and reopening. The three Python shared-layout checks also pass. The existing settings child fixture now launches
both selected backends and checks the actual child's renderer environment.
`--render-settings-graphics` preview was reviewed at 1152x772. NativeLauncher.csproj
and LinuxLauncher.csproj compile with zero errors on Windows (two pre-existing
Avalonia obsolete-event warnings each). Native runtime UI checks remain pending.
SDK 9.0.312 selects ILLink 8.0.25 while the native lock expects 8.0.32; compilation
used isolated NuGetLockFilePath files under build/, leaving tracked locks intact.
No renderer/game rebuild is required for this settings-only change; see the
existing hardware validation above for the two production backends and recovery.

Actual launcher/game follow-up (2026-11-21): source f4b615a9 passed D3D11 then
Vulkan for 120 seconds each on RTX 4070 SUPER / driver 617.42. An isolated WPF
harness selects and saves through SettingsPage, then invokes unmodified
LauncherWindow.LaunchGame; no backend environment injection. Both reach Story,
move/fire, present on the selected backend without fallback, and exit 0. Normal
launcher runs do not enable SDK validation. Evidence and scope are recorded in
STATUS; local report is logs/launcher-renderer-gameplay/report.json. No code fix.

## Extended acceptance goal

The live gate matrix is docs/development/windows-renderer-validation.md; its
remaining gates must not be inferred from the earlier opening-mission smoke.
tools/testing/test-launcher-renderers-windows.py compiles the opt-in WPF harness
tests/launcher_lifecycle/RendererGameSmoke.cs outside release package inputs. It
uses actual shared SettingsPage Save and unchanged LauncherWindow.LaunchGame,
per-label application caches, owned fixture copies and process-path-checked
cleanup. --warm repeats a populated application cache; driver caches are outside
this cold/warm scope. --capture runs are separate from resource measurements.
CPU time, memory, handles and optional dedicated GPU bytes are sampled alongside
guest ticks. First-present observation is polled and is not scanout timing.
--compile-only needs no game data or GPU and is registered in launcher-build.yml.
tests/launcher/test_renderer_acceptance.py guards stale passes, redirected
Data/Saves paths, timeout cleanup, package separation and measurement windows.
First four baseline-cold-warm-01 cases passed; STATUS records hashes and resource
differences, including the Vulkan private-commit increase. Repeats, precise frame
pacing, display/quality matrices, additional levels and save/load remain pending.

Live first-present observation must use ReadLiveLog with FileShare.ReadWrite
(and Delete), because the launcher retains its stderr writer. compile_harness
also runs --self-test-io against an active writer without requiring a GPU/game.
baseline-cold-warm-02 failed in the old observer; its report remains failed.

Display acceptance: --display-cycle (requires capture, starts windowed) checks
actual WS_POPUP transitions with Alt+Enter, resizes the client and restores it.
--capture-times selects <=12 ordered in-session checkpoints. effective_settings
rejects scene/MSAA mismatch and SMAA fallback; Auto checks native resize logs
against the actual resized client. Seven runner safety/measurement tests cover
these gates. baseline-cold-warm-03 passed all4 on2428e479; STATUS records dedicated
GPU allocation and memory observations. Display/quality hardware runs are next.

Fixed scene display-fixed-01 passed both backends (sourcef6c2958b), including
actual fullscreen1920x1080, windowed640x480, resize1008x729 and restoration,
with active scene1280x720 unchanged as intended. Auto remains a separate gate.
--scenario-file validates <=64 ordered key actions/holds and snapshots only
known expectations. story-scotland.json selects the second reviewed Story row;
its hardware coverage is pending. expected_weapon=-1 is a world/input test,
not firearm acceptance. Measurements exclude flagged cutscenes. Eight runner
unit tests pass; explicit guest saving is not proven by a normal window close.

## Auto-resolution allocation

Auto failures on both old backends exposed native HUD sizing consuming the
pending host-size change. pc_display_scene_size now initializes its cached
Auto mode once; later queries retain the accepted dimensions until
pc_display_frame_done successfully calls xbox_D3D8ChangeResolution. An
interleaved HUD query must not suppress allocation or defeat failure retry.
tests/graphics/pc_display_auto.c reproduces the old failure and checks cached
geometry, successful resize, rollback, retry and manual override on shared SDL
policy. Both display-model tests pass; normal Windows rebuild and actual paired
Auto resize acceptance are still required. Mac/Linux native runs are pending.

Optional ETW console is pinned by config/build/presentmon-windows.json (2.6.0,
MIT, standalone EXE only). Official release asset size/SHA were verified; its
license/receipt stay under ignored external/presentmon/2.6.0. No service, driver
or OS permission changes were made. A PID-filtered two-second ETW prerequisite
probe starts/stops successfully without elevation; actual graphics traces remain
pending. Launcher samples now include QPC milliseconds for later ETW alignment.

Rebuilt Windows Auto acceptance now passes both backends on gameSHAa8a26b31
(source8b3e6fca): native targets1920x1080,640x480,1008x729,640x480 follow the
actual mode/resize cycle, with reviewed consistent gameplay captures and exit0.
Mac/Linux runtime acceptance remains pending; historical failed reports remain
failed. --presentmon uses the pinned console, UUID session names and owned PID
filtering, with --no_track_input. --repetitions3 makes one cold and two warm
application-cache runs per backend. QPC-aligned ETW measurements require one
swap chain, owned PID, sufficient displayed frames and coherent interval/row
coverage. These are measurement-integrity gates; relative performance acceptance
requires the separate comparison of all runs. Nine runner unit tests pass.
