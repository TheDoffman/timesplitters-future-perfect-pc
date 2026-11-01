# Complete game language packs

## Console language and included packs

`platform/2310-game-language.patch` adds `tsfp_game_language` in kernel_xbox.c.
`TSFP_GAME_LANGUAGE` en/de/fr/es/it maps to Xbox IDs 1/3/4/5/6; absent or invalid
values fall back to English. Both XC_LANGUAGE and the whole/user EEPROM images
use that helper, retaining valid section checksums and console region policy.
The guest chooses its own complete-language assets; there is no pack rewrite.
The supported US XBE's XGetLanguage 0037D3BC -> game mapping 000229C0 -> startup
00070360 sets language global 004C0454; archive lookup 00350CC0 uses its original
005373E4 table. `tests/compiler/test_game_language.py` executes the production
query/image paths plus those four translated original routines and verifies all
five sounds archives. Original routines require the local verified XBE; no game
code/data is tracked. Listening/full localized gameplay remains separate.

US language-gate follow-up: the live US title sets region flag 0074CF60=1,
so 00070360 normally skips XGetLanguage and leaves 004C0454=0. The earlier
four-routine test zeroed that flag and missed the failure. Shared
src/interface/pc_game_language_guest.h now adapts only the read at 00070360
when TSFP_GAME_LANGUAGE is valid, leaving the region field and other consumers
unchanged. It also adapts the English-reset store at 0003C308 before text reload.
The existing pc_native_numeric.h load/store composition consumes these hooks at
both original and native frame rates. Tests execute both region values and the
alternate original boot routine 0003C2D0; native_timing_guest.c verifies actual
production macro wiring and unchanged unrelated accesses. See STATUS for live
verification; EEPROM consistency alone never proves title asset selection.

The portable EEPROM fixture adapts inherited printf diagnostics to inttypes.h
32-bit formats, casting NTSTATUS for hexadecimal output. Non-Windows compilers
treat vendor include directories as system headers (the pinned kernel header
contains a nested-comment warning); extracted implementation/oracle code keeps
-Werror and all inherited assertions/checksum checks. Vendor bytes are unchanged.
