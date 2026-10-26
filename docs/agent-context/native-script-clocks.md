# Native scripted object clocks

Authoritative policy: `src/pc_native_numeric.h` and shared
`src/pc_native_trap_guest.h`. Generated guest C is read-only evidence.

## Venice timed callback

The three normal/challenge Venice descriptors route their +110 callback to
US222B60. The original level dispatcher228870 already admits native subticks,
but this callback had a second integer game-time gate. It subtracts live float
seconds from its private timer, so skipping subticks loses elapsed time. The
exact222B60 load now follows positive game time; its EAX is replaced by the
state pointer before use. Original mode, reset222740 (six seconds), event156F,
the authored900-second repeat, sound services and state layout remain original.
Zero-float pause rejects a stale whole count. Other callback sites stay unchanged.

`native_venice_callback.c` lifts the original dispatcher, reset and callback,
with descriptor lookup and sound consumers controlled. The test verifies actual
XBE descriptor pointers/names, then countdown/event/stack/callee-saved state at
native120/144/180/240 normal/quarter intervals, a coalesced authored-tick
Original60 reference, pause and600 mixed intervals.
The old entry gate reproduces half elapsed time at120 and fails. This test is
selected through existing `test_native_timing.py` on both hosts; original cases
skip without the supported private XBE. This proves the selected CPU scheduling
path, not audible output, every level callback or a live Venice match.
The asset-free timing-hook fixture also checks exact site/address selection,
advancing beta intervals, pause and Original fallback on both CI hosts.

## Subway timed callback

The normal/challenge Subway descriptors route +110 to21AF90. Like Venice,
its entry gate discarded subticks before subtracting float seconds. The exact
load now follows positive game time; EAX is replaced by its state pointer first.
Its active-sound polling, completion branch, random choice excluding the previous
variant and randomized next delay retain their original control flow. Pause
does not poll sound completion, change timers or consume random draws.

`native_subway_callback.c` runs original228870,21AF50,21AF90,12380,153BB0 and
153BD0: dispatch, reset, callback, nonrepeat choice and both LCG consumers.
Sound lookup/playback/completion and descriptor lookup are controlled. Native
60/120/144/180/240 normal/quarter intervals compare three actual event choices
and RNG seed observations with a coalesced authored-tick Original60 reference.
Completion is simulated at0.3 game seconds; that is not a live audio clock.
Event times stay within0.15 game seconds after three events; chosen variants,
per-event seeds and final seed after seven total draws must agree exactly.
Scope/pause/stack/callee-saved/FPU checks remain independent. Restoring only the
old gate reproduces0.25005341 consumed seconds per game second at native60
quarter speed and fails. Both map fixtures share the parameterized original-code
test and verify XBE descriptors before execution. Public timing-hook scope tests
cover both exact sites. This does not validate sound output or all level callbacks.

## Registered yaw callback

US224170 advances entity yaw at+70 using float seconds, but its integer entry
gate discarded fractional frames. Initialization225430 registers this callback
as type3 throughA5CE0 at entity+160. A7670 reads that registered slot; A7870
then callsA7360, which detects yaw changes, marks+28 bit0x1000 and publishes
the yaw at+108. A14B0 mode1 consumes that dirty flag and builds the cached yaw
matrix via153440. These are actual lifted paths, not an inferred update order.

Only224170's entry load now follows positive game time. EAX becomes entity flags
before use, so its original angle arithmetic,0/91-degree clamps, moving flag,
sound378/379 start/stop and state selection remain. No transform is interpolated
or retained by a new cache. Pause with a stale whole count does not advance yaw.

`native_scripted_yaw.c` runs original registration, both entity passes, model
status, post-update, matrix cache and affine point transform, including the
existing accelerated point path. It verifies independent sin/cos coordinates
and fresh points on every advancing native frame before the endpoint.96 cases
cover both directions, four carried phases, normal/quarter intervals through240
and a coalesced authored-tick Original60 reference. Exact endpoint/event counts,
unchanged clocks, pause, ABI and frame propagation are checked. Reverting only
the gate fails; suppressing only the original dirty store also fails. The
measured largest stop-time error is one original tick. Eighteen whole/catch-up
cases retain byte-equal entity/point/event state. Sound, world/sector/model
services and physics are controlled. This is CPU scheduling/pose propagation,
not live collision geometry, renderer/GPU pixels or physical Mac acceptance.
The existing timing test file selects originals when the private XBE exists;
the asset-free timing-hook fixture checks this exact site on both hosts.

Do not change every inventory match into a fractional entry gate:2238D0 uses
whole ticks for a damping loop while separately integrating float time, and
1DDF10 already integrates float motion before a whole-count helper loop. Those
sites need their own operator/event analysis; neither is changed here.

## Deferred actor voice clock

US164E00 is a deferred actor-audio scheduler, not a physics step. Original
normal-frame caller1CE2E0 invokes it at1CE3B3 without coalescing the game delta.
The scheduler's integer entry gate discarded fractional seconds from the
one-second pending voice,0.2-second secondary request and five-second cooldown.
Only that entry load now follows positive game time. EAX is unused after the
gate; queue ordering, handle-generation tests, voice selection and stores remain.

`native_actor_voice_clock.c` executes original normal-frame caller, queue/reset
setters1648D0/164920/164960, scheduler164E00, default selection164900, main RNG
153B40 and64-bit multiply3C9ED0. It also executes153C00: that helper saves/restores
the separate float RNG from7497CC/D0 according to whole ticks; it does not seed
it from a tick count. Its existing protocol remains. Other frame services,
voice definitions and audio consumers are controlled boundaries.

576 cases cover rates through240, normal/quarter intervals, four carried phases,
three voice paths and independent stale primary/secondary generations. The
original queued delays, cooldown admission/re-admission and single main RNG
draw agree with an independent64-bit recurrence; stale actors deliver/draw
nothing. Pause with stale whole ticks holds timers, delivery and main RNG.
The largest event-time error is one original tick in simulated game time.
36 whole/catch-up cases preserve byte-equal queue/timer/RNG/event observations.
Old-gate and disabled-generation controls fail at their intended assertions.
Public scope tests cover this exact site on both hosts. Private-XBE originals
skip without the supported asset. This is request scheduling, not audible PCM,
physical audio latency, all caller services or live scene acceptance.

## TRAP motion and contact lifetime

US1D8AB0 rotates a tagged object using a sine-table impulse, drift, upper clamp,
restitution and contact counter. The earlier whole-tick repair (PR363) restored
authored mechanics but held poses between carried ticks. This follow-up admits
fractional game intervals at its entry gate and converts phase/travel reads
1D8AF5/1D8B6A together with the force/travel operators.

The authored map is kick then drift: `v += a(theta); theta += v/60`. In the
original velocity coordinate a fractional symmetric step is
`K((h-1)/2) D(h) K((h+1)/2)`, where h is live game ticks. The first kick at
1D8B46 uses `(h+1)/2` of the original impulse. The drift at1D8B7A uses that
velocity and live h; its second kick uses `(h-1)/2` of the impulse sampled at
the resulting angle. Both use the original quantized sine table/constants.
At h=1 the formula is exactly the original kick/drift map. Repeated fractional
steps are a numerical approximation, not bit-equal original whole-frame states.
Original/whole/catch-up calls bypass the fractional operators entirely.

Original threshold, sound/control calls, clamp, restitution, dirty flags and
removal still execute. More frequent clamp contacts otherwise consume the
integer contact count faster. Only the increment at1D8BFD is replaced after
its store by the carried game-tick count; the first sound-state increment stays
original. Thus fractional contacts cannot expire the object early. No per-entity
cache, pose interpolation, retained phase, allocation, layout or OS adapter.
Pause with stale integer ticks is rejected before the callback changes state.

`PC_GPU_ACK_AFTER_STORE32` retains the original exact3D66C3 acknowledgement
notification. The numeric policy composes it with the exact1D8BFD contact hook;
neither hook observes the other's store. Tests execute notification composition
as well as the actual guest GPU store, fallback and acknowledgement guards.

## Validation and limits

`native_trap_motion.c` executes the original US callback and sine initializer;
sound/control/removal services are spies.40 rate/slow/carried-phase schedules
cover60/120/144/180/240, fresh motion on every pre-sound frame, a one-original-tick
pose envelope with0.25degree tolerance, phase-zero aligned endpoint error below
0.25degrees, event time windows, terminal state,
phase, pause and ABI.12 additional mixed-rate/jitter schedules cover live changes.
Original/whole/catch-up state/events remain byte-equal. Independently removing
gate, either conversion, either coupled operator, or contact normalization fails.
The public native clock fixture exercises the coupled map, fallback and GPU-hook
composition on both CI hosts without game assets.

`native_trap_transform.c` extends CPU propagation through original initialization
1D8C40, callback1D8AB0, post-updateA7360, Euler cacheA14B0 and affine point151E80.
A74F4 sets +28 bit0x1000 when C0 changes; A14B0 consumes/clears it, rebuilds
the cached rotation at+274, copies it and supplies translation. Its mode2 path
loads C0 atA15C6 and calls151650, which evaluates sin/cos directly; the force
sine table is not a quantized rendering-angle lookup.80 rate/slow/phase/mode
cases require fresh native off-axis points, Original held-frame cache reuse,
independent trigonometric coordinates, point fast/original bit parity, pause,
ABI and restored clocks. Four
independent dirty-store/angle/copy/point-transform controls fail. Registration,
collision and sector services remain spies; the fixture drives routine order.
It does not prove normal frame scheduling, live mesh identity or GPU rendering.

Rejected original-code experiments show why isolated delta scaling is wrong:
ordinary fractional Euler motion differs by about3.49degrees at aligned240
samples and retires at0.70417s versus Original0.81667s. The symmetric coupled
candidate reduces aligned pose error below0.165degrees but still retires early
without contact normalization. With normalization,40 schedules retain sound/
retirement within one original tick plus sampling resolution, with fresh poses.
Frozen private experiment provenance and local results are referenced in STATUS.

These CPU numerical checks do not establish normal-play TRAP activation,
rendered appearance, actual event-service behavior, damage/collision parity,
frame pacing or physical Mac gameplay. The earlier bounded Mansion route did
not activate TRAP. Do not repeat it without a source-backed trigger route.
See STATUS and MAC-060 for measured host evidence and remaining acceptance.
