# Recompiling a game

Turning an Xbox game you own into a native executable, start to finish.

You need the game files from your own disc — none ship here, and none ever will.

**Part I** gets you from a shelf of ISOs to a program that builds and runs.
**Parts II–V** are the campaign after that: getting it to actually run, getting
it *right*, and only then making it better.

> **New here?** Read Part I top to bottom before running anything. Each step
> assumes the previous one worked.

---

## Before you start: what this actually costs

**Playable is a long way past booting, and booting is a long way past
compiling.** One title has reached playable with this toolkit. The most advanced
downstream port — purpose-built runtime, months of work — reports the game
booting at ~29 fps with save, audio and movie gates passing, and still describes
itself as not playable.

Plan for a campaign, not an afternoon. The good news: the work is heavily gated.
You always know what the next blocker is, because the program tells you. The
failure mode is not "stuck with no information", it is "many small things in
sequence".

Two rules decide whether that sequence is progress or thrash:

1. **Never hand-edit generated C.** Fix the lifter and regenerate. A hand-edit
   vanishes on the next pass and hides a bug that will hit the next title too.
2. **Track a frontier, not a feeling.** Your frontier is the furthest *natural
   game event* you have observed — "reaches the title screen", not "ran longer".
   A crash-free run that reaches nothing new is not progress.

---

# Part I — From a disc to a first run

## 0. Install the dependencies

**Linux / WSL:**

```bash
bash tools/linux/install_deps.sh
pip install capstone
```

**Windows:** Visual Studio 2022 with the C++ workload, CMake 3.20+, Python 3.10+,
then `pip install capstone`.

`capstone` is the only requirement for the pipeline itself. The optional
`tools/fusion` analysis also needs `pefile` and `numpy`; without them its tests
error out and everything else still works.

---

## 1. Prove the toolchain before blaming the game

Do this once, before a game is involved. When something breaks later, you want
to already know the lifter is sound.

```bash
python3 -m unittest discover -s tools -p "test_*.py" -t .
```

On Windows with a 32-bit MSVC available, also run the differential conformance
harness:

```bash
py -3 -m tools.conformance
```

It assembles snippets, runs them **on your real CPU**, lifts the same bytes,
runs the lifted C, and compares. Because we target x86 and run on x86, the host
CPU is the reference implementation — it cannot be wrong about what an
instruction does. Any disagreement is a lifter bug, found with no game at all.

> **Gate:** tests green. If they are not, stop here — everything downstream will
> lie to you.

---

## 2. Pick a target

Some games are far easier than others, and the two things that matter most —
which XDK built the disc, and whether it used LTCG — **are not on any wiki**.
The XDK build is a property of that particular disc build, recorded in the
XBE's library-version table. The only way to know is to read it.

Point the survey at your ISOs. It opens each image and reads `default.xbe`
without unpacking anything:

```bash
python3 scripts/survey_xbe_library.py /path/to/your/isos
```

You get a ranked table — lower score is a better first target — and a summary
of which XDK builds you actually own:

```
 SCORE  TITLE                     XDK D3D       RW        .text  IMP  CONCERNS
  -2.0  Small RW Racer           5849 D3D8      3.7.0.0    256K    5
   0.5  Tiny Puzzler             5849 D3D8      -          128K    5
  17.0  Big Online Shooter       5233 D3D8LTCG  -         1536K    5  XONLINE; WMADEC

XDK builds present:
  5233   1 title
  5849   2 titles
```

To list only the discs on one build:

```bash
python3 scripts/survey_xbe_library.py /path/to/your/isos --xdk 5849
```

**What to look for:** a small `.text`, a documented engine like RenderWare, and
an empty `CONCERNS` column. Anything listed there is a subsystem the toolkit
does not implement yet, and it will stop you before you see a frame.

**What to ignore:** `D3D8LTCG`. It makes signature matching harder but is not
disqualifying — the proven target is itself an LTCG build.

The ranking reads the header only. It cannot see hand-rolled push buffers or
threading complexity, so treat it as a shortlist, not a verdict.

> **Strongly recommended for your first title:** pick one built with the same
> XDK as a title already known to work — 5849 is the one this toolkit has been
> proven against. Anything that breaks is then, by definition, something the
> project wrongly treated as universal, which makes it a bug worth fixing
> rather than a mystery. Once a second title on that XDK runs, a third on a
> *different* build tells you what is XDK-specific.
>
> If nothing in your library is on 5849, that is fine — just expect the first
> failures to be a mix of generality bugs and XDK differences, and use the
> build summary to pick whichever build you own the most of.

> **Gate:** you know your target's XDK build and whether it is LTCG.

---

## 3. Extract the disc

The toolkit ships its own extractor — no external tool needed:

```bash
python3 -m tools.xiso ls     "Your Game.iso"          # see what's on the disc
python3 -m tools.xiso unpack "Your Game.iso" -o game_files/
```

You want `game_files/default.xbe` plus the game's data files beside it.
[extract-xiso](https://github.com/XboxDev/extract-xiso) also works if you
already use it.

---

## 4. Run the pipeline

One command:

```bash
python3 scripts/recompile.py game_files/default.xbe
```

That runs four stages in order, each feeding the next:

| # | Stage | What it does |
|---|---|---|
| 1 | `parse` | Reads the XBE header, sections and imports |
| 2 | `disasm` | Disassembles `.text`, finds functions and cross-references |
| 3 | `identify` | Classifies each function: CRT, RenderWare, or game |
| 4 | `lift` | Translates the game's x86 into C |

It defaults to **game code only**, which is what you want. Lifting the CRT and
XDK code you intend to replace with native implementations costs compile time
and debugging attention for code you will never run. Use `--all` only when you
have a specific reason.

Generated sources land in `src/game/recomp/gen/`.

**If a stage fails**, fix the cause and resume where you left off rather than
starting over:

```bash
python3 scripts/recompile.py game_files/default.xbe --from identify
```

---

## 5. Cost the kernel work before you commit

Before building, get a per-title read on how much kernel work is ahead:

```bash
python3 -m tools.kernel_audit.coverage game_files/default_analysis.json --list
```

It splits what is missing into three very different amounts of work:

- **an implementation exists, it needs a bridge wrapper** — not as cheap as it looks
- **it is a data export** — needs a value, not a function
- **nothing exists yet** — real work

This is your early "is this the right target?" signal, while switching is still
cheap. An ordinal with an `xbox_*` implementation but no bridge silently returns
0 to the game, which is worse than failing loudly.

---

## 6. Set up your project

Copy the template and fill in three values:

```bash
cp -r templates/new-game ../my-game
```

Edit `../my-game/src/main.c`:

```c
#define YOUR_GAME_ENTRY_POINT   0x00011000              /* from step 4 */
#define YOUR_GAME_XBE_PATH      "game/default.xbe"
#define YOUR_GAME_DIR           "game"
```

The entry point was printed by stage 1. To see it again:

```bash
python3 -m tools.xbe_parser game_files/default.xbe | grep "Entry Point"
```

---

## 7. Build

```bash
cd ../my-game
cmake -B build -DXBOXRECOMP_DIR=../xboxrecomp
cmake --build build
```

> **Gate:** it links and runs, even if it does nothing at all.

---

# Part II — Getting it to run

This is where nearly all of the time goes. One loop, repeated:

> run → read stderr → classify → fix → **regenerate** → run

```bash
./build/my_game 2> run.log
```

## The most common first result

The program starts, makes two kernel calls, and exits cleanly. That reads like
success and is **zero progress**.

The usual cause is that the game's thread start routine was never discovered.
The address handed to `PsCreateSystemThreadEx` is only ever *pushed as an
argument* — nothing calls it — so static analysis never sees a function there,
it gets no dispatch entry, and the game thread silently never starts.

Feed the run back into discovery:

```bash
python3 -m tools.seed_from_log run.log game_files/default.xbe \
    --functions tools/disasm/output/functions.json \
    --seeds     config/seed_functions.json

python3 -m tools.disasm game_files/default.xbe --text-only \
    --seed-functions config/seed_functions.json
python3 scripts/recompile.py game_files/default.xbe --from identify
```

Add `--dry-run` first to see what it would seed.

**Do this every iteration.** Each run discovers indirect-call targets that
static analysis cannot see, and each regeneration turns them into real
functions. This loop is the main engine of early progress — not hand-written
overrides.

**Seeding is not free.** An address the game mentioned is not necessarily a
function; a garbage slot points at data just as easily, and seeding data splits
real functions and breaks the build far worse than the missing target did. The
tool gates every candidate on being in an executable section *and* decoding as a
function body, and it still warns: on one title, seeding two bad entries took a
boot from 34 assets loaded down to 1. **If your frontier moves backwards after a
seed pass, suspect the seeds first.**

## Fix things in this order

**Get to a black screen with no crashes before you care about rendering.** A
render bug you chase before init is correct is usually a *symptom* of the init
bug, and you will fix it twice.

| Frontier | What you are fixing |
|---|---|
| Exits immediately | Thread start routine not discovered — seed and regenerate |
| CRT startup | Heap init, TLS, entry point and ESP setup |
| Hardware init | Kernel ordinals, MMIO hooks, EEPROM/SMC values |
| Asset loading | Path translation, file I/O, disc layout |
| First frame | D3D8 device creation, texture formats, vertex declarations |
| Menus | Input, fonts, more texture formats |
| Gameplay | Everything else, plus timing and audio |

## What the errors mean

| stderr shows | What's wrong | What to do |
|---|---|---|
| `[ICALL] unknown target 0x...` | A call target was never detected as a function | Seed from the log and regenerate |
| Access violation at `0xFD......` | GPU register access | NV2A hooks not initialised |
| Access violation at `0xFE......` | Audio register access | APU hooks not initialised |
| `SKIP-READ` | Unmapped memory access | Usually a native pointer where a guest address was expected |
| Hangs forever | Waiting on hardware state that never changes | Stub the wait, or fake the state |
| Stack overflow | Wrong ESP at entry, or runaway recursion | Check your entry point value |
| **Wrong physics or RNG, but no crash** | **x87 floating-point divergence** | **Suspect this first when behaviour differs from xemu without any error** |

## Asking the run a direct question

Four switches, each answering one question. All are read from the environment,
so no rebuild is needed to turn one on.

| Question | Switch | What you get |
|---|---|---|
| Where do the calls go? | `RECOMP_TRACE_PROFILE=50` | Top 40 functions by entry count, every 50 calls |
| Did *this* function ever run? | `RECOMP_PROFILE_DUMP=run.prof` | Every function entered, with hit counts |
| Did A run before B? | same file, `first_call` column | The call ordinal each function was first entered at |
| Who changed this word? | `RECOMP_WATCH_VA=0x5A8868` | Every change to that guest word, naming the next function entered |
| What did it ask the kernel for? | `RECOMP_KERNEL_LOG_BUDGET=1000000` | Every kernel call, not the first 200 |

Two traps worth knowing before you trust a number:

**The stderr profile is a top-40 list.** A function missing from it has not been
shown to be absent from the run. Use `RECOMP_PROFILE_DUMP` to ask directly.

**`kernel calls: 200` is a budget, not a count.** The log stops at
`RECOMP_KERNEL_LOG_BUDGET` (default 200) and the summary line then says so.
Raise it before concluding anything about which ordinals a title uses.

`RECOMP_WATCH_VA` samples at function entry, so it names the first function
entered *after* the value changed, not the instruction that wrote it. That is
enough to narrow a 25,000-function run to one call boundary.

## When it runs forever instead of crashing

A title that never faults gives no address to start from, and comparing its
state against a reference is the slow way in: every structure may match while
the fault is a function that entered and never returned.

    python3 scripts/stall_report.py runs/runNN.prof <project>/src/recomp/gen

**frontier** is the last functions entered, by call ordinal. **dead ends** are
functions that were entered, call other functions, and none of them ran. The
latest dead end is what is holding everything up -- a wait on hardware state
reads as a loop on a memory read, an early return as a test of a value that
should not be zero.

Then `RECOMP_WATCH_VA` names whoever writes that value, and xemu says what it
should be.

## Before you trust a number

Absence has two causes: it did not happen, or the tool could not show it.
They look identical and only one is a finding.

| Looks like | Check |
|---|---|
| a function "never ran" | the stderr profile is the **top 40**; use `RECOMP_PROFILE_DUMP` |
| `kernel calls: 200` | that is the **budget**, not a count |
| a run that "changed nothing" | did the build actually succeed, and were the same switches set |
| an all-zero structure | was it sampled **before** the title built it |
| a missing capability | grep for it being present and **switched off** |

`run_and_report.py` records the profile interval, the kernel budget and the
active switches in each run's `.meta`, and refuses to present a comparison
across different ones as an effect.

## When you stop making progress

The discipline that ends the longest debugging sessions:

**If two runs stop at the same event, stop varying the run.** Do not try another
tweak. Instead: compare both logs, find the *earliest* point where they differ
from each other or from expectation, state one falsifiable mechanism, and test
the smallest change that would prove it wrong.

Keep each iteration bounded — if you cannot observe the result in about a
minute, you are not iterating, you are waiting.

## Overrides are the last resort

An unresolved `[ICALL]` is usually a **discovery** gap. Seeding fixes it for
every title; an override fixes it for exactly one and does not transfer. Reach
for an override only after seeding has failed.

When you do, it goes in **one file**: `src/recomp_manual.c`. It is checked
before the generated dispatch table, so an override always wins.

```c
/* 0x00067890 spins on an APU DMA bit the audio HLE never sets.
 * Remove once apu_vp reports buffer completion. */
void sub_00067890(void) {
    g_eax = 0;
    esp += 4; return;   /* consume the pushed return address */
}
```

**Write down why, every time.** Nothing enforces it, which is exactly why it
gets skipped — and an override whose rationale was never recorded can never be
re-evaluated, because nobody can tell whether the bug it works around still
exists. Say what the *toolkit* gets wrong and what would let the override be
deleted, not just the symptom.

Keep them in this file and nowhere else. `tools/recomp/manual_scan.py` reads
`recomp_manual.c` to decide which functions *not* to generate; a hand-written
definition it cannot see becomes a duplicate symbol, and one it wrongly thinks
exists becomes an unresolved external.

> **Gate:** the game reaches a menu you can navigate.

---

# Part III — Getting it right

Now the dangerous class appears: things that do not crash and are simply wrong.
Physics that drifts, RNG that diverges, timers that run at the wrong rate.

These cost more time than every crash combined, because there is no error to
read. You need a reference.

## The two tools worth adding

Each answers a different question — reach for the right one.

### xemu — "what does the real thing do?"

xemu runs the same game correctly. Launch it with its GDB stub:

```bash
xemu -s -S      # -s opens GDB on :1234, -S waits for you to attach
```

Use it for anything about **live** behaviour: what a kernel call actually
returns, what a struct contains at a given moment, what value a register holds
when a function is entered.

Break at the same function in both, dump state, find the first disagreement.
**Add a state-dump hook to your build early** so this is a flag rather than a
rebuild — it is the single cheapest thing you can do now that pays off later.

Details in `docs/technical/xemu-debugging.md`.

### Ghidra — "where is this, statically?"

Ghidra answers questions about the binary at rest: function boundaries, cross
references, what calls what. It is a second opinion on `tools.func_id`, not a
view of a running game. Its debugger can attach to xemu's GDB stub, but expect
to fight it over i386 detection — not worth it as a first step.

The toolkit already ships an automated path that is usually enough:

```bash
bash tools/ghidra_naming/run_ghidra.sh
```

That runs Ghidra headless, recovers real CRT and XDK symbol names, and merges
them into `functions.json`, so generated C reads `RwMatrixMultiply` instead of
`sub_00123ABC`. See `tools/ghidra_naming/README.md`.

**If you drive Ghidra interactively (including via an MCP server), keep queries
bounded.** Ask for one specific thing — this function's bounds, this address's
xrefs — and record which binary you asked about. Open-ended exploration produces
answers you cannot reproduce later, and an address is meaningless without
knowing which build it came from.

### Which one

| Question | Tool |
|---|---|
| What does this kernel call really return? | xemu |
| What is in this struct at this moment? | xemu |
| Why does my build differ from the real game? | xemu |
| Where does this function start and end? | Ghidra |
| What calls this address? | Ghidra |
| What is this function's real name? | `ghidra_naming` |
| What does this XDK/NV2A register mean? | Public Cxbx-Reloaded or nxdk sources |

Read public sources for semantics, then implement independently.

> **Gate:** a full activity — one race, one match, one level — completes with the
> same outcome as xemu.

---

# Part IV — Playable

Define this concretely before you claim it, or the line keeps moving. A
reasonable bar:

- [ ] Boots to the title screen unattended, every time
- [ ] Every menu is navigable and every option applies
- [ ] At least one full activity completes start to finish
- [ ] Saves persist, and reload correctly after a restart
- [ ] Audio plays: music, effects, and any voice
- [ ] Controller input is complete, including analog and rumble
- [ ] Frame pacing is stable enough to play, not just to observe
- [ ] It survives a long session without leaking or drifting
- [ ] **No override in `recomp_manual.c` is a lie about game logic**

That last one matters most. Stubbing a wait to get past it is fine as
scaffolding and fatal as a finish line — it means the game is not doing what the
game does. Audit the override list before declaring playable.

---

# Part V — Enhancements

Widescreen, high resolution, higher frame rates, texture replacement, modding —
all of it is easier and safer once behaviour is correct, because you have
something to regress against. Built on an unstable base, an enhancement bug is
indistinguishable from a port bug and you will chase both at once.

The one thing worth doing early is the state-dump hook from Part III. It costs
little and it is what makes every later "did I break something?" answerable.

---

## What to send upstream

Anything you fix in the lifter, the kernel layer, or the D3D layer helps every
title and belongs upstream, not in a fork. Per-title findings — addresses,
overrides, layouts — stay local.

The split is simple: **if a fix mentions a specific game, it is yours; if it
mentions an instruction, an ordinal, or an API, it is everyone's.**

## Where things live

| Path | What |
|---|---|
| `scripts/` | Survey, pipeline driver |
| `tools/` | Pipeline stages, kernel audit, conformance, seeding, Ghidra naming |
| `src/` | Runtime: kernel, D3D8, audio, input HLE |
| `templates/new-game/` | Starting point for a title |
| `docs/pipeline/` | Detailed stage-by-stage reference |
| `docs/technical/` | Memory layout, register model, xemu debugging |

## Further reading

- `docs/GETTING_STARTED.md` — a longer tour of the same ground
- `docs/technical/lessons-learned.md` — mistakes worth not repeating
- `docs/technical/xemu-debugging.md` — using xemu as a reference oracle
- `docs/technical/downstream-lessons.md` — what a downstream port teaches
- `docs/DECOMP.md` — replacing generated functions with real source

## Legal

This toolkit ships engine and tooling code, not game content. Recompiling
requires files from a copy you own. Do not distribute recompiled binaries
containing game code or assets, and do not redistribute XDK library code
recovered during signature work.
