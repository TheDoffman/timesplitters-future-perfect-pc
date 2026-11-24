# Windows Vulkan versus D3D11 stutter follow-up (2026-11-23)

The [focused gameplay follow-up](renderer-frame-pacing-focused.md) records the
2026-11-24 physical144Hz tests and supersedes the pending-focus state below.

No Vulkan-specific frame-pacing regression was reproduced in these bounded
Windows tests. This does not resolve reports from other hardware, settings,
levels or longer sessions. No renderer defaults or production code changed.

## Setup

Windows 11 Pro 26300, RTX 4070 SUPER, NVIDIA driver 617.42.
The earlier adapter-level query returned 144 Hz. Follow-up active-display
enumeration proves the attached primary display (DISPLAY2) is 1920x1080
at 60 Hz; inactive adapter outputs are not the active screen. No monitor
mode was changed. This explains why 143 submissions cannot yield 144 visible
refreshes in this environment.
One frozen executable for every run, SHA256
`7e43aabb7168183989aeda9c3092df4e5d0e69c0de21b8d8b2ee50e95426482b`.
Compatibility fingerprint
`82a7bd3213756e1b1bde0d73f8c80f614523631048931c4c70189ec7931e8905`
is embedded in the executable; all 87 normalized source/graphics/version
manifest entries checked against the tested source matched. Investigation
checkout base: `c70c3a4c20ac80a0807eb5d3609c5b49bbefc0cf`.

Render target 1280x720, windowed client 640x480 (fullscreen clients 1920x1080); MSAA1, post-AA off,
anisotropy Original, AO off, V-Sync off, shader cache on, validation off.
Synthetic Test Lab profiles and backend-private application cache directories;
personal saves/settings and global driver/OS caches were untouched. Cold means
empty application caches, not a cold NVIDIA driver cache. Warm runs reuse the
appropriate backend cache. Vulkan warm pipeline-cache reads were observed.

Story uses the same scheduled load, movement and firing route. Vietnam uses
the same ten-bot match setup and movement/fire/respawn schedule; AI matches
are not deterministic. Scene, pause and cinematic state checkpoints verify
active gameplay. Loading/menu frames are excluded from the table.

All measured gameplay checkpoints are unfocused. Windows Security owns the
foreground and the owned-window focus attempt cannot restore it. These runs
verify scene progression and background cadence, not normal foreground
player-facing presentation. The earlier conclusion must be read with this
restriction. Foreground acceptance remains pending user completion of that
dialog; no attempt was made to dismiss or interact with it.

## Successful-present intervals

Intervals measure host time between successful game presents, not input latency
or guaranteed physical scanout. p99 means 99% of measured intervals were at or
below that value. No screenshots, Vulkan validation or CPU stack tracing ran
during these windows. PresentMon was enabled for all matched warm runs.

| Case | FPS target | Window s | Frames | p99 ms | Max ms | >1.5x budget | >25 ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| d3d11-vietnam | 60 | 65.46 | 3926 | 17.374 | 50.044 | 1 | 1 |
| vulkan-vietnam | 60 | 65.43 | 3926 | 17.426 | 21.344 | 0 | 0 |
| d3d11-cold | 60 | 38.61 | 2316 | 17.218 | 18.512 | 0 | 0 |
| d3d11-warm1 | 60 | 38.60 | 2316 | 17.300 | 17.700 | 0 | 0 |
| d3d11-warm2 | 60 | 38.64 | 2318 | 17.235 | 17.516 | 0 | 0 |
| vulkan-cold | 60 | 38.61 | 2317 | 17.236 | 20.663 | 0 | 0 |
| vulkan-warm1 | 60 | 38.62 | 2317 | 17.346 | 18.492 | 0 | 0 |
| vulkan-warm2 | 60 | 38.61 | 2316 | 17.268 | 17.592 | 0 | 0 |
| d3d11-144 | 144 | 38.61 | 2317 | 16.937 | 17.066 | 2317 | 0 |
| d3d11-full144 | 144 | 38.60 | 5558 | 7.608 | 8.925 | 0 | 0 |
| vulkan-144 | 144 | 38.60 | 5529 | 8.083 | 11.932 | 2 | 0 |
| vulkan-full144 | 144 | 38.62 | 5490 | 8.409 | 11.151 | 5 | 0 |

## Interpretation and limits

At Original60, both Story warm repeats have zero intervals over 25 ms.
Vietnam Vulkan has none; D3D11 has one 50.044 ms host interval. That isolated
outlier does not establish a backend defect or prove Vulkan is better.

With a native144 target, Vulkan averages 143.2 successful presents/s
(p99 8.083 ms, max 11.932 ms), while D3D11 remains at 60.0/s
(p99 16.937 ms, max 17.066 ms). D3D11 spends 10.475 ms on average
inside Present versus Vulkan 0.190 ms. Both logs confirm native144 was
requested. This is not a valid equal-throughput 144-FPS comparison;
the budget-overrun count in the table uses the requested target.
The existing D3D11 windowed path only enables tearing/VRR when fullscreen,
and the two backends use different presentation paths here. The source
contract is a candidate explanation for the 60-Hz limit, not a proven cause.
PresentMon also reports only ~60 displayed changes/s for both runs despite
the adapter-level query reporting 144 Hz. The active screen is actually60 Hz.
Thus 143 Vulkan submissions must not be
described as verified 144-Hz visible output. A fullscreen/high-refresh
comparison with a genuinely high-refresh active screen and foreground
ownership remains a separate check.

The fullscreen background follow-up removes the D3D11 60-submission limit:
D3D11 p99/max is7.608/8.925 ms; Vulkan is8.409/11.151 ms. This supports
a presentation-path explanation for the windowed difference rather than a
GPU-throughput bottleneck. Neither run has a frame over25 ms. Focus remains
false throughout; these are not foreground acceptance. The D3D11 fullscreen
run has API timing only (the ETW helper did not start because the first
report record was focus metadata, not a PID checkpoint).

Loading hitches exist on both backends: in the same 39-52 s first-load window,
Vulkan cold reaches 215.998 ms (11 frames >25 ms, 4 >50 ms); D3D11 cold
reaches 307.940 ms (10 >25 ms, 5 >50 ms). These are separate from gameplay.
They have not been attributed specifically to shader compilation.

PresentMon 2.6.0 captures are filtered to the owned game PID and QPC-aligned
to gameplay markers. Full warm-run row coverage is checked. However D3D11
reports an independent-flip swapchain while Vulkan reports a composed GDI
path with swapchain address zero. Its display fields are supplementary
evidence, not equivalent direct-scanout proof. Cold Vulkan has no ETW capture;
cold D3D11 has only a partial diagnostic capture and is excluded from ETW
comparisons. An initial ETW cleanup timeout affected one wrapper exit after
the game had passed; owned-session cleanup was corrected for subsequent runs.

This is not an uncapped throughput comparison, a long-session soak, or
cross-vendor acceptance. Fullscreen, higher resolutions/quality, Enhanced
texture packs, online matches, AMD/Intel and other drivers remain untested
in this follow-up (fullscreen background runs are included in the table).
No conclusion about Linux/macOS follows from these runs.
Next discriminating evidence is the affected GPU/driver, map, FPS target,
display mode and quality/texture settings, followed by a matched replay on
that configuration. Capture CPU/texture/shader diagnostics only after a
repeatable spike is located so instrumentation does not pollute the baseline.

## Local reproduction and evidence

The isolated helper `build/frame_pacing_probe.py` delegates inputs and scene
checks to tracked `tests/gameplay/stability_transitions_game.py`. Arguments:
`RUN_NAME vulkan|d3d11 story-local-load|crowd-vietnam [0|120|144]`.
Zero selects Original60. Each run has a fresh synthetic profile, while each
backend reuses its own cache after its cold run. Native144 is verified by
`[NATIVE-TIMING] 144 fresh updates/s target` and observed present cadence.

Ignored local artifacts: `private/test-saves/pacing-*` contain checkpoints,
actual renderer logs, raw successful-present CSVs and owned PresentMon CSVs;
`logs/frame-pacing-results.json` contains per-case statistics and coverage;
`logs/frame-pacing-provenance.json` records binary/dependency hashes and
source checks. `build/analyze_frame_pacing.py` computes linearly interpolated
quantiles after aligning QPC timestamps to active gameplay markers. Story
window: local-loading through local-fire (~38.6 s); Vietnam: combat-one
through combat-six (~65.4 s). Evidence remains local; no game assets or saves
are included in this report.

## Test-harness correction

`tests/gameplay/stability_transitions_game.py` now records the current mode
of the monitor containing the owned game window in `window.active_display`.
It uses MonitorFromWindow, GetMonitorInfoW and EnumDisplaySettingsW rather
than adapter-level refresh counters. Null means the query failed; it never
invents a rate or changes the desktop mode. Fullscreen Story routes use the
existing owned-window focus action. Opt-in `--require-foreground` rejects
unfocused command checkpoints after retaining their evidence and performs
the normal owned-child cleanup. Use this flag for foreground performance
acceptance; background diagnostic scenarios remain possible without it.
