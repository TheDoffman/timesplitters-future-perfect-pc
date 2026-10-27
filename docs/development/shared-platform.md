# Shared PC and native Mac implementation

Ordinary features should be implemented once and built for both hosts. This
requires common behavior plus small adapters for each operating system or GPU
API. Git synchronization, a shared account and a backlog do not translate code.

## Short captured-frame checks on Mac

The shared `d3d8_replay` capture reader and Xbox renderer now build on native
Mac Vulkan through a thin SDL window adapter. No guest execution or game files
are required to build it or run its synthetic triangle/readback contract:

```sh
.venv/bin/python tools/testing/test-rhi-mac.py --driver kosmickrisp
```

Use `--driver moltenvk` to check the other pinned driver. CI runs the asset-free
Mac contract alongside the existing RHI checks. It verifies red-triangle and
blue-clear pixels; a passing fixture does not establish mission fidelity.

For an existing local capture, the bounded wrapper replays one frame in at most
30 seconds using isolated runtime, display, input and save paths:

```sh
.venv/bin/python tools/build/replay-native-mac.py private/captures/native-mac/khallos-sky-probe.d3dcap --draws 3 --skip-draw 0
```

The example capture is local evidence, not a repository fixture. The output
reuses `private/captures/native-mac/replay/frame-000.bmp`; retain selected frames
before rerunning. Logs and source/capture/executable hashes are in
`logs/native-frame-replay.log` and `.json`. `--draws` limits the draw prefix and
`--skip-draw` omits a zero-based draw. These are diagnostic exclusions, not fixes.
Captured game assets stay ignored and local. Windows retains its existing
replay frontend; the new SDL adapter is only selected on Mac.

## Current boundaries

| Area | Common implementation | Host-specific work |
| --- | --- | --- |
| Launcher pages and theme | `Launcher.xaml`, `*Page.cs`, `PageTheme.cs`, `PageTheme.xaml`, `TabsTheme.xaml` | `Ui.cs` maps WPF/Avalonia events and controls; native templates/text rendering, dialogs, process lifetime and file opening |
| Guest execution and gameplay fixes | Pinned runtime, complete patch stack, generated guest code | Build/compiler and process entry |
| Menu, camera and display policy | `pc_display*`, `pc_menu_*`, `pc_cutscene.h`, `pc_ui_layout.h` | OS pointer events, window/resolution application |
| Screen-copy rectangle policy | `graphics/screen_copy_rect.h` clips paired source/destination rectangles once | RHI shader draw or legacy GL blit |
| Text-entry dialogs | `pc_text_input.h` queue/commands and `pc_menu_text.inc` original guest editing callbacks | Win32 character messages; SDL text-input events and main-thread activation |
| Controls and configuration | `pc_input*`, `pc_bindings.inc`, shared validation | Win32/SDL events and atomic file writes |
| Debug settings | `pc_debug_options.def` generates fields, keys, labels and capability metadata | Win32/Cocoa controls, layout, log-folder opening |
| Menu reconstruction shader | `graphics/menu_sampling_shader.h` and `pc_menu_sampling.h` eligibility | HLSL/GLSL intrinsic/resource syntax and uniform/constant binding |
| Renderer window/events | `RhiHost` callbacks and drawable-size contract | Default Win32 host and SDL/Cocoa host; worker threads use cached physical dimensions |
| Xbox graphics semantics | `d3d8_*` device/resources/shaders/display in Windows and normal Mac Vulkan builds | SDL/Vulkan window and CoreText text bitmap; legacy Mac GL remains a compatibility fallback |

The shared debug definition automatically exposes a new boolean setting in both
dialogs and persistence, unless its required capability is unavailable. Runtime
behavior for that setting must still be implemented. Mac currently lacks the
network-latency provider, so its dialog preserves that saved value without
offering an unsupported control. Existing keys and defaults remain compatible.

The menu shader has one algorithmic body. Changing its reconstruction or glyph
coverage rules changes both backends. Its language adapters must contain only
syntax/resource operations, not a second set of coefficients or feature rules.
Windows and supported Macs compile this through the common RHI frontend. The
legacy Mac GL adapter also consumes the shared algorithm.

## Adding a feature

1. Put platform-independent state, validation and behavior in the common model
   or patch stack. Extend an existing shared entry point before creating a new
   host-specific implementation. Keep generated game C reproducible.
2. Extend adapters only for OS/GPU operations. Use capability checks for missing
   providers. An operating-system guard around portable logic requires a reason
   and an explicit adaptation/validation entry.
3. Add behavioral checks using the same inputs/expectations on both hosts.
   Register them in `.github/workflows/runtime-patches.yml`. Its suite selector
   includes source/test changes; PR checks always report even for unrelated
   paths. New tests do not run merely because a file has been created.
4. Run available native checks and record other-host checks as pending. Keep
   shared-source, compile, GPU, window/input and gameplay evidence distinct.
5. Update `docs/project/MAC_PORT_BACKLOG.md` for remaining adapters/validation and `docs/project/STATUS.md`
   for measured results. Record a platform exception rather than silently
   presenting partial functionality as complete cross-platform support.

Source-only CI needs no game assets. Menu sampling runs the same pixel contract
on Windows D3D11 WARP and macOS offscreen OpenGL. Debug model checks include adding
a definition-only option to a disposable fixture; native UI tests additionally
check generated labels, capability visibility and save/cancel/preservation.
The Mac menu translation-unit check catches missing internal callbacks that a
mocked policy test can miss. A configured workflow is not evidence that its
remote Windows job has run successfully.

Use the supported native builder and bounded isolated runner for local game
checks. Preserve original 60 FPS defaults and the 60 Hz console interrupt clock.
The launcher offers opt-in native 120/144 beta settings for compatible game builds;
this does not revive the retired interpolation/prediction engine or automatic
monitor switching. Historical patches/contracts below describe implementation
history, not current feature validation.
Preserve both display configs, controls, personal saves and the canonical
Test Lab fixture. See root `AGENTS.md` for commands and cleanup requirements.

## Common renderer and remaining exceptions

The Windows frontend already targets the pinned runtime's `rhi.h`, which accepts
HLSL shader source. The Mac GL renderer still implements much of the Xbox-facing
device/resource/shader behavior separately. Sharing a small shader library does
not make every future Windows renderer feature work on Mac automatically.

The common frontend now compiles on Mac in the asset-free contract, including
device, resources, state, shader compilation and display policy. `RhiHost` removes
Win32 event pumping and window sizing from that frontend. The current GL renderer
uses the same interface through `renderer_host_sdl.c`; callback registration is
outside the render-thread lifetime and drawable sizes are physical pixels.
The native Vulkan game links this frontend by default on macOS 26+; legacy GL
remains available explicitly and for older hosts.

The existing Vulkan backend now has an SDL surface adapter and uses the same
loader as SDL. It compiles the PC HLSL through DXC and passes the shared menu
pixel contract, swapchain readback and presentation on this Mac. Portability
extensions are enabled only when advertised. This is the selected integration
route. The native game connects its Xbox frontend and host services. Normal
builds now select it on hosts supported by the pinned SDK; GL is the older-host
compatibility route. Missing/changed SDK files fail verification rather than
silently selecting a different frontend.

The asset-free probe uses `tools/testing/test-rhi-mac.py` and the SDK pinned in
`config/build/vulkan-sdk.json`. Setup verifies the publisher's archive checksum and
installs only inside ignored `external/vulkan-sdk`; receipts verify the relevant
installed files on later runs. Licenses and version manifests remain with it.
The shared renderer/probe needs macOS 26+ with this SDK; the GL fallback's
requirement is unchanged. Run `--setup-sdk` once, then `--driver moltenvk` or
`--driver kosmickrisp`. See [Mac guide](../players/macos.md#common-renderer-development-probe).
The same `tests/graphics/rhi_menu_sampling.c` pixel test runs on Windows D3D11 WARP,
without a swapchain there. Hosted Windows and Mac validation passed at
`083e57c`; see [the CI results](../project/STATUS.md#shared-platform-ci-validation-2026-10-19).
New cube/volume resource cases pass on hosted Mac Vulkan and Windows WARP at
`e864831`. The deeper Xbox-factory cases also pass on both hosts at `f2c2261`
in [run 39308234267](https://github.com/TheDoffman/timesplitters-future-perfect-pc/actions/runs/39308234267).

The game can now be built with `tools/build/build-native-mac.py --renderer vulkan`.
Its SDL adapter creates the window and supplies physical dimensions/events;
CoreText rasterizes the common overlay text. The PC device/resource/shader,
combiner, screen-copy and display code is compiled directly into the Mac game.
The bounded runner selects the pinned GPU environment from `BUILD.json` while
preserving all isolated save/config paths. See the [developer commands](../players/macos.md#opt-in-common-game-renderer).
First-mission evidence is scoped in [STATUS](../project/STATUS.md#common-xbox-renderer-on-native-mac-2026-10-19).
Normal `--renderer auto` builds now select this implementation on supported
hosts. `--setup` installs its pinned SDK; an explicit `--renderer gl` build needs
no SDK. `BUILD.json` records the requested/resolved renderer and Vulkan driver.
New builds select MoltenVK; older manifests without a driver retain KosmicKrisp.
Both launchers call `tools/build/vulkan_sdk.py` for the same verified driver mapping.
Driver overrides do not change gameplay settings or the 60 Hz guest clock.

Interpolation now uses common replay-owned depth/stencil per source subresource,
with fresh history each replay and restoration of guest target bindings. The
shared RHI GPU contract runs on Mac Vulkan and is registered for Windows WARP;
the actual native Xbox device also checks automatic/custom target routing.
The common RHI and GL adapters use one bounded present-timing recorder. It counts
only accepted API presentations, separating guest/interpolated/other kinds and
retaining skipped/failed counts. Failed replay does not advance HLE present totals
or feed stale timing into its scheduler. These are CPU API-call timings, not
scanout measurements.
The common Vulkan game passes programmed native settings, resize and fullscreen
acceptance. The completed native launcher is integrated with the renderer on
`codex/shared-native-integration`; the command launcher also selects the verified
GPU loader from the build manifest. Combined GL/Vulkan suites, native launcher
lifecycle and a bounded first-mission check pass at `ad9cb0f`; see
[integration evidence and limits](../project/STATUS.md#integrated-host-validation-and-launcher-cleanup-fix-2026-10-19).
Windows CI found a launcher cleanup casing regression, fixed in shared code
at `c553f9a`; [hosted revalidation](https://github.com/TheDoffman/timesplitters-future-perfect-pc/actions/runs/39311742707) passes on both hosts. The subsequent screen-copy contract and bounded Khallos
check add acceptance coverage; defaults now select the common implementation
on supported hosts, with the remaining visual gaps explicitly retained.
Keep shared build pins; do not copy another OS's build tree.

Then migrate remaining shader/resource behavior incrementally, using shared
asset-free contracts plus bounded scene checks. Cube/volume textures, movie
planes, framebuffer effects and advanced combiners need explicit acceptance;
first-menu success cannot certify them. Khallos sky fidelity, complete campaign
coverage and real texture-pack scenes remain separate
validation gaps. Networking and launcher distribution also remain genuine host
integration work, tracked in the backlog.

## Local implementation milestones

| Focused branch | Dependency | Result |
| --- | --- | --- |
| `codex/shared-platform-baseline` | main `5131d29` plus native stack `f562e14` | `bee4ed9`: merged native build/regression baseline and backlog reconciliation |
| `codex/shared-menu-rendering` | `bee4ed9` | `dde016d`: one menu shader algorithm and GPU contract for both APIs |
| `codex/shared-debug-settings` | `dde016d` | `780346f`: one debug setting definition list, UI/persistence consumers and capability checks |
| `codex/shared-feature-contract` | `780346f` | `1a650b5`: shared-by-default rules and this handoff |
| `codex/shared-renderer-host` | `1a650b5` | `e382e50`: common host interface, SDL adapter and both-host core compile contract |
| `codex/shared-vulkan-mac` | `e382e50` | `4d23a6d`: native common Vulkan backend, pinned SDK and asset-free RHI GPU contract |
| `codex/shared-native-renderer` | `4d23a6d` | `2c63fbf`: common Xbox frontend in an opt-in native game, text/window adapters and shader compatibility fixes |
| `codex/shared-interpolation-depth` | `2c63fbf` | `3e78f7c`: Shared replay depth/stencil isolation, target restoration and GPU/fault-injection contracts |
| `codex/shared-present-metrics` | `3e78f7c` | Shared API-acceptance counts, frame timing and interpolation failure handling |
| `codex/shared-platform-validation` | `a0c8a1a` | `083e57c`: pinned SDK download and Windows test-harness fixes; both hosted jobs and GPU contracts pass |
| `codex/shared-native-window-acceptance` | `37b4af9` plus validation fixes `083e57c` | One native settings/host fixture for both renderers; common Vulkan settings and window transitions pass |
| `codex/shared-renderer-default` | `42462f0` | `a4261ee`: common frontend selected on SDK-supported hosts; both hosted jobs pass, GL fallback retained |
| `codex/shared-screen-copy-contract` | `c553f9a` | Common rectangle clipping and production screen-copy GPU contract; local Mac and both hosted GPU contracts pass at `42462f0` |
| `codex/shared-texture-contract` | `296f970` | `f2c2261`: both hosted lower-level and Xbox-factory cube/volume contracts pass |

The complete ancestor stack through `a0c8a1a` is published on
`codex/shared-present-metrics`, with `codex/shared-platform-validation` also
published at `083e57c`, and `codex/shared-native-window-acceptance` published
through `296f970`, and `codex/shared-texture-contract` published at `4419205`;
the other milestone branch refs remain local.
The native window acceptance work depends on these validation fixes. Nothing has been
merged into main or verified on the other physical computer. Hosted CI results,
measured scope and unavailable checks are recorded in `docs/project/STATUS.md`.

The normal shared-renderer build policy and all selected host suites pass at
`a4261ee` in [run 39314716127](https://github.com/TheDoffman/timesplitters-future-perfect-pc/actions/runs/39314716127).
The complete integration stack is published on `codex/shared-renderer-default`.
Merge/review that branch before expecting the main checkout on another device to
use this architecture; no merge into main has been performed. New common code
still needs both-host checks, and genuine adapter exceptions stay in the backlog.

## Launcher changes that reach both builds

Edit the shared files under `tools/launcher` for ordinary launcher UI work:
`Launcher.xaml` defines Home, and `SettingsPage.cs`, `WhatsNewPage.cs`,
`GameVersionsPage.cs`, and `MaintenancePage.cs` define the other screens and their
interactions. Both builds compile every `*Page.cs` in this directory automatically.
`PageTheme.cs` defines cards and headings; `PageTheme.xaml`/`TabsTheme.xaml` contain
theme values. Mac generates its home/theme syntax at build time, including shared
button states and expander geometry. Do not edit the ignored generated AXAML.

The Mac `NativeNews`, `NativeVersions` and `NativeMaintenance` classes connect
shared pages to the native window's lifetime, clipboard, file dialogs and process
operations; they must not add alternate page layouts. `Ui.cs` contains framework
operations such as input capture, visibility, tooltips and scroll forwarding.
Rich-text rendering and native control templates remain framework adapters.
OS-specific startup/process integrations still need the corresponding host work;
editing a Windows-only adapter cannot automatically implement a Mac OS API.

Use the same shared models for feature rules and the same shared pages for buttons,
labels, spacing and page state. A new host operation belongs behind an adapter.
Build with `tools/build/build-launcher.ps1 -Test` on Windows and
`tools/build/build-mac-launcher.py --test` on Mac. Both execute
`tests/launcher/launcher_shared_ui.cs` against their actual UI framework. The layout/theme
propagation tests also mutate a disposable source to verify that a single label,
margin, button-padding, corner-radius or disabled-state edit reaches the Mac output.
The release workflow runs both builds before publishing one release's packages.
The Windows two-executable package and existing updater size limits are retained.

API-free Windows/Mac build and regression checks are described in
[launcher compatibility](../release/launcher-compatibility.md). Both host tests remain
required evidence; CI does not generate missing OS adapters or replace visual review.

Issue #105 uses shared prompt layout scopes (`pc_text_read_scope.h` and shared
guest hooks) plus the shared runtime late-frame patch. Existing Windows/Darwin
VM adapters supply validation; no second Mac feature implementation is intended.
Windows weapon-set acceptance measured 53.8-54.3 FPS against the original ~20,
with before/back-out around 59. Mac build/runtime acceptance remains pending
under MAC-002; shared source and Windows checks do not verify the Mac host.
