# Native Uplink holding

Discord report1566835743572238366, base831c324a, coreb3545910. Windows native
120/180 live barrel lift/hold/release passes; see STATUS.md for provenance.
Merge/publication and native Mac/Linux acceptance remain pending. For rockets see
[projectiles](native-projectiles.md).

Original US1C3100 repeat gate1C397F and request gate1C2B36 discard zero-whole
intervals. Uplink physics callback1C55F0 event7 calls3508B0, which replenishes
selected-object pull strength (+2DC) using fractional seconds. Dropped requests
lose holding time at native rates.

pc_native_uplink.h admits only positive finite native intervals with zero whole
ticks, linked hand/state weapon36, physics variant+38=1 and function+5C=1.
The original1C2C00 input routine selects function1 from the authored Uplink
descriptor's0x800 flag when variant1 is active, including for the primary
button. Function0 is not the physics holding function.
The numeric macro supplies the audited live hand: EBP at1C397F, saved argument
[ESP+14] at1C2B36. These use the generator's live GPR cells. No clock is rewritten.
Other guns, map/function0 and whole/catch-up values retain original policy.

The existing test_native_timing.py CI route includes test_original_uplink_hold_gates.
It loads the original Uplink descriptor and translates1C2C00/1B7ED0 input and
eligibility, the request routine, repeat-gate instruction block,
weapon callback and pull-strength updater from the supported US XBE. Targeting,
weapon-state handoff and rigid-body services are controlled boundaries. Both
paths preserve charge at60/120/144/180/240, four carried phases and normal/quarter
speed. Separate mutations restoring either old gate lose requests and charge
at120. Guards cover Original, pause, map/function0, other guns and bad links.
The separate live Windows Russian Connection route confirms actual barrel
lift/hold/release at120/180 with original targeting and physics. Throwing,
multiplayer, native Mac and Linux gameplay remain unverified.

## Throw impulse

US350320 release block3505CF..35061A calls351AA0, then16C8F0 to add a
one-shot force/mass into body+110/+114/+118. The real body solver integrates
and clears this force on the next frame. At native120 it therefore imparted
half the original impulse. Only numeric multiplication3505D7 normalizes that
release force by the positive fractional raw interval (4E7948), retaining
the original game-speed multiplier and unchanged native60/Original behavior.
No global force, clock, target, grab or cooldown is rewritten. Invalid/pause
intervals and unrelated multiplication sites keep original behavior.

The existing native_rigid_motion fixture lifts the original throw block,
351AA0 and16C8F0 together with the original body clock/solver/integrator.
Controlled target/direction and empty space exclude map collisions. It checks
nonzero force, velocity and one-second travel at60/120/144/180/240 in normal
and quarter-speed time. Speed matches within .001; position allows2% for
original integration step size. Removing just the throw normalization must
fail at120. This is simulated physics evidence, not live-map acceptance.

Windows manual acceptance, 2026-11-07: the user confirmed matching throw
distance at60 and240 FPS in the corrected isolated build. Other objects/maps,
multiplayer and native Mac/Linux throws remain unverified.
