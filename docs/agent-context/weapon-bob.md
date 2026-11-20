# High-FPS walking weapon bob (#557)

Base main1f835403, local fix/557-weapon-bob; not published. Idle sway and
walking bob are distinct paths in US1C4400. The prior fixture only exercised
idle sway; it stubbed the movement sample. D5EB0 snapshots horizontal movement
at D61A7 into player+170 before this frame's world step, and retains whole
intervals in +10C/+148. The walking phase division1C45BF and amplitude division
1C4640 therefore undercount speed when native displacement spans a fraction of
a tick. The latter also clamps its divisor to at least one. Original-code
regression reproduced half phase/amplitude at120 versus60 before correction.

pc_native_weapon_bob.h observes only D6251's existing whole-clock load, without
changing its returned value. Four player/actor/scene-keyed records retain the
preceding update's fractional game interval. The two audited divisions normalize
that previous displacement using its matching interval when below one tick.
First observation, scene/actor replacement, clock rewind, invalid time,
whole/catch-up intervals and native-disabled paths retain the original divisor.
No integer counter, simulation delta, position, weapon configuration, idle sine
crossing, RNG call or shot cooldown is changed. Implementation is instantiated
once by pc_native_visual_time_guest.inc; numeric policy routes the audited sites.

Tests: native_weapon_sway.c executes original D613D-D6256 movement snapshot,
complete1C4400 and original sine initialization, plus model stores for idle sway.
Compare walking phase and offsets at60/120/144/180/240, normal/quarter-speed;
check mixed frame durations, four independent players, pause/invalid time,
scene reset, catch-up, neighbor sites and original idle RNG cadence. Existing
native_player_dispatch.c uses the real observer alongside its original dispatcher.
Run python -m pytest tests/gameplay/test_native_timing.py -k
'original_fractional_dispatch and (native_weapon_sway or native_player_dispatch)'.
These execute original routines with controlled movement/services, not a live
map or every weapon's authored model. Windows/POSIX fixture validation and any
remaining live/native acceptance are recorded in STATUS.md.
