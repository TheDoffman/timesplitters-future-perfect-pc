# Native high refresh development

The `feature/native-144-fps` branch adds an opt-in path that runs the title's
own update and rendering loop at a 60, 120, 180 or 240 FPS target. Without a saved
FPS preference, normal play keeps main's fixed-60 default. The old interpolated presentation engine and Smooth
motion setting remain retired.
The initial branch was merged into main through PR #285 (`8f3b422`);
focused follow-up branches retain the same high-refresh goal and beta status.

Build this branch with the normal `tsfp.ps1 -Action Build` workflow. On Windows,
open `Play-Native.cmd` for the 120 FPS target. Add `180` or `240` for those targets;
`Play-Native.cmd 60` provides a native timing comparison at the original rate.
These launch defaults do not rewrite saved graphics preferences.

In **Settings → Video Options** (also in paused Options),
choose **60 FPS (Original)**, **120 FPS (Beta)**, **180 FPS (Beta)** or
**240 FPS (Beta)**, then select
**Apply**. The change takes effect on the next game update without
restarting, and the FPS counter reflects the active target. All targets above 60 retain their Beta labels. The counter displays actual measured FPS, not rounded preset steps. Original
60 FPS is the compatibility fallback. Back discards an unapplied choice; a
save failure leaves the active rate unchanged.
The selection lives in `pc-frame-rate.ini` beside `pc-display.ini` (or beside
input settings when no display path is specified), independently of resolution,
FOV, texture choices and saves. A valid saved choice takes precedence over
launcher/environment defaults on the next startup, including `Play-Native.cmd`.
Saved 144 FPS preferences load as 120 FPS without rewriting the file until an explicit save.
The legacy 144 environment override remains available for regression comparisons.
Invalid/missing preferences retain the existing launch behavior. Automated
rate comparisons must use isolated settings directories without a saved FPS
choice unless they are explicitly testing preference precedence.

**Video Options** groups aspect ratio, resolution, FOV, texture style, subtitles and
frame rate on one page. All controls stage changes until the single **Apply**
button is selected. It stays gray when the draft matches the active settings,
including after a successful apply. A resolution preview keeps its 15-second
confirmation; accepting it commits the entire batch. Reverting it restores the
resolution and keeps the other drafts pending. Failed saves restore the active
display settings and do not publish pending FPS/texture changes. FOV occupies
one control row, and Apply has no silver divider above it. Settings lists Audio
Options, Video Options, Controls and Copy Profile, with Cheats last when unlocked.
Shared menu focus bars use rounded blue corners throughout the frontend and pause menus. Back remains available through
the normal key/footer. **Audio Options** retains the audio controls; Subtitles
now uses Video Options and the native profile save mechanism. Texture style stays session-only, like F10. The same
guest menu serves Windows and Mac; native Mac visual validation is pending.
This is a development checkout entry point, not an installed release update.

The FPS overlay shows `target 120 (Beta)`, `target 180 (Beta)` or
`target 240 (Beta)` alongside
measured FPS for the high-refresh presets. Native 60 shows `target 60` and
original timing shows `cap 60`, without a beta label. Network RTT appears only
when a network mode is active; offline play hides the latency readout.

For a direct game launch on either host, set `TSFP_NATIVE_FPS=240` or put
`native_fps = 240` in its isolated runtime display configuration. Supported
values are 60, 120, 180 and 240 (plus legacy diagnostic 144); absent, zero and invalid values retain original
timing. Legacy presentation settings retain main's fixed migration values.
Leave the console interrupt clock at 60 Hz. Do not use
`RECOMP_VBLANK_HZ=144` to enable this feature.

## Water at beta frame rates

Current development source enables continuous water updates at native beta rates
on the common renderer. The original disturbance and byte-feedback simulation
still runs on authored ticks; intermediate frames evolve the cosmetic field
before the original ripple shader draws it. The field follows one authored tick
behind to preserve event ordering. Original60 and legacy GL keep original water.
For diagnosis, `TSFP_SURFACE_NATIVE_TEST=0` disables the new field. If GPU setup
fails, original output remains until scene/device reset permits another attempt.
The shared implementation still needs native Mac GPU/gameplay validation.

## Timing and motion

The original US timing routine waits for an integer console tick and clamps
the frame delta to at least one. Native timing replaces that wait with a
rational host deadline and measures elapsed time with the host performance
counter. It gives continuous movement fractional deltas in the original
60-Hz units and carries remainders for integer timers. Pause stops game time
while unscaled time advances; loading stalls retain the original maximum
three-tick catch-up. The console ISR, kernel counters and audio clocks keep
their original frequency.

The original frontend consumes button edges on whole ticks. Native mode keeps
button-state sampling at that cadence and clears consumed edges between those
ticks, preventing a press from disappearing before the frontend can consume it.
Raw mouse aiming and continuous rendering do not use that button gate.

Raw mouse batches reset their age after consumption, preserving fresh counts
at different camera update rates. Terminal pointers use the same consumption
helper and accept packets on every positive fractional game interval in native
mode; pause still consumes motion without moving the pointer or hidden camera.
Original mode retains its whole-tick terminal gate. Packet-count fixtures do
not measure physical-device latency or establish live terminal visual acceptance.

The entity post-update also uses live fractional time when deriving velocity
from position changes. Its original whole-tick guard cleared the result on
fractional updates even after an entity moved. This changes that guard only;
collision response and remaining fixed-tick physics integrators still need
separate validation.

Weapon-sound requests also accept a live fractional interval. This prevents
the sound helper from dropping an already-issued event on a zero-whole-tick
frame, and lets its repeat timer consume fractional time when called every
update. The enclosing firing/state-machine gates still need separate audit;
this helper change does not establish complete weapon cadence or audio parity.

The audited generic firing routine retains its authored whole-tick projectile
gate. Its event timers and nested sound requests use that same whole-tick span
inside the call, then restore fractional time for other updates. Opening this
gate on every fractional frame would overproduce zero-interval projectiles.
This preserves discrete event cadence while continuous animation/movement
remain on fractional updates; other weapon caller families need their own audit.

The recompiler also supports an optional title numeric-conversion header.
Future Perfect uses it to read the adjacent fractional delta when an original
instruction converts an integer frame delta to floating point. Ordinary
integer counters retain whole ticks. Other addresses and the disabled mode
keep their original conversion. A frozen animation-pose scope restores the
incoming fractional delta rather than rounding it through an integer.

The US level-script dispatcher's pause gate now tests whether fractional game
time advanced. Previously it skipped zero-whole-tick frames even though an
animation had advanced; the next animation update could overwrite an event
crossing before the script saw it. NPC and player dispatchers' similar gates
discarded fractional movement and elapsed time. A site-specific global-load
policy changes those gates at `0x228880`, `0x15EB0C` and `0xD5EBF`. The integer
delta in guest memory retains its original value. The
player's per-update damping also uses fractional elapsed time to retain its
original half-life. The original dispatchers and event readers are exercised
by `tests/native_script_events.c`, `tests/native_npc_dispatch.c` and
`tests/native_player_dispatch.c`.

Player aim and weapon-offset easing split their original response over
fractional raw intervals, accounting for live game speed. This preserves the
original 60-Hz normal/slow-motion settling rate at higher refresh targets.
The six audited coefficient sites retain whole/catch-up updates and authored
snap thresholds; raw mouse packet consumption is unchanged.

Weapon sway also samples fractional level time at its final visual multiply.
The original integer sine crossing still chooses random amplitudes at the
same authored tick, preserving event cadence. Both original weapon-model
paths consume the updated value, including their sway-disabled flag. This
removes integer-clock holds in the tested normal-speed 120/144 update sequence;
slow motion can still revisit entries in the original sine lookup table.

Scripted camera translation and direction use the same live-time gate at
`0xC73C0` and `0xC7610`. Direction easing at `0xC7651` scales its original
per-update decay by elapsed game time, retaining its one-tick coefficient.
`tests/native_camera_motion.c` executes both original routines at 60/120/144,
including slow motion, pause, angular wrap and direct target snaps. Collision
services are mocked; complete camera modes and moving targets require scene
acceptance.

The camera-effect caller also runs on live fractional intervals. Its effect
timer and collision evaluation can therefore advance each native frame. The
old whole-tick kick-decay loop runs once with time-scaled factors, preserving
its one-tick result and cutoff threshold. `tests/native_camera_effects.c`
exercises the original effect routine, matrix helpers and caller gate, with
an unobstructed collision fixture. Real geometry and complete camera modes
remain separate acceptance work.

Camera mode 27 evaluates its authored track on each advancing fractional
interval. Its next sample must be at least one keyframe ahead even when the
whole-tick delta is zero; otherwise the existing blend repeatedly samples the
same pose. The native policy changes the gate at `0xCD70F`, that zero-span
case at `0xCD851`, and the divisor at `0xCD88E` to match the selected integer
span during fractional catch-up. Multi-tick spans, external track clocks, looping,
completion and large discontinuity cuts retain their original logic.

These changes target responsive input, consistent frame pacing and continuous
motion. They do not guarantee a performance increase: running more genuine
updates and draws costs CPU and GPU time. The renderer's normal scene remains
in use, including its effects and UI.

## Validation and remaining work

### Current work priority (2026-10-21)

The user asks to prioritize improvements that matter during normal play and
stop spending disproportionate time on one mission. Prioritize consistent
frame pacing, responsive aiming, weapon/animation behavior and ordinary menu,
pause and level transitions across representative scenes and modes. Keep
Scotland's unresolved route/combat observations in the regression record;
do not make repeated investigation of that mission a prerequisite for broader
progress. Use short, focused scene checks and return to a specific scene when
new evidence or a relevant fix gives a concrete reason. This changes work
order, not the full-game acceptance scope below.

### Goal and acceptance criteria

The goal is a smooth, modern feel across the full game at a native 144 FPS
target, expanded to selectable 60/120/180/240 presets, with correct gameplay
speed. Native 60/144 remain diagnostic comparisons. The scope includes texture
fluidity, animation smoothness, and keeping previously addressed bugs resolved.
These requirements remain part of the active goal until verified:

- Animation: compare player hands, weapon equip/fire/reload, character movement,
  camera motion, effects and cinematics at original 60 FPS and native
  60/120/144/180/240.
  They must progress smoothly, finish correctly and retain their original
  duration, event timing and interaction with pause/resume.
- Texture fluidity: check animated and scrolling textures, water, moving surface
  effects and texture transitions while moving the camera. Verify continuous
  motion at the correct speed, stable mip/filtering behavior, and existing
  texture replacement, alpha and fallback behavior. Record any visible hitching,
  flicker or shimmer and its cause. Artwork production remains on its existing
  texture branch; this goal covers runtime motion and rendering behavior.
- Previous fixes: build a regression checklist from the base revision's resolved
  issue/PR notes, existing regression tests and STATUS evidence. Link each
  affected fix to a test or a specific manual check, with source revision,
  result and remaining coverage gaps. Check input/menu navigation, camera,
  weapon behavior, timing, audio, display/aspect ratios, texture loading,
  settings/save preservation, networking and existing platform support.
  Run relevant checks with native mode enabled and with the original path.
  A historical passing result is not a current-branch verification.
- Completion: resolve observed regressions, preserve previous fixes, and record
  representative level/mode and visual comparisons plus measured frame pacing.
  An FPS counter, successful build, scene flag, or aggregate test count alone
  does not meet this goal. Unavailable host checks and untested gameplay remain
  explicit; do not claim all bugs are resolved without supporting evidence.

The opening timing regression required repairs to both script-event and NPC
dispatch gates. Repairing the script-event gate allowed native 144 to equip
the pistol and show the HUD/next objective, but the NPC still approached too
slowly. Correcting its movement gate restored walking speed to within 0.3%
of the original 60 FPS reference in the measured opening segment. The player
fix subsequently restores steady walking speed to within 0.05% of a
same-build original-60 reference, with matching sampled ammunition totals.
The latest bounded runs pass handoff, firing, hand-animation and pause/resume
checks. The unskipped cinematic also reaches the handoff on the prior NPC
build; exact natural-intro timing, other gameplay systems and full-game
progression remain unverified. See the
[regression record](native-high-refresh-regressions.md) for current evidence
and the prior-fix checklist.

Run `python -m pytest tests/test_native_timing.py` after applying the complete
patch stack. It checks elapsed-time integration across supported/diagnostic rates,
integer timers, pause/resume, stalls, ABI stack balance, numeric conversions,
the original fallback, and fractional animation-time restoration.

The bounded Windows integration runner accepts:

```powershell
.venv\Scripts\python.exe tests/native_refresh_game.py --native --fps 144 --seconds 120 --stress --pause-check --windowed
```

It uses only disposable Test Lab data, caps its logs and recycles its virtual
disks. It records native update counts and game-clock ticks separately from
host presentation counts. A closed test window makes the run incomplete;
partial timing evidence is retained with that status. `--capture` keeps up to
24 renderer BMPs, with its final three slots rotating. The optional `TSFP_NATIVE_TRACE` CSV
contains at most 20,000 update timestamps and deltas. These measure CPU update
cadence, not GPU completion, monitor scanout or end-to-end input latency.

For effect reports, add `--trace-effects`. This samples the owned child's
effect lists approximately every half second during Story mode, saving
`<tag>-effects.json` within the disposable test directory. Each traversal is
limited to 256 effects and the output to 16 MiB. Records include effect type,
age, lifetime, deferred start, position and view flags; type-32 beams also
include colors, strand delays and the original dispatch-gate result when the
current player is readable. Validation records the tracer's hash and whether
its output limit was reached. Reads are not atomic, pool addresses can be
recycled, and short-lived effects can fall between samples. An eligible effect
is not proof that a GPU draw or visible frame occurred. This option changes
only observation of the isolated test child, not personal settings.

Measured Windows results and their source/build scope are in `STATUS.md`.
Native Mac build/runtime validation, additional levels and modes, vehicles,
recorded demos, multiplayer synchronization, full campaign timing, and
perceptual smoothness remain acceptance work. A passing first-mission test
does not establish native 144 FPS correctness across the full game. The mode
remains opt-in while that work is incomplete.

## Acceptance audit (2026-10-24)

This audit uses source `82b27c31`, the regression record, current test bodies and
the Windows build identified in STATUS. Earlier observations retain their named
build scope. Passing isolated routines does not establish their complete callers
or the rendered result. The goal remains incomplete.

| Requirement | Evidence inspected | Remaining acceptance |
| --- | --- | --- |
| Fresh updates, speed and pacing | Native clock and network cadence fixtures; current Disco and preceding Temple guest-frame/Present traces | Sustained performance across representative demanding scenes; reported dips remain |
| Opening handoff and animation | Original animation timeline/event fixture, script dispatch controls, recorded opening comparison | Current 180/240 rendered opening comparison and natural cinematic timing |
| Movement, aiming and cameras | Player/NPC dispatch, locomotion, aim, mouse, camera and vehicle fixtures | Live camera/vehicle/terminal transitions; physical-input feel |
| Physics | Original rigid-body, vehicle, falling, force and contact fixtures | Representative real collision geometry and interactions |
| Weapons and effects | Original firing, sound, projectile, beam, sniper and reload fixtures; live firing/reload cycle; Temple floor-rocket lethality/lifetime/cleanup and respawn at Original60/180/240 | Specific delayed-trail, sniper-hit and pre-bridge NPC reports remain without targeted live confirmation |
| Scripts and pause | Phase-swept events/reload and pause/resume checks | Broader progression and interactions with moving actors/cameras |
| Menus and settings | Four presets, staged Apply, save/error contracts; weapon-menu scene checks | Broader visual/menu and native Mac checks |
| Texture fluidity | Fractional water implementation, GPU-image cycle and resource reuse; texture contracts | Other animated/scrolling surfaces, filtering/alpha/transitions across scenes |
| Cinematics and audio | Original polling clocks and real-file movie decoder tests; prior movie tint correction | Visible/audible synchronization across representative scenes and host output |
| Multiplayer | Cadence/correction/replica fixtures through240; mixed-preset movement, completed matches, matching results/menu captures and240/60 rematch reset plus second-round completion in STATUS | Exact damage equivalence, further modes and cross-machine behavior |
| Prior fixes, saves and platforms | Regression checklist, isolated signed-profile preservation, shared code and host CI | Scene checks for prior fixes; physical Mac gameplay remains unverified |

Mixed-preset loopback now supplies bounded movement/combat observations. Keep
that coverage separate from CPU optimization measurements and Internet latency.
Further scene/visual comparisons remain necessary. The user's request to defer
the first mission changes investigation order, not its unresolved reports.

## NPC firing timing

The shared native timing path also scopes the NPC firing transaction
`sub_001BB5C0` to its carried whole-game-tick span, restoring fractional time
before returning. Its original projectile gate and ammunition operations
remain intact. This avoids losing event-timer and repeating-sound time on
the fractional updates skipped by that gate. See the
[NPC validation record](native-high-refresh-regressions.md#npc-firing-event-tick-scope)
for source/build status and the remaining caller/damage coverage.
