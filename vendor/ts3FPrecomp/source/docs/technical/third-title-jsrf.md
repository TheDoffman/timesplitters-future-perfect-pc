# Third title: Jet Set Radio Future, XDK 4134

Started 20 September 2026. **Not running yet**: it boots, creates its D3D
device, and then hangs in its own allocator. Two things that were wrong were
wrong in the toolkit rather than in this title, which is the point of a third
XDK.

---

## What it is

| | |
| --- | --- |
| Title ID | `0x5345000A` |
| XDK build | **4134** — older than TimeSplitters 2's 4721 and Burnout's 5849 |
| Direct3D | D3D8, not LTCG |
| Engine | not RenderWare |
| `.text` | 1518.8 KB |
| Kernel imports | 120 |
| Libraries | D3D8, DSOUND, LIBCMT, LIBCPMT, XAPILIB, XBOXKRNL, XGRAPHC |

`tools.xdk_symbols` finds 363 symbols, 131 of them D3D8 functions, so the OOVPA
database covers 4134 well. The pipeline lifts 9136 functions with none failed
and the project builds clean first time.

```bash
py -3 -m tools.xdk_symbols "games/Jet Set Radio Future/default.xbe"
py -3 scripts/recompile.py "games/Jet Set Radio Future/default.xbe" \
    --work-dir games/_pipeline/jsrf/out --project titles/jsrf \
    --seeds config/seeds/5345000A.json
```

---

## What it proved was wrongly universal

### 1. The GPU time fence matcher read one register layout

`mirror_gpu_time_fence()` in `src/hle/hle_d3d8.c` took the two device offsets
from `D3D_BlockOnTime`'s prologue, matching literal bytes:

```
56                push esi
8B 35 <g_pDevice> mov  esi, [D3D_g_pDevice]
8B 46 <A>         mov  eax, [esi + A]
```

4134 keeps the `time` argument in esi and puts the device in **edi**:

```
0x00191441  8B 74 24 08     mov esi, [esp+8]        ; the argument
0x00191446  8B 3D E0DC1900  mov edi, [0x19DCE0]     ; the device
0x0019144C  8B 47 34        mov eax, [edi + 0x34]
0x00191451  8B 47 30        mov eax, [edi + 0x30]
```

so every `8B 46` is `8B 47` and the match failed, leaving the title waiting on
a fence nothing advances — the exact hang that code exists to prevent.

It now scans the prologue for the load of `D3D_g_pDevice` wherever it sits,
takes the destination register from the ModRM byte, and reads the offsets
against that register. Matching on the address also rejects a prologue that
reads some other global, which used to need a separate check.

**The offsets themselves were the same as 4721** (`+0x30` submitted, `+0x34`
pointer to completed). Only the register differed — which is why a table keyed
by XDK version would have been the wrong shape, and reading the prologue is
right.

### 2. A function static analysis never found

`0x00154DAA`, called from `0x00154E1F`, was skipped as `[STUB]`. The chain from
there was worth recording because none of it pointed at the real cause:

1. the skipped call left a failure code (`0x80004005`, E_FAIL) on the stack;
2. its caller raised a C++ exception (`0xE06D7363`);
3. this runtime cannot unwind, so the throw returned and the unwind ran anyway;
4. the heap came out of it with a free-list node linked to itself;
5. the allocator hung walking that list.

Seeded in `config/seeds/5345000A.json` with that chain as its note. The
`[STUB]`, the exception and the E_FAIL are all gone.

---

## Where it stops

The main thread spins at 96% in `sub_001497DC`, the title's allocator, walking
free-list bucket 0. `RECOMP_SAMPLE=500` names it; the loop is a list walk that
ends when it reaches the bucket head:

```c
loc_00149B28:  if (ecx == eax) goto done;        /* eax back at the head */
               if (LO16(edx) <= MEM16(eax - 8)) goto done;  /* big enough */
               eax = MEM32(eax); goto loc_00149B28;
```

The list contains one node, `0x0105EE68`, whose `next` is itself, so neither
exit is ever taken.

**It is a double free, and the node is not corrupt.** Watching the address
through `RECOMP_WATCH_WRITE` gives the sequence: zeroed, then linked correctly
(`-> 0x00F81180`, the bucket head), written correctly twice more, then
overwritten with its own address. The insert that does it is a sorted insert
walking bucket 0 (`0x0014A074`–`0x0014A0A0`), and the walk finds the node
already in the list before finding its place, so `node->next = <walk position>`
writes the node into itself.

The guest call chain at that moment, caught by faulting deliberately when the
walk position equals the node being inserted:

```
sub_0015FCC0 -> sub_0015FC40 -> sub_0015FD40 -> sub_00160EA0
             -> sub_00160C30 -> sub_0017C965 -> sub_0014A821 -> free internals
```

### What has been ruled out

- **Lifter miscompilation.** No `movsx: unhandled` markers and no bare narrow
  reads in 1,047,085 lines of generated code.
- **The SEH frame.** `sub_0017D1F8` is `__SEH_prolog` and ends `lea ebp,
  [esp+0x10]`, which is exactly what `detect_seh_helpers` matches, so the frame
  the allocator's locals hang off is right.
- **`SET_LO16` on a pointer.** `edi = 0x0105021A` at the bad write looks like a
  size stuffed into a pointer's low half, but `66 8B 7D DC` really is
  `mov di, word [ebp-0x24]` and only `di` is read.
- **Uncommitted memory.** The node sits past the first commits, but with a
  large `--kernel-log` the title is seen committing up to `0x01061000`, which
  covers it. The earlier doubt came from the 200-call log budget truncating.
- **Exceptions.** Zero `[EXCEPTION]`, `[STUB]`, `[THROW]` and `[INT3]` since
  the seed. `_except_handler3` keeps appearing in `callers:` lines, but those
  are a heuristic stack scan picking up stale frames.

### Two traps worth knowing before picking this up

- **`sub_00149F5E+0x946` is a misattributed symbol.** Two rounds of
  instrumentation went into that function's insert sites and neither fired,
  because the linker folds identical functions. `docs/technical/memory-watchpoints.md`
  says to trust the `callers:` line over the symbol; it is right.
- **The registers in the watchpoint dump are the answer.** `ecx == edx ==
  0x0105EE68` with `eax = node - 8` and `esi = 0x00F81180` identified the store
  and the code path in one go, after the symbol had sent two attempts the wrong
  way.

### Found: a LOCK prefix cost the refcount its branch

Instrumenting `free`'s entry gave both call sites, and from there the answer
was one line of generated code. The objects are reference counted through the
COM idiom:

```
lock xadd [this+8], edx   ; edx = -1, so refcount--
jne  still_referenced     ; sum non-zero: somebody else still holds it
push 1
call [vtable+0x48]        ; deleting destructor, flags=1: destruct AND delete
```

which lifted to `if (_flags) goto still_referenced;` — and `_flags` is declared
`int _flags = 0` in every generated function and assigned nowhere. The branch
could never be taken, so **every `Release()` destroyed the object however many
references remained**, and the next `Release()` freed it a second time.

The cause was `"lock xadd"` sitting in the lifter's `_FLAGS_UNDEFINED`,
described as "complex flag behavior". LOCK changes atomicity, not arithmetic:
a locked instruction leaves exactly the flags its unlocked form leaves. The
prefix is now stripped where flags are tracked. `"lock cmpxchg"` had the
quieter half of the same bug — it matched no list at all, so the *previous*
instruction's flags were left standing as if they were its own.

**It was never JSRF-specific.** Counting locked atomics across the titles
lifted here: 87 in JSRF, 56 in Panzer Dragoon Orta, 50 in Marvel vs Capcom 2,
5 each in Black and Burnout 2, and **none at all in TimeSplitters 2 or Outrun
2** — which is why TimeSplitters 2 was unaffected and could never have
revealed this.

### Where it stops now

Past the hang, and a long way past it. The title runs through XAPI start-up,
loads its input bindings, opens a pad, starts the APU, finds both DSP
doorbells and has DirectSound playing four buffers at 48 kHz. It then makes
two indirect calls through pointers that are not code —

```
[ICALL] target 0x01054A70 is not code (call #1297)
[ICALL] target 0x00000000 is not code (call #1298)
```

— and writes a launch data page naming its own title ID with an empty path,
which `HalReturnToFirmware` routine 2 turns into an exit. The header layout is
confirmed against Cxbx-Reloaded's `LAUNCH_DATA_HEADER`
(`dwLaunchDataType`, `dwTitleId`, `szLaunchPath[520]`), so the fields are being
read correctly; `type=1` is not one of its four documented constants.

Those two wild pointers are the next thing. The first is a heap address, which
means a vtable slot or callback holding data rather than a function — the same
shape as an object used after it was destroyed, so it is worth checking
whether any premature destruction survives the fix before looking further.

---

## KTHREAD.TlsData pointed at a scratch buffer (20 Sep 2026)

Chasing the null indirect call turned up a real bug, though not yet the one
that stops this title.

`fs:[0x28]` is `KPCR.PrcbData`, whose first field is the current thread, and
`KTHREAD + 0x28` is `TlsData` — both confirmed against Cxbx-Reloaded's
`types.h`. `xbox_memory_layout.c` set `TlsData` to `FAKE_RWDATA_VA`
(0x00700000), a separate buffer that is only ever zeroed, while the image's
TLS block — built from the XBE TLS directory — sat at 0x00770000 and was
reachable only through `fs:[4]`. On hardware those are the same memory.

So a title that keeps per-thread state in TLS read zeros. `TlsData` now points
at the image block when the image has a TLS directory; without one the old
buffer still stands. TimeSplitters 2 is unaffected (2069 swaps, 1.66M draws
after the change).

It moved JSRF's wild pointer from 0x00700010 to 0x00770010 and no further: the
object at `TlsData + 0x10` still has a null vtable, so the title still
relaunches itself rather than starting.

**What Cxbx says the layout should be**, from `KiInitializeContextThread`:

```cpp
TlsDataSize = ALIGN_UP(TlsDataSize, ulong_xt);
StackAddress -= TlsDataSize;          // carved off the top of the thread stack
if (TlsDataSize) {
    Thread->TlsData = StackAddress;   // the base of the block
    // Title will process which section of TlsData will be fill with data
    // and zero'd. So, we leave this untouched.
}
```

Three differences from what this runtime does, each worth checking before
chasing the null vtable further:

- The block belongs at the **top of the thread's own stack**, not at a fixed
  address shared by every thread. Per-thread TLS is the point of TLS.
- Its size is `ALIGN_UP(TlsDataSize, 4)` — **12 bytes** for JSRF, whose TLS
  directory has no initialised data and `SizeOfZeroFill = 12`. This runtime
  builds 20: the zero-fill rounded up to 16, plus a 4-byte tail so `fs:[4]`
  lands past the data. JSRF reads `TlsData + 0x10`, which is inside our 20 and
  outside Cxbx's 12 — so either the size is wrong here, or the read is not a
  TLS access and the address is a coincidence of both layouts.
- Cxbx **does not initialise the block**; the title fills it. This runtime
  zero-fills it and writes slot 0.

**`RECOMP_ICALL_FATAL=1`** was added for this: it faults on a skipped indirect
call so the crash handler prints the guest stack. The `from 0x...` in the
ordinary `[ICALL]` line is read off the top of the guest stack and reads 0
exactly when a null target most needs explaining.

---

## The null call, traced to the instruction (20 Sep 2026)

`--trace-all-entries` plus a corrected call-site report named it in one run,
after four inferences in a row had failed. The instruction is `0x0006FA4F`:

```
0x0006F9F6  push 0x8840        ; 34,880 bytes
0x0006F9FB  call 0x4a8f0       ; operator new -> jmp malloc (0x17c953)
0x0006FA07  test eax, eax
0x0006FA11  je  skip           ; non-null, so construction runs
0x0006FA19  call 0x12210       ; constructor(this = eax)
0x0006FA2C  mov [0x22fce0], eax
...
0x0006FA41  mov ecx, [0x22fce0]
0x0006FA47  test ecx, ecx
0x0006FA49  je  skip           ; non-null
0x0006FA4B  mov eax, [ecx]     ; its vtable -> 0
0x0006FA4F  call [eax]         ; the null call
```

### What is measured

- `[0x0022FCE0]` is written exactly once, `0 -> 0x00770010`, on the path from
  the CRT into main (`RECOMP_WATCH_WRITE`).
- That value is what `operator new(0x8840)` returned, so the title's own
  `malloc` produced it.
- **No kernel allocation ever returns an address in `0x0077xxxx`.** Every
  `NtAllocateVirtualMemory` and every `[HEAP]` line in the run is at
  `0x00F80000` or above; the title's CRT heap is the 1 MB reserve at
  `0x00F81000`.
- Nothing ever writes `[0x00770010]`, the vtable slot, so the object was never
  constructed there despite the constructor being called.
- `0x00770010` is `TlsData + 0x10`, and the CRT's own thread start-up leaves
  `edi` at exactly that address.

### The CRT builds the TLS block itself

`sub_00147EBB`, the CRT thread start-up, does:

```
mov eax, fs:[0x28]      ; current thread
mov edx, [eax + 0x28]   ; TlsData
add edx, 4
mov [edx - 4], edx      ; *(TlsData) = TlsData + 4
mov esi, [0x1e0c2c]     ; image TLS data start
rep movsd               ; copy the init data to TlsData + 4
lea edi, [ebx + edx]
rep stosd               ; zero-fill the rest
```

So `TlsData` must point at writable memory of at least `4 + init + zero_fill`
— 16 bytes here — and the CRT, not the kernel, fills it. That matches
Cxbx-Reloaded's `KiInitializeContextThread`, which carves the block off the
thread's stack and leaves the contents to the title.

### What is not established

**Why `malloc` returns a pointer into the TLS region rather than the heap.**
That is the open question and everything else is downstream of it.

The next step is to instrument `malloc` (`sub_0017C953`) at entry and exit to
log the requested size against the returned pointer, and to dump the heap
state on the call that returns `0x00770010`. That distinguishes a corrupted
free list from a heap whose arena was never set to the region the kernel
actually gave it.

### Hypotheses measured and rejected

Recorded because each was plausible, and because the pattern -- reasoning from
a register value rather than from the call site -- is what cost the time.

- **"The TLS block is the wrong size."** Cxbx sizes it 12 bytes for JSRF
  against the 20 built here, but the CRT only needs 16 and writes its own
  contents, so 20 is sufficient.
- **"The TLS index is wrong."** The title stores `-5` at `0x00264850` from
  `xbe_entry_point+0x1A4`, exactly what this runtime assumes.
- **"The title subtracts TlsData from the TLS slot, so the slot must be
  in-block."** That code is at `loc_00147FD4`, reached only from
  `loc_00147FD0` with a non-null register, and the path through it sets that
  register to zero first. It does not run.
- **"`0x00770010` is an object the title placed in TLS."** It is the address
  the CRT's zero-fill loop ends on, left in `edi`, and separately what malloc
  returned. Two unrelated things at one address.
- **"`KTHREAD.TlsData` pointed at a scratch buffer."** True, and fixed -- see
  above -- but not the cause. The faulting address moved with the fix, which
  looked like confirmation and was not.

### The null call is object cleanup, and the numbers do not yet reconcile

Instrumenting the allocator (`sub_001497DC`, entry and exit) settled two
things and opened a third.

**`malloc` is fine.** The allocation in question is `operator new(0x8840)` =
34,880 bytes, and it returns `0x01054A70` — inside the heap
(`0x00F81000`–`0x01081000`). Every allocation in the run is in the arena; none
is below `0x00F80000`. The earlier claim that malloc returned a pointer into
the TLS region was wrong.

**The constructor writes the vtable.** `RECOMP_WATCH_WRITE` on `0x01054A70`:

```
write to 0x01054A70 from sub_00012210+0x162
  0x00000000 -> 0x001C4458
```

and `0x001C4458` in the image holds `0x00012BF0`, which disassembles to a
scalar deleting destructor (`mov esi,ecx; call dtor; test [esp+8],1; je;
push esi; call operator delete`). The call site is `push 1; call [eax]`.

**So `0x0006FA4F` is ordinary cleanup**, not an error path — the function
allocates an object, uses it, and destroys it through vtable slot 0. The
string after the vtable, `Z:\Media\Cache\JSRF_FATAL.ERR`, is data belonging to
that object and not evidence that an error occurred.

### The open contradiction

Two measurements from the same build disagree and have not been reconciled:

- the heap probe says the 34,880-byte allocation returned `0x01054A70`;
- `RECOMP_WATCH_WRITE` on the global `0x0022FCE0` — which the code sets from
  that allocation's result — says it is written once, `0 -> 0x00770010`.

`0x00770010` is never returned by any allocation in the run; the probe flags
anything below `0x00F80000` and printed nothing. So either the global is
written from a path other than `mov [0x22fce0], eax` at `0x0006FA2C`, or two
different objects are involved and the destructor runs on the wrong one.

**Resolve that before anything else.** Watch `0x0022FCE0` and the allocator in
the same run, and print the guest return address on each write to the global.
Every theory built on top of one of these two numbers without the other has
been wrong so far.

### Method note

Six hypotheses were measured and rejected in this session: TLS block size, TLS
index, a subtraction behind a branch that never runs, "the address is an object
in TLS", `KTHREAD.TlsData` (a real bug, fixed, but not this one), and "malloc
returns a pointer outside the heap". Each came from reasoning about a register
value rather than from measuring the thing itself. The two measurements that
actually moved this forward were a trace build naming the call site and a
watchpoint naming the writer.
