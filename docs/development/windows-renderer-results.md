# Windows Vulkan versus D3D11: measured results

Validation is ongoing. These are the current-host performance results; the
[acceptance matrix](windows-renderer-validation.md) owns remaining quality,
level, stability and save/load gates.

Host: Windows 11 Pro26300 x64, RTX4070SUPER, driver617.42,24logicalCPU.
Game source8b3e6fca, native SHA256
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
This is an observed cost, not classified as a leak. Sustained memory-growth
acceptance remains open until the longer soak is reviewed.
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
