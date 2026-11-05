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
