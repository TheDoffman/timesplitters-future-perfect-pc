# Native high refresh regression record

The user requires previously addressed bugs to stay resolved while native
144 FPS, animations and texture fluidity are developed. This record starts from
main `5db4efc` and the existing issue-resolution evidence in `STATUS.md`.
It is an initial coverage inventory, not an exhaustive certification of every
historical issue. Expand it as timing-sensitive fixes are audited.

## Fresh TRAP poses (2026-10-25, PR #364 follow-up)

The earlier discrete-clock repair below is superseded for fractional intervals.
Shared coupled half-kick/drift steps now produce fresh poses, while a carried-
tick contact budget preserves the original lifetime. Original/whole/catch-up
states and events remain byte-equal. The original-US fixture checks40 rate/slow/
phase schedules and12 live-switch/jitter schedules, pose/event windows, phase,
terminal state, pause and ABI. Six independent gate/conversion/operator/contact
mutations fail. The public clock fixture covers the coupled map and GPU-hook
composition. Windows build passes; live TRAP activation/rendering and physical
Mac remain open. See STATUS and the script-clock guide for measured limits.

## Player-statistics timing (2026-10-25)

D1FD0's whole gate records only0.25 seconds per game second at240 for two
time counters. D2CB7 now scopes/restores its float delta to carried ticks,
preserving original float-addition cadence while recording caller-computed
distance every frame. D2079's pause guard blocks stale tick counts.
Original-US tests cover160 rate/slow/phase/hour schedules, eligibility,
distance, pause, uneven intervals, fallback and ten-minute bit-equal time totals.
Opening the gate alone fails long-run rounding parity. Independent scope/pause
mutations fail; the public fixture covers dispatcher/restoration on both CI hosts.
Windows build and119 timing/vehicle tests pass. Native240 Disco statistics
progress agrees with the integer clock within0.0043s over31.28s; this is bounded
counter evidence, not pacing or broad gameplay validation. The matching
Original60 route differs by0.0027s over41.18s and also exits cleanly with
fixture/profile preservation. Physical Mac remains pending; see STATUS.

## Mansion TREE live progression (2026-10-25)

Read-only map callback records and the in-game menu locate TREE/TRAP in Mansion
of Madness, Story index4/scene1. Normal isolated entrance play on the current
1fe73b2c executable activates TREE in Original60/native240. Both have91 active
observations, monotonic phase, matching original sampled pose math, terminal80
clamp and active-state clearing. Their activation-duration brackets overlap
around4.53–4.63s; terminal phase differs by less than one original tick. Signed
profiles and clean exits/cleanup pass. The native run later dies after a much
longer idle interval; this does not establish survival/damage or mission
progression parity. A bounded upper-floor calibration does not activate TRAP.
No production change was needed.

The observed foyer rendering does not identify the TREE mesh. Full rendered
target/event-service parity, live TRAP activation and physical Mac remain open;
process reads/traces/captures are not pacing or per-frame atomic evidence. See
[STATUS](../STATUS.md#mansion-tree-live-activation-and-phase-2026-10-25-pr-363)
for source mapping, provenance, corrected serialized offsets and limitations.

## Scripted TRAP mixed clock (2026-10-25)

TRAP1D8AB0 adds a fixed impulse once per whole-tick call, but broad native
conversion replaces its phase/travel span with only that native frame's
fraction. Original-routine comparisons reproduce altered motion/state/event
progression. Preserve the authored solver by retaining whole units at only
1D8AF5/1D8B6A; freeze its gate when a local pause leaves a stale carried tick.
Opening the gate on every native frame would multiply impulses and also fails.

The fixture executes the original solver, sine-table initializer and sine
helper. Forty schedules through240/slow time/carried phases compare full entity
bytes and sound/control/removal calls at every frame with Original; catch-up
and pause are covered. Restoring either conversion or opening the gate is a
failing control. All118 timing/vehicle tests,21 context/backlog checks and the
supported Windows rebuild/bundled checks pass. Asset-free clock checks run on
both CI hosts; private assets
are needed for original-routine execution. This restores60Hz-authored mechanics,
not fresh fractional trap poses or live scene/Mac acceptance. See
[STATUS](../STATUS.md#trap-solver-mixed-clock-repair-2026-10-25).

## Scripted TREE callback phase (2026-10-25)

Original US1D88B0 integrates its animation phase with float game time, but its
whole-tick entry gate drops subticks. Original-routine execution reproduces
phase0.25 after one game second at native240. A site-specific gate correction
admits positive fractional intervals; the original routine retains pose math,
event crossings, terminal clamp and state. Neighbor callback1D8AB0 remains
outside the repair because it has separate per-call behavior.

Forty original-routine schedules through240 cover phase/pose, slow time, carried
phases, pause, single crossing/completion events, event times, ABI and exact
whole/catch-up fallback. Restoring the old gate fails. The asset-free clock
fixture covers the policy on both CI hosts; private-XBE cases skip without
supported assets and do not certify Mac gameplay. All117 timing/vehicle tests,
21 context/backlog checks and the supported Windows build/bundled checks pass.
Live TREE activation/rendered appearance and physical Mac remain unverified.
See [STATUS](../STATUS.md#tree-callback-fractional-phase-gate-2026-10-25).

## Merged-main Challenge timeout and pause (2026-10-25)

A supported Windows rebuild including main063789df completes Hart Attack's
zero-score timeout route at Original60 and native240. Both enter Game Over in
the same observed level-tick bracket5581..5584, retain health, hold their clock
through an eight-second pause and resume at normal speed. Ordinary forward
movement works and the three-second firing hold consumes eight pistol rounds
in each run. Reviewed captures show the challenge HUD, pause menu and zero-score
result screen in both modes. The signed test profile and canonical fixture stay
unchanged; both games exit cleanly.

This validates the sampled Challenge failure/timeout path, not a medal-winning
run, target scoring/animation, damage parity, identical displacement or the full
Challenge mode. Capture-bearing runs do not establish pacing; their native
Present recorder caps at20,000 rows before the late window. Separate repeats
without captures/traces and with delayed recording measure60.000/239.997FPS
over approximately28seconds, with consecutive fresh guest IDs. Native p99 is
4.713ms/worst5.736ms, game/wall1.000023; this is the stationary late phase, not
crowded combat. Device1280x720 and selected-window client640x480 differ, so no
physical display-resolution claim follows. One native paused
position observation is unavailable and excluded; all159 paused clock readings
remain valid/fixed. See [STATUS](../STATUS.md#merged-main-hart-attack-timeout-and-pause-2026-10-25)
for reproducibility, measurements and limits. No production change was justified.

## Mixed-rate Online results and rematches (2026-10-25)

Two isolated local Online peers complete a five-point Deathmatch and a full
rematch with Original60 hosting native240, then with native240 hosting
Original60. Both rounds in each direction produce matching final records for
all eight participants (two humans, six bots). Normal Results, Scores and
Options navigation works; the rematch resets all eight scores and the match
clock before completing again. Owned-window captures show the results and
score table rendered, including the 240 FPS Beta overlay.

These runs use executable `4d09cd2c`, built before main's later controller/
mouse-capture changes; they do not validate a rebuild of merged main. They use
the production Online directory/browser/bridge on loopback, asynchronous
memory observations and test-window captures. They establish this match-flow
and final-record behavior, not clean pacing, identical combat trajectories,
physical latency, remote Internet behavior or Mac networking. The runner
intentionally terminates its owned games after acceptance and preserves the
signed test profiles. See [STATUS](../STATUS.md#mixed-rate-online-match-completion-and-rematches-2026-10-25)
for commands, provenance and limitations. No production change was needed.

## Current Windows audio output presence (2026-10-25)

The current4d09cd2c executable completes ten-bot Disco sessions at native240
and trueOriginal60. A read-only helper meters only the owned game's Windows
render sessions; it neither records audio nor changes volume/mute. Across
approximately65.8s of active foreground gameplay per run, all324(native) and
322(original) sampled points have valid unmuted sessions and nonzero mixed game
output. Profiles/signatures and isolated cleanup pass;25 shared audio gain/
timing tests pass. This closes an output-presence gap beyond fake devices.

Mixed music/voices and roughly200ms sample spacing do not identify individual
weapons, prove heard sound, detect brief underruns or establish sync/latency/
frame pacing. Physical Mac/per-event audio acceptance remains open. Full method,
provenance and limits are in [STATUS](../STATUS.md#live-windows-process-audio-output-presence-2026-10-25-pr-361).
No production audio behavior changes in this check.

## Current Temple crowd coverage (2026-10-24)

The925f9f72 Windows binary completes separate ten-bot Temple sessions at
Original60/native180/240. Complete45-second host-clock windows measure
60.00054/179.99977/239.91279FPS; minimum one-second rates are60/180/236,
and p99 intervals17.145/6.038/4.849ms. All sampled checkpoints remain active
and foreground, guest frame IDs advance, and game/wall ratios stay within
0.02% of unity. Clean windows exclude captures, tracing, CPU profiling and
concurrent local builds/tests; automated input polling remains overhead.
Signed profiles and the canonical fixture are preserved. A separate desktop
capture verifies Temple scene/HUD/weapon/effect appearance in sampled combat.
This broadens current crowded-map coverage; it does not prove simultaneous
visibility of all bots, locked240, physical latency or full-game/Mac acceptance.
See [STATUS](../STATUS.md) for exact binary, settings, traces and limits.

## Current diagnostic144 crowded-match check (2026-10-24)

The ed07ee3d Windows build completes the current ten-bot Disco route with
143.999013FPS across45seconds, minimum one/five-second rates144, p997.372ms
and worst8.241ms. All6,479 guest IDs advance;19foreground active checkpoints
retain game/wall1.0002459. No captures, CPU profiler or per-update trace;
automated input polling remains. This current-build result supports the
original144FPS crowd target in that sampled match, not all maps or sustained240.
Diagnostic144 remains outside the four player-facing presets. See STATUS for
provenance and limits. No runtime change was justified by the trace-hook
microbenchmark examined alongside this run.

## Weapon-menu repair preserved at the current presets (2026-10-24)

Issue#105's prior fixed60 repair now has current-build Original60/native240
coverage on e9195841/ed07ee3d. Reviewed captures show entering the Vietnam Arcade
weapon list, selecting its third slot with the correct shotgun preview, and
backing out. Separate capture-free windows average59.999–60.000 and
239.999–240.016FPS with consecutive guest-frame IDs and no interpolation.
Native p99 stays below4.82ms in the three complete five-second menu windows.
The recorder cap excludes the native back-out window; only its separate swap
log reports240.0FPS. All four routes exit cleanly with profiles preserved.
This verifies that specific menu regression on Windows, not combat pacing,
full animation/texture fidelity or Mac gameplay. Input automation remains
active; see [STATUS](../STATUS.md) for capture overhead, timing and host limits.

## Expanded presets and original mechanics (2026-10-24)

Player-facing choices are exactly Original60 and native120/180/240 Beta;
native60/144 remain diagnostic comparisons. Six original-US fixtures extend
NPC dispatch, entity velocity, projectile motion, beam lifetime, camera easing
and HUD clocks to 180/240 without relaxing assertions/tolerances. All six pass
with verified private XBE data on Windows. Public CI skips absent private data;
native Mac execution and broader gameplay remain unverified.

The follow-up adds nine original-US fixtures at 180/240: player/NPC aiming,
coupled homing projectiles, beam geometry, camera tracks, falling/force regions
and rocking/bouncing objects. All nine pass with existing tolerances, preserving
their pause, lifecycle, damage/event and original-reference checks. Mounted
mouse tests also cover the new rates with 125/1000/8000 Hz packets, jitter and
legacy clock/consumption controls; the host suite passes all 11 tests. Controlled
targets/collisions/effect spies and packet injection are bounded evidence, not
full live combat, rendered-pose or physical input-latency acceptance.
Light decisions, network correction, screen/strip distortion, escort recovery
and menu motion also pass their original-US fixtures at 180/240. Portable
mouse-cadence and coupled-surface math checks pass; the latter does not validate
visible water integration. Existing tolerances remain unchanged throughout.

Script event delivery now covers every phase of the 60/120/144/180/240 cadence
at normal and quarter speed. The actual handoff dispatcher grants the weapon
and eight rounds exactly once; the disabled native path still reproduces missed
events on fractional phases. Original60, pause/resume, timer globals and ABI
checks remain. Original weapon sounds, camera effects and Arcade countdowns
also pass at180/240 with their existing timer, sound-count, collision/decay and
input-release assertions. These four original-code tests pass on Windows with
verified private data. They are not rendered opening-sequence or audible-output
acceptance, and their public CI skips do not validate Mac gameplay.

The current `4e16421` executable's complete ten-bot Disco windows measure
60.000418, 119.933859 and 167.835127 FPS at Original60/120/180. Two preceding
240 windows on that same executable measure 227.035 and 233.604. Fresh guest
IDs and near-unity game/wall ratios support sampled cadence/speed, not locked
180/240 or full-game acceptance. See [STATUS](../STATUS.md) for windows,
provenance, tails, nondeterministic workload and physical-display limitations.

## Crowded multiplayer preset coverage (2026-10-22)

PR #295 (`252e72e`, implementation head `4a40569`) reduces repeated memory
queries across an outlined label's four offset passes and foreground draw.
The same supported Windows executable now has original-60, native-120 and
two native-144 ten-bot Hotel observations at 3440x1440/FOV 73. Strict complete
host-clock intervals give 59.999, 119.840, 139.701 and 142.401 guest FPS;
none of these predeclared 50-second windows exceeds 33.33 ms. Asynchronous
level-clock endpoints over approximately 61.44 seconds give game/wall ratios
0.999912-1.000504 during combat and respawns. Native-144 baseline before the
optimization was approximately 132 FPS in this workload.

These checks support pacing and game-clock consistency in the sampled match,
not identical bot trajectories, weapon cadence, individual rendered poses or
full-game correctness. A separate Enhanced run and visual checks are recorded
with their distinct scope in [STATUS](../STATUS.md#2026-10-22--crowded-multiplayer-text-overhead).
All selected Windows/macOS CI checks pass for that implementation head;
native Mac gameplay, online human matches, remaining animation/physics audits,
texture-fluidity acceptance and representative full-game coverage remain open.
The earlier live 144-to-60 handoff gap was subsequently repaired; see
[the handoff evidence](#native-start-return-to-original-60-2026-10-22).
That bounded result does not establish every rate/scene transition.

## Fresh analogue input at native cadence (2026-10-21)

The native pad wrapper previously skipped the complete original decoder on
zero-whole-tick updates to retain frontend button edges. That also held all
stick/trigger axes at 60 samples/s, including keyboard movement mapped to the
left stick. This was a separate bottleneck from raw mouse motion, which already
has its own native-rate path. The new original-code fixture passes original
60 and native 60, then fails on the first native-120 fractional poll before the
fix (`logs/native-analog-before.log`).

`pc_native_input_guest` now calls the original `sub_0018FF70` on intervening
native updates too, retaining its connection checks, original dead zones and
axis ranges. It restores the previously published digital held state and
clears digital edge fields on those fractional updates. The next whole-tick
sample therefore still produces each press/release exactly once. Whole-tick
and disabled-native calls remain original; a null fractional record retains
its safe return. This refreshes input rather than interpolating old samples.

`tests/native_analog_input.c` lifts the complete US decoder and neutral-reset
routine from the pinned/hash-verified XBE, including its original button tables
and conversion constants. Controlled XInput devices vary all four stick axes
for all four ports at original 60 and native 60/120/144; every native poll sees
the changed axes. Separate trigger cases check fresh analogue values while
preserving the digital firing-edge cadence. Pause, catch-up (one poll), device
disappearance, original zero-tick behavior and stack/callee-saved invariants
pass. The decoder submits absolute rumble values more frequently, but the
fixture verifies identical commands across rates; the host feedback adapter
sets those values rather than accumulating elapsed vibration. Physical device
polling/latency and rumble still need hardware testing.

Six focused checks pass in 8.40 s, including the 864 pause/resume input schedules,
raw mouse cadence and network-gate mutations (`logs/native-analog-fixed.log`).
Ten menu/FOV/profile/terminal/backlog checks pass, with one Mac-only compile check
skipped (`logs/native-analog-input-regressions.log`). Windows PowerShell
`tsfp.ps1 Build` and its required checks pass (`logs/native-analog-build.log`).
The original routine is shared by both hosts; no Windows-only gameplay path
was added, but actual native Mac behavior is unverified.

### Live input regression check

On the rebuilt Windows executable, isolated synthetic keyboard/mouse runs
navigate Settings → Video Options, apply 120 FPS in-process from 144, open
Controls, and return to the main menu. Reviewed captures show 120 FPS (Beta)
with gray Apply, the actual Controls page, and the main menu. The runtime log
confirms the 144-to-120 target transition. One initial scripted Controls step
returned to Video because the parent selection resets; the corrected replay
uses two Down presses and its final captures verify the intended route.

A separate native-144 Venice run consumes two revolver rounds (6 → 4), turns
30 degrees from 300 mouse counts, and changes position by 17.099 units. Pause
has 444 updates with zero game delta spanning 3.402 raw seconds; Continue
resumes the world. The player is then killed by Viola (reviewed capture and
health 0), so this is not surviving/full-match acceptance. Capture-enabled,
minimized concurrent sessions are behavior checks, not performance or physical
input-latency measurements.

Both final sessions exit cleanly, retain identical signed profiles and the
canonical fixture, and recycle their disposable partitions. Evidence resides
in `private/test-saves/native-analog-{menu,game}` and
`private/pipeline/native-analog-live-summary.json`. Build source is `8b8deaf`
plus this revision's input wrapper; executable SHA-256 is
`cd0d7679a40d12f86fda79c74fa2e7287ca48a6fa75c9478ef02ebe56d8b83d8`.
The bounded runner hash is
`9289c332daf11eb7e73ceff458a7efd28376dc5eddfa26017b7e00278eff8bec`.
Hardware controller latency/rumble, hotplug behavior, native Mac execution and
full-game coverage remain open. The lifted original-code fixture requires the
private supported XBE; shared CI selection alone does not prove it executed.

## Completed-update multiplayer poses (2026-10-21)

The shared clock hook now has an opt-in `TSFP_NATIVE_POSE_TRACE` recorder in
`src/pc_native_pose_trace.h`. Original US disassembly shows the gameplay loop
calling `sub_0003C440` at 0x3D3CA, then `sub_00153B00`, then the clock at
0x3D3E6. The observer requires return address 0x3D3EB and samples before the
clock changes the next update's delta. It records one completed update for
two human slots in an active network match, including frame ID, scene, game
delta, level tick, entity identity, XYZ and absolute QPC time. Other clock
callers, inactive matches and invalid entity pointers are skipped. The file is
bounded at 20,000 rows; diagnostics are disabled unless explicitly requested.
No movement, correction or transport policy changes are made. Completed
update poses still do not establish GPU completion or monitor scanout.

The bounded Windows runner exposes `--trace-network-poses`. The comparison
uses the existing Vietnam/no-bot replay, plus backward/left/right two-second
client holds after the original bidirectional walks. Captures and asynchronous
memory sampling are off; Present profiling is on. `network_pose_stats.py`
measures the same predeclared 0.2-second-trimmed input windows, requires
consecutive frame IDs and stable scene/entity identity, and reports unchanged
updates rather than silently discarding stationary tails. Its ten portable
checks cover 60/120/144 cadence/game time, missing/repeated frames, changed
identity/scene, non-finite data, insufficient coverage and stationary tails.

Windows PowerShell `tsfp.ps1 Build` and its required checks pass
(`logs/native-pose-boundary-build.log`). Five focused timing/cadence/pose checks
pass, including the existing network-gate mutations; statistics/CI/backlog
checks pass 82 tests (`logs/native-pose-boundary-tests.log`,
`logs/native-pose-stats-tests.log`). Native Mac execution remains unverified;
the recorder is shared, but the Mac runner/transport require separate work.

### Original 60 and native 144/120 results

All five predeclared holds pass the completed-update continuity check for the
moving actor on both peers: **930 original-60 observations and 2,058 native
observations change position on every consecutive update**. These counts cover
only the approximately 1.55-second central windows, not entire matches.

| Movement | Native local update rate | Native remote update rate | Unchanged updates, local / remote |
| --- | --- | --- | --- |
| Host forward | 144.006 Hz | 120.017 Hz | 0 / 0 |
| Client forward | 120.027 Hz | 144.064 Hz | 0 / 0 |
| Client backward | 120.017 Hz | 144.101 Hz | 0 / 0 |
| Client left | 120.032 Hz | 143.958 Hz | 0 / 0 |
| Client right | 119.964 Hz | 144.070 Hz | 0 / 0 |

The original reference ranges from 59.915 to 60.033 updates/s across those
same checks, also with zero unchanged positions. Native completed-update
interval p99 is at most 11.413 ms and the worst interval is 12.492 ms; original
worst interval is 21.189 ms. Native accumulated game-time/wall-time ratios are
0.999122–1.000444 (original 0.998575–1.000550). These are update-boundary timing
measurements, not presentation or scanout intervals. The separately retained
Present profiles contain only guest entries; no interpolated entries appear.

This improves on the asynchronous memory observations: temporary changes
within an update are no longer counted as extra frames. It also demonstrates
continuous 120 FPS movement in this run. Earlier local stopping is not
reproduced here, and its precise cause remains unresolved. Different spawns,
headings and terrain still prevent a movement-speed parity claim; the shorter
sideways distances are retained rather than compared as equivalent routes.

Source and evidence: Windows x64 Release based on `70ab800` plus this revision's
shared pose observer, executable SHA-256
`4246f90b5bc911c04acc838835c18d66d8e03d3a0ac302d238674585431d7699`;
runner SHA-256
`9289c332daf11eb7e73ceff458a7efd28376dc5eddfa26017b7e00278eff8bec`.
Runs are `private/test-saves/lan-pose-{60,native}-{host,client}`; summaries are
`private/pipeline/lan-pose-{60,native}-summary.json`. All four processes exit
normally with unchanged signed profiles and fixture, and their disposable
partitions are recycled. Windows build/patch checks passed. Mac, cross-machine
latency, packet-loss recovery, combat and sustained rendered-pose coverage
remain open.

The replay is retained as `tests/fixtures/lan-pose-commands.json`; the host's
non-movement progress label is clarified after the run without changing its
inputs or timing. Unlike the earlier two-walk replay, its host observation now
occurs during subsequent client motion, so use `network_pose_stats.py`, not the
earlier settled-endpoint checker. No settled-endpoint claim is made for these
new four-direction runs.

## In-motion loopback observations (2026-10-21)

`stability_transitions_game.py --sample-network-ms 2` now samples both actor
slots independently of input waits, with read-begin/read-end timestamps and a
32 MiB per-session bound. Python 3.12 on this Windows host implements
`monotonic()` using GetTickCount64 (15.625 ms resolution), which produced
repeated timestamps in the first attempt. That timing evidence was rejected;
one subsequent coarse-clock attempt was stopped cleanly before gameplay.
The runner now uses `perf_counter()` (QueryPerformanceCounter, 100 ns reported
resolution) consistently for holds, duration and observations, with Python's
high-resolution sleep. All sessions remain bounded and isolated.

The corrected original-60 and native 144-host/120-client pairs use the explicit
Vietnam Deathmatch/no-bot replay, with renderer captures disabled and windows
minimized. Both directions move and agree exactly at the settled endpoint.
`tools/network_motion_stats.py` measures the central approximately 1.56 seconds
of each two-second hold, preserving all samples in that predeclared window.

| Pair / moving actor | Local observed changes/s | Remote observed changes/s | Local / remote longest observed unchanged interval |
| --- | --- | --- | --- |
| Original 60/60, host | 60.077 | 65.169 | 19.194 / 18.756 ms |
| Original 60/60, client | 60.026 | 72.158 | 19.381 / 19.660 ms |
| Native 144/120, host | 144.367 | 124.003 | 10.131 / 11.143 ms |
| Native 144/120, client | 100.229 | 127.679 | 264.940 / 244.744 ms |

Observation-gap p99 is 2.560–2.801 ms; no gap in these windows exceeds the
observer's target frame duration. Remote counts above the rendering target
show why memory changes must not be described as distinct rendered frames:
the original prediction/correction code can modify the position within an
update. This read-only evidence is also not packet latency or scanout.

The native client's final quarter-second is stationary on both local and
remote machines. Its level clock continues, health stays 1.75, and the input
hold remains requested. Motion changes direction just before stopping, which
is consistent with contact, but the uncaptured run cannot establish the cause.
This segment is not accepted as uninterrupted 120 FPS movement; do not hide
its stationary tail by selecting a shorter post-hoc analysis window. Different
spawns and headings also prevent a speed-parity claim from these distances.

The six new statistics tests reject frozen, wrong-scene, non-finite and invalid
timestamp evidence, and explicitly report undersampling. Together with shared
CI/backlog checks, 72 tests pass in 0.17 s (`logs/network-motion-tooling.log`).
The portable analyzer joins both host jobs; Windows process-memory sampling
needs a separate Mac equivalent. Full rendered-pose smoothness remains open.

Evidence: `private/test-saves/lan-motion-{60,native}-{host,client}` and
`private/pipeline/lan-motion-{60,native}-summary.json`. The four corrected
sessions exit normally with unchanged signed profiles and canonical fixture;
their disposable virtual disks are recycled. They use Windows x64 Release
from the menu-separator source (committed as `1f93646`), executable SHA-256
`ae7e026fca5c345426a779192765e990304379c19227d495c66c89d08edb4e2a`,
and runner SHA-256
`5e2f65212e4a01152e3bbbda0485ffef72186b03c72948867f6e62886b85dde3`.
No new gameplay change or build was made for this audit. Mac runtime,
cross-machine transport, combat, sustained motion and frame-boundary poses
remain unverified.

### Captured reverse-direction follow-up

A third pair (`lan-motion-reverse-{host,client}`) uses the same binary and
runner, with the client holding S instead of W and renderer captures enabled.
The captured scene is Vietnam with both humans visible; the client's settled
view faces the waterfall basin from a bank. This does not reveal the surface
behind the player or reproduce the earlier spawn/heading.

Both settled endpoints again agree exactly. The host's trimmed window records
131.078 local and 115.698 remote changes/s, with approximately 34.7 ms maximum
unchanged intervals. The client travels only 1.923 units in its measured window
and is stationary for 1.144 s locally and 1.118 s remotely. This is another
unsuitable uninterrupted-movement route, not evidence of network stalling and
not acceptance of continuous 120 FPS movement. Capture-enabled observations
must not replace the earlier capture-free pacing measurements. The exact
reason for local stopping remains unresolved; a controlled unobstructed route
and frame-boundary pose capture are the next meaningful acceptance steps.

Both sessions exit normally, signed profiles/fixture remain unchanged, and
disposable disks/rotating BMPs are removed. The selected captures and summary
remain in the two run directories and
`private/pipeline/lan-motion-reverse-summary.json`. This follow-up adds no
production changes and makes no frame-perfect, latency or full-game claim.

## Remote-position correction audit (2026-10-21)

`tests/native_network_correction.c` executes the complete original US routine
`sub_00356E20` (0x356E20..0x3570C7), lifted from the hash-verified executable.
Its maximum correction already reads the float game delta at 0x4E794C and
multiplies it by the original 0.2 constant. No gameplay patch is required here.
The test supplies a controlled prediction callback and network eligibility;
it does not execute remote prediction, transport, or the rendered frame.

Eight half-second sequences cover original 60 and native 60/120/144 at normal
and quarter speed. Every update advances, including zero-whole-tick updates;
total correction is 6 units at normal speed and 1.5 at quarter speed. Pause,
a fractional update, a three-tick catch-up, diagonal normalization, a small
error, the 20-unit teleport cutoff, stale-peer handling, local updates and
offline dispatch retain their original behavior. Stack/register and game-delta
invariants are checked. The timing/network/movement/backlog suite passes
68 tests in 64.82 s (`logs/network-correction-suite.log`). This is Windows x64
fixture evidence; the XBE-dependent case is not proof of an asset-free CI run
or native Mac execution. Gameplay code and timing policy are unchanged.

## Current evidence

Windows x64, supported US XBE, pinned runtime, local development branch
`feature/native-144-fps`, 2026-10-20. The initial Windows gameplay comparisons
used executable SHA-256
`80771b53aaa3a15b04530a4b7427106e7516093402816e27a29d5d6dd1bfc31c`.
Later script-event and NPC repairs use separate builds and stronger runner
observations; their hashes and results are recorded below. The latest NPC
comparison repairs the skipped-intro opening regression in a bounded test.

The following focused source checks passed **91 tests** in 61.83 seconds.
Local evidence: `logs/native-144-preserved-fixes.log`. They exercise the
existing fix mechanisms; they do not execute all affected levels with the
native clock enabled. Mac execution remains unavailable here.

| Previously addressed behavior | Current source checks | Native 144 FPS scene acceptance |
| --- | --- | --- |
| Companion jeep dismount loop, issue #100; floating-point/control-flow joins | `test_recomp_vehicle_joins.py`, `test_recomp_float_joins.py` | Scotland jeep encounter, dismount/remount and combat transitions pending |
| Terminal depth clears, issue #47 | `test_terminal_depth.py` | Possession and return to player camera pending |
| U-Genix monitor composition, issue #49 | `test_camera_feed.py` | Crow/logo/scanlines, camera motion and aspect-ratio comparisons pending |
| Story Objectives health and shield, issue #11 | `test_objectives_health.py` | Damage, shield change and objective-screen updates pending |
| Frontend profile acceptance and menu mouse behavior | `test_profile_accept.py`, `test_menu_mouse.py` | Startup/profile and pause navigation observed; full menus, sliders, physical input and settings preservation pending |
| Audio timing and buffering, issue #8 | `test_audio_timing.py` | Playback cadence and synchronization during cinematics/gameplay/pause pending |
| Automatic look behavior | `test_auto_look.py` | Aiming, scripted look and vehicle camera checks pending |
| Texture loading, DDS handling, mode toggles and fallback | `test_texture_runtime.py` | Animated/scrolling surfaces, replacement textures, alpha and transitions pending |
| Cutscene letterboxing and channel clear rules | `test_cutscene_letterbox.py`, `test_channel_clears.py` | Representative cinematics, ultrawide and texture-camera transitions pending |

Outstanding inventory includes Ghost Gun/spirit rendering, gameplay/frontend
FOV isolation, terminal mouse routing, multiplayer movement/networking,
launcher/install/update behavior and platform-specific fixes. Use their
existing test suites and issue-resolution notes before assigning a result.
An unchanged file, an old passing result, or another platform's successful run
does not replace the relevant current check.

## Venice gameplay and presentation pacing (2026-10-21)

Windows x64/D3D11, gameplay source `dbb84e3` and documentation head `6392b6e`,
executable SHA-256
`b9f21b2537d132f8d62c908e0c2f5d438f37f40caaf49d5477e99808fa0a28b1`.
A bounded native-144 renderer-capture session reaches Venice (scene 46), shows
its revolver, fires three rounds, moves/turns, opens/closes pause and continues
through combat. It eventually reaches match results. This adds an arena beyond
Vietnam/Hotel; it does not validate every Venice route or animated surface.
Selected captures are in `private/test-saves/broader-arcade-review`.

Two separate runs then replay the same commands at original 60 and native 144,
using recommended bots/weapons, 15-point/10-minute defaults, isolated Test Lab
saves, 1280x720 windowed rendering and no image captures. These windows are not
minimized. The shared recorder measures accepted host Present intervals with
`--profile-presents`, and records a separate CSV for each process restart.
The replay is retained in `tests/fixtures/venice-pacing-commands.json`; the
local orchestration and summary scripts are in `private/pipeline` as
`check-venice-pacing.py` and `summarize-venice-pacing.py`. No personal settings
or saves are used.

| Measurement | Original 60 | Native 144 |
| --- | ---: | ---: |
| Guest-frame presentations/s | 59.999 | 143.648 |
| Median interval | 16.669 ms | 6.970 ms |
| 95th percentile | 17.559 ms | 7.830 ms |
| 99th percentile | 17.950 ms | 8.278 ms |
| Worst measured interval | 18.331 ms | 10.702 ms |
| Intervals over 33.33 ms | 0 | 0 |
| Recorded presentations in window | 1,524 | 3,654 |
| Level-clock seconds/wall second | 0.99977 | 0.99931 |

The native clock trace independently records 7,199 consecutive unpaused
scene-46 update intervals over 50.091 s: 143.720 updates/s, median 6.981 ms,
p99 8.240 ms, and game-time/wall-time ratio 1.000000. During pause it records
488 consecutive zero-game-delta updates spanning 3.382 s. Both replays consume
two revolver rounds before a weapon pickup and turn from yaw 132 to 102 with
the same 300-count synthetic mouse input. Position and health stay unchanged
between the pause/resume command snapshots in both runs. Physical input
latency and every intermediate paused frame are not measured by those snapshots.

Host windows are selected between the explicit pacing markers, trimmed by two
seconds at each end: 78.938–104.328 s at original 60 and 78.922–104.359 s at
native 144. CSV time begins at the first accepted Present, while runner time
begins before launch; trimming allows startup-offset uncertainty, rather than
claiming millisecond-exact workload alignment. All sampled states inside the
marker spans remain scene 46, active match state 7, unpaused. Both windows
include deaths/respawns and different bot outcomes/positions; this is a
representative match comparison, not deterministic gameplay or movement-speed
parity. API acceptance does not prove GPU completion, monitor scanout, or
unique pixels in every submitted frame. No interpolated presents or failed
presents are reported.

Both runs exit normally, preserve the fixture and keep signed test profiles
unchanged. Reports, input/state observations, CSVs and `pacing-summary.json`
are under `private/test-saves/venice-pacing-60` and `venice-pacing-144`;
matching logs are under `logs/`. The frame-statistics tests pass (2 tests).
Full-match completion is supported only by the separate capture run here;
the two pacing runs end during an active match. Other arenas/modes, network
peers, physical aiming, live force regions and native Mac acceptance remain open.

### Native 120 preset follow-up

The same Venice replay also passes at native 120 on the unchanged executable
above, using committed runner `555fb33` (runner SHA-256
`36ddc441f0d1fcdf701542fe84c570ff93fd4706fd9ba5ff8df0a92752773d28`).
The renderer remains windowed, visible and capture-free. Between 78.937 and
104.359 s, 3,051 guest presentations average 120.003/s: median 8.300 ms,
p95 9.145 ms, p99 9.429 ms, worst 11.333 ms. There are no intervals above
33.33 ms, interpolated presents, or reported presentation failures.
The level-clock/wall-time ratio is 1.00039. Independently, 6,013 consecutive
active scene-46 update intervals over 50.138 s average 119.930/s, with p99
8.878 ms and game-time/wall-time ratio 1.000000.

The revolver again consumes two rounds (6 to 4, reserve 30), and 300 mouse
counts turn yaw 132 to 102. The trace records 402 consecutive zero-game-delta
updates during pause, from 66.441767 to 69.783961 s (3.342 s). The resume
snapshot, taken after two seconds of resumed play, has changed position and
health from combat; it is not evidence of movement while paused. Bot outcomes
and player deaths differ between the three rates. This is a preset/pacing and
selected-behavior check, not identical physics trajectories or complete Venice
coverage. It shares the timing-origin and API-versus-scanout limitations above.

The bounded run exits normally with unchanged signed test profiles and fixture;
its disposable disks are recycled. Evidence is in
`private/test-saves/venice-pacing-120` and `logs/venice-pacing-120.log`.
The Present CSV SHA-256 is
`1317c71a58d1766aa75367de7d13c29cc0eb48aa6b72d6948b3e0b4c8cac7113`.
No game source changed for these comparisons, so no rebuild is needed.

## Region-driven player force and drag

The original player mode `0x800` updater (`0xD4820..0xD4AFF`) adds vertical
force, clamps speed, then loops over a 0.99 drag factor while an integer loop
counter is below the floating game delta. Every positive fractional update
therefore applies a whole drag step. At 120/144 this caused excessive drag.
The original selector `0x225C70..0x225DEE` supplies force from height inside
four cylindrical regions and uses state setter `0xD2CE0..0xD2D16` to enter
mode 0x800 or return to falling (4). The level dispatcher calls this selector
for scene 52 at `0x68207..0x68210`; the fixture does not execute that dispatcher.

Shared numeric policy now splits the drag factor over fractional raw intervals,
normalizes the force increment for the same original affine response, and
retains the original post-drag speed limits (+/-0.594). The view-height 0.8
factor at `0xD498F` receives the same raw-time correction as the two airborne
paths. Normal, slower and accelerated game time are handled. Whole/catch-up
updates remain original. For speed s, the original per-raw-tick drag is
Q = 0.99^ceil(s). Fractional updates split this affine force/drag response;
when the guest loop executes more than once, its exponent is divided between
those iterations. The clamp accounts for remaining iterations so the final
speed limit remains the original 0.6*Q. A one-ULP correction prevents an exact
integer speed from acquiring an extra drag step through float round-off.

`tests/native_player_force.c` executes all four complete routines plus the
original falling continuation `0xD4100`. Region records and a flat-floor collision
service are controlled; the selector, state setter, force, clamping, movement,
view offset and landing code are original. Existing CI collects this fixture
through `test_native_timing.py`. The pinned US XBE supplies code and constants.

| Two game seconds, upward force 1 from Y=100 | Original 60 | Native 120 | Native 144 |
| --- | --- | --- | --- |
| Before correction, normal speed | 148.612564 | 137.141251 | 133.471725 |
| After correction, normal speed | 148.612564 | 148.464554 | 148.438110 |
| Final vertical speed | 0.594000 | 0.594000 | 0.594000 |
| After correction, quarter speed | 123.671913 | 123.656326 | 123.651520 |

All 24 constant-force sequences pass across original 60/native 60/120/144,
normal/quarter time and upward/balanced/downward force. Eight six-game-second
sequences run the real selector and movement together; maximum sampled position
differences are 0.046669/0.052988 units at native 120/144, or 0.007625/0.008889
at quarter speed. These are bounded integration differences, not exact parity.

Eight more sequences move out of the cylinder, return to falling and land:
entry, exit and landing each occur once. Normal-speed original/native-144 exit
times are 3.350000/3.333333 seconds and land times 3.966667/3.951389; quarter
speed gives 13.333333/13.340278 and 16.100000/16.097222. Additional checks cover
region radius/height boundaries and ordering, two-player isolation, death-state
transition protection, pause, uneven intervals, original 1/2/3-tick catch-up,
native-disabled and neighboring-site scope. No network-peer claim is made.

Windows x64 results: all 159 related regression checks pass in 146.51 seconds
(`logs/native-player-force-regressions.log`). The pre-fix fixture failure is
`logs/native-player-force-red.log`; full numerical results are in
`logs/native-player-force-green.log`. The earlier independent diagnostic is
`logs/native-state800-probe.log`. Build/scene results follow after validation.
Mac execution, actual level geometry and network synchronization remain open.
This does not certify full-game movement or physics.


### Accelerated force/drag and irregular frame intervals

The continued audit adds 24 constant-force sequences at 1.5x, 2x and 3x speed
across original 60/native 60/120/144. Before the extension, native 120 at 1.5x
speed differed from the original downward terminal velocity by -0.005940020
(`logs/native-force-fast-red.log`). All final terminal-speed differences are
below 0.00001; whole-raw-tick execution is unchanged. Position integration
remains approximate: at native 144 the upward two-second displacement differs
by -0.270035, -0.330566 and -0.517395 units respectively.

Twelve additional six-second sequences execute the real region selector and
force updater together. Maximum native-144 position/velocity differences are
0.074510/0.000422 at 1.5x, 0.106811/0.000386 at 2x, and 0.169543/0.000868 at 3x.
The fixture now covers 76 constant-force/region/exit sequences overall, plus
pause, ABI, player isolation, bounds, uneven intervals and catch-up checks.

Irregular raw intervals 0.18168/0.81832/0.27/0.73 exposed an additional float
boundary: recovering 3x speed from game/raw sometimes produced 3.000000238,
incorrectly requesting four drag steps. The fixture failed before the one-ULP
correction (`logs/native-force-fast-jitter-red.log`) and now matches the
original velocity within 0.000000045 at a shared elapsed time. Nine accelerated
1/2/3-raw-tick catch-up comparisons remain bit-identical to original execution.
These are controlled original-code comparisons, not live scene-52 acceptance.

The focused original-code fixture passes (`logs/native-force-fast-green.log`).
The wider timing/frame/entity/effect/backlog suite passed 63 tests in 91.77 s
before the last rounding correction; the focused fixture was rerun afterward.
After the final rounding correction, all 56 timing/backlog checks pass in
82.95 s (`logs/native-force-final-regressions.log`). The supported Windows x64
Build passes, including its GPU/input/settings/save checks
(`logs/native-force-final-build.log`), using MSVC 14.44.35207 and MSBuild
17.14.40. Gameplay source is local commit `dbb84e3`; executable SHA-256 is
`b9f21b2537d132f8d62c908e0c2f5d438f37f40caaf49d5477e99808fa0a28b1`.
No new live force-region or Mac runtime acceptance is claimed.

## Airborne view offset and controlled landing

The complete original US falling routine `0xD4100..0xD4609` and fixed-speed
descent routine `0xD4610..0xD481D` multiply player view-height offset `+0x98`
by 0.8 on every update. Native fractional dispatch therefore made this offset
settle faster with refresh rate. The first native-120 step from 0.25 produced
0.200000003 instead of the original-time response 0.223606799.

Shared `pc_native_numeric.h` now splits only the two damping factors at
`0xD41D0` and `0xD46B4` over fractional raw ticks. Raw time preserves the
original per-update response with a quarter-speed game delta. Zero game time
keeps the offset; whole and catch-up updates retain the original single factor.
Gravity, collision, damage and movement state transitions are unchanged.

`tests/native_player_fall.c`, registered in the existing native-timing CI suite,
executes both complete original routines and pinned-XBE constants. A controlled
flat floor replaces collision; statistics are recorded by a service spy. The
separate four-unit fall-death rule is disabled via its original flag so larger
falls reach the real landing damage branch. Death dispatch and level geometry
are outside this fixture. All 32 combinations pass on Windows x64: original
60/native 60/120/144, normal/quarter game delta, 3/18-unit heights, both paths.

- Each landing requests grounded state 2 exactly once, with zero floor height.
- The 3-unit gravity fall retains health 100; 18 units retains health 70.
  Fixed-speed descent leaves health 100 in both cases.
- Landing time differences remain within one original plus one native update.
- Every airborne view-offset sample matches the original-time damping curve.
- Longer gravity falls move on every update and reach the original terminal
  velocity -0.6. Pause, uneven intervals, unchanged 1/2/3-tick catch-up and
  neighboring-site/native-disabled behavior also pass.

This is **not exact trajectory parity**: existing fractional Euler integration
produces a maximum sampled vertical difference of 0.150002 units at 120 and
0.174995 at 144 over 1.5 game seconds; quarter speed gives 0.038418/0.043938.
The test records and bounds this difference to half an original step at terminal
speed. It is not evidence that ledges, slopes, moving floors, world collisions,
physical input or perceptual landing behavior are fully verified. Gravity
integration and scene acceptance remain open in the broader movement audit.

Local evidence: `logs/native-player-fall-red.log` records the pre-fix failure;
`logs/native-player-fall-green.log` records all 32 landing sequences, six
trajectory comparisons and timing-scope checks. The 158 related Windows checks
pass (`logs/native-player-fall-regressions.log`, 161.64 seconds). The supported
Windows Build and built-in checks pass (`logs/native-player-fall-build.log`
and `.err`), with compiled parent label `d486df6` plus this source change and
executable SHA-256
`7775cc334b6895133a3d8fa4926f94853d02f7ba37886f9fb24327ac1996f897`.
Mac execution remains pending. A separate movement-state-0x800 routine at `0xD4820`
also damps this offset and has a velocity-damping loop bounded by float time;
its coupled force/drag path is a remaining audit target, not covered here.
A private original-routine diagnostic (`logs/native-state800-probe.log`) already
shows a material mismatch: under constant normalized force 1, two seconds moves
from Y=100 to 148.612564 at original 60, but only 137.141251/133.471725 at native
120/144. Force 0 gives the symmetric downward discrepancy; force 0.5 balances
gravity. These are controlled inputs, not a claim about an observed live scene.
The original caller `0x225C70` supplies this force from height within cylindrical
regions and enters/exits state 0x800. The subsequent region-driven force section above records the coupled
caller/damping/clamp repair and its remaining integration differences.

Two bounded native-120 Windows attempts (`player-fall-smoke-120` and
`player-fall-gameplay-120`) remained in frontend scene 101. The first scripted
startup selected profile creation; the second loaded Test Lab with slower,
inspected input but the subsequent scripted route reached Online/System Link
instead of Story. All labels such as `gameplay-start` and `fire` are command
intent only, not evidence those actions occurred. Both owned processes exited
normally within their 180-second bounds, with fixture/profile and personal
settings unchanged. No gameplay, live falling, firing or network-connection
acceptance is claimed. Future live checks need inspected menu transitions
before sending gameplay inputs; the source fix is supported by the complete
original-routine fixture and successful build, with scene acceptance open.

## Hotel deathmatch completion and results flow

Windows x64/D3D11 source `2adcf29`, executable SHA-256
`858ce97b290710485935f282c388ac71131eababe8244cb1b75b7147acb1a0ef`, completes
one local Hotel deathmatch at original 60 and native 144. Both use the isolated
Test Lab fixture, Cortez, recommended weapons/bots, five-point score limit,
three-minute time limit and the same normal menu setup. The original run's
interactive setup inspection takes longer; these are not identical wall-time
or bot-RNG replays. No game source change accompanies this validation.

| Check | Original 60 | Native 144 |
| --- | --- | --- |
| Gameplay | Scene 39, match state 7, dual pistols visible | Same scene/state and initial loadout |
| Combat/death | Health falls to zero; final scoreboard records two player deaths | Continuous read-only observer records three deaths and two intervening respawns |
| Match finish | State 8, results pause; reviewed Scores page shows The Freak winning with 5 points | State 8, results pause; reviewed Results page shows the completed standings |
| Continue after results | Not exercised before the bounded run ended | Three Next presses start a fresh state-5 countdown, visible dual pistols, health restored |
| Exit/data | Normal runner exit, valid unchanged fixture/profile | Same; personal controls/display hashes also unchanged |

Evidence directories are `private/test-saves/arcade-flow-60` and
`arcade-flow-144`; logs have matching names under `logs/`. Reviewed captures
include setup limits, original `continue-or-respawn.png` (Scores), native
`continue-or-respawn.png` (Results) and native `after-results.png` (new match
countdown). The original session reaches its 240-second bound after setup
inspection; the native session stops normally around 150 seconds. The second
match is not claimed as played or completed. No network peer was present.

The runner samples state between commands, so its multi-second waits leave
observation gaps. Its summary alone cannot count every death/respawn. The
native run additionally has `flow-observer.jsonl`, a read-only 250-ms process
observer. It sees initial countdown -> active play -> results, with positive
health at level ticks 198, death at 491, respawn at 1100, death at 1616,
respawn at 2216 and death at 2619. Counts and first-observed times have the
sampling granularity of that observer. Original respawns are supported by the
final death count, not a complete continuous trace. `flow-summary.json` in each
run retains the coarser command-state summary separately.

The first native setup attempt ended in the test runner's 16-key command
limit assertion; it did not crash the game. Its log is retained as
`logs/arcade-flow-144-command-limit.log`. Splitting the setup command allowed
the subsequent complete run. A late request for an intermediate Scores capture
arrived after results navigation had already executed; the native evidence is
Results plus rematch countdown, not a reviewed native Scores page.

Bots kill the idle test player early, so these runs do not establish new
player-firing/reload acceptance. The different bot outcomes and match lengths
are not evidence of a game-speed mismatch. Captures/minimized execution and
unequal workloads also make these unsuitable for a pacing comparison. They
expand evidence beyond the prior partial Vietnam matches to one complete
Hotel match and native results-to-rematch transition. Other arenas/modes,
network peers, physical play, full-game correctness and Mac remain open.

## Aim and weapon-offset easing

The original player routine `0x25A7F0` eases offsets later consumed by the
weapon model (`player+F64/F68`) and aim direction (`+F6C/F70`). Its fractional
updates multiply error by game delta and the authored 0.1 coefficient.
Subdividing this linear update changes its settling rate: the actual original
block's first 120-FPS step towards 8 is 0.400000006, whereas subdividing the
original 60-Hz response gives 0.410533622. The failing behavioral check is in
`logs/native-aim-easing-red.log` (after a corrected test-only XMM field name).

Shared `pc_native_numeric.h` corrects only the final six coefficient
multiplications at `25ABCE`, `25AC1E`, `25AC80`, `25ACCA`, `25B13C` and `25B16D`.
For raw intervals below one tick it derives the original one-raw-tick weight
at the current game speed (`weight = coefficient * game_delta / raw_delta`)
and splits its remaining-error factor over the raw interval. This retains
original normal/slow-motion response at 60 FPS. Whole/catch-up updates, original
mode, target acquisition and authored snap branches remain unchanged. Raw
mouse packet consumption is outside this correction.

`native_aim_easing.c` executes both original easing blocks through their stores,
including the first block's snap branches. Their outgoing join instructions
are replaced by RET only in the fixture. The pinned XBE supplies original
instructions/constants. Incoming target vectors are controlled. All 16 steady
sequences pass across original 60/native 60/120/144 and normal/quarter speed.
Every step is compared with the original 60-Hz response; pause leaves offsets
unchanged. Uneven fractional intervals also match repeated original updates.
One/two/three-tick catch-up matches original results exactly. Snap thresholds,
neighboring sites, disabled native mode and limiting coefficients are covered.
For a target offset of 6 after 30 game ticks, normal-speed final values are
5.745652676 / 5.745653152 / 5.745652676 at 60/120/144; quarter-speed values are
5.712455750 / 5.712455273 / 5.712454796. The preliminary focused result is in
`logs/native-aim-easing-green.log`.

The expanded timing, float/vehicle translator, auto-look, CI-scope and Mac
backlog suites pass all 157 checks (`logs/native-aim-easing-regressions.log`).
The supported Windows Build and its built-in checks pass
(`logs/native-aim-easing-build.log` / `.err`). The binary SHA-256 is
`858ce97b290710485935f282c388ac71131eababe8244cb1b75b7147acb1a0ef`,
compiled with parent label `0c905e6` plus this revision's easing source.

The bounded native-144 What Lies Below smoke completes normally at about
97 seconds (`private/test-saves/aim-easing-smoke-144`,
`logs/aim-easing-smoke-144.log`). Yaw goes 209 -> 149 -> 209 degrees; movement
continues after pause/resume. Revolver ammunition goes 6/30 -> 3/30 -> 6/27,
matching the prior captured-input original-60/native-144 route (four synthetic
presses produce three rounds in that route). Pause changes to 1 and resumes
to 0. Health remains 35. Reviewed move/after-resume captures show the weapon,
hand, environment and HUD. The last cumulative API-present report has 12,000
guest frames with zero interpolated/other/skipped/failed frames; this includes
menus and is not a pacing or scanout measurement. Fixture and personal settings
hashes are unchanged. The separately running menu-footer-source game was not
stopped or controlled. This smoke also completes the reload/pause integration
checks that were interrupted on the preceding weapon-sway build.

Full caller/target-acquisition behavior, physical aiming feel and Mac execution
remain separate acceptance work. This is a small response-consistency change,
not a measured latency or performance improvement.

## Animation timeline and event coupling

On the `60f140c` runtime, the new `native_animation_clock.c` fixture executes
complete original US timeline updater `0x5ADC0..0x5AFAA` together with event
reader `0x61570`, bounds converter `0x585E0` and track selector `0x59CF0`.
Unlike the earlier script fixture, it obtains event bounds from the original
animation update instead of setting them by hand. The pinned XBE supplies
code and referenced scalar constants; clip metadata, root samples and the
loop callback are controlled services.

All 32 sequences pass: original 60 and native 60/120/144, normal and quarter
speed, looping and clamped clips, forward and reverse playback. Each covers
three game seconds starting at phase 0.137 of a one-second clip. Every
nonterminal advancing sample changes. Looping clips wrap three times and
produce the same accumulated three-axis root displacement at every rate;
forward loop callbacks occur three times. Forward frame-23 events occur once
for a clamped clip and three times for a loop, each no later than one update
after its authored time (plus 0.1 ms float tolerance). Pausing after every
update changes neither time nor root/callback counts and produces no event.
A three-tick catch-up across a boundary and a zero-duration clip also pass.

The updater already integrates `0x4E794C`; no runtime correction is justified
by this result. `logs/native-animation-clock.log` records the standalone
fixture. The coupled clock, script handoff, NPC dispatcher, weapon sway and
NPC weapon-event fixtures pass together (five checks) in
`logs/native-animation-clock-related.log`. Existing parametrized CI includes
the new fixture. No binary rebuild is needed for these test/document changes.
This is timeline/event evidence, not rendered skeleton or physical root-motion
acceptance. Reverse event semantics, blend transitions, real asset decoding,
full caller coverage and Mac execution remain unverified.

## Native weapon sway

The complete original weapon dispatcher `0x1c4400..0x1c4a4b` samples a sine
wave from the integer level clock, chooses a random amplitude at an authored
crossing, and passes the resulting motion value to its three weapon updates.
Before this correction the 12-second native-120 fixture repeats that value on
720 of 1,440 updates, while original 60 has no repeats. The failure is captured
in `logs/native-weapon-sway-red.log`.

Shared numeric policy changes only the final multiplication at `0x1c4539`.
`pc_native_weapon_sway` uses the existing observed fractional level clock and
the original constants and 2,048-entry sine table to resample its left operand.
The original integer samples, crossing comparison, RNG calls and amplitude
stores remain untouched. No draw advances time. Zero phase offset, invalid or
unobserved clock state, rebases and out-of-range phase conversion retain the
incoming value; disabled native mode retains the original multiply.

`tests/native_weapon_sway.c` runs the complete original dispatcher and sine
initializer, and executes both original consumer blocks through their model
C8 stores (`0x1c3659..0x1c3680` and `0x1c37cf..0x1c37e9`). The first preserves
the authored sway-disabled weapon flag. Weapon-state, input, render and other
services are explicitly mocked; these are numeric/model-store checks, not GPU
output or complete firing/animation acceptance. All three weapon slots receive
the same motion argument. Constants come from the pinned supported US XBE.

The corrected normal-speed sequence has no repeated motion values at 60,
120 or 144, preserves two RNG events at the same original tick boundaries,
and ends at the original value 0.456892014. Equal whole-time samples match the
original reference. Quarter speed, pause between every update, repeated visual
sampling, disabled native behavior, unrelated sites, rebase/unobserved-clock
fallbacks and signed-conversion overflow are covered. At quarter speed the
original lookup-table resolution still produces 416/704 repeated samples at
120/144; these are retained and are not claimed as fresh unique poses.

The broader validation run has 154 passing checks and a test-only unresolved
sway-policy reference in the player fixture's variable-site checks. An
abort-on-use boundary repairs that dependency; the player and expanded model
consumer fixtures then both pass. Logs are
`logs/native-weapon-sway-regressions.log` and
`logs/native-weapon-sway-model-fixture.log`. Existing CI includes the new
parametrized fixture. The full Windows Build and built-in checks pass; logs
are `logs/native-weapon-sway-build.log` and `.err`. The resulting binary has
SHA-256 `5bb5eab308ba236056c22ae0ca8121e7bc02e4c96eba160068020ca1682200cb`,
compiled with parent label `500f8be` plus this revision's source changes.
The bounded native-144 smoke reached What Lies Below gameplay, displayed the
revolver and hands, changed yaw from 209 to 149 degrees, moved the player,
and fired three rounds (6/30 to 3/30). The captured-input firing count matches
the preceding original-60/native-144 comparison. The window then closed via
the normal user-close path (`ExitProcess(0)`) before the reload checkpoint;
the runner consequently failed its unexpected-exit assertion. This is an
incomplete smoke, not a passing reload/pause/resume test or a crash. Evidence:
`private/test-saves/weapon-sway-smoke-144` and
`logs/weapon-sway-smoke-144.log`. The fixture and personal controls/display
hashes are unchanged. Other sessions using the separate menu-footer build
were left untouched. Mac compilation and perceptual smoothness remain
unverified.

## Button delivery after fractional pause offsets

The raw clock continues during pause while game time stops, so their carried
whole ticks can occur on different native frames after resume. The expanded
`tests/native_timing_guest.c` now runs the real timing step and input wrapper
together across 864 schedules: 60/120/144 FPS, 24 pause durations and 12 press
phases per rate. Each press is held for at least 100 ms. These runs include
9,084 frames with different raw/game tick counts. Every schedule delivers one
press edge and one release edge, retains held state for a game-tick consumer,
and delivers each edge within one original 60-Hz sampling interval. Paused
intervals advance no game time. The existing disabled-path/ABI checks remain.

The pad-poll service is mocked using the original input-record layout. This
checks clock/edge coordination, not physical-device latency, short taps below
the sampling interval or the complete weapon state machine. Existing CI
already includes this fixture in `tests/test_native_timing.py`.

Inspection of the pinned US executable also distinguishes held-input reader
`0x42760` from pressed-edge reader `0x427b0`. The generic firing call sites at
`0x1c3317` and `0x1c33af` are selected using held input; weapon-selection
handling at `0x1bf140` uses pressed edges. Therefore a mismatch between raw and
game tick phases is not by itself evidence that the observed held-fire shots
were lost. No clock alignment, edge buffering or weapon cadence change is
made on that hypothesis. Complete firing eligibility, animation and reload
caller coverage remain separate work; earlier live firing differences remain
open. Local disassembly evidence is `private/pipeline/input-audit-*.asm` and
`audit-001C3100.asm`.
The complete native timing suite passes all 50 checks, including the expanded
input schedules and existing original weapon/NPC firing, sound and cooldown
fixtures. Evidence is `logs/input-weapon-phase-checks.log`. This revision changes
only tests and evidence; the executable remains the tested `d336cd9` build.
Mac execution of the expanded fixture remains unverified.

## What Lies Below frame pacing without captures

The Windows x64/D3D11 `later-story-pacing-144` run uses the `d336cd9` beta-overlay
build, SHA-256
`0358d1725a61424d2398f09ac27f811e412c082f0b0fc9a19a73d74a2e7bc976`.
It replays aiming, movement, revolver firing/reload and pause/resume, then four
forward/backward movement pairs with mouse turns. Screenshot readback is
explicitly disabled. The isolated minimized/background run completes normally
at about 119 seconds with unchanged test profiles and personal settings.

Selection retains every interval between consecutive trace rows that both
have scene 17, flags 524296 and positive game delta. This produces two active
gameplay spans separated by pause; 613 scene rows have zero game delta and
are excluded, as are menu/cinematic transitions. The 16,573-row trace ends
before its 20,000-row cap. No percentile-based outlier removal is applied.

| Active gameplay measure | Native 144 result |
| --- | ---: |
| Intervals / summed host time | 8,987 / 62.485569 s |
| Mean fresh update rate | 143.825 FPS |
| Median / p95 / p99 interval | 7.0382 / 7.3593 / 8.0834 ms |
| Minimum / maximum interval | 3.7714 / 12.3813 ms |
| Intervals above 16.67 / 33.33 ms | 0 / 0 |
| Game seconds per host second | 1.000000 |

The last presentation record reports 16,500 guest, zero interpolated/other,
and zero skipped/failed presentations across the run. Those cumulative API
counters have a different window from the CPU timing table. Neither measures
GPU completion, monitor scanout, physical-device latency or foreground load.
This scene result supports current pacing; it does not justify a new pacing
algorithm or prove full-game performance.

This no-capture replay consumes four revolver rounds for four synthetic presses
and reloads to 6 loaded/26 reserve, whereas the earlier captured comparisons
consumed three at both original 60 and native 144. Synthetic firing results
are therefore sensitive to the run's timing; do not treat one sequence as
universal weapon cadence or assign the difference to high refresh alone.

Evidence: `private/test-saves/later-story-pacing-144/{provenance.json,
native-frames.csv,report.json,pacing-summary.json,run0.err}`,
`logs/later-story-pacing-144.log`, and the local reproducible coordinator,
commands and summarizer in `private/pipeline` (`check-later-pacing.py`,
`later-story-pacing-inputs.json`, `summarize-later-pacing.py`).

## Broader player-facing check: What Lies Below

After the user requested broader priorities, two bounded Windows x64/D3D11
runs of What Lies Below (mission 5, scene 17) exercise the same movement,
mouse-look, firing, reload and pause/resume sequence at original 60 and native
144. Both use the hit-response build from `319191e`, SHA-256
`f0a3a1e11dfc77f334961d011b861717cf911bf75d866d8e8fd5a1f06f8ec42e`.
Both exit normally after about 90 seconds with unchanged profiles.

The starting revolver has 6 loaded/30 reserve rounds. Four synthetic presses
consume three rounds at both rates; reloading restores 6 loaded/27 reserve.
Thus this particular firing sequence matches the original path; it does not
establish that every press fires or resolve the earlier Scotland pistol report.
Mouse-look endpoints match at 209, 149 and 209 degrees. The first movement
endpoint differs by about 0.000093 world units; the post-resume endpoint differs
by about 0.280 units after moving against geometry. Health remains 35. Pause is
entered and exited, and movement works afterward. This is a short input/weapon/
transition comparison, not combat, complete level progression, exact animation
frame analysis or a frame-pacing measurement (renderer captures were enabled).

Reviewed native captures show the level-selection title, visible basement
geometry, starting revolver and readable objective. Evidence is in
`private/test-saves/later-story-experience-{60,144}`, the matching logs, and
`private/pipeline/later-story-validation.json`. These checks broaden scene
coverage without reopening the deferred Scotland investigation.

## Player hit displacement and recovery

The active-player dispatcher decayed the three hit-response channels at
`player+0xf4/+0xf8/+0xfc` by the original 0.9 factor on every native update.
Their setter is `0xb9510`; walking movement at `0x259344..0x259388` adds the
channels directly to the candidate position before that decay. At native
120/144 this compresses the hit response into less game time. Correcting
only the decay would instead increase total displacement, because movement
would still add the full value more often.

Shared `src/pc_native_numeric.h` now handles both halves at six audited
instructions. Decays at `0xd64a4`, `0xd64bd` and `0xd64d6` use `q^dt`, where
`q` is the original 0.9 constant and `dt` is the live fractional game tick.
The corresponding additions at `0x259352`, `0x259368` and `0x25937f` multiply
the incoming channel by `(1-q^dt)/(1-q)`. This extends the original geometric
sum to fractional intervals, so original tick boundaries retain the same
unobstructed displacement and remaining response. One-tick arithmetic and
the disabled native path retain the original operations. The live pause gate
still prevents the whole active dispatcher from advancing at zero game time;
the new addition policy also contributes zero displacement at zero time.
No shared constant, damage amount, hit probability, collision decision,
weapon state, or saved setting is rewritten.

The expanded `tests/native_player_dispatch.c` executes the complete original
active/inactive dispatcher and complete original hit initializer `0xb9510`
and damage-response setup `0xb9270`. A world-step spy executes the three real
movement additions and stores, ending that block at its outgoing join before
collision processing. It supplies the incoming position and z register as
the original walking caller does. Collision, other world/input services,
HUD/rumble and optional camera-kick services remain boundaries. This is
coupled integration/decay evidence, not an end-to-end collision or combat test.
Constants and the selected original hit-profile table come from the pinned
supported US XBE. Existing CI already runs this parametrized fixture.

Before the decay correction, a synthetic initial channel of 8 becomes
2.789426804 at original 60 after ten ticks, but 0.972612739 at native 120.
The coupled test also rejects a decay-only correction: at quarter-speed
native 60 it produces displacement 0.623538136 instead of 0.162096575.
Local reproductions are `logs/native-player-hit-red.log` and
`logs/native-player-hit-coupled-red.log`.

With both corrections, an original profile-9 hit initializes the x channel
to approximately 0.07. After ten game ticks the original-60/native-60/native-120/
native-144 displacements are 0.455925107, 0.455925107, 0.455925018 and
0.455924898. The remaining x channel is approximately 0.02440748 in each
case; negative y and positive z channels are checked as well. Quarter-speed,
uneven intervals totaling ten ticks, a new hit after decay, paused/stale-whole
intervals, one-tick exact arithmetic, disabled behavior and adjacent unrelated
instruction sites are covered. The fixture checks that each advancing native
interval reaches the world step. Live geometry, hit interruptions during
firing and the previously observed Scotland route differences still require
separate acceptance; this fix is not evidence that those reports are resolved.

The final full Windows Build and built-in checks pass; matching symbols were
regenerated. Binary SHA-256 is
`f0a3a1e11dfc77f334961d011b861717cf911bf75d866d8e8fd5a1f06f8ec42e`,
compiled with parent label `01f99ab` plus this revision's source changes.
The broader run passes 153 checks; the expanded fixture initially needed an
abort-on-use rocking-policy boundary for its variable-site scope checks and
passes after that test-only linker repair. Evidence is in
`logs/native-player-hit-final-build.log`, its `.err`,
`logs/native-player-hit-final-regressions.log` and
`logs/native-player-hit-final-fixture.log`. The first decay-only build was
superseded by this complete build before gameplay validation. Shared source
requires no new Mac adapter; actual Mac compilation/execution remains open.

The isolated minimized `scotland-hit-decay-144` smoke attempt ended before
reaching gameplay. Its runtime log records `window closed by the user` and
`ExitProcess(0x00000000)`; the runner correctly reports an incomplete test,
not a successful gameplay check. The canonical profile remained unchanged.
Evidence is `logs/scotland-hit-decay-144.log` and
`private/test-saves/scotland-hit-decay-144/run0.err`. Live acceptance of this
build remains open; the early window close is not evidence of a crash.

The bounded retry `scotland-hit-decay-retry-144` completed normally with
unchanged profiles, movement, aiming, firing and pause/resume. Health stayed
at 35 and every sampled hit vector was zero, so this run did not exercise the
hit correction in combat. Eight synthetic fire presses consumed six pistol
rounds; that observation remains unresolved rather than counted as weapon
parity. Evidence is the run's `report.json`, `states.json`, captures and
`logs/scotland-hit-decay-retry-144.log`. At the user's request, further Scotland
investigation is deferred in favor of broad player-facing coverage.

## Scotland route under fire

Windows x64/D3D11, unchanged HUD-fix executable SHA-256
`167e9bce3b96d4ff8ceaab378d52aaadb9d0b923fa3a7a42374a1364db413397`.
The bounded runner now reads player entity health at `entity+0x1cc`, the
field written by the original damage routine exercised in
`tests/native_sniper_damage.c`. It does not modify health, position, enemies
or gameplay. Health is reported in original units, not a percentage. This
lets a navigation failure or combat death be distinguished from a frozen
simulation; it does not identify the attacker or damage cause.

The exploratory `scotland-route-144` run reached the arch, was left exposed,
and reached Game Over. Retry returned directly to gameplay; mistaken menu
inputs then paused it and entered Display settings. Those navigation mistakes
are not timing regressions. A later resumed route reached the ruined building.
The exploratory `scotland-route-60` run mapped the bends toward the castle,
received damage and emptied the pistol at the approach. It used longer
operator pauses and is not the matched baseline below.

The three `scotland-route-compare-{60,120,144}` runs replay the same bounded
route with short pauses: dock handoff; yaw 65 forward three seconds; yaw 77
forward three then two seconds through the arch; yaw 167 forward three
seconds; yaw 212, 152, 92 and 182 forward three seconds each; then eight
pistol-button presses with 0.18-second holds and 0.25-second gaps. Original
60 leaves native timing disabled. Native 120/144 use the actual fractional
clock. Renderer captures are enabled, so these are not pacing measurements.
The 60/144 repeats additionally sample movement/health every approximately
50 ms during holds; 120 records command endpoints only.

All begin at the identical dock position with health 35 and pistol 8/52.
Through the `switchback` command the largest observed endpoint separation
from the matched original-60 replay is 0.415 world units. On the following
`upper-path` segment, incoming fire and backward displacement occur in the
original-60 repeat as well as at 120/144. Reviewed captures put all three
below the upper path near the wall. The first exploratory 60 run had stayed
farther up the path, illustrating why a single route result is insufficient.
The repeated original-60 run eventually dies; both native runs remain alive
at the final observation. This does not establish equal damage or difficulty.

| Observation | Original 60 health | Native 120 health | Native 144 health |
| --- | ---: | ---: | ---: |
| Dock | 35.0 | 35.0 | 35.0 |
| Around ruin | 32.6 | 33.8 | 33.8 |
| Switchback | 31.6 | 33.2 | 33.8 |
| Upper path | 5.1 | 20.3 | 18.5 |
| Upper turn | 3.0 | 19.3 | 15.8 |
| After return-fire inputs | 0.0 (Game Over) | 12.4 | 3.2 |

The final native-120 pistol is empty; native 144 retains two rounds after
those eight presses. The original-60 player dies during that sequence and
switches to the death weapon. These are not equivalent firing conditions,
so neither shot-count parity nor a firing regression is established. Damage
reactions, collision, enemy decisions and input delivery need a controlled
follow-up. Do not claim the castle fight, jeep, or complete Scotland mission
passed from these runs. The next route should clear threatening enemies
before advancing rather than use this exposed walk as a physics benchmark.

All five sessions exit normally, retain unchanged isolated profiles and
preserve the canonical fixture. Personal controls/display hashes still match
the preceding check. No gameplay source, executable or Mac adapter changed;
Mac execution remains unverified. Local evidence includes the run folders,
matching `logs/scotland-route*.log`, `private/pipeline/scotland-route-inputs.json`,
`check-scotland-route.py`, `summarize-scotland-route.py`, and
`scotland-route-validation.json`. The matched runner SHA-256 is
`21ad347de718d4242fb76e1451954401e0859a885de18b645d2cd9b4241aaf2f`.

## Scotland aiming and shore movement

Windows x64/D3D11, the unchanged `6540b5c` HUD-fix source binary SHA-256
`167e9bce3b96d4ff8ceaab378d52aaadb9d0b923fa3a7a42374a1364db413397`.
The bounded transition runner now accepts one sequenced synthetic mouse packet
per command (`mouse: [dx, dy]`, each count bounded to +/-4096) and optional
50 ms position/clock observations during its existing <=3-second key holds
(`sample_hold: true`). Packets use the game's existing test input adapter;
this is not physical mouse delivery or input-to-display latency measurement.
The runner also records the absolute level tick and whether it is minimized.
There is no new gameplay, timing, input policy or executable change here.

The isolated `scotland-locomotion-min-{60,120,144}` comparisons select Scotland,
skip the boat intro and wait for the pistol handoff. From the identical starting
look `[110.005348, 0.001624]`, 900 horizontal counts turn to yaw 20.005346;
-450 counts then set yaw 65.005348 toward the shore path. Both results match
at original 60 and native 120/144. After two seconds forward and two backward,
`[100,50]` changes aim to `[55.005348,-4.998376]`; `[-100,-50]` restores it to
`[65.005348,0.001624]` at all rates. A paused `[900,500]` packet changes neither
position nor aim, and resume produces no retained-motion jump. The same
isolated mouse sensitivity (0.1) and original unzoomed FOV apply throughout.

Reviewed renderer captures establish that this path is unobstructed over the
short walk. It advances from the dock onto the shore, unlike the earlier
wall-facing comparison. Linear least-squares fits to 50 ms position samples
between held seconds 0.5 and 1.8 give the following world-unit/second estimates:

| Mode | Forward | Backward |
| --- | ---: | ---: |
| Original 60 | 6.189483 | 6.260947 |
| Native 120 | 6.248019 | 6.206928 |
| Native 144 | 6.281620 | 6.166030 |

These capture-heavy short estimates vary by about 1.5% between rates and
have up to 0.168 world-unit fit residuals. They do not establish exact speed
parity. Separate three-second holds without renderer dumps use the same
binary, route, isolated controls and minimized window. Fitting held seconds
0.5 through 2.8 (46 samples, about 2.26 seconds per segment) to both observer
wall time and the absolute level clock gives:

| Mode | Forward / wall second | Backward / wall second | Forward / game second | Backward / game second |
| --- | ---: | ---: | ---: | ---: |
| Original 60 | 6.256263 | 6.150714 | 6.250089 | 6.179163 |
| Native 120 | 6.255006 | 6.182455 | 6.248912 | 6.167250 |
| Native 144 | 6.243758 | 6.185598 | 6.235904 | 6.186768 |

Compared to original 60, game-time movement estimates differ by at most
0.227%. The original path itself has slightly different forward/backward
speeds over this dock/shore transition. Game/wall clock fit ratios range from
0.995362 to 1.002430; position residuals reach 0.115 world units. These short,
asynchronous observations of a discrete level clock establish close behavior
on this route, not bit-exact physics or a universal speed tolerance. Initial
acceleration, key-delivery delay and release are outside the fitted window;
final return positions are therefore not used as a speed-parity assertion.
The additional runs also reproduce the same aim angles and pause discard,
and position remains fixed after releasing movement through pause/resume.

Evidence lives in `private/test-saves/scotland-locomotion-timing-{60,120,144}`
and matching logs. Each provenance records `captures: false`, `minimized: true`,
and runner SHA-256
`393a52cd23ea1c5f5a92eee3e8edcde0990ec6baaa16b5516ca7fdfce356af71`.
No renderer BMP dumps are produced. The local
`private/pipeline/locomotion-timing-validation.json` records both fits;
`locomotion-validation.json` retains the separate captured comparison.
All six completed sessions exit normally with unchanged valid test profiles.

All three completed captured sessions exit normally and preserve their
isolated profile signatures. Tests run minimized to avoid interrupting the
user; renderer captures are not monitor scanout. An earlier exploratory run
only validated the first camera turn before its duration limit; a separate
startup run received a normal window-close exit before gameplay. Neither is
counted as a completed movement comparison. Full acceleration/collision,
vehicles, physical input, multiplayer, complete levels and Mac execution
remain unverified by these checks.

## HUD message timing

The opening comparison led to an audit of delayed objectives and HUD notices.
Original US `sub_0014A5A0` computes fractional elapsed game seconds but gates
its entire queue update on the whole game delta at `0x14A5CD`. At native
120/144, zero-whole-tick intervals are discarded. This slows both initial
message delays and their expiry, despite correct overall game-clock speed.
The gate register is replaced at `0x14A670`; its value is never used as a
counter. The shared numeric policy now opens only this audited load for
positive live game time and preserves pause and the disabled-native path.
No message duration, queue priority or sound timing constant is changed.

`tests/native_hud_messages.c` executes the complete original update,
eligibility, conflict-table and queue-limit initializer routines translated
from the checksum-pinned US XBE. Only sound services/player count are spies.
Before correction, a one-second notice lasts 2.016667 seconds at native 120
and a half-second-delayed notice has not expired after three seconds
(`logs/native-hud-before-test.log`). After correction, original 60 and
native 60/120/144 expire the first notice in 1.016667/1.016667/1.008333/
1.006944 game seconds, within one update of its authored duration. The
half-second delay, single-message queue limit, one-shot sound trigger,
sound-bound hold, pause, quarter-speed intervals and adjacent-site isolation
also pass (`logs/native-hud-after-test.log`). A separate type-6 case uses a
representative 20-second delay and the five-second lifetime observed in
Scotland; it appears/expires within one update of 20/25 game seconds at
each rate. This does not assert that every live script authors a 20-second
delay. This fixture is part of the
existing native timing suite registered in shared Windows/Mac CI; execution
requires the pinned local XBE, and local Mac execution remains unavailable.

The pre-fix live `scotland-hud-before` run uses the previous binary SHA-256
`41ca454f88d9d11279fe57cbf24fbd56dbca9b72a5d5550c154c20898a91fa43`.
The read-only `--watch-hud` observer records the sixteen bounded player-one
message slots. Between elapsed wall seconds 114.188 and 124.360, the delayed
type-6 notice advances only 5.0163 game seconds (-15.9631 to -10.9468).
The captured run includes renderer-dump overhead; it establishes lost message
time, not a precise target-rate benchmark. The isolated profile is preserved
and the session exits normally.

Full Windows Release build and built-in checks pass
(`logs/native-hud-build.log`); 133 timing/translation/CI/backlog regressions
pass (`logs/native-hud-regressions.log`). The expanded objective fixture
also passes separately. The new binary SHA-256 is
`167e9bce3b96d4ff8ceaab378d52aaadb9d0b923fa3a7a42374a1364db413397`;
its compiled label is `7783767`, built with this HUD source correction before
commit. The original-60/native-120/native-144 bounded Scotland comparisons
use that same binary and the identical command sequence in isolated profiles.
Evidence is `private/test-saves/scotland-hud-after-{60,120,144}`, matching
logs, and `private/pipeline/hud-message-validation.json`.

| Mode | Observed message seconds / wall second | Observed unpaused span |
| --- | ---: | ---: |
| Original 60 | 1.000024 | 23.266 seconds |
| Native 120 | 0.999715 | 23.265 seconds |
| Native 144 | 1.000389 | 25.421 seconds |

These endpoint measurements span the scripted delay and visible banner, with
small asynchronous sampling error. Reviewed captures show the Scotland
objective banner fading; queue observations and later captures confirm its
removal. Pause takes effect after key delivery, holds the remaining lifetime,
and resume lets the notice expire. Pickup notices also retire in each run.
All three sessions exit normally with valid, unchanged test profiles; personal
controls/display hashes remain unchanged. These checks establish this HUD
clock behavior, not full cutscene/subtitle audio synchronization, all mission
scripts, multiplayer UI, physical input, or Mac execution. The Scotland jeep
encounter remains untested at native rates.

## Scotland opening comparison

The same Windows x64/D3D11 countdown-fix binary (`a0a7214` source, SHA-256
`41ca454f88d9d11279fe57cbf24fbd56dbca9b72a5d5550c154c20898a91fa43`)
completed bounded original-60 and native-144 runs using separate disposable
Test Lab profiles. Evidence is in `private/test-saves/scotland-original60`
and `scotland-native144`, with matching logs under `logs/`.

Both runs select Scotland the Brave, reach the boat cinematic (scene 30),
skip into dock gameplay (scene 4, mission 1), fire, move, pause and resume.
The initial position and look values match exactly. A 1.5-second synthetic
fire hold consumes three pistol rounds at both rates (8 to 5, reserve 52).
A three-second forward hold ends at `[1.616877, -46.362591, 103.591850]`
at original 60 and `[1.603127, -46.362701, 103.577972]` at native 144.
The endpoints differ by approximately 0.020 world units; input delivery and
sampling are not frame-identical. Both pause states suppress input and keep
position fixed, and both resume with suppression cleared. Reviewed opening
and post-movement captures show the same dock/wall location and ammunition.
Objective-message timing and animation phase are not established by these
non-synchronized captures. Both runs exit normally with unchanged, valid
profile signatures.

This covers basic opening controls only. Movement ends at the nearby wall;
it does not establish unobstructed movement speed or collision behavior
throughout the level. The jeep encounter, companion dismount/remount, combat,
mission completion and audio synchronization remain unverified at native 144.
The native trace cap was reached before gameplay, so this run supplies no
Scotland frame-pacing result. Native 120 Scotland and Mac execution remain
unverified.

## Arcade pacing without renderer captures

After the countdown correction, both native targets were measured on the same
Windows x64/D3D11 executable (`a0a7214` source, binary SHA-256
`41ca454f88d9d11279fe57cbf24fbd56dbca9b72a5d5550c154c20898a91fa43`).
`tests/stability_transitions_game.py --no-captures` omits all renderer dump
settings and skips screenshot labels, while preserving state observations,
bounded CPU timing CSV, logs and isolated Test Lab saves. Its provenance
records `captures: false`; neither run produced renderer BMPs. No game
source or pacing policy changed for this comparison.

| Target | Measured CPU updates/s | Intervals / seconds | p50 / p95 / p99 ms | Worst ms | Game/wall time |
| --- | ---: | --- | --- | ---: | ---: |
| 120 | 119.958 | 5,752 / 47.9502 | 8.3309 / 8.7218 / 8.9563 | 12.6748 | 1.000000 |
| 144 | 143.936 | 6,869 / 47.7225 | 6.9785 / 7.3793 / 7.9785 | 11.5586 | 1.000000 |

Evidence directories are `private/test-saves/arcade-pacing-120` and
`arcade-pacing-144`; logs use the corresponding names under `logs/`.
`update-stats.json` summarizes unpaused scene-48 intervals after trace second
62 / 58 respectively. The runs include combat/death/respawn states and are
not identical camera/AI workloads. Initial firing consumes six rounds and
forward movement changes position at each rate. At 120, later observations
confirm respawn with dual pistols and a further ten rounds consumed. The 144
late weapon-37 state does not prove a second firearm check. Both sessions
exit normally and retain unchanged, valid profile signatures. Personal
controls/display hashes remain unchanged.

Earlier capture-heavy runs had p99 near 33 ms. The much lower no-capture
intervals support screenshot overhead as a substantial contributor, not a
proven isolated causal measurement. Short scene CPU samples do not establish
full-game 144 FPS, GPU completion, monitor scanout, physical input latency,
4K/ultrawide performance, or other hardware/OS behavior. Retain captures for
visual checks separately from primary pacing measurements.

## Arcade Vietnam comparison

### Match-start countdown diagnosis

A bounded replay on Windows build SHA-256
`6934be7528dfe9635b34c0ca9579f3a93e5183e2373291dec2ad9ff7a88770f2`
reproduces stationary position and unchanged ammunition at native 144
(`private/test-saves/arcade-input-trace`, `logs/arcade-input-trace.log`).
The input watch observes the suppression mask reset to zero each frame,
then set to `0xFFFFFDFF` before the next suppression call, and finally
`0xFFFFFFFF` before `sub_0025F0C0`. This rules out a missing controller-mask
reset. Original dispatcher `0x68830` applies `0xFFFFFDFF` to all four ports
while the match-start countdown is active.

At `0x69043` it adds raw fractional time to integer counter `0x4D287C`,
then truncates back to integer at `0x69051`. Sub-tick additions disappear
at 120/144 FPS, so the start state cannot finish. The site-specific shared
numeric policy consumes the timing system's accumulated whole raw ticks
for this counter. Other additions keep their fractional values; the
original-60 path is unchanged. No input mask or match state is forcibly
cleared.

`tests/native_arcade_countdown.c` executes the complete checksum-verified US
state dispatcher and original mask writer. Rendering/messages/sound services
are spies and unrelated branches abort. Before the correction, original 60
and native 60 reach state 7 at tick 184 with four cues; native 120 remains in
state 5 with zero cues after 300 ticks. After the correction, original 60 and
native 60/120/144 reach state 7 at tick 184 with the same four cue IDs. The
fixture also checks all four input masks, guest return-stack preservation,
zero raw time, accumulated multi-tick input, adjacent-site scope and disabled
native policy. The full Windows Release build and its built-in checks pass, as do 132
shared timing/translation/CI checks. Final scope/Arcade-statistics checks
pass separately. These original-code checks alone do not establish playable
Arcade.

The first corrected live run (`arcade-countdown-120`) uses executable SHA-256
`41ca454f88d9d11279fe57cbf24fbd56dbca9b72a5d5550c154c20898a91fa43`.
The match reaches state 7 and the input suppression mask clears. After the
three-second forward command the player moves from `[48.9512, 0.3954, 66.0679]`
to `[66.2618, -0.4588, 52.9205]`. Firing reduces the dual magazines from 18/18
to 13/13 with shared reserve 24; the reviewed fire capture shows these counts
and bullet impacts. Pause/resume toggles the pause state correctly. The later
weapon-37/death state means the post-resume command is not a second firearm
acceptance check. Test profiles remain unchanged; the runner exits cleanly.

`native_update_stats.py --scene 48` filters Arcade intervals explicitly; the
default remains Story scene 2. The 120 trace after second 58 includes 2,057
unpaused intervals over 19.0999 seconds: 107.697 updates/s, p50/p95/p99
8.3202/9.8263/32.5744 ms, worst 318.0451 ms. Raw/game time ratios are both
0.979667; the existing three-tick catch-up clamp discards time during long
stalls. Renderer capture and scripted input were active. This diagnoses
remaining pacing limits, not stable target performance or monitor scanout.

The final native-144 replay (`arcade-countdown-144-final`) passes match start,
movement, firing and pause/resume on the same executable. The player moves
from `[48.9512, 0.3954, 66.0679]` to `[68.0795, -0.1995, 51.7039]` during the
three-second forward command. The earlier 1.5-second fire command reduces
both magazines from 18 to 15, with reserve 24; reviewed captures agree. The
input mask clears after start and resume. The run exits normally, preserves
profile signatures and contents, and reports no crash/unresolved call. The
first 144 attempt is partial evidence only: it confirms movement, then the
player is killed by Goddard and a disappearing rotating BMP stops the test
helper. The helper now tolerates that enumeration race; no game crash occurred.

The final 144 trace after second 56 includes 1,344 unpaused Arcade intervals
over 10.3235 seconds: 130.188 updates/s, p50/p95/p99 6.9124/8.2455/33.0204 ms,
worst 37.9402 ms. Raw/game time both advance at 1.0 seconds per wall second.
These short, capture-heavy scene samples establish neither sustained 144 FPS
nor full-game correctness. Full matches, other arenas/modes, online peers,
physical aiming, stable pacing and Mac execution remain open. Personal
controls/display SHA-256 values remain unchanged and no personal FPS sidecar
was created. Disposable test disks are recycled by the runner.

### Earlier baseline

Windows x64 build `c4171db` was tested through the normal Arcade menus using
isolated Test Lab profiles: Deathmatch, Vietnam, recommended weapons/bots,
Cortez. Executable SHA-256:
`85ebe7f5665eb2426b7d80438abfe59d8fe7c70584082268ae54122b57baa2ad`.
The extended `tests/stability_transitions_game.py` records read-only player
position/look/equipment/ammunition, bounded held-key commands, command times
and executable/runner hashes. The runner hash for both sessions is
`362787dc1a8eaf727985bd05b67ef4c4fd800eb8a4e445be0e0bc1536af26e6b`.
Commands and screenshots are retained in `private/test-saves/native-arcade`
and `private/test-saves/original-arcade`; logs are `logs/native-arcade.log`
and `logs/original-arcade.log`.

The native-144 session was bounded to 300 seconds and stopped normally after
the observations; the original-60 session was bounded to 150 seconds. They
use the same helper and binary, with the explicit native-rate override only
in the native session. Both preserve the fixture and exit normally; neither
reports a crash/unresolved indirect call. The runner's generic completion
message certifies process/fixture handling, not the scenario's gameplay.

Native-144 **fails Arcade gameplay acceptance**. At 179.391 seconds the arena
is visible and scene is 48, flags 34571298, player at
`[48.951206, 0.395421, 66.067909]`, dual weapon ID 2 with 18 rounds in each
magazine and shared reserve 24 (total 60). Three-second forward, strafe and
fire commands, including another fire attempt after pause/resume, leave the
same position and ammunition at 244.969 seconds. Reviewed captures
`match-start.png` and `refire.png` show the same view and full magazines.
The pause menu opens and closes normally. A separate read-only clock sample
(`live-clock-observation.json`) finds update IDs 39097/39169/39238 and level
ticks 4764/4794/4824 at roughly half-second spacing, positive fractional game
deltas near 0.42 and pause count zero. The title is not globally paused;
advancing its clock has not produced responsive player gameplay here.

Original-60 shows combat, death and respawn (`spawn-confirmed.png` includes
the dead player). During the later three-second fire command, the same ammo
type's total falls from 60 to 51, with 14/13 rounds loaded and shared reserve
24; the visible `game-fire.png` matches those values. Positions change and
pause/resume succeeds. Respawns and combat prevent treating these positions
as a controlled movement-distance comparison. Menu timing also differs: the
initial queued original commands stopped at the Ready screen, so their early
move/fire labels are excluded; subsequent confirmed scene-48 observations
provide the comparison. No network peers were used: this is local Arcade,
not online multiplayer validation.

The native CPU trace exhausts its 20,000-row cap at 143.542 seconds, before
the match, so it cannot supply gameplay interval percentiles for this run.
Late FPS reports show 140.4 swaps/s in native mode and 59.4 in original mode;
these are not evidence of correct simulation or an isolated performance
comparison. The separate user session remained open and responsive. Personal
controls/display hashes stayed unchanged; each runner recycled its disposable
disks/lock and preserved the canonical profile.

Next investigation: reproduce with faster state-confirmed menu navigation,
compare native 60 against native 144, and inspect the Arcade simulation/input
dispatch and any temporary delta scopes using original code. The frontend
input gate is a candidate, not a confirmed cause. No production correction,
Arcade pass, full-match completion or Mac result is claimed from this work.

## Terminal mouse fractional cadence

The follow-up to camera input found two independent losses in
`src/pc_terminal.h`: its separate consumption did not rebase the mouse batch,
and its pause guard rejected zero whole ticks even when native fractional
time advanced. Rejected terminal motion is deliberately consumed to prevent
it turning the hidden player or leaking on terminal exit, so those packets
were lost rather than deferred to the next tick.

`tests/native_terminal_mouse.c` executes the shared helper against guarded
guest memory. In the original helper, a one-second 1,000 Hz stream requesting
100 horizontal and 25 vertical pointer units produces only 39.1 and 9.775
units at native 144. At quarter speed the horizontal move falls to 9.5 units.
Original fixed-60 produces 87.4 horizontal units because of the separate
buffer-age bug. Evidence: `logs/native-terminal-before.log`.

Shared `pc_input_consume_motion` now returns the previous batch age while
clearing counts and rebasing the empty batch. Both camera aiming and terminal
movement use it. The terminal helper takes explicit native-mode selection from
the existing shared rate lookup in each host adapter. Native mode requires
positive finite fractional game time at `0x4E794C`; original mode retains its
whole gate at `0x4E7944`. A zero fractional delta rejects movement even when a
stale whole counter is positive. Counts remain unscaled by delta, zoom or
inversion, with the original pointer bounds and inactivity reset.

Forty-two cases combine original 60 and native 60/120/144, normal/quarter-speed
native clocks, 125/1,000/8,000 Hz packet schedules and uniform/uneven frame
intervals. Every corrected path retains the requested displacement within
0.003 pointer units. Movement occurs on exactly the frames receiving packets;
low-poll-rate input can legitimately leave some uneven frames without motion.
Additional cases cover native pause, invalid/nonfinite time, stale positive
whole ticks, original disabled-native behavior, all four local input indices,
catch-up without multiplying counts, scripted/disabled pointer descriptors,
ownership mismatch, stale input and return to the gameplay camera without a
stored jump. The original terminal routing tests and all 48 camera packet
cases still pass (`logs/native-terminal-fixed.log`).

Windows and SDL adapters also have synthetic integration cases that select
native 144, exercise a zero-whole live interval and stale-whole pause, then
switch back to original mode. Mac execution remains unverified. These fixtures
do not drive a live mission terminal, measure hardware input latency, or
establish pixel-level pointer smoothness. All 161 shared timing,
terminal/automatic-look/menu input, float/vehicle-join, CI-scope, backlog and
XMV-stop checks pass in 77.78 seconds (`logs/native-terminal-shared.log`).
The complete unchanged patch stack verifies with zero updated runtime files
(`logs/native-terminal-patches.log`).

The full supported Windows MSVC x64 Release build of `c4171db` passes its
built-in checks, including the real Windows adapter's five native/original
selection, pause and resume cases (`logs/native-terminal-build.log` / `.err`).
Executable SHA-256:
`85ebe7f5665eb2426b7d80438abfe59d8fe7c70584082268ae54122b57baa2ad`.
Matching crash symbols were generated. The SDL adapter integration case is
present in source but has not executed on Mac. No new live terminal scene or
gameplay timing measurement was performed for this source checkpoint; the
previous opening replay did not exercise a terminal and is not acceptance
evidence for it.

The user's PID 38248 remains responsive on the retained `3c2fcc2` executable;
the normal executable is ready for the next launch. Controls/display hashes
match `private/pipeline/terminal-protected-before.json`. No interactive game
was closed, and automated checks used fixture/disposable data.

## Raw mouse packet cadence

The native-refresh aiming audit found an input-buffer age bug in shared
`src/pc_input.c`. `pc_input_motion` drops an unconsumed batch after 50 ms to
avoid storing a long turn while gameplay/camera updates are suspended.
`pc_input_look` consumed its counts without resetting that age. At ordinary
frame rates, a new packet could therefore clear other fresh packets already
queued for the same frame using a timestamp from a previously consumed batch.
The resulting count loss depended on packet timing and camera update rate.

The correction saves the incoming timestamp for the existing stale-batch
check, then rebases the empty buffer to the current consumption time. Normal
camera updates retain all packets received between them. When no camera
consumes input, the original 50 ms discard still bounds accumulated movement;
the existing 100 ms stale-look rejection and focus/capture clearing remain.
Mouse sensitivity, FOV scaling, pitch limits and counts-based yaw are unchanged.
This helper is used by both Windows `pc_controls.c` and native SDL
`input_sdl.c`, including the original fixed-60 mode. No platform fork or new
timing gate is added.

`tests/native_mouse_cadence.c` executes that actual shared input code, with
packet arrivals independent of camera updates. For a one-second 1,000 Hz
stream containing 1,000 horizontal counts at sensitivity 0.1, the original
60/120/144 update paths turn 87.4/93.8/93.3 degrees instead of 100. The fixed
paths turn 100 within 0.0002 degrees of float accumulation error. Twelve
uniform unzoomed rate combinations expose loss in eleven original cases;
the original 125 Hz mouse / 144 Hz camera happens to retain all counts.
Evidence: `logs/native-mouse-cadence-before.log`.

The expanded fixture passes 48 combinations: 60/120/144 camera updates;
125/500/1,000/8,000 Hz packet arrivals; uniform and alternating +/-2 ms frame
offsets; normal view and half-FOV inverted pitch. Each run delivers the same
total horizontal/vertical counts, checks the analytical end angles and verifies
that a second consumption cannot replay the same motion. Additional checks
cover stale buffered motion, fresh input after a stall, bounded continuous
motion while no camera consumes it, focus loss and capture transitions.
Evidence: `logs/native-mouse-cadence-fixed.log`. The test is registered in
the existing shared native timing suite. These are synthetic packet schedules,
not measured physical-device delivery, end-to-end input latency or live
camera-pose acceptance. Actual Mac execution remains pending.

All 153 shared native timing, float/vehicle-join, CI-scope, backlog and XMV-stop
checks pass in 69.51 seconds (`logs/native-mouse-shared.log`). The unchanged
complete patch stack verifies with zero updated runtime files
(`logs/native-mouse-patches.log`). Seven terminal routing, automatic-look and
menu-mouse checks pass (`logs/native-mouse-context-checks.log`). The full
supported Windows Release build of `149bef9` and its built-in checks pass,
including the real host input adapter/guest-camera integration checks
(`logs/native-mouse-build.log` / `.err`). Executable SHA-256:
`530861ca93dd9241a1d9247a338e40495e307810b71b828c118cb3e0b9f88a94`.

A 120-second native-144 isolated replay passes the opening firearm handoff,
hand animation, movement, firing and pause/resume checks. Evidence is
`logs/native-mouse-gameplay.log` and the `native-144-mouse-cadence` records in
`private/test-saves/high-refresh`. Its runner hash is
`f328225561af2c719afe184bb0299417f367572ea043cbc7636354ff187764e1`.
After 60 seconds, 7,934 active Story CPU intervals span 56.524 seconds: mean
140.365 updates/s, p50/p95/p99 7.0401/8.4030/9.4087 ms, maximum 35.5394 ms.
Game and raw time both advance at 1.000000 seconds per wall second. This run
uses sparse synthetic replay events; the independent packet fixture above,
not this gameplay replay, establishes the dense-packet count-loss correction.
The retained end capture shows the equipped pistol and 80 reserve / 16 loaded
rounds. It also contains an orange streak at the top right; the deferred trail
report is not cleared by this smoke test.

The user's previous `3c2fcc2` session (PID 38248) remained responsive on its
retained executable throughout build and validation. Thus the timing sample
includes another running game and is not an isolated performance benchmark.
Controls/display hashes are unchanged and the runner cleaned its disposable
disks/lock. The normal executable is ready for the next launch; no forced
restart occurred.

Follow-up input audit: `pc_terminal_mouse` consumed motion separately in
`src/pc_terminal.h`. Its buffer-age handling and integer game-delta gate are
unchanged by this camera fix. Existing terminal routing checks passed, but did
not cover native fractional pointer cadence. The separate reproduction and
pause-preserving correction are recorded above; live terminal visual
acceptance remains open.

## Latest-build opening comparison

On Windows x64, the latest production source `3c2fcc2` was compared using
the same executable and runner in two 120-second isolated Test Lab replays:

```
python tests/native_refresh_game.py --fps 60 --label movie-strip-regression --stress --pause-check --seconds 120 --windowed --capture
python tests/native_refresh_game.py --fps 144 --native --label movie-strip-regression --stress --pause-check --seconds 120 --windowed --capture
```

Executable SHA-256:
`16ddf892fc209737f682345c5d7203945bc531a52dadb471e9f231c180c753f7`.
Runner SHA-256 at both executions:
`060b1424dab281a9cbaffd85c0eb9609fd0666f5fa50b254cbbab1c1cea98492`.
The supported US XBE hash is checked by the runner before launch. Both runs
completed, passed their gameplay/pause checks and were terminated at the
requested bound by the runner (child exit 1 is the Windows termination result,
not an observed crash). Logs are `logs/native-latest-original60.log` and
`logs/native-latest-144.log`; full records use the corresponding
`60-movie-strip-regression` / `native-144-movie-strip-regression` prefixes in
`private/test-saves/high-refresh`.

| Observation | Original 60 | Native target 144 |
| --- | ---: | ---: |
| First sampled firearm handoff, seconds after launch | 72.844 | 72.328 |
| Total ammunition, start/end of 85–115 s action window | 128 / 96 | 128 / 96 |
| Observed hand animation clips in action window | 1180, 1188, 1190 | 1180, 1188, 1190 |
| Movement, hand advancement, pause/resume | Pass | Pass |

The half-second observer and wall-time input replay do not prove identical
handoff timing or trajectories. Final sampled positions differ by about
0.4 game units; movement parity remains broader than these smoke checks.
The selected original frame `60-movie-strip-regression-frame_s00006900.bmp`
and native frame `native-144-movie-strip-regression-frame_s00016560.bmp` were
reviewed: both show the opening canyon, an equipped visible pistol and
80 reserve / 16 loaded ammunition. They are not synchronized frame pairs or
proof of perceptual smoothness.

`tools/native_update_stats.py` measures 7,936 unpaused Story CPU update
intervals after 60 seconds, totaling 56.5248 seconds: mean 140.399 updates/s,
p50 7.0356 ms, p95 8.3620 ms, p99 9.3273 ms, maximum 35.1005 ms. Summed game
and raw deltas both yield 1.000000 seconds per wall second. The original path's
last five-second FPS report is 59.7; it has no equivalent detailed CPU trace
in this run, so this is not a percentile comparison. A separate interactive
game was open during each test (older `b3c30cf` during original-60, latest
`3c2fcc2` during native-144); this is not an isolated performance benchmark.

Presentation records show guest-tagged API submissions, no interpolation and
no reported presentation failures. They cannot alone prove fresh content:
the movie investigation below demonstrates repeated guest-tagged pictures.
The runner's assertion wording and its test name now reflect that limitation;
the checks themselves are unchanged. Fresh update cadence is measured
separately by the native CPU trace.

The presentation-check unit test passes, as do all 66 backlog/CI-scope checks
(`logs/native-latest-evidence-checks.log`, `logs/native-latest-doc-checks.log`).
Only test wording and evidence documentation changed after these live runs;
no production rebuild is required for this checkpoint.

The user-requested latest-build session (PID 38248 at validation) remained
responsive throughout the native run. Personal controls/display hashes match
`private/pipeline/gameplay-comparison-protected-before.json`; disposable
test disks and the run lock were cleaned by the runner. These replays disable
startup movies and skip the intro. They do not trigger/accept the new strip
distortion visually, test the later bridge encounter, resolve the deferred
enemy/trail/sniper reports, establish input latency or GPU/scanout timing,
or validate later levels, other modes, multiplayer or native Mac execution.

## Opening-sequence regression

The original timing and native timing at 60 FPS reach firearm/HUD gameplay.
Read-only sampling of the original 60 FPS reference shows weapon ID 14 equipped
by about 80 seconds and retained through the movement/firing replay.

At native 144 FPS, captures show unarmed hands and no firearm/HUD. Additional
sampling established that movement and hand animation can progress: the
problem is more specific than a completely frozen animator. The opening weapon
handoff/progression remains unresolved. Do not apply a forced weapon grant as a
substitute for repairing the original behavior.

The runner now records player position, hand/body/weapon clip times, equipped
weapon IDs and weapon actions. Its first-mission stress checks require actual
movement, advancing/changing hand animation and the expected firearm equipped.
The checks reject a frozen scene and an animated but unarmed player. They are
bounded smoke checks, not measurements of correct weapon cadence or full-game
progression. Unit checks for the timing hook and observations pass eight cases.

A 150-second run without requesting an intro skip remained in the opening
cinematic and did not reach Story gameplay. It is insufficient to isolate a
skip-specific regression. A subsequent 300-second native 144 run reached Story
at 157.609 seconds without skipping and remained equipped with unarmed weapon
ID 37 through 299.734 seconds. Thus the failure also occurs when the cinematic
finishes naturally. It is not explained solely by the skip action.

The older runner's 300-second metadata passes only its scene/no-crash/native
hook assertions. A separate post-run handoff check evaluates the actual
observations: original 60 FPS passes, native 144 FPS fails. Local records are
`60-handoff-analysis.json` and `native-144-natural-intro-handoff-analysis.json`
under `private/test-saves/high-refresh`. New runner versions require the
firearm once at least a minute of Story-player observations is available,
including runs without the movement/firing stress replay.

Natural-intro runs have a separate filename tag to preserve their scope.
Validation records include the executable hash, runner hash, selected mode and
explicit checks. Test data stays isolated from personal saves/settings, and
the canonical Test Lab fixture is preserved.

### Fractional script-event repair

The original US level-script dispatcher `sub_00228870` returns immediately
when the integer game delta is zero. Its first-mission callback
`sub_00218910` grants loadout 25 (firearm ID 14) when the RBS1 actor's animation
`0xA1C` crosses the `0x2000` event. The event reader `sub_00061570` examines
only the current and previous animation times via `sub_000585E0`. Skipping a
fractional frame can therefore lose a crossing permanently.

The shared load policy now changes the dispatcher load at `0x228880` to test
the live fractional game delta. It does not modify guest integer timers or
other reads of the same delta. A compiled test executes the original
dispatcher, handoff callback and animation-event readers with mocked asset
lookup and inventory side effects. It reproduces missed events with the
old gate, catches the event exactly once at every phase of the 144:60 cadence,
and checks pause, original-mode behavior and stack balance. Nine timing tests
pass. The recompiler suite passes 352 tests and 87 subtests, with 41 skips.

Windows build SHA-256
`f74f5b15364a1d742a37c6e7cd8500cf665c19ab949127329954b174e6ee6006`
includes this repair. Its 120-second native 144 stress/pause/capture run now
reaches the pistol, HUD and next objective, but first equips at **93.579 s**.
The original 60 reference equips at **73.047 s**. Movement, changing and
advancing hands, pause/resume and eventual handoff pass; the stress-window
requirement to already have the firearm fails. Correct opening timing remains
unresolved. These diagnostic runs are not controlled performance benchmarks.

This run also exposed a runner reporting defect: its final equipped check
reused the stress-check key, overwriting a failure in metadata even though the
terminal assertion failed. New runners use `opening_firearm_equipped_at_end`
separately and test this late-handoff case. Preserve the old metadata as
historical output, but use `native-script-events-analysis.json` for its
corrected post-run assessment. Local evidence is under
`private/test-saves/high-refresh`; selected images are
`native-144-unarmed-before.bmp` and `native-144-handoff-after.bmp`.
Build, unit and runtime logs use `logs/native-script-events-*`.

Two further runs on the same repaired executable omit stress and pause inputs
and sample the RBS1 actor. Original 60 equips at **73.063 s**; native 144
equips at **89.578 s**. During comparable portions of walk clip 42, the
reference covers about **3.020 world units/s** and native covers **1.240**,
a **2.436×** difference. The native animation itself advances about one second
per wall second. This separates slow actor translation from the repaired
event delivery and identifies the next motion audit. The compared spans,
positions and source hashes are saved in `native-script-events-npc-comparison.json`.
The actor observations come from `60-handoff-loadout-gameplay.json` and
`native-144-handoff-gameplay.json`; these are bounded first-mission diagnostics,
not full-game timing acceptance.

### NPC movement dispatcher

The next audit found a second whole-tick pause gate: `sub_0015EAE0` loads
the integer game delta at `0x15EB0C`, returns when it is less than one, and
otherwise overwrites that register. This dispatcher advances AI timers by
the fractional delta and calls the movement state dispatcher. Animation
root displacement has already been generated for each fractional update;
skipping its consumer loses those updates.

The shared policy now treats this specific load as a live fractional-time
gate too. A compiled test executes the original dispatcher and verifies its
actual elapsed-time counters and movement callback cadence at 60, 120 and
144 updates/s, plus pause/resume, bounded catch-up, register/stack preservation
and unchanged integer fields. It reproduces the old counter slowdown.
All **10 native timing tests** pass. Windows Release and its built-in checks
pass. The new executable SHA-256 is
`614eec11e1896273a4424988893b0a298122149c494136c159c26f9f7fb9c5d5`.

Its 120-second native 144 run passes all current runner checks, including
equipped-before-stress, movement, advancing/changing hands and pause/resume.
First firearm equip is **72.063 s**, compared with **73.063 s** in the earlier
original-60 reference. Matching clip-42 spans from 56.047 to 58.047 seconds
measure **3.02986 versus 3.03783 world units/s**, a **0.26%** difference.
These spans precede stress/pause inputs. This is evidence of restored opening
movement, not exact event parity or full-game timing acceptance.

A subsequent **300-second unskipped** run on this same build passes the
eventual-handoff check: Story observations begin at 157.609 s, firearm ID 14
first appears at **182.125 s** and is retained through 299.750 s. The earlier
unskipped failure remained unarmed through the end of its 300-second test.
This confirms restored progression without the skip action; matching
cinematic duration against original 60 FPS still needs a reference run.
Evidence uses the `native-144-natural-intro-handoff-none` prefix and
`logs/native-npc-natural-intro.log`. No stress/pause replay was used.

The first attempted diagnostic stopped at 50 seconds because its fixed NPC
watch address no longer matched the actor allocation. It did not pass.
The successful rerun uses `--trace-handoff --handoff-watch none`: bounded
read-only actor sampling through the live tag lookup, without a fixed memory
watch. Evidence and source hashes are in `native-npc-dispatch-comparison.json`
and `native-npc-144-handoff-{gameplay,validation}.json` under the isolated
high-refresh test directory. Logs use `logs/native-npc-dispatch-*`.

Before this NPC change, the combined existing-fix, timing and patch-transition
suite passed **120 tests in 404.79 s**. The command includes the suites listed
above plus `test_native_timing.py` and `test_patch_updates.py`; output is in
`logs/native-script-events-regressions.log`. This remains bounded source
coverage, not full-game or Mac validation.

### Player movement and damping

The preserved original-60 stress replay moves the player at about 6.25 world
units/s during steady forward motion, while the NPC-repaired native 144 replay
still moves the player at about 2.6. The player dispatcher `sub_000D5EB0`
has its own whole-tick pause gate at `0xD5EBF`, followed by fractional movement
and timer consumers. Its integer load at `0xD6251` is a separate state update
and must retain its original value.

The player gate now uses fractional time. The same routine halves
`player+0x1008` at `0xD5F60` once per update; running that unchanged at 144 Hz
would alter its half-life. A generic optional scalar-multiply hook keeps
ordinary multiplication as its default. The title policy changes only this
audited site to use `factor ** fractional_delta`, preserving the original
decay per game second. Other constants and multiplications remain exact.

The original player routine is compiled and executed in a test that checks
its real timer and damping arithmetic, pause/resume and bounded catch-up at
60/120/144. World/AI helpers are mocked on the inactive-player branch; this
does not establish full player physics. **11 native timing tests** and
**352 recompiler tests with 87 subtests** pass (41 skips). Windows build and
its built-in checks pass. The runner now samples loaded/reserve
ammunition and requires consumption during its firing replay; reload-only
transfers cannot satisfy that check.

Build SHA-256
`a52dc8bbd81f92f27d203cce27ac05fec8ee9f507863106ad7831493660736b6`
passes 120-second stress/pause runs at native 144 and original 60 FPS.
In the unobstructed forward segment around 86.56–87.56 seconds, speed is
**6.25801 versus 6.25497 world units/s** (0.049% difference). The previous
NPC-only build moved at 2.61088 in that segment. Both current runs start with
16 loaded and 112 reserve rounds, have 12/112 near 90 s and 0/112 near 100 s,
and finish reloaded at 16/80 near 115 s: **32 rounds consumed**. First handoff
is 72.047 s native versus 73.063 s reference. Every runner check passes.

The longer walking path diverges as the runs take slightly different headings
and approach obstacles; it is not used as an unobstructed speed comparison.
Exact acceleration, collision behavior, shot-event timing and other weapons
remain unverified. These are behavioral diagnostics with other test processes
on the host, not controlled performance benchmarks.
Evidence/source hashes are in `native-player-comparison.json`, current
`native-144-handoff-none` and `60-handoff-none` records; logs use
`logs/native-player-*`. The earlier NPC-only records were retained separately
as `native-npc-144-handoff-*`.

The same executable/runner also passes native **60 and 120 FPS** stress/pause
replays. Measured steady speeds are 6.25804 and 6.25385 world units/s
respectively, both within 0.05% of the original reference; both finish at
16 loaded / 80 reserve rounds. First handoff observations are 72.547 s and
72.047 s. The four-mode evidence and hashes are in
`native-player-rate-matrix.json`. The bounded runner terminates its child at
the deadline, producing exit code 1 on Windows; `completed` plus the explicit
checks distinguish these planned stops from early exits.

Refreshed current-source suites pass **91 prior-fix tests in 62.53 s**,
**20 patch-transition tests in 320.26 s** and **11 timing tests**. Logs are
`native-player-preserved-fixes.log`, `native-player-patch-regressions.log`
and `native-player-unit.log`. Their combined scope is 122 tests; the
recompiler's 352 tests/87 subtests are recorded separately.

## Completion gate

Keep each failed or unrun scene check open. Repair new regressions, then rerun
the relevant prior-fix check in original and native modes. Keep frame pacing,
animation/event timing, animated texture behavior, visual fidelity and
gameplay progression separate in the evidence. Completion requires the full
[goal and acceptance criteria](native-high-refresh.md#goal-and-acceptance-criteria).

### Main integration validation

Checkpoint `5830f12` has been integrated with fetched main `7b0838a`. Both sets
of validation notes and Mac backlog entries are retained; the native entry
was assigned MAC-058 because MAC-057 already tracked optional texture delivery.
It is now **MAC-060** after PR #285 incorporated main's crane/text entries
MAC-058/059. Main's fixed-60 default, retired interpolation engine, launcher
repairs and packaging changes remain in place.

The native schedule now uses `pc_native_clock.h`, independent of the retired
presentation helper. An additive patch removes the old native interpolation
guard before main's retirement patch runs; the final renderer has inert
interpolation entry points. The native guest timing and flip-gate bypass
remain opt-in. Legacy timing settings still migrate to main's fixed values.
An upgrade test covers the pre-merge native checkpoint and idempotent reapply.

The bounded runner is now `tests/native_refresh_game.py`, with only original
60 or native 60/120/144 modes. Native acceptance requires recorded guest
presents and zero interpolated/other presents, rather than an initialization
message. `--label` keeps integration evidence separate from earlier captures.

Windows Release and its built-in checks pass. **14 timing/default/retired-engine
checks**, **119 patch/launcher/Mac-tool tests with 13 subtests** (two host-dependent
skips), and the backlog integrity test pass. Current integration build SHA-256:
`95660e9e93227cbb05e2f1b0a66bd97caa609c9c8cbef4ad2e68fc79f4bd23e2`.

The original-60 and native-144 120-second opening-level replays both pass
movement, hand animation, firearm handoff, ammunition use and pause/resume.
Native presentation records contain guest frames only, with no failed presents.
Matching one-second walking spans measure 6.256 and 6.262 world units/s
(0.10% difference). First firearm observations are 73.062 and 72.063 seconds;
both runs end with 16 loaded rounds and 80 in reserve. These observations
do not establish identical event cadence, collision behavior or full-level
correctness. The native run overlapped source tests, so it is not a controlled
performance benchmark.

Logs use `logs/native-main-*`; isolated evidence uses `main-integration`
labels, with source hashes in `native-main-comparison.json`. Real Mac execution,
broader levels and texture/camera animation acceptance remain pending. The
runtime overlay's cap label still reflects the fixed console flip-gate setting
even when native pacing bypasses that gate; its label needs a separate repair.

### Scripted camera fractional motion

The original US `sub_000C73C0` translates a camera using the fractional delta
but skips the entire operation when the whole-tick field is zero. It also
returns zero in that case; its caller at `0xCE076` can interpret this as
completion and clear a transition flag. The direction routine `sub_000C7610`
has a similar gate, followed by per-call angular easing.

The native load policy lets these two routines consume live fractional time.
The division at `0xC7651` converts the original easing coefficient `a` to
`1 - (1 - a)^delta`, using stable logarithm/exponential arithmetic and keeping
the original one-tick value. Other divisions and the disabled native path
retain their original arithmetic. The additive camera patch adds a
site-aware scalar-division hook; no generated game C is edited.

`tests/native_camera_motion.c` executes both original routines with mocked
collision/sector lookup. It reproduces translation loss under the old gate,
checks that a distant target does not report completion on a fractional step,
and compares camera translation and angular decay at 60/120/144. It also
covers pause, quarter-speed time, catch-up, angular wrap, direct target snaps,
stack/register preservation and unchanged integer deltas. A gate-only repair
failed the angular comparison before the easing hook was applied.

The native timing, bounded capture cleanup and existing auto-look tests pass
**15 cases**. Recompiler checks pass **352 tests and 87 subtests**, with 41
skips; **37 original floating-point/vehicle regression checks** and **22
patch-migration/backlog checks** also pass. Windows Release and its built-in
checks pass. Build SHA-256:
`6609f626278bc4ce6d77c9bd1597c74c7e4a748973a36ebcecc1ef6d3ad5acb1`.

Both 240-second natural-intro runs pass scene/handoff/no-crash checks, with
guest-only presents and no presentation failures in native mode. First
player/firearm observations are 158.531/183.625 s for original 60 and
157.625/182.141 s for native 144. Subtracting embedded animation time from
the first observed NPC idle and handoff clips estimates the intervening
span at 20.678 and 20.599 s (0.079 s difference). These estimates help
separate initial loading delay from clip progression, but do not prove exact
event timing or a complete animation-rate match.

The native trace retained 7.309 s of unpaused Story intervals after 160 s:
141.748 mean CPU updates/s, p50/p95/p99 intervals 7.002/7.781/8.428 ms, and
34.706 ms worst interval. Both game and raw deltas sum to 1.0 second per
wall second. The timing summary now reports those two clocks separately,
including quarter-speed test coverage. This is a bounded captured diagnostic
run, with source tests overlapping part of the intro; it is not sustained
144 FPS or monitor-scanout acceptance. The 20,000-row trace cap limits the
recorded gameplay interval after the long intro.

Local evidence is `native-camera-natural-comparison.json`, its source files
and their hashes under `private/test-saves/high-refresh`, plus
`logs/native-camera-*`. Reviewed end-of-run captures show the firearm/HUD and
rendered opening environment on both paths; they are different moments, not
matched visual-fidelity comparisons. The runner now explicitly recycles
early and rolling captures to retain only three images per run, rather than
relying on the runtime's KEEP_LAST option, which also retains early frames.
Camera samples from `--trace-camera` are asynchronous half-second reads and
can observe temporary rendering cameras. The comparison identifies the
dominant Story camera pointer; it cannot establish per-frame visual smoothness
or prove that every camera path exercised the repaired routines.

The broader camera audit remains open. The mode-27 branch at `0xCD70F`
is covered by the later camera-track checkpoint below. Do not broaden the load policy without following register uses,
per-call constants and the affected original behavior. Moving targets, real
collision geometry, vehicle cameras, multiplayer cameras and full cinematics
need scene checks.

### Camera effect timer, collision cadence and kick decay

The follow-up covers the outer gate at `0xCEA8D` and `sub_000C79C0`.
The routine's effect timer and phase increments already use fractional game
time, but the caller skipped zero-whole-tick intervals. A kick could also be
absent from a newly supplied base pose on those skipped frames.

Inside the routine, `0xC7D73` and `0xC7E0C` bound a whole-tick decay loop for
camera fields `+0x3A4` and `+0x3AC`, with factors at `+0x3A8` and `+0x3B0`.
The original kick setter's observed caller supplies a 0.8 factor for both.
The native policy runs this loop once per advancing interval and raises those
factors to elapsed ticks at multiply sites `0xC7DA8` and `0xC7DC5`. It preserves
the original one-tick multiplication, threshold clamp and guest integer fields.
Pause skips the entire effect evaluation.

`tests/native_camera_effects.c` executes the complete original effect routine,
its three original matrix helpers, and the original caller's gate/call slice.
Only a terminal RET is added at that slice's join; the complete caller is not
executed. The fixture supplies a fresh base pose and unobstructed collision
services. It reproduces lost timer time with the old gate and verifies live
offset application, collision-call cadence, kick decay, thresholds, quarter
speed, three-tick catch-up, flag-driven reset, pause and stack preservation at
60/120/144. The two loop loads are tested together with the decay multipliers,
not treated as independent pause gates.

The combined native timing, camera, auto-look and original floating-point/
vehicle suites pass **53 tests**. Windows Release and built-in checks pass.
Build SHA-256:
`5820d4f6bd4dda225bf7181cb67f638e1712d7b1a2bcbbf5ddbdc57a0c307d95`.
Logs use `logs/native-camera-effects-*`; camera observations now include effect
time, kick amplitudes and decay factors. These asynchronous samples remain
diagnostics, not per-frame or complete collision acceptance.

The separate 120-second original-60 and native-144 movement/firing/pause runs
both pass every runner check. Walking measures 6.255391 and 6.259301 world
units/s (0.063% difference), and both finish with 16 loaded rounds/80 reserve.
There are no nonzero kick samples in either asynchronous camera trace; live
kick acceptance is therefore still pending. The unprofiled native interval
after 85 seconds contains 4,635 intervals over 34.465 seconds: 134.484 mean
updates/s, p50/p95/p99 7.078/10.285/11.433 ms and a 12.756 ms maximum. Both
game and raw deltas sum to one second per wall second. Five-second windows
fall to roughly 111 FPS during movement/firing, so sustained 144 is unmet.
Neither run captured frames or enabled CPU profiling. Source hashes and
comparison details are in the ignored `native-camera-effects-comparison.json`.

A separate 120-second `camera-profile` replay passes all checks. Its final
200-Hz sample contains 19,976 main-thread samples, 16,133 on CPU, including
4,706 leaves in `NtQueryVirtualMemory` and 2,513 in `sub_003D6690`. Repeated
memory queries in text/camera call chains warrant causal investigation.
Optimized static-function names may be folded; names such as `fprintf` or
`pc_online_disconnect` in a chain do not establish that those named operations
caused the cost. These totals include startup, loading and menus and do not
measure GPU completion. Runner metadata now explicitly records profiling,
capture, resource-sampling and PresentMon flags; the obsolete GPU-trace option
was removed because the current runtime no longer implements that hook.

### Text wrapping query overhead

Following the profile chain into original `sub_00040C10` shows that its loop
calls the prompt-aware width routine while building a stack-local fragment.
The prompt wrapper previously queried both its argument stack and that text,
even when one readable mapping covered both. `pc_prompt_guest.inc` now reuses
the validated forward stack span within that one wrapper invocation. Text
outside the span receives a fresh query; rewritten scratch storage retains
its separate write-protection check. Nothing is cached across guest calls.

Windows real-VM tests confirm one read query instead of two for an ordinary
stack-local fragment, translation/ABI preservation, fallback for text below
the stack, strings crossing differently protected regions, no-access fallback,
argument frames crossing a boundary, and fresh checks after protection changes.
Windows Release and all built-in checks pass. Equivalent stack-local cases
were added to the Mac self-test; that host remains unavailable.

The unprofiled 120-second native replay passes all gameplay checks and ends
with 16 loaded rounds/80 reserve. The interval after 85 seconds averages
138.101 updates/s across 34.728 seconds, compared with 134.484 before this
change. Its p50/p95/p99 are 7.023/9.040/10.121 ms, with a 12.819 ms maximum.
Both clocks retain a 1.0 game/raw second per wall second. This single before/
after comparison suggests improvement but is not a repeatability claim;
five-second windows still fall to 119.8 FPS during the stress replay.
Build SHA-256:
`f4800caaa98d593b8b62bc657c95081463a8a35d2a7fd1a139b3dc712bf19b04`.
Evidence: `logs/native-prompt-span-*` and ignored
`native-prompt-span-comparison.json`, with source trace hashes.

The separate 120-second `prompt-profile` run also passes all checks. It
contains 19,970 samples (15,748 on CPU). `NtQueryVirtualMemory` leaves fall
from 4,706 to 4,055; the text-wrap query chain falls from 1,881 to 1,093
samples. Total reported main-thread CPU time is 94.3 versus 92.8 seconds,
so this does not establish a reduction in overall CPU time. The narrower
chain reduction supports the redundant-query diagnosis, while whole-run
variation and remaining render/update costs still require investigation.

### Authored camera-track sampling

The original switch table maps camera mode 27 to `0xCD691`. Its float track
clock advances before the whole-tick gate at `0xCD70F`, so native fractional
frames previously left the pose unchanged. The branch also selects its second
keyframe using the integer delta loaded at `0xCD851`. Removing only the gate
still samples the same keyframe twice on zero-whole-tick intervals.

The native policy evaluates the track on positive live game time and supplies
a one-keyframe span only when that span would otherwise be zero. Existing
fractional blend weights then evaluate the authored track at the current time.
The divisor at `0xCD88E` also uses the selected integer span, rather than
assuming it equals the float elapsed delta. A 2.5-tick interval with a two-frame
span exposed a separate error after the gate/span repair; a 1.8-tick interval
with a two-frame span covers the opposite mismatch. Multi-tick spans are
retained, as are track-end, loop, external-clock and
large-discontinuity cut behavior. This uses the game's existing keyframe
evaluation, not synthesized intermediate presentation frames.

`native_camera_track.c` executes the original complete mode branch, with its
three exits redirected to a terminal RET in the in-memory fixture only.
Synthetic linear keyframe decoding, matrix/sector/projection services and
player input are stubbed. The enclosing camera routine and real asset decoder
are not covered by that fixture. The original policy fails the fractional
pose comparison; a gate-only change also fails it. The complete change passes
60/120/144, quarter speed, pause, externally synchronized clocks, three-tick
and fractional catch-up, loop/completion, discontinuity cuts, stack and
integer-delta checks. Native-disabled division and neighboring sites stay exact.

The frontend audit also traces `sub_002DA0E0` to its raw fractional delta at
`0x4E7948`, its rotating-layer helper `sub_002DAB30`, and sheet/texture-index
helper `sub_002DAF90`. No timing rewrite is needed for these paths. Their
source tests use original code with matrix/menu services stubbed; broader
world texture/UV/water motion, rendered filtering and replacement-texture
coverage remain pending.

The combined timing/camera/frontend/auto-look/float/vehicle suite passes
**55 tests**, and Windows Release plus its built-in checks pass.
Final executable SHA-256:
`ea81dc2b9e148d2e71063e2e7ea64a2221cb61b8abed5a95825f4844298097dc`.
Logs: `native-camera-track-regressions.log`,
`native-camera-track-acceptance-build.log`, and `native-menu-motion-tests.log`.

One intermediate build's debug-settings test encountered system error 5 while
saving its disposable settings and blocked behind nested error dialogs. Its
timer repeatedly queued Save, and an attempted IDOK dismissal did not match
the observed dialog's OK control ID (IDCANCEL). `tests/pc_debug.c` now posts
Save once and dismisses save-error dialogs with WM_CLOSE. A forced read-only
save verifies bounded dismissal and unchanged settings; the final build passes
that case and ordinary Save/Cancel. The original unexpected access-denied
event's underlying filesystem cause is unconfirmed. No production persistence
behavior or personal files were changed, and interrupted runs are not counted
as passing checks.

The final 240-second unskipped native run passes scene, opening-firearm,
guest-only-presentation and no-failed-present checks. It observes 45 mode-27
samples for track 2485, from 157.625 to 179.656 seconds. The track clock
advances at 0.999546 seconds per wall second; the original-60 reference's 45
samples measure 1.000023. First firearm is observed at 182.156 versus 183.656
seconds. Subtracting the median estimated track start gives 24.830 versus
25.115 seconds to that observation. Half-second sampling and loading offsets
prevent exact event or pose-parity claims.

The reference executable is the intermediate
`0797de6d828bd395acbb5c13036d8e4deb59cbbd1ca3a9e2f6c9d865b2a0c167`
build, before the final native-only divisor correction; the final native
executable is the `ea81dc2...` build above. The original-path division remains
exact in the source test. Both runs use runner SHA-256
`16065fa19dff50563a80b802d7b6f15789dbc032a3e0690131a1e4f9a68a35d0`.
No captures or profiler were enabled. The final native trace reaches its
20,000-row limit at 154.936 s, before the track begins, so there is no
cinematic/gameplay interval distribution from that run. Extend the bounded
trace window before using another long-intro run for pacing acceptance.

Local source hashes and comparison calculations are in the ignored
`private/test-saves/high-refresh/native-camera-track-comparison.json`.
Run labels are `60-natural-intro-handoff-none-camera-track` and
`native-144-natural-intro-handoff-none-camera-track-final`. These half-second
observations establish exercised track progression and completion, not
per-frame visual smoothness, full cinematics or full-game acceptance.

## Explicit Vietnam network reference (2026-10-21)

A corrected original-60 pair and a native-144 host/native-120 client pair now
both select Vietnam, Deathmatch and Bot Set None explicitly through the menus.
Each reaches scene 48/network state 15 with two human slots and zero bots.
Reviewed captures show first-person gameplay, not team selection. After separate
two-second forward holds, the remote actor reaches the same settled position:

| Configuration | Moving player | Displacement | Settled endpoint error |
| --- | --- | ---: | ---: |
| Original 60 / original 60 | Host | 15.88007 | 0.0 |
| Original 60 / original 60 | Client | 15.76641 | 0.0 |
| Native 144 / native 120 | Host | 15.91010 | 0.0 |
| Native 144 / native 120 | Client | 15.98143 | 0.0 |

Distances are in game world units. Spawn locations/headings and terrain differ;
this comparison establishes bidirectional movement delivery after stopping,
not precise speed parity, continuous remote motion, packet latency or frame
pacing. Both pairs use minimized 1280x720 renderer capture sessions on one
Windows PC, with the unchanged `dbb84e3` executable and SHA-256 recorded below.
All four processes exit normally, preserving signed profiles and the fixture;
their disposable partition images are recycled. No production game changes or
rebuild were needed. Actual Mac and cross-machine LAN/WAN remain unverified.

The original pair uses operator-steered waits between explicit setup choices;
the native pair replays the corrected `tests/fixtures/lan-movement-commands.json`
automatically. The fixture now includes the mode/map/no-bot selections, replacing
the rejected default-lobby route. It is a bounded input recipe, not a guarantee
that a session passes. `tools/network_movement_stats.py HOST_DIR CLIENT_DIR`
requires an active matching scene, finite actor coordinates, at least 0.01 units
of actual displacement and at most 0.01 units of settled endpoint difference.
The comparison explicitly rejects equal stationary endpoints. Its five tests
include frozen peers, stale coordinates, wrong scenes and non-finite data; it
is included in the shared runtime CI workflow. The previously rejected original
run fails this checker, while both corrected pairs and the earlier mixed-rate
Subway movement pass. No automated check here certifies in-motion smoothness.

Evidence directories are `private/test-saves/lan-explicit-60-host`,
`lan-explicit-60-client`, `lan-explicit-native-host`, and
`lan-explicit-native-client`. Endpoint summaries are retained in
`private/pipeline/lan-explicit-60-summary.json` and
`lan-explicit-native-summary.json`. All four use runner SHA-256
`1b412b3f165b10bf294230d2dc31340d78062842bfe0f5f3b347f6b82b120cee`.
The network-summary and CI-scope checks have 70 passes in 0.16 s
(`logs/network-movement-stats-tests.log`). The final selection including the
Mac backlog check has 71 passes in 0.17 s (`logs/network-movement-stats-final.log`);
Mac runtime acceptance is separate.

## Mixed-rate loopback movement (2026-10-21)

A bounded two-process Windows x64/D3D11 System Link session uses the unchanged
`dbb84e3` game executable, SHA-256
`b9f21b2537d132f8d62c908e0c2f5d438f37f40caaf49d5477e99808fa0a28b1`.
The host targets native 144 and the client native 120. Both use isolated Test
Lab saves/settings, loopback addresses 127.0.0.2/3, minimized 1280x720 windows,
rotating renderer captures and a 300-second bound. They join the same Subway
Thief match (scene 3, network state 15), with two human players and no selected
bots. Captures verify the ready lobby, gameplay, remote player model and menu.
These conditions do not measure sustained FPS, scanout or real-network latency.

Read-only actor observations establish bidirectional settled-position delivery:

| Input | Local displacement | Local/remote endpoint difference |
| --- | ---: | ---: |
| Host holds forward for 2 s | 11.02293 units | 0.0 units |
| Client holds forward for 2 s | 15.69957 units | 0.0 units |

The two players start at different positions/headings and encounter different
geometry; displacement values are not a speed-parity comparison. The endpoint
check uses actor slot 0 for the host and slot 1 for the client. Other allocated
actor records are not evidence of active players or bots. The generic `player`
snapshot uses a current-view pointer that can point at different players during
network processing; it must not be treated as a stable local-player identity.

The client then moves backward while the host's Options menu is open. Its
position [46.005505, -12, 6.334576] reaches the host's actor record while the
menu snapshot reports paused=1. Continue returns the host to gameplay, and
both instances remain connected. This checks menu/transport coexistence, not
physical input latency or every intermediate remote pose. Host dual-pistol
ammunition decreases during firing, but no player damage occurs. The first
capture shows aim beside the target; the second has a wall in the sight line.
Damage/death/respawn/score replication is therefore not validated by this run.

Evidence: `private/test-saves/lan-native-host` and `lan-native-client`, with
summarized endpoints in `private/pipeline/lan-native-summary.json`. The exact
runner hash for this pair is
`7aed4f432dc99c9e410e2e957c5c6732140337adc0358c8f72415795a16827fc`.
Both processes exit normally; signed profiles and the canonical fixture remain
unchanged, and disposable partition images/frame dumps are cleaned up.

### Original-60 reference attempt rejected

A subsequent original-60 pair discovers, joins and starts a session, but the
default setup selects Team Deathmatch / Chinese (scene 25). Its reviewed
`match-start.png` shows the team-selection screen. Both players remain at
exactly their starting positions during the movement commands. Equal remote
coordinates cannot establish movement replication when neither player moves;
this run is rejected as a behavioral reference. It is not a high-refresh defect
reproduction. Explicit mode/map selection and team-selection completion are
required for the next original-60 comparison.

The rejected route is retained in each run directory's `commands.jsonl`.
The tracked replay has since been corrected as recorded above. Raw observations/captures are under
`private/test-saves/lan-original-host` and `lan-original-client`; the local
summary is explicitly marked `movement_validated=false`. These processes also
exit normally with unchanged profiles/fixture. Their runner hash is
`1b412b3f165b10bf294230d2dc31340d78062842bfe0f5f3b347f6b82b120cee`.

`tests/stability_transitions_game.py --lan-role host|client` now verifies the
existing multiplayer build/content, configures only the loopback peer, and adds
network state/role/actor observations to the existing bounded isolated runner.
Each instance needs a distinct run name; only one pair may use these addresses
at a time. This adds test tooling, not production transport behavior. The corrected original-60 settled-movement reference is recorded above.
Intermediate remote smoothness, cross-machine LAN/WAN, bot lifecycle,
mixed-rate match completion and native Mac remain open.

## Native clock and network movement cadence (2026-10-21)

`tests/native_network_cadence.c` composes the production
`pc_native_timing_guest.inc` clock with `pc_network_guest.inc` movement and
full-state refresh adapters. It reuses the existing clock fixture's fake host
timers, with an observer that advances a modeled 60 Hz ISR when QPC is read.
The model runs during timer waits too, so a native target change cannot leave
its ISR at the time before the wait. The initial fixture omitted this and its
prescribed timestamps ran ahead/behind the pacing deadline; this was a test
model correction, not a reproduced game defect.

Six two-second cases cover native 60/120/144 with ordinary counters and with
both raw ISR and match counters crossing UINT32_MAX. Each produces 120
movement-send gate openings and 40 full-state refresh requests for each of
four local-player records. Duplicate pump calls do not add sends. The clock
fields and callback stack/return values remain intact. Four successive
one-second phases at 144/120/144/60 retain 240 match ticks and gate openings,
and 80 refresh requests per local record.

A one-second paused phase still opens 60 transport movement gates on the raw
ISR, while the unchanged match clock generates no full-state refresh requests.
Resume adds 60 match ticks and 20 refreshes. A simulated 100 ms host stall
produces one send opportunity and one refresh per record, with the existing
three-tick gameplay catch-up cap; repeated calls do not create a packet burst.
Two disposable mutations replace the gate's ISR or refresh age's match clock
with rendered-frame counts. Both fail the cadence assertions, demonstrating
that the fixture rejects refresh-rate-dependent packet frequency.

The production game and transport code are unchanged. Packet encoding and
socket delivery are spies; this does not prove delivery latency, remote-player
prediction, bot pose continuity, mixed-rate peer synchronization or actual LAN/
Online behavior. Native 60 here uses the native clock, not the original timing
routine. The existing adapter checks separately preserve the original guest
fallback, offline behavior and bot callbacks. The fixture exercises the Windows
adapter through portable C on either compiler; it does not add a Mac transport.
It is included in `test_native_timing.py`, already selected by the shared
Windows/macOS runtime workflow. Actual Mac execution remains pending.

Focused clock/cadence checks, including the two rejected mutations, pass in
`logs/native-network-cadence-green.log`. The timing, network dispatch/results and backlog selection has 64 passes
in 66.49 s (`logs/native-network-cadence-regressions.log`). No game rebuild
is needed for these test/documentation changes.

## Offline network callback overhead

The previous CPU profile found repeated environment lookups in
`pc_network_enabled`. Bot animation, bounds, activity, damage and frame hooks
queried it before checking guest eligibility, including ordinary offline
actors. `pc_network_bots_guest.inc` and `pc_network_bots_lobby.inc` now reject
ineligible states/actors first. Eligible callbacks still query live settings;
the transport and ready-file polling implementation are unchanged.

The guest-dispatch fixture counts network queries across offline animation
and combat callbacks while checking original-call count, return value, stack
and pose argument preservation. It also switches the enabled result between
eligible actor calls to check that it remains live. Existing host/guest bot
behavior, movement cadence, original result-builder and native fallback tests
pass. The compiler fixture now uses the common MSVC discovery helper rather
than silently skipping when `cl` is absent from the shell PATH. These tests
are registered in the Windows/macOS runtime workflow; actual Mac execution
and network transport remain pending. The combined network, CI-scope and
backlog selection has 71 passes in `logs/native-network-guards-tests.log`.
The complete Windows build and built-in checks pass in
`logs/native-network-guards-build.log`.

Two isolated 120-second native-144 runs use the same runner
`16065fa19dff50563a80b802d7b6f15789dbc032a3e0690131a1e4f9a68a35d0`,
with walking, firing, turning, pause/resume and handoff sampling. Neither
uses captures, profiler, resource sampling or PresentMon. Both pass all
reported behavioral checks. Unpaused Story intervals from 85 seconds:

| Measure | Before | After |
| --- | ---: | ---: |
| Observed duration | 34.3333 s | 34.4999 s |
| CPU updates | 4,740 | 4,837 |
| Mean updates/s | 138.059 | 140.203 |
| p50 interval | 7.0444 ms | 7.0153 ms |
| p95 interval | 9.0831 ms | 8.4934 ms |
| p99 interval | 10.1881 ms | 9.3970 ms |
| Maximum interval | 13.3604 ms | 10.9974 ms |
| Game seconds / wall second | 1.0 | 1.0 |

The baseline executable is
`2389e61f7d36d789984ff3575ed26040953118c1a258ac12ae361b96e8ef0b46`
(overlay checkpoint `a83cf05`); the revised executable is
`a920df05eb37f18c2247ebb233a074d98cb591f8672eb7104961573705f612c7`.
Run labels are `native-144-handoff-none-network-guards-before` and
`native-144-handoff-none-network-guards-after`. Trace hashes, source hashes
and validation results are retained in
`private/test-saves/high-refresh/native-network-guards-comparison.json`.
This single comparison is encouraging but does not establish repeatability,
GPU/scanout timing, full-game coverage or sustained 144 FPS. The direct XNet
dispatch still checks host settings; no persistent environment cache was added.

## Entity velocity on fractional updates

US entity post-update `sub_000A7360` derives entity+0xF8/0xFC/0x100 velocity
from the position difference between +0x64 and +0xEC, divided by fractional
game delta. Its guard at 0xA75E7 instead checked the integer delta. On a live
interval without a whole tick, it wrote zero velocity and still updated the
previous position, losing that interval's motion from the derived velocity.
`pc_native_global_load` now supplies a live-game-time boolean at this site
only. The original path, other loads, integer timers and division formula
are unchanged; a paused interval does not divide by zero.

`tests/native_entity_velocity.c` executes the complete original routine
(0xA7360–0xA7666) and its linked-entity dispatcher (0xA7870–0xA79A2), translated
from the hash-verified US XBE. Two entities receive prescribed positions;
collision, sector and unrelated services are mocked and unused indirect
callbacks fail the test. This is a velocity-derivation test, not a physics
integration or collision-response test. It checks each update at 60/120/144,
quarter speed, pause (including a changed position with a carried integer
tick), stationary objects, three-tick and fractional catch-up, register/stack
balance and preservation of integer/float delta fields. It also covers the
original behavior and neighboring/unrelated loads.

Before the guard change, the test fails with native=1, whole=0, delta=0.25:
velocity is zero instead of 0.125 (`logs/native-entity-velocity-red.log`).
With the fix, the timing/camera/auto-look/float/vehicle/frontend/entity suite
has 56 passes (`logs/native-entity-velocity-regressions.log`). The final focused
test also checks finite results and passes. The full Windows Release build,
patch validation and built-in checks pass in
`logs/native-entity-velocity-build.log`.

Same-build 120-second original-60 and native-144 runs use walking, turning,
firing, handoff and pause/resume replay. Both pass their reported behavioral
checks and end with pistol ammunition 16 loaded / 80 reserve. The runner now
samples entity previous-position and velocity alongside animation; these are
asynchronous half-second reads, not atomic per-update observations.
Six samples in the steady forward window 86.5–89.5 seconds have median body
speeds 6.25886 / 6.25914 units per second. Position-derived forward speeds are
6.26610 / 6.21808, about 0.77% apart. This narrow observation does not establish
full movement/physics parity.

The native CPU update trace after 85 seconds reports 75.247 updates/s,
p50/p95/p99 intervals 9.9746/27.0758/31.4509 ms and maximum 84.0939 ms.
Game and raw time advance at 0.998992 seconds per wall second; long intervals
still encounter the original bounded catch-up policy. **Do not compare this
as a clean performance regression against the prior 140.203 run.**
Another game process (`sp26-cod`, PID 34128) started at 10:53:37 local time,
before both tests. A follow-up measurement found 42.875 CPU seconds over
5.837 wall seconds and 99.76% GPU 3D-engine utilization. That measurement is
after the tests, not a synchronized GPU trace; the overlap makes performance
acceptance inconclusive. The process was left unchanged. A clean pacing
retest remains required.

Both runs use executable SHA-256
`076eec0128aa55f977ee5475701797b0342c7a5cab3936e3921efb0bf7b03b34`
and runner
`65b94927ec6bed6e603ec219d2274e3ce3dd2a280ef023550196b20ac0486e2a`.
Labels are `60-handoff-none-entity-velocity` and
`native-144-handoff-none-entity-velocity`. No captures, profiler, resource
sampler or PresentMon were enabled. Validation, source/trace hashes and the
contention note are in
`private/test-saves/high-refresh/native-entity-velocity-comparison.json`.
Mac execution, collisions, other physics/weapon paths and full-game acceptance
remain unverified.

Further audit candidates remain unchanged: 0xA84F0 contains a whole-tick
physics loop whose per-step integration must be understood before conversion;
weapon routines have whole-tick gates at 0x1B6FF0, 0x1BB5FD and 0x1BD044.
The first also advances a float duration at 0x1B7194. These observations are
an audit queue, not proven fixes or permission to replace every integer gate.

## Weapon sound events on fractional updates

The original US helper `sub_001B6FF0` returns before selecting or emitting a
sound when its integer game delta is zero, even if fractional game time is
advancing. The native-only numeric guard now tests live fractional time at
0x1B6FF0. The return register is overwritten before subsequent use. Paused
updates still return; other reads and the original timing path retain their
behavior. The repeat timer at weapon+0xFC already integrates fractional
seconds, and its original reset-to-zero threshold behavior is preserved.

`tests/native_weapon_sound.c` executes the hash-verified original helper,
both sound parameter builders (0x21780 and 0x21FB0), and their initializer
(0x45390). Constants come from the same verified XBE. The audio backend and
RNG are mocked; unexpected indirect weapon callbacks fail the test. Checks
cover fractional one-shot dispatch at 60/120/144, one backend event per
accepted call, positional coordinates and nonpositional flags, sound ID and
handle, original one-tick cadence, repeat elapsed time at normal/quarter
speed, pause including a carried integer tick, catch-up and missing sounds.

The original guard fails `fractional one-shot dispatch` in
`logs/native-weapon-sound-red.log`. With the fix, 57 timing/camera/auto-look/
float/vehicle checks pass in `logs/native-weapon-sound-regressions.log`.
The 17 audio-adapter checks pass in `logs/native-weapon-sound-audio.log`;
they exercise deterministic fake-device stream/buffer behavior, not audible
output. Full patch validation, Windows x64 Release and its built-in checks
pass in `logs/native-weapon-sound-build.log`. The compiler is MSVC
14.44.35207 with Windows SDK 10.0.26100.0 and SDL 3.4.16; upstream remains
pinned to d90c92988a9f99d0580d2ca3f7efd6ec3a7b7845.

The isolated 120-second native-144 run
`native-144-handoff-none-weapon-sound` passes every reported check: Story,
native hook, guest presents with interpolation disabled, no present failures
or crash, movement, changing/advancing hand animation, firearm equipped,
ammunition consumed and pause/resume. Its exit code 1 is the runner's expected
bounded termination (`completed`, `terminated_by_runner` and `passed` are
all true). No capture, profiler, resource sampler or PresentMon was enabled.
The other game remained active; no FPS improvement or clean performance
comparison is claimed. Evidence lives in `private/test-saves/high-refresh`
and `logs/native-weapon-sound-144.log`.

This build uses base source ce77aa6 plus the weapon-sound change; executable
SHA-256 is `0f4bc5c9b26fce5b262c18ffa53a4602d3c89fdf111fa504c56a7d7f7d6ebe3a`.
Runner SHA-256 is
`65b94927ec6bed6e603ec219d2274e3ce3dd2a280ef023550196b20ac0486e2a`.
The same executable was subsequently opened for the user's manual test;
startup confirms the 144 native-update target and 3440x1440 at 144 Hz.
That startup is not evidence of sustained 144 FPS.

The helper consumes RNG before checking its repeat threshold. More live
invocations can therefore consume more random values; cross-rate RNG sequence
and audible parity are unverified. All three direct caller families require
separate cadence coverage. `sub_001BB5C0` has a whole-tick gate at 0x1BB5FD,
and `sub_00244F30` has one at 0x244FCD before duration accumulation at
0x245094 and projectile work. `sub_001C0200` is also called by several
weapon states; its caller's gates at 0x1C38CC/0x1C397F protect firing work.
Do not open these gates indiscriminately: ammunition, projectile count,
damage and random consumption need equal-duration comparisons first.
The discrete cooldown at 0x1C3243 subtracts carried whole ticks and must not
be replaced with a boolean. Complete weapon mechanics, other levels/modes,
multiplayer and actual Mac execution remain pending.

### Deferred weapon cooldown preservation

The original branch 0x1C320C–0x1C3273 subtracts the carried whole-game-tick
count from state+0x110, then invokes 0x1C0110 once when the counter expires
and callback metadata is present. `tests/native_weapon_cooldown.c` executes
these original bytes, replacing only the instruction at the final join with
RET and mocking the completion callback. The fixture supplies the enclosing
frame/register context; it does not run the whole player weapon update.

The original and native policies both pass at 60/120/144 for delays of
1/7/30/60 ticks, normal and quarter speed, and pause between updates. Each
counter expires at the expected accumulated game tick and dispatches one
completion, with no repeats afterward. Three-tick catch-up subtracts three,
while a live zero-whole-tick update leaves this discrete counter unchanged.
Alternate mode selection, primary fallback, missing metadata and absent
weapons are covered. No runtime change is needed at this site. This is a
check of a deferred action counter, not a complete firing/reload timing claim.
The focused sound/cooldown pair passes in `logs/native-weapon-branches.log`.
The final expanded timing/camera/auto-look/float/vehicle suite passes all 58
checks in `logs/native-weapon-final-regressions.log` (42.49 seconds). The 66
CI-scope/backlog contracts pass in `logs/native-weapon-sound-contracts.log`.
The cooldown addition changes tests only, so the already-built executable
and its runtime/build validation remain applicable.
Source and smoke-evidence hashes are retained in
`private/test-saves/high-refresh/native-weapon-sound-evidence.json`.

## Generic firing mixed-clock regression

This section records the pre-fix failure. The following
[event-tick scope](#generic-firing-event-tick-scope) addresses the reproduced
case; broader weapon acceptance remains open.

Audit checkpoint: d6e8fe4 plus the new test; Windows x64/MSVC 14.44.35207.
No production code changes or game rebuild are part of this audit. The
existing user session was left running.

`tests/native_weapon_fire.c` translates the complete original routines
0x244F30–0x24598E (generic firing), 0x1BC890–0x1BC988 (integer weapon-counter
update) and 0x1B5660–0x1B567B (mode lookup) from the hash-verified US XBE.
It invokes counter update then firing on every synthetic update, with the
native policy enabled at 120/144 and the original policy at 60. The fixture
uses the actor-flag 0x40 socket path, a single projectile per event and
controlled weapon definitions. Actor lookup/socket/direction, eligibility,
RNG, projectile construction, effects and sound dispatch are mocked. Unused
services and indirect callbacks assert. It verifies return-stack/register
preservation, paused updates and expected request counts. It does not run
whole actor/player callers or real collisions, damage, ammunition and audio.

Over one second, with the original gate intact:

| Configured interval | FPS | Projectile requests | Weapon+0x104 |
| --- | ---: | ---: | ---: |
| 0 s | 60 original | 60 | 0.999999702 |
| 0 s | 120 native | 60 | 0.499999851 |
| 0 s | 144 native | 60 | 0.416666836 |
| 0.1 s | 60 original | 10 | 0.166666672 |
| 0.1 s | 120 native | 10 | 0.083333336 |
| 0.1 s | 144 native | 10 | 0.069444448 |

The gate at 0x244FCD only accepts whole-tick frames. The accepted path adds
`game_float / 60` to weapon+0x104 at 0x245094; fractional time from skipped
frames never reaches that addition. The field also has consumers/reset/decay
paths in other weapon routines, so this fixture does not assign it a single
user-visible meaning such as firing cooldown or damage.

A test-only override that opens the gate on every live fractional update
raises zero-interval projectile requests to 120/144 and RNG requests to
360/432, versus 60 and 180 originally. With a 0.1-second interval, integer
cooldown thresholds still limit shots to 10, and the timer discrepancy
remains. The experiment is isolated inside the test numeric hook; **the
runtime gate is unchanged**. This demonstrates why merely replacing this
integer gate with a live-time boolean is insufficient.

The test is marked strict expected failure for `WeaponTimingMismatch` only.
That exception is raised after successful compilation/execution when the
unmodified native path differs from the original reference's timer or shot
count. Build failures, ABI assertions and other errors remain real failures;
an unexpected parity pass requires removing the marker. This known failure
is not included in passing timing counts. Measurements are retained in
`logs/native-weapon-fire-measurements.log`, with the focused pytest result in
`logs/native-weapon-fire-audit.log`. The combined timing/camera/auto-look/
float/vehicle/CI-scope/backlog run reports **124 passed, 1 expected failure**
in 46.54 seconds (`logs/native-weapon-fire-regressions.log`). No new runtime
build or game smoke was needed for this test/documentation-only checkpoint.

The next fix must separate discrete firing cadence from fractional timer
consumption, characterize the field's other writers/decay and preserve
nested sound/event timing. Acceptance must include zero and positive fire
intervals, variable/catch-up steps, slow motion, pause, the alternate mode,
multiple projectiles, actual caller states and representative weapons.
A complete damage/ammo comparison remains necessary before claiming weapon
mechanics parity. Actual Mac execution remains unverified under MAC-060.

## Generic firing event-tick scope

`pc_native_weapon_guest.h` wraps only the audited US generic firing routine
0x244F30. Native mode supplies its carried whole-game-tick span as the float
delta for this discrete transaction and restores both fields on return.
The original gate, cooldown counters, projectile loop and reset behavior
remain original code. A locally paused float interval suppresses the
transaction even if the caller retained a whole tick. The non-native path
calls the original body without changing either field.

This gives the routine's event-timer additions and its nested sound helper
the same elapsed tick span as the discrete firing gate. It does not run
additional firing calls or round every native update up to a tick. Continuous
animation/movement outside this routine retains fractional time. The additive
`native-weapon-event-scope.patch` supplies the manual wrapper; the common
patch installer orders it after native timing/input wrappers. Generation
retains `sub_00244F30_gen` reproducibly; generated C is not hand-edited.
The original clock 0x1538C0 supplies matching integer/float tick values in
its normal path, which is the reference for this discrete transaction.

The original-code fixture now executes the sound helper 0x1B6FF0, parameter
builders 0x21780/0x21FB0 and initializer 0x45390 in addition to firing, mode
lookup and counter update. Only the final audio backend is mocked. Nested
projectile/audio callbacks assert the scoped delta, and every call checks
that caller time fields are restored. Actor/socket services, projectile
construction, effects, eligibility and RNG remain mocked; this does not
prove damage, ammunition, audible output or complete caller behavior.

There are 96 configuration combinations: primary/secondary/fallback mode,
normal/repeating sound, one/three projectiles, the definition's 0x200 phase
gate off/on, zero/0.1-second interval, and normal/quarter-speed schedules.
Each compares 60/120/144, including pause between updates, integer cooldown
remainders, emitted sound count and repeat remainder, projectile/effect/RNG
counts and both event-timer fields. Quarter-speed scheduling supplies 15
whole game ticks per simulated second, comparing against original calls
with matching integer/float deltas; it is not a playtest of every in-game
slow-motion mode. Separate repeated variable-step sequences include zero,
fractional and three-tick catch-up intervals. A retained whole tick with a
zero float delta produces no firing/audio events. Actual actor callers and
arbitrary-rate physics remain separate acceptance work.

The focused test passes in `logs/native-weapon-fire-scope-focused.log`.
The original timer/shot mismatch now passes without an expected-failure
marker. The intentionally unsafe gate-opening alternative remains confined
to the test to demonstrate overfire. Full stack application passes in
`logs/native-weapon-fire-scope-patches.log`; source generation passes in
`logs/native-weapon-fire-scope-generate.log`. Windows Release and its built-in
checks pass (`logs/native-weapon-fire-scope-build.log`). The initial broader
suite passed 173 checks and exposed four patch-transition failures. The
overlay link patch unnecessarily included optional texture-source context;
its hunk now depends only on the common input target. Historical fixtures now
exclude the new firing wrapper when they exclude its prerequisite native
wrappers. Reapplying the complete corrected stack updates zero runtime files
(`logs/native-weapon-fire-scope-patches-recheck.log`), so these compatibility
repairs do not alter the tested executable. All 35 patch-transition/tracking
rechecks pass in 395.08 seconds
(`logs/native-weapon-fire-scope-patch-recheck.log`), including all four
previous failures. The other 142 checks passed in the original broader run.

The Windows x64 Release executable SHA-256 is
`1edf3c62f7f648448e8d8a3fc8050d53f00cfe2aff509bd694d349653ad55ded`.
The isolated 120-second native-144 run
`native-144-handoff-none-weapon-event-scope` passes all reported checks:
Story gameplay, fresh guest presentation with interpolation disabled, movement,
hand-animation progression, opening weapon handoff and ammunition consumption,
pause/resume, and no reported crash or presentation failure. The runner ends
the process deliberately (exit 1, `terminated_by_runner: true`). It uses the
verified US XBE and runner SHA-256
`65b94927ec6bed6e603ec219d2274e3ce3dd2a280ef023550196b20ac0486e2a`.
Evidence is retained under `private/test-saves/high-refresh/` and in
`logs/native-weapon-fire-scope-144.log`. This behavior check does not establish
sustained 144 FPS: the runtime reported a 69.28 FPS mean over its first 115
seconds, including startup. No GPU/display timing was collected.

The interactive game was reopened with `Play-Native.cmd 144`; its
`logs/play-20261020-115115.err` confirms the native 144 fresh-update target,
interpolation disabled and 3440x1440 fullscreen at 144 Hz. The earlier overlay
fix remains included: native target replaces the misleading cap label, and
offline sessions hide RTT. The shared firing wrapper has no host adapter;
actual Mac compilation/execution remains unverified under MAC-060.

## NPC firing event-tick scope

The complete original NPC firing routine 0x1BB5C0–0x1BC64F has a direct
whole-game-tick comparison at 0x1BB5FD and an event-timer addition using the
float delta at 0x1BBADE. It resets its integer weapon counters and invokes
projectile, ammunition and sound operations inside this gated transaction.
Opening its gate would change the number of transactions; leaving fractional
time in this gate loses elapsed event-timer and repeating-sound time.

The original-code fixture reproduced these one-second measurements before
the fix, with one projectile per invocation and a one-shot sound:

| Update rate | Projectiles | Sounds | RNG calls | Weapon event timer |
| --- | ---: | ---: | ---: | ---: |
| Original 60 | 60 | 60 | 240 | 0.999999702 |
| Native 120, unscoped | 60 | 60 | 240 | 0.499999851 |
| Native 144, unscoped | 60 | 60 | 240 | 0.416666836 |

Using the shared `pc_native_weapon_fire_guest` scope restores 0.999999702
at all three rates without changing these event counts. The additive
`native-npc-weapon-event-scope.patch` now supplies the production wrapper
for 0x1BB5C0, ordered after the generic firing wrapper. The scope restores
both game-time fields on return; continuous caller updates retain their
fractional time. Original timing calls the original body unchanged.

The routine already has a multiplayer bot-shot wrapper. The timing scope
is its inner callback, preserving network capture before firing and event
publication after the original return. The fresh-stack test now rejects
duplicate guest function definitions, which generation alone does not
reject. The network dispatch fixture checks the composed hooks with
simulated Windows transport and native fallback: a carried tick publishes
one shot, a fractional update without a whole tick and a paused interval
publish none, and caller time/register/stack state is restored.
All five network dispatch/results checks pass in 6.26 seconds
(`logs/native-npc-weapon-scope-network.log`). This is an ABI/event test,
not an actual multiplayer session.

`tests/native_npc_weapon_fire.c` executes the complete original routine,
mode lookup, sound selector and parameter builders, both ammunition helpers
(0x1B6790/0x1B66E0), sound-position helpers, actor flag helpers, effect-state
query and statistics eligibility/update routines from the verified US XBE.
Projectile construction, weapon-property services, effects and final audio
output remain mocked; the fixture uses a deterministic RNG. Unexpected
services/indirect callbacks fail. It checks guest ABI, stack/FPU balance,
finite projectile direction and restoration of both clock fields.

The 96 configurations cover left/right weapon slots, primary/secondary/
fallback definitions, one-shot/repeating sound, one/three projectiles,
normal/quarter-speed schedules and actor/weapon ammunition pools. Original
60 FPS and scoped native 120/144 agree on event count, ammo decrement,
projectile/audio/effect/RNG counts and event/sound timer remainders. Pauses
are interleaved; separate repeated variable-step schedules include zero,
fractional and three-tick catch-up intervals. Invalid target/body/weapon
exits remain inert. A retained whole tick with a zero float delta emits no
events. Quarter-speed schedules test 15 carried game ticks per second,
not the full in-game slow-motion state machine.

The unscoped regression reproducer remains in the fixture. Its measured
failure was recorded in `logs/native-npc-weapon-measurements.log` before
adding the production wrapper. The final scoped parity test passes without
an expected-failure marker (`logs/native-npc-weapon-scope-focused.log`).
The broader timing/audio/camera/animation/CI/backlog suite reports 143
passed in 54.62 seconds (`logs/native-npc-weapon-scope-regressions.log`).
Full stack application passes (`logs/native-npc-weapon-scope-patches.log`).
Generation completes in 117.2 seconds
(`logs/native-npc-weapon-scope-generate.log`), preserving
`sub_001BB5C0_gen` for the manual wrapper. Five targeted fresh/repeated-build,
historical controls/Mac-stack and texture/main branch-transition checks
pass in 139.17 seconds
(`logs/native-npc-weapon-scope-patch-transitions.log`).

The `cfaef73` checkpoint now passes the Windows x64 Release build and
built-in checks with MSVC 14.44.35207
(`logs/native-npc-weapon-scope-build.log`). Executable SHA-256:
`afb5ed6a71c05f3c8a3ddc1ede825e8f2f07f4a05d39dc3901de665a35840d26`.
The isolated 120-second `native-144-handoff-none-npc-weapon-event-scope`
run passes all reported checks: Story movement, hand animation, firearm
handoff and ammunition consumption, pause/resume, native fresh guest
presentation with interpolation disabled, and no reported crash or failed
presentation. The runner deliberately terminates the process at its limit
(exit 1, `terminated_by_runner: true`). Logs are in
`logs/native-npc-weapon-scope-144.log`; structured evidence is under
`private/test-saves/high-refresh/`. The original verified XBE and runner
hash `65b94927ec6bed6e603ec219d2274e3ce3dd2a280ef023550196b20ac0486e2a`
are unchanged. The game was reopened; `logs/play-20261020-121550.err`
confirms native 144 and fullscreen 3440x1440 at 144 Hz.

The runtime reported 62.76 FPS over its first 115 seconds, including startup.
The user reported that COD was open during this run. Treat it as contended
behavior validation, not a clean performance benchmark or proof of a
regression. No GPU completion or monitor scanout was measured.

A second isolated 120-second run, `native-144-handoff-none-npc-weapon-cod-closed`,
used the identical executable, XBE, runner and flags. COD was absent from
process checks at the start and during gameplay. All reported behavior
checks passed again. `tools/native_update_stats.py --from-seconds 85`
compared unpaused Story CPU update intervals:

| Run | Intervals | Window (s) | Mean updates/s | p50 (ms) | p95 (ms) | p99 (ms) | Worst (ms) | Game seconds / wall second |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| COD reported open | 2131 | 34.3704 | 62.001 | 14.1186 | 30.4936 | 33.3740 | 37.5897 | 1.0 |
| COD absent repeat | 4798 | 34.6179 | 138.599 | 7.0151 | 8.9571 | 9.8945 | 19.7135 | 1.0 |

The repeat's final five-second FPS samples reported 144.1 and 144.0.
This supports resource contention as the explanation for the earlier low
rate, without isolating every background variable or proving full-game
performance. These are CPU update intervals in the windowed test; they
do not measure GPU completion, monitor scanout or input latency.
Evidence is in `logs/native-npc-weapon-cod-closed-144.log` and the matching
`private/test-saves/high-refresh/` artifacts. No compiler or pytest work
ran during the measured window. The interactive game was reopened afterward;
`logs/play-20261020-122121.err` confirms native 144, interpolation disabled
and fullscreen 3440x1440 at 144 Hz.

Full actor caller cadence, damage/collision, audible output, other weapon
families, multiplayer and actual Mac compilation/execution remain open.

## Object rocking and bounce audit

This section preserves the baseline and earlier experiments. The current
source correction is described under [Native object rocking integration](#native-object-rocking-integration).

The callback at 0xA84F0–0xA8A97 updates an object's vertical rocking/bounce
offset and two tilt components, then builds its transform. Its initializer
0xA8AA0 selects a model matrix, applies an impulse and installs 0xA84F0 as
the entity+0x16C callback (assignment at 0xA8BC5). This is a specific object
motion path, not the general player or world collision integrator.

The callback executes one fixed step per carried whole game tick. Both
loop bounds read 0x4E7944 at 0xA8595/0xA8980; it never consumes the fractional
game delta. Each loop uses 1/60 in vertical integration and adds fixed
angular increments. Opening these bounds alone would accelerate the
simulation rather than making its current motion smooth.

`tests/native_object_rocking.c` executes the complete original callback,
Euler/translation matrix builders and matrix multiplication routines from
the verified US XBE. The fixture supplies an identity base transform per
update and checks the resulting vertical translation against the simulated
offset. Its free-flight start is offset 10, vertical velocity 5 and zero
tilt. Bounce/math and impact-audio services fail if reached; this deliberately
does not mock a collision outcome. This setup stays above the routine's
0.3 near-ground threshold. Stack/FPU balance, non-frame callee-saved
registers, unchanged clock fields and finite matrices are checked.
The translator publishes framed helper EBP as frame TLS; it is not treated
as a restored guest register by this fixture.

| Updates per second | Schedule | Changed transforms | Repeated transforms | Final offset | Final velocity |
| ---: | --- | ---: | ---: | ---: | ---: |
| 60 | Normal | 60 | 0 | 9.546200752 | -4.523219109 |
| 120 | Normal | 60 | 60 | 9.546200752 | -4.523219109 |
| 144 | Normal | 60 | 84 | 9.546200752 | -4.523219109 |
| 60 | Quarter-speed | 15 | 45 | 10.686166763 | 1.669251561 |
| 120 | Quarter-speed | 15 | 105 | 10.686166763 | 1.669251561 |
| 144 | Quarter-speed | 15 | 129 | 10.686166763 | 1.669251561 |

The endpoints match exactly because the same number of fixed steps run.
Ordinary pause is inert, and one three-tick catch-up call matches three
one-tick calls. The missing native progression is recorded as a strict
expected failure that accepts only `ObjectRockingQuantization`; compile,
ABI and other fixture errors remain failures. Measurements are in
`logs/native-object-rocking-measurements.log`. The broader timing, auto-look,
float/vehicle translation, audio, CI scope and Mac backlog suite reports
143 passed and one expected failure in 44.75 seconds, recorded in
`logs/native-object-rocking-regressions.log`.

The initial audit made no production change to this routine. A motion fix must preserve the
original step's damping/acceleration, characterize tilt clamping, impact
timing/audio, bounce restitution and settling, and produce a fresh transform
for each advancing native interval. Whole-game collision coverage, actual
scene review and Mac execution remain open. This audit does not change
the already rebuilt NPC-weapon executable.

The additional `native_object_bounce.c` fixture now follows the original
callback through near-ground tilt clamping, floor impacts and callback
retirement. It runs the original matrix routines, sound defaults builder
0x45390 and positional sound builder 0x21910. The audio-device boundary is
captured rather than played. The CRT helper 0x3C9DF8 is adapted with host
`asin`; its entry and x87 asin-kernel bytes are verified against the XBE.
This is not a test of the complete CRT exceptional-input machinery or
audible output.

Across 72 combinations of 60/120/144 updates, normal/quarter-speed time,
zero/nonzero tilt and six impact flags, the original code produces the
same impact tick sequence and byte-identical 48-byte sound parameters.
Starting at height 0.2 and velocity -3 produces three audible impact
requests for each enabled material, with sound IDs CDA/CDB/CDC and the
original flag priority. Position and gain are checked against the entity.
Six single steps and two three-tick catch-up calls also produce identical
physics and sound payloads. A paused moving object emits no sound and
retains its state.

Settling is checked before the whole-tick loop: the normal 60 FPS case
retires at game tick 63, while extra zero-tick callbacks retire the same
state at tick 62 at 120/144 FPS and under quarter-speed schedules. This
one-tick timestamp distinction is recorded, not hidden by a blanket
cross-rate equality assertion. The fixture preserves this original
behavior while documenting what a fractional replacement must address.
Measurements are in `logs/native-object-bounce-measurements.log`; the
focused checks are in `logs/native-object-bounce-audit.log`. No production
motion conversion is included in this audit.

### Native pause guard and rejected fractional prototype

Extending the pause check to an already-settled object reproduced a separate
failure: with height 0 and velocity 0.2, the original callback clears +0x16C,
clears the rocking flag and stamps +0x90 even when the live game delta is zero.
The existing moving-object pause check did not enter that pre-loop branch.
`logs/native-rocking-pause-before.log` records the failing entity-state check.

The shared policy now takes the bypass branch at 0xA8526 only during native
zero-time calls, then supplies zero to this callback's two whole-tick loop
bounds. It still executes the original matrix builders: returning at function
entry would omit the rocking transform when the caller supplies a fresh base
matrix. No guest field is temporarily changed. Advancing intervals and the
native-off path retain the original comparison and loop counts. The additive
`native-object-pause` patch exposes an optional float-branch policy in the
lifter; the generic default preserves the original condition, and operands
remain captured at the comparison instruction.

The original-code fixture checks byte-identical entity state and matrices
for a tilted moving object with zero live delta and carried counts 0 through
6. It separately checks settled-object pause, positive-fraction resumption,
native-off zero-tick compatibility, and the existing 72 advancing bounce/sound
configurations. The strict rocking-cadence expected failure remains open.
This pause correction does not make bounce motion advance at 144 Hz.

A separate, unshipped prototype tested a fractional extension of the original
free-flight affine update. For one original step, write `v' = r*v - q` and
`h' = h + a*v - b`, where `dt=1/60`, `r=1-damping*dt`, `q=gravity*dt`,
`a=0.8333333134651184*dt`, and `b=4.158332824707031*dt*dt`. For a fraction `d`,
`s=(1-r^d)/(1-r)`, `v(d)=r^d*v-q*s`, and
`h(d)=h+a*s*v-b*d-a*q/(1-r)*(d-s)`. Angular increments were scaled by `d`;
the original floor, tilt and sound logic ran after each fractional step.

This produced 144 changed free-flight transforms out of 144 updates, with
one-second endpoints within 0.00002 of the original in the measured normal
and quarter-speed cases. It did **not** preserve the sampled impact response:

| Case (height 0.2, velocity -3, impact flag 0x40) | Audible requests | Settling game tick |
| --- | ---: | ---: |
| Original 60, normal | 3 | 63 |
| Prototype 120, normal | 3 | 55 |
| Prototype 144, normal | 3 | 53 |
| Original 60, quarter speed | 3 | 62 |
| Prototype 120, quarter speed | 3 | 52 |
| Prototype 144, quarter speed | 3 | 52 |

At normal 144 the impact tick sequence changed from 5/29/43 to 4/28/41,
and the last sound gain changed from 0.359771818 to 0.313612521. The prototype
measured these differences by relaxing cross-rate assertions only in its
temporary in-memory fixture; the tracked acceptance assertions were not
weakened. All 72 prototype configurations completed, but completion is not
acceptance. See `logs/native-rocking-fractional-experiment.log`; the ignored
driver is `private/pipeline/rocking-fractional-experiment.py`. No prototype
integration, floor response or sound change is present in production source.
Preserving original impact/settling behavior while advancing fractional motion
remains the next design problem; free-flight endpoint agreement is insufficient.

A second ignored prototype splits the affine motion at original one-tick
boundaries and evaluates near-floor constraints only at those boundaries.
In the same fixture it retained impact ticks 5/29/43 and sound gains within
0.000001, while still producing 144/144 changed free-flight transforms.
However, it can carry negative height between boundary checks, and its
phase-gated retirement waits until tick 65 at 144 FPS rather than 62/63.
The nonnegative-height assertion was relaxed only in that temporary
experiment; this is another incomplete design, not an accepted fix.
`logs/native-rocking-contact-boundary-experiment.log` and the ignored
`private/pipeline/rocking-contact-boundary-experiment.py` preserve the result.
Object initialization/reuse, interleaved callbacks, floor presentation and
settling across partial steps must be resolved before integrating such a path.

For the pause correction, the shared timing, float/vehicle translation,
CI-scope and backlog suite passes **135 tests with the existing rocking
expected failure** in 64.37 seconds (`logs/native-rocking-pause-shared.log`).
All **21 patch-upgrade checks pass** in 418.91 seconds, including older
Windows/Mac stacks and repeated application (`logs/native-rocking-pause-upgrades.log`).
The supported Windows `tsfp.ps1 Generate` pipeline completes in 83.4 seconds,
and its generation fingerprint is current. A Release/x64 `ClCompile` pass
with MSBuild 17.14.40 and MSVC 14.44.35207 succeeds for the generated units,
manual code and display menu. It uses two compiler workers at below-normal
priority. Logs are `logs/native-rocking-pause-generate.log` and
`logs/native-rocking-pause-compile-only.log`. MSB8028 again reports shared
intermediate-project files; there are no compilation errors.
This was compile-only, with no relink or launch: the responsive interactive
PID 25304 still runs `fc3a7ce`, whose executable SHA-256 remains
`912f416b5f6eaeabf545c960c255491907988eae9f031ead4231b24832cbc333`.
The pause correction, NPC aim correction and kind-35 projectile motion
changes still need a complete supported build and targeted live checks.
Actual Mac compilation/runtime and full-game acceptance remain unverified.

## Native object rocking integration

The shared `pc_native_rocking_guest.h` implementation now advances the
0xA84F0 callback on fractional intervals. It evaluates the affine
gravity/damping flow at the current time, using the last original contact
state as its integration origin. It does not blend two rendered poses or
require a future frame. At an original one-tick boundary it uses the exact
original float operation order, then executes the original floor response,
tilt limits and sound builders. Keeping those contact intervals avoids the
earlier prototype's changed impacts and settling. The original matrix
builders run for every advancing native update and during pause.

| Native update rate | Fresh transforms, normal speed | Fresh transforms, quarter speed |
| ---: | ---: | ---: |
| 60 | 60/60 | 60/60 |
| 120 | 120/120 | 120/120 |
| 144 | 144/144 | 144/144 |

The one-second height/velocity endpoints match the original **exactly** in
all six native cases: 9.546200752 / -4.523219109 at normal speed and
10.686166763 / 1.669251561 at quarter speed. The native-off quarter-speed
reference still changes 15/60 transforms. The strict expected-failure mark
has been removed; repeated native transforms now fail the test normally.

The bounce fixture expands to 96 combinations: native-off 60 plus native
60/120/144, normal/quarter speed, two initial tilts and six material flags.
All preserve the original impact ticks and byte-identical sound payloads,
including gain, material priority and position. Native settling retains the
previously documented next-callback distinction (tick 62 versus original
normal-speed 60-FPS tick 63). Zero-time calls preserve state and matrices.
A pause immediately before the first floor-impact boundary retains latent
penetration without drawing below-floor height, emitting early audio, or
losing the eventual original impact velocity. The sound boundary is captured,
not played; the previously verified CRT asin service remains a host adapter.

Timing state is kept per slot of the original object pool, whose base and
capacity are 0x72F5E8/0x72F5E4 and whose stride is 0x2E8. The original pool
reset at 0xA5AF0 explicitly clears that array; its shared wrapper invalidates
the host state too. The 0xA8AA0 initializer either starts a fresh rocking
instance or adds an impulse to an existing instance, including recursive
child impulses. New instances reset their slot; live impulses rebase the
motion at the current phase without restarting the contact clock. A hidden
negative offset is restored only for physical calculations, while exposed
height remains nonnegative between contact boundaries. No guest padding is
used. Pool base/capacity changes clear the host state; native-off, invalid pool
membership, allocation failure, non-60 base rate and intervals outside the
audited 0–6-tick range retain the original updater.

The original-code lifecycle checks compare separate and interleaved runs,
nested update contexts, 60 single ticks versus uneven fractional/catch-up
steps, impulses applied partway through a tick, rejected impulses, recursive
initialization, address reuse, pool reset/base/capacity changes and unknown
entities. The original initializer's RNG call count is retained. These checks
exercise both the shared header and translated original routines, rather than
replacing the bounce response with a test approximation. The pool-reset
wrapper's original-call boundary is spied; the full level-reset chain is not
executed by this fixture. Whole-game callback/lifecycle coverage remains open.

The additive patch provides optional scalar add/subtract policy hooks and
wraps the updater, initializer and pool reset. Generic arithmetic is unchanged;
the title policy selects only the audited rocking instructions. Full generation
confirms the recursive initializer calls the wrapper, not its generated body.
Measurements and lifecycle results are in
`logs/native-rocking-integration-measurements.log` (two focused checks).
The broader timing, float/vehicle translation, CI-scope and backlog suite
passes **136 tests with no expected failures** in 67.23 seconds
(`logs/native-rocking-integration-shared.log`). Supported Windows generation
passes in 91.0 seconds (`logs/native-rocking-integration-generate.log`).
All **21 patch-upgrade checks pass** in 424.54 seconds
(`logs/native-rocking-integration-upgrades.log`). Release/x64 `ClCompile`
succeeds with MSBuild 17.14.40 / MSVC 14.44.35207, using two below-normal
priority compiler workers (`logs/native-rocking-integration-compile-only.log`).
The existing MSB8028 intermediate-directory warning remains; no compile errors
occurred. This check compiles the generated/manual code and display menu but
does not relink or replace the running executable.

A ten-second read-only observation of the unchanged interactive `fc3a7ce`
build found state 101, a pool capacity of 20 and no listed entities in all six
samples (`private/pipeline/rocking-pool-observation.json`). Those non-atomic
samples establish no live rocking behavior or scene acceptance.

The subsequent full supported Windows PowerShell 5 `tsfp.ps1 Build` succeeds
on `dee6170`, including its built-in checks, network identity seal and matching
crash symbols (`logs/native-rocking-full-build.log`). The loaded `fc3a7ce`
executable was renamed in place to
`build/windows/Release/futureperfect_recomp.running-fc3a7ce.exe`, preserving
the responsive interactive PID 25304. Its SHA-256 remains
`912f416b5f6eaeabf545c960c255491907988eae9f031ead4231b24832cbc333`.
The new normal Release executable is `dee6170`, SHA-256
`3e5932fb336506015b35e83312a8a3c9c8cd5eee2a855843e916462e0bb454e6`.
The retained loaded image is needed until that interactive process exits;
its original reported process path does not establish that it runs the new
file. Ignored `private/pipeline/live-build-handoff.json` records the handoff.
No second game-data copy or build tree was created. Representative live scenes
and actual Mac compilation/runtime remain pending.

The first effects-enabled run exposed an observer startup race: it read the
scene global before the guest address space was mapped. The runner now probes
that global before any guest observation, tolerates startup unavailability for
at most five seconds, and still fails on missing/partial memory at the deadline.
The game-exit check remains active throughout. A focused regression exercises
unmapped, partial, mapped menu-state and later unreadable memory. Five observer/
smoke-check tests and five effects/entity tracing tests pass; 66 CI-scope/backlog
checks also pass. The runner change does not alter game code or require relinking.

The repeated 120-second native-144 run on the new executable passes all checks:
Story, firearm handoff, advancing/changing hand animations, movement, ammunition
consumption, pause/resume, fresh guest-only successful presents and no logged
crash. It uses isolated Test Lab saves/configuration and internal replay input,
with its window created behind the user's existing window without activation.
The owned child alone is terminated at the deadline; the resulting child exit
code 1 is intentional, recorded separately from the successful runner result.
Logs are `logs/native-rocking-live-144.log` and
`private/test-saves/high-refresh/native-144-rocking-build-*`.

The unpaused Story window after trace second 60 contains 7,811 CPU update
intervals over 56.4306 seconds: mean **138.418 updates/s**, p50 **7.0673 ms**,
p95 **8.8808 ms**, p99 **10.1254 ms**, worst **32.1661 ms**, and game/raw time
ratios both **1.000000**. The old interactive renderer remains running, so this
is a shared-load measurement, not an isolated performance benchmark or proof
of steady 144 Hz, GPU completion, monitor scanout or input latency.

The effects observer retains 147 snapshots, 146 complete; one detects changing
list heads. Observed effect kinds are 2/5/9/11/12/29/39, with **no type-32 beam**.
All 74 entity traversals complete, but none observes callback 0xA84F0. These
half-second/one-second non-atomic samples can miss short-lived effects and
cannot verify live rocking or identify the reported delayed trail. A reviewed
late capture shows the starting canyon, weapon 14, crosshair, ammunition HUD
and surrounding geometry. The replay does not reach the pre-bridge encounter,
plasma-sniper hit scenario or post-bridge ship landing; those reports remain
open despite the passing smoke check and original-code fixtures.

The matching 120-second original-60 replay also passes all gameplay and pause
checks on the same executable (`logs/native-rocking-live-60.log`, evidence
prefix `60-rocking-reference`). All 146 effects and 73 entity traversals
complete; it observes the same effect-kind set, again no type-32 beams or
rocking callbacks. These are repeated samples, not equal-spawn-count or
per-frame motion comparisons. The reviewed final capture shows the same
starting canyon/weapon/HUD, with 16 rounds loaded and 80 reserve in both runs.
Each run retains three BMPs; all evidence for the pair totals about 25.1 MiB.
The disposable disk cleanup completes and the run lock is removed. Personal
controls/display SHA-256 values match their pre-build values, and the old
interactive PID 25304 remains responsive. The user subsequently deferred
the encounter-specific reports and asked to prioritize overall experience;
they remain recorded as unverified rather than resolved.

## Performance follow-up and rejected GPU-poll experiment

After the user deferred the encounter-specific reports, the `dee6170` build
passed a 120-second native-144 replay with 200 Hz main-thread profiling
(`native-144-experience-profile-*`). The whole-run sample includes startup,
menus and gameplay: 5,182 of 20,573 samples ended in `NtQueryVirtualMemory`
and 2,531 in `sub_003D6690`. Folded wrapper symbols can be misleading;
these counts neither isolate active gameplay cost nor establish GPU time.

Experimental commit `aef8d29` immediately acknowledged the audited PFB-flush
poll at 0x3D66D0. A 32-case original-submit fixture, 143 shared checks,
21 patch-upgrade checks and a full Windows build passed. The actual two-minute
replay **failed** to reach Story or perform movement/firing/pause checks.
Its profile instead accumulated 14,390 samples in `sub_003D65B0`; the last
buffered frame timestamp was 34.3128 seconds. Sampling does not identify the
precise stalled instruction or prove the dependency between the two waits.
This is evidence that the isolated fixture did not model full device progress,
not permission to bypass another driver wait.

The entire optimization was reverted in `eae18a3`, the complete patch stack
reapplied, generated source refreshed and the supported Windows Release build
rerun successfully. The rejected executable was removed by an exact hash
check. The restored executable SHA-256 is
`12863fe128dfde83e4a726c5feb924a0ba361e77ae8aae9dcf8caeeb69d4ada2`.
All 137 shared regressions pass. The 120-second restored native-144 replay
passes Story, movement, firearm handoff/animation, ammunition consumption,
pause/resume and fresh guest-only presents. A late capture shows the starting
canyon, weapon and HUD. This does not revisit the user's deferred encounter.
Evidence: `logs/native-gpu-flush-revert-build.log`,
`logs/native-gpu-flush-revert-shared.log`, `logs/native-gpu-flush-restored-live.log`,
`private/test-saves/high-refresh/native-144-gpu-flush-restored-*` and
`private/pipeline/gpu-flush-rejected.json`. The user's old PID 25304 exited
independently before the experimental test; no user session was terminated.

The subsequent, narrower prompt change keeps one fresh mapping snapshot per
wrapper invocation. It reuses the OS-reported bounds and write permission
for nearby text and scratch, without retaining them across guest execution.
On Windows a non-page-aligned stack and nearby rewritten text require one
VM query instead of two (or three for text below ESP). Page-aligned stacks,
other regions, protection boundaries and unwritable scratch still receive
their necessary queries. Windows real-VM tests exercise those cases, changed
permissions on successive calls, no-access and guard pages. The native Mac
self-test adds read-only scratch and page-boundary cases but is not run here.
The prompt change is committed locally as `6fc4d87`. Its full supported Windows
Release build and built-in tests pass, along with 66 CI-scope/backlog checks.
The controller-routing subtest explicitly skips when foreground activation is
refused; this is not physical-controller acceptance. The generated-code
fingerprint remains current and the pinned US XBE hash is unchanged. Executable
SHA-256: `1f59510ed7bc8ce088dd87238546bee89e65ac30abf46f8ce3f61d4700109b02`.

The 120-second native-144 replay with profiling and capture passes all Story,
movement, firearm handoff/animation, ammo, pause/resume and fresh-present checks.
The reviewed final capture shows the starting canyon, weapon and ammunition HUD
(16 loaded, 80 reserve). Its unpaused Story intervals after trace second 60
average **137.512 updates/s**, p50 **7.0515 ms**, p95 **9.1119 ms**, p99
**10.4290 ms**, worst **34.2793 ms**, with game/wall time ratio **1.000000**.
The restored build's earlier capture-only run averaged 138.285 updates/s;
profiling and background-load differences prevent a causal before/after
performance conclusion. No overall FPS gain or perceived-smoothness improvement
is claimed from the reduced query count. Stable 144 FPS and complete-game
coverage remain open. The matching 120-second original-60 replay also passes
all gameplay/pause checks on the same executable. Its reviewed final capture
shows the same canyon/weapon/HUD and 16 loaded, 80 reserve ammunition.
Evidence: `logs/native-prompt-region-live-60.log` and
`private/test-saves/high-refresh/60-prompt-region-reference-*`.

Evidence: `logs/native-prompt-region-full-build.log`,
`logs/native-prompt-region-test.log`, `logs/native-prompt-region-shared.log`,
`logs/native-prompt-region-live-144.log`,
`private/test-saves/high-refresh/native-144-prompt-region-*` and
`private/pipeline/prompt-region-validation.json`. Selected final captures are
retained; disposable test disks and run locks are cleaned, and no test child
remains. Redundant intermediate captures and the no-longer-loaded `fc3a7ce`
executable were removed after validation. Personal controls/display hashes
remain unchanged. Nothing has been pushed and actual Mac validation is pending.

## MapMaker random light preview

The visual timing audit identifies `sub_00116130` as a light-color preview
routine, not world texture scrolling. Its only direct call in the pinned US
disassembly is 0xA1E8B, gated by scene 100 (MapMaker) and entity type 2.
It selects constant, sinusoidal, square-wave or random light behavior from
the 40-byte parameter record, writes a packed color and a float RGB table,
then propagates that color through `sub_00115F50`.

The mode-3 branch tests whole game ticks at 0x11622E. A zero value jumps to
the color blend with XMM0 still zero, replacing the previous color with the
dark endpoint. The complete original routine reproduces white `ffffffff`
becoming black `ff000000` on a live 144-Hz fractional interval without any
RNG call or deadline change. Modes 0/1/2 retain their color for that same
clock sample. This identifies extra dark subframes, independently of any
authored random flicker; it is not a diagnosis of the deferred gun trails.

The shared wrapper in `pc_native_light_guest.h` leaves mode-3 color and its
already-propagated model state intact until the next whole-tick decision.
Native pause also retains it even when a stale positive whole count remains.
Other modes, positive-whole updates and the original-60 path call the original
routine unchanged. The caller does not consume a return value; the wrapper
preserves its cdecl return-slot ownership and callee-saved registers. There
is no host cache, so initialization/palette edits/slot reuse remain guest-owned;
a new or edited random light takes effect on the next original decision tick.

`tests/native_light_preview.c` executes the complete original routine and
switch table, with deterministic RNG and a spy for downstream model updates.
An eight-game-second reference supplies 480 decisions and 69 RNG calls.
Native 60/120/144 at normal/quarter speed matches every packed color, float RGB
table entry, deadline and cumulative RNG count. Native 144 holds 672 normal
or 4,128 quarter-speed intermediate frames without introducing dark subframes;
paused probes preserve the same state. The unpatched counterexample remains
part of the fixture. This proves the color decision/dispatch boundary, not
actual model upload, GPU output, complete MapMaker operation or Mac execution.
Runtime source is committed locally as `8893ba8`. The supported Windows Release
build and built-in checks pass; the physical-controller routing subtest skips
when Windows refuses foreground activation. All 138 shared checks and 21
patch-upgrade checks pass. An additional original-code check preserves the
complete parameter record, RGB table and RNG for one/two/three-tick catch-up.
The generated original/manual wrapper declarations and fingerprint are current.
Executable SHA-256:
`56c319e0f4cd6fba9d3e9a6cbea169f0927870222897f1c0a91e791808a18de7`.

The 120-second native-144 Story replay passes movement, firearm handoff/hand
animation, ammunition use, pause/resume and fresh guest-only presents. The
reviewed final capture shows the starting canyon, weapon and ammunition HUD.
Active CPU intervals average 136.724 updates/s, p95 9.2886 ms, p99 11.3105 ms,
worst 51.1706 ms; game/wall ratio is 0.999979 with the existing catch-up clamp.
This is not a performance comparison, steady-144 proof, or MapMaker acceptance.
Actual light/model rendering in MapMaker and actual Mac execution remain open.
Evidence: `logs/native-light-preview-{shared,upgrades,full-build,live}.log`,
`logs/native-light-preview-fixed.log` and
`private/test-saves/high-refresh/native-144-light-preview-*`.

The audit also records all nine direct writers of the absolute visual clock
at 0x7DE318 in `private/pipeline/visual-clock-writes.asm`: ordinary increments,
initialization, recorded playback and network synchronization. Smooth pulse
and screen-distortion consumers still sample whole absolute time. They need
fractional-phase support that respects those clock resets and overrides;
globally replacing integer absolute-time reads is not justified by this
evidence and is not part of this correction.

## Screen-distortion absolute-time audit

The full original US `sub_000F9700` (`0xF9700..0xF9B0B`) calculates a
screen-distortion grid from the whole absolute clock at `0x7DE318`. Its direct
caller is the per-player effects renderer `sub_000FED20` at `0xFEDA3`, after
processing five effect slots. This is a framebuffer distortion effect; it
does not establish the behavior of world texture scrolling or all animations.

`tests/native_screen_distortion.c`, registered in the existing shared native
timing suite, executes that entire routine, the original grid-size selector
`sub_000F8AA0`, and original sine-table initializer `sub_00153C20` plus its
FSIN helper `sub_00012010`. All code and constants are read from the pinned,
SHA-256-verified US executable. The fixture supplies a controlled 640x448
grid with amplitude 0.75, fixed edges, a viewport and active-player records.
Graphics services are spies: every update checks all 99 vertex positions,
all 80 draw packets of 21 words, the framebuffer-copy bounds, call counts,
stack/callee-saved registers, FPU depth and unchanged guest timing fields.
This tests guest geometry and dispatch, not GPU output or scene activation.

For eight game seconds, the current production numeric policy repeats the
whole original-60 mesh and draw packets on each intermediate native frame.
A **test-only** clock-read probe supplies the exact fractional absolute time
to the original calculation. It is not linked into the game and does not
implement clock accumulation, reset detection or network/replay integration.

| Target FPS | Speed | Updates | Current repeated outputs | Fractional probe repeated outputs |
|---|---|---:|---:|---:|
| 60 | Normal | 480 | 0 | 0 |
| 120 | Normal | 960 | 480 | 23 |
| 144 | Normal | 1,152 | 672 | 44 |
| 60 | Quarter | 1,920 | 1,440 | 279 |
| 120 | Quarter | 3,840 | 3,360 | 1,452 |
| 144 | Quarter | 4,608 | 4,128 | 2,059 |

Every probe sample at a whole-tick boundary matches the original mesh and
draw packets exactly. The remaining repeated outputs reflect the original
integer pixel conversion; fractional time alone cannot remove that geometry
quantization. The fixture also samples repeated paused times, forward/backward
rebases, zero-time resets and two player-view selections at the same time.
These verify consumer determinism, **not** execution of the actual pause,
reset, replay or network writer paths. Quarter speed is a controlled clock
input, not proof of the complete live slow-motion path.

The next production correction needs a fractional absolute clock tied to the
original clock writes. It must distinguish an advancing zero-whole-tick
interval from a branch that never updates the level clock, explicitly rebase
on initialization/demo/network writes (including same-value writes), and
allow several player views to sample one clock value without advancing it.
The six ordinary direct increment stores are `0x688BB`, `0x68F8E`,
`0x691F3`, `0x69239`, `0x69265` and `0x69325`; direct reset/override stores
are `0x670E8`, `0x43595` and `0x357939`. This inventory does not rule out
indirect writes. In particular, the `0x6924D` path can skip the increment
after calling `sub_00153AF0`, so elapsed host time alone is insufficient.

Evidence: `logs/native-screen-distortion-audit.log` and
`logs/native-screen-distortion-shared.log`. On Windows x64/MSVC, all 139 shared
timing, float/vehicle-join, CI-scope and backlog checks pass in 71.25 seconds.
Runtime build `8893ba8` is unchanged;
no rebuild or gameplay interruption is needed for this test-only audit.
Actual Mac execution and live distortion visuals remain unverified.

## Fractional screen-distortion clock

Following the preceding reproduction, shared source now tracks the fractional
difference between continuous elapsed level time and the original integer
clock. `src/pc_native_visual_time.h` owns that model;
`src/pc_native_visual_time_guest.inc` observes the six ordinary direct clock
increments and the three direct initialization/demo/network stores through
an additive `*-native-visual-time.patch`. Store values and integer mechanics
are unchanged. Ordinary increments consume the live float delta at the store;
explicit rebases clear the fractional difference, even for same-value writes.
A branch that does not write the clock does not advance this state.

The shared lifter adds a default identity policy for direct 32-bit global MOV
stores and an instruction-address-aware integer-to-float conversion policy.
Only stores to `0x7DE318` and the audited read at `0xF9726` use the new title
logic. Other stores/conversions and the original inactive-native path retain
their values. Render callbacks sample the clock; they do not accumulate time
per view or call. A different unobserved clock value falls back to its original
integer and rebases the next observed increment. Same-value indirect writes
cannot be distinguished this way; the known lifecycle sites are explicit
direct stores, but complete live lifecycle coverage remains required.

The complete original distortion fixture now compares three paths: original
whole time, an exact fractional probe, and the production store/sample policy.
The production policy matches the probe's repeated-output counts in all six
normal/quarter-speed 60/120/144 cases above, including **44 repeated outputs
instead of 672 at native 144** over eight game seconds. Original whole-tick
mesh/draw packets match exactly in those sequences. Integer pixel coordinates
still limit very small changes. Additional original-code checks cover paused
sampling, rebase/seek sampling and two player-view selections at the same time.

`tests/native_visual_time_guest.c` executes all nine original store
instructions from the pinned XBE, with a RET appended after each store. It
checks exact guest integer values, stack/callee-saved/FPU state, fractional
increments, catch-up, paused increments, same-value rebases, inactive-native
behavior and non-target numeric reads. These slices test store dispatch;
they do not execute the full enclosing state machines or network/replay code.
`tests/native_visual_time.c` additionally checks skipped-update/view sampling,
resets across a whole-tick carry, stale-positive-whole pause, mismatched or
unknown writes, signed-clock wrap and invalid-delta fallback. Accumulating
float deltas with a double offset has less than 0.006 tick drift over the
synthetic one-hour 144-Hz case (under 0.1 ms). That is a model measurement,
not a wall-clock runtime measurement.

All 146 shared timing, float/vehicle-join, CI-scope and backlog checks pass on
Windows x64/MSVC (`logs/native-visual-time-shared.log`, 71.87 seconds).
The store slice and distortion fixture are registered in the existing shared
native timing suite. All 21 patch-upgrade checks pass (399.99 seconds), and
the regenerated game contains exactly nine direct clock-store hooks and one
scoped distortion read. The generation fingerprint is current. Runtime source
is committed locally as `ae410c4`; the supported Windows Release build and
built-in checks pass, with matching crash symbols. The physical-controller
routing subtest skips because foreground activation was refused. Executable
SHA-256: `2c77a2aa320732d9a5b492c68c169d8104c446f5437cfc53626dbc9524dcbaab`.

The 120-second native-144 Story replay passes movement, hand animation,
firearm handoff, ammunition use, pause/resume and fresh guest-only presents.
The reviewed final capture shows the canyon, firing weapon and HUD. Its
7,846 active CPU intervals over 56.7008 seconds average 138.375 updates/s:
p50 7.056 ms, p95 8.8298 ms, p99 10.1903 ms, worst 34.8258 ms, game/wall and
raw/wall ratios 1.0. The older interactive session was still running during
this test. These are concurrent-load CPU measurements, not isolated FPS
improvement, stable-144, monitor scanout or live-distortion acceptance.

The subsequent original-60 replay reached Story but closed around 60 seconds,
before pause/resume and the scripted movement/fire checks. Its log records
`window closed by the user`, exit 0, and no runner termination. Validation
correctly reports incomplete/failed acceptance; this is not a passing
original-60 comparison or evidence of a crash. The older interactive PID
32624 also ended with a logged user window close. No window was relaunched.
The temporary original image was removed only after both its PID was absent
and the new native replay passed, with both image hashes checked. Personal
controls/display hashes stayed unchanged. Final captures and validation
records remain; redundant captures and disposable test disks were recycled.

Evidence: `logs/native-visual-time-{shared,upgrades,generated,build,live,original60}.log`,
the focused store/distortion logs, `private/pipeline/visual-time-build-handoff.json`,
and `private/test-saves/high-refresh/{native-144-,60-}visual-time-*` records.
Actual Mac execution, live distortion visuals, complete original-60 comparison,
full replay/network/level lifecycle behavior and world texture animation remain
unverified; this correction does not close the full high-refresh goal.

## Procedural-texture cache cadence

The original surface updater `sub_00127500` advances two wrapped texture
offsets at record `+0xB0/+0xB4`, velocities at `+0xA8/+0xAC` and another
phase at `+0xB8`. The downstream surface routine `sub_00127830` consumes
those offsets while constructing coordinate data, including at `0x127BD2`,
`0x127BF5`, `0x127D88` and `0x127DB1`. The float-delta offset integration
already advances on fractional intervals in the controlled zero-force/
zero-drag fixture. This does not establish all surface/material behaviors.

The same updater invokes procedural-texture dispatcher `sub_001956A0` using
a resource pointer at `+0xBC`. It compares a per-view timestamp against console
vblank `0x7497A8`, calls the dispatcher, then stores that vblank into the cache.
The dispatcher immediately returns when whole game ticks are zero. At native
rates a zero-whole frame can therefore populate the cache on a new vblank;
the following positive-whole frame shares that vblank and skips real work.
The two clocks have independent fractional phase, so this is not just an
incorrectly chosen FPS constant.

`tests/native_surface_cache.c` executes the complete pinned US caller
(`0x127500..0x12782C`) and dispatcher (`0x1956A0..0x19576D`). Two controlled
material records alias one procedural resource. Its graphics services and
five nested texture-pass bodies are spies; the outer call sequence and two
resource swaps execute original instructions. The fixture checks the full
sequence and restored handles, callee-saved registers/stack/FPU depth,
unchanged timing globals and integrated U/V/phase values. It does not execute
the nested GPU kernels, produce a procedural image, or prove material loading.

Over eight game seconds at native 144, the original cache produces respectively
480, 192, **0**, and 192 updates for display-clock phase offsets of 0, 1/4,
1/2 and 3/4 tick. At native 120, the latter two offsets also suppress all
updates. Native 60 has no such collision in these sequences. These controlled
phase offsets reproduce a timing failure; they are not measurements of its
frequency in live levels.

The shared numeric policy now maps only the two vblank loads at `0x12764C`
and `0x127694` to native frame/update counter `0x7497B8`. Every advancing
native update gets a fresh cache key, while aliases within a view still
deduplicate. The procedural callee keeps its authored whole-tick gate and
positive-interval pass sequence, including its original one-dispatch
catch-up behavior. Its load at `0x1956A0` now reads zero during a native
zero-float pause, preventing stale positive whole counts from evolving the
image. That register is replaced by a graphics-state return value before
subsequent use. Native-disabled behavior and neighboring loads stay original.

The fixed fixture restores 480 updates in each normal/quarter-speed
60/120/144 sequence across all four phase offsets and view slots 0 and 3.
It checks every individual frame, not just the final count. Additional cases
cover a second view's independent stamp, shared-resource aliases, ordinary
pause/resume, the stale-whole paused-image counterexample and multi-tick
catch-up. U/V/phase values retain their original constant-velocity integration.
The procedural GPU algorithm is still discrete; evaluating its image at
fractional intervals requires auditing its nested passes and coefficients.
This cache correction alone does not establish native-144 procedural simulation
or complete texture fluidity.

Evidence: `logs/native-surface-cache-before.log` reproduces the failure;
`logs/native-surface-cache-fixed.log` records the correction. The fixture is
registered in the existing shared native timing suite. All 147 shared timing,
float/vehicle-join, CI-scope and backlog checks pass in 61.92 seconds
(`logs/native-surface-cache-shared.log`). The unchanged complete patch stack
verifies with zero updated files (`logs/native-surface-cache-patches.log`);
no new patch or generator change is needed for these existing load-policy sites.
Windows source checkpoint `b3c30cf` builds successfully through the supported
MSVC x64 Release workflow and passes its built-in checks
(`logs/native-surface-cache-build.log` / `.err`). Executable SHA-256:
`3b73d1223ffc53841fb3431ccc3caf79dd4afd8829c576687151fcc9f2e73f1e`.
Personal controls and display files match their pre-build hashes. The user
requested an interactive launch of this build at native target 144; PID 22536
was responding, using the normal personal save route. Startup is not gameplay
or texture visual acceptance. The previous original-60 comparison remains
incomplete; actual Mac execution and live surface/texture acceptance are open.

The lifecycle extension executes original resource lookup/allocation
`0x127400..0x127494` and initialization `0x194B00..0x194BB0` as well. Five
controlled pool slots start with poisoned stamps. New allocation sets all six
stamp fields to `-1`; requesting an existing resource preserves its stamps,
and an exhausted pool returns null. Recycling one slot at the same native
update ID invalidates the previous resource's stamps and permits a fresh
update. Four player views each dispatch once, with duplicate calls suppressed.
Original initialization sets the resource tables, handles and image state;
render-target binding and the two 128-by-128 clears are argument-checking
spies, not GPU work. The focused fixture passes in 1.27 seconds
(`logs/native-surface-cache-lifecycle.log`). This tests allocator behavior with
a controlled recycled slot, not a complete level-unload/load sequence.

The nearby clock reads in `sub_00195810` select strings using shifted vblank
bits; they do not access these resource stamps and remain unchanged. A direct
call scan finds the procedural dispatcher only at `0x127684`; that does not
exclude indirect callers. The surface construction routine's broader material
and GPU behavior remains outside this fixture.

## Fractional strip distortion

The separate screen effect `sub_000F9B10` samples whole level time at `0xF9B4B`,
divides by the original ticks-per-second value and uses two sine waves to
offset screen-copy source coordinates. It emits two-row strips through
`sub_0001F090`; destination coordinates, row stepping and tint remain original.
The absolute-time read is continuous visual phase, not an event counter.

`tests/native_strip_distortion.c` executes the entire pinned US routine
(`0xF9B10..0xF9CAF`) and the original sine-table generator/FSIN helper. Graphics
services are spies that capture every nine-word copy packet. In a controlled
640-by-448 region the routine emits 224 strips. The fixture compares original
whole time, exact rational fractional time, and production visual-clock
sampling through the existing direct-store observer.

Over eight game seconds at normal speed, original native 120 repeats 480 of
960 frames, and native 144 repeats 672 of 1,152. The correction repeats zero
in both sequences. At quarter speed, original 60/120/144 repeat respectively
1,440/3,360/4,128 frames, versus zero with the correction. These are captured
draw-argument counts, not live frame-pacing or GPU measurements. Original
whole-tick packets match exactly at every common boundary. Native 60 normal
speed remains identical to original 60.

Every production frame is compared with the exact fractional reference.
All normal-speed cases and quarter-speed 60/120 match byte-for-byte. Five of
4,608 quarter-speed 144 frames differ by at most one source-coordinate pixel:
accumulated float deltas differ from rational time by at most 0.000031 ticks
at those samples, crossing a lookup-table/integer-pixel boundary. Destination
coordinates, extents and tint stay exact. The assertion explicitly bounds
this rounding difference rather than calling the outputs identical.

Additional cases check repeated paused draws, explicit clock rebases, four
views sampling without advancing time, clipped X/Y ranges with odd/even
heights, empty-range early-outs, neighboring unmodified sites and the original
native-disabled path. The only runtime change adds `0xF9B4B` to the existing
site-specific fractional visual sampling policy in `pc_native_numeric.h`.
Guest clocks and every other read remain unchanged; no new patch is needed.

Evidence: `logs/native-strip-before.log` records the original production-policy
failure at 120; `logs/native-strip-fixed.log` records all six corrected cases.
All 148 shared timing/float/vehicle/CI-scope/backlog checks pass in 70.07 seconds
(`logs/native-strip-shared.log`). The unchanged complete patch stack verifies
with zero updates (`logs/native-strip-patches.log`), and the generated caller
contains the site-aware conversion at `0xF9B4B` with a current generation
fingerprint. The full Windows MSVC x64 Release build of `a08f41a` and its
built-in checks pass (`logs/native-strip-build.log` / `.err`). Executable SHA-256:
`e4082f51b9d4fb9ee595f7eb06db8982973bb6bcfab4df8f60bb3dababc4e163`.

The user's existing PID 22536 remains on `b3c30cf`, retained as
`futureperfect_recomp.running-b3c30cf.exe` while the normal executable was
rebuilt. It was not terminated or restarted. Personal controls/display hashes
remain unchanged. Live strip-effect triggering, actual GPU visuals, complete
original-60 gameplay comparison and Mac execution remain pending. This does
not establish full-game native 144 FPS coverage.

A separate read-only observation of that older interactive session found
`attract.xmv` playback in the log while the guest update counter stayed at
18,092, console vblank at 38,742 and level clock at 7,550 for six seconds in
scene 101. RHI present counts continued growing. Those counters represent
accepted API submissions, including repeated movie pictures, not fresh game
updates or decoded movie frames. The shared recorder already excludes positive
skip statuses and negative failures. Movie-loop pacing and fresh-picture
accounting require a separate audit; this observation was made during a build
and is not a performance benchmark. Evidence is
`private/pipeline/strip-interactive-counter-audit.json` and the corresponding
`logs/play-20261020-180012.err` movie/presentation records.

## Native movie polling

The live `b3c30cf` attract-movie observation above exposed a separate loop:
native mode bypasses `xbox_Nv2aFlipGateArm()` at guest swaps, but movie playback
does not call the level-update pacer. `xmv_play_update` frequently returns
NOFRAME while the guest keeps submitting the same movie layer. Consequently
accepted present calls can increase far faster than decoded pictures or guest
updates. This is a pacing gap, not evidence of fresh high-refresh gameplay.

`src/pc_native_movie_pacing.h` adds a per-playback rational schedule using the
existing `PcNativeClock`, native-rate setting and platform clock/wait interface.
The first poll is immediate; subsequent polls wait until the next native
60/120/144 slot. Already-paced callers do not wait twice. Late work skips missed
slots without an immediate catch-up burst. Rate/clock changes rebase the poll
schedule, timer failures fall back to a short sleep, and playback close releases
the timer. The guest clock, video timestamps and movie wall clock are untouched.

The additive `*-native-movie-pacing.patch` places the wait in the existing host
player after handle validation and before processing its next poll. It stores
the pacing state in that player's movie object and closes it with playback.
Disabled native mode returns immediately. The existing host decoder is only
available on Windows; the Mac player remains a stub under MAC-009. The pacing
helper is shared and should be consumed by that backend when implemented.

`tests/native_movie_pacing.c` checks exact rational cadence at 60/120/144 with
two counter frequencies, independent playback schedules, already-paced calls,
late work, reset/disable, timer create/set/wait failures and timer lifetime.
`tests/native_movie_update.c` compiles the actual patched movie structure and
get/start/update/time/close/audio-feed bodies. Demux, decoder, audio output and
rendering are synthetic backends; the fixture does not decode a real file.
Its ninety 20-FPS packet timestamps and 2,880 PCM bytes are identical with and
without pacing, including rejected audio submissions retried without loss.
At a modeled 100 microseconds of guest work per poll, native 144 reduces calls
from 44,502 to 643. EOF is observed at 4,458 rather than 4,450 ms, within one
poll interval after the final picture; timestamp values are unchanged.
Invalid/closed handles do not wait, and close releases the timer exactly once.

Evidence: `logs/native-movie-pacing-model.log` and
`logs/native-movie-update.log`. These checks establish scheduling and update
protocol behavior, not decoded image quality, audible synchronization, real
CPU savings or human skip latency.

Source `3c2fcc2` passes all 152 shared timing/float/vehicle/CI-scope/backlog and
existing movie-stop checks in 76.77 seconds (`logs/native-movie-shared.log`),
plus all 21 complete patch-upgrade checks in 351.81 seconds
(`logs/native-movie-patch-upgrades.log`). The supported Windows MSVC x64 Release
build and built-in checks pass (`logs/native-movie-build.log` / `.err`).
Executable SHA-256:
`16ddf892fc209737f682345c5d7203945bc531a52dadb471e9f231c180c753f7`.
The interactive PID 22536 remains on its retained `b3c30cf` executable; no
restart or game input was performed for this validation. Personal controls and
display hashes match the pre-build record. A redundant 274,829-byte standalone
compiler folder remains at `build/path-tests/native-movie-file`: automatic
approval review blocked its cleanup; no alternate deletion was attempted.

`tests/test_native_movie_file.py` additionally compiles the complete Windows
host player plus the real XMV demuxer, FFmpeg decoder and ADPCM helper against
the local verified dependency. It reads `private/game/xmv/eag_e.xmv`, with a
controlled host clock/wait implementation and mocked GPU/audio-device output.
It runs windowlessly, does not touch saves/settings, and explicitly skips on
hosts without the Windows backend or local data/dependency. The shared CI
workflow includes this optional test; a skip does not establish real decoding.
The local test passes in 2.14 seconds (`logs/native-movie-real-file.log`).

Original 60-Hz polling and native 60/120/144 all emit 88 pictures with identical
full BGRA sequence hash `ab1283f013e63d41`, timestamp sequence hash
`6e7e769a7ca873f5`, and 511,912 PCM bytes with hash `84ca1802fb1feb6d`.
Poll counts are 177/177/351/420, and modeled EOF observation times are
2,933/2,933/2,916/2,909 ms respectively. The last picture and following EOF are
observed at polling boundaries; picture timestamps themselves remain exact.
This compares actual decoded content and update protocol, not real-time CPU
usage, OS timer accuracy, audible synchronization, on-screen presentation,
skip/input latency, attract-movie completion or full cinematic coverage.

The file fixture now also builds with `LIVE_HOST_CLOCK`, using real Windows
QueryPerformanceCounter and waitable timers without virtual-time overrides.
All native 60/120/144 runs emit the same 88 pictures, full image/timestamp
hashes and 511,912 PCM bytes as the controlled original-60 reference above.
After close, the saved timer handle fails `GetHandleInformation` with
`ERROR_INVALID_HANDLE`, and the player's timer/state fields are cleared.
No GPU window or audio device is opened; the user's interactive game remains
running on `b3c30cf` throughout. The two focused tests pass in 12.24 seconds
(`logs/native-movie-windows-timers.log`).

| Native target | Poll intervals | Mean (ms) | p50 (ms) | p95 (ms) | p99 (ms) | Max (ms) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 60 | 176 | 16.6547 | 16.6287 | 19.1922 | 19.5307 | 20.0750 |
| 120 | 350 | 8.3262 | 8.5138 | 10.6537 | 10.9167 | 11.0859 |
| 144 | 419 | 6.9374 | 7.0187 | 9.2085 | 9.5942 | 9.9869 |

These are intervals between completed player polls, including actual decode
and full-frame test hashing, measured alongside the older interactive session.
They demonstrate bounded host polling and successful OS-timer integration,
not isolated wake-up latency, GPU presentation, scanout or full-game frame
pacing. This follow-up changes fixtures/documentation only; the ready game
executable remains the validated `3c2fcc2` build.

## Bridge-crash placement investigation

The user reports a later ship crash intruding into the playable canyon,
on the way to the base entrance immediately after the bridge blows up.
Two user screenshots show the obstruction beside the destroyed bridge.
The initial investigation below established the scene evidence. A subsequent
original-code reproduction identifies a timing defect and source correction,
recorded at the end of this section; rebuilt scene verification remains open.

The running Windows executable remains the tested `cfaef73` build with
SHA-256 `afb5ed6a71c05f3c8a3ddc1ede825e8f2f07f4a05d39dc3901de665a35840d26`.
The user requested continued gameplay during investigation. The saved
`private/pipeline/ship-bridge-user-snapshot.json`, `ship-bridge-models.json`
and `ship-bridge-static-models.json` are bounded, read-only process-memory
observations; they do not send game input, change memory, or change saves.
The first snapshot contains 66 entities and player position approximately
(-88.60, -11.08, -91.54).

Two debris objects have clips 2536/2537 (`crash_debris01/02`) with unchanged
previous/current times of 1.814850 seconds. The bridge object has clip
2550 (`bridge_collapse_`) at 4.062600 seconds and callback 0x216E00.
Their sampled bone bases have unit lengths. These observations rule out
neither an incorrect terminal pose nor a separate misplaced ship.
The four sampled type-0x80 ships use ambient clips 2512–2515 and continue
moving. A separate type-1 object at approximately (18.08, -26.12, -30.28)
has a large bounding extent, clip zero, and model definition 0x81416728;
its relationship to the visible obstruction still needs confirmation.
Do not identify it solely by entity address 0x8204BB80: an earlier snapshot
of this same pool slot contained an NPC. Pool addresses are recycled.
The generic tracer now records type, tag and model definition explicitly,
and the runner records the tracer's source hash for future runs.

The two completed 120-second isolated opening runs,
`60-ship-path-reference` and `native-144-ship-path-native`, each contain
73 complete entity-list samples. Their existing smoke checks pass, but
neither reaches the bridge event. Ambient positions at equal wall times
have different clip phases; those differences are not evidence of a path
regression. Both used runner SHA-256
`1cdfe69268a9bdb55ace432c47c94503437f8ff10ed74a74c5770b615a187626`;
they predate the tracer-hash metadata field. The subsequent
`60-bridge-reference` run was stopped to return the game to the user and
is incomplete, not a passing bridge comparison.

At this stage, identifying the ship's model and spawn/movement/settling path
was still pending. The later investigation below resolves that identification.
A bridge-event comparison at original 60 Hz and native 144 Hz with isolated
data remains required. No speculative position offset or asset edit has been
applied. Mac behavior is unverified.

The offline timing, entity-trace, float/vehicle translation, audio and CI
scope checks report 144 passed and one expected failure in 44.21 seconds
(`logs/native-ship-audit-regressions.log`). The expected failure is the
previously measured object-rocking cadence gap, not this ship report.
The Mac backlog integrity check also passes. These tests validate the
diagnostics and existing covered behaviors; they do not resolve the scene.

### Mothership identification and source correction

The later read-only template snapshot (`private/pipeline/ship-templates.json`)
identifies entity 0x8204BB80 as model 0x53A. Its template name/path pointers
resolve into the level's object-name table at offsets 0x10753 / 0x1077A:
`INTRO_TIMESPLITTERS_MOTHERSHIP_CRASHED` / `intro/intro_mscrash`.
The original updater at 0x217050 spawns this model at 0x2185A7 with the
live mothership's position and orientation, then removes the moving object.
This supersedes the earlier uncertainty about which object was the wreck.

The updater's entry load at 0x217059 is only a pause gate, with its EAX
value replaced at 0x217090. Its forces, position integration, angular motion,
crash timer and associated shared timers use 0x4E794C, the fractional delta.
Skipping zero-whole-tick frames therefore loses motion time. Meanwhile,
the angular and linear damping loops consume the carried whole ticks,
so damping becomes too strong relative to elapsed motion time. This changes
the crash trajectory, not just its apparent frame rate.

`src/pc_native_numeric.h` now allows this gate on every positive fractional
interval in native mode. Its two damping loops and all other loads remain
unchanged. Pause still stops the callback, original 60 Hz behavior is
unchanged, and the original multi-tick catch-up path stays intact.

`tests/native_mothership.c` runs the complete original 0x217050–0x21862F
updater, original shared effect-timer helper 0x2157B0, vector transforms and
scalar approach helper. It supplies a controlled airborne state, centered
random wobble, a sine table and an effect-matrix adapter. Sound, particles,
damage/player feedback, asset creation and deletion are bounded fixture
services; the actual wreck-creation request is captured and checked.
It does not exercise collision/rendering, audio output, the preceding
combat/trigger sequence, or the full random-number stream.

Normal-speed results from the same starting state:

| Path | Updates/s | Changed transforms in first second | Crash seconds | Wreck X | Wreck Y | Wreck Z |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Original reference | 60 | 60 | 9.66667 | 48.60029 | -44.30268 | -119.41282 |
| Original gate at native cadence | 144 | 60 | 26.58333 | 43.02375 | -26.25528 | -128.49136 |
| Corrected native gate | 120 | 120 | 9.64167 | 48.28055 | -44.04354 | -119.62134 |
| Corrected native gate | 144 | 144 | 9.64583 | 48.36892 | -44.14344 | -119.55445 |

Assertions cover six 60/120/144 × normal/quarter-speed combinations:
fresh position each advancing callback, one-second velocity within 0.005,
position within 0.03, complete crash time within 0.05 game seconds, landing
within 0.75 units per axis and angles within 0.05 degrees of the original.
These tolerances allow sampled height/time thresholds at state transitions.
The original 60 Hz result is byte-identical with the native gate enabled,
as are separate one- through six-tick catch-up calls. Paused entity state,
callee-saved registers, stack, FPU balance and restored clock fields are
checked. A counterfactual run without the gate correction must still
reproduce the late/high crash. Measurements:
`logs/native-mothership-regression.log`.

The patch stack validates without runtime-file changes, and the existing
generated production routine already uses the scoped global-load hook at
0x217059. No hand-edited generated C or modified game asset is involved.
The broader native timing, entity-trace, float/vehicle translation, audio,
CI-scope and Mac-backlog checks pass: 146 passed and one expected failure
in 49.07 seconds (`logs/native-mothership-shared-regressions.log`).
The expected failure remains the separate object-rocking cadence gap.
An additional schedule alternates intervals from 2/12 to 10/12 of a
game tick and repeats at quarter speed, carrying whole ticks independently.
It pauses twice in each of crash states 3, 4 and 5, including immediately
before wreck creation; entity state, spawn count and retirement count stay
unchanged during every pause. Both schedules create exactly one wreck and
retire the ship once. Crash duration is 9.6333/9.6347 game seconds, with
landing coordinates within 0.39 units per axis of the original 60 FPS
reference. The focused original-code test passes in 42.25 seconds while
the Windows build runs (`logs/native-mothership-variable-regression.log`).
These checks retain centered random wobble and bounded effect stubs;
they do not replace a bridge-scene playthrough.
After the user's interactive process had closed (both process-name and
Win32 process queries found no game process), the supported
`tsfp.ps1 -Action Build` completed successfully, including its built-in
Windows tests. The runtime source is `d4c5ee8`; the subsequent changes
only extend this fixture and documentation. The new Release executable
SHA-256 is `08ba9a6be1d6f078123e53e69a00254e05b6fc7f131f4e896ec4f3124c250c61`.
Build output is `logs/native-mothership-windows-build.log`.
The rebuilt executable subsequently passed a 120-second isolated Test Lab
run at native 144 with movement/firing and pause/resume replay, using
`tests/native_refresh_game.py --fps 144 --native --label ship-fix-build
--stress --pause-check --seconds 120 --windowed`. All recorded checks pass,
including opening firearm handoff, advancing/changing hand animation,
ammunition consumption, fresh guest presents and no interpolation.
The runner terminated its child at the duration limit; recorded exit code 1
is that intentional termination, not a normal game exit.
Evidence is `private/test-saves/high-refresh/native-144-ship-fix-build-validation.json`
and `logs/native-mothership-built-smoke.log`.

The unpaused CPU-update sample starting at 85 seconds contains 4,743
intervals across 34.3823 seconds: 137.949 updates/second, p50 7.0546 ms,
p95 9.0974 ms and p99 10.1704 ms, with game/raw time both 1.0 seconds per
wall second. This windowed 1280×720 opening-area test does not establish
144 FPS throughout the game or monitor scanout. It does not reach the bridge.
Audio initialized through XAudio2 at master gain 1 and submitted nonzero
samples; this does not establish audible output at the user's device.
An isolated bridge-scene comparison is still required before calling the
reported obstruction resolved; actual Mac execution and full mission/
random-effect coverage remain pending.

### Ship debris inherited velocity

A separate original initializer, 0x2168C0–0x216CA5, creates 30 debris
fragments and a final effect. It derives parent velocity from the difference
between entity+0xB4/B8/BC and entity+0xE0/E4/E8, multiplied by
60 / elapsed game ticks. Its gate at 0x216958 skipped that division whenever
the carried whole-tick count was zero. At 144 FPS, a parent velocity of
(12, -24, 36) consequently contributed only (5, -10, 15) on those frames.
This makes fragment motion depend on which frame triggers destruction.

The shared numeric policy now uses positive fractional time for that
division guard. EAX is replaced at 0x216987 before any other use.
This does not open a periodic spawn gate: fragment count, model selection,
random calls, effect arguments and allocation-failure behavior stay original.
Zero-time initialization keeps the original no-division behavior; this
one-shot event still creates fragments when invoked with zero elapsed time.

`tests/native_ship_debris.c` runs the complete original routine with its
original scalar constants and model table. The allocator and final effect
service capture requests, and a repeatable xorshift stream supplies both
positive and negative random values. The fixture covers both model-selection
branches, inheritance enabled/disabled, successful/all-failed/alternating-failed
allocations, fractional normal/quarter-speed intervals, carried zero/one ticks,
two/six-tick catch-up and zero time. Native one-tick output is byte-identical
to the original. Other schedules match inherited velocity within 0.0001;
all remaining output bytes, including RNG call count and generated rotations,
must match exactly. Parent data, guest time fields and ABI are preserved.
The original fractional counterexample remains in the test.

The new test failed at the inherited-velocity assertion before the correction
(`logs/native-ship-debris-before.log`) and passes afterward
(`logs/native-ship-debris-after.log`). This correction is in shared source;
the interactive executable built from `d4c5ee8` does not yet include it.
Full Windows rebuild, rendered debris review and actual Mac execution remain
pending. The separately tested mothership landing fix remains in that build.
The full shared timing, entity-trace, float/vehicle translation, audio,
CI-scope and Mac-backlog suite passes: 147 passed and one expected failure
in 50.69 seconds (`logs/native-ship-debris-shared.log`). The expected failure
is still object rocking. The complete patch stack validates with zero runtime
files changed; generated production C already has the scoped load at 0x216958.

### Additional first-mission reports awaiting reproduction

The user reports enemies before the bridge that react to damage and die
but neither move nor shoot; trails from any fired gun appear about three
seconds late, increase with additional shots, and eventually disappear;
and the weapon described as the plasma sniper does no damage.
These are open behavior regressions, not covered by the passing opening-area
smoke check. Existing NPC fixtures isolate dispatcher/firing arithmetic and
mock world/actor/projectile services; they do not prove live AI transitions,
projectile collision, damage or trail timing.

The reported build is the interactive executable with SHA-256
`08ba9a6be1d6f078123e53e69a00254e05b6fc7f131f4e896ec4f3124c250c61`.
The screenshot `codex-clipboard-d06e74af-098b-4c32-9dff-eaf353fcf3b4.png`
shows several red trails near the bridge. A bounded read-only 30-second
entity observation is saved in
`private/pipeline/npc-trails-user-observation.jsonl`. At least one NPC moves
during this observation; other actors include a fixed `portalnonjumper02`
pose and `zomdeath01` poses. Actor addresses can be recycled and this
post-bridge observation does not identify the particular pre-bridge enemy.
Do not infer a root cause or resolution from those pose names alone.


### Projectile travel on fractional frames

Source baseline `b551804`; shared gate change at 0xF4A66 and
`tests/native_projectile_motion.c` are the next local checkpoint. Windows x64,
pinned US XBE, repository Python/MSVC test toolchain. No personal saves,
settings or live process memory were modified. The interactive process later
closed independently; both Get-Process and Win32_Process queries found none.

The bounded read-only 45-second observation in
`private/pipeline/npc-projectile-observation.jsonl` contains 179 snapshots
(10,148,429 bytes). Captured kinds 28/31/45 use updater 0xF4A50. A continuous
kind-31 segment with speed 240 travels about 24.9 units in 0.25 seconds while
its age advances by 0.25 seconds: approximately 100 units/s rather than 240.
The original routine skips its fractional displacement/collision sweep when
integer delta is zero, but outer 0xDC7B0 advances age on every callback.
The shared change opens only that entry gate for positive live game time.

The new fixture translates the complete original 0xF4A50..0xF51AF routine.
A deterministic collision plane replaces the world query; impact, damage,
rendering and effect services are captured rather than executed. Before the
fix it fails the requirement for one sweep per advancing interval
(`logs/native-projectile-before.log`). After the fix it passes
(`logs/native-projectile-after.log`):

- Kinds 28/31/45, speeds 180/240/480, 60/120/144 FPS and quarter speed.
- At speed 240, a plane 180 units away is reached at about 0.75 seconds,
  versus 1.80555 seconds on the unpatched native-144 schedule.
- A plane 360 units away receives one impact/damage callback at about
  1.50694 seconds; the unpatched shot expires after traveling only 200 units
  and never reaches it.
- No-hit expiry and cleanup, exactly one hit/retirement, zero-time pause on
  every frame, uneven intervals, two-/six-tick catch-up, ABI/timing integrity,
  and byte-identical native-60 free-flight state.

Limits: these callbacks do not prove enemy health decreases, plasma-sniper
identity/damage, trail appearance or pre-bridge AI recovery. The observed
kind-35 updater 0xF08D0 has a similar gate plus homing/decay behavior that needs
separate coverage. The sum-of-squared-step accumulator at projectile +0xF4 is
frame dependent and can feed the original damage-falloff helper 0xB9920 via
0xDCEF0. This change does not redefine that mechanic; cross-rate falloff and
partial-hit behavior remain explicit follow-up work. The focused shared suite passes 148 checks with one existing object-rocking
expected failure (`logs/native-projectile-shared.log`). Full patch-stack
validation passes with zero runtime files updated; generation is current.
Full Windows rebuild, scene replay and actual Mac execution are pending at
this source checkpoint.


The supported Windows Release build of `ef2ace3` and built-in tests pass
(`logs/native-projectile-windows-build.log`). Executable SHA-256:
`9c9f801015f988ad4ce952d54136324c6117e2dd2324802484ef9c3b7114d77d`.
The 120-second isolated Test Lab native-144 run passes movement, equipped
firearm/ammunition, hand animation, pause/resume, native hook and successful
guest presentation checks, with interpolation disabled. Evidence is
`private/test-saves/high-refresh/native-144-projectile-travel-validation.json`
and `logs/native-projectile-built-smoke.log`. Runner termination is intentional
(exit 1); completed/passed are true. The 73 bounded entity snapshots are in
the companion entities JSON and do not reach the reported bridge encounter.

From 85 seconds onward: 4,825 unpaused Story CPU intervals over 34.7362
seconds, mean 138.904 updates/s, p50/p95/p99 7.0446/8.9396/9.9019 ms,
worst 12.3229 ms, game/raw clocks both 1.0 seconds per wall second. This
windowed 1280x720 opening check is neither scanout measurement nor a full
campaign/damage test. The interactive launcher was started again at native
144 with personal settings/saves after the isolated child exited.

A read-only inspection of the owned isolated child's weapon table located
it at guest 0x4EEF74 (pointer 0x4F9BAC). Weapon record stride is 0x29C;
fire-mode flags start at +0x68 and projectile kind is +0x6C (secondary +0xE8).
Weapon ID 17, recorded throughout the earlier user observation, uses kind 24
for both modes. Its descriptor selects initializer 0xE36D0 and updater
0xF5FF0, which calls the immediate trace/hit helper 0xF5D30. Therefore the
0xF4A50 travel fix does **not** resolve that weapon's damage report.
Kind 24's original hit path reaches damage setup 0xB91F0/0xB9220 and
application 0xBD0F0; actual health behavior is still pending reproduction.
The small table snapshot is ignored at
`private/pipeline/projectile-weapon-table.bin`. No game memory was written.


### Sniper damage on fractional frames

Source baseline `2d90c6c`, Windows x64, pinned US XBE and repository MSVC/Python.
The previous projectile-travel fix does not affect weapon 17 / shot kind 24.
Tracing its damage chain found a second independent timing gate:
`sub_000BB6C0` returns immediately when integer delta 0x4E7944 is zero.
The sniper's immediate ray can hit on such a frame, calculate positive damage,
then retire before a later whole-tick frame. Its health change is lost.
The gate's EAX is replaced at 0xBB6D2; no counter consumes its value. The
shared `pc_native_numeric.h` policy now tests positive live game time at that
load only. Damage amounts, health arithmetic, network policy and continuous-
damage scaling are unchanged. Zero live time still blocks application.

`tests/native_sniper_damage.c` translates the complete original routines:
0xE36D0 initialization, 0xF5FF0 update, 0xF5D30 hit processing,
0xB91F0/0xB9220 damage setup, 0xBD0F0/0xB9790/0xB9720 damage calculation,
and 0xBB6C0/0xBB490/0xBA310 damage dispatch and actor health subtraction.
Original scalar and damage-table constants come from the checksum-verified XBE.
The world ray result, region choice, network/eligibility-service response,
modifiers, statistics, sound/render effects and reactions are controlled
fixture services. The actual subtraction of entity +0x1CC runs original code;
it is no longer replaced by an impact-call counter.

The initial arithmetic-only fixture passed with damage 0.65 at every rate;
that did not establish health loss. Extending through application reproduces
the failed health effect (`logs/native-sniper-health-before.log`). With the
source correction, `logs/native-sniper-health-after.log` verifies:

- An accepted hit reduces health 1.00 to 0.35, matching original 60 FPS,
  with deltas 1, 1/2, 5/12, 1/4, 5/48, 2 and 6 ticks.
- Fractional intervals work with either zero or one carried integer tick.
- Misses, untargetable actors, rejected damage and zero-time pause leave
  health unchanged. The unpatched zero-whole positive-time case also leaves
  health at 1.00, explicitly preserving the counterfactual reproduction.
- Each shot performs one trace and one beam emission, then retires once;
  the next update does not apply duplicate damage. ABI, x87 stack balance
  and live timing fields are preserved.

A fixture boundary typo initially omitted the miss-path RET from 0xF5D30;
correcting its range to end at 0xF5FEC resolved that fixture-only stack mismatch.
It was not a production translation defect. This source change is shared
with Mac. Full Windows rebuild, actual user-scene sniper damage confirmation
and actual Mac execution remain pending while the interactive PID 2212 runs.
The delayed trails and pre-bridge NPC behavior remain separate open reports.


The focused shared regression suite passes 149 checks with one existing
object-rocking expected failure in 52.91 seconds (`logs/native-sniper-shared.log`).
Full patch validation passes with zero runtime files updated
(`logs/native-sniper-patches.log`); generation is current. The production
translation already calls the shared policy at 0xBB6C0, so no generated C was
edited. These are offline checks, not a rebuilt or live-verified sniper fix.


### Beam-manager lifetime audit

Source baseline `e7da2e4`, Windows x64, pinned US XBE. The kind-24 sniper
updater emits type-32 effects through 0x104F30. That constructor sets age zero
and lifetime 1.2 seconds. The complete manager 0x10A760 uses float delta
0x4E794C, stores its effective delta in 0x73BBA8, and advances effect age
by delta/60. Type-32 dispatch calls 0x1053D0. The inspected gameplay caller
0x1CE2E0 reaches this manager without a whole-tick gate on the normal path.

`tests/native_beam_lifetime.c` executes the complete original manager and
original descriptor/switch tables. Type-32 geometry updates and unrelated
subsystem services are spies; expiry captures the original decision while
replacing allocator cleanup. This isolates manager scheduling, not complete
beam construction, rendering or GPU resource lifetime. It does not prove
which effect produced the user's red trails.

`logs/native-beam-lifetime.log` passes 13 schedules: original 60, native
60/120/144 at normal/quarter speed, and explicit 0.3-second deferred starts.
With no explicit delay, the updater runs on the first advancing frame.
Expiry is 1.216667 game seconds at original/native 60 and 1.201389 at native
144, within normal step/float boundary rounding of 1.2 seconds. Deferred
starts remain within one step of their requested time, and total expiry
within two steps of delay plus lifetime. A zero-time callback before each
live frame leaves age unchanged; register preservation, x87 stack balance
and timing globals are checked. The manager may visit its geometry updater
at zero time; this fixture does not assert that all visual state is frozen.
An initial fixture omission left the constructor's lifetime constant unloaded;
loading its verified original value resolved the fixture-only failure.
No production timing change was justified by these results.

A bounded read-only observation of interactive PID 2212 collected 596
snapshots over 30 seconds (`private/pipeline/fx-trails-observation.jsonl`,
72,977 bytes), with advancing time but no active effect heads. It did not
capture firing or the reported delayed trails. No input, memory writes,
restart or personal-settings changes were performed. Further reproduction
and render-path investigation remain necessary; actual Mac execution is
also pending. Sniper source fix `e7da2e4` still needs a Windows rebuild and
live gameplay verification; the current interactive executable remains
`ef2ace3`.

The timing, CI runtime-scope and Mac-backlog checks pass 94 tests with one
existing object-rocking expected failure in 33.61 seconds
(`logs/native-beam-shared.log`). The new harness is included in the existing
CI timing suite. This is audit coverage, not a delayed-trail fix.


### NPC animation-marker shots

Source baseline `b6b9311`. While tracing the non-shooting NPC report, two
animation-event callers were found: 0x27AE14..0x27AE83 and
0x2B9BCE..0x2B9C0C. Both query marker mask 0x200 through original 0x61570,
then call 0x1BB5C0 and clear temporary firing flags. The marker selector
recognizes crossed animation bounds; a rejected shot cannot be retried after
the bounds advance. The weapon's whole-tick gate rejects a positive fractional
interval with zero integer ticks, even when an actual one-time event reached
it. These callers are reached through 0x27D590 in mode 0x0A; this does not
identify the user's particular first-mission enemy or explain no movement.
Other one-time attack transitions (0x2612FB/0x2B739D) need separate auditing.

The extended `tests/native_npc_weapon_fire.c` executes original 0x61570,
both complete event-test/call/flag-cleanup branches, and the existing full
firing/sound chain. Only each caller branch's following state transition is
replaced by RET. Animation lookup/table/bounds are controlled services;
projectile construction, collision, live AI and audio output remain mocked.
Calls enter the same manual wrapper as production, while the original firing
body remains `_gen`. The first fixture attempt reversed the two animation
bound outputs; correcting them restores the original-60 reference. Production
code was not responsible for that fixture setup error.

The pre-fix original-60 event produces one shot and sound; the fractional
0.5-tick event produces neither (`logs/native-npc-animation-event-before.log`).
A retained counterfactual using the previous transaction wrapper confirms
that the following whole-tick frame cannot recover the event.

Shared `pc_native_npc_weapon_fire_guest` recognizes only the two audited
return sites (0x27AE66/0x2B9BF8). A positive zero-whole event gets one scoped
firing transaction, using the existing event timer/sound scope; original live
time is restored on return. It does not open the gate for held-fire callers,
change the global clock, queue stale actors or alter the original damage
amounts. Pause, even with a stale nonzero integer tick, remains blocked. The
network wrapper remains outside this helper and receives the original caller
stack and restored timing. The additive `native-npc-animation-shots` patch is
ordered after the existing NPC event-scope patch; historical-stack fixtures
exclude it along with its prerequisite when recreating old states.

`logs/native-npc-animation-event-after.log` verifies one shot, one sound,
one ammo debit and one event-timer increment for both callers with deltas
1, 1/2, 5/12 and 5/48. An unchanged bound on the following frame produces no
duplicate. Zero-time and invalid-target/weapon cases produce no firing or
timer changes. Existing 96 held-fire configurations, 60/120/144, quarter speed,
variable/catch-up schedules and pause checks continue to pass. Full stack
application passes and updates only the manual runtime wrapper
(`logs/native-npc-animation-event-patches.log`). Existing production generated
C already calls the manual NPC entry with the two original return addresses;
no generated C was edited. Windows rebuild and live confirmation, including
the reported pre-bridge behavior, and actual Mac execution remain pending.


### NPC and sniper build validation

The animation-event source change is `81c213c`; manual guest-memory access is
corrected in `fc3a7ce`. The full Windows build initially caught an integration
mistake: MEM32 is defined for generated routines, not this manual runtime
file. The wrapper now uses the same direct guest-memory read convention as
neighboring wrappers. The first build invocation also used PowerShell 7,
whose drawing assembly did not satisfy the icon helper; the supported Windows
PowerShell invocation avoids that shell mismatch. These failures were resolved
before launching the new executable.

The broader suite passes **169 tests, one existing object-rocking xfail** in
475.07 seconds (`logs/native-npc-animation-shared.log`), covering native timing,
float/vehicle joins, network guest/fallback dispatch, CI scope, Mac backlog,
and patch updates/tracking. After the manual access correction, fresh/repeated
patch application and the original NPC firing fixture pass two focused checks
(`logs/native-npc-animation-final-integration.log`). Windows Release and all
build-command tests pass (`logs/native-npc-animation-windows-rebuild.log`).
Executable SHA-256: `912f416b5f6eaeabf545c960c255491907988eae9f031ead4231b24832cbc333`.

The isolated 120-second `native-144-npc-animation-sniper` test is complete and
passed. All crash, Story, native timing, guest presentation, interpolation-off,
presentation-failure, movement, hand-animation/handoff, ammunition and
pause/resume checks pass. It collected 73 entity snapshots and 140 gameplay
samples. Exit code 1 is the runner's intentional bounded termination, not a
crash. Logs: `logs/native-npc-animation-built-smoke.log`; structured evidence:
`private/test-saves/high-refresh/native-144-npc-animation-sniper-validation.json`.
This opening-area replay does not equip or test a live sniper hit and does not
reach the reported bridge enemies/crash; those targeted checks remain open.

From trace second 85, 4,755 complete intervals span 34.5557 seconds: **137.60
CPU updates/s**, p50/p95/p99 **7.0507/9.2407/10.3390 ms**, worst **11.9642 ms**.
Integrated game time/wall time is 1.0000000001. The single truncated final CSV
row from bounded termination was excluded. These are CPU timing samples,
not scanout or a stable-144 certification; the text FPS log also records dips.
No prior performance regression cause is established by this one run.

Personal controls/display hashes match their pre-test values. Normal
native-144 play was reopened as PID 25304, responding, from the same executable.
`logs/play-20261020-141507.err` confirms native timing/interpolation off and
XAudio2 initialization; that is not an audible-output test. An initial launcher
attempt inherited an incompatible PowerShell module path and failed before
game creation (`logs/launch-20261020-141431-445.err`). Its owned launcher was
closed, and a child-process-only Windows module path allowed the normal
`Play-Native.cmd 144` launch; no persistent environment/settings edit was made.
The delayed trails, specific pre-bridge enemy behavior, remaining physics/
weapon/campaign audits, full-game performance and actual Mac validation remain
open. The goal is active.


### NPC aim recovery and tracking

Baseline `87ed587`, pinned US XBE, Windows x64. The aim portion of 0x2B8F90
loads integer delta at 0x2B93DA and skips both recovery and tracking on a
zero-whole interval. For non-actor targets, 0x2B9720 consumes fractional time
when reducing actor-state +0x9FC toward zero; lost intervals slow recovery.
For actor targets, the original interpolation helper 0x126C0 instead consumes
a fixed 5% argument pushed at 0x2B963F. A target-state flag also enables a
random decision at 0x2B9478, with possible additional random draws. Opening
the gate alone would accelerate blending and random decisions.

`tests/native_npc_aim.c` executes original 0x2B93D5..0x2B9792, the zero-result
store at 0x2B9381, and the original clamp/sign/interpolation helpers at
0x124F0/0x123D0/0x126C0. Outgoing joins end with a fixture RET before subsequent
eligibility/firing work. Actor geometry and random outputs are controlled;
the CRT float-to-integer boundary is modeled, and two actor-state services
return false. This tests the original aim branch, not pathfinding, target
selection, firing eligibility, rendered appearance or a live mission.

Before correction (`logs/native-npc-aim-audit.log`), a recovery value of 0.75
ends at 0.0833327 after one game second at original 60, versus 0.4722231 at
native 144, and 0.6805558 at native 144 quarter speed. The actor-target path
reaches the same final value as original 60 but changes only 60 of 144
advancing frames. An isolated gate-only counterfactual changes every frame
but shifts tracking from 0.0250130 to -0.0095290 and increases RNG calls from
60 to 144. That counterfactual remains in the fixture to demonstrate the
coupled behavior; it is not a production mode.

The shared correction has three parts:

- The pause-only load policy at 0x2B93DA accepts every positive live interval.
  Its ECX is only compared before replacement on either path.
- The immediate 5% argument is replaced by `1 - (1 - 0.05)^delta_ticks` at
  0x2B963F only. One-tick values retain their original bits. Other callers of
  the shared interpolation helper and other immediate arguments stay exact.
- On zero-whole live frames, only the chance call at 0x2B9478 selects a helper
  returning a guaranteed nonzero chance outcome without consuming RNG. The
  target's velocity adjustment still executes. Whole-tick chance calls and
  their rare additional draws execute the original RNG. Guest return-stack
  handling remains inside the existing ABI-call boundary.

The additive `native-npc-aim-timing` patch adds generic immediate-argument and
direct-callee policy hooks with identity defaults, plus the small shared
manual RNG-skip helper. Title addresses stay in `pc_native_numeric.h`, not the
lifter. Constant site tests occur outside the function-pointer helper so other
calls retain their original callees and do not acquire an RNG-helper linkage
dependency. The initial generic helper form exposed such a linkage dependency
in an unrelated fixture; the constant-site macro form resolved it. No generated
C was edited by hand. Historical patch fixtures exclude the new patch with its
native timing prerequisites.

`logs/native-npc-aim-fixed.log` and final expanded
`logs/native-npc-aim-final.log` verify normal/quarter-speed stable-input results
within 1e-5 of original 60, including fresh changes on all native 120/144
frames. Rare-event schedules preserve 60 chance decisions, 66 actual RNG draws
and events at game ticks 20/40/60. Their post-event blend phase is allowed to
differ by at most one original blend step (5% of the full [-1,1] range, 0.1);
observed final differences are at most about 0.034. This is not bit-identical
trajectory parity across an instantaneous random change. Zero-time callbacks,
including stale nonzero integer ticks, preserve state. Uneven intervals and
2-/6-tick catch-up match the elapsed-time recovery/blend result while retaining
the original per-callback chance cadence. Positive/negative recovery clamps
exactly at zero, and a missing target changes neither aim nor RNG.

The wider suite passes 484 checks and 87 subtests, with 41 skips and one existing
object-rocking expected failure in 57.36 seconds
(`logs/native-npc-aim-shared.log`). It includes native timing, scalar/vehicle
joins, CI scope, Mac backlog and the upstream recompiler tests. The later rare/
boundary additions pass separately. Fresh/repeated application, historical Mac
and controls upgrades, and patch tracking pass 17 checks in 48.38 seconds
(`logs/native-npc-aim-patch-transitions.log`). Full stack application updates
three runtime source/template files (`logs/native-npc-aim-patches.log`).

The interactive executable remains `fc3a7ce` / PID 25304 and was left running.
A bounded read-only observation produced no Story snapshots during its window
(`private/pipeline/npc-current-build-observation.jsonl`); it did not reproduce
the reported pre-bridge enemy behavior. This source correction has not yet
been rebuilt or verified in that scene. Delayed trails, live enemy movement,
remaining campaign/physics audits and actual Mac execution remain open.

Full generation completes in 73.6 seconds (`logs/native-npc-aim-generate.log`),
and the generation-current check passes. The production translation contains
the intended gate at 0x2B93DA, RNG-callee policy at 0x2B9478 and coefficient
argument policy at 0x2B963F. The shared manual file contains the matching
RNG-skip helper. This verifies integration into generated source, not a linked
or running executable; the interactive process remains unchanged.

### Beam construction and vertex output audit

Source baseline `b45a799`, Windows x64, checksum-verified supported US XBE.
`tests/native_beam_geometry.c` extends the lifetime investigation by executing
the complete original type-32 constructor (0x104F30), geometry update
(0x1053D0), renderer (0x141A0) and manager (0x10A760). The original renderer
writes positions, colors/alpha and texture coordinates into a captured guest
vertex buffer. Allocation, texture/render services, quality selection and
random draws are controlled. A host-generated sine table substitutes the
initialized guest trig service; original scalar constants and manager
descriptor/switch tables come from the verified XBE.

The 84 schedules cover original 60 and native 60/120/144 at normal/quarter
speed, two/four configured strands, both 8/16-segment quality paths, and random
values 0, 0.5 and 0.999. Each live update is followed by a zero-time update:
age, all 0xA00 bytes of geometry and visible-vertex counts remain unchanged.
The original vertex count/format, finite positions, register preservation,
x87 balance and unchanged timing globals are asserted.

Every schedule emits nonzero vertex alpha on its first advancing frame.
The last nonzero-alpha frame varies with the original random strand delay:
about 0.68–1.00 game seconds. Across rates and slow motion it agrees within
one original 60-Hz frame. Manager retirement stays within two update intervals
of its 1.2-game-second lifetime. Original/native 60 final geometry is
byte-identical, and random draws occur only during construction with identical
counts. This supplies evidence against a three-second onset delay inside
these type-32 routines under the tested inputs; it does not identify the
effect in the user's screenshot or justify a production timing change.

The renderer is called directly in this fixture. The surrounding 0x10ADF0
dispatcher also requires scene visibility flags, zero deferred-start time
and the current player's visibility mask before calling it at 0x10BFBA.
Those visibility decisions and GPU submission/resource lifetime remain
outside this test. Other effect types and full firing-to-display behavior
still need reproduction. Actual Mac execution is unverified.

A read-only snapshot of interactive PID 25304 found zero entity/effect list
heads while its timing continued to advance. It did not capture a shot or
trail and does not prove gameplay correctness. The game was left running
on build `fc3a7ce`; no input, memory writes, restart or personal-settings
changes were performed. The separate NPC aim source fix remains unbuilt.

The focused geometry run passes (`logs/native-beam-geometry.log`). With the
additional cross-rate fade and exact native-60 assertions, timing,
recompiler float/vehicle, CI-scope and backlog checks pass 133 tests with the
existing object-rocking expected failure in 57.74 seconds
(`logs/native-beam-geometry-shared.log`). The fixture is registered in the
existing `test_native_timing.py` CI suite. No runtime code changed, so this
audit did not require replacing the running executable.

### Effect visibility and reproduction tracing

Source baseline `84b0157`, Windows x64, supported checksum-verified US XBE.
The beam fixture now also translates 0x10BF5C..0x10BFC9, the entire type-32
list branch in the original render dispatcher. Only its outgoing join into
the next unrelated effect list is replaced with a return, after checking
the original bytes. The branch receives the original current-player bit and
player-index field; all 84 construction/geometry schedules now reach vertex
output through that branch.

Six additional 60/120/144 normal/quarter-speed schedules exercise visibility.
The original manager reads controlled active-player/room-visibility inputs
and builds the effect's visibility flags. A beam hidden for 0.25 game seconds
becomes drawable on the next live fractional update when that room becomes
visible. The prepared flag, explicit-hidden flag, player view mask and room
visibility mask independently suppress rendering as intended. Zero-time
callbacks can refresh visibility without advancing effect age. Explicit
0.3-game-second deferred starts reach vertex output at 0.300000–0.305556
game seconds (within one update); retirement leaves an empty list that the
original branch traverses without drawing. `logs/native-beam-visibility.log`
passes these cases. Upstream camera/room queries remain controlled, so this
does not prove the actual bridge scene's visibility.

Read-only inspection of the host path finds that `host_DrawPrimitiveUP`
copies vertex bytes into a deferred payload when that mode is active;
otherwise it submits to the device immediately. The swap path flushes deferred
operations before capture/presentation. The device appends vertex bytes to a
dynamic ring and calls `rhi_draw`. That inspection does not establish GPU
completion, resource correctness or visible output, and supplies no runtime
fix for the user's delayed-trail report.

`tools/native_effect_trace.py` now provides bounded read-only snapshots for
reproduction. It traverses all 40 effect types with a global 256-node default
limit, rejects invalid pointers, type changes, cycles and unreadable/changed
heads, and marks traversal completeness separately from atomicity. Records
include age/lifetime/delay, position, flags, view mask, and optional type-32
colors/endpoint/strand delays. Its type-32 eligibility result describes only
the original dispatch gate, not a GPU draw. Missing player context yields
unknown eligibility; unreadable optional geometry is explicit. Nonfinite
floats become null rather than invalid JSON numbers.

The existing isolated Windows runner accepts `--trace-effects` and samples
its owned child approximately every 0.5 seconds during Story mode, with a
16 MiB output limit. It retains `<tag>-effects.json` and the tracer hash,
sample count and limit status in validation metadata. Short effects can fall
between samples and recycled addresses cannot establish continuous identity.
The new tests cover visibility metadata, missing/short reads, list mutation,
limits and optional payload failures and are registered in shared CI.
The runner option has passed its help/import check; a complete isolated
gameplay run with this option is still pending. Actual Mac execution remains
unverified. No production runtime code changed in this audit.

The targeted trace/CI checks pass 70 tests. The final timing, effect/entity
trace, CI-scope and backlog suite passes 101 tests with the existing rocking
expected failure in 36.45 seconds (`logs/native-effect-dispatch-shared.log`).
A 30-second read-only observation of interactive PID 25304 collected 299
snapshots / 69,818 bytes with no traversal faults. All were outside Story
(state 101) with empty effect heads, so this validates the read path but does
not reproduce the reported shot/trail. Evidence is retained in
`private/pipeline/effects-dispatch-observation.jsonl` and its `.json` summary.
The interactive process and its executable were left untouched.

### Second projectile updater coupled timing

This subsection records the pre-fix audit at `bf6b5d8`; the source correction
and its validation follow below.

Source baseline `38c3f4c`, Windows x64, supported checksum-verified US XBE.
`tests/native_projectile_coupled.c` translates the complete original
0xF08D0..0xF1A4B updater, kind-35 initializer 0xE78F0..0xE799A,
target sampler 0xE1FA0..0xE2370 and steering 0xE2370..0xE2514.
The original switch table and scalar constants come from the XBE. Collision
queries return no impact; line-of-sight queries accept a controlled target.
Effect allocation/position updates and deterministic RNG are captured
services. This executes their original call sites and arithmetic, but does
not render effects or apply damage.

The initializer allocates four effects through 0x101660 with configuration
IDs 83, 83, 84 and 85, assigns sizes 45/40/37/35, and clears the
target sampling counter. The updater's gate at 0xF08D0 rejects zero-whole
ticks although displacement uses live float delta at 0xF0D45. The
non-homing path also applies the original 1.02 speed multiplier in the
whole-tick loop at 0xF1513/0xF1544. This is acceleration, not damping.
For kind 35, the effect positioning code at 0xF1735..0xF1952 makes nine RNG
draws and updates four effect positions per accepted callback. With homing
flag 0x2000, the target helper decrements integer +0x1AC once per callback,
refreshes line of sight when negative and resets it to 10; steering adds a
fixed per-callback contribution before normalizing the direction.

`logs/native-projectile-coupled-audit.log` executes 16 one-game-second
schedules: original 60, native 60/120/144 at normal/quarter speed, and a
native-144 counterfactual that opens only the gate, for both homing modes.
Original/native 60 shot bytes are identical. Zero-time callbacks between all
live updates preserve projectile/effect bytes and all callback/RNG counts.
Guest registers, stack/x87 balance and timing globals are checked.

| Normal-speed case | X position after one game second | Fresh movement frames | Target checks | RNG draws | Effect position updates |
| --- | ---: | ---: | ---: | ---: | ---: |
| Original 60, unsteered | 456.205841 | 60/60 | 0 | 540 | 240 |
| Current native 144, unsteered | 190.085800 | 60/144 | 0 | 540 | 240 |
| Gate-only native 144, unsteered | 454.700317 | 144/144 | 0 | 1296 | 576 |
| Original 60, homing | 215.687500 | 60/60 | 6 | 540 | 240 |
| Current native 144, homing | 89.950760 | 60/144 | 6 | 540 | 240 |
| Gate-only native 144, homing | 215.079788 | 144/144 | 14 | 1296 | 576 |

The homing Y positions are 104.512825, 43.384315 and 106.175316 respectively.
Unsteered terminal speed remains 787.446533 in these schedules; homing speed
remains 240. Quarter-speed native 144 loses still more motion: X is
47.521450 unsteered and 22.498043 homing over the same game-time duration,
with only 60 fresh movement frames out of 576.

The new strict `ProjectileCoupledTiming` expected failure applies only after
the complete fixture runs successfully. Compilation, missing-service,
constant-table, ABI or fixture failures remain ordinary failures. Acceptance
requires fresh movement and effect positioning each advancing frame,
original RNG and target-check counts, matching terminal speed, and a final
position within one original frame's travel of the original 60 reference.
This is recorded missing behavior, not a passing native-timing claim.

Opening this gate alone is not a valid fix. The correction needs to separate
continuous travel/steering/effect anchoring from target-sampling and random
decisions, preserving initialization and recycled projectile identity.
The nine random trail values must not consume the shared RNG 2.4 times as
often, and the target counter must retain its game-time cadence. Other kinds
sharing the updater, collision/retirement branches, multi-tick catch-up and
distance-based damage falloff also need coverage before broader changes.
No production code changed, no game rebuild/restart was performed, and the
user's screenshot has not been identified as this projectile/effect type.
Actual Mac execution remains unverified.

The timing, CI-scope and backlog suite completes with 96 passes and two
strict expected failures: this newly measured coupling defect and the
existing object-rocking quantization (`logs/native-projectile-coupled-shared.log`,
40.17 seconds). A final focused run also verifies all four original effect
configuration IDs, owner slots and sizes before reproducing the timing
failure; the backlog format check passes. The test is in the existing shared
timing CI suite. These results do not mark the runtime defect resolved.


### Kind-35 projectile motion and trail correction

The shared kind-35 wrapper now enables 0xF08D0 on positive fractional time,
scales its homing contribution at 0xE237A by the live step, and updates all
four original trail anchors each advancing frame. The 1.02 acceleration
loop retains carried whole ticks. Target sampling retains the original
whole-update countdown. A newborn homing shot acquires a usable target on
its first fractional update, then skips the duplicate countdown at the next
whole boundary. The original target-vector calculation still runs each frame.

Nine random trail values are cached separately by their original call sites
and original projectile-pool slot. Fraction-only callbacks reuse these values
at fresh positions; they do not consume the shared RNG. Missing emitters do
not shift the other coordinates. Initialization resets the slot even with
native timing disabled. The pool bound is derived from the original 0xE8EB0
allocation: 0xB8A0 bytes with 0x22C stride, or 85 complete slots. Unknown slots
fall back to original behavior. Nested updater calls save/restore the active
scope; other projectile kinds and native-off retain the original policy.

The complete original-code fixture now passes its former strict expected
failure. At native 144, normal-speed unsteered X is 454.700317 after one game
second versus original 456.205841 (before fix 190.085800). Homing X/Y is
215.639862/104.503189 versus original 215.687500/104.512825. Every advancing
frame produces fresh movement and effect positions. All tested rates retain
540 trail RNG draws and six target checks per game second. Quarter speed
also passes at 60/120/144; native/original 60 normal-speed projectile bytes
remain identical. Initial fractional jitter is neutral until its first
whole-tick random sample, avoiding extra random draws at birth.

Additional checks execute the original collision and expiry branches against
a controlled X=100 plane and controlled allocation/sound/damage services.
Each impact dispatches one damage callback, creates one impact object and
cleans up all four attached effects; expiry retires once without an impact
hit callback. Arrival and expiry times differ from original-step timing by
less than one 60 Hz frame. This verifies dispatch, not actual health loss,
scene geometry or rendered impact effects. Pause checks include zero float
time with a stale whole-tick value of one. Missing-emitter tests verify other
anchors keep their own cached jitter. An uneven .125/.875/2/.25/.75/6-tick
schedule verifies integrated motion, the original acceleration loop and one
random set per accepted whole update. The separate sidecar fixture checks
interleaved and recycled slots, native-off initialization, unknown slots,
pool changes, nested unrelated kinds and uncached impact random calls.

This change is deliberately limited to kind 35. The user's red trails have
not yet been identified as this projectile/effect. Original distance
accumulation at +0xF4, its use by damage falloff, moving/occluded targets,
other updater kinds and full mission behavior remain open. Windows linkage
and an isolated live smoke check are pending; the user's interactive
fc3a7ce executable is left running unchanged. Actual Mac execution is pending;
the implementation and tests use shared source with no OS-specific gameplay
adapter. The wider native-144 goal remains active.


Validation on Windows/MSVC: the final shared timing/CI/backlog/entity/effect
suite passed **103 tests with one expected object-rocking failure**
(`logs/native-projectile-final-shared.log`, 44.94 s). The initial combined
timing and full patch-transition suite passed **52 tests with that same
expected failure** (`logs/native-projectile-coupled-shared.log`, 400.12 s).
Focused projectile integration and scope checks pass separately. The supported
`tsfp.ps1 Generate` command completed successfully, both manual wrappers appear
in the generated declarations, and the generator fingerprint is current
(`logs/native-projectile-generate.log`). The generation/backlog contract checks
passed two tests (`logs/native-projectile-generation-checks.log`). These are source,
translation and fixture results; they do not claim a new linked executable.


### Projectile distance accumulation and damage dispatch

At source baseline `5ece0b9`, the existing saved read-only capture
`private/pipeline/npc-projectile-observation.jsonl` contains 10,148,608 bytes
(SHA-256 `06fd0a428238d8ebfe0e118e360b905ddb408efae14d3c3a635faef7e8e5fe52`,
last modified 2026-10-20 13:25:49 local). Decoding each projectile's kind at
+0, damage-profile index at +8 and distance accumulator at +0xF4 gives:

| Kind | Profile | Updater | Observations |
| --- | --- | --- | ---: |
| 28 | 2 | 0xF4A50 | 47 |
| 31 | 8 | 0xF4A50 | 375 |
| 35 | -1 | 0xF08D0 | 56 |
| 45 | 2 | 0xF4A50 | 82 |
| 46 | 2 | 0xF4A50 | 200 |
| 24 | 9 | 0xF5FF0 | 3 |
| 23 | 0 | 0xEFF90 | 40 |

These are repeated observations, not unique projectiles or proven weapon
identities. The old capture is not evidence about the newly corrected build.
It does identify realistic kind/profile combinations for the original-code
fixtures, including kind 46 that the earlier movement test omitted.

Both moving-projectile fixtures now execute the complete original collision
record path 0xDCEF0, profile helper 0xB9920 and falloff helper 0xB9720. The
controlled scene hit populates the original updater's hit record; the original
caller forwards it to those routines before their final service boundary
0x12A170 is captured. Actual kind flags and profile values are loaded from the
checksum-verified US XBE. Kinds 28/31/45/46 have flag 4 in their descriptor
(0x9C) and bypass the falloff computation, forwarding scalar 1 at every tested
rate. For the captured kind-35 profile -1, the original helper forwards 0 at
every rate without evaluating an exponent. This is one collision-record
scalar, not the projectile's total damage: the original kind-35 impact still
creates its impact object and follows its separate impact callback.

The +0xF4 accumulator remains frame dependent. For example, the controlled
kind-28 impact has approximately 405 at 60 FPS and 202.5 at 120 FPS, while the
forwarded scalar remains exactly 1. Consequently, these captured paths do not
justify changing the accumulator globally or blaming it for missing sniper
health loss. The sniper's separate original damage/health fixture remains
necessary; it is not replaced by these projectile checks.

Fifteen additional caller-contract cases deliberately use kind 35 with
profiles -1, 2, 9, 11 and 12 and accumulator values 0, 16 and 4096. Positive
flat profiles preserve their table multiplier; non-flat profiles exercise the
actual falloff branch. The CRT pow adapter is replaced by host pow with its
original x87 argument/return convention; the original adapter bytes are
verified before translation. For profile 12 the scalar is 1, 0.732094169 and
0.006808810 respectively. Profiles 11/12 with kind 35 are controls, not observed
live combinations. This demonstrates the remaining conditional sensitivity
without inventing a confirmed live damage regression. Mapping all legal
profile/kind combinations, other accumulator consumers, target health and
full collision geometry remain open.

The movement fixture adds kind 46 to its normal/quarter-speed 60/120/144
travel, impact and expiry schedules. Captured profiles verify one collision
record, the expected scalar, one impact callback and one retirement/cleanup.
Focused projectile checks pass (`logs/native-projectile-falloff-focused.log`).
No gameplay code changed in this audit. The same portable original-code
fixtures run through the existing shared timing test entry; actual Mac
execution remains unverified.


The final timing/CI/backlog suite passed **98 tests with one expected
object-rocking failure** (`logs/native-projectile-falloff-shared.log`,
148.32 s while production compilation was active). The focused projectile
run passed three tests (`logs/native-projectile-falloff-focused.log`, 29.55 s),
and the final backlog format check passed. Original-code tests require the
pinned local XBE; these results do not imply an asset-free CI run executed it.

Production Windows compilation of the `5ece0b9` gameplay source completed
successfully using the existing Release project with MSBuild 17.14.40 and
MSVC 14.44.35207:

```
MSBuild.exe build/windows/futureperfect_recomp.vcxproj /t:ClCompile /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /m:2 /v:minimal
```

This compiled the manual runtime wrappers and all generated game units
without linking/replacing the executable. The log
`logs/native-projectile-compile-only.log` contains warning MSB8028 about reused
intermediate project files, but no compilation errors. Full linking and the
supported `tsfp.ps1 Build` regression checks still remain. The interactive
PID 25304 remained responsive and its executable SHA-256 stayed
`912f416b5f6eaeabf545c960c255491907988eae9f031ead4231b24832cbc333`.
Thus the NPC aiming and kind-35 corrections have production compilation
evidence, while the user's running fc3a7ce game does not yet include them.

## Procedural-texture live resource audit (2026-10-21)

The bounded Windows runner now accepts `--watch-surfaces`. It reads the
original five-slot pool (`0x742350`, stride `0xFC`), allocation count at
`0x7421A0`, kind, six per-view stamps, parameter/image-table pointers, four
image handles/indices, and pending disturbance count. Pointer/range checks and
fixed five-slot iteration bound the reads. A second pool read exposes loading
transitions. These are asynchronous process-memory observations: stamps can
change without executing a GPU pass, and a read can cross an update. They do
not measure image freshness, GPU completion or presentation cadence.

`tests/fixtures/procedural-surface-commands.json` preserves the two replays.
Use the named replay as JSONL commands with `stability_transitions_game.py
--watch-surfaces --seconds 120 --minimized`; original 60 omits `--native-fps`,
and the native comparison adds `--native-fps 144`. Both routes include aiming,
movement and pause/resume. Commands begin with a 24-second startup wait.

| Scene and path | In-game observations | Allocated resources | Kind |
| --- | ---: | ---: | --- |
| Venice, original 60 | 100 | 0 | none |
| Venice, native 144 | 100 | 0 | none |
| Scotland shoreline, original 60 | 52 | 1 | 1 |
| Scotland shoreline, native 144 | 52 | 1 | 1 |

These counts include periodic/label observations and held-input samples, not
independent rendered frames. Each selected observation has the same pool
pointer before/after reading. Venice scene 46 has an initialized but empty
pool throughout the selected roughly 13-second gameplay windows. Scotland
scene 4 has resource kind 1 throughout roughly 12-second windows, including
pause/resume. Its parameter address is `0x4E8DB8`, image table `0x74FA20`,
image handles `[1483,1482,1481,1480]`, and indices `[0,2,1,3]`. The native
per-view cache stamp tracks native frame count (occasionally one behind due
to asynchronous reading); the original path uses its console-vblank stamp.
Neither observation establishes the actual nested-pass count. The pending
local disturbance count is zero in these samples, which does not exclude
random disturbances inserted by the separate noise pass.

Reviewed `surface-scotland-60/start.png` shows the starting shoreline/pier;
`surface-scotland-144/turn-right.png` shows the sea. The Venice spawn capture
shows the actual deathmatch interior. The empty Venice pool therefore cannot
validate this procedural effect. Scotland provides a concrete active resource
for subsequent render-target capture and fractional-time integration work.

The pinned US disassembly identifies the nested pipeline IDs and callbacks:
`0x12` uses `0x2BA80`, `0x13` uses `0x2BE30`, `0x14` uses `0x2BF10`, and
`0x16` uses `0x2C320` (table at `0x476270`, stride 16). `0x1952D0` makes two
RNG calls and draws a randomly offset texture; `0x195110` executes two
feedback passes using resource-parameter offsets `+0x10..+0x1C`. For kind 1,
the authored values are approximately `0.17, 0.105, 0.65, 0.98`. Its two
pixel-shader definitions at `0x4BBE20`/`0x4BBF10` contain seven/eight combiner
stages; the output pass at `0x4BC318` contains six. These observations identify
the coupled image algorithm, not a verified differential equation. Increasing
the dispatch frequency alone would also increase random disturbance draws
and change feedback evolution. A fractional implementation must preserve
original-60 evolution and event cadence while producing fresh intermediate
simulation images; scaling all coefficients blindly is not validated.

All four sessions exited cleanly, signed profiles and the canonical fixture
were preserved, and disposable disks/BMPs were recycled. Logs and selected
captures remain under `logs/surface-{venice,scotland}-{60,144}.log` and matching
`private/test-saves` folders. Machine-readable evidence is
`private/pipeline/procedural-surface-live-summary.json`. Tested Windows x64
executable SHA-256 is
`cd0d7679a40d12f86fda79c74fa2e7287ca48a6fa75c9478ef02ebe56d8b83d8`, from
`764ea2b`; only the read-only test runner changed. Python 3.12.14; per-run
provenance records runner/executable hashes and QPC clock metadata. Captured,
minimized sessions are not performance measurements. The existing lifted
surface-cache test still passes (1 passed, 60 deselected), and the runner
passes Python compilation/CLI checks. No production timing or shader change
was made, so no rebuild was needed. Fractional procedural GPU simulation,
actual per-frame image evidence, other resource kinds/scenes and native Mac
validation remain open.

## Procedural GPU image freshness (2026-10-21)

`pc_native_surface_trace.h` adds an opt-in, bounded diagnostic around original
`sub_001956A0` and its final image pass `sub_00195280`. The final-pass wrapper
retains the current render-target surface before the dispatcher restores its
previous target. The dispatch wrapper reads that actual surface after every
eligible dispatch, including fractional frames where no simulation pass ran.
It uses shared `GetRenderTarget`, surface `GetDesc`, `LockRect(READONLY)` and
`UnlockRect` interfaces. CPU cache stamps are not used as image evidence.

`TSFP_SURFACE_TRACE=<csv>` writes a CSV and tightly packed 128x128 four-byte
pixel samples to `<csv>.rgba`, capped at 360 images (22.5 MiB). It selects
Scotland scene 4, match state 7, level tick >=300 and resource kind 1, excludes
view aliases in the same frame, checks dimensions/format/pitch, and releases
its COM reference on resource/scene changes, failure and completion. The
trace is disabled by default; both wrappers then directly execute the original
routine. GPU synchronization changes the workload, so this is deliberately
not a performance or physical-display measurement. The original algorithm,
whole-tick gate, randomness and coefficients are unchanged.

Use `tests/stability_transitions_game.py --trace-surface-images --watch-surfaces`
with the Scotland replay in `tests/fixtures/procedural-surface-commands.json`.
Original 60 omits `--native-fps`; native 144 adds that option. The new patch is
ordered after the existing native wrapper series by `apply-patches.py`, and
regeneration creates both `_gen` bodies. No generated game C is edited.
`tools/procedural_surface_stats.py <csv>` validates every pixel hash, exact byte
length, finite deltas, consecutive frame IDs, unchanged resource/format/path,
and scene identity before comparing complete images. Executed-but-unchanged
images remain counted; there is no filtering of stationary tails.

| Image comparison | Original 60 | Native 144 |
| --- | ---: | ---: |
| Captured images | 360 | 360 |
| Successive comparisons | 359 | 359 |
| Changed complete images | 359 | 154 |
| Identical complete images | 0 | 205 |
| Changed after executed pass | 359 | 154 |
| Unchanged after skipped pass | 0 | 205 |
| Longest consecutive unchanged updates | 0 | 2 |
| Sum of observed game-tick deltas after first sample | 371 | 164.504526177 |

This confirms that the active procedural image retains its previous contents
on fractional native updates. Every executed pass in these windows changes
pixels, and every skipped pass retains them exactly. This is a remaining
native-fluidity gap, not an FPS-counter problem. Different capture durations,
random states and synchronous-readback overhead mean these samples do not
establish wave-speed/appearance parity or sustained rendering performance.
The next production change needs a verified fractional integration of the
coupled feedback passes and preserved disturbance cadence, followed by actual
image comparison against original 60. Merely increasing discrete pass frequency
would change evolution; the native result here is not accepted as complete.

The controlled COM fixture `tests/native_surface_trace.c` verifies disabled
pass-through, unchanged guest stack, read-only flags, pitched-row handling,
first-image availability, retained-image reads on skipped updates, view-alias
deduplication, reference release on scene changes, GPU-read failure and the
360-image bound. Its surfaces are doubles, not GPU proof. Eight portable
statistics checks reject truncated/hash-invalid/gapped/resource-changing/
non-finite/wrong-scene evidence and count executed-but-frozen images. These
checks join the existing shared timing and runtime CI selections. Focused
surface/statistics/CI checks pass 75 tests; Windows Build and its built-in
checks pass (`logs/native-surface-trace-build-fixed.log`). The first build
caught reliance on out-of-scope timing macros; dedicated surface accessors
correct it without changing the timing wrapper.

Patch tracking/update/backlog checks initially passed 34 and failed two
historical setup cases that accidentally included the new wrapper before its
prerequisites. Their historical exclusion lists now omit the diagnostic;
both corrected migrations pass, as does the separately rerun native-checkpoint
migration. The complete active stack also applies/builds successfully. Logs:
`native-surface-trace-patch-checks.log`, `native-surface-trace-migration-fixed.log`
and `native-surface-trace-checkpoint.log`.

Both real GPU sessions exit cleanly with signed profiles and the canonical
fixture preserved; disposable virtual disks and redundant BMPs are recycled.
Evidence: `private/test-saves/surface-image-{60,144}/run0-surface.csv` and
`.rgba`, selected scene captures and first-image previews; corresponding
`logs/surface-image-{60,144}.log`; aggregate
`private/pipeline/procedural-surface-image-summary.json`. Windows x64 executable
SHA-256 `03c279b03b26f257d8401cda2305b6e1982137a0ac2e91bbb3dab066a58e5dc9`,
base `7e330dc` plus the diagnostic wrappers in this change. Python 3.12.14;
per-run provenance records executable/runner hashes and QPC metadata. Raw
four-byte readback channels are compared as bytes; the RGB-only diagnostic
previews do not establish cross-backend channel semantics. Shared RHI and
legacy native-GL surface implementations supply readback/readonly unlock;
actual Mac capture and full-game texture/effect coverage remain unverified.

## Rejected fractional surface candidate (2026-10-21)

The `d404ead` image diagnostic established a genuine remaining texture hold.
This experiment adds `pc_native_surface_guest.h` and a subsequent manual-wrapper
patch. It is explicitly gated by `TSFP_SURFACE_FRACTIONAL_TEST=1`, absent from
player settings and disabled by default. The bounded runner selects it only
with `--fractional-surfaces` and explicitly clears it otherwise. **The candidate
fails image acceptance and must not become the normal high-refresh path.**

For positive native intervals below one authored tick at rates above 60, the
candidate executes the original dispatcher each frame. It temporarily scales
acceleration and velocity coefficients by delta, exponentiates damping, and
restores the original parameter table and whole-tick gate afterward. Local
queued disturbances and random-noise draws keep their original whole-tick
cadence. The original smoothing pass retains radius one on whole events and
uses radius zero between them. Original 60/native 60, pause and full-tick
catch-up retain the original policy. This is a trial integrator, not a proven
fractional equivalent of the original quantized shader pipeline.

`tests/native_surface_fractional.c` executes the pinned original dispatcher
and two-pass coefficient-upload routine. Graphics spies verify call counts,
coefficient scope, stack/register/FPU state, parameter/index restoration,
pause and catch-up. Eight-second schedules yield 480/480/960/1152 steps for
original 60/native 60/120/144 and 960 combined disturbance calls in every case.
A second process with the experiment disabled keeps 480 steps and zero extra
copy passes at all rates. These fixtures deliberately do not claim pixel
correctness. The original US XBE hash is verified before lifting/execution.

Three isolated Scotland replays use Windows x64, Python 3.12.14, the shared
HLE/RHI frontend and D3D11 backend. Each produces 360 complete final-texture
readbacks. The build is based on `d404ead` plus the experimental wrappers;
executable SHA-256:
`1155d591ca12382eb8c1847effdbbdf2844b315a292fd2ce801644988b2d2ff4`.

| Measurement | Original 60, flag enabled | Candidate 120 | Candidate 144 |
| --- | ---: | ---: | ---: |
| Changed image pairs / 359 | 359 | 359 | 0 |
| Unchanged image pairs / 359 | 0 | 0 | 359 |
| Executed simulation but unchanged | 0 | 0 | 359 |
| Raw byte channel 1 standard deviation | 24.8907 | 16.4159 | 0 |
| Raw byte channel 2 standard deviation | 24.0195 | 15.9464 | 0 |
| Sum of game deltas after first sample (ticks) | 374 | 195.001446 | 164.939040 |

The 144 output is exactly `(255,128,128,255)` for every pixel in every sampled
image. The 120 result has 34.05%/33.61% less variation in its two varying byte
channels than the same-build original reference. Previous original-60 and
unmodified-native-144 captures both have standard deviations near 24–25,
so this is materially different from the previous observed range. These are
raw channel statistics, not a calibrated measurement of water height. The
reviewed three-image contact sheet also shows a flat 144 texture. Random states
and capture durations differ; cross-run exact pixel equality is not asserted.

The statistics tool now computes per-channel integer histograms and derives
mean/standard deviation from them. This avoids substantial float32 summation
error over millions of bytes. Its new fixture covers both changing uniform
frames and the actual flat four-channel pattern, alongside existing hashes,
frame continuity and executed-but-frozen detection. Private exploratory
lag correlations use actual cumulative game ticks; undefined correlations for
constant channels are null, not fabricated zeros. Those exploratory correlations
are not a speed-parity acceptance test.

Windows Build and its built-in checks pass. The first build caught a missing
`g_esi` declaration at the new include; the corrected build succeeds. All 71
then-current timing/statistics checks pass; after adding disabled-mode and
histogram checks, 12 focused surface checks pass. Three historical patch-stack
migration cases pass with the new dependency excluded from their older setups.
Their CPU success does not override the failed GPU acceptance. The experiment
remains separate from normal play, and no personal configuration enables it.

Evidence remains local under `private/test-saves/surface-fractional-{60,120,144}`,
including CSV/raw images, provenance, selected captures and scenario reports;
`logs/surface-fractional-{60,120,144}.log`;
`logs/native-surface-fractional-build-fixed.log`;
`logs/native-surface-fractional-timing.log`;
`private/pipeline/procedural-surface-fractional-summary.json`; and
`private/pipeline/surface-fractional-contact.png`. Each session exits normally,
with signed profiles and canonical fixture unchanged and disposable disks/BMPs
recycled. Synchronous readback/minimization makes these unsuitable for pacing
claims. Full-game effects and native Mac execution remain open.

Next work must identify and remove the loss of fractional feedback, then
compare impulse response and steady-state behavior against the original
shader passes before enabling a replacement. Original `sub_003D9520` quantizes
uploaded constants to eight bits, and the feedback targets are eight-bit
textures; simply scaling float coefficients does not preserve sub-byte state.
These are concrete precision hazards, but the exact share of the observed
failure due to target rounding, centering bias, filtering and integration is
not yet isolated. RHI already maps half-float render targets; the selected native legacy GL adapter (`src/native/renderer_gl.c`) allocates
render targets as `GL_RGBA8` regardless of format. Its CPU pitch already uses
the format's byte size, but float render storage/readback still needs adapter
work before such a format change could be portable. Preserving higher-precision state,
correct zero-time identity and authored disturbance behavior are necessary
investigation targets. Increasing brightness or random impulses to disguise
the flat output is not an accepted fix. Capture-free pacing and cross-platform
validation follow a successful image/behavior comparison.

A subsequent disabled-mode control attempt (`surface-control-144`) exits cleanly
but remains at the profile-name screen and captures no procedural images. Its
runner's successful bounded shutdown is **not** scene/image acceptance. The
snapshot and log are retained as rejected test evidence; the source-level
original-dispatch checks remain valid independently of that navigation failure.

The completed retry (`surface-control-retry-144`) reaches Scotland with the same
executable and the experiment explicitly disabled. Of 359 image comparisons,
157 change on executed passes and 202 repeat on skipped passes; no executed
pass is frozen. Channel standard deviations are 24.6491 and 23.6327, consistent
with the existing path's observed range. This verifies that the candidate's
flat output is confined to its opt-in path in this scene, while the known
60-tick texture hold still needs fixing. It does not establish full-game parity.
The 360-image control, its hashes/provenance and
`private/pipeline/surface-control-retry-summary.json` are retained; the session
exits cleanly with profiles/fixture preserved and disks recycled. Patch tracking
and backlog checks additionally pass all 15 cases. No pacing claim is made.

## Half-float procedural feedback experiment (2026-10-21)

The preceding candidate lost all visible variation at native 144 with 8-bit
feedback targets. A separately selectable `TSFP_SURFACE_FLOAT_TEST=1` experiment
now allocates only the three feedback intermediates per resource as host
`A16B16G16R16F`. The fourth, final effect texture remains in its original byte
format. Guest texture headers, allocation sizes, handles and bytes are unchanged.
The constructor wrapper registers at most 15 exact target addresses, resets
old hints at a new pool and leaves normal rendering untouched when disabled.
The host texture cache records its actual render format and recreates a target
if its precision hint changes, instead of silently returning an old-format
cache entry. This experimental recreation does not preserve the old target's
contents; it is used before original initialization clears the new resource,
not as an accepted live frame-rate switching mechanism.

`pc_surface_precision.h` owns the bounded mapping policy; the original US
constructor wrapper is in `pc_surface_precision_guest.h`. The complete patch
stack adds the hook and host-format plumbing in `hle_d3d8_texture.c`. Both
Windows and the native Mac common RHI use that same implementation. Legacy GL
refuses this diagnostic request because its render-target allocation is fixed
at `GL_RGBA8`; the runner rejects the unsupported marker. This corrects the
previous audit's reference to the unused generic POSIX GL constructor: the
actual native adapter already computes CPU pitch from the guest format. Normal
legacy-GL behavior is preserved; its float storage/sampling/readback and actual
Mac validation remain open.

Four bounded Scotland GPU runs use executable SHA-256
`b5fa2323c838c632d2a19af00733c96b54586b4b49d9787df9498a16da99bd31`,
base `9fe6037` plus this precision experiment. They select `--float-surfaces`;
only the fractional rows also select `--fractional-surfaces`. The original
60 row omits native timing entirely. Per-run provenance records those flags,
executable/runner hashes, Python 3.12.14 and QPC. Renderer logs confirm that
three targets per intro/Scotland resource use host format 0x34 while the final
captured image is still a 128x128 byte texture. Renderer screenshots are disabled
for these runs, but the synchronous texture readbacks still disqualify them as
pacing measurements.

| Path | Changed pairs / 359 | Repeated pairs | Channel 1 std | Channel 2 std |
| --- | ---: | ---: | ---: | ---: |
| Prior original 60, byte targets | 359 | 0 | 24.8907 | 24.0195 |
| Prior fractional 144, byte targets | 0 | 359 | 0 | 0 |
| Original 60, float targets | 359 | 0 | 19.9405 | 19.1807 |
| Native 144 whole-tick feedback, float targets | 149 | 210 | 19.7471 | 19.2683 |
| Native 120 fractional feedback, float targets | 359 | 0 | 19.1387 | 18.4523 |
| Native 144 fractional feedback, float targets | 359 | 0 | 18.8516 | 18.1925 |

Changing intermediate precision eliminates the frozen final image at 144;
all positive sampled updates also change pixels at 120. The whole-tick float
control still retains images between authored ticks, which distinguishes
storage precision from simulation cadence. Relative to float-target original
60, the fractional 144 variation is about 5.5%/5.2% lower, and 120 about
4.0%/3.8% lower. Exploratory six-game-tick correlations are approximately
0.346/0.324 at float 60 versus 0.372/0.357 at fractional float 144; these use
nearest observed samples and do not prove identical wave speed. Different
random states, durations and variable deltas prevent deterministic trajectory
claims. Float storage itself changes the original byte-target distribution by
about 20%, so neither original appearance nor mechanics parity is declared.
The reviewed contact sheet shows structured variation in all float cases,
including 144; these raw-channel previews are not rendered-world acceptance.

A new asset-free check in the shared RHI fixture renders two distinct,
sub-byte values into both byte and half-float targets, samples them back into
a float target and checks the half-float readback bits. The values collapse to
byte 128 in the byte target and remain distinct (0x3804/0x3805) in half float.
Windows WARP passes this real renderer contract; the same source is consumed
by the native Vulkan runner, whose execution is unverified here. This proves
the storage capability, not the original water equations. The first fixture
compile hit Windows' `small` macro; renaming that local viewport fixes it.

The portable constructor/mapping fixture checks explicit opt-in and disabled
pass-through, all five resources, exclusion of each final texture, untouched
guest headers, pool reset, slot/address bounds, unsupported capability and
guest stack preservation. Windows Build and its built-in checks pass; 89
native-timing/statistics/patch-tracking/backlog checks, three historical patch
migrations and the common RHI WARP fixture pass. The affected checks already
run through the existing shared runtime workflow selections.

The three older byte captures named `surface-fractional-60`,
`surface-fractional-144` and `surface-control-retry-144` now retain their raw
images as lossless `.rgba.gz` files, verified against the original bytes and
hashes before removing only those exact uncompressed duplicates. The statistics
tool reads either representation with bounds on both compressed input and
expanded output; a regression checks exact equality and rejects overexpansion.
This keeps the retained texture-test evidence within the 256 MiB temporary
budget. No original assets, personal settings or saves are changed.

Evidence: `private/test-saves/surface-float-{60-original,144-original,120-fractional,144-fractional}`,
matching logs, `private/pipeline/procedural-surface-precision-summary.json`,
`surface-float-120-summary.json`, and `surface-precision-contact.png`;
`logs/native-surface-precision-{build,checks,migrations,rhi}.log`. All four
sessions exit cleanly with profiles/fixture preserved and disposable disks
recycled. Both experimental switches remain off by default. Remaining work
before acceptance includes rendered-water review, live 60/120/144 switching
without state loss, original-60 appearance behavior, native Mac/legacy GL,
other procedural kinds and the wider full-game goal. Storage precision is a
confirmed contributor to the previous freeze, not proof that all integration,
constant quantization, centering and filtering issues are solved.


### Capture-free Scotland pacing with precision experiment

Three sequential bounded Windows x64/D3D11 sessions use the same executable
hash above and `procedural-surface-pacing-commands.json`, windowed at 1280x720.
Original 60 omits native timing; native 144 control disables both experiments;
native 144 candidate enables both. All disable renderer captures and texture
readbacks, remain unminimized, and record successful Present API intervals.
Each uses a stationary 20-second shoreline interval, trimmed by two seconds
at each end for measurement. Runner markers and Present CSV have different
startup origins; the trim accommodates that uncertainty rather than claiming
millisecond-precise alignment. All endpoint positions and camera angles match.

| Path | Guest presents | Guest FPS | Median ms | p95 ms | p99 ms | Worst ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Original 60 control | 966 | 59.999 | 16.675 | 17.466 | 17.821 | 18.277 |
| Native 144 control | 2318 | 143.993 | 7.052 | 7.922 | 8.282 | 9.605 |
| Native 144 fractional + float | 2319 | 144.006 | 7.029 | 7.926 | 8.639 | 9.615 |

No interpolated presents or intervals over 33.33 ms occur in those selected
windows. CPU update means are 143.998 and 144.002 for the native control and
candidate; their elapsed-game/elapsed-wall ratios both round to 1.000000.
Endpoint level-clock ratios are 1.000779, 1.000779 and 0.999901 respectively,
including asynchronous read/marker uncertainty. Health is 35 at all gameplay
observations. The runner blocks during its 20-second wait, so there are zero
interior world-state observations; endpoint snapshots alone do not prove
continuous pose stability. The native per-update trace independently remains
in unpaused scene 4 with positive game delta throughout the selected interval.
The original-60 case has no corresponding per-update trace.

The first summary attempt rejected the absent interior observations instead
of misreporting them as continuous coverage; the corrected private summary
records their count explicitly. All three sessions exit cleanly, preserve
profile signatures/the fixture and recycle disposable disks. Evidence is in
`private/test-saves/surface-pacing-{60-control,144-control,144-candidate}`
(provenance, reports, raw timing CSVs and `pacing-summary.json`), with
`logs/surface-pacing-summary.log` and `private/pipeline/summarize-surface-pacing.py`.
The candidate sustains the target in this scene, but this single sequential
comparison does not isolate its rendering cost from system variance, prove
monitor scanout, establish full-game pacing or accept the changed appearance.


### Rendered shoreline inspection

Two additional capture-enabled sessions (`surface-visual-60-control` and
`surface-visual-144-candidate`) replay the existing Scotland commands with the
same executable. Original 60 has both switches disabled; native 144 enables
both. Reviewed `turn-right.png` renderer outputs show the same pier/camera
composition with textured water, the horizon and reflected cloud lighting
present in both. No obvious missing water, black target or gross reflection
breakage is visible in these stills. This is a single static view at 1280x720,
not a motion review or exact appearance-equivalence test. World/RNG phase is
not synchronized; the earlier raw-image distribution difference still prevents
acceptance. These capture-enabled runs are not used for pacing results.

Both reports reach scene 4, retain health 35, traverse forward, enter pause and
resume. Camera position/look match at the inspected label. Their profile/fixture
checks pass and all disposable partitions are recycled. Capture provenance,
report/PNG files and logs are retained locally; the scene must still be reviewed
in motion, at other viewpoints and across live preset changes before release.

The four new float image captures are also losslessly gzip-compressed after
byte-for-byte decompression checks, with raw SHA-256 receipts in
`private/pipeline/surface-precision-compression.json`. This recovers space for
the two scene captures while preserving all image evidence.


## Procedural shader arithmetic audit (2026-10-21)

The half-float experiment removes storage starvation but still changes wave
statistics. A new Windows WARP diagnostic reads the original second feedback
program at `0x4BBF10` and smoothing program at `0x4BBC40` from the SHA-256-pinned
US XBE. It compiles their unchanged HLSL through the current production combiner
parser/generator and common RHI. No shader bytes or generated HLSL are checked
in. The fixture uses known constant texels in float32 textures/targets to isolate
arithmetic from filtering footprints, 8-bit/half-float storage and random input.
It is an original-program GPU test, not a full simulation or scene replay.

For the second feedback pass, the unsaturated persistent-channel equations
measured across the test states are:

```
v = G - 0.5 + a * (R - 0.5 + neighborB - B)
G_next = v + 128/255
B_next = d * (B + b * v)
```

Here `a`, `b`, `d` are the quantized uploaded acceleration, displacement and
damping coefficients. The actual dynamic vectors and per-stage mapping come
from the original definition and the established `0x195110` upload. In this
program, damping multiplies the blue displacement output; it does not multiply
the green velocity output. The new numerical fixture corrects the earlier
informal interpretation of damping the velocity. Its two sampled textures
contain the same known texel, so the neighbor difference is zero in these
specific executions; nonuniform spatial behavior is not tested.

Even with `a=b=0` and `d=1`, the second pass adds `128/255 - 0.5`, approximately
0.001960784, to green on every execution. A neutral input of 0.5 becomes
0.501960814 on WARP. The smaller-step candidate executes this operation more
often while leaving that offset unchanged. This is an additional arithmetic
rate dependency, independent of intermediate storage quantization. The runtime
already skips zero-game-time dispatch, so this limiting-kernel test does not
claim that paused gameplay runs or drifts.

A diagnostic-only float constant `0.5 + (128/255 - 0.5) * delta` removes this
offset at zero and scales it with elapsed ticks. Tests cover three input states
at delta 0, 0.0001, 60/144, 60/120 and 1. The corrected zero-step green/blue
outputs match their inputs within 1e-6; complete float outputs at delta 1 are
bit-identical between original/corrected constants. This is not enabled in the
game. Fractional coefficient quantization, clipping, coupling, spatial filtering
and intermediate rounding still prevent a full-trajectory equivalence claim.

The smoothing shader is also not an identity at zero sampling radius. With
four identical texels `x` and the authored vertex color `127/255`, its tested
output is:

```
min(1, x - 0.5 + 128/255) * (254/255)
```

WARP outputs 0.201168805 from 0.2, 0.499992341 from 0.5, 0.798815906 from 0.8,
and 0.996078432 from 1. The initial diagnostic formula omitted the intermediate
clamp and failed at input 1; retaining that original clamp makes all five inputs
pass across every pixel/channel. This detects real loss of variation when the
supposed copy runs at extra fractional updates with float intermediates. Byte
rounding hides most of this change, which explains why treating the byte pass
as an exact copy was insufficient after changing storage. It does not by itself
quantify the contribution to the preceding complete-scene differences.

Next implementation work must remove the extra smoothing/copy operation while
preserving feedback-buffer ownership, and deliver the time-scaled centering
constant through ordered renderer commands with correct shader/draw scope.
Directly reading a mutable guest delta from a delayed host draw would be unsafe:
`hle_d3d8_record.c` queues draws/state and supports replay. Whole-tick/original-60
results, pause/resume, live presets and common renderer behavior remain required.
Do not compensate for these errors by increasing random disturbances or gain.

Validation: `python -m pytest tests/test_surface_shader_feedback.py -q -s`
passes both shader cases on Windows WARP (`logs/surface-shader-feedback.log`).
The initial test launch lacked the runtime import path; adding that explicit
path fixes independent invocation. The workflow includes this test, which skips
without Windows or the pinned local XBE; a hosted skip is not GPU validation.
This checkpoint changes diagnostics/documentation only. Existing executable
and opt-in defaults remain as in `66eec0f`; no interactive game was closed.


## Fractional feedback buffer reuse (2026-10-21)

The opt-in fractional path now bypasses `0x1950B0` smoothing on updates with
no whole game tick. The original dispatcher first swaps the current/destination
indices and normally swaps them again after smoothing. The wrapper cancels that
second swap when smoothing is skipped, allowing feedback to read the latest
state directly and write the other buffer. It no longer runs the nonidentity
zero-radius copy from the preceding arithmetic audit. Whole-tick smoothing,
discrete disturbances and original-60/disabled dispatch are unchanged. The
fourth final image remains distinct from the feedback intermediates.

A subsequent reproducible patch replaces the old `0x2BA80` radius wrapper with
the `0x1950B0` wrapper. The original smoothing body, dispatcher and two feedback
passes are lifted together in the updated CPU integration fixture. Draw spies
track independent content generations for the four image IDs: every smoothing
and feedback read must see the latest state, each destination must differ from
its sources, and final conversion must read the newly completed feedback.
The old destination begins with a stale sentinel, so silently reusing it fails.
Both experiment-on and disabled runs check original/native 60, 120 and 144,
pause, catch-up and rate changes without resetting buffer ownership. These
spies prove lineage/ABI/cadence rather than GPU pixel values.

Across eight-second synthetic schedules, authored smoothing runs 480 times and
disturbance callbacks 960 times at every rate. The enabled fractional feedback
runs 480/960/1152 times at 60/120/144; disabled runs execute 480 at all rates.
The full patch stack applies, Windows Release Build and built-in checks pass,
91 focused timing/shader/statistics/patch/backlog cases pass, and all three
historical migration cases pass. Build executable SHA-256 is
`4106bee3f0a5427ad8429bc08ab1f88e0b629d8502adc1ab9c6035785bc83afb`,
base `63243fc` plus this change. Evidence: `logs/surface-copy-{build,checks,migrations,patches}.log`.
This still uses both opt-in experiments; centering-offset transport, live
precision transitions, actual Mac execution and full-game acceptance remain open.


Two bounded Scotland GPU runs on that executable keep both experiment switches
on, capture 360 final byte-format images at 120 and 144, and disable renderer
screenshots. These minimized, synchronous-readback runs cannot measure pacing.
The image tool verifies every raw hash and consecutive update ID. Both sessions
exit cleanly, retain profile signatures/the fixture, and recycle disposable
partitions. Their raw captures are gzip-compressed only after byte-for-byte
verification; older `surface-fractional-120`, `surface-image-60` and
`surface-image-144` evidence was likewise compressed with raw SHA receipts in
`private/pipeline/surface-copy-space.json`, recovering 48.38 MiB before testing.

| Path | Changed pairs / 359 | Channel 1 std | Channel 2 std |
| --- | ---: | ---: | ---: |
| Prior float 120 with zero-radius copies | 359 | 19.1387 | 18.4523 |
| Direct-buffer 120 | 359 | 19.3332 | 18.3723 |
| Prior float 144 with zero-radius copies | 359 | 18.8516 | 18.1925 |
| Direct-buffer 144 | 359 | 12.8683 | 12.3266 |
| Float-target original 60 reference | 359 | 19.9405 | 19.1807 |

The 120 distribution remains close to its prior experiment. The 144 result is
**not accepted**: its channel variation is substantially lower, with channel-1
std declining from 14.3242 in the first 60 images to 11.0676 in the last 60.
The inspected raw-channel contact sheet also shows weaker structure at 144.
Freshness and correct CPU buffer lineage do not establish healthy wave dynamics.
The uncorrected per-pass velocity offset, quantized constants and coupled update
remain possible contributors; these runs do not isolate which accounts for the
new decline. Original noise code selects the current index dynamically, and
the common renderer restores nonaliasing shader views around target changes;
those source checks do not substitute for intermediate GPU-state observation.

Evidence: `private/test-saves/surface-copy-{120,144}-fractional`,
`private/pipeline/procedural-surface-copy-summary.json`,
`surface-copy-contact.png`, and `logs/surface-copy-images.log`. The fixture now
prints enabled and disabled schedules (`logs/surface-copy-lineage.log`). No
capture-free pacing or new in-world appearance acceptance is claimed for this
change. It remains confined to the disabled-by-default experiment. Next work
must combine correct state reuse with the shader centering correction and
observe intermediate state instead of tuning visible gain to hide the decline.


## Ordered fractional water centering (2026-10-21)

The optional correction now travels with the draw commands as a copied float4,
through both the deferred queue and the existing replay recorder. It overrides
only bank 1, stage 6 of the validated second feedback shader (`0x4BBF10`) during
a positive fractional surface step. Changing shaders or leaving that scope
queues a clear. Packed guest render states and the shader cache key remain
unchanged. `pc_surface_bias.h` defines the shared centering formula; the common
RHI and legacy GL adapters use the same bounded float-constant state model.
Device initialization resets that state. This avoids reading mutable guest time
from a later queued GPU draw.

Capture format 9 records overrides, including all enabled/disabled slots in the
frame-start snapshot; versions 5 through 8 remain readable. The replay reader
rejects short payloads, bad indices/enable values and nonfinite constants.
A subsequent patch reproduces all upstream changes, including the versioned
capture fixture. New tests extract the actual production queue/set/clear,
shader-scope and replay functions. They exercise copied payload ownership,
ordered clears, recorded replay, lossless capture round trips and malformed
chunks. The original translated shader GPU test now gets its corrected value
through the shared override model, keeping zero-step identity and bit-identical
one-tick output for the tested states. The original guest dispatcher fixture
also runs explicitly with the centering flag enabled and checks scope cleanup.

The correction requires all three explicitly enabled switches:
`TSFP_SURFACE_BIAS_TEST=1`, `TSFP_SURFACE_FRACTIONAL_TEST=1` and
`TSFP_SURFACE_FLOAT_TEST=1`. The bounded runner's `--bias-surfaces` rejects
missing dependencies and records the choice in provenance; absent switches
explicitly disable it. No experiment is enabled in ordinary gameplay.

Windows x64 Release Build and built-in checks pass under Windows PowerShell 5.
The complete selected timing, original-shader, capture, ordered-constant and
patch-migration suite passes 90 cases in 440.52 seconds; backlog integrity adds
one passing check. This includes all six historical stack/line-ending migration
combinations, fresh/repeated application and conflict-preservation checks.
Build uses MSVC 14.44.35207 / SDK 10.0.26100.0. Executable SHA-256:
`0764c79610871abdab6875f05683cb2a48348d9fd4cf3c337acd3e1b28094fcd`,
base `62eb824` plus this change. The first focused run found one old capture
assertion still expecting version 8; updating the fixture in the same patch
corrected that mismatch. Build output retains existing unreachable-code
warnings in the device frontend. Native Mac compilation/execution is unavailable;
legacy GL still refuses the half-float target experiment, so its constant support
alone does not make the water experiment available there.

Two bounded minimized Scotland runs capture 360 final byte-format images each.
All 359 consecutive pairs change at both 120 and 144. At 144, corrected channel-1
variation is stable across the sample (first 60 std 19.1674, last 60 19.1134),
in contrast to the preceding uncorrected run (14.3242 to 11.0676). Corrected
channel-2 std is 17.9335 to 17.9604. These runs contain synchronous GPU readbacks;
they are not frame-pacing measurements. The directly inspected first/last raw
channel contact sheet shows restored structure at 144, without asserting
identical spatial patterns, animation speed or scene appearance.

| Path | Channel 1 std | Channel 2 std |
| --- | ---: | ---: |
| Historical float-target original 60 | 19.9405 | 19.1807 |
| Historical direct-buffer 120 | 19.3332 | 18.3723 |
| Corrected 120 | 18.8573 | 18.2360 |
| Historical direct-buffer 144 | 12.8683 | 12.3266 |
| Corrected 144 | 19.3034 | 18.0951 |
| Same-build 144, correction disabled | 12.0537 | 11.6518 |

A third bounded run uses the same executable, precision/fractional switches,
scene commands and capture method at 144 with only bias correction disabled.
It reproduces the loss: channel-1 std falls from 14.0194 to 11.2507 and channel-2
from 13.5069 to 10.7385. All images still change, demonstrating why freshness
alone is inadequate. This supports the centering correction's causal role in
the restored variation; it does not establish full temporal equivalence.
Control provenance and logs are under `surface-bias-control-144-fractional`
and `logs/surface-bias-control-images.log`.

Profiles, canonical fixture and signatures are preserved; pause/resume completes
and the runner recycles its temporary partitions. Raw images are compressed only
after byte-exact verification. Evidence: `logs/surface-bias-{build,checks,images,metrics}.log`,
`private/test-saves/surface-bias-{120,144}-fractional`,
`private/pipeline/procedural-surface-bias-summary.json` and
`private/pipeline/surface-bias-contact.png`.

This is progress toward a stable fractional update, not acceptance for normal
play. The float-target 60 reference itself differs from original byte storage.
Longer-run evolution, intermediate-state behavior, quantized coefficients,
original appearance/motion, content-preserving live FPS/precision transitions,
capture-free pacing and actual Mac execution remain unverified. Full-game
coverage and the broader goal remain open.


## Feedback texture preservation (2026-10-21)

The host cache previously released a rendered texture whenever its requested
host precision changed, then created an empty replacement. It now allocates a
provisional replacement, copies the live GPU content through the existing
common format-converting shader, and publishes it only when preservation
succeeds. The cache slot is marked used before earlier deferred draws flush, preventing
queued binds from evicting the source during the transition. Existing texture-stage
bindings move to the replacement before the old texture is retired. Allocation
or copy failure keeps the original cache entry, live content and bindings.
Ordinary cache hits do not repeat the copy or readback.

`d3d8_PreserveFeedbackTexture` is deliberately restricted to matching one-level
render targets in the supported byte/half formats. It also reads the converted
GPU result back once into the new texture's CPU storage, reversing the host
half-float channel ordering and applying the Xbox swizzle where needed. Capture
writes that storage when the new texture is first published, so it retains the
copied state rather than recording the original empty allocation. No new capture
chunk is needed. This does not fix the existing general limitation for targets
rendered in earlier frames before a capture begins. The one-time synchronization
cost during a precision transition is not yet measured in live gameplay.

The shared real-RHI fixture now constructs linear and swizzled byte targets with
all 256 values represented in every channel. CPU source storage is deliberately
stale/zero; only the GPU contains the test pattern. Copying to half-float and
back preserves every byte exactly. Reconstructing textures from both converted
CPU images reproduces their GPU pixels bit-for-bit, including channel order and
swizzle. The source GPU image, output target and viewport also remain unchanged.
These cases run in the common Windows WARP fixture and are wired into the native
Mac Vulkan fixture; actual Mac execution remains unverified.

The first GPU check exposed two assumptions: WARP's render conversion can
truncate to the adjacent lower half-float value (`254/255` became `0x3bf7`),
and directly storing host RGBA16 bytes did not match the runtime's ABGR16 upload
layout. The test now permits one half ULP while retaining exact byte round-trip
checks; production reverses the channel transform before swizzling. The CPU-copy
reconstruction check caught and verified that correction rather than relying
on a successful draw alone.

A separate fixture executes the actual production cache function with injected
allocation/copy failures, checking promotion, demotion, queued-draw flush order,
binding replacement, unchanged guest identity, rollback and matching-cache reuse.
Windows Release Build and built-in tests pass. Executable SHA-256:
`9e824f4df45b86b6359b900ba4499f559ae0a2baf95c9bff4d6b02223b962254`,
base `bf43dc7` plus this change, Windows x64/MSVC 14.44.35207/SDK 10.0.26100.0.
The selected suite passes 85 cases in 283.08 seconds, and three historical
patch-stack migrations pass separately in 87.81 seconds. A final cache-slot
protection refinement is followed by another successful Windows Build and
three passing GPU/transaction/backlog checks; the transaction fixture requires
the used-swap stamp before the queue flush. The full migration suite was not
repeated after that single-field refinement.
Evidence: `logs/surface-preservation-{build,gpu,transaction,checks,migrations,patches,final}.log`.

Legacy GL returns unsupported for this conversion and retains the old texture;
it already refuses the half-float feedback registration. Common RHI provides the
implementation for both Windows and native Mac without a second simulation.
No interactive game was closed or new game session launched for these asset-free
checks. Personal saves and settings were not used.

This checkpoint fixes destructive reformatting, but does not yet connect texture
precision selection to live FPS presets. The existing float diagnostic still
requests float targets independently of FPS. The next integration must choose
byte storage for original 60 and float storage for higher refresh, convert
before a changed-precision source is sampled as well as before target writes,
then validate 60/120/144 transitions in-game. Appearance/motion parity, longer
wave evolution, capture-free pacing and full-game/Mac coverage remain open.

## Live water precision transitions (2026-10-21)

The shared opt-in policy now chooses original byte feedback storage at 60 FPS
and half-float at 120/144. It requires all four flags:
`TSFP_SURFACE_FRACTIONAL_TEST`, `TSFP_SURFACE_FLOAT_TEST`,
`TSFP_SURFACE_BIAS_TEST`, and `TSFP_SURFACE_LIVE_TEST`. Normal play retains
the existing path. The live flag has no effect without the other three.

The guest simulation publishes the precision choice before dispatch, including
paused/original/catch-up paths. The HLE setter finishes old queued draws before
changing modes because resource creation happens immediately while binds may be
queued. A cache read converts a changed-precision source before sampling, even
when that texture has not yet been chosen as a destination. The prior
transactional preservation path handles allocation/copy failure and rebinding.
120-to-144 changes do not reformat textures or add a precision barrier.

The optional `TSFP_SURFACE_TRACE_SWITCHES=1` diagnostic records the first 90
positive-time images at each rate, with a 360-image total bound. Rate/segment
columns distinguish paused gaps from consecutive captures. The analysis tool's
`--switch-cycle` option requires exactly 144/120/60/144, 90 images per segment,
and the original timing path at 60; per-segment analysis then checks hashes,
consecutive frame IDs, resource/format/path consistency and actual image changes.
Partial navigation cannot pass this acceptance check. Trace fixtures also check
that paused mode changes consume no images and that the bound releases references.

Windows x64/MSVC 14.44.35207/SDK 10.0.26100.0 Release Build and its built-in
checks pass. Executable SHA-256:
`9ed32894f21ce48e1d5185596a7c52a332b91324e6f7cf918e672cad0e5f173d`,
base `5301c3a` plus the live policy/trace change. The selected suite passes 85
cases (six historical variants deselected); three historical patch migrations
pass separately. Final trace/transaction/statistics checks pass 25 cases after
adding strict complete-cycle acceptance and pause checks; 15 patch-tracking and
backlog integrity checks also pass. Later edits affect
only tests, analysis and documentation, not the tested runtime binary.

The bounded Scotland run in `private/test-saves/surface-live-cycle` applies
144/120/60/144 through Video Options in one process. Both precision transitions
log successful preservation of all three registered feedback textures. The
procedural resource remains the same throughout the four captured segments.
Menu captures show each applied preset and disabled Apply; state samples show
unpaused scene 4 after each return. Profile signatures remain valid and hashes
unchanged; the canonical fixture is preserved and temporary partitions recycled.
The run exits cleanly after 284.425 seconds of commands.

| Segment | Timing path | Images | Fresh pairs | Channel 1 std | Channel 2 std |
| --- | --- | ---: | ---: | ---: | ---: |
| Initial 144 | Native | 90 | 89/89 | 18.5532 | 17.9479 |
| 120 | Native | 90 | 89/89 | 19.2549 | 18.6855 |
| 60 | Original | 90 | 89/89 | 25.0471 | 23.7691 |
| Return 144 | Native | 90 | 89/89 | 20.2485 | 19.3712 |

The inspected raw-channel contact sheet retains wave structure in all segments;
the 60 segment has greater variation. These samples occur at different times,
so their statistics are not a paired dynamics/appearance equivalence test.
Gameplay screenshots face the shoreline wall and establish menu return, not
water appearance in the world. The synchronous GPU image capture is expressly
not a switch-latency, frame-pacing, GPU-completion or scanout measurement.

Evidence: `logs/surface-live-{build,checks,migrations,focused,patches}.log`,
`logs/surface-live-{final,integrity}.log`, `logs/surface-live-cycle-game.log`,
and the run's `surface-results.json`,
`water-channel1-contact.png`, menu captures, reports and provenance. The raw
capture is gzip-compressed after byte-for-byte verification; uncompressed
SHA-256 is `084a0deeb065c806bcf734c801d314cf5809eb0cc7f7458775c4cbaa4b5944ec`.
Earlier `surface-live-switches` only reached 144/120 because navigation failed;
`surface-live-verified` entered the wrong mission and contributes no transition
evidence. The successful cycle recovered from initial Arcade navigation using
observed screens, then completed the measured sequence in Scotland. No failed
navigation is counted as a game physics or rendering failure.

The experiment remains disabled pending original-byte appearance and longer
wave dynamics validation. Capture-free switch latency, representative additional
surfaces/levels, native Mac execution and full-game goal coverage remain open.


## Clock-aligned live FPS pacing (2026-10-21)

The optional shared Present CSV now appends `clock_ns`, the already-sampled
host timestamp. Windows RHI derives it from QPC. The isolated runner records
`perf_counter_ns()` for each key submission/release and labeled observation,
so input-request windows can be aligned without guessing the first-Present
startup offset. These timestamps mark test requests, not physical input latency
or the exact guest callback. Other hosts must establish matching clock domains;
GL/SDL timestamps cannot be assumed to share Windows QPC's origin.

`native_frame_stats.summarize_clock_window` requires data on both sides of a
requested window, consecutive successful-present counters, increasing clocks
and agreement between raw timestamp differences and serialized frame intervals.
Only full intervals inside the window enter the statistics. Legacy relative-time
summaries still work. Tests retain a known 60 ms stall at a large clock origin
and reject truncated bounds, missing timestamps, counter gaps, backward time,
inconsistent durations, NaNs and windows with no complete interval. The new
statistics tests are included in the Windows/macOS runtime workflow.

Windows x64/MSVC 14.44.35207/SDK 10.0.26100.0 Release Build and built-in checks
pass. The broader recorder/host/statistics/backlog selection passes 29 tests,
with one CoreText host-adapter check skipped on Windows. Another 66 CI-scope
and backlog-integrity cases pass. Tested executable
SHA-256: `0c535bd0057283ef6337f90308bb34ec6c91fd041704bb25374afc549bb3b44b`,
base `93a805a` plus the diagnostic timestamp change. This checkpoint changes
observation, not game timing or the default experiment state.

Two sequential bounded runs use `tests/fixtures/live-fps-pacing-commands.json`
at 1280x720, windowed and unminimized. Both start with the original 60 clock,
apply 120 then 144 then original 60 through Video Options, and sample stationary
Scotland gameplay for 12 seconds after each return. Complete intervals in the
one-second-trimmed windows are analyzed. The candidate enables all four water
experiment flags; the control disables all four. Renderer dumps and diagnostic
texture readbacks are off in both; the candidate's actual precision-conversion
readbacks remain part of the measured implementation. This task ran no builds or test suites
alongside these two complete cycles; background system activity was not controlled.

| Path / preset | Guest FPS | p99 ms | Worst ms | Intervals over 33.33 ms |
| --- | ---: | ---: | ---: | ---: |
| Candidate original 60 | 60.000 | 17.796 | 18.369 | 0 |
| Candidate 120 | 120.006 | 9.580 | 10.301 | 0 |
| Candidate 144 | 142.705 | 9.213 | 37.069 | 1 |
| Candidate return 60 | 60.001 | 17.890 | 18.389 | 0 |
| Control original 60 | 59.999 | 18.229 | 18.849 | 0 |
| Control 120 | 119.612 | 9.662 | 27.471 | 0 |
| Control 144 | 143.588 | 8.247 | 27.378 | 0 |
| Control return 60 | 59.996 | 18.284 | 18.670 | 0 |

The Apply windows extend from 0.5 seconds before the final Select request
through the command's post-Apply observation (including its two-second wait).
They include normal old/new-rate frames and are not isolated conversion timings:

| Change | Candidate worst ms | Control worst ms |
| --- | ---: | ---: |
| 60 to 120 | 17.770 | 17.775 |
| 120 to 144 | 13.989 | 13.539 |
| 144 to original 60 | 35.937 | 39.075 |

Each downward switch has one interval above 33.33 ms. The candidate gap ends
38.356 ms after Select submission with only 0.130 ms inside Present; the control
gap ends 69.323 ms after submission with 0.084 ms inside Present. Thus water
precision conversion is not necessary to reproduce the downward-switch gap.
The shared Apply/clock handoff warrants phase-level timing before changing it;
these measurements do not identify its exact cause or exclude system scheduling.
The candidate's separate steady-144 gap is 37.069 ms, followed by 32.200 and
19.551 ms intervals, away from a precision transition. The control also has
shorter steady-play outliers. Neither run establishes uniformly hitch-free play
or an isolated performance cost for the water experiment.

All four endpoint positions/look angles match within each run and across runs;
health stays 35 and the player retains eight loaded rounds and 52 reserve.
Endpoint level-time/wall-time ratios are 0.999715-0.999821. These asynchronous
endpoints support bounded game-speed consistency, not continuous movement or
full animation/mechanics parity. All analyzed presents are guest frames;
there are no interpolated frames, and periodic counters report zero skipped or
failed presents. API acceptance intervals do not measure GPU completion/scanout.

The candidate records 14,601 intervals and ends after 168.381 seconds of
commands; the control records 14,618 and ends after 168.324. Both exit cleanly,
preserve signed profiles and the canonical Test Lab fixture, and recycle their
disposable partition images. The earlier manually paced attempt reaches the
20,000-row cap before return-to-60, and strict analysis correctly rejects that
missing window. The automated replay fits the same unchanged bound; the partial
attempt is not used as complete-cycle acceptance.

Evidence: `logs/live-switch-clock-{build,tests,regressions,integrity}.log`,
`logs/live-switch-pacing-{complete,control}-{game,summary}.log`, and
`private/test-saves/live-switch-pacing-{complete,control}` containing provenance,
input timestamps, reports, raw Present CSVs and `pacing-summary.json`. The private
summary script and read-only menu probe are retained under `private/pipeline`.
No screenshots or game assets were added. Native Mac execution, other scenes,
full-game behavior, original water appearance and the measured pacing gaps remain
open. The next targeted pacing check should distinguish settings application,
original guest-clock waiting and host flip waiting during the 144-to-60 handoff.


## Vehicle and rigid-body clock candidate (2026-10-22)

The complete original US vehicle dispatcher (`17E680`) calls spring update
`175150` behind whole-tick gate `17E6C3`. In the controlled stationary fixture,
60/120/144 rendered updates consumed 1/0.5/0.416666657 seconds per second.
Opening only the gate restored time but made linear adjustment `17E6EC` run
2/2.4 times too quickly. The original rest routine (`176530`) similarly reached
its 60-count sleep threshold after 0.5/0.416666667 seconds if simply called at
native cadence. These are coupled changes, not permission to open all gates.

The underlying complete rigid clock (`171FB0`) also showed 60 solver submissions
and 1/0.5/0.416666670 seconds at 60/120/144 after history settled. The candidate
uses each live game interval and `ceil(delta)` equal substeps instead of losing
zero-whole intervals. The original collision-dependent secondary step selection
is retained. Its original-60 history is rebased only after a native handoff, at
the first advancing interval, including return through pause or slow motion.

Candidate tests report 60/120/144 spring calls and approximately one second;
linear state 20 ends at 13.999977112/14.000053406/14.000045776. Rest eligibility
occurs at one second at every rate. Spring displacement remains step-size
sensitive (.297045410/.300243407/.300794750 in this fixture); equal integrated
time does not establish identical handling. The first combined native timing
run passed 67 tests. Final focused four tests pass, including slow/fast motion, catch-up, the
original secondary-step selection and history handoff. Companion translator,
context and backlog checks pass114; full-stack install/upgrade checks pass four.

`tests/test_native_vehicle_timing.py` lifts original complete routines from the
hash-verified private US XBE; controlled world, driver, rest-eligibility and
solver services prevent claims about real geometry/contact behavior. Four
negative controls separately remove gate, linear adjustment, rest cadence and
rigid-clock fixes and must fail. An asset-free synthetic byte MOV/DEC/store test
runs on both selected CI hosts; the private-XBE tests skip when assets are absent.
No generated code or private assets are distributed. Full driving/force and
collision integration, live modes, multiplayer physics, performance and native
Mac execution remain unverified. The playable combined build is unchanged.

### Original moving-body and support-contact checks

`tests/native_rigid_motion.c` extends the clock fixture through original solver
`171560`, body integrator `16CA00`, quaternion/matrix helpers, relative contact
velocity `1667C0`, resolver `16FC00` and Jacobian/bounds helper `168720`. The
fixture supplies body state, material/allocation services and an upward,
zero-depth contact at the body's centre; world broadphase, contact notifications
and final entity publication remain controlled boundaries. No game bytes are
checked in. The private supported XBE is hash-verified before lifting routines.

The full vehicle test file passes four cases on Windows in 17.10 seconds
(`logs/native-rigid-motion-final.log`). Free motion and constant force produce
60/120/144 fresh body positions and one second of integrated time. Without
external force, final X is 5.998522282/5.998525143/5.998499870. With force X=12
and inverse mass 0.5, final X is 8.948050499/8.973025322/8.977188110 and velocity
is 11.995525360/11.995513916/11.995511055. Constant angular speed agrees with the
analytic half-radian rotation; quaternion normalization remains intact. These
checks allow the original forward-Euler step-size difference, not lost time.

The support case starts with downward velocity -4 and persistent downward force
-20. Original contact policy selects eight secondary integrations per update:
480/960/1152 per second. The body stops after the first update, retaining a
small first-step displacement of -0.008333325/-0.004166665/-0.003472221; final
vertical speed is approximately zero. Every advancing frame still resolves
support impulses. A stationary supported body is not required to change
position merely to satisfy a fresh-update assertion. Zero-time pause preserves
the complete body bytes. Negative controls separately restore the old clock
and remove contact resolution; both fail assertions on the relevant cases.

The initial contact fixture assumed two secondary steps and retained lateral
motion while reusing its contact point. Inspection confirmed the authored
factor is one eighth; the intended axial support case now starts with zero
lateral/angular velocity. No production change or relaxed physical threshold
was used to make that case pass. Sliding/rolling contact is not covered.

An additional private two-identical-contact diagnostic runs the original RNG
`153B40` and its 64-bit multiplication helper `3C9ED0`. Contact shuffling at
`1715A0` consumes 480/960/1152 shared random draws at 60/120/144. With initial
seed 1, the final 64-bit seeds are `13b6de5c:93f5b261`, `06996cae:198df4c1`,
and `97219018:3fd0b281`. This is characterization of an unresolved coupling,
not accepted gameplay parity. The constraint-list shuffle at `170E51` also
uses the same RNG but is not exercised by this diagnostic. Evidence:
`private/pipeline/rigid-contact-rng-audit.py` and
`logs/rigid-contact-rng-audit.log`. The candidate remains WIP and has not
replaced the playable crowd-performance build. Multi-contact ordering, active
vehicle forces/input, real geometry, entity publication and live performance
remain required before acceptance.

### Shuffle RNG isolation and secondary-step rounding correction

The subsequent candidate wraps the complete original `171FB0` with
`pc_native_physics_guest.h`. Within that scope, each `172114 -> 171560` primary
solve is classified using the carried whole-tick budget. The original number
of authored primary solves retains shared RNG side effects. Additional solves
still shuffle and integrate every time, but only shuffle sites `1715A0` and
`170E51` use a private advancing sequence. The original `153B40` generator runs
for every draw; its seed pair and counter are restored after private draws.
Other gameplay random calls remain shared, including calls inside a solver.
Nested scopes restore their parent and retain nothing between frames/levels.

The original-routine motion fixture now supplies three coincident support
contacts, exercising two shuffle draws per secondary solve. At 60/120/144 it
executes 960/1920/2304 original random calls but commits 960 shared draws at
every rate, ending at the identical seed `06996cae:198df4c1`. Physical contact
integration remains fresh. A mutation that bypasses isolation fails the native
case. This checks RNG cadence with controlled contact lists, not identical
arbitrary collision trajectories or full vehicle handling.

The fast-motion extension initially failed independently: game delta 1.25
partitioned into two primary steps, each with secondary duration primary/8,
but repeated float additions left the eighth sum slightly below the target.
The original loop then performed a ninth solve. One wall second at triple
game speed integrated 3.375000101 seconds instead of 3. This failure was not
hidden by relaxing the test. Native ADDSS site `17164F` now snaps to `8*step`
only within four float epsilons of that boundary. The correction preserves
intermediate sums and original-timing arithmetic. The same full original
solver now integrates 3.000000089 seconds; its negative control fails when
the correction is removed.

Expanded native support cases report:

| Case | Simulated seconds | Carried ticks | Primary solves | Actual / shared RNG draws |
| --- | ---: | ---: | ---: | ---: |
| Quarter speed, 576 frames at 144 | 1.000000007 | 60 | 576 | 9216 / 960 |
| Triple speed, 144 frames at 144 | 3.000000089 | 180 | 288 | 4608 / 2880 |
| Repeated irregular intervals | 1.383333387 | 83 | 120 | 1920 / 1328 |

An independent 64-bit recurrence checks the complete shared seed, not only its
draw count. The asset-free scope case checks varied whole/primary budgets,
nested fractional solvers, an unrelated shared draw between private draws,
original-mode behavior and scope cleanup. Five expanded cases passed in 20.98
seconds (`logs/native-vehicle-rng-expanded.log`). The additional asset-free
20,000-partition rounding contract also passes. Final Windows suites pass 70
timing/vehicle tests in 91.37 seconds, 115 translator/generation/context/backlog
companions in 24.17 seconds, and four fresh/repeated/native-checkpoint/controls/
Mac-stack patch checks in 74.42 seconds. Logs are
`native-vehicle-rng-regressions.log`, `native-vehicle-rng-companions.log` and
`native-vehicle-rng-upgrades.log` under `logs/`. The first companion command
used a nonexistent scope-test filename and collected nothing; the corrected
command produced the reported 115 passes. No full-build or live-driving claim is made;
the previously tested crowd executable remains installed.

### Entity publication, vehicle components and rigid-body sleep

The next fixture extension executes original `1719E0/165A40/9E840` publication
and bounds transforms, `1753A0` vehicle component integration, and
`1677E0/167880` sleep/wake. Entity position and velocity now receive the actual
body results every advancing frame. The fixture supplies controlled external
forces before each advancing update because the original publisher clears
the accumulators. A rotating nonzero center offset exercises the position and
bounds transform. Component acceleration, angle accumulation and clamping run
at 60/120/144; their small forward-Euler differences remain explicit.

This exposed another real timer defect: direct byte-memory DEC `171F83`
expired ten sleep counts after ten rendered updates, taking
0.166667/0.083333/0.069444 seconds. The candidate extends the existing byte-load
policy to this exact decrement. The lifter applies the hook once to its input,
then derives flags from the actual stored result, retaining carry. Unrelated
sites and original timing keep their arithmetic. Synthetic tests exhaust all
256 byte values at the target and an unrelated site, checking ZF, OF and CF.

The full original-routine test now sleeps after 10/20/24 frames at 60/120/144
(0.166667 seconds), or 40/80/96 at quarter game speed (0.666667 seconds).
Triple game speed retains the original raw-clock sleep cadence. Sleeping bodies
leave the active list; two wake calls reinsert the body once, and the next
positive fractional update publishes movement immediately. Removing the new
sleep policy fails the 120 FPS case. Evidence includes
`logs/native-rigid-publication-before.log`, `logs/native-rigid-sleep-wake.log`
and `logs/native-rigid-memory-dec.log`.

Windows final suites pass 110 timing/vehicle/float-join/vehicle-join/generation
tests in 132.34 seconds (`logs/native-rigid-publication-regressions.log`) and
four fresh/repeated/checkpoint/controls/Mac-stack patch cases in 101.69 seconds
(`logs/native-rigid-publication-upgrades.log`). The earlier focused test used
an unsupported synthetic SETO instruction; replacing that test-only instruction
with the supported JO branch yielded two passing exhaustive DEC cases. No
unrelated lifter instruction support was added. Real world geometry, vehicle
driver/force production, damage/model notification and rendered driving remain
unverified. These routine results do not establish full handling acceptance.

The first supported Windows build regenerated guest code but failed to link
`sub_00171FB0_gen`. The runtime manual scanner only recognizes standalone
`extern void sub_X_gen(void);` declarations; combining it with two other
declarations was valid C but caused the generator to omit the original body.
The patch now separates that declaration, and a new asset-free regression uses
the actual scanner to require both wrapper recognition and solver/RNG name
pinning. The full patch loader applied the repair normally. The failed build
is recorded in `logs/native-vehicle-full-build.log`; a corrected build is
required before any gameplay claim.

### First full build and live vehicle comparison: acceptance still fails

The corrected supported Windows/MSVC build at `ba4f0bf` passes generation,
linking and the builder's native tests (`logs/native-vehicle-wrapper-build.log`).
Executable SHA-256 is
`d973dbe170da727c0d8bc554cd16e1445aa03b01b1077f3776b199632d45b78f`.
The scanner regression passes, 78 generation/context/backlog/scope checks pass
in 1.39 seconds, and four full-stack install/upgrade cases pass in 186.13 seconds.
The patch loader applied the correction normally; generated source was not edited.

Bounded isolated Test Lab runs use Siberia multiplayer, Classic textures,
1280x720 and the default six bots on the same Windows i7-13700K/RTX 4070 SUPER
D3D11 host. The initial exploratory `vehicle-live-144` run establishes that
entry, rendered driving, ice/bank motion and collision with a wall execute.
It contains long idle periods and combat damage, so it is not the matched
handling comparison. W moves the player on foot; the right trigger (replayed
left mouse button) accelerates the jeep. Initial W-only vehicle commands did
not test acceleration.

The subsequent `vehicle-live-60`, `vehicle-compare-120` and
`vehicle-compare-144` runs use the same shorter command sequence. Each starts
from the same player spawn, approaches and enters the parked jeep, holds the
right trigger for three seconds, then drives toward the bank and pauses/resumes.
Captures were reviewed at all three rates. Player health remains 1.75 through
the compared throttle interval. Read-only player position samples taken during
that first throttle hold give:

| Requested preset | Sample span (seconds) | Authored ticks advanced | Distance (game units) |
| --- | ---: | ---: | ---: |
| Original 60 | 2.972633 | 179 | 29.958242 |
| Native 120 | 2.975056 | 178 | 19.875402 |
| Native 144 | 2.977932 | 178 | 22.393338 |

Each row has 60 asynchronous samples. Initial XYZ values are approximately
(-12.48, 1.57, 7.03), differing by less than 0.02 game units. These results
contradict handling acceptance despite normal elapsed game time and the passing
controlled-routine tests. They do not yet isolate whether force production,
wheel/contact coupling or another vehicle path causes the difference. The
later bank trajectories also diverge; no arbitrary terrain-parity claim follows.
These are captured diagnostic runs, not locked 120/144 performance tests. The
native trace confirms positive fractional updates; it does not record vehicle
state at every completed update. The first exploratory trace hits its 20,000-row
bound before the entire driving route; the shorter comparisons remain within it.

All four runners exit cleanly, preserve fixture/profile signatures and recycle
their disposable partitions. Logs are `logs/native-vehicle-live-{60,144}.log`
and `logs/native-vehicle-compare-{120,144}.log`; command sequences, captures,
provenance, observations and reports are in the correspondingly named isolated
directories under `private/test-saves/`. The summarized throttle samples are
`private/pipeline/vehicle-live-comparison.json`. Exit-button attempts did not
establish leaving the jeep and remain unverified. Mac driving is untested.

The candidate EXE/PDB are retained at
`private/build-recovery/vehicle-ba4f0bf/` for continued diagnosis. The previously
tested combined crowd-performance EXE/PDB were restored to the normal Release
location, with executable hash
`0072d5b4651b25b886e11fa1d939e4e2ef4f5306e347422f570135c068efc7ab`
reverified. No candidate PR or release has been published. The next investigation
must include actual driver/force/contact production, beyond the controlled
component inputs that currently pass.

### Driver input gate follow-up

The original driver dispatcher `17DA00` reads the integer game delta at
`17DA03` and exits when it is zero. Its throttle/brake producer `172EE0` then
does not replenish vehicle fields `+530/+534`, which the now-fractional vehicle
dispatcher consumes and clears. The new `native_vehicle_driver.c` fixture lifts
both complete routines and the original seat switch table from the verified US
XBE. Before correction, native120 runs only 60 throttle updates in a second and
integrates 100 units of a requested 200-unit input (Original60 integrates 200).
See `logs/native-vehicle-driver-before.log` for that actual failing assertion.

The shared numeric policy now treats `17DA03` as a positive-game-time entry
gate. Its temporary integer value is overwritten before use as anything else.
Original timing remains unchanged; stale integer ticks cannot advance a native
zero-float pause. With the correction, the original producer runs on every
native interval and integrates the same input at 60/120/144. Tests also cover
fractional analog throttle changes, braking, reverse, release, quarter/triple
speed, irregular intervals, pause and stack/callee-saved invariants. The
`legacy-driver` negative control restores the original gate and must fail.
Model lookup, wake and seat-action services are controlled. This test establishes
input production, not tire traction, weapon behavior or live handling parity.
The Windows regression run passes 112 timing/vehicle/float-join/vehicle-join/
generation cases in 132.95 seconds (`logs/native-vehicle-driver-regressions.log`),
plus 77 context/backlog/CI-scope cases in 1.18 seconds. The original-routine
test requires the supported private XBE and skips without it.

### Rebuilt driver comparison (`67c1864`)

The supported Windows build and its build tests pass. Executable SHA-256 is
`02757346892118abf6caf7eb1376634afce4753a93155600c21d799b3ef2adb8`.
The same established Siberia input sequence runs sequentially at 144, 60 and
120 with six default bots, Classic textures and 1280x720 captures. All sampled
throttle windows have 60 observations, 178 authored ticks, health 1.75 and
starting positions within 0.02 units. Initial throttle travel is now:

| Requested preset | Sample span (seconds) | Travel (game units) |
| --- | ---: | ---: |
| Native 60 | 2.975451 | 29.694886 |
| Native 120 | 2.976108 | 29.677941 |
| Native 144 | 2.975363 | 29.674005 |

This removes the earlier large acceleration deficit; the spread is below 0.1%.
The later route still diverges at terrain contact: 60/120 stop against the ice
pillar near Z=67.57/68.56, while 144 proceeds onto the bank near Z=106.06. Captures
confirm these different outcomes. No collision root cause is established by
this replay, and broader handling acceptance remains open. The exit attempt
again does not leave the vehicle. Pause and resume are observed at each rate.

Native120/144 traces have median positive game deltas 0.52116/0.51533 ticks;
1,703/1,766 advancing gameplay updates carry zero integer ticks. These are
diagnostic runs, not locked-preset FPS or pacing measurements; a separate
menu-footer checkout's game also remains open and untouched. All three runners
exit cleanly, preserve fixture/profile signatures and recycle disposable disks.
Evidence: `private/test-saves/vehicle-driver-{60,120,144}`, corresponding
`logs/native-vehicle-driver-{60,120,144}.log`, and
`private/pipeline/vehicle-driver-comparison.json`. Runner SHA-256 is
`f9e0751890fc3511181c993a3881737c3c7ed173b9fac7f4c03742b71c9755f5`.
The previous playable crowd EXE/PDB are restored and their executable hash
reverified. Current candidate files are in `private/build-recovery/vehicle-67c1864`;
the superseded `vehicle-ba4f0bf` binary pair was recycled. Source remains on the
focused WIP branch, without a candidate PR or release. Mac execution is pending.

### Character contact velocity and notification follow-up

Analysis of the existing pillar replay finds the approaches differ before
impact. Aligning adjacent asynchronous observations at Z=66 gives X values
-11.46446/-11.64785/-11.75056 and headings 2.81737/2.18834/1.84633 degrees
at requested 60/120/144. The alignment is an analysis of samples, not a game
timing change. Evidence is `private/pipeline/vehicle-contact-approach.json`.
These small pre-impact differences prevent attributing the later different
outcomes to a collision defect without a controlled contact comparison.

The audit independently identifies a missed gate in original contact-relative
velocity function `1667C0`: `166899` skips the moving character contribution on
zero-whole native updates. The guarded conversion already divides per-update
displacement by float game time. The gate register is immediately restored;
it is not a tick counter. The shared numeric policy now admits positive live
game intervals there, preserving original mode and avoiding division by zero
on native pauses with a stale whole tick. Static and rigid-body branches are
unaffected. This is not a fix for the static-pillar trajectory.

`native_contact_velocity.c` lifts full `1667C0/166060` from the verified US XBE.
It checks analytical point velocity (linear plus angular cross offset), body,
static and character contacts, all axes and signed/zero actor velocities,
60/120/144, quarter/triple speed, irregular intervals, pause, nullable output
and stack/callee-saved state. Before correction it passes Original60 then fails
at native120 (`logs/native-contact-velocity-before.log`). The restored-gate
negative control must still fail at that rate.

The coupled rigid fixture adds a body resting on a character moving upward at
2 units/second with gravity. A body-position-only assertion passes even with
the old gate: the impulse solver independently reads live displacement.
The observable defect is the relative normal/vector velocity forwarded through
the original `16E780/16DA90` path to the body impact callback. The corrected
pipeline reports relative velocity near zero instead of the body's absolute
2 units/second on fractional updates. The callback assertion rejects
`legacy-character-contact`; the original dispatcher and contact-kind table run
unchanged. Body positions after one second remain approximately
1.999984/1.999991/2.000027 at 60/120/144 with velocity approximately 2.
Material coefficients are controlled at zero, so actual damage/effects and
live character movement remain outside this check.

Validation: 113 timing/vehicle/float-join/vehicle-join/generation cases pass in
139.99 seconds (`logs/native-contact-velocity-regressions.log`), followed by the
expanded original notification-path fixture in 12.04 seconds
(`logs/native-character-contact-notification.log`). These original-routine
tests require the private supported XBE. Full game build/live contact and
native Mac execution of this correction remain pending. The playable crowd
EXE/PDB and retained driver candidate `67c1864` are unchanged.

### Initial motion inherited by spawned physics objects

Original `1727A0` creates a physics object from a model joint. Its gate at
`172AB5` checked integer time before inheriting the difference between current
and previous joint positions. A spawn on a fractional native update therefore
permanently lost its initial linear velocity and angular momentum. The
conversion at `172B01` already divides by fractional game time; the temporary
gate register is overwritten. The shared positive-time gate now includes this
site, while Original60 and native pause retain their intended behavior.

Joint history is copied by `57040`, called at `A7425` in the entity updater.
Caller `2107E8` passes a nonzero inheritance factor; `1FA7B2` passes zero and
intentionally disables inheritance. This routine creates an object once per
event; it is not a continuous animated-collider update.

`tests/native_spawn_motion.c` lifts full `57040/1727A0`, real linear-velocity
and angular-momentum setters `167CE0/167C00/167AE0`, and their matrix helpers
from the verified US XBE. Controlled services provide allocation, neutral
Euler extraction, owner transforms, CRT floor, bounds and wake notification.
The history routine samples the prescribed joint before it moves. Independent
spawn events check every carried-clock phase at 60/120/144, signed/disabled
inheritance, identity/rotated transforms, an offset vertex, slow/long intervals,
pause with stale whole time, missing history, allocation failure and guest ABI.
The before-fix run passes Original60 and fails native120's velocity assertion
(`logs/native-spawn-motion-before.log`). After correction it passes, including
the old-gate negative control (`logs/native-spawn-motion-after.log`). These
checks establish initial body state, not live appearance or full trajectories.
The broader Windows suite passes 114 timing/vehicle/translator/generation
checks in 147.56 seconds (`logs/native-spawn-motion-regressions.log`) and 77
context/backlog/scope checks in 1.17 seconds. Supported Windows build `3e1ee87`
and its built-in validation pass (`logs/native-spawn-motion-build.log`), using
MSVC 14.44.35207, Windows SDK 10.0.26100 and the common Xbox frontend. This
build also includes the prior character-contact correction `6232685`.
Candidate EXE SHA-256:
`6cfe6c7a5b5fe71b0f128b520e1e08affd1e50e3fbd672de783c34f21387c42e`.
The EXE/PDB pair is retained in `private/build-recovery/vehicle-3e1ee87`;
the superseded `vehicle-67c1864` pair was recycled. The normal playable
`4adb6b0` pair was restored and its executable hash reverified as
`0072d5b4651b25b886e11fa1d939e4e2ef4f5306e347422f570135c068efc7ab`.
No game session was launched for these two additions. Live spawn/contact,
broader handling, measured pacing and native Mac validation remain pending.

### Actor-to-vehicle contact velocity units

The previously deferred `17CBAF` gate cannot safely be treated as another
missing fractional-update gate. Original entity post-update `A7360` already
divides displacement by live game time at `A75F8`, publishing units per
authored tick in entity `+F8/+FC/+100`. Callback `17CAE0` then multiplies those
fields by60, but on positive whole counts also divides by live time again at
`17CBD6`. The same constant-speed contact at120 therefore alternates between
the correct speed and twice that speed. Merely opening the gate would make
both phases wrong. Native `17CBD6` now returns its numerator, leaving the
original mode, callback cadence and `17CBAF` gate unchanged.

Jeep initialization registers this callback as event7 (`182C04 -> A5CE0`),
stored at entity `+170`. Player collision delivers it at `C4A3F`; original
physics contact handling queues events through `1709F0/170B53 -> 16C070` and
drains them at `16C126` in `16C0D0`. `A7870` runs the physics clock before
the entity post-update, so prior published velocity may be consumed by the
next interval. Reusing a different interval must not rescale that velocity.

`native_vehicle_actor_impact.c` executes full original publication `A7360`,
registration `A5CE0`, queue append/drain `16C070/16C0D0`, callback `17CAE0`,
point velocity/contact construction `166060/1665F0`, impulse selection
`17BC90`, and suspension response `174EC0` with its original mass/matrix
helpers. Motion and contact geometry are prescribed; allocator release and
the final effect service are controlled. Each case is an independent contact,
not a repeated-contact or driving simulation. It checks all carried-clock
phases at60/120/144, signed/stationary motion, 25 combinations of producer and
event intervals, original-mode multi-tick behavior, pause and guest ABI.
Both contact speed and actual suspension component changes are asserted.

After fixing test-only stub declarations and contact-normal sign expectation,
the before-fix run passes60 and zero-whole120, then fails the carried120 case:
normal speed -10.4 instead of -5.2 (`logs/native-actor-impact-before.log`).
The corrected fixture passes (`logs/native-actor-impact-after.log`), including
negative controls restoring the old division or opening only the old gate.
The combined timing/vehicle/numeric-join/generation suite passes 115 tests
in 146.56 seconds. Supported Windows build `60e5b36` and its built-in checks
pass (`logs/native-actor-impact-build.log`), using MSVC 14.44.35207 and
Windows SDK 10.0.26100 with the common Xbox frontend. Executable SHA-256:
`6431273bb202aa4a0efa9b66cd2611cd3b63191cd9b3cb111115d6aa59c224d4`.
Live vehicle/actor acceptance and native Mac validation remain pending.

The bounded native144 Siberia probe is recorded in
`private/test-saves/vehicle-actor-trace-60e5b36-144` (`report.json`, selected
captures, `run0.err`, `contact-probe.json`). It walks into the parked jeep
without entering it, reaching 64 bounded `17CAE0` entries from the original
player collision return `C4A41`. The scene also records 64 `17BC90` and ten
`174EC0` entries; these include vehicle/world setup contacts and are not
attributed to the player impact. Pause/resume changes the observed pause
flag 0/1/0, health stays 1.75, and the isolated profile/signatures are intact.
The reviewed contact capture shows the player stopped beside the jeep.
Entry traces do not expose the actor argument/type or confirm that a particular
entry reached `17CBD6`; they do not measure impact strength or repeated-contact
parity. No locked-144 FPS or full handling claim follows from this probe.

The first probe (`vehicle-actor-60e5b36-144`) reached the same scene but had
no function trace because `prepare_run` correctly strips inherited debug
environment variables. The runner now accepts explicit `--trace-vehicle-contact`
after isolation, with a 64-entry budget per function, mutually exclusive with
the different `--watch-input` function trace. The repeated probe uses that flag.
Both bounded child processes exited normally and disposable partitions were
cleaned by the runner. The new candidate EXE/PDB is retained under
`private/build-recovery/vehicle-60e5b36`; the superseded `3e1ee87` pair was
recycled. Playable `4adb6b0` was restored with its previously recorded hash
verified. The root object cache remains the newer candidate, requiring a
fresh link after preserving the playable pair before any subsequent rebuild.

### Steering response subdivision

The next original-routine audit identifies a separate steering-response mismatch.
`172EE0` computes a speed-dependent signed target, subtracts the current steering
value at vehicle data `+538`, multiplies by float game time at `173062` and
then by 0.07 at `17306A`. Dividing this linear weight over more rendered updates
changes its remaining-error decay. The same half-second quarter-speed stage
ends at -0.102306642 with the original 60 update response but -0.100790747 at
native120 before correction. This is a controlled steering result, not proof
of the earlier static-pillar trajectory's cause.

The shared numeric policy adds only `17306A` to the existing aim-easing rule.
For a fractional raw interval, it splits the authored remaining-error factor
at the current game-speed ratio. One-tick and catch-up intervals keep original
arithmetic, as do authored weights outside the decay range. The speed-dependent
target clamp and wheel eligibility/distribution remain original code.

The expanded `native_vehicle_driver.c` executes full original `17DA00/172EE0`
from the hash-verified US XBE. It compares independent 30-step Original60
recurrences for left, right and released targets across Original60/native60,
120/144 and irregular raw intervals, three prescribed game speeds and three
vehicle-speed inputs (including the target clamp). Every advancing sample must
publish fresh steering and the correct eligible-wheel angle; other wheels,
paused state, stack and saved registers remain intact. Whole/catch-up and
overshooting-weight cases preserve their linear step. The before-fix log fails
at native120; the corrected fixture passes and the independent
`legacy-steering` mutation restores the failure. Logs:
`logs/native-vehicle-steering-{before,after}.log`.

Two asset-free synthetic SSE cases in the same registered Python suite exercise
the actual lifter and numeric policy at the steering site and an unrelated
site, including Original-mode arithmetic and preserved upper lanes. Both pass
on Windows (`logs/native-steering-coefficient.log`). Private original-routine
cases still skip when the game asset is absent. World geometry, wheel forces,
physical input and live handling remain separate acceptance work; no full-game
or native Mac claim follows from these checks.

The full Windows timing/vehicle/float-join/vehicle-join/generation suite passes
117 tests in 153.58 seconds (`logs/native-steering-regressions.log`); another
77 context/backlog/scope checks pass. Across 135 steering stage comparisons,
maximum absolute error from the independent Original60 recurrence is
0.000000238. Supported Windows build `464f850` and all built-in checks pass
(`logs/native-steering-build.log`), using MSVC 14.44.35207, SDK 10.0.26100 and
the common Xbox frontend. This pair was superseded by integrated `d0116a7`;
its historical EXE SHA-256 is
`a3794aa372fd1e975dbe67444c7b7b4c4de476abaa8545d9ada5ac58080a9068`.
The normal `4adb6b0` pair was restored with its previously recorded hash verified.
No live driving run was performed at that source revision. Subsequent integrated
source `d0116a7` incorporates main `7545d4b` and passes the supported Windows
build, 147 combined runtime/source checks, four upgrades and 88 repository
checks. All selected Windows/Mac CI and transition jobs pass at `d0116a7`
(runtime workflow 39851684749). Original-routine cases require private assets;
these hosted results do not establish actual Mac gameplay.

**Reference-clock correction:** the live `vehicle-*-60` records in this vehicle
investigation use native60 (`--native-fps 60`), verified by saved provenance and
startup logs. They do not exercise Original60; prior original-clock labels are
withdrawn. This does not alter offline fixtures that explicitly disable native
timing. Use zero/disabled native timing for a genuine live original reference.

Initial integrated live144 runs were excluded from steering comparison: one
observer confused a body handle with a pointer and had no samples, and in the
next run a bot killed the player before entry. The later native60 route
showed steering/throttle and a stationary paused jeep but still had six bots,
despite its `60-empty` name. `logs/vehicle-integrated-live-summary.json` records
these exclusions and limits. A subsequent setup probe selected the normal
None bot preset (roster257, zero bots); the different spawn invalidated its old
walk-to-jeep route. No handling comparison is claimed from that probe.
Use the verified None preset and observed approach position for future matched
runs; preserve asynchronous diagnostic scope and inspect actual jeep entry.
The corrected `vehicle-integrated-settled-{60,120,144}` runs use that setup,
a bounded normal-input approach and six seconds for the entry animation.
All three pass signed steering (>0.9 magnitude in each direction), throttle
movement and stationary paused-body checks, with zero bots and unchanged
profiles. The 3,502 / 3,507 / 3,536 read-only samples are asynchronous; see
STATUS and `logs/vehicle-nobots-comparison.json` for sampled differences.
Those images prompted a separate camera/body position probe because their
180-frame dump cadence yields different capture ages across presets. Before
the camera fix, valid ownership/mode/FOV and matching frame brackets show
distance 10.44548 at native60 versus 15.13290 at native144 near vehicle Z30.
Original `CA040` discarded fractional frames despite consuming a fractional
physics-history duration; `CA424` also required a nonzero spring-substep count.
Shared numeric fix `24d0ad5`, integrated into `fb261e1`, corrects both loads
while retaining Original60, authored coefficients and world routines.

The full-original camera fixture reproduces the deficit and passes after the
fix; gate/count counterfactuals fail independently. Full Windows build and all
selected CI pass at `fb261e1`. Its matched no-bot camera runs at 60/120/144
measure distances 10.10340 / 10.10925 / 9.97361 near Z30, with signed steering,
stationary pause and unchanged profiles. Captures show the expected chase
view. STATUS records all checkpoints, binary provenance and scope. These
single short captured runs use asynchronous observations, not completed-update
or scanout traces. Exact response, complete trajectories, terrain/contact,
physical input, vehicle exit, frame pacing and native Mac acceptance remain
pending. Evidence: `logs/vehicle-camera-comparison.json` and isolated
`vehicle-integrated-camfix-{60,120,144}` reports/provenance/captures.

The subsequent straight no-bot obstacle route uses that same `fb261e1` binary,
removes all pre-drive steering, and extends throttle through two additional
three-second bursts. Native60/native144 stop near Z67.987/67.912, whereas
native120 reaches the bank near Z109.037. A second native60 stops at Z67.568.
Captures confirm the different outcomes. Steering remains below0.001, health
stays1.75 and pause/profile-preservation checks pass in all four sessions.
The earlier six-bot replay split at144; this one splits at120. Small position,
orientation and vertical-motion differences are present before the pillar
edge, including differences between the two native60 runs. Neither replay
establishes the collision root cause. No production mechanics change was made;
controlled obstacle/contact acceptance remains open. See STATUS and
`logs/vehicle-terrain-comparison.json`, plus the isolated
`vehicle-integrated-terrain-{60,120,144,60-repeat}` records. Captured asynchronous
observations do not establish exact trajectories or pacing.

### Synchronous vehicle contacts and actual original-clock reference

Build `20dc25d` adds a disabled-by-default bounded solver-entry diagnostic.
Generated arena records and selected pointers are read at `172114 -> 171560`
with checked memory spans. The arena has already passed selection/merging;
it is not untouched producer output. The previous asynchronous probe sampled
the arena after reset and cannot establish absence of contacts.

New original60/native60/native120 runs on the same Windows executable record
453/663/870 solver entries and 945/1,385/1,750 selected target contacts. The
original run explicitly uses `--native-fps 0` and verifies `native=0`; controls
verify `native=1`. All three stop at the pillar, near Z67.946/68.027/67.622, with
visible contact response and stationary pause. Initial contact geometry and
approach velocities differ; the earlier pass-through remains unresolved.
The native144 navigation attempt never reaches the jeep and is excluded.
Independent frame-bracketed camera samples near Z30 are 10.15778/10.13406/
10.03145 units for original60/native60/native120. These short captured runs are
not pacing evidence or full original-versus144 acceptance. See STATUS and
`logs/vehicle-solver-comparison.json` for scope, provenance and exclusions.

The subsequent `vehicle-integrated-solver-144-detour` reaches the jeep using
normal walking waypoints around the obstructing column, then runs the same
jeep inputs. Its 241 solver entries include 655 selected contacts within
Z60..80; the jeep continues to the bank near Z105.204. The first object normal
points sideways (-0.9962,-0.0654,+0.0573), unlike the original-clock front normal
(-0.0551,0,-0.9921). Allocated records contain four side-facing normals and no
front-facing normals in that window; selection may already have merged them.
This identifies different contact geometry entering the solver, not a proven
missed response. Approach differences and broader handling remain open.
Camera/body distance near Z30 is 10.01738 at144 versus 10.15778 at original60;
114 settled pause observations are stationary. These captured synthetic-control
runs remain separate from pacing and scanout measurements.

The original rigid fixture also checks 576 nonzero-depth/rebound/quantized-normal
cases against an independent generalized impulse with unit inverse inertia.
Original penetration correction is already a velocity (12 * clamped overlap),
not a per-frame displacement. Dropping that term fails the new counterfactual.
All 19 vehicle tests pass; controlled geometry and the private-XBE requirement
limit this evidence. No runtime mechanics change follows from the audit.

### Vehicle exit and on-foot controls

Windows `20dc25d` passes a bounded Siberia jeep route at genuine Original60 and
native144: enter, throttle three seconds, settle five seconds, Y exit, one
second of walking, pause/resume. Exit restores weapon 4 and camera 24 -> 0; walking
moves 7.988814/8.001610/7.990989 units while the jeep stays stationary. Each run has 114
settled zero-delta pause samples with unchanged body/camera positions, zero
bots, health 1.75 and unchanged valid isolated profiles. The synchronous trace
verifies native=0/native=1. Exit/movement captures were reviewed. Full handling,
other seats/vehicles, physical input, pacing and native Mac remain unverified.

Default Y supplies logical 0x40, which original `D2470` forwards to `17DA00`.
Earlier E/F/R attempts are excluded as incorrect input (F/R measured 0x20/0x10),
not classified as high-refresh defects. Guest logical bits differ from Xbox
analog indices. This is validation/documentation only, with no runtime change.
Evidence: `vehicle-integrated-exit-y-{original60,120,144}` and
`logs/vehicle-exit-comparison.json`; source/build/hash/host details in STATUS.

## Native-start return to Original 60 (2026-10-22)

The earlier 36–39 ms cycle gaps were measured before main's late-frame pacing
repair. On main 564fda6, a corrected Original60 → 120 → 144 → Original60 replay
has no interval over 33.33 ms in its three Apply windows (worst 18.371, 15.672,
17.720 ms). The return frame is 9.396 ms: original clock 2.540 ms and flip gate
0.0008 ms. This does not retroactively assign the historical gaps one cause.

Starting directly at native 144 still reproduces a distinct double wait. Native
Swap bypasses Arm, leaving its function-local previous-boundary value zero.
On return, the original clock waits for the next ISR; Arm then resets that
ready event and waits for another tick. A bounded opt-in QPC phase diagnostic
measured a 31.568 ms frame: original-clock wait 9.993 ms, gate wait 16.391 ms,
and original Swap body 0.520 ms. The same replay with the fix measures 20.128 ms:
clock 14.505 ms, gate 0.0008 ms, Swap body 0.426 ms. The varying first clock wait
reflects the phase at Apply; the identified extra gate wait is removed.

`-native-flip-handoff.patch` records the last real vblank during both native
Swap and Present-only bypasses. Arm retains its real-signal and elapsed-period
checks, with normal cheap-frame pacing. The shared observer neither waits nor
consumes/synthesizes a tick. Tests execute the actual observer/Arm bodies at five
native phases and retain cheap/late/no-signal cases. Removing the observation
reproduces the failing first-return assertion. The diagnostic trace code was
removed before the final production build; it is retained only in ignored
investigation artifacts.

The old live replay entered Controls after the pause menu reorder; finishing
its command labels did not prove FPS changes. The tracked fixture now uses one
Down to Video Options. Analysis additionally requires actual NATIVE-TIMING
transitions and the saved preference; the stale run is rejected as evidence.

Phase evidence: `private/test-saves/handoff-{phase-corrected,initial-native,
initial-native-fixed}`, QPC joins in `handoff-phase-summary.json`, and
`logs/handoff-*-phase.log`. All tests use isolated signed Test Lab copies and
recycle disposable partitions. No personal settings/saves are changed. Native
Mac execution, other scenes, full-game mechanics and monitor scanout remain
unverified. This patch is independent of the crowded-multiplayer optimization
in PR #295; these main-based binaries do not contain that unmerged change.


Final trace-free Windows binary SHA-256:
`70d3327017b1989136a5a42b1c5abacf38c44010d3fe82e4d7c6d2bc535f4f78`.
The stationary Scotland cycle (`handoff-final-cycle`, 1280×720) measures:

| Window | Mean FPS | p99 interval ms | Worst interval ms |
| --- | ---: | ---: | ---: |
| Original 60 | 59.998 | 18.395 | 18.904 |
| Native 120 | 120.000 | 9.673 | 10.524 |
| Native 144 | 143.994 | 8.225 | 8.813 |
| Return to Original 60 | 60.005 | 17.860 | 18.151 |

The three Apply windows have worst intervals 17.727, 13.510 and 22.922 ms;
none exceeds 33.33 ms. These windows include the surrounding menu wait, not
just conversion. All observed presents are guest frames. Strict analysis
requires continuous frame IDs and clock coverage, matching health/position/look,
and actual applied rates. Endpoint game-clock/wall-clock ratios range from
0.999829 to 1.001160; endpoints are quantized and do not establish animation or
mechanics parity. Supported Windows Build, 44 selected frame-clock/patch tests,
backlog integrity and repository-context checks pass. Two initial historical
fixture failures were repaired by excluding the new dependent patch from old
stacks; all three affected upgrade cases were rerun successfully.

The same trace-free binary's native-start replay (`handoff-final-native-start`)
confirms 143.987 FPS before switching and 59.998 afterward (p99 8.209/17.875 ms,
worst 9.385/18.310 ms). Its Apply window worst is 18.105 ms, with no interval
over 33.33 ms. Endpoint clock ratios are 0.999821/0.999836. Both final runs exit
cleanly with valid isolated save signatures; no tracing/captures/CPU sampling
ran during measurement. Saved preference ends at Original 60 in each run.

## Walking update and boost resource audit (2026-10-22)

The existing remote-correction fixture controls `sub_001CE850` rather than
executing its player movement. Following its dispatch chain reaches the normal
walking routine `sub_00258F50` through `sub_000D5EB0` and `sub_000D5C70`.
The new fixture executes all of US `0x258F50..0x2597FF`, lifting code and scalar
constants from the hash-verified supported XBE. Controlled input displacement,
world eligibility and a flat collision floor replace real input/level geometry.
No gameplay code was copied into the fixture or edited in generated output.

Ordinary forward travel is 12 units over one second at original 60 and native
60/120/144 (maximum observed numerical error below 0.00002). Boosted travel
is 19.2 units at each rate. However, the mode-byte-0x11 path subtracts the same
0.006 resource charge at `0x259060` on every active update. With initial test
resource 100, one second leaves 99.640198 at original/native 60, 99.280396 at
120 and 99.136475 at 144. The full-routine resource assertion fails at 120
before the fix; normal walking and boost speed still pass.

The shared site-specific policy now splits the charge across fractional active
updates. It uses the lesser of raw and game time, capped at one original charge,
so quarter-speed consumption matches the original parent dispatcher's skipped
zero-whole-tick updates, while triple-speed and catch-up calls retain one-charge
semantics. The disabled-native path, unrelated subtraction sites, shared charge
constant, boost multiplier and depletion threshold remain original.

After the fix, 120 leaves 99.640198 and 144 leaves 99.639648. Quarter-speed
results are within 0.0003 of 99.91; triple speed remains approximately 99.64.
A separate initial-resource-1 test checks depletion time/travel within one
original update, retaining threshold overshoot. Eligibility, released input,
empty resource, pause/catch-up, diagonal movement, stopping/reversal, stack and
callee-saved registers are checked. A no-fix mutation fails the same resource
assertion. Asset-free clock-to-resource cases cover all presets in shared CI.

Scope: this is actual-routine evidence with controlled services, not live boost
activation, real collision/terrain, upstream input acceleration, full network
prediction or native Mac execution. A read-only frontend lookup through the
existing Discord rule-name decoder identifies mode index 17 as Escort (string
ID 5861); control index 3 resolves to Deathmatch. The bounded 60-second startup
run exits cleanly and preserves the signed test profile. This identifies the
mode, but live activation/appearance still needs validation. Evidence: `logs/native-locomotion-{red,expanded}.log` and
`logs/native-boost-focused.log`; source branch `fix/native-boost-timing`.

### Escort resource recovery and match dispatch

Following the resource write reaches `sub_000654E0`, called through the complete
mode dispatcher `sub_000681C0` and active-match state in `sub_00068830`.
Recovery waits until more than five game seconds since the last boost use,
then adds a fixed 0.003 at US site `0x6552D` on every match update. Unlike the
walking gate, this caller executes on fractional updates with zero integer
game ticks. The original-code regression leaves 0.1800001 after one second
at original/native 60 but 0.3599999 at 120 before the fix.

The shared addition policy scales only this charge by raw fractional time,
capped at one original charge. Game-speed scaling still affects the original
five-second delay; it does not alter recovery's rate once eligible. The original
match dispatcher owns pause suppression. The selected actor, mirrored resource,
threshold overshoot and existing health-dependent clamp are unchanged.

`native_escort_recovery.c` lifts all three complete routines, preserving their
switch tables and scalar constants. It verifies 0.18 units per second across
60/120/144, quarter/normal/triple game speed, the tick-300/301 eligibility
boundary, paused match dispatch, catch-up, selected actor and clamp/overshoot.
Restoring the original addition only at the recovery site must fail with
the original doubled 120-FPS recovery. External world, network, rendering and
pause-query services are controlled; this is not live Escort or network parity.
The asset-free clock/resource fixture also checks recovery in both-host CI.
Evidence: `logs/native-escort-recovery-{red,fixed,regressions}.log`.

## Fractional damping coefficient packing (2026-10-22)

The first diagnostic used rounded test coefficients and found scalar damping
drift. Follow-up execution of the complete current-recompiler US uploader
`sub_003D9520` exposed an incorrect assumption: its lifted `cvtss2si` uses a C
integer cast, truncating rather than rounding. The earlier 250/255 anchor and
-20.70%/+5.58% figures describe that rounded diagnostic, not the current game's
60-FPS upload. This audit now derives every packed test coefficient from the
actual re-lifted upload routine and passes those values to the GPU case.

For authored `(acceleration, displacement, damping)=(0.105,0.65,0.98)`, actual
packed bytes are `(26,165,249)` at one tick, `(13,82,252)` at half a tick and
`(11,69,252)` at 60/144 ticks. These are controlled inputs, not a captured
Scotland trajectory. The uploader's command-buffer space, definition mappings
and ABI are supplied/checked; refill is an unexpected failure.

The GPU test uses the hash-verified US second feedback shader, production HLSL
translator and common RHI, uniform float32 inputs/targets, zero acceleration
and displacement coupling, and feedback only in blue. One second from 0.6 gives:

| Coefficient path | FPS | Final blue | Relative to current original-60 path |
| --- | ---: | ---: | ---: |
| Current packed upload | 60 | 0.143780917 | -0.00001% |
| Current packed upload | 120 | 0.145008832 | +0.85400% |
| Current packed upload | 144 | 0.109154858 | -24.08252% |
| Precise anchored fractional damping | 120 | 0.143780857 | -0.00005% |
| Precise anchored fractional damping | 144 | 0.143780395 | -0.00038% |

The shared `pc_surface_coefficients` policy clamps/truncates the original one-tick
values once, scales the acceleration/displacement anchors by fractional time,
and raises the damping anchor to that time. The correction is part of the
existing opt-in bias/float/fractional experiment; normal play and the original
whole-tick/catch-up path remain unchanged. It preserves the current recompiled
60-FPS contract; the lifter's broader CVTSS2SI/MXCSR fidelity remains a separate
unverified hardware-fidelity issue and is not silently changed here.

Original parameters are copied before the guest's fractional parameter scope.
The HLE shader-state path sends acceleration/displacement/damping through the
existing ordered float-constant payloads at bank 0 stages 3/5/7, alongside the
centering value at bank 1 stage 6, only for the validated second feedback shader.
Changing shader, leaving fractional scope, or invalid parameters clears the
appropriate slots. Identical updates are cached; new parameters at the same
delta refresh the values. Queued and replayed commands own copied values.
No delayed draw reads mutable guest timing or parameter memory.

Five focused cases pass: queue/scope/copy/replay checks, three original-program
WARP cases (including actual uploader capture), and the original procedural
update/upload integration with disabled and precision/bias/live variants.
Every damping pixel/update is compared with the scalar equation. Broader
coupled/spatial dynamics, original byte-storage appearance, additional levels
and native Mac GPU/scene validation remain unverified; water remains opt-in.
The full GPU cases require the private supported XBE and Windows WARP. Hosted
skips are not GPU acceptance. Evidence: `logs/surface-damping-upload.log` and
`logs/surface-coefficients-focused.log`. No private game code is distributed.

### Coefficient build and bounded scene comparison

Source `1d02157` was integrated as `20ee848` with the independent crowd,
FPS-handoff and Escort repairs, then built with the supported Windows script.
Host: Windows 10.0.26300 AMD64, MSVC 14.44.35207 and SDK 10.0.26100.0;
the shader diagnostic uses the production common-RHI D3D11 WARP route.
All required build/native checks pass (`logs/surface-coefficients-combined-build.log`).
The selected local timing/shader/queue/preservation/statistics/patch/backlog/
context suite passes 118 tests (`logs/surface-coefficients-regressions.log`).

The built executable hash is
`9eb9a3c2ac5d80c0157f1b3bd15fbeedc31cfd34884882a021b606e2f9c0c88e`.
Three isolated runs use the existing Scotland fixture and bounded 120-second
runner, stopping after approximately 73 seconds. Original 60 has experiments
off; the 120/144 runs enable float, fractional, bias and live surfaces. All
reach scene 4, turn toward the shoreline, move, pause, resume and exit cleanly.
Profiles remain unchanged with valid signatures, fixtures are preserved, and
disposable disks are recycled. No personal session is left running.

| Path | Images | Changed adjacent pairs | Captured game ticks | First 120 ticks channel 1/2 standard deviation |
| --- | ---: | ---: | ---: | --- |
| Original 60 | 360 | 359/359 | 372.000 | 24.639 / 23.854 |
| Candidate 120 | 360 | 359/359 | 197.518 | 19.351 / 18.589 |
| Candidate 144 | 360 | 359/359 | 167.515 | 19.315 / 18.343 |

The validator checks actual GPU bytes, image hashes, consecutive guest frames
and stable resource/format/path. All shoreline stills show water without a
missing/black target. Pause/resume state and health remain correct at the
observed checkpoints. These limited observations do not establish complete
wave dynamics, continuous motion fidelity or long-term stability. In particular,
the amplitude statistics still differ. Random trajectories are unpaired and
capture durations differ; equal elapsed-window statistics do not remove that
limitation. Synchronous GPU readback invalidates ordinary pacing comparisons.
Do not enable the experiment based on this run.

Evidence: private `surface-coefficients-{60-control,120-candidate,144-candidate}`
reports/captures and `procedural-surface-coefficients-summary.json`. Each raw
23,592,960-byte trace was gzip-compressed and byte-exact roundtrip-verified
before recycling the raw file; selected PNGs and provenance are retained.
No private assets, generated code or screenshots are distributed in Git.

### Deterministic coupled feedback and storage comparison

`surface_coupling_gpu.inc` runs the unchanged production-translated second
feedback shader on D3D11 WARP. Texture 0 supplies persistent state initialized
to R/G/B = 0.5/0.5/0.65; texture 1 supplies a fixed neutral neighbor. The initial
shader inputs are identical for every run. Actual GPU render targets use float32,
half-float or UNORM8; readback feeds their stored G/B into the next update without
an extra conversion. Constants use the actual uploader anchors and the current
precise fractional helper. This tests the coupled kernel rather than only its
isolated damping coefficient.

Every pixel/update is compared with the unsaturated coupled equation. Float32
error is below 3e-6, half-float below 0.0005, UNORM8 below 0.501/255. The states
stay within the tested unsaturated range. All runs execute one second; results:

| Storage | FPS | Final green | Final blue | Changed updates |
| --- | ---: | ---: | ---: | ---: |
| Float32 | 60 | 0.521120727 | 0.446032643 | 60/60 |
| Float32 | 120 | 0.520336211 | 0.448130667 | 120/120 |
| Float32 | 144 | 0.520253778 | 0.448478669 | 144/144 |
| Half-float | 60 | 0.520507812 | 0.448242188 | 60/60 |
| Half-float | 120 | 0.519531250 | 0.452636719 | 120/120 |
| Half-float | 144 | 0.519531250 | 0.454833984 | 144/144 |
| UNORM8 | 60 | 0.521568656 | 0.443137258 | 60/60 |
| UNORM8 | 120 | 0.513725519 | 0.443137258 | 120/120 |
| UNORM8 | 144 | 0.521568656 | 0.447058827 | 144/144 |

Even with precise coefficients, float32 fractional coupling does not reproduce
the complete one-tick coupled update. Half-float adds a separately measurable
rounding effect; byte storage also changes the trajectory. The assertions
characterize these discrepancies, not accept them as full-game equivalence.
There is no random input, smoothing pass, first feedback pass or spatial field
in this fixture, so these results do not by themselves explain the full scene's
variance deficit. They identify two independent next targets for integration
work. Local evidence: `logs/surface-coupling-gpu.log`. The four original-program
GPU cases remain asset-dependent and Windows-only; hosted skips do not prove
native Mac GPU execution.

PR #298's first historical upgrade checks failed because three old-stack
fixtures selected the new coefficient patch while omitting its surface-constant
prerequisite. Corrected old-stack selection excludes both. Full current-stack
installation, fresh/repeated builds and local-edit preservation remain tested;
no runtime patch or game behavior is weakened to bypass this failure.

After the fixture repair, all four affected upgrade/fresh-install cases pass
locally in 65.37 seconds. The four original shader cases, ordered constants and
context/backlog checks pass 17 cases in 14.88 seconds. Source-only validation
needs no rebuild or additional gameplay run because runtime code is unchanged.

## Two-pass spatial feedback and full-float candidate (2026-10-22)

The new `surface_spatial_gpu.inc` extracts both original pixel programs
(0x4BBE20 and 0x4BBF10) from the verified US XBE and uses their unchanged
production HLSL/common-RHI WARP execution. The synthetic vertex shader supplies
explicit offsets observed in callback 0x2BF10: the first pass samples center,
(-1,-1), (+1,-1), (+1,+1); the second reads the first-pass center and prior
state at (-1,+1). Coordinates are texel-centered on a 64x64 clamped field.
This does not execute the original vertex microcode or verify all guest-to-GPU
coordinate conventions. It is a controlled spatial kernel, not a scene replay.

Initial green is 0.5; blue is a 0.05 Gaussian disturbance around 0.5. All
inputs are identical at all rates/formats. Each pass stores through actual GPU
float32, half-float or UNORM8 targets; decoded stored values become the next
inputs. Every pixel is checked against the unsaturated stencil. The first-pass
red is `128/255 + k*(128/255-B) + Bnw+Bne+Bse-3*B`; green/blue persist.
The second uses the already documented coupled equation and the remaining
southwest neighbor. `k` and the three feedback coefficients use current
uploader truncation anchors. No smoothing or random forcing is applied.

| Storage | FPS | Mean blue | Blue standard deviation | RMS blue vs same-format 60 |
| --- | ---: | ---: | ---: | ---: |
| Float32 | 60 | 0.567762114 | 0.001816350 | 0 |
| Float32 | 120 | 0.566613230 | 0.001811594 | 0.001149301 |
| Float32 | 144 | 0.566421304 | 0.001811069 | 0.001341274 |
| Half-float | 60 | 0.560359955 | 0.002508131 | 0 |
| Half-float | 120 | 0.546896696 | 0.005342875 | 0.013886190 |
| Half-float | 144 | 0.562387943 | 0.008247281 | 0.010135495 |
| UNORM8 | 60 | 0.604052758 | 0.024023703 | 0 |
| UNORM8 | 120 | 0.686370270 | 0.031592553 | 0.084577354 |
| UNORM8 | 144 | 0.645906118 | 0.042783565 | 0.072853017 |

All frames changed. Maximum per-pass equation/storage errors were 0.000000328,
0.000488222 and 0.001960814 respectively. The assertions characterize existing
gaps, not declare cross-rate wave equivalence. Evidence: `logs/surface-spatial-gpu.log`.

The dependent full-float candidate changes only opt-in intermediate format
selection from 0x34 to 0x4B. The common resource adapter already supports full
float; the new subsequent patch extends precision preservation's allowlist and
reverses 32-bit channel words when rebuilding guest-layout capture storage.
The GPU contract tests original byte data across both byte layouts, byte/full
promotion/demotion, exact clone reconstruction, live GPU rather than stale CPU
source, output-state retention and invalid inputs; existing half tests remain.
The cache transaction still handles queued work, failed allocation/copy and
read-first transitions. Historical old-stack tests exclude the new patch until
its preservation prerequisite is installed.

This doubles each registered intermediate's GPU storage from 8 to 16 bytes per
pixel; at 128x128, three intermediates rise from 384 to 768 KiB per resource.
Corresponding CPU capture storage also grows. The final byte texture stays
unchanged. No new game assets or normal-play setting is introduced. Full scene
motion, original-byte appearance, long-term behavior, live preset switching,
performance and native Mac GPU/gameplay validation remain pending. Legacy GL
continues to decline this opt-in experiment. Do not enable it from this fixture.

### Full-float build and scene evidence

Focused source `6035ed7` was integrated as `6a766f5` with the earlier crowd,
clock-handoff and Escort repairs. Supported Windows Build and all its required
checks pass (`logs/surface-float32-combined-build.log`); executable SHA-256 is
`c11dd28d2207fec7f40a5be2c658cf0713cf8edb2ab6535e0f4b818e290fa8de`.
Host/toolchain remain Windows 10.0.26300 AMD64, MSVC 14.44.35207 and SDK
10.0.26100.0, common HLE/RHI D3D11 renderer.

Three isolated 120-second-bounded routes stop after approximately 73 seconds,
reaching Scotland scene 4 with shoreline viewing, movement and pause/resume.
Health is 35 at the observed checkpoints. Original 60 uses normal byte buffers;
120/144 select all four existing experiment flags. High-rate logs confirm 0x4B
host intermediates. All runs exit cleanly, signed profiles are unchanged,
fixtures remain valid and disposable disks are recycled.

| Path | Images | Changed pairs | Captured game ticks | First 120 ticks channel 1/2 standard deviation |
| --- | ---: | ---: | ---: | --- |
| Original 60 | 360 | 359/359 | 375.000 | 24.396 / 24.093 |
| Full-float 120 | 360 | 359/359 | 196.138 | 18.940 / 18.147 |
| Full-float 144 | 360 | 359/359 | 164.384 | 19.014 / 18.381 |

Actual-byte hashes, consecutive guest frames and stable resource/path/format
validate every trace. Reviewed shoreline stills show water. Statistics remain
different from original-byte output; neither unpaired RNG nor short synchronous
GPU captures support full motion equivalence or a pacing/performance claim.
Full-float removes the identified storage error in the deterministic test but
has not closed the game's original-byte appearance/dynamics gap.

Evidence is private under `surface-float32-*`, with measurement/storage JSONs.
Every raw 23,592,960-byte trace was gzip-compressed and byte-exact verified before
recycling its raw file. No game assets, screenshots or generated code are in Git.
The Mac CI common-RHI GPU step passes for `6035ed7`, including the extended
conversion/capture contract; original-XBE wave tests and native Mac gameplay
remain unverified. Live preset cycles, long-duration motion and capture-free
performance remain pending for this format change.

## Crowded-map camera query follow-up (2026-10-22)

Windows x64 / MSVC 19.44 / D3D11, i7-13700K, RTX 4070 SUPER driver
32.0.16.1742. Isolated Test Lab deathmatches use ten enabled bots, recommended
weapons, Classic textures, 3440x1440, FOV 73 and native 144. Separate captured
setup runs confirm bot counts and actual Vietnam/Venice scenes. Normal combat,
death and respawn use synthetic inputs; bots and spawn outcomes are not
deterministic. No human network matches or full-map coverage are established.

The initial Venice pacing attempt reached its 15-point score limit during the
measurement window. Strict gameplay-state analysis rejected it. Its apparent
143.202 FPS is not accepted gameplay evidence. The corrected route changes only
the disposable test's score limit to None (Go → Down → Left five times; Up twice
then Enter opens Bot Set), retaining the ten-minute time limit. This lets the
same complete combat window remain active.

Every accepted pacing row covers spawn-observation+5 through +55 seconds, with
consecutive Present clock evidence on both sides and scene/match-state/pause
checks at all observed checkpoints through combat-six. No screenshot capture,
CPU sampler, parallel local tests or builds run during these timing windows.
These are successful host Present intervals, not GPU completion or scanout.

| Run | Mean FPS | p95 ms | p99 ms | Worst ms | >9 ms | Slowest 1 s |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Vietnam, prior combined build | 141.303 | 8.490 | 9.242 | 35.056 | 1.953% | 125 |
| Venice, prior combined build | 140.843 | 8.495 | 9.447 | 31.830 | 2.215% | 117 |
| Venice, projection candidate | 142.933 | 8.111 | 8.889 | 60.555 | 0.798% | 129 |
| Venice, candidate repeat | 143.240 | 8.149 | 8.702 | 11.812 | 0.572% | 137 |

The baseline is combined source `6a766f5`, executable SHA-256
`c11dd28d2207fec7f40a5be2c658cf0713cf8edb2ab6535e0f4b818e290fa8de`.
Candidate source `4adb6b0` adds PR #300 to the same earlier #295–#299 changes;
executable SHA-256
`0072d5b4651b25b886e11fa1d939e4e2ef4f5306e347422f570135c068efc7ab`.
Both use the supported builder and verified US XBE. The water experiments are
disabled. All measured presents are guest frames; none are interpolated.
Gameplay-clock/wall ratios are 0.999972 (Vietnam), 0.999932 (Venice baseline),
0.999698 and 0.999937 (candidate/repeat). Asynchronous combat/respawn endpoints
do not establish exact motion, animation or weapon parity.

A separate 200 Hz sample profile on the prior build shows 1,873/6,872 guest-main
samples in NtQueryVirtualMemory during cumulative 110–150 second reports, with
460 chains through perspective correction and 208 through camera correction.
Other folded linker names are not reliable identities. The diagnostic includes
profiler overhead and is not used as a performance result. PR #300 initializes
a local writable-region snapshot after the guest routine returns. Camera and
source matrix are checked before the output, because actual decal callers use
temporary guest-stack output in a different region. This reduces three queries
to two in the split-region case, or one in a common region, with bounds and
permissions still enforced. No AI/animation throttling or quality reduction.

Sixteen focused geometry, ABI, real-VM and cutscene tests pass. Query counts
cover both region layouts; retaining a snapshot across calls must fail. The
OS test includes read-only/no-access, crossing spans, overflow, new snapshots
and Windows guard pages. Nineteen companion cases pass, with one Mac-only
local skip. The supported final build passes its required checks. PR #300 source
`beff7a7` passes selected Windows/Mac native and transition CI, including the Mac
real-VM contract; this is not actual Mac camera/cutscene or performance acceptance.

The candidate's 60.555 ms outlier occurs 38.879 seconds into its window; its
Present body is only 0.148 ms. It does not recur in the repeat. Its cause remains
unknown; do not discard it or label this locked/hitch-free 144 FPS. The observed
average/tail improvement supports the narrow optimization, with wider gameplay,
Mac visual/performance acceptance and rare-hitch investigation still open.

Evidence: isolated `crowd-vietnam-native144`, `crowd-venice-long144`,
`crowd-projection-144` and `crowd-projection-repeat144` reports/provenance/Present
CSVs, `logs/crowd-*-matrix.json` and `*-stats.json`; rejected
`crowd-venice-native144`; separate `crowd-venice-profile` and
`logs/crowd-venice-sample-delta.json`; setup images in `crowd-vietnam-setup`,
`crowd-venice-setup`, `crowd-venice-match-rules` and `crowd-projection-visual`.
All sessions exit cleanly with unchanged valid profile signatures, preserved
fixtures, recycled disposable disks and no personal game left open.

## Coupled fractional water operator (2026-10-23)

The earlier controlled tests isolate two independent problems: fractional
coupling and intermediate storage. This candidate addresses the first without
claiming the second is solved. The game still uses its existing paths; no hook
calls the candidate, and the normal-play water experiment remains disabled.

For centered velocity `g = G - 0.5` and displacement `h = B`, the verified
one-step linear model is `v = g + a*(forcing - k*h)`,
`g_next = v + bias`, `h_next = d*(h + travel*v)`. Its linear matrix is
`[[1, -a*k], [d*travel, d*(1-a*travel*k)]]`; its offset is
`[a*forcing+bias, d*travel*a*forcing]`. The new portable helper takes the real
principal fractional matrix power and preserves the affine fixed point.
It handles the complex-conjugate and positive-real eigenvalue cases, including
the repeated-eigenvalue limit, and rejects invalid/non-real-step inputs without
publishing partial output. Timestep zero is identity and one retains the original
matrix. Input coefficients remain anchored to the actual uploader's packed bytes.

The old three-parameter scaling cannot express this operator: the required
velocity-retention coefficient is approximately 1.00824992 at 120 and 1.00802577
at 144 in the fixed-neighbor fixture, whereas the old shader structure fixes it
at one. The new shader body evaluates both coupled rows; language adapters only
supply vector syntax. These rows must operate on spatial modes, not independent
world texels with an assumed constant restoring strength.

`native_surface_coupled.c` runs through the existing Windows/Mac native-timing
suite without assets. It checks 65 restoring strengths spanning the clamped
diagonal stencil, original/native rates, 0.17+0.83 split-step composition,
identity and invalid-input nonmutation. Local maximum trajectory error is
0.0000023031 after float coefficient storage. Additional cases cover positive,
repeated and complex eigenvalues against independent original equations. The
full native-timing suite passes 67 tests in 85.65 seconds. This is a linear
numerical contract.

The Windows WARP fixture still executes the unmodified production-translated
original pixel programs and extracts coefficient bytes from the complete US
uploader. Their float32 60-step results supply the independent GPU references.
The shared candidate shader matches the fixed-neighbor result within
0.0000008345 at 60/120/144, with every candidate update changing the state.

The 64x64 spatial extension uses an orthonormal DCT-II for the same clamped
diagonal-neighbor stencil. Its mode restoring strength is
`k + 4 - 4*cos(pi*x/64)*cos(pi*y/64)`; only the zero mode receives the constant
forcing/centering offsets. The shared separable GPU transform runs before per-mode stepping, and its inverse
reconstructs the field after **every** update. The fixture reuses its basis and
scratch textures. CPU transforms remain only as independent numeric references;
asymmetric corner impulses, a checkerboard and deterministic noise exercise both
axes, the inverse transpose and high frequencies. Their maximum absolute error
is 0.0000028611. The direct 64-term-per-axis transform is bounded, but its game
frame-time cost has not been measured and no runtime hook calls it.

| Candidate rate | G/B RMS vs original float32 60 | Maximum error | G/B RMS vs original byte 60 | Fresh updates |
| --- | ---: | ---: | ---: | ---: |
| 60 | 0.0000008438 | 0.0000018478 | 0.03137490 | 60/60 |
| 120 | 0.0000009699 | 0.0000020266 | 0.03137500 | 120/120 |
| 144 | 0.0000007604 | 0.0000017286 | 0.03137483 | 144/144 |

Temporary-header mutations reset fractional velocity retention to one. Both
the fixed-neighbor and spatial GPU comparisons then fail their numeric acceptance
after successful compilation, proving that those tests detect the missing term.
An additional mutation removes the inverse-transform transpose and must fail
the GPU round-trip comparison. Both spectral state and reconstructed world
field are checked for fresh updates at every candidate frame. All eight GPU
cases pass in 30.74 seconds, including the three expected-failure mutations. Existing byte/half-float discrepancy checks remain.
No supported game rebuild is needed for these currently uncalled helpers/tests.

Outstanding: original byte-feedback appearance/rounding, clipping, smoothing,
disturbance timing, original guest geometry/viewport submission, runtime ownership/state restoration for the GPU
transform resources, runtime integration, live-rate transitions, actual
frame-time cost and native Mac GPU/gameplay acceptance. Do not enable the water
experiment solely from these controlled results. Evidence is local in
logs/water-coupling-gpu-suite.log; shader bytes and game assets are not tracked.

### Original vertex program and rounding isolation

The updated WARP fixture extracts the vertex program at US 0x4BBB90 (header
0x00062078, six instructions) and translates it with the unmodified production
vertex parser/emitter. The original program consumes v1/v2/v3, copies position,
swizzles color and adds c17-c20 to UVs. It now runs on the GPU alongside both
original pixel programs. The fixture still supplies a synthetic fullscreen
triangle/NDC position and texel-centered UVs; this does not validate the game's
actual vertex submission or viewport/half-texel handling. All controlled spatial
results are unchanged. Translation helpers use isolated runtime configuration.

Three added one-second Original-60 controls isolate byte rounding. Each pass
renders to its selected actual GPU storage format. The gradient-only control
retains the first pass's byte red result but explicitly restores copied green/
blue channels from the float input before pass two. This is an intentional
ablation, not game behavior. Each mixed path must remain distinct from the
original both-byte result (RMS blue difference > 0.005).

| Storage control | Mean blue | Blue standard deviation | RMS blue vs both-byte 60 |
| --- | ---: | ---: | ---: |
| Both passes byte (reference) | 0.604052758 | 0.024023703 | 0 |
| First pass byte, second float | 0.595155834 | 0.023109208 | 0.012476170 |
| First pass float, second byte | 0.562469389 | 0.009027910 | 0.045231253 |
| First gradient byte, copied G/B and second pass float | 0.570406683 | 0.001638243 | 0.041304309 |

The closer first-byte-only result cannot be attributed solely to rounding the
spatial gradient: it also repeatedly rounds the carried velocity/displacement.
Keeping only the quantized gradient loses that result. Mean signed rounding
errors are recorded per pass/channel, rather than treating quantization as
zero-mean noise. For the both-byte reference, first red mean error is
+0.000151594 and second green mean error is +0.000358667 per update. These are
state-dependent observations, not constants to add to the fractional solver.
Neither changing one target format nor rounding only the gradient preserves
the original byte feedback in this field. Full nonlinear/state-rounding behavior
remains unresolved. Evidence: logs/water-original-vertex.log and
logs/water-quantization-ablation.log; no game asset bytes are committed. All
eight updated GPU cases pass in 31.96 seconds (logs/water-coupling-gpu-followup.log).
### Integrated vehicle-physics build: crowded-match pacing

The later `20dc25d` build is compared with normal playable `4adb6b0` using the
same configured ten-bot Venice route, native144, 3440x1440/FOV 73/Classic and
unlimited score. Capture-free/no-sampler minimized Windows runs produce
143.803/143.901 guest presents/sec over the strict complete-interval 50-second
window. p95 is 7.737/7.782 ms, p99 is 8.116/8.195 ms, worst is 27.428/34.091 ms;
slowest full second is 141 for both. All presents are guest frames. Active
scene 46/state 7/unpaused checkpoints and clock/wall ratios 0.999959/0.999937
validate the measurement scope; signed isolated profiles remain unchanged.

This supplies integration pacing evidence after fresh physics updates, not
an individual-fix speedup, deterministic scene comparison or scanout/physical
input acceptance. It does not prove the reported 110-FPS drops are eliminated.
Both largest gaps follow E key-down after zero-health checkpoints by 67.273/
47.697 ms; their Present bodies take 0.069/0.068 ms. Respawn is a follow-up lead,
not a proven cause, and other slow frames have different timing. Source hashes,
CI scope and remaining gaps are in STATUS; raw evidence is under
`vehicle-integrated-crowd-144`/`crowd-playable-compare-144` and
`logs/integrated-crowd-comparison.json`. No runtime change or rebuild.

The subsequent read-only CPU/health diagnostic reproduces a 25.856-ms gap
after health restoration, with a 0.070-ms Present call. A follow-up observing
emulated GPU handshake words has a worst selected gap of 14.332 ms and does
not reproduce the larger event. These instrumented runs preserve isolated
profiles and verify the ten-bot roster, but neither identifies a causal routine
or proves an FPS improvement. Keep the hitch open; STATUS records the CPU
counter limitations, raw evidence and need for a timestamped stack/wait trace.

That follow-up now captures texture-file I/O beneath `tsfp_texture_upload`
in slow frames. A revised saved-stack sampler reduces median suspension to
0.0188 ms from 0.4553 ms; it remains diagnostic. Its worst selected interval is
12.701 ms versus 9.456 ms in one isolated no-pack control, with differing bot
outcomes. Neither reproduces the 25.856-ms event or proves a performance fix.
Preserve Classic/Enhanced and live switching; measure individual upload stages
before changing the loader. STATUS records all three runs and their limits.

### Captured periodic boundary and authored target size

A single bounded Original60 Scotland run (`water-runtime-state`) used the
preserved source-4adb6b0 executable, SHA-256
0072d5b4651b25b886e11fa1d939e4e2ef4f5306e347422f570135c068efc7ab, with the verified
US XBE and isolated Test Lab settings/saves. Host frame 3600 was recorded in a
9,220,188-byte version-9 D3D capture. The run completed cleanly without changed
profile hashes; signatures and fixture remain valid, and disposable partitions
were recycled. This binary does not contain the experimental solver.

The capture identifies four water draws by vertex-microcode SHA-256
363fd81ebd9f0567bbcbea7fafab686241707d85396df0a8abef78ce79bf0425, matching the
supported XBE's six-instruction program. In particular, the feedback pair uses:

- 128x128, Xbox format 6, single-level render targets and sampled inputs;
- U/V address mode 1 (wrap), min/mag filter 2 (linear);
- a four-vertex triangle strip, stride 24, positions (0,0), (0,128),
  (128,0), (128,128), Z 0.001 and UVs (0,0)..(1,1);
- viewport (0,0,128,128), depth 0..1;
- enabled screen-space undo, scale (64,-64,16777215,1), offset (64,64,0,0);
- the same first-pass diagonal and second-pass southwest UV offsets as the
  callback-derived fixture, now observed at +/-1/128.

Recorded pixel tokens are host handles, **not** guest shader addresses. The
inspection therefore selects the verified vertex program and inspects offsets,
bindings and pass order, rather than equating a host token with 0x4BBE20/0x4BBF10.
The capture's system-memory texture copies are not live feedback pixels; only
its state/geometry is used here. Private evidence: the run's water.d3dcap,
capture-provenance.json, report.json, and private/pipeline/water-runtime-draw-state.json.
No game data is committed.

The observed wrapping invalidates direct gameplay use of the earlier clamped
DCT basis. `graphics/surface_basis.h` now owns both policies: DCT-II for clamp
controls and real orthonormal Hartley weights `(cos(2*pi*k*x/N) + sin(2*pi*k*x/N))
/ sqrt(N)` for wrap. Periodic restoring shift is
`4 - 4*cos(2*pi*kx/width)*cos(2*pi*ky/height)`. The shared shader takes the axis
extent, and resources/basis weights must match it; the shared policy accepts
extents 1..128 and rejects invalid coordinates/boundary values.

The 128x128 wrapped GPU case mirrors the observed quad, normalized UVs, viewport,
screen-space undo and linear wrap sampler through the original vertex/pixel
programs. Its synthetic initial field includes a small edge pulse in addition
to the center Gaussian. This crosses the seam immediately and exposes the old
boundary error; substituting a clamped basis fails the numeric comparison.
The basis and coupled updates remain on the GPU, with a fresh reconstruction
after every fractional step.

| Periodic candidate FPS | RMS G/B vs original float32 60 | Maximum error | RMS G/B vs original byte 60 | Fresh reconstructed updates |
| --- | ---: | ---: | ---: | ---: |
| 60 | 0.0000006489 | 0.0000016690 | 0.03692987 | 60/60 |
| 120 | 0.0000007953 | 0.0000019074 | 0.03693001 | 120/120 |
| 144 | 0.0000007933 | 0.0000019074 | 0.03693000 | 144/144 |

The asset-free shared-basis fixture checks orthonormality and actual neighbor
stencils for both boundaries at 1/7/64/128; maximum error is 4.623e-14. It is
registered in the existing Windows/Mac native-timing suite. All 11 WARP cases
pass in 48.61 seconds, including the new boundary negative control and the
previous retention/transpose controls. Evidence: logs/water-wrapped-suite.log
and logs/water-authored-size.log. These remain controlled float32 results: byte
rounding/clipping, actual smoothing/disturbance inputs, runtime resource/state
integration, live rate changes, gameplay frame-time cost, other surfaces and
native Mac GPU/gameplay acceptance remain open. Normal play is unchanged.

### Original smoothing plus feedback storage reference

Follow-up PR #321 adds an Original60-only reference with the observed cardinal
smoothing pass before both feedback passes. It translates shader 0x4BBC40
unchanged and uses the original vertex program with the captured +/-1/128
cardinal offsets, pixel-space quad and wrapped linear sampling. Input remains
the controlled center Gaussian plus seam pulse; random disturbances and final
normal/shading are excluded. The fractional candidate comparison remains the
separate feedback-only contract, not a claim to match this fuller reference.

At each of 60 updates, all RGB smoothing output is checked against the explicit
wrapped four-neighbor average and the shader's nonidentity centering/color
factor. Float32 maximum filter error is 5.96046448e-8; byte error is bounded by
half a byte step. A mutation zeros all smoothing offsets and must fail the
neighbor calculation, so the test distinguishes smoothing from a copy.

| Original60 three-pass storage | Mean blue | Blue standard deviation | RMS blue vs both-byte |
| --- | ---: | ---: | ---: |
| All float32 | 0.576436446 | 0.000468331 | — |
| All half float | 0.559374392 | 0.001727153 | — |
| All byte | 0.562230514 | 0.023116937 | 0 |
| Smoothing and first feedback byte, second float | 0.629445438 | 0.040056530 | 0.080110445 |
| Smoothing and first feedback float, second byte | 0.631365151 | 0.019306744 | 0.076352467 |

Combined float32/byte G/B RMS is 0.0192863519. Thus smoothing changes the field
but does not explain away the storage difference; the full nonlinear byte
behavior still needs treatment before candidate gameplay integration. Mixed
controls use the first-feedback format for smoothing too, and are not identical
to the earlier two-pass ablations. With byte smoothing, the gradient-only
control's copied G/B are already quantized, explaining its identical result to
the first mixed row. No constant drift correction is inferred from these means.

Windows x64/MSVC/common-RHI WARP evidence is in logs/water-smoothed-reference.log
and logs/water-smoothed-suite.log; all 13 GPU cases pass in 58.56 seconds,
including the missing-offset negative control. This is controlled original-shader coverage;
no new game binary or native Mac GPU/gameplay result is claimed.

### Final ripple consumer and alpha-height encoding

The supported executable's effect table at `0x476270 + 22*16` selects callback
0x2C320, used by final routine 0x195280. That callback binds pixel definition
0x4BC318 and the same six-instruction vertex program, uploads packed .6 red/
green strength and selects left/right/down/up texel offsets. This matches the
fourth water draw in the retained Original60 capture. The final shader derives
its slopes from **texture alpha**, not directly from the blue height channel.

The spectral fixture previously reconstructed `(velocity,height,0,1)` solely
for numeric inspection. Feeding that texture directly to the original final
pass would make its alpha differences zero. Shared `surface_state_shader.h`
materializes `(0,velocity+0.5,height,height)`; red is scratch regenerated by the
first feedback pass. It does not replace the original final shader or clamp
the stored state. The storage adapter owns precision. No game hook uses it yet.

`surface_final_gpu.inc` executes the unmodified production-translated final
shader and original vertex program. An independent calculation checks its
cardinal alpha differences, staged clamping, packed strength and centering.
Float output tolerance is 1e-5; actual byte target tolerance is half a byte
step (0.501/255). Replacing alpha-height with alpha=1 must fail after successful
compilation. The fixture also materializes and executes final ripple generation
after **every** candidate fractional update, checking fresh red/green output.

The 128x128 wrapped feedback-only candidate's final float-reference maximum
errors at 60/120/144 are 1.084805e-5, 1.090765e-5 and 1.031160e-5 respectively.
Fresh final outputs are 60/60, 120/120 and 144/144. This extends the controlled
linear comparison through its original consumer; it does not solve byte feedback.

For the Original60 smoothing-plus-feedback reference, final red/green RMS from
the flat encoded slope is 0.000786400 (float feedback), 0.004454691 (half) and
0.057356713 (byte). Float/byte final-output RMS is 0.057236962, and remains
0.057267247 when both final outputs use an actual byte render target. Thus the
remaining storage difference affects the generated ripple shading input and is
not solely a uniform height offset. Full material/scene visibility, disturbances,
runtime resource/state handling and capture-free GPU cost remain unverified.
Evidence: logs/water-final-initial.log and logs/water-final-controls.log;
the latter passes three cases, including the constant-alpha negative control.
The complete updated suite passes all 14 cases in 72.15 seconds
(`logs/water-final-suite.log`). Candidate versus float-reference final byte RGB
has zero maximum difference in the tested 64/128 clamp and 128-wrap fields at
60/120/144. This equality compares float feedback paths, not original-byte state.

### Held rounding input and authored byte boundaries

The follow-up keeps the first-pass quantization event at its original boundary,
while the coupled field evolves at each native frame. Starting from byte G/B,
the first pass's copied channels already have the correct precision. Its red
residual `e = byte_red - unclipped_linear_red` is an additional spatial forcing
input. The one-tick contribution is `(a*e, d*b*a*e)`; the fractional response
comes from the same affine operator. Shared `pc_surface_coupled_forced_step`
stores this response in the fourth component of its two rows, and
`TSFP_SURFACE_FORCED_SHADER` applies the transformed residual. That input stays
fixed until the next authored boundary; it is not re-rounded every native frame.

`surface_rounding_gpu.inc` executes the original first-pass byte format and the
shared unclipped gradient to measure the residual on the GPU, transforms it and the live state, and advances each
fractional piece. A 144-Hz frame crossing a 60-Hz boundary splits into two pieces.
At the boundary the persistent state is written to an actual byte target.
Original smoothing, when selected, executes at authored boundaries too. The
fixture starts by encoding its initial Gaussian/edge pulse into a real byte
target and computes an independent original 60-step reference. Unlike earlier
storage ablations, the first input here is already byte-representable.

The initial raw GPU attempt diverged at authored tick 14: blue was
0.566666483879089 versus original arithmetic 0.566667437553406, straddling a byte
midpoint. The small numerical difference became a full byte step and grew over
later iterations. Private evidence: logs/water-rounding-flow-unresolved.log.
An independent double-precision numerical model matched the byte-initialized
trajectory, with and without smoothing; that model is supporting evidence only,
not the GPU acceptance result or an implementation used by the game.

Shared `surface_rounding_shader.h` guards ambiguous conversion. It uses original
second-pass arithmetic evaluated from the held first-pass byte result and the
interval's input state **at the boundary**. It substitutes that scalar only
when both `abs(candidate-original)` and distance to a half-byte threshold are
within `32 * FLT_EPSILON` (3.814697265625e-6 in normalized units). Other values
remain the integrated candidate; the render target performs the byte conversion.
This is a bounded numerical rounding decision, not an unconditional state reset
or interpolation of saved frames. Direct cases check close ambiguity, rejection
of a distant replacement, preservation away from thresholds and out-of-range
values. Removing this handling or the held residual must fail.

The initial GPU run matches original byte G/B exactly at **every** authored
boundary in all six schedules (60/120/144, smoothing off/on). Maximum arithmetic
disagreement before conversion is 3.09944153e-6. CPU tests independently compose
held forcing through 5/12 + 5/12 + 2/12 pieces for 65 modes and three signed
residuals, and check invalid-output preservation. This is progress on the storage
gap in one controlled field; disturbances, intermediate clipping, live-rate
transitions, runtime resource/state ownership, game GPU cost and Mac GPU/gameplay
remain unverified. Existing normal-play water is unchanged.

The complete updated WARP suite passes 17 cases in 83.64 seconds
(`logs/water-rounding-flow-suite.log`). All six held-rounding schedules also
match the original final byte ripple RGB exactly. Fresh byte ripple outputs
are 60/60, 120/120 and 144/144 with smoothing both off and on, so freshness is
checked after final byte conversion as well as in the internal float field.
The two new required-failure controls pass, and the focused portable coupled
test passes in 1.77 seconds. No full game build or frame-time claim follows.
PR #321 subsequently reached main through #322. The held-rounding follow-up is
on `fix/native-water-rounding`, rebased onto 2252af1, with no new PR yet.
Applying the full integrated patch stack updates nine runtime files; repeating
the GPU suite then passes all 17 cases in 82.79 seconds, and the focused native
coupled check passes in 1.15 seconds. Evidence: logs/water-rounding-main-suite.log.

### Strong first-pass clipping and mixed frame rates

A stronger initial field (height 0.5 with +0.45 at (1,3), -0.45 at (96,23),
zero signed velocity, encoded into an actual byte target) exposed missing
first-pass clipping. The earlier residual subtracted the already-clipped float
shader result and therefore corrected only rounding. It diverged immediately:
tick 0, cell 385 green was 0.313725501 versus 0.407843143. Over one second,
maximum byte-state error reached 0.192156881 and final byte RGB error 0.450980425
(`logs/water-impulse-before.log`).

`surface_gradient_shader.h` now computes the unclipped linear gradient used by
the coupled operator. Actual byte red minus that gradient includes clipping as
well as rounding. The strong and original mild fields both match every authored
G/B boundary and final byte RGB at 60/120/144, smoothing off/on. A mutation that
clamps the shared gradient again must fail the original-byte comparison.

The fixture also uses a 720-unit clock to run 37 frames at 144, 30 at 120, 15 at
60, then 35 at 144: 117 frames over one second. All three changes occur with an
authored tick partly elapsed. Phase and held residual survive these changes;
all 60 boundaries and final byte RGB match, and all 117 final byte ripple frames
are fresh for both fields and both smoothing choices. Across all 16 field/rate/
smoothing combinations, maximum pre-quantization disagreement is 3.21865082e-6.
The five focused cases pass in 33.83 seconds (`logs/water-impulse-switch.log`).
The full shader suite passes all 19 cases in 97.52 seconds
(`logs/water-clipping-full-suite.log`).

This tests synthetic initial impulses, not the game's disturbance shader or
arbitrary velocity/height saturation in the second pass. The integer fixture
clock does not validate the host-counter clock's runtime integration. Shared
helpers remain uncalled by the game; resource ownership, actual live setting
changes, GPU cost, scene behavior and Mac execution are still pending.

### Second-pass acceleration and height limits

A third initial field uses signed velocity +0.49 and height 0.98 on one half
of the 128-square map, -0.49 and height 0.02 on the other. The existing candidate
failed at tick 0: green 0.843137264 versus 0.890196085, growing to maximum byte
state error 0.976470590 and final byte RGB error 1. The unchanged production
translation shows a signed [-1,1] clamp after adding the fourth neighbor to the
first-pass red acceleration, and an upper height clamp before damping.
Evidence: logs/water-saturation-before.log.

Shared `surface_feedback_residual` now computes the difference between that
clamped acceleration and the unclipped linear stencil. The coupled field
integrates this held residual. At authored boundaries, `surface_limit_boundary`
caps signed velocity to [-0.5,0.5] and height to [0,damping], before guarded byte
rounding. The height upper bound represents the original pre-damping clip;
clipping only to 1 after damping is not equivalent. These limits do not run
after each fractional piece. The original reference shaders remain unchanged.

All 24 trajectories (three fields, four rate schedules, smoothing off/on) match
every authored byte G/B boundary and final byte RGB. The extreme field repeats
some final colors at 60 Hz: 53 changed frames without smoothing, 52 with it,
matching the independently executed original exactly. Every 120/144/mixed-rate
frame still changes the final byte ripple output (120/120,144/144,117/117).
The freshness test now measures the original instead of requiring artificial
color changes on its saturated 60-Hz frames. Mild and impulse fields retain
the every-frame requirement at all rates.

Removing the acceleration clamp or authored boundary limits must fail the byte
comparison, independently of the existing rounding/clipping controls. The eight
focused GPU cases pass in 54.18 seconds (logs/water-saturation-controls.log).
The complete shader suite passes all 22 cases in 117.52 seconds
(logs/water-saturation-full-suite.log). Context and whitespace checks pass.
This is controlled saturation coverage, not actual disturbance or game-scene
validation. The helper remains uncalled by gameplay; no runtime scheduler,
resource owner, Mac GPU or game performance claim follows from these results.

### Original texture-noise input before smoothing and feedback

Pinned dispatcher `1956A0` swaps the current target, calls queued disturbances
`1953B0`, then texture noise `1952D0`, smoothing `1950B0`, feedback `195110`
and final output `195280`. The noise callback resolves texture 0x365, consumes
two RNG results for UV offsets and draws effect 19 (`2BE30`). That effect uses
pixel definition `4BBD30` and the four-instruction vertex program at `4BBBF8`.
The vertex input mask is 0x010e; input 8 supplies unused secondary UVs, captured
as (0,0,0,1). This differs from the feedback program's mask 0x000e.

The preserved Original60 Scotland frame-3600 capture includes the noise draw.
Its source is immutable texture ID 4, Xbox A8L8 format 0x1a, 128x128, usage 0;
unlike feedback target snapshots, its captured system-memory texels are usable
source data. State is ONE/ONE additive RGB blending, RGB write mask 7, linear
wrap sampling. The pixel shader multiplies source RGB by (0,1,0), so the draw
adds luminance to green and preserves red, blue and alpha. Observed UV origin
is (0.506488204,0.382273525), with a full pixel-space quad. Private evidence:
private/pipeline/water-disturbance-draw-state.json and its inspection script.

`surface_noise_gpu.inc` executes the original translated pixel/vertex programs
with that texture and state before smoothing at each authored boundary. The
test varies UV offsets deterministically; it does not claim to reproduce the
game's RNG sequence. A candidate-only duplicate draw is a required failing
mutation. Initial draws additionally check nonzero additive green and unchanged
red/blue/alpha. The asset is read from the private capture into temporary test
files, never tracked. Missing captures cause an explicit skip, not a pass.

The two focused cases pass in 14.56 seconds (logs/water-noise-integration.log).
All eight noise trajectories match every original byte G/B boundary and final
byte RGB, with maximum pre-quantization disagreement 2.56299973e-6. Freshness is
60/60 without smoothing and 27/60 with it at original 60 Hz, identically in the
candidate; high and mixed rates produce 120/120,144/144,117/117 changed outputs.
These are shader/input/order checks, not queued gameplay splash geometry, actual
RNG scheduling, runtime integration, scene acceptance or game GPU performance.
The complete updated suite passes all 24 cases in 132.19 seconds with no skips
(logs/water-noise-full-suite.log), including the nonzero-green/channel-preservation
checks. Repository context and staged whitespace checks pass.

### Exact clock intervals and pause-preserving splits

The production native clock publishes float deltas for guest motion and integer
ticks for timers. Summing those float deltas into a double still yields only
59.999998569489 after one second at exactly 144 Hz, so deriving authored
boundaries from that sum would miss the 60th tick. `PcNativeStep` now also carries
scaled host counter units, starting game phase and units per tick. The shared
guest wrapper publishes that snapshot as `g_tsfp_native_step`, clearing it in
Original mode. No guest field, ISR, float delta or whole-tick policy changes.
Invalid/null clocks and unrealistically large frequencies are rejected before
mutation, keeping the bounded scaled arithmetic within int64 range.

Shared `pc_surface_interval_split` consumes exact units, marks starts/ends and
emits zero to four fractional pieces. A pause emits no pieces and retains phase;
rate changes require no phase reset. Four pieces cover a three-tick catch-up
that begins inside a tick. The portable test independently accounts for consumed
units/boundaries at three frequencies, with jitter, changing presets, pause,
stall clamping, frequency reset and rejected-output preservation. Resetting phase
on entry is a required failing mutation. Four focused tests pass in 5.37 seconds,
and all 18 asset-free timing cases pass in 23.70 seconds.

The original-shader GPU fixture now obtains pieces from this production clock
and splitter, using a controlled 72-MHz source. It inserts a half-second pause
during motion and requires zero slices plus bit-identical final ripple output,
then resumes the existing boundary/color/freshness comparisons. Counter jitter
is tested on the CPU; live scheduling and resource lifecycle are not implied.
The snapshot precedes guest-local time overrides, so a future runtime consumer
must respect those scopes and initialize held input/state for newly created or
reset resources. There is still no enabled coupled-water game hook.
All 24 GPU cases pass in 133.40 seconds with this producer/splitter and pause
check (logs/water-clock-gpu-suite.log); all 32 field/rate/smoothing trajectories
retain byte-exact authored boundaries/final colors. The separate frame-clock
suite passes nine cases in 10.48 seconds (logs/water-clock-flip-gate.log).
Repository context and staged whitespace checks pass. These are Windows results;
the new shared CPU test is registered for Mac CI, not locally verified on Mac.

### Immutable mode data for variable frame intervals

The prior GPU fixture constructed two complete coefficient images per distinct
fractional duration. That finite cache worked for controlled constant presets,
but real host intervals vary. Using it directly in a runtime owner could rebuild
thousands of coefficients and allocate/upload images whenever a duration changed.
`surface_mode.h` now packs each mode's eigenvalue data and fixed points once.
`surface_evolve_shader.h` forms the fractional matrix and held-forcing response
on the GPU from those immutable values and a per-step delta/scale uniform.

The two 128x128 float4 mode images contain 512 KiB of coefficient pixels; each
step changes only a 16-byte buffer (delta and damping^(delta/2)). The CPU no
longer constructs a matrix for every mode on each new duration. Positive packed
angle selects complex roots, negative selects real roots, and zero selects the
repeated-root limit. Both fixed points retain the original affine centering and
additional held residual. Existing clipping, quantization and authored-event
boundaries remain unchanged. No interpolation of stored future frames is used.

The portable fixture checks all three eigenvalue branches against the existing
operator and independent one-tick equations, including irregular .413/.001/.407/
.179 pieces. It observes maximum error 2.02655792e-6 and rejects invalid/overflowing
packed outputs without mutation. Four focused CPU checks pass in 5.47 seconds;
the final packing case including overflow rejection passes in 1.20 seconds.

The GPU fixture executes the shared shader for all four fields and adds a fifth
schedule: 144 frames with alternating +/-0.1-ms intervals, totaling one second.
All 40 field/rate/smoothing combinations match every original byte G/B boundary
and final byte ripple RGB. All high/mixed/jitter schedules retain fresh final
output every advancing frame, and the mid-motion pause remains unchanged.
Maximum pre-quantization disagreement remains 3.21865082e-6. Ten focused GPU
cases pass in 72.16 seconds (logs/water-mode-gpu.log). Held-residual removal
mutates the new shader; a separate diagonal-entry mutation targets its matrix.

This removes coefficient reconstruction/allocation from the candidate's update
loop; it does not establish a measured game speedup or acceptable total GPU cost.
The direct spatial transforms and their integration cost still require profiling.
The RHI explicitly permits a self-contained pass to leave shaders, layouts,
state objects, uniform buffers and samplers changed because game draws rebind
them; output targets/viewports must be restored and temporary textures unbound.
Resource lifecycle, original queued disturbances and the supported gameplay hook
remain unfinished, as do native Mac execution and scene acceptance.
The full updated GPU suite passes all 25 cases in 141.24 seconds, including the
new matrix-diagonal mutation (logs/water-mode-full-suite.log). Repository context
and staged whitespace checks pass. The byte ambiguity guard was not widened.

### Initial D3D11 hardware gate (2026-10-24; superseded below)

The candidate's WARP comparisons do not establish hardware correctness. Running
`python tests/test_surface_shader_feedback.py --hardware-check --profile 0` on
the NVIDIA GeForce RTX 4070 SUPER (vendor 10de, device 2783, feature level b000)
fails the existing boundary comparison at tick 0, cell 0, green: 127/255 versus
the independently executed original's 128/255. Candidate prequantized green is
0.500199913978577 versus original float-target 0.500999629497528. Across the
first 60-tick trajectory, maximum byte G/B error is 0.0901961029, arithmetic
error 0.00759711862 and final byte RGB error 0.615686297. The original-noise
profile also fails from that first cell, with final RGB error 1. Root cause is
not established; this is neither accumulated clock drift nor a reason to widen
the arithmetic guard. See logs/water-hardware-current-check.log and
logs/water-hardware-check.log. The first broader hardware attempt also failed
an older float/half storage probe before reaching the candidate; that control
is tuned for WARP. Explicit hardware modes now select only candidate setup and
its reference, while regular pytest continues to run all the original probes.

`--benchmark --profile 3` uses hardware timestamp/disjoint queries, recorded
around the same original/candidate draw sequences. Query creation and all query
polling happen outside the measured sequence; first-noise and per-frame image
readbacks are omitted only in this mode. It still checks final byte output and
exits unsuccessfully if mismatched. The initial run measured 0.009899 ms mean
original and 0.106240 ms candidate GPU timestamp span at 60; it failed final
output and therefore never measured 120/144. Spans include submission bubbles,
exclude initialization/readback, are not game frame times, and do not establish
acceptable production cost. The bounded helper records adapter identity and
rejects disjoint/zero clocks or ten-second query timeout. The full WARP suite
still passes 25 cases in 141.81 seconds (logs/water-hardware-harness-suite.log).
Resource/hook integration is deferred until the first hardware step is correct;
queued splashes, actual event delivery, Mac GPUs and scene coverage remain open.

### Hardware sampler and conversion corrections (2026-10-24)

The initial hardware-only fixture reused sampler creation but skipped the older
storage loop that bound the four wrap samplers. This was an invalid reference
configuration, not evidence of a first-step production shader defect. All entry
points now bind those samplers in common setup. This removes the first-tick
failure, but the correctly configured mild trajectory still failed at tick 21,
cell 1146, blue: 165/255 versus 166/255. Candidate prequantized value was
0.649169504642487, original float result 0.649170935153961. Error was within the
existing arithmetic guard, but neither value was near the ideal 165.5/255
threshold, so the old correction did nothing.

A direct passthrough-shader sweep to the same hardware byte target observes a
transition between inputs 0.649169862270355 and 0.649169921875. The ideal
threshold is 0.649019607843137. This establishes the old threshold assumption
was false on this GPU; it does not establish a universal hardware conversion
algorithm. The probe remains a bounded optional diagnostic and reports actual
transitions without asserting other GPUs share them.

Shared `surface_rounding_shader.h` now canonicalizes to the original shader's
arithmetic only if disagreement is <=32*FLT_EPSILON, independent of predicted
byte thresholds. This correction still runs only at authored boundaries;
fractional evolution remains independent. The tolerance is unchanged, and a
larger disagreement is preserved for failure detection. Direct cases exercise
the observed nearby pair and a larger rejected difference. An unconditional
copy mutation must fail; existing missing-forcing, incorrect matrix, clipping,
noise and raw-rounding controls remain.

All four profiles pass on RTX 4070 SUPER (driver 32.0.16.1742), ten schedules
each with smoothing off/on, 60/120/144, mixed nonzero-phase switches, jitter and
mid-motion pause. Every authored G/B byte boundary and final RGB byte matches
the independently executed original reference. Maximum arithmetic error is
3.21865082e-6; original saturated/noise repeat counts differ from WARP, but the
candidate preserves this adapter's counts at 60 and all higher/mixed schedules
remain fresh every advancing frame. Logs: water-hardware-canonical.log and
water-hardware-canonical-{1,2,3}.log.

The corrected, capture-free noise-profile benchmark passes every final-image
check, running without parallel tests. Hardware timestamp means (ms) are:

| Path | Smoothing off | Smoothing on |
| --- | ---: | ---: |
| Original 60 | 0.010121 | 0.011844 |
| Candidate 60 | 0.107264 | 0.108201 |
| Candidate 120 | 0.070852 | 0.072243 |
| Candidate 144 | 0.075883 | 0.076961 |

Candidate 144 p95 is 0.140288/0.143360 ms; some frames cross authored boundaries
and execute additional transforms. Log: water-hardware-canonical-cost.log.
This single bounded run includes submission bubbles and covers one 128-square
field; it is not game FPS, a sustained pacing measurement, other-GPU evidence,
or a speedup over Original. Resource ownership/hooks, queued splashes, runtime
event order, scene acceptance and native Mac execution remain required.
The full WARP regression suite passes all 26 cases in 146.83 seconds, including
the unconditional-copy negative control (logs/water-hardware-canonical-suite.log).
Repository context and whitespace checks pass; no game build was replaced.

### Coupled GPU resource ownership (2026-10-24)

The trajectory fixture now uses shared `graphics/surface_gpu.h` for the coupled
kernel's resource lifecycle and draws. Each instance owns immutable mode/basis
textures, two spectral states, held forcing, world and scratch fields, shaders,
a 16-byte step buffer and raster/depth/blend states. At 128x128 its eight float4
images contain 2 MiB of pixel data, excluding other resources and driver overhead.
No per-duration cache is allocated. Original event, smoothing, quantization and
final shading passes remain caller-owned; guest resource/event integration is
still absent.

Create/begin/advance/reset/destroy are render-thread operations. Begin accepts
full single-mip sampled float4 fields of matching shape, copies external state,
and rejects internal aliases that would be overwritten. Zero time changes no
pixels; invalid time/storage/aliases leave the state untouched. Reset blocks
advancement and invalidates the output accessor until initialization. Destruction
must precede device destruction. Each draw scope preserves color/depth targets
and all viewports and clears texture slots 0-3 and its temporary step buffer.
The current RHI restore adapter skips an empty viewport list, so the owner
explicitly restores zero viewports too. Other pipeline bindings may change,
as allowed by the RHI's normal-draw rebinding contract.

`surface_owner_gpu.inc` verifies these behaviors on WARP/hardware, including two
distinct reset/input cycles, multiple/empty viewport lists, target identities,
an untouched sentinel color target and actual D3D11 binding queries. Missing
reset/output restoration mutations must fail. `native_surface_gpu_owner.c`
injects failure at each of 32 RHI allocation calls, requires complete typed-handle
cleanup, then recreates the owner. That asset-free fixture is registered in the
existing dual-host timing group; it does not execute GPU shaders.

All 40 RTX original-byte trajectories still pass through this owner. The full
GPU suite passes 28 cases in 159.55 s and the asset-free timing group passes 20
in 25.48 s. Final empty-viewport/output-accessor checks additionally pass the
hardware profile-0 run. Logs: water-owner-lifecycle.log,
water-owner-final-lifecycle.log, water-owner-hardware-{1,2,3}.log,
water-owner-gpu-suite.log and water-owner-host-suite.log. No native Mac execution
or game build is claimed.

Two isolated no-readback timing runs pass endpoint comparisons but show variable
144-Hz means (0.075534 to 0.739911 ms across conditions). A read-only 100-ms GPU
clock trace during the repeat observes P8/P5 low-clock states and P0 at 2520 MHz;
it cannot assign a clock to each measured frame. Logs: water-owner-cost.log,
water-owner-cost-repeat.log, water-owner-clocks.csv. These short workloads are
not a controlled before/after performance comparison. No power/clock setting was
changed. Sustained scene measurements, caller hooks and actual queued disturbance
delivery remain required before enabling this path.

### Bounded GPU water resource identity (2026-10-24)

`graphics/surface_gpu_cache.h` supplies a render-thread registry for the five
resources allocated by the guest constructor. An entry retains the four source
RHI images, preventing freed pointer addresses from masquerading as the old
identity. Repeated resource/generation/configuration/image requests reuse an
owner with no allocations. Changed identity builds a replacement before releasing
the old owner; any failure returns NULL and leaves the old entry intact. The
caller must fall back for that failed request, not use the mismatched old state.
The registry refuses a sixth resource rather than evicting a potentially active
one. Start zeroed, clear before scene/device reset, and advance generation when
initializing a resource in place.

The asset-free cache fixture checks reuse, generation/configuration/source
replacement, every allocation failure during replacement, image retention,
capacity rejection and clear/recreation. The shared fake RHI has no shader
emulation. The actual GPU trajectory fixture reacquires every frame, replaces
the generation between smoothing runs and releases caller references before
cache cleanup. These are resource-lifetime checks, not actual level-reload or
gameplay evidence. Guest/HLE construction/reset and deferred-command integration,
original event delivery and local timing remain unwired. No new game build or
Mac run is claimed.

Validation: the asset-free timing group passes 21 cases in 25.68 s and the WARP
suite passes 28 in 160.72 s. All 40 RTX 4070 SUPER trajectories pass with exact
authored byte boundaries/final output through the cache. Logs:
`water-cache-host-suite.log`, `water-cache-gpu-suite.log` and
`water-cache-hardware-{0,1,2,3}.log`. No performance comparison was run for this
change; the previously documented isolated-clock confound remains unresolved.

### Completed gameplay feedback captures (2026-10-24)

`TSFP_SURFACE_CAPTURE` observes original feedback `195110` using a copied guest
identity snapshot and a callback in the same HLE deferred stream, immediately
after the original two draws. The callback matches already-rendered host textures
by VA/data/format/size and reads their actual RHI pixels. No guest pointer is
retained or texture created by the capture. Unsupported storage/identity/readback
or file I/O stops the probe; captures are bounded to 16 records. This closes the
specific evidence gap caused by stale CPU copies of render targets in older D3D
captures. It does not install the new solver as a game update.

The supported Windows builder passes with executable SHA-256
`60e6dedf3f164ef06004ed63dc970e28f11f2d72259ff0f5296fd8fdf8958023`, from base
`0cec704` plus this capture change. Both bounded, isolated Scotland routes
complete cleanly with unchanged valid Test Lab profiles. `water-live-feedback`
uses native144 and captures host frames 6822-6858; `water-live-feedback-original`
uses Original60 and captures 3034-3049. Both cover level ticks 300-315 and have
16 distinct input fields. Metadata stores the actual authored coefficients
(0.17, 0.105, 0.65, 0.98); the translated byte constants remain 43, 26, 165, 249.
Each 3,147,208-byte TSWATER1 file contains input/first/output RGBA8 triplets.
Hashes and source/executable provenance are retained locally, with the summary
in `private/pipeline/live-water-feedback-validation.json`.

On RTX 4070 SUPER, original-shader replay matches first-pass red and second-pass
green/blue/alpha exactly on all 32 fields. The shared coupled GPU owner also
matches each captured endpoint under 1, 1/2 + 1/2, and 5/12 + 5/12 + 1/6 steps.
Both capture cases pass in 12.13 s (`water-live-gpu-final.log`). An initial WARP
replay differed from these hardware bytes by one byte; cross-adapter UNORM
transitions are already known to differ. Hardware capture tests therefore use
local hardware; the separate WARP suite keeps its own original shader reference.
The failed cross-adapter comparison remains in `water-live-gpu.log`, and no
numerical tolerance or candidate algorithm was changed to hide it.

The CPU collector test verifies copied input/first/output order and safe rejection
without partially publishing a snapshot. The asset-free group passes 22 cases
in 27.50 s. GPU readback and extra tests synchronize/stall the GPU: these sessions
are not evidence of in-game speed or pacing. Captured fields include whatever
original disturbances/smoothing preceded them, but the run does not establish
queued splash coverage or equal event/RNG ordering across FPS choices. Runtime
fractional updates/final shading, local time scopes, resource resets, performance
and native Mac validation still need integration and acceptance checks.

All 21 patch-update scenarios pass in 542.79 s (`water-live-patches.log`),
including historical upgrades and branch switching. The capture patch follows
the existing surface patches explicitly; historical test stacks exclude it
until its dependencies are present. The supported Windows build's bundled
settings/save-path checks also pass. These host checks do not establish native
Mac compilation or rendering of the capture extension.

### Shared solver in the live deferred renderer (2026-10-24)

`pc_surface_gpu_begin_bytes` moves byte unpacking and the original first-pass
residual into shared owner-managed shaders and storage. It validates source and
first-pass sampled views before drawing, rejects aliases/unsupported storage,
and requires the authored 128/255 center. The ordinary float-input API remains.
The owner now has nine float4 images (2.25 MiB of pixel payload at 128 square)
and 37 allocation stages, all covered by failure cleanup and cache replacement
tests. Controlled GPU trajectories call the new entry directly; captured replay
keeps its independent setup to check it.

`TSFP_SURFACE_GPU_CAPTURE=1` invokes the owner using actual source views after
the original feedback in the deferred stream. It initializes and advances
5/12 + 5/12 + 1/6, reading the first-piece and full-tick results into a TSWGPU1
sidecar. Creation/destruction stay inside each diagnostic callback. This is a
bounded integration proof, not the persistent cache path or visible-water
replacement. The original guest events and GPU feedback/final passes still run.

The supported Windows build and bundled checks pass. The source is `15d774c`
plus the shared byte-input/probe extension; executable SHA-256 is
`5db64564f6663cf1f6a4b10ebf801774d6f82c2746a7031db8a2fbdec03a9940`.
`water-runtime-gpu` ran the bounded native144 Scotland route, exited cleanly,
preserved profile hashes/signatures and recycled disposable disks. Sixteen
records span host frames 6837-6864 and level ticks 300-317. The source byte file
is 3,147,208 bytes and the paired float file is 8,390,088 bytes; hashes and exact
source identities are in its local `capture-provenance.json`.

The 31-case GPU suite passes in 182.59 s. Its live runtime case checks copied
metadata, finite values, and 32 intermediate/endpoint fields against the
independent replay within the unchanged 32-float-epsilon arithmetic bound.
This is float-field agreement, not a claim that the raw runtime float field was
byte-identical without boundary conversion. Existing original shader/byte
comparisons remain exact. CPU owner/cache tests pass in 2.84 s and inject every
one of the new 37 allocation failures. Logs: `water-byte-entry-gpu.log`,
`water-runtime-owner-cpu.log`, `water-runtime-gpu-build.log`,
`water-runtime-gpu-game.log`.

The probe allocates and synchronizes readbacks per sample, so its frame times
are excluded from performance claims. Actual native time-scope/event scheduling,
persistent resource reset/reuse, final-shader hookup, visible motion acceptance,
sustained performance and native Mac execution are still required. No new PR.

The 16 captured local whole counts match the clock trace, and local deltas match
its printed values within 5e-10 (`clock-scope-comparison.json`). This observes
one route, not every guest-local time scope. The allocation/readback probe causes
one three-tick catch-up: original `1956A0` gates on nonzero ticks but executes its
feedback pair only once. Consequently the one-tick probe comparison is not proof
of correct elapsed-time integration during stalls. The normal runtime connection
must explicitly handle that distinction and preserve event/RNG call counts.


## Character-effect dispatcher subticks (2026-10-24)

US1260C0 is called by player/NPC dispatch (NPC passes actor+C68). Its first
whole-tick gate drops fractional time before nested effects which already use
the float interval. The original11FEC0 timer reproduces0.5 elapsed seconds at
120 for a one-second interval. The shared numeric hook now uses positive live
time at this gate, retaining original inactive behavior and paused zero-time.
The new original-dispatcher/matrix fixture sweeps rates through240, quarter
speed and carried phases, with an old-gate control. Nested effect systems other
than11FEC0 remain spies. STATUS records the Windows build and actual scene
coverage; do not infer complete effect rendering or Mac acceptance from it.

## Character status3 growth (2026-10-24)

Original125FE0/120AB0 repeatedly scale the live effect matrix. Linear factors
split by FPS gave X/Z scale2.213807 at120 versus2.202338 atOriginal60 after
half a second. Only constructor call120BBF now splits the original one-raw-tick
factor geometrically at the current game speed. Original/integer/catch-up
arithmetic, rotation, timer/cleanup and stack arguments are preserved. The
original dispatcher/matrix fixture compares all16 entries across rates, speeds,
jitter and live changes; old-path control still differs. This is model-transform
evidence, not proof of the effect's complete rendered appearance. See STATUS.
