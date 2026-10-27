# Marvel vs Capcom 2: unlock state, and a shop-points switch

29 September 2026. Title 43430007, XDK build per `default_xdk_symbols.json`.

What is implemented is small: an opt-in switch that sets the shop's points
balance to 9999, so a player can buy the hidden characters, art and colours
through the game's own shop. The rest of this file records where the game keeps
its unlock state, so that a proper "unlock everything" toggle can be built on it
later.

Marks: **[V]** verified against this binary (lifted C in
`titles/mvc2/src/recomp/gen`, or the XBE bytes); **[R]** verified by running it;
**[I]** inferred from the code; **[U]** unverified, from memory or public
knowledge. Everything below was found by reading the lifted code unless it says
[R].

---

## The switch: RECOMP_MVC2_MAX_POINTS

```
RECOMP_MVC2_MAX_POINTS=1            environment variable
max_points = 1                      or in %APPDATA%\xboxrecomp\titles\43430007.conf
```

Off by default. The environment wins over the file
(`recomp_config_bool`, `src/config/recomp_config.h`). When it applies, stderr
says so:

```
[MVC2] shop points set to 9999 (RECOMP_MVC2_MAX_POINTS, profile loaded)
```

It is two wraps in `titles/mvc2/src/recomp_manual.c`, using the manual-file
wrap mechanism (`extern void sub_X_gen(void);` makes the recompiler emit the
generated body under that name; `tools/recomp/manual_scan.py` `_WRAP_RE`):

| function | what it is | the wrap |
|---|---|---|
| `sub_001DEBA0` | a new profile's defaults [V] | runs the body, then writes 9999 [R: the log line printed on a 60 s idle run, and the save files were byte-identical afterwards] |
| `sub_001DFB40` | save block -> live state [V] | runs the body, then writes 9999 when `al == 1` on entry |

The loaded-save path has not been seen to fire yet, because the save is loaded
later in the front end than an idle run reaches.

This is the first title to use a wrap. The mechanism was broken until
29 Sep 2026: generated callers called `sub_X_gen` directly, so a wrapper
was never reached (fixed in the recompiler with a `call_name`).

`sub_001DFB40` begins `eax = ZX8(LO8(eax)); eax--; if zero goto ...`, so its
first argument is `al`, and `al == 1` is the path the load routine takes after a
load that passed its checks. That path stores `MEM16(0x676422)`. The other `al`
values copy other data and are passed through untouched [V].

**The launcher cannot show this yet.** `recomp_settings_write`
(`src/config/recomp_config.c`) rewrites the whole settings file from the fixed
`RecompSettings` struct, so a hand-added `max_points` line is lost the next time
the launcher saves, and the launcher has no per-title rows (`build_rows` in
`src/launcher/launcher.c`). Until that changes, use the environment variable.
Making titles able to declare their own launcher options is toolkit work.

**Saves.** Nothing is done at save time. The 9999 balance, and everything bought
with it, go into the real profile the next time the game saves, and stay there
with the switch off. That is the intent: the player unlocks things through the
game and keeps them. Turning the switch off does not take points away; it stops
topping them up.

---

## Where the unlock state lives

### The save file [V]

- `save.dat` (string at `0x2D5BC8`) in a save container named "GAME DATA"
  (`0x2D5C50`). On this runtime:
  `games/Marvel Vs Capcom 2/UDATA/43430007/<save id>/save.dat`, because
  `\Device\Harddisk0\Partition1\` maps to the game folder
  (`src/kernel/kernel_path.c`).
- Load `sub_001E04D0`, save `sub_001E0CC0`.
- Layout: header `MVLVS.C2_SYS` (the Dreamcast system file's name, 0xC bytes at
  `0x2D57F0`), two checksum words, a 0x754-byte block and a second copy of it
  (0xEBC in all), a 20-byte `XCalculateSignature`, and one more byte.
- The load rejects the file if the signature (`sub_001E6190`, a `repe cmpsb`)
  or either checksum (`sub_001E02A0` byte sum, `sub_001E0300` word sum) does not
  match. So editing `save.dat` is the wrong route: the signature is keyed by the
  title, and editing does nothing when there is no save yet.
- The block lives at guest `0x33B898`-`0x33BFEB` while it is packed or unpacked.

### Block to live state [V]

Unpacked in `sub_001DFB40` (around `recomp_0015.c:56306-56380`), packed in
`sub_001E06B0` (around `58391-58712`):

| block | live |
|---|---|
| `0x33B8AC` / `B0` | profile+0x30 / +0x34 (characters) |
| `0x33B8B4` / `B8` | profile+0x38 / +0x3C (second per-character item) |
| `0x33B8A6` | points `0x676422` |
| `0x33B8DC` / `E0` | gallery `0x676444` / `48` |
| `0x33B8BC` | shop level `0x5F91AA` |
| `0x33B8A4` / `A5` | level `0x676420` / `21` |

The profile pointer `[0x6555A0]` is always `0x6555E0` (set at `0x001DEBA9`), so
the profile fields have static addresses [V].

### Live addresses

| what | address | notes |
|---|---|---|
| characters | `0x655610` / `0x655614` | bit per character [V] |
| second per-character item | `0x655618` / `0x65561C` | probably alternate colours or costume [I] |
| gallery art | `0x676444` / `0x676448` | bits line up with the `GAL00`-`GAL3A.TM2` files [V] |
| shop level (u8) | `0x5F91AA` | 0-7; probably gates the EX options [I] |
| points balance (u16) | `0x676422` | what the shop spends; capped at 9999 in `sub_0001A9D0` [V] |
| second points total (u16) | `0x676426` | also capped at 9999 (`sub_0001AA60`) [V] |
| level | `0x676420` | 0-99, derived from both totals [V] |

- **The game's own "everything" value** is `0xF8FFFFFF` / `0x07FFFFFF`: bits
  0-0x3A except 0x18-0x1A, the three boss slots. The game recognises it and
  sets a flag at `0x5C9DCD = 1` (`sub_0016FD40`, `sub_0016E2B0`) [V].
- **The default roster** is `0x00FFFF77` / `0x02000004`, set in `sub_001DEBA0`
  and repeated in the character-select mask builder. That is 24 characters of
  56 [V for the counts; that these match the retail game is U].
- **The name table is not a reliable map.** Mapping bits through the debug
  name table at `0x2D1BB8` ("00 RYU" ... "3A KOBUN") gives an odd result:
  "07 WOLVERINE" is locked by default and "39 HONE WOLVERINE" is not. Bits
  may not map one to one to those names [I].
- **The gallery viewer** loads file `0x391 + index` from the table at
  `0x2E0B30`, and `GAL18`-`GAL1A` are missing, matching the boss slots [V].

### The shop [V]

- Purchase is `sub_00017D60`. It checks the price at `0x3354B0[slot*3]`
  against `0x676422` and subtracts it. There are four slots; the item ids are
  at `0x3354A8[slot]`.
- Slots 1 and 2 are characters: they set a bit in profile+0x30 (id < 0x20) or
  +0x34.
- Slot 3 is art: `sub_0001AC50(id)` sets a bit in `0x676444`/`48`. The id is a
  random unlocked character whose art is not yet owned (`sub_00017390`). Price:
  100 × (arts owned + 1).
- Slot 0 is one of two things. Either it raises the shop level `0x5F91AA` by
  one, to 7, priced (level + 2) × 500 (`sub_00017350`). Or it sets a bit in
  profile+0x38/+0x3C for an already unlocked character (`sub_00017260`).
- Nothing else is sold. No stage unlocks were found.

---

## A proper "unlock everything" toggle, for later

Wrap three functions and keep the real values aside, so the unlock is session
only and the real save never records it:

1. `sub_001DEBA0` (new profile): run the body, remember the real values, write
   the unlock.
2. `sub_001DFB40` with `al == 1` (loaded): the same.
3. `sub_001E06B0` (pack, before every save): put the remembered real values
   back, run the body, write the unlock again.

The values to write: `0xF8FFFFFF` / `0x07FFFFFF` into the character, second
item and gallery masks, and 7 into `0x5F91AA`. A switch
`RECOMP_MVC2_UNLOCK_ALL`, read the same way as `max_points`.

Without step 3 the next autosave makes the unlock permanent. Points earned in
an unlocked session would still persist, which is probably what a player
wants.

A generic Action Replay-style "write these values every frame" mechanism would
serve other titles too, but alone it has the same persistence problem, and
there is no per-title frame hook today.

**First experiment.** `RECOMP_WATCH_WRITE=0x655610`
(`docs/technical/memory-watchpoints.md`) while driving to character select.
The watch should name `sub_001DEBA0`, then `sub_001DFB40`, as the writers,
which confirms the hook points. Then add step 1-3 for the character mask alone
and check the roster shows 56 in a frame capture.

---

## Open

- What profile+0x38 and the shop level look like in the game [I/U above].
- Whether the bit-to-character mapping matches the name table.
