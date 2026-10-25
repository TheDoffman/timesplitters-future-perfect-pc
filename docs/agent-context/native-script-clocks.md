# Native scripted object clocks

Authoritative policy: `src/pc_native_numeric.h` and shared
`src/pc_native_trap_guest.h`. Generated guest C is read-only evidence.

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
