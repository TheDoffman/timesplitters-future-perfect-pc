# Native Uplink holding

Candidate for Discord report1566835743572238366, base831c324a. Windows gameplay
acceptance is pending; no resolution comment posted. For rocket timing see
[projectiles](native-projectiles.md).

Original US1C3100 repeat gate1C397F and request gate1C2B36 discard zero-whole
intervals. Uplink physics callback1C55F0 event7 calls3508B0, which replenishes
selected-object pull strength (+2DC) using fractional seconds. Dropped requests
lose holding time at native rates.

pc_native_uplink.h admits only positive finite native intervals with zero whole
ticks, linked hand/state weapon36, physics variant+38=1 and primary+5C=0.
The numeric macro supplies the audited live hand: EBP at1C397F, saved argument
[ESP+14] at1C2B36. These use the generator's live GPR cells. No clock is rewritten.
Other guns, map/secondary mode and whole/catch-up values retain original policy.

The existing test_native_timing.py CI route includes test_original_uplink_hold_gates.
It translates the original request routine, repeat-gate instruction block,
weapon callback and pull-strength updater from the supported US XBE. Targeting,
weapon-state handoff and rigid-body services are controlled boundaries. Both
paths preserve charge at60/120/144/180/240, four carried phases and normal/quarter
speed. Separate mutations restoring either old gate lose requests and charge
at120. Guards cover Original, pause, map/secondary, other guns and bad links.
This establishes request/charge behavior, not live lift, release/throw,
collisions, multiplayer, native Mac or Linux gameplay. Verify those separately.
