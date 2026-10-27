# Fifth title: Panzer Dragoon Orta, XDK 4928 — cold-start note

Started 19 September 2026 as a reconnaissance run alongside Outrun 2. It got
as far as **a lift that succeeds and a build that does not**. Nothing has run
yet. This is the note to pick it up from.

**Update, 20 September 2026: it builds and it runs.** One line in the lifter
unblocked the compile. The title now gets through XAPI start-up, the file
system, D3D8 device creation, the shadow renderer, DirectSound and input
binding, and then hangs before its first `Swap` in a COM-style enumeration
loop that walks off the end of a six-entry vtable. The detail is in "What
happened on 20 September" at the bottom; the sections between are the
reconnaissance as it stood, and are still accurate except where that section
says otherwise.

## What it is

| | |
| --- | --- |
| Title | Panzer Dragoon: ORTA |
| Title ID | 0x53450007 |
| XDK build | **4928** — a fifth distinct version (5849, 5344, 4721, 5028 are the others) |
| Graphics | D3D8, not LTCG |
| Engine | not RenderWare |
| `.text` | 2668 KB — the largest of the five |
| Image | 5156 KB |
| Kernel imports | 115 |
| Sections | 16, one demand-loaded (`$$XTIMAGE`) |
| Survey score | 15.92 (lower is better) |
| Concern flagged | a WMADEC section, and WMA decoding is not implemented |

Libraries are CalcSig, D3D8, D3DX8, DSOUND, LIBCMT, LIBCPMT, XAPILIB,
XBOXKRNL and XGRAPHC, all 1.0.4928. `LIBCPMT` is the C++ standard library, so
expect the same `std::map` shapes Outrun 2 is currently stuck in.

## How far it got

The XDK symbol step and all four pipeline stages ran clean:

```
py -3 -m tools.xdk_symbols "games/Panzer Dragoon Orta/default.xbe"
py -3 scripts/recompile.py "games/Panzer Dragoon Orta/default.xbe" \
    --work-dir games/_pipeline/pdorta/out --project titles/pdorta
```

| | |
| --- | --- |
| Functions found | 16,901 |
| Translated | 16,881 |
| Failed | **0** |
| Lines of C | 1,671,809 in 20 files |
| Unresolved call stubs | 81 |

CMake configured. The compile then failed, on two lines, in one function.

## The one thing blocking the build

```
recomp_0009.c(68378): error C2065: 'dr0': undeclared identifier
recomp_0009.c(68385): error C2065: 'dr0': undeclared identifier
```

Both are `eax = dr0;` inside `sub_0021118E`. `dr0` is an x86 debug register.
Nothing declares it, so the file will not compile, and one bad function stops
the whole title.

**It is two bugs, and they are both general.**

1. **The translator emits an undeclared identifier instead of a stub.** Every
   other instruction it cannot model degrades gracefully; this one produces C
   that does not build. Whatever it does for `iretd` and `lcall` — which this
   title also hits, and which did not stop the build — it should do for the
   debug and control registers. That is the fix that unblocks the title, and
   it is small.

2. **`sub_0021118E` is almost certainly not a function.** Read its body and
   it is obvious nonsense: `mov cs, [esi]`, `adc` against offsets like
   `0x39002110` and `-771743473`, `and [eax], eax` repeated. That is the
   disassembler walking data as if it were code. Fixing (1) makes the title
   build regardless, but this belongs on the function-discovery pile, because
   a misidentified region that happens to decode to something harmless is a
   silent version of the same problem.

## Next

1. ~~Make the translator stub unknown register reads~~ — done, 20 Sep 2026.
2. ~~Run it and classify where it stops~~ — done, 20 Sep 2026.
3. Expect the 4928 XDK to need its own signature work, as 4721 did. It is a
   fifth version, so whatever breaks is a fresh data point about what the
   project still treats as universal.
4. WMADEC is present. If audio start-up stalls somewhere no other title
   reached, that is the first place to look. (It does not: XAudio2 and the APU
   come up, and the hang is elsewhere.)

---

## What happened on 20 September

### The translator fix

The blocker was one line in `tools/recomp/lifter.py`, and the shape of the fix
was already in the file two lines above the bug.

`_fmt_reg` translates a register operand into a C expression. It special-cases
the segment registers — `cs` reads as `0 /* seg:cs */`, and `_fmt_set_reg`
turns a write into a comment — and then ends with `return name`, handing back
the Capstone spelling for anything it does not know. For `eax`, `esi` and the
rest that is right, because those *are* the names of the C locals. For `dr0` it
is not: nothing declares `dr0`, so the generated C did not compile.

That is a different failure from the one `iretd` and `lcall` have. Those reach
the `Unhandled` tail of `lift_instruction`, get recorded in
`Lifter.unimplemented`, and emit `/* TODO: iretd */` — a comment, which builds.
`mov eax, dr0` never gets that far, because `mov` has a handler and the handler
formats its operands. So the fix belongs in the formatter, not in the
instruction table, and it copies what the formatter already does for segments:

```python
_SYSTEM_REGS = frozenset(
    [f"dr{i}" for i in range(16)]
    + [f"cr{i}" for i in range(16)]
    + [f"tr{i}" for i in range(8)]
)
```

A read becomes `0 /* dr0: system register, not modelled */`, a write becomes
`/* mov dr0, <value> - system register */;`. Defined value, no-op store, and a
comment naming what it was — the same contract the rest of the file uses for
things it cannot model. The full sixteen of each are covered rather than the
`dr0`-`dr7` / `cr0`-`cr4` that exist on a Pentium III, because Capstone will
name `cr8`-`cr15` if a byte sequence decodes that way and the point is that no
register name can ever reach the output as a bare identifier again.

`tools/recomp/test_lifter_system_regs.py` pins it, in the style of the other
`test_lifter_*.py` files: a read is a defined zero, a write writes nothing, no
`drN`/`crN` appears on the left of an `=`, and an ordinary `mov eax, ecx` is
untouched. 227 existing tests still pass.

The generated line now reads:

```c
    eax = 0 /* dr0: system register, not modelled */;
```

Point 2 of "The one thing blocking the build" above still stands: `sub_0021118E`
is data, not a function. The fix makes that harmless rather than fatal, which is
the right trade, but it does not find it.

### It builds

Re-lifted and built with no further errors — that was the only error class.

```
16,887 of 16,907 functions, 0 failed, 1,670,695 lines of C in 17 files
pdorta_recomp.vcxproj -> titles\pdorta\build\Release\pdorta_recomp.exe
```

(16,907 rather than 16,901 because five runtime-observed addresses were seeded;
see below.)

### Where it stops

Run with `RECOMP_GAME_DIR` pointing at `games/Panzer Dragoon Orta`:

```
py -3 scripts/run_and_report.py titles/pdorta/build/Release/pdorta_recomp.exe \
    --seconds 40 --out-dir games/_pipeline/pdorta/runs --tag first
```

It does not crash. It gets a long way and then hangs:

- XAPI start-up, the retail disc check, 77 files opened off the partition image
- the heap, `KeConnectInterrupt` for the vblank and the APU
- `Direct3D_CreateDevice` replaced by name, `[HLE-D3D8] shadow device 640x480`
- XAudio2 at 48 kHz, the APU voice processor
- `[INPUT] bindings from ...json`, port 1 bound to XInput pad 0

and then, forever:

```
[ICALL] target 0x13541C27 is not code -- skipped 1 time(s) (null or wild function pointer, at call #3580)
[ICALL] target 0x11D08E33 is not code -- skipped 1 time(s) (null or wild function pointer, at call #3581)
[ICALL] target 0x00000000 is not code -- skipped 1 time(s) (null or wild function pointer, at call #3582)
...
[ICALL] target 0x13541C27 is not code -- skipped 1000000000 time(s) (null or wild function pointer, at call #3000003834)
```

Three billion of each in forty seconds. No frame is ever presented — there is
no `Swap` and no `[FPS]` line in the whole log. The vblank ISR keeps ticking
(`KeInsertQueueDpc` / `KeSetEvent`, ordinals 119 and 145, one pair per vblank,
counts still climbing at the end), while `RtlEnterCriticalSection` and the rest
are frozen at the value they had when the spin began. That is the "main thread
stops entering new functions, ISR keeps ticking" row of `CLAUDE.md`'s table,
but not its cause — this is not `D3D_BlockOnTime`.

#### What is spinning

`RECOMP_SAMPLE=1000` names it in one run, with no trace build:

```
  [guest main             tid 10268 ]    1099 samples,  98.7% on-CPU, 21.8s CPU time
       67.10%  sub_0003CDF0  [pdorta_recomp.exe]
       28.76%  recomp_icall_not_code_log  [pdorta_recomp.exe]
      inclusive (on the stack), hottest first:
       95.85%  sub_00231DF8 ... sub_0022D5E6 ... sub_0003CDF0
```

`sub_0022D5E6` is `XapiThreadStartup` and `sub_00231DF8` is `mainXapiStartup`,
so this is the title's own main thread.

`sub_0003CDF0` is a COM-style enumeration over an object held in the global at
guest `0x00342708`. Its inner loop is three indirect calls — vtable `+0x1C`,
then `+0x08` on the object that call was supposed to return, then vtable `+0x18`
— and it repeats while the result is non-negative.

#### Why it never leaves the loop

The object's vtable is at guest `0x00334888`, and reading `.rdata` there shows
it has exactly six entries:

```
0x00334888  0x0026A8A0   QueryInterface
0x0033488C  0x002746E0   AddRef
0x00334890  0x0026F830   Release
0x00334894  0x0029D1C0
0x00334898  0x0029D2B0
0x0033489C  0x0029D190
0x003348A0  0x13541C27   <- not a function: DirectInput8 GUID table starts here
0x003348A4  0x11D08E33
```

`0x13541C27` and `0x11D08E33` are the first two dwords of
`13541C27-8E33-11D0-9AD0-00A0C90A43CB`, and the strings `DirectInput8` sit a few
bytes earlier. So slots `+0x18` and `+0x1C` are past the end of the vtable, and
the third wild target — `0x00000000` — is `out->vtable+8` on an out-parameter
the skipped call never filled in.

The loop is infinite because of what the runtime does with a call it refuses:
`RECOMP_ICALL_SAFE` sets `eax = 0` and carries on. Zero is `S_OK`, the loop's
back-edge is `jge`, so a skipped HRESULT call reads as success and the loop
takes the back-edge every time. **A skipped indirect call is not a neutral
event in COM-shaped code — it is a guaranteed infinite loop.** That is an
engine-level observation about `templates/runtime/recomp_types.h`, not a
Panzer Dragoon Orta one; nothing has been changed there.

The object itself is genuine: `RECOMP_WATCH_WRITE=0x342708` names
`sub_002730B0+0x366` storing a real heap pointer, and watching that object's
first dword names `sub_00272C00+0x1B5` storing a real `.rdata` vtable. The
title's mini-COM (`sub_00272C00` is a `DllGetClassObject`-alike over a class
table at `0x0032DB54`, called with `IID_IClassFactory` —
`00000001-0000-0000-C000-000000000046` — at the head of that table) is working.
**The object is simply not the class `sub_0003CDF0` expects.** That is the
question to answer next.

#### The seeds did not help, but they were still worth taking

The first run reported five unresolved indirect-call targets. `tools.seed_from_log`
accepted four of them plus the `PsCreateSystemThreadEx` start routine, written
to `config/seeds/53450007.json` (the title-ID naming the seeds README requires —
the tool defaults to the name you give it and warns if it is wrong).

Re-lifting with the seeds removed four of the five unresolved targets and left
`0x00293774`, which the tool correctly refuses: it "restores esi/edi without
saving it", i.e. it is the middle of `sub_002934D0`. **The hang is unchanged** —
same three targets, same call numbers, still no `Swap`. So the missing functions
were real but were not the cause.

#### One engine limit worth writing down

Before the hang, start-up prints `D3D8 VSH: No free shader slots` 55 times.
`NV2A_VS_MAX_SLOTS` is 128 in `src/d3d/d3d8_vsh.h`, and Panzer Dragoon Orta
creates at least 183 vertex shaders during start-up — more than any title the
shadow renderer has seen. It is not why the title hangs (the messages stop well
before the spin begins), but it will matter the moment it draws. Not changed
here; `src/d3d` was owned by other work at the time.

### Next, again

1. **Find out which class the object at `0x00342708` should be.** Dump the
   class table at `0x0032DB54` properly — it is a list of `{GUID, factory}`
   pairs — and compare the CLSID at `0x003325C0` against it. A second class
   whose vtable has eight or more slots is the answer; if there is no such
   class, then `sub_0003CDF0` is being reached in a state it should not be.
2. **Then walk backwards from `sub_002730B0`**, which is what actually stored
   the pointer, rather than from `sub_00272CF0`, which only looks like the
   creator.
3. Raise `NV2A_VS_MAX_SLOTS`, or make the slot table grow, before judging any
   frame this title draws.
4. Consider whether `RECOMP_ICALL_SAFE` should return something other than
   `eax = 0` — or at least whether a skipped call inside a loop should be
   fatal rather than silent. Three billion iterations of a hot loop is a
   long time to spend saying nothing new.
