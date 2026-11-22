# Windows Vulkan versus D3D11: measured results

Available-host acceptance is complete: the tested Windows Vulkan default passes
the [acceptance matrix](windows-renderer-validation.md), with D3D11 retained as
the launcher option and automatic startup fallback. Frame pacing is comparable
at the 60 FPS cap; Vulkan's measured allocation footprint is higher.
This is bounded NVIDIA/Windows acceptance, not whole-game or cross-vendor certification.

Host: Windows 11 Pro 26300 x64, RTX 4070 SUPER, driver 617.42, 24 logical CPUs.
Game build source 8b3e6fca, native SHA256
`a8a26b31dfa9bc4174bdfe6fd4d157f3e01b3fefe17e269cb51b809db80a7cb1`.
Launcher SHA256
`a4854d60b0b5262512ad41f07f543293638f4b637438df9fd98b6acaade4c690`.
Cohort source10efb9ef; pinned standalone PresentMon2.6.0. All six runs use the
actual launcher setting/Save/LaunchGame path,1920x1080 fullscreen, Original60,
MSAA1/post-AAOff/filtering1, identical Test Lab copies and scheduled actions.
One cold and two warm application-cache runs per backend; vendor/OS caches
remain untouched. Capture and Vulkan validation are disabled in timing runs.

## Steady gameplay,85-115seconds

Intervals are PID-filtered ETW records aligned to QPC samples in active gameplay.
Each case has one observed swapchain and enough real displayed frames; no
interpolation or other-process frames are counted.

| Backend/cache | FPS | Submitted p95/p99 ms | Displayed p95/p99 ms | GPU busy mean ms | Display latency p95 ms | >25ms submitted/displayed |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| d3d11-cold | 59.996 | 17.028 / 17.127 | 17.948 / 19.008 | 3.959 | 4.432 | 0 / 0 |
| d3d11-warm-1 | 60.008 | 17.045 / 17.160 | 18.230 / 19.234 | 3.988 | 4.760 | 0 / 0 |
| d3d11-warm-2 | 60.026 | 17.021 / 17.113 | 18.139 / 19.312 | 3.999 | 4.874 | 0 / 0 |
| vulkan-cold | 59.992 | 17.077 / 17.198 | 18.164 / 19.358 | 4.299 | 5.256 | 0 / 0 |
| vulkan-warm-1 | 60.000 | 17.076 / 17.186 | 18.135 / 19.305 | 4.294 | 5.320 | 0 / 0 |
| vulkan-warm-2 | 59.999 | 17.066 / 17.216 | 17.605 / 19.058 | 4.265 | 5.081 | 0 / 0 |

All relative interval gates pass: median limit+10%, p95/p99 limit+20%, evaluated
using the median of each backend's three run statistics. This is a current-host,
opening-mission result, not a full-game or cross-vendor claim.

| Statistic | Vulkan/D3D11 ratio | Limit |
| --- | ---: | ---: |
| submitted_median_ms | 0.9999 | 1.10 |
| submitted_p95_ms | 1.0028 | 1.20 |
| submitted_p99_ms | 1.0042 | 1.20 |
| displayed_median_ms | 1.0001 | 1.10 |
| displayed_p95_ms | 0.9998 | 1.20 |
| displayed_p99_ms | 1.0037 | 1.20 |

Display latency is the reported submitted-frame/display relationship, not an
input-to-photon measurement. GPU busy is PresentMon's per-frame busy duration,
not board power or total device utilization. The60FPS cap limits conclusions
about maximum uncapped throughput.

## Resources and cache behavior

CPU core equivalents are process CPU time divided by measured wall time; memory
values are peak observations in the same active gameplay window.

| Backend/cache | CPU cores | Private MiB | Resident MiB | Dedicated GPU MiB | First present observed s | Cache files before/after |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| d3d11-cold | 0.637 | 271.73 | 231.71 | 74.11 | 1.808 | 0 / 40 |
| d3d11-warm-1 | 0.586 | 273.36 | 232.41 | 74.75 | 0.523 | 40 / 41 |
| d3d11-warm-2 | 0.560 | 273.21 | 227.12 | 74.75 | 0.566 | 41 / 41 |
| vulkan-cold | 0.624 | 382.19 | 238.30 | 206.85 | 0.941 | 0 / 41 |
| vulkan-warm-1 | 0.602 | 379.26 | 237.27 | 206.85 | 0.716 | 41 / 42 |
| vulkan-warm-2 | 0.596 | 379.49 | 232.52 | 206.85 | 0.697 | 42 / 42 |

Vulkan has a higher allocated/committed footprint and slightly higher per-frame
GPU work/latency in this cohort, while interval cadence remains comparable.
This is an observed cost, not classified as a leak. The bounded five-minute
growth checks below pass; slower leaks and other levels remain outside that result.
Warm cases have populated application caches and logged disk-cache reads.
First-present times are polled readiness observations, not isolated compiler
benchmarks. Fixed menu input timing also masks small loading-time differences.

## Pre-gameplay frame gaps

These records precede85seconds and include menus, asset loading, transitions and
cinematics. Large gaps here cannot be attributed solely to shader compilation.
They are reported separately rather than included in steady gameplay percentiles.

The largest inspected gaps occur during startup/menus or scene transitions near
36 and47seconds, before player/weapon readiness near72seconds. Both backends have
such gaps. These traces do not isolate shader compilation from other loading work.

| Backend/cache | Observed records | >25ms | >50ms | Maximum interval ms |
| --- | ---: | ---: | ---: | ---: |
| d3d11-cold | 4931 | 15 | 6 | 309.289 |
| d3d11-warm-1 | 5047 | 15 | 2 | 210.106 |
| d3d11-warm-2 | 5042 | 13 | 1 | 98.058 |
| vulkan-cold | 4983 | 18 | 6 | 367.730 |
| vulkan-warm-1 | 5026 | 15 | 4 | 323.233 |
| vulkan-warm-2 | 5026 | 16 | 4 | 307.404 |

Raw local evidence: logs/renderer-validation/pacing-etw-01/report.json, per-case
verified.json, presentmon.csv/log, samples and backend logs. Fixture signatures
remain unchanged, test disks are recycled, and each case exits0 with its requested
backend. Exact source/tool hashes are retained in the root report. No merge or
release publication is authorized by this validation work.

## Quality runs: visual acceptance open

Four paired120second runs complete with correct requested backend, expected
effective settings, active gameplay and clean exit:1920x1080/MSAA2/FXAA/filter4;
2560x1440/MSAA4/SMAA/filter16;3840x2160/MSAA8/Off/filter8; and1024x768/MSAA1/Off/
filter2 with fullscreen/resize cycling. Local labels are quality-fxaa-01,
quality-smaa-01, quality-msaa8-01 and quality-window-01.

Machine checks do not establish visual correctness. Reviewed Vulkan fullscreen
GDI CopyFromScreen captures in quality-fxaa-01 are identical black images with
initial overlay/footer despite advancing game/present logs. A focused same-build
1920x1080/MSAA2/FXAA diagnostic now shows correct world/weapon/HUD/fire through
both GPU readback and DXGI desktop duplication. Windows Graphics Capture timed
out and supplies no evidence. This identifies the stale GDI capture limitation
in that configuration; it does not retroactively approve every quality case.
Actual-launcher quality recapture with the DXGI helper and reviewed pairs remains
in progress. The paired quality-fxaa-dxgi-03 runs now complete cleanly: reviewed
80/112second DXGI images show consistent world, fire, weapon, transparent tutorial
panel, HUD and ammo16->12. Viewpoints/animation differ slightly, so these are
qualitative comparisons, not pixel-identical replay evidence. Both backends have
unusable black11second captures; these are excluded from appearance evidence.
A three-arrival DXGI probe produces a current image. The helper now drains two
initial arrivals before copying and rejects masked or pointer-only output.
Remaining quality cases use that stricter helper.
Existing stable windowed captures remain narrower evidence. Additional
levels, targeted effects, sustained play and actual guest save/load remain open.
AMD/Intel and native Mac/Linux are unverified.

Diagnostic artifacts: logs/fullscreen-duplication-diagnostic-01.png and
private/test-saves/high-refresh/60-fullscreen-gpu-fxaa-01-vulkan-frame*.bmp.
That focused runner bypasses the launcher and terminates at its deadline; it is
capture diagnosis, not clean-exit/launcher acceptance. The modified diagnostic
source is logs/run-fullscreen-gpu-diagnostic.py, preserving the original runner
and overriding only resolution and isolated graphics settings.

The subsequent quality-smaa-dxgi-01 and quality-msaa8-dxgi-01 pairs also complete
cleanly. Reviewed paired112second desktop images show consistent textured world,
weapon, NPC, HUD and ammo12, with working scene occlusion and no visible missing
surfaces. Effective logs confirm2560x1440/MSAA4/SMAA and3840x2160/MSAA8/Off,
respectively. The physical capture is1920x1080: higher scene resolutions are
presented at that monitor size, so this is not native4K monitor acceptance.
Their38second star-field transition appears on both backends and is not evidence
of a Vulkan-specific blank scene. Exact shadow/reflection/depth encounters still
need targeted coverage; the opening canyon alone cannot prove those categories.

quality-window-dxgi-01 also passes both backends. Its fixed1024x768 scene survives
client640x480 ->fullscreen1920x1080 ->640x480 ->1008x729 ->640x480 without failed
presents or a changed renderer. Reviewed112second outer-window captures show
consistent scene, weapon, crosshair/HUD and ammo12. The earlier rebuilt Auto case
separately verifies that native scene targets follow the actual resized client.

The first Scotland attempt, story-scotland-dxgi-01, remains failed: visible
Scotland gameplay uses scene4 while the original acceptance harness sampled only
scene2. The corrected scenario explicitly declares4, and all world/measurement
gates now use that validated expectation. story-scotland-dxgi-02 is a fresh paired
run; no old failure is converted into a pass. This was an observer assumption,
not evidence of a renderer regression.

Both story-scotland-dxgi-02 cases now pass on the same game executable. Reviewed
80/112second images show the cliff/wooden landing, revolver, bullet decal and HUD
with ammo8->4. The late view is close to a wall on both backends, not a broad
outdoor-feature survey. Resource/tick sampling uses actual scene4 and observes
60.014Hz D3D11 /60.007Hz Vulkan in the controlled window. Its wildcard weapon-ID
expectation is a world/input gate; ammunition evidence comes from reviewed images.

## Cross-renderer guest save/load passed

The original guest Controls Accept/Cancel path passed Vulkan ->D3D11 ->Vulkan and
D3D11 ->Vulkan ->D3D11 using separate disposable Test Lab copies. Each sequence
starts from the same signed seed. Accept changes exactly profile bodybyte6 from
255 to254 (Inverse Look ON ->OFF), with a valid updated HMAC; both directions
produce SHA256 `95fdcc0f058e2789dadc9f4f5f41bd570a2eb3b94b1d3ff890e5baae8dd985fc`.
The next renderer loads OFF into its original Controls draft. Creating an ON
draft and pressing Cancel preserves that saved file and OFF after another
process restart. All six requested child backends actually present, with no
fallback/crash, and the bounded session runner closes each child cleanly.

Local verification files are private/test-saves/renderer-save-vk-dd-vk-03/
verification.json and private/test-saves/renderer-save-dd-vk-dd/verification.json.
This is direct-child guest persistence acceptance, separate from launcher choice
persistence. Failed earlier probes remain failed: one used obsolete Controls
navigation; another correctly saved but assumed an obsolete two-slot fixture.
The updated test verifies the changed draft before acting and selects the actual
canonical saved slot. Original fixture signatures/bytes remain preserved.

## Five-minute movement and pause checks passed

story-soak-dxgi-02 runs each backend for300seconds through the actual launcher,
same1920x1080/Original60 settings and copied seed. Walking continues through294s;
Escape152s/E154s exercises the original pause menu. Both cases observe the pause
state and resumed gameplay, at least86 active last30second samples, movement,
clean exit, no failed presents/fallback/render errors and bounded resource growth.
Reviewed200/295second desktop captures show consistent world/NPC/weapon/HUD/ammo.

| Backend | Last30s guest ticks/s | Median private growth MiB | Median dedicated GPU growth MiB | Handle change |
| --- | ---: | ---: | ---: | ---: |
| D3D11 | 59.966 | 14.08 | 10.16 | -8 |
| Vulkan | 59.986 | 17.40 | 8.45 | -10 |

Growth compares the85-115second and final30second windows. Limits are128MiB for
private/dedicated GPU allocation and32 additional handles; these are regression
alarms, not proof against slower leaks or sustained combat. Capture runs supply
functional/stability evidence and are not substituted for uncaptured ETW timing.

The earlier story-soak-dxgi-01 report remains failed: its observer recorded time
before blocking input holds, producing58.945Hz from stale seconds despite
59.977Hz using actual QPC elapsed time. The corrected harness samples seconds,
QPC and guest state after blocking work. Its fresh runs have under10ms variation
in seconds/QPC offset, consistent with the reported0.01second rounding. The
strict59-61Hz acceptance gate was retained.

## Actual-launcher compiler recovery passed

launcher-dxc-recovery-01 saves Vulkan through the normal settings UI, then launches
with a valid test-only compiler DLL missing DxcCreateInstance in its disposable
Bin. The normal game logs the unavailable-compiler diagnostic and recovery,
presents exclusively with D3D11, performs the same first-mission movement/fire
checks and exits0. The saved renderer remains Vulkan. Other backend/shader/present
checks remain strict. This tests compiler unavailability, not removal of a driver.

Cleanup restores the original compiler hard link; its SHA256 is
`5888d3e590f5bfd8743484e7f31313996734adec5741506271a5c6368cbe8ba9`.
Source/build compiler DLLs and installed drivers are unchanged. Existing separate
production RHI contracts cover missing-driver/no-device recovery and ownership;
those fixtures are distinct from this real-game launcher check.

## Arcade and targeted visual review passed

arcade-venice-dxgi-01 passes both actual-launcher paths at the same game hash.
Reviewed 33s captures confirm Venice, and 47s captures confirm no score limit and
recommended bots. The 80/112s views show outdoor/indoor stone geometry, lamps,
weapon/HUD, damage blending and ammunition changes. Both runs move, advance the
game clock at approximately 60 Hz, present with the requested backend and exit0.
Spawn positions, bot actions and viewpoints differ; this is qualitative review.

The existing surface-native route additionally runs on each explicitly selected
backend with Original60, classic textures and the same 1280x720 scene. Reviewed
private/test-saves/renderer-water-dd and renderer-water-vk turn-right.png and
path-heading.png show consistent Scotland ocean/environment shading, jetty,
foreground rock occlusion, weapon/HUD and splash blending. Splash/animation phase
varies. Both direct-child diagnostic sessions close cleanly with the same game
hash, actual requested presents and no fallback/crash/shader error. These GPU
readbacks support targeted feature review; they are separate from launcher and
uncaptured performance acceptance.

Final production RHI hardware checks pass all eight cases in
logs/renderer-validation/rhi-final-02/verified.json, fixture SHA256
`ae3daec556d9a627d0029c971689b27bc76943da33c96f91fa3cb74cfa2ce5c3`.
Default/explicit Vulkan run with validation active and no diagnostics, using the
existing SDK layer directory external/vulkan-sdk/windows/1.4.363.0/Bin.
The earlier final wrapper attempt is rejected because validation was inactive;
it is not substituted for this validated rerun. Pixel/resource assertions cover
production HLSL sampling, blend/stencil/scissor, full/partial screen copies,
byte/half/full-float feedback, shared depth, SSAO contact/sky/resize, MSAA1/4
resource/resolve behavior and visibility/cutout/wall occlusion. They complement
reviewed level images rather than certify every shadow/mirror/effect encounter.

Final selected unit/model/fixture checks: 30 passed. --compile-only also passes
for the real-log sharing probe, C# launcher harness, DXGI capture helper and
unavailable-compiler test DLL. Fixture/source files remain preserved; no merge
or release was published. All measured game runs use the native executable hash
above; source/build hashes and per-case provenance remain in local reports.

## Coverage limits and reproduction

Only the RTX 4070 SUPER/617.42 Windows host was available. AMD, Intel, other
drivers, native Mac/Linux, native 4K/8K scanout and other monitor arrangements are
unverified. Follow the matrix's same-executable/settings/profile procedure on each
additional host. Vendor/OS shader caches were not cleared; no input-to-photon or
uncapped-throughput claim is made. The runs cover representative openings and a
five-minute movement/pause sequence, not a full-game playthrough or slower leaks.
Native follow-up uses the [Linux acceptance procedure](linux-acceptance.md) and
[Mac port acceptance backlog](../project/MAC_PORT_BACKLOG.md), with each host's
own native build/data fixture. A Windows or software-renderer pass cannot certify them.

Reproduce the paired extended and Arcade checks after building the normal game
and launcher and preparing the supported local Test Lab fixture:

```powershell
.venv/Scripts/python.exe tools/testing/test-launcher-renderers-windows.py --label my-soak --seconds 300 --scenario-file tests/fixtures/renderer-scenarios/story-opening-soak.json --fullscreen --width 1920 --height 1080 --capture-times 80,112,200,295
.venv/Scripts/python.exe tools/testing/test-launcher-renderers-windows.py --label my-venice --scenario-file tests/fixtures/renderer-scenarios/arcade-venice.json --capture-times 33,47,70,80,112
.venv/Scripts/python.exe tools/testing/test-launcher-renderers-windows.py --label my-recovery --backend vulkan --unavailable-dxc --capture
.venv/Scripts/python.exe tools/testing/test-rhi-windows.py --validation-layer-path external/vulkan-sdk/windows/1.4.363.0/Bin
```

Use fresh labels. Measurement and quality commands are in the matrix; use the
same explicit test-only backend sequence with stability_transitions_game.py for
the surface-native diagnostic route. Keep timing, validation and GPU readback
runs separate. Saved user preferences and personal profiles are not test fixtures.
