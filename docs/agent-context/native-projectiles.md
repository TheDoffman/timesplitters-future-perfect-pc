# Native projectile motion

`src/gameplay/pc_native_projectile_guest.h` scopes the US `F08D0` updater.
The historic kind35 helper names remain ABI entrypoints. Its fractional policy
covers kinds 8–11, 35, 39 and 42. The entry whole-tick gate otherwise skips
frames while `F0D45` multiplies translation by only the current fractional game
delta: at 240 FPS this loses three quarters of straight-flight time.

The wrapper opens the gate on positive game delta, scales E2370 steering by
that delta, and retains E1FA0 target-scan cadence. The original whole-tick speed
multiplier at F1513/F1544 remains unchanged. Kind42 boost at F15DD stays on
that same boundary, avoiding amplification from applying boost before the next
multiplier. No global clock is rewritten.
Kind 50 uses different E2520 steering and is deliberately outside this policy.
Other updater families retain their existing policies (F4A50 regression coverage
includes kinds 28/31/45/46).

Patch `2596-native-rocket-projectile-timing.patch` adds E28D0 initialization to
clear recycled pool-slot state, alongside the existing E78F0 kind35 initializer.
It follows the deferred kind35/object-rocking patches in apply-patches.py.
The original 85-slot pool bounds and nested context restore remain in force.
At the six F156D/F1582/F1597/F19C9/F19DE/F19F9 smoke RNG sites, fractional-only
frames return a neutral x87 value without advancing the RNG. Their F15C4/F1A2D
emit calls are suppressed on those frames. Translation and kind42 acceleration
still execute (boost uses the carried whole count). Persistent emitters continue
following the projectile every frame;
impact/explosion randomness is not suppressed.

`tests/gameplay/test_native_timing.py -k projectile` executes original US XBE
code with controlled collision, sound and emitter boundaries. The coupled test
covers all six newly included kinds at 60/120/144/180/240, straight/homing,
normal/quarter speed, pause, impact/expiry and original 60-Hz references.
It compares distance/speed/target scans and smoke RNG cadence; restoring the
old entry gate must fail. The scope test covers slot recycling/nesting.
These fixtures establish routine behavior, not live-map collision or rendered
trail appearance. A supported private US XBE is required; skips are not passes.
