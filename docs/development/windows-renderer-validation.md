# Windows renderer acceptance

This is the acceptance matrix for making Vulkan the Windows default while
retaining Direct3D 11. It is an active investigation, not a declaration of full
game or cross-vendor compatibility. Compare the same executable, original
60-Hz gameplay timing, settings, profile and input sequence on both backends.

The available host is Windows 11 Pro 10.0.26300 x64, NVIDIA RTX 4070 SUPER,
driver 617.42. AMD, Intel and other driver versions are **unverified**. Do not
install another driver over this host simply to expand the matrix.

## Gates and current evidence

| Gate | Acceptance evidence required | Current state |
| --- | --- | --- |
| L1 Launcher selection | Save D3D11, run; save Vulkan, run; reopening preserves preference and logs identify only the requested backend | Passed, source f4b615a9, two 120-second first-mission runs |
| L2 Recovery | Default/explicit Vulkan missing-driver, missing-DXC and no-device startup recover to D3D11; explicit D3D11 ownership remains correct | Existing eight-case production RHI contract passed; retain executable hashes |
| P1 Frame pacing | At least three matched uncaptured runs per backend; report median/p95/p99 intervals, >25/50-ms frames, CPU and memory; loading excluded | Earlier single-pair CPU-present comparison passed; repetitions and resource comparison pending |
| P2 Cold/warm loading | Empty per-test caches, then same-cache repeat; record window/scene readiness, cache files and resource use; no inherited personal caches | Two cold/warm sets pass; second set includes dedicated GPU counters; precise loading/stutter comparisons pending |
| V1 Common rendering | Reviewed paired level captures identifying the actual shadow, blend/transparency, reflection/screen-copy, filtering, depth and AA features exercised | Opening-mission world and weapon reviewed; targeted feature coverage pending |
| V2 Resource contracts | Production HLSL, signed texture mapping, texture formats, depth replay, screen copy, MSAA, SSAO and visibility checks on both hardware backends | Existing production RHI fixture passed; this is separate from level appearance |
| D1 Display | Windowed/fullscreen transitions, window resize, supported startup resolutions, repeated switching; requested backend preserved; no failed presents | 1280x720 settings / windowed startup passed; remaining transitions and resolutions pending |
| Q1 Quality settings | Off/FXAA/SMAA, supported MSAA and filtering values selected and exercised on both backends; confirm effective runtime settings and reviewed captures | Existing RHI MSAA resource checks passed; launcher/game setting matrix pending |
| S1 Stability | Bounded extended gameplay across representative Story and Arcade levels; no crash, hang, render error, unexplained resource growth or unintended fallback | First Story mission for 120 seconds each passed; extended and Arcade coverage pending |
| G1 Gameplay | Compare game-clock cadence, movement, firearm/ammo, input, pause/resume and save/load from copied profiles | First-mission movement, firearm, 60-Hz ticks passed; explicit save/load and broader comparisons pending |
| H1 Hardware | Run the same matrix on available GPUs; list model/driver and actual tested scope | NVIDIA available; AMD/Intel and other drivers unverified, follow-up below |

Performance gates flag regressions for investigation rather than excusing visual
or gameplay faults. Investigate repeatable Vulkan median interval regressions
above 10%, p95/p99 regressions above 20%, new long stalls, or sustained memory/CPU
growth against matched D3D11. Resolve or explain measurements before acceptance.
Report variability and do not classify a single outlier as an established cause.
Process CPU time and CPU successful-present intervals do not measure GPU
completion or monitor scanout. VRAM must be labelled unavailable when no usable
per-process GPU counter is available.

Visual acceptance requires inspecting the relevant feature, not simply observing
that the level loaded. Use representative outdoor/indoor Story and Arcade scenes,
including surfaces/effects that actually exercise each category. Record exact
level, viewpoint, settings and reviewed capture paths. Existing common bugs must
be distinguished from regressions introduced by backend switching.

## Reproducible launcher runs

Build the normal game and launcher first. The supported US XBE and local unlocked
Test Lab fixture are required. The runner validates the XBE hash, holds an
exclusive sandbox lock, uses hard links for immutable game files and disposable
copies for saves/settings, and recycles only the six known virtual disks.
Application cache paths are isolated through APPDATA/LOCALAPPDATA; vendor/OS
driver caches are not cleared. No backend variable is
injected. The unchanged SettingsPage and LauncherWindow launch path select the
backend. Preview mode only suppresses launcher networking.

```powershell
.venv/Scripts/python.exe tools/testing/test-launcher-renderers-windows.py --label baseline-cold-warm-01 --warm
.venv/Scripts/python.exe tools/testing/test-launcher-renderers-windows.py --label baseline-visual-01 --capture
```

Labels are unique evidence/cache identities. Reusing an existing successful
case is rejected, so a failed rerun cannot accidentally inherit a stale pass.
The JSON report begins with passed=false, records source and executable/harness
hashes, and includes per-case results. Visual capture runs are separate from
measurement runs. The report records host/GPU/driver metadata and executable
hashes; per-process dedicated GPU memory is nullable if counters are unavailable.
The initial baseline set predates that automatic GPU metadata/counter extension;
its measured host and exact source/harness hashes are recorded in STATUS.

The first scenario is the opening Story mission; the runner currently does not
claim that this scenario covers Arcade or every visual category. Extend scenario
coverage as required by the pending gates. Root evidence lives under
logs/renderer-validation; it remains local because it contains game captures.

## Additional hardware follow-up

On AMD, Intel or another driver, use the same source, verified native executable
and settings where compatible; record any necessary rebuild as a separate
comparison. Run the launcher matrix, production RHI tests and targeted visuals.
Include adapter/driver identifiers, cache state, timing distributions, resources,
exact levels and errors. Do not infer hardware acceptance from compilation,
Windows WARP, another GPU, or the shared Vulkan backend on Mac/Linux.

The current work remains on feature/windows-vulkan because it depends on its
unmerged backend/default and launcher-setting changes. No merge or release is
authorized by this validation goal.

Display and effective-quality checks use the same launcher path:

```powershell
.venv/Scripts/python.exe tools/testing/test-launcher-renderers-windows.py --label display-fixed-01 --display-cycle --capture-times 11,15,19,23,38,66,71,80,104,112
.venv/Scripts/python.exe tools/testing/test-launcher-renderers-windows.py --label display-auto-01 --width 1 --height 1 --display-cycle --capture
```

These perturb timing and are labelled visual runs. Window/fullscreen state is
read from the actual window, not the saved checkbox. Auto must log a native scene
target matching the resized client. Fixed render resolution intentionally differs
from presentation-window size; validate the scene log, not window bounds alone.
The hardware result for these new scenarios is pending until their reports pass.
