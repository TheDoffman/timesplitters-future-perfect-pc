# Focused Vulkan versus D3D11 gameplay (2026-11-24)

The corrected foreground comparison shows similar usual frame intervals,
with a real Vulkan combat hitch recorded separately below. Its cause is not
established, so no renderer optimization or default change is claimed.
This is a bounded NVIDIA result, not a
guarantee for every GPU, driver, scene or session length.

## Controlled conditions

RTX 4070 SUPER, driver617.42, Windows11 Pro26300. The active monitor is now
DISPLAY1,3440x1440 at144Hz, verified from the monitor containing the game
window at checkpoints. This differs from the prior DISPLAY2/1920x1080/60Hz
environment. No desktop mode or driver was changed by the tests.

The same frozen game executable was used for both backends and both days:
SHA256 `7e43aabb7168183989aeda9c3092df4e5d0e69c0de21b8d8b2ee50e95426482b`.
Binary/source provenance is recorded in the [earlier report](renderer-frame-pacing-follow-up.md).
The scene render target remains1280x720; fullscreen client3440x1440 is the
presentation size, not native3440x1440 scene rendering. MSAA1, post-AAOff,
AOOff, anisotropyOriginal, V-SyncOff, cacheOn, Vulkan validationOff.
Each case uses synthetic saves, isolated settings and backend-private warm
application caches. Personal files and global driver caches are untouched.

The runner requires foreground ownership after its initial readiness
checkpoint. Every measured gameplay checkpoint is foreground, visible,
not minimized, in the expected scene, unpaused and outside a cinematic.
The requested renderer is independently checked against actual present logs;
startup recovery to OpenGL is rejected, not counted as Vulkan.

## Measured gameplay

p99 is the interval at or below which99% of samples fall. API intervals
measure successful guest presents; displayed intervals are the PID-filtered
PresentMon2.6.0 observations aligned to QPC scene markers. All listed cases
have full ETW row coverage. Display timing is not input-to-photon latency.

| Case | Target FPS | Seconds | API p99 / max ms | Display p99 / max ms | API / display >25 ms | Focus checkpoints |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| d3d11 Vietnam 10 bots | 60 | 65.30 | 17.397 / 20.989 | 21.057 / 26.650 | 0 / 3 | 29/29 |
| vulkan Vietnam 10 bots | 60 | 65.29 | 17.525 / 58.384 | 21.281 / 58.121 | 3 / 7 | 29/29 |
| vulkan Vietnam 10 bots repeat | 60 | 65.29 | 17.338 / 30.877 | 21.615 / 31.170 | 1 / 12 | 29/29 |
| d3d11 first Story mission | 144 | 38.62 | 7.824 / 9.936 | 10.142 / 15.428 | 0 / 0 | 7/7 |
| vulkan first Story mission | 144 | 38.58 | 8.126 / 10.219 | 10.691 / 15.255 | 0 / 0 | 7/7 |

At144FPS, Vulkan API p99 is3.9% higher than D3D11; displayed p99 is5.4%
higher. Worst API intervals are10.219ms versus9.936ms; worst displayed
intervals are15.255ms versus15.428ms. Neither has an interval exceeding
1.5 times the requested144FPS budget at the API. These small differences
do not identify a blocking Vulkan bottleneck or establish a repeatable
regression. Display modes differ: Vulkan Hardware Legacy Flip, D3D11
Hardware Independent Flip. Both observe approximately143 displayed
changes/s on this144Hz active display.

The first default60 ten-bot Vulkan run has API p99 17.525ms but a58.384ms
outlier and3 API intervals over25ms. ETW confirms a58.121ms display gap
and7 displayed intervals over25ms. Matching D3D11 has API p99 17.397ms,
max20.989ms, and zero API intervals over25ms; max displayed26.650ms.
The Vulkan58ms frame spends0.217ms in Present and records4.177ms GPU busy.
This does not support a58ms GPU-rendering or Present-call bottleneck;
it also does not rule out CPU-side driver/shader work, texture uploads,
guest work or scheduling. Vulkan pipeline-cache data grows by12,264bytes
after the run, which is a possible first-use compilation lead, not timing
attribution. No pipeline compilation root cause is claimed.

The clean Vulkan combat repeat has API p99 17.338ms, max30.877ms and1
API interval over25ms; displayed p99 21.615ms, max31.170ms and12 intervals
over25ms. The58ms event did not repeat. The smaller tail gaps remain
worth investigating, especially the difference in display tails; the
usual API p99 is comparable to D3D11. These are two Vulkan combat samples
and one matched D3D11 sample, not a statistically conclusive backend ranking.

## Separate diagnostic pass

`pacing-vulkan-focused60diag` enables the existing200Hz CPU stack sampler
and texture-stage timestamps. It is not pooled into the clean timing table.
Its API p99/max is17.379/17.962ms with no API interval over25ms; its
largest displayed gap is29.506ms. Thus the58/31ms submission hitches
did not occur under profiling, and samples cannot identify their cause.
The24 recorded combat texture stages are deferred stages, with maximum
CPU duration0.141ms. This is not a measurement of every asynchronous GPU
upload or driver operation and does not clear the entire upload pipeline.
CPU snapshots and texture-stages.csv remain in that isolated case directory.
Further attribution requires a trace containing the actual long submission
gap, or reproduction on the affected map/settings/GPU. No root cause or
production performance fix is claimed from an uneventful diagnostic run.

Loading and startup are excluded from these gameplay windows. Synthetic
AI matches follow the same setup/input schedule but are not deterministic.
No screenshots, per-update native trace or CPU stack sampling run during
the timing windows. No CPU/GPU or shader compilation root cause is inferred
from average FPS. Higher scene resolution, heavier graphics, Enhanced packs,
other maps, long sessions, online matches and AMD/Intel remain unverified.

## Rejected runs and harness correction

The first focused Vulkan attempt (`pacing-vulkan-focused144`) fell back to
OpenGL and is excluded. The newly added focus action minimized/restored the
window during device initialization; Vulkan startup rejects a zero-size
surface. Moving that action after the initial24-second readiness wait
allowed normal Vulkan initialization. A subsequent attempt stopped because
the guard checked focus before the scheduled focus action. The final guard
exempts only the initial readiness checkpoint and checks every later one.
Failed or incomplete attempts are not pooled into the matched comparison.

An earlier focused D3D11 attempt (`pacing-d3d11-focused144`) had a93.847ms
API outlier. The corrected-route repeat above did not reproduce it. Its
cause is unassigned; it is not evidence that D3D11 is generally slower.

The tracked change is confined to the Windows gameplay test harness:
active monitor reporting, optional foreground acceptance and correct startup
focus ordering. Production renderer code and platform defaults are unchanged.
The helper now finds the first PID checkpoint even when focus metadata is
the first report record, so owned ETW tracing starts and closes correctly.

## Reproduction and evidence

Local isolated helper arguments:
`RUN_NAME vulkan|d3d11 story-local-load|crowd-vietnam 144|0 fullscreen require-foreground`.
Zero selects Original60. It invokes tracked
`tests/gameplay/stability_transitions_game.py --fullscreen --require-foreground
--no-captures --no-native-trace --profile-presents` with the matching
`--renderer-sequence`, scenario and native FPS. Story window: local-loading
through local-fire (~38.6s); Vietnam: combat-one through combat-six (~65.4s).
Case directories `private/test-saves/pacing-*-focused144c` and
`private/test-saves/pacing-*-focused60bots` retain checkpoints, actual renderer
logs, API CSVs and PresentMon CSVs. Analysis is in
`logs/frame-pacing-results.json`; raw artifacts and helpers remain ignored.
No game assets or saves are published. The user requested publication to main
after reviewing these measurements; this report does not claim a renderer fix.
