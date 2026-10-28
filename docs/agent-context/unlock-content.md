# Unlock Content implementation and validation

## Profile-scoped Unlock Content

`src/interface/pc_unlock_menu.inc` owns Extras > Unlock Content and the nine
independent Off/On categories. Extras keeps Movies and Credits, adds Unlock
Content, and exposes the original Cheats descriptor when any cheat is available.
Settings no longer adds Cheats. The original Cheats builder/tick/draw remain
unchanged and contain no early-access switch. The category panel uses 65% of
the native menu width, leaving space between labels and the On/Off selectors
at both 4:3 and widescreen sizes; compact row spacing keeps all nine visible.
Only category rows appear: there is no Back row or help message. The standard
Back input still saves and returns to Extras through UC_BACK.

`src/storage/pc_unlock_content.h` stores a versioned TSUC extension in the first
16 reserved profile bytes at US +1EE4, within the existing signed 1F54-byte body.
Only zero or valid v1 extension data is writable; unknown data is preserved.
No completion masks, medals, scores or pickups are changed. Off removes the
access override, so naturally earned progress (including new progress) survives.
The active primary profile is resolved through 356870(0)/696F0 on every check;
no process-global unlock flag or profile-pointer cache grants access. New-profile
69D90 initialization clears the extension; ordinary load/copy retains it.

The guest menu records its profile identity and declines edits after a switch.
Back uses the existing 1922D0 signed-save/error workflow. Its all-profile pass
preserves unrelated pending commits required by the game's global dirty bit;
only the chosen profile's extension changes. Disabling Cheats also removes
active cheat bits that no longer pass the original eligibility check.
The Windows/native Mac implementation is shared; native Mac visual/save checks
remain MAC-119. See `tests/storage/test_unlock_content.py` and the bounded
`unlock_content_game.py` runner (fresh progress in a disposable fixture copy).

## Profile early-access hooks

`patches/storage/2070-profile-unlock-content.patch` adds manual wrappers for
US 1CD3E0 (access requirements), 1CDA70 (48 catalog events), 1CD560 (movie/pickup
feature getter) and 69D90 (profile initialization). Regenerate through the normal
pipeline. `pc_unlock_menu.inc` grants only mapped content checks, retaining
zero/unknown disabled requirements. Event access is bounded to the 48 real
entries and their three-entry group; these mission requirements are initialized
at runtime, unlike the static group descriptors. Movie access is restricted to
the two Movies-menu calls (2CC17C/2CC1E9), never gameplay pickup checks.
The original completion/award getters and all progress writes stay unchanged.
Extras construction replaces indirect 2D08C0; callbacks 1237B..1237E occupy
verified US INT3 bytes. Cheats keeps original descriptor 523930 and callbacks.

## Unlock Content validation

`tests/storage/test_unlock_content.py` compiles the production menu/access
adapter, tests category/profile isolation, save routing, unknown metadata,
new-slot reset and progress preservation. With the supported local US XBE it
also executes the original access predicates and original tables (150 character
entries, 48 events, story/difficulty/co-op, maps, modes, weapons and cheats),
checks callee-saved registers/stack and verifies menu/padding/save ABI bytes.
CI runs the portable fixture on both hosts; private-XBE checks skip when absent.
`tests/storage/unlock_content_game.py` runs bounded Windows visual/save/reload
acceptance using a disposable fixture copy whose progress is explicitly fresh.
It shares the existing command runner, isolates every save/settings path and
never edits the canonical Test Lab fixture or personal files.

Final Windows acceptance and native limits: [STATUS](../project/STATUS.md#profile-scoped-unlock-content-2026-10-28).
