# Host return to the multiplayer lobby

`src/network/pc_multiplayer_exit.inc` owns shared Arcade Online/System Link host
exits. `pc_display_menu.c` includes it before the Story adapters. Additive network
patch2620 routes the existing result/65470,65420,3592F0,79860 and1AF950 wrappers
through these adapters, retaining the existing Story fallback. No new packet or
generated-code edit is used. Windows, native Mac and Linux share this policy.
Patch2625 admits both TSFP_PC_CONTROLS and TSFP_NATIVE_DISPLAY at all thirteen
exit/result/profile-save wrappers. The native target does not define the Windows
controls flag: enabling shared adapter source alone leaves these wrappers taking
their stock fallback. Keep this distinction covered by compiling the applied
runtime wrappers under stock, Windows and native definitions.

The final user scope (2026-11-06) keeps a guest's own Leave Match/Quit and browser
Back behavior original. Only the host's EC060/EC030/EAF60/EB030 rows read Return to
Lobby. Host pause Yes40h and results exits use the retained return. Do not
reintroduce the abandoned guest-to-browser/directory-retention change.

Admission requires transport enabled, non-Story mode, an active network session,
host/member role and a gameplay level0..100. Guest79094C is a level number,
not a multiplayer mode: the live fixture uses48;101..107 are frontends. Cancel,
local sessions, guest-owned actions and rematch callbacks retain their originals.

Host exit closes its local dialog, records progress with original668F0 during
play, and tears down local menu states through2EC650. Original65470 schedules
unload65F40 ->356920 and the reliable classB return. Clear advertised in-game
774178 before lobby advertisements resume. The guest3592F0 receiver follows
that same retained path without closing an absent pause dialog. Lobby ticks
1A8D30/1A8F30 finish original16/17 through3544B0, then3535C0 resets old match
clocks/caches once while preserving peers. Do not use356BB0 on that path.

Unplanned host loss is the original35C35B call returning35C360 into65420 at
phase15/16, or
lobby8D notifications from1CC4D9/1CC552. These select Online/System Link menu
4E92C4 and close the companion after original guest cleanup. A guest's voluntary
2EBDB0 call into65420 and its later phase18 retirement callback are excluded. Later stock teardown callbacks must not
overwrite an already pending retained return. The final owned-root factory
admits stock network descriptors including4E9574 and4E92C4; root construction
and unrelated child factories stay original. Transition diagnostics are numeric;
factory observations are capped at16 per return.

## Persistent profiles

Joining moves profile bodies into network player slots, while frontend1922D0
saves by persistent local identities. `pc_network_profiles.inc` owns the capture
and synchronous save mapping shared with Story. Capture before Online/System
Link browser entry. For a pending exit or an active host/member session, temporarily
present uniquely identified bodies in persistent slots for the original save call,
then restore network order
and propagate updated bodies back. Preserve aggregate pointers and original
metadata/checksum/error handling. Missing/ambiguous identities use the original.
Story retains its prior scope and consumes the same algorithm. Native Arcade
uses this helper too. Network phase11/12 alone is insufficient: without this
mapping, the live guest had an original save-error screen rather than its lobby.
The lobby factory consumes `multiplayer_destination` while the retained party's
profiles still occupy network slots. Do not use that short-lived destination as
the save mapper's sole guard: later stock saves can compare a remote profile's
CRC against a healthy local file and show the damaged-profile panel. Keep the
original read/CRC/write failure workflow; never dismiss or bypass this panel.
Story remains excluded from Arcade admission; disconnected sessions without a
pending exit keep the original save scope.

## Validation

`tests/network/multiplayer_exit.c` runs the production adapter through the
existing Windows/macOS `test_quit_menu.py` CI matrix. It checks host pause/results,
remote return, original guest actions, exact loss callers, multiple levels,
retained clocks/peers, captions, profile-body preservation and pointer restore.
`test_multiplayer_original_profile_save` executes original1922D0, its profile
helpers and original CRC routines against in-memory storage with different local
and remote profile CRCs. It reproduces error7 with the old destination-only scope,
checks repeated successful local saves with the fix, and preserves genuine CRC
and write errors. The fixture is already in the shared menu CI selection.
The original-US contract binds the host-loss call and Online menu constructor.
Existing original guest-reward/config fixtures execute the save and reset paths.
Keep CPU evidence distinct from paired gameplay and physical host acceptance;
STATUS.md records measured results and the native Mac/Linux/System Link gaps.

## Remaining three-platform save investigation

The user's Windows-host/WSL/Mac run at compatibility511dc174 still showed damaged
profiles on Windows/WSL and a missing Test Lab file on Mac. The Windows log entered
4E8D40 before the lobby factory consumed its destination; the prior late-save
scope fix is insufficient for that report. Native logs had no retained-return
adapter events, matching the wrapper-gate omission above. Native gates are now
repaired; the Windows failure remains under investigation until reproduced.
Numeric PC-PROFILE-SAVE diagnostics log the first16 dirty admissions, captured
metadata addresses/device/CRC/temp/body pointers, and original failure code/kind/
slot plus a CRC of the original save frame's scratch read buffer. That scratch
CRC is not proof that the read completed. No profile bytes/names are logged, no
error panel is hidden, and original signature/read/CRC/write behavior is retained.
