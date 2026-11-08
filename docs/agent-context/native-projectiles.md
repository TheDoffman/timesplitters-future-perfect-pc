# Native projectile motion

The US projectile table is `4DEB28`, 54 entries with a 64-byte stride;
initializer/update pointers are at +30/+34. `4F9BAC` points to the 66 weapon
records (stride29C, firing kinds at +6C/+E8). Do not stop the audit at kind50.
`test_original_projectile_dispatch_table` checks every updater assignment.

The loaded Machine Wars scene5 tracked vehicles (model11, flags17) have two
guns. Their gun data +6C points to weapon states: +30 is weapon59 for the main
cannon and60 for the second gun. Weapon59 fires kind51 / F6600; weapon60 fires
kind45 / F4A50. The ordinary flare menu weapon4 fires kind38 / F1A90.
Kind41 shares F1A90, but is not the tank cannon.

## Flight time

`pc_native_numeric.h` opens only positive-game-time entry gates where an
updater otherwise discarded intervals while translating by the current
fractional delta. Whole counters and discrete speed multipliers stay unchanged.

| Updater | Kinds | Policy |
| --- | --- | --- |
| F4A50 | 3–7,27–31,40,45,46,49 | Existing fractional entry F4A66 |
| F08D0 | 8–11,35,39,42,50 | Scoped entry F08D0; fractional translation |
| F1A90 | 38,41 | Scoped entry F1A96 and coupled ballistic repair |
| F2B80 | 12,13,15 | Entry F2CBB; whole speed-decay loop retained |
| F39A0 | 14,16–19 | Entry F39E2; whole speed-decay loop retained |
| F51F0 | 20 | Entry F5200; whole speed-decay loop retained |
| F5920 | 21,22 | Entry F5936 |
| EAD10 | 32,33 | Entry EAD16; EBA4A/EBA58/EBA67 drag uses pow(factor,delta) |
| F61E0 | 37 | Entry F61E0 |
| F6600 | 51 | Entry F6600, including the Machine Wars main cannon |
| ECB90 | 43 | Movement already fractional; random steering cadence repaired |
| ED7C0 | 52,53 | Already fractional; movement retained |

F1A90 mixes fixed and fractional steps. F1AF0 displacement and the three
F25D0/F25E4/F25F4 look-ahead products now scale by delta. F28F4 normalizes the
newly derived speed by delta before EDBE0 receives it. Scaling displacement
alone loses speed; omitting look-ahead scaling creates a first-frame spike.
E2F90 initialization and F1A90 execution use the shared projectile scope.
Ballistic jitter is sampled on whole ticks, then interpolated per coordinate
between the previous/current samples during fractional flight.

F08D0 retains E1FA0 target-scan cadence and fractional E2370 steering. Kind42
boost stays on the original whole speed-multiplier boundary. Kind50's distinct
E2520 spring runs only on whole boundaries; its E280A rotation interval is
normalized back to the authored step. Translation continues every frame.
No shared guest clock is overwritten. ECB90's ECCD6 random steering sample is
held between whole ticks while its original fractional motion continues.

## Scope and effects

The historic kind35 helper names remain ABI entrypoints in
`pc_native_projectile_guest.h`. Sidecars cover the original 85-slot /22C pool,
restore nested contexts and reset on every DC4A0 allocation, even when native
timing is off or the new kind has no initializer. Existing E28D0/E78F0 resets
remain. Patch2605 follows2596 in the loader's deferred order. Each wrapped original
must have its own `extern void sub_ADDRESS_gen(void);` declaration: the
production C scanner does not discover comma-separated extern declarators.
The fresh-stack fixture checks all five originals with that actual scanner.

Flight-smoke RNG/emission sites in F08D0, F1A90, F2B80 and F39A0 keep whole-tick
cadence. The F33FF/F3428/F3451 object-spin samples and existing rocket visual
jitter are cached per call site and slot. Collision/explosion randomness is
not suppressed. Persistent effect transforms follow the fresh position.

The remaining table paths were inspected separately: 0/1/2/36 have no updater,
34 is a no-op, 24/47 use immediate hit processing,23 is a stationary burning
flare effect. EBB00/EC120 (26/25) trace fixed-length beams; E4800 (44) derives
position from total age; E8490 (48) already consumes fractional time without
an entry whole gate. Their movement gates are retained. Existing kind49 trail
randomness and kind44 age-dependent effect choices are not newly normalized.

## Verification

`tests/gameplay/test_native_timing.py -k projectile` executes original supported
US XBE routines with controlled empty-scene, collision, sound and particle
boundaries. A missing private XBE is a skip, never a pass. Ballistic fixtures
execute E0F30/E2F90/F1A90 and compare .5/1/1.5 game seconds at
60/120/144/180/240, normal/quarter speed: position error <.15 units, speed
within the original envelope, unchanged smoke/RNG counts, exact native60 object
bytes, pause, guest stack/x87/callee-saved registers. Independent ablations
of entry, step, look-ahead, speed normalization, random and smoke policies must
fail behavioral assertions, rather than fail to compile.

Family fixtures compare complete original flight updaters across the same
rates and speeds, including all F4A50 table kinds, the type51 tank path,
three-axis drag, whole speed multipliers, RNG/effects and pause controls.
Their controlled initial flight excludes kind17's short timed detonation.
The existing coupled rocket fixture covers straight/homing, initialization,
impact/expiry, target scans, slot reuse/nesting and whole/fractional schedules;
kind50 now executes the original E2520/matrix helpers as well. Tank-gate and
vector-drag ablations must fail. These establish guest behavior, not full
live-map collision, vehicle handling or rendered trails. Keep separate live
and host-build evidence in STATUS.md.

## Fixed-length beam damage (#482 investigation)

EBB00/EC120 (kinds26/25) retain their whole-game-tick trace gates. Their
BD0F0 dispatch reaches B9960 object damage or B9790 character damage; continuous
profiles previously multiplied by only the latest fractional tick at B99B4/B9901.
That loses damage on frames between traces: the controlled kind26/object case
measured6,3,2,1.5 damage per game second at60/120/180/240 respectively.
`pc_native_beam_damage` replaces that multiplier with the whole game tick count
only at those two sites and only for kinds25/26 with native timing active.
The guest stack offsets are C (B99B4) and18 (B9901); no clock, trace, RNG,
profile value or other projectile kind is modified.

`test_original_beam_damage` lifts both complete beam updaters and their original
hit-info/falloff/object/character damage routines. Collision target, hit effects,
character classification and damage application are controlled service boundaries.
At60/120/144/180/240, normal/quarter speed, totals match Original60 with60 traces
and240 RNG calls per game second; pause changes neither. The legacy multiplier
negative control fails the damage comparison. `native_timing_guest.c` also checks
both stack offsets, all54 kinds, original/native and0/1/2 whole ticks without
private assets. These tests do not establish shield-door overload or live mission
acceptance; #482's other reported timing paths need independent investigation.

`test_original_projectile_fuses` additionally executes DC7B0's full85-slot
pool dispatch/age integration and F39A0's stationary kind17/19 fuse logic,
plus F04B0's kind18 proximity arming and delayed explosion.
It verifies original table targets and checks six-/two-second detonations at
60/120/144/180/240 within one60Hz tick, with fuse audio and explosion consumers
controlled. A nearby target and clear launch space also preserve the proximity
mine's approximately two-second arm/launch/explode sequence. This passes without
a new mine timing change; it does not identify
or validate the Crow boss's scripted bombs, collision-triggered detonation or
arming behavior. Do not treat it as closure evidence for that report.

## Detached darts (#482)

E0520 selects E0300 (Story) or E03D0 (Arcade) when an entity releases attached
projectiles. The one-time downward velocity at E036B/E04E7 was multiplied by
the latest fractional frame. `pc_native_dart_release` normalizes positive
subtick intervals to one authored tick for kinds32/33 only. Zero intervals,
whole/catch-up values, original mode and other kinds retain their inputs.
Ownership clearing, collision selection and retirement remain original.

`native_dart_release.c` executes both original detach paths, then EAD10 and its
matrix helpers. Release velocity matches exactly at60/120/144/180/240, normal
and quarter speed; half-second falling distance stays within0.03 world units
of Original60. The legacy product fails. Scene collision queries are controlled;
this does not establish a live corpse/player collision result. The asset-free
fixture checks all54 kinds, zero/subtick/whole/catch-up and native-off selection.

Beam and spawn countdown fixes retain zero-float pauses even with a stale whole count.

## Crow boss mines (#482)

Crow's custom eight-slot pool at75F5E8+11A0 is separate from F04B0 proximity
mines. The boss update28FE30 calls28F560 at290BDC; this executes2AA6A0's
fixed-step arming, four-direction effect emission, rise/fall and retirement,
followed by player interaction. RECOMP_FAST_ENTRY now skips28F560 on native
frames without an accumulated game tick or with zero local game time. It pops
only the guest return address. Boss movement and the separate draw28D710 remain
unrestricted. Original mode and whole/catch-up calls keep original behavior.

`native_boss_mines.c` lifts28F560 and2AA6A0 from the pinned US image, copies
constants through4A0000 (including the double rotation constant), and compares
arming/explosion/retirement ticks and four-way effect counts at60/120/144/180/240
and quarter speed. Near/far target cases, pause and ABI are checked. Removing
only the entry gate fails. Effects and collision consumers are controlled;
this is original CPU state-machine evidence, not a completed live boss fight.
