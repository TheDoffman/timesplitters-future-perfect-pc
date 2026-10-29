# Mounted-gun ownership (#385)

`src/input/pc_mounted.h` follows the player entity's `+0xC14` gun link in
mounted mode `0x80`. The US `B76B0` mount and `B7830` dismount, plus CCTV entries
`373E10`/`3756E0`, write/clear the operator at gun data `+0xD8`. The helper checks
that reciprocal link and the gun entity backpointer before changing aim.

Gun data `+0x68` is an AI target (`242F60`/`245CE0`), not the operator. Data `+4`
is an authored instance name copied by constructor `245990`; `AGUN` is one name,
not a type discriminator. The previous helper incorrectly required both that
name and the AI target to match the player. Do not restore those restrictions.

The guarded readable extent is `0xDC`, covering the operator before any write;
only the original angle prefix needs write access. Mode, reciprocal links,
locked states, pause, finite values and authored limits remain guarded. Windows
and SDL share the helper. Separate vehicle-seat mode `0x8000` remains outside it.

`tests/gameplay/test_mounted_mouse.py` runs asset-free ownership/cadence fixtures,
both adapter bodies with controlled memory maps, and an optional supported-XBE
test executing the original `B76B0`/`B7830` ownership writes and cleanup. No
generated guest code is checked in. The input self-tests use the corrected
operator field too. These checks do not establish live mission or physical Mac
input acceptance; see STATUS and MAC-001.

`mounted_guest_binding.c` passes the original routines' `C14`/`D8` links to the
actual mouse helper. Lookup, player-mode and weapon boundary services are
controlled; ownership writes/cleanup execute unchanged. The optional test
covers four names, independent AI targets, repeated mounting and Original60/
native cadence; it skips without local game data. The adapter fixtures reject
target-only authorization, a different operator, broken backpointers and an
unreadable operator tail. Runtime CI already selects the mounted suite.

`python -m pytest tests/gameplay/test_mounted_mouse.py tests/input/test_terminal_mouse.py -q`
compiles guarded host-memory C fixtures on Windows/macOS. Mounted tests cover
operator/local port, single-consumption relative motion, gun-specific
pitch sign, FOV/inversion/speed, authored clamps/fixed axes/full-circle wrap,
paused/stale/focus rejection and returning to ordinary look. Runtime CI includes
both tests. Physical turret aiming/firing and native host delivery remain
separate gameplay checks; see #182 in STATUS and MAC-001.
`native_mounted_mouse.c` checks 60/120/144 with 125/1000/8000 Hz packets,
jitter, quarter speed, fresh/integrated displacement, pause/invalid clocks,
handoff and untouched player angles. Old-clock/batch mutations must fail.
These controlled-field tests do not certify live turret rendering/input.
Consult STATUS for current build hashes, not historical candidate paths.
`mouse_region.c` checks actual Windows/SDL mouse bodies with controlled maps:
one query for either address order, forward page spans, split/read-only/inaccessible
FOV maps, expiry, terminal routing and bounds. Persistent/uncached/unordered
mutations must fail. `camera_region_host.c` validates real OS protection changes
on Windows/macOS. Mocked locks/events are not physical input. `auto_look_region.c`
checks the pre-guest auto-look scope: one same-map query, split/read-only maps,
expiry and bit restoration. Its mutations run in registered `test_auto_look.py`.
`tests/input/pc_controls.c` and the SDL `--input-self-test` now exercise mounted
mode through the actual adapters, changing the native rate without recreating
input capture. The focused Windows `tsfp_pc_controls_test` target passes with
the isolated fixture config and scratch directory. SDL physical-host execution
is still pending. The portable packet fixture explicitly includes `<stddef.h>`
for `ptrdiff_t`; Mac Clang does not supply it through `<stdlib.h>` as MSVC did.
