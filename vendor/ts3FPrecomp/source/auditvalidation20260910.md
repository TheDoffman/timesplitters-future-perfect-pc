# Audit validation brief — 10 September 2026

An external review of branch `claude/xbox-recomp-compatibility-4nx2mn` (68 commits
past `origin/main`, head `d56998b`) made the claims below. This document turns each
claim into something you can check, and says what to do if it holds.

**How to work this document**

1. Go top to bottom. Severity order is dependency order: the first item changes the
   result of every measurement after it.
2. For each item, run the *Check* exactly as written before reading the *Fix*. Record
   the result in the *Status* line as one of `CONFIRMED`, `REFUTED (reason)`, or
   `PARTIAL (what differs)`. A refuted claim is a finding too; do not delete it.
3. Apply a *Fix* only after the check confirms the claim. Every fix carries an
   *Acceptance* test; the fix is done when that passes and the full suite still does:

   ```bash
   python3 -m unittest discover -s tools -p "test_*.py" -t .    # 122 pass, 1 skip at d56998b
   ```

4. Every disassembler or lifter change must add **two** tests: one built from the
   failing layout, one built from the most common layout the heuristic will see.
   The regression in V1 passed its own test because only the failing layout was
   encoded.
5. Where a Burnout 3 build is available, record its function count and frontier
   before and after any disassembler change, in the commit body. Nothing on this
   branch records a Burnout 3 run; a second title is the cheapest regression gate.
6. Never hand-edit generated C. Fix the tool and regenerate.
7. Commit each item separately, with the item ID in the subject
   (`disasm: V1 — skip the jmp's own displacement in the backward table scan`).

---

## V1 · Backward switch-table scan deletes the dispatch jump — CRITICAL

**Claim.** `resync_jump_tables` (`tools/disasm/engine.py:293–311`, commit 59b75ce)
reads the dword at `tbl - 4` as a table entry. MSVC places a switch table directly
after `jmp dword ptr [reg*4 + disp32]` (`FF 24 8D disp32`), so `tbl - 4` is the
jump's own displacement, whose value is `tbl`, which is inside the section. The scan
accepts it, moves the table start back one slot, and the cleanup loop at
`engine.py:312–315` deletes the jump instruction. `_find_function_end`
(`functions.py:1141–1147`) then finds neither a table nor an instruction at the
dispatch and ends the function there. Every function with an inline switch is
truncated, on every title. `test_jump_tables.py` passes only because its fixture
puts the table 0xE0 bytes after the jump.

**Check.** Save as `scratch/repro_inline_table.py` and run from the repo root.

```python
"""Inline switch table: MSVC layout, table immediately after the 7-byte jmp."""
import struct, sys
sys.path.insert(0, ".")
from tools.disasm.test_jump_tables import _Image, BASE
from tools.disasm.engine import DisasmEngine

img = _Image(BASE, bytes(0x400))
jmp_off = 0x20
table_va = BASE + jmp_off + 7
img.code[jmp_off:jmp_off + 3] = b"\xff\x24\x8d"
img.code[jmp_off + 3:jmp_off + 7] = struct.pack("<I", table_va)   # index 0 = table start
targets = [BASE + 0x200 + 0x10 * i for i in range(4)]
for i, t in enumerate(targets):
    off = table_va - BASE + i * 4
    img.code[off:off + 4] = struct.pack("<I", t)
    img.code[t - BASE] = 0xC3

eng = DisasmEngine(img)
eng.linear_sweep(img.sections[0])
eng.resync_jump_tables()
print("jump_tables:", {hex(k): hex(v) for k, v in eng.jump_tables.items()},
      "expected", hex(table_va), "->", hex(table_va + 16))
print("jmp at", hex(BASE + jmp_off), "->", eng.instructions.get(BASE + jmp_off))
```

Expected on `d56998b` (confirms the claim):

```
jump_tables: {'0x11023': '0x11037'} expected 0x11027 -> 0x11037
jmp at 0x11020 -> None
```

Expected on `origin/main` (correct behaviour):

```
jump_tables: {'0x11027': '0x11037'} ...
jmp at 0x11020 -> Instruction(... mnemonic='jmp' ...)
```

Also check the lifter's copy: `tools/recomp/lifter.py:1928–1941` (commit a5f4dc5)
has the same backward scan, and `_analyze_switch_table` (near `lifter.py:1976–1980`)
stops at the first out-of-function entry walking from the front, so one spurious
leading word discards the whole switch.

**Fix.** In both places, before accepting a backward candidate at `addr`:

- reject it if the dword's value equals the current table address, or
- reject it if an instruction in `self.instructions` ends exactly at `tbl` and
  `addr >= that instruction's address` (the word belongs to the jump).

Prefer the second form; it is the actual invariant.

**Acceptance.**

- Add `test_a_table_directly_after_its_dispatch_is_not_extended_backwards` to
  `tools/disasm/test_jump_tables.py` using the layout above; it must fail before
  the fix and pass after.
- Add the same layout for the lifter's reader.
- The existing `test_a_table_addressed_from_its_last_entry_is_found` still passes.
- Regenerate Burnout 2 and compare against the previous run: function count,
  `[PROFILE] N functions entered`, unresolved ICALL count. Record all three in the
  commit body. If a Burnout 3 build exists, do the same there.

**Status.** `CONFIRMED` as a defect, `PARTIAL` on severity. Fixed in `03c636f`.

The repro reproduced exactly: table start moved to `0x11023`, dispatch jump
deleted. Fixed by flooring the backward scan at the dispatch instruction's
`end_address`, looked up by `insn.jump_table` — testing `target == tbl` only
catches the index-0 layout, which is how a first attempt passed one new test
and failed the other. Two tests added in the inline layout; 129 pass.

The severity claim does not hold for this title. Across the whole image there
are 190 `jmp [reg*4+disp32]` sites (.text 177, D3D 9, XNET 3, XONLINE 1) and
**none** place the table immediately after the jump, so Burnout 2 regenerated
byte-identically: 35,598 functions, 2,157,398 bytes covered, before and after.
Nothing measured on this branch is invalidated. The bug was latent here.

---

## V2 · Burnout 2's seeds are applied by default to any XBE — HIGH

**Claim.** `config/seed_functions.json` holds 214 Burnout 2 addresses with no title
key. `scripts/recompile.py:160` loads it for every XBE. `tools/disasm/disasm.py:164–215`
rejects a seed only when it lands mid-instruction, so a foreign address on an
instruction boundary becomes a fake function start and splits the real function it
sits in.

**Check.**

```bash
python3 -c "import json; d=json.load(open('config/seed_functions.json')); print(len(d), d[0])"
sed -n 155,165p scripts/recompile.py
python3 scripts/make_test_xbe.py --help          # build a synthetic XBE into scratch/
python3 scripts/recompile.py scratch/<synthetic>.xbe --dry-run | grep -n seed
```

Confirms if the dry run shows `--seed-functions config/seed_functions.json` for an
XBE that is not Burnout 2.

**Fix.** Key seeds to the title. Options, in order of preference:

1. `config/seeds/<title-id-hex>.json`, title ID read from the XBE certificate
   (`tools/xbe_parser` already exposes it). The driver loads only the matching file.
2. A `"title_id"` field in the seed file; `recompile.py` and `tools.disasm` refuse
   a file whose title does not match the XBE and say so.

`tools/seed_from_log` should write the title ID it observed into the file it emits.
Move the existing 214 entries to the Burnout 2 file.

**Acceptance.** A dry run on a synthetic XBE shows no seed file. A dry run on
Burnout 2 shows its file. A seed file with the wrong title ID makes the driver exit
non-zero with a message naming both IDs. Add a unit test for the refusal.

**Status.** `CONFIRMED`. Fixed in `069ecd1`.

Seeds are now `config/seeds/<TITLEID>.json` and the driver loads only the
matching file; Burnout 2's 214 moved to `41430019.json`. Measured on five
discs: Burnout 2 gets its file, Dead to Rights, Def Jam, Time Splitters 2 and
Outrun 2 each get none. `seed_from_log` defaults to the same path and warns on
a mismatch. Five tests added.

---

## V3 · Driver, template and walkthrough disagree on where generated code goes — HIGH

**Claim.** `tools/recomp/__main__.py:211` defaults to `src/game/recomp/gen` inside
the toolkit. `templates/new-game/CMakeLists.txt:72` globs `src/recomp/gen/*.c`
inside the project. The template comment at line 67 says files are `gen_0000.c`;
the recompiler writes `recomp_0000.c`. `INSTRUCTIONS.md` step 4 never mentions
`--gen-dir`. Following the walkthrough builds a project with zero lifted functions,
and step 7's gate passes anyway.

**Check.**

```bash
grep -n "gen-dir\|src/game/recomp/gen" tools/recomp/__main__.py scripts/recompile.py
grep -n "recomp/gen\|gen_0" templates/new-game/CMakeLists.txt
grep -n "gen-dir" INSTRUCTIONS.md CLAUDE.md
```

Confirms if the last grep is empty and the first two show different paths.

**Fix.** Pick one: (a) `recompile.py` takes `--project DIR` and defaults `--gen-dir`
to `DIR/src/recomp/gen`; or (b) step 4 of `INSTRUCTIONS.md` and the stage-by-stage
block in `CLAUDE.md` pass `--gen-dir ../my-game/src/recomp/gen` explicitly. Fix the
`gen_0000.c` comment either way. Consider making the template's CMake fail at
configure time when the glob matches nothing, so step 7's gate means something.

**Acceptance.** Following `INSTRUCTIONS.md` steps 4–7 literally, on a synthetic XBE
and a fresh copy of the template, produces a project whose glob matches the generated
files. Configure with an empty `gen/` fails with a message.

**Status.** _______

---

## V4 · NV2A interrupt status latches forever once the vblank chain is on — HIGH

**Claim.** Commit c0ad587 stops the ack thread clearing `PMC_INTR_0` and
`PCRTC_INTR_0` while `s_vblank_owns_intr` is set (`src/kernel/xbox_memory_layout.c:186, 585`).
`kernel_vblank_tick` sets the bits (`src/kernel/kernel_bridge.c:2029–2030`). The NV2A
aperture is plain committed memory (only page 0 and the APU range are `PAGE_NOACCESS`),
so a guest write-1-to-clear from the ISR changes nothing, and nothing else in the
tree clears the bits. After the first vblank both stay set. That is the re-entry
hazard the ack table was written for after Halo's native stack overflow. The commit
measured `0 -> 0x01000000`, never the return to zero. `s_vblank_owns_intr` is a plain
`int` written by the timer thread and read by the ack thread.

**Check.**

```bash
grep -rn "PMC_INTR_0\|PCRTC_INTR_0\|0x600100\|vblank_owns" src/kernel/*.c | grep -v "^\s*//\|\*"
```

Look for any `&= ~` or `= 0` on those registers that runs after the tick sets them.
Then, at runtime with `RECOMP_VBLANK` set:

```
RECOMP_WATCH_VA=0xFD000100 ./build/<title> 2>run.log
grep "0xFD000100" run.log | head
```

Confirms if the watch shows the transition to `0x01000000` and never back to `0`.

**Fix.** Either clear the bits when the ISR returns (in the raise/dispatch path that
calls the guest ISR), or map the PCRTC page so writes trap and implement
write-1-to-clear. Change `s_vblank_owns_intr` to `volatile LONG` and use the same
interlocked pattern as `g_nv2a_ack_stop`.

**Acceptance.** The watch shows `0 -> 0x01000000 -> 0` once per frame. A Halo build,
if available, with `RECOMP_VBLANK` set does not overflow the native stack.

**Status.** `CONFIRMED`. Fixed in `9f5931b`, and it exposed the next blocker.

Before: the watch on `0xFD000100` showed one transition, `0 -> 0x01000000`,
and never a return to zero — one vblank in a 45-second run. After: 1,752
transitions cycling `0 -> 0x01000000 -> 0`, the vblank ISR from 1 entry to
2,774 and its DPC from 1 to 2,773. `s_vblank_owns_intr` is `volatile LONG`
behind the same interlocked pattern as `g_nv2a_ack_stop`.

The ack runs as soon as the ISR returns, which is where hardware clears it.
Acking after `kernel_drain_dpcs()` was tried first and is wrong: it leaves the
status set while the DPC runs, a state hardware never presents.

**Still blocked, separately.** D3D8's vblank DPC reads a pending-event mask at
`[[context]+0x100]`, tests it against `0xFFFFEFFF`, finds nothing, and takes
its early exit on all 2,773 entries. So `sub_000B86C0` — the vertical-blank
callback the title hands D3D8 at `0x000B8D3D` — still never runs, the counter
at `0x005518FC` it feeds is still zero, and the loader still spins in
`sub_000CC5A0`. The ISR claims the interrupt without recording what it was,
most likely reading status registers beyond the two this tick sets.

---

## V5 · cdecl cleanup detection misses the shape the lifter usually emits — MEDIUM

**Claim.** `tools/recomp/translator.py:145–156` (commit 113aa7d) inspects only the
first non-label line after the indirect call. In any function with `needs_cf` the
lifter emits `_cf = (int)(...)` before `esp = esp + 0x10;`, so the probe sees the
flag line, classifies the site as stdcall, and the double cleanup returns. MSVC's
`pop ecx` cleanup and deferred cleanup after several calls are also missed.
`test_icall_convention.py` uses only the flag-free shape.

**Check.** Lift `add esp, 0x10` inside a function that needs the carry flag (any
function containing a later `adc`/`sbb`/`jc`) and print the emitted lines after an
indirect call. Confirm the `_cf =` line precedes the `esp =` line, then step the
probe by hand or with a print.

**Fix.** Scan forward past `_cf`/flag assignments to the first write of `esp`;
recognise `esp = esp + 4` produced by `pop ecx`/`pop edx`; bound the scan at the
next call or label.

**Acceptance.** Add a `needs_cf` fixture and a `pop ecx` fixture to
`test_icall_convention.py`; both fail before and pass after. Existing six still pass.

**Status.** _______

---

## V6 · "DSP command" registers are AC'97 bus-master control registers — MEDIUM

**Claim.** `src/apu/apu_mmio_hook.c:278–284` names 0xFEC0011B and 0xFEC0017B
`MCPX_DSP_GP_CONTROL` and `MCPX_DSP_EP_CONTROL`. They are the AC'97 PCM-Out and
SPDIF-Out control registers (bus-master base 0x100 and 0x170, control register at
+0x0B). Bit 0x02 is Reset Registers, which hardware self-clears, which is why
clearing it on write is exactly right. The behaviour is correct; the names and the
comment about a DSP are not.

**Check.** Compare against a public AC'97 bus-master register map (Intel ICH AC'97
spec, or xemu's `hw/xbox/mcpx/apu` and `hw/audio/ac97.c`): NABMBAR + 0x10 is the
PCM-Out block, +0x1B its control register, bit 1 RR. Confirm 0xFEC00000 is the AC'97
base on the MCPX, not the APU DSP.

**Fix.** Rename the constants to AC'97 names, rewrite the comment, and leave the
behaviour. Add the write decoder to `tests/mmio_decode` (it is a pure function) and
give that directory a `CMakeLists.txt` so it is actually built.

**Acceptance.** `grep -rn DSP src/apu/apu_mmio_hook.c` returns only genuine DSP
references. `tests/mmio_decode` builds and passes.

**Status.** `CONFIRMED`. Already fixed in `0e255d4`, before this document
arrived — an independent audit of the same commits reached the same
conclusion. Renamed to `MCPX_AC97_BM0_CR` / `MCPX_AC97_BM1_CR` /
`MCPX_AC97_CR_RR`, comment rewritten around the NABM block and RR's
self-clearing behaviour, behaviour unchanged. `tests/mmio_decode` still has no
`CMakeLists.txt`; not addressed.

---

## V7 · APU wiring lives in the out-of-tree project, not the template — MEDIUM

**Claim.** Commit 5918a4d says "the project's main.c creates the device and routes
faults". `templates/new-game/src/main.c` references none of `apu_hook_handle_mmio`,
`mcpx_apu_init_standalone`, `mcpx_ac97_handle_write`. A third title copied from the
template gets the 0xFE8xxxxx crash the commit fixed. `g_ac97_page_trapped` is set
(`xbox_memory_layout.c:1692`) and never read. `g_contig_blocks[512]` has no overflow
check.

**Check.**

```bash
grep -n "apu_hook_handle_mmio\|mcpx_apu_init_standalone\|mcpx_ac97_handle_write" templates/new-game/src/main.c
grep -rn "g_ac97_page_trapped" src/
grep -n "g_contig_blocks\[" src/kernel/*.c
```

**Fix.** Port the VEH routing into the template's `main.c`. Better: move the whole
fault-routing switch (NV2A, APU, AC'97 ranges) into the runtime behind one
`xbox_handle_fault(ctx)` so a project installs a single handler. Read or delete
`g_ac97_page_trapped`. Bound `g_contig_blocks` and log when full.

**Acceptance.** A fresh copy of the template, built against a title that touches
APU MMIO, reaches `[APU]` log lines instead of an access violation at 0xFE8xxxxx.

**Status.** `PARTIAL`, not yet fixed. `g_ac97_page_trapped` was already
removed in `0e255d4`, and the duplicate `XBOX_MCPX_AC97_PAGE` in `apu.h` with
it. The template still has none of the APU wiring, and `g_contig_blocks[512]`
still has no overflow log — both open. The suggested `xbox_handle_fault(ctx)`
in the runtime looks right: three ranges are already routed by hand in one
project's `main.c`.

---

## V8 · `--game-only` still stubs RenderWare and float-initialiser code — MEDIUM

**Claim.** `tools/recomp/__main__.py:109–137` includes `game_*`, `unknown` and `crt`.
The ten `rw_*` categories (`tools/func_id/config.py:340`) and `data_init`
(`stub_classifier.py:75`) remain excluded, though nothing HLEs either. Commit
afb25ce's own argument, "excluding library code is only sound once something
implements it", applies to them.

**Check.**

```bash
sed -n 105,140p tools/recomp/__main__.py
grep -n "rw_" tools/func_id/config.py | head
python3 -m tools.recomp <burnout2>.xbe --game-only --dry-run 2>&1 | grep -i "excluded\|categories"
```

Count the `rw_*` and `data_init` functions the summary reports as excluded.

**Fix.** Include every category except those with a registered native replacement,
and make the flag's help text list that set precisely.

**Acceptance.** On Burnout 2 the translated function count rises by the excluded
count and the unresolved ICALL count does not rise. Add a `game_categories` unit
test.

**Status.** _______

---

## V9 · Docs contradict the code — MEDIUM

**Claim.** Four discrepancies:

- `docs/pipeline/05-runtime.md:334–340`, `docs/pipeline/06-debugging.md:296`,
  `tools/README.md:275` direct overrides to `title_overrides.c`, which the branch
  reverted; `CLAUDE.md` and `INSTRUCTIONS.md` say `recomp_manual.c`.
- `INSTRUCTIONS.md` Part II's seed loop runs `tools.disasm --text-only --seed-functions`
  then `recompile.py --from identify`. The driver deliberately avoids `--text-only`
  (`scripts/recompile.py:41–52`) and already applies seeds. `CLAUDE.md`'s
  stage-by-stage block also uses `--text-only -v`.
- `docs/formats/xbe.md` omits `PeBaseAddress` at 0x013C and shifts later header
  fields by 4 (thunk 0x0154 vs actual 0x0158, libs 0x0160 vs 0x0164). Pre-existing;
  the survey docstring cites it.
- `INSTRUCTIONS.md` implies the walkthrough runs on Linux; steps 7 onward need
  Windows.

**Check.**

```bash
grep -rn "title_overrides" --include=*.md . tools/README.md
grep -n "text-only" INSTRUCTIONS.md CLAUDE.md scripts/recompile.py
grep -n "0x013C\|0x0158\|0x0164\|0x0154\|0x0160" docs/formats/xbe.md tools/xbe_parser/*.py
cmake -B scratch/build -S . 2>&1 | tail -3      # stops at SDL2 on Linux
```

**Fix.** Replace the three `title_overrides.c` references; rewrite the seed loop as
`python3 scripts/recompile.py <xbe> --from disasm`; correct the header table in
`docs/formats/xbe.md` against `tools/xbe_parser`; add a one-line "Windows required
from step 7" note under Part I.

**Acceptance.** The greps return nothing stale. Each command in `INSTRUCTIONS.md`
Part I and II runs as written on a synthetic XBE.

**Status.** _______

---

## V10 · Script robustness and runtime hazards — LOW

Each is small; check with the command, fix if confirmed.

| ID | Claim | Check | Fix |
|---|---|---|---|
| V10a | `scripts/pb_unhandled.py:47` reads `sys.argv[1]`; `--help` tracebacks | `python3 scripts/pb_unhandled.py --help` | argparse |
| V10b | `scripts/stackwalk.py:82` default `--functions` is cwd-relative | run from `/tmp` | resolve from `__file__` like `pb_unhandled.py` |
| V10c | `scripts/stall_report.py:32` regexes accept only `sub_` names; `xbe_entry_point` and `ghidra_naming` output vanish | run on a profile with renamed functions | match any C identifier the dispatch table knows |
| V10d | `scripts/xemu_probe.py:133–150` labels xemu addresses with *this runtime's* layout; `settimeout` persists after `halt()` | read `describe()` | label from the real kernel layout or drop labels; reset timeout |
| V10e | `scripts/run_and_report.py:84–99` keys on strings only the template's VEH prints | grep the runtime `src/` for `[CRASH]`, `Xbox VA of fault` | emit those lines from the runtime, or document the contract |
| V10f | `bridge_log_guest_text` in `src/kernel/kernel_bridge.c` reads `length` bytes with no upper bound | read it beside `bridge_guest_string` | bound like its neighbour |
| V10g | `xbox_ContiguousAlloc` has no lock; DPCs run on the timer thread, guest threads are real threads | grep `g_contig_next` for a lock | lock, or document that the "cooperative single-thread model" in `CLAUDE.md` is aspirational |
| V10h | `src/kernel/kernel_xbox.c:206` hard-codes NTSC in XC_VIDEO, so `RECOMP_XBOX_REGION=pal` disagrees with AV_REGION | read both | derive from region |
| V10i | Commit 083dd6f's body says the stale `STACK_ARG(0)` was the return address; the dispatcher pops it first (`kernel_bridge.c:5395–5396`) so it was the caller's frame slot | read the dispatcher | note only; fix is correct |
| V10j | `block_extent_end` (`engine.py:493`) is dead after 4fb34f4 but keeps tests | grep callers | delete or mark |

**Status.** Not worked, except V10i, which is noted: the commit body for
`083dd6f` says `STACK_ARG(0)` was the return address. The value read was
garbage either way and the fix is right, but the description is wrong.

---

## V11 · Foundations untouched, Win32 surface grew — STRATEGIC

**Claim.** `CLAUDE.md` names four changes to land before bulk codegen: base+offset
memory model, per-function register context, x87 policy, yield checks at loop
back-edges. None is started. The branch adds `VirtualProtect`, a `PCONTEXT` x86-64
decoder, `GetTickCount64` and a `WINAPI` thread. Fixed fake-structure VAs
`0x00760000–0x00770000` (`xbox_memory_layout.c:1421–1468`) collide with any title
whose image or BSS reaches them.

**Check.**

```bash
grep -rn "MapViewOfFileEx\|CreateFileMapping" src include templates | wc -l
grep -rn "long double\|float80" tools/recomp/*.py | grep -v test_ | wc -l      # 0
grep -n "FAKE_PRCB_VA\|FAKE_TLS_VA\|0x00770000" src/kernel/xbox_memory_layout.c
```

**Action.** Not a fix. Write down, in `CLAUDE.md`, the milestone at which bulk
codegen stops and the memory and register model work starts (suggested: Burnout 2
reaches a navigable menu), and an estimate of the retrofit cost at the current
function count versus after that milestone. Make the fake-structure VAs derive from
the mapped image's end rather than constants.

**Status.** _______

---

## Refuted or partial claims

Record here anything above that did not hold, with the evidence. This section is as
valuable as the fixes.

| ID | Result | Evidence |
|---|---|---|
| V1 (severity) | `PARTIAL` — the defect is real, the blast radius is not | 190 `jmp [reg*4+disp32]` sites in the image, 0 with the table immediately after the jump. Burnout 2 regenerates byte-identically across the fix: 35,598 functions, 2,157,398 bytes. "Every function with an inline switch, on every title" is true of the mechanism and false of this title; the claim that it invalidates the branch's measurements does not hold. |
| V4 (sufficiency) | `PARTIAL` — fixing the latch does not deliver a vblank callback | The chain now cycles 1,752 times, and D3D8's DPC still early-exits on every one because the pending mask at `[[context]+0x100]` is empty. A second defect sits behind this one. |
