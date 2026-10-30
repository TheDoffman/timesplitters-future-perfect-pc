# Memory watchpoints: "what wrote this?"

Every hard bug in this project so far has had the same shape. A structure
holds a value it should not, the code walks into it, and the fault tells you
where the damage was *read*, not where it was *done*. TimeSplitters 2's dark
level, Marvel vs Capcom 2's stalled frame loop and Outrun 2's wild pointer
were all that.

This is the tool for that question. It is three switches that work together,
and they are all available in a plain lift — no `--trace-all-entries` build,
no profiler, no measurable cost when they are off.

## The three switches

| switch | what it does |
| --- | --- |
| `RECOMP_TRAP_NULL=1` | Makes guest page zero unreadable, so a null dereference faults where it happens instead of quietly reading zeros. **Try this first.** |
| `RECOMP_FIND_VALUE=0x...` | At a crash, scans guest RAM and prints every address holding that value. Answers "who is holding this bad pointer?" |
| `RECOMP_WATCH_WRITE=0x...` | Faults on every write to an address and names the instruction that did it. Answers "what put it there?" |

`RECOMP_TRAP_NULL` is older than the other two and lives in
`src/kernel/xbox_memory_layout.c`. The other two are
`src/kernel/xbox_watchpoint.c`.

## The loop they form

The three compose into a routine that turns "it crashed on garbage" into a
named function in about three runs, as long as the failure is deterministic.
Check that first: run twice and see whether the bad value is identical. If it
is, the title computed it the same way both times and every step below is
repeatable. If it is not, you are looking at uninitialised memory or a race,
and this is the wrong tool.

**1. Trap null.** Most wild pointers are not wild, they are null plus an
offset. With page zero mapped, a null store lands on real memory and the
title carries on until something unrelated trips over it, which is how a
one-line bug turns into a day. Trapping it moves the fault to the instruction
that made the mistake.

**2. Find who holds it.** If the fault address is genuinely strange rather
than small, scan for it:

```
RECOMP_FIND_VALUE=0xF8604020 <title>_recomp.exe
```

A single hit is the field to watch. No hits at all means the value was
computed into a register and never stored, so skip to reading the code.

**3. Watch that field.**

```
RECOMP_WATCH_WRITE=0x00000004 RECOMP_WATCH_BUDGET=20 <title>_recomp.exe
```

```
[WATCH] write to 0x00000004 from sub_001C425B+0x542
[WATCH]   host insn: 89 14 01 43 8B 04 3C 83 C0 FC 43 89
[WATCH]   guest esp=0x00F7EFAC eax=0x8239F620 ecx=0x001BF8C8 esi=0x00000000 edi=0x00000004
[WATCH]   callers: 0x001C0DA6 <- 0x001BDAF2 <- 0x001BE80F
[WATCH]   0x008239F5 -> 0x8239F620
```

The spec is `addr[:len][,addr[:len]]`, length defaulting to a dword, up to
eight of them. `RECOMP_WATCH_READS=1` traps reads as well, which is much
slower. `RECOMP_WATCH_BUDGET` caps the reports and disarms afterwards, so a
watch on a hot address cannot fill a disk; it defaults to 200.

Addresses in the contiguous window (`0x80000000`, where
`MmAllocateContiguousMemory` and most streamed data land) can be watched as
well as low RAM. They used to be refused as "outside mapped guest RAM".

### Arming late: `RECOMP_WATCH_ARM_ON`

`RECOMP_WATCH_ARM_ON=<text>` holds the watches until the title opens a file
whose guest path contains `<text>`; `<text>#N` waits for the Nth such open.
Use it when the page is busy long before the write that matters. Every
write to a watched page traps, and on a hot page that slows the title enough
to change what it does. TimeSplitters: Future Perfect's cutscene record at
`0x813A6580` shares a page with start-up work: armed from boot, the scripted
presses landed on different screens and the run never reached the cutscene.
`RECOMP_WATCH_ARM_ON=skelts3#2` armed it straight after the cutscene pack was
read, and the next run named the writer.

**Never arm a watch over memory the title is about to read a file into.** The
kernel reads with the host's `ReadFile`, which cannot write into a read-only
page, so the read fails with `STATUS_UNSUCCESSFUL` and the title reports a
damaged disc. Future Perfect showed "There is a problem with the disc you are
using" for exactly that reason. It also means a watch never sees data
arriving from a file: to find where a value came from, compare the file's
bytes against what was read (the `[READ]` line now names the destination,
`-> 0x...`), then arm after the read and watch what the title does to it.

## How it works, and the four things that follow from it

The watched page is made read-only, so a write to it raises an access
violation. The handler reports it, puts the page back, and sets the
processor's trap flag so the *next* instruction raises a single-step
exception — at which point the write has landed, the new value can be read,
and the page is protected again.

That mechanism has consequences worth knowing before you trust the output.

- **Protection is per page**, so a watch on one word traps every write to the
  4 KB around it. Those are stepped over silently, but they cost a fault
  each, and on a busy page that is slow.

- **There is a one-instruction window** where the page is writable. Another
  thread writing in that window is missed. Guest code here is cooperatively
  single-threaded, so in practice that is the vblank thread and the ISR.

- **Mirrors are separate mappings** of the same memory, so a write through a
  different alias of the same physical page does not trap. Watch the address
  the writer actually uses, not the one you read it through.

- **The symbol can lie.** The name comes from the host RIP through dbghelp,
  and the linker folds identical functions, so a small lifted function can be
  reported under a neighbour's name. The `callers:` line is guest return
  addresses recovered from the guest stack and does not have that problem —
  **trust it over the symbol.** The host instruction bytes are printed for the
  same reason: they say the operand size and the base register, which the
  symbol offset does not.

## Worked example: Outrun 2

The title faulted after its first `Swap`, dereferencing 0xF8604020. That
address is in no aperture the runtime maps, and a day went into deciding
whether it was a missing memory window.

`RECOMP_FIND_VALUE` ended that in one run: exactly one word in 64 MB held it,
at **guest address 4**. A value living at address 4 is not an aperture
question, it is a null pointer.

`RECOMP_WATCH_WRITE=0x00000004` then showed three overlapping unaligned
stores, at addresses 4, 5 and 6, each a dword, together assembling
0xF8604020 out of three unrelated values a byte apart. The bad pointer was
never written by anything. It was **debris** — the tail of one store and the
head of the next, read back as a pointer.

`RECOMP_TRAP_NULL=1` then moved the fault to where the mistake actually is:

```
[CRASH] Access violation at RIP=..., fault addr=0x10008 (read)
  Xbox VA of fault: 0x00000008
  ecx=0x00000000
  in sub_001BD1DA+0x3B
```

`sub_001BD1DA` reads `[ecx+8]` and then walks `+4`, `+0x10` and `+0x14`,
comparing keys and branching left or right. That is a red-black tree lookup,
which is to say a `std::map`, and its `this` is null. The chain is
`sub_001C0D4C` -> `sub_001C051F` -> `sub_001BD97C` -> `sub_001BD1DA`, and the
null arrives as `sub_001C0D4C`'s first stack parameter.

Three runs, from "faults on a wild pointer" to "a std::map pointer is null
and here is the parameter it came in on". The original 0xF8604020 was a red
herring in its entirety.

## It cannot watch a stack address

Tried on 20 September 2026 and it does not work. The mechanism protects a
4 KB page, and the guest stack page is touched by every call, every push and
every local write. Each one faults and single-steps, and Outrun 2 did not
reach a point 25 seconds into its start-up within 90 seconds of watching one
stack slot. No reports came out, because the collateral traps are stepped over
silently by design; it simply never got there.

So a watch is for guest data: heap, globals, image data, device pages. For a
local, the practical route is the one that worked here -- have the runtime
print the frame pointer at the moment of interest, work the offset out from
the lifted code, and read the value from a dump rather than trapping on it.
The `[THROW]` report prints `esp`, `ebp` and `seh_ebp` for exactly that.

## What it does not do

It cannot watch an address before guest memory is mapped, it cannot watch
host memory, and it does not break into a debugger. For the case where two
runs need comparing rather than one run explaining — "does our copy of this
object look like xemu's" — the tool is still `RECOMP_DUMP_VA` against the
xemu GDB stub, as `CLAUDE.md` describes under **Reference oracle**.

There is an older watch, `RECOMP_WATCH_VA` in `src/kernel/recomp_trace.c`,
which polls an address at every function entry. It needs a
`--trace-all-entries` build and can only say the value changed somewhere
between entering one function and entering the next. Prefer
`RECOMP_WATCH_WRITE`; keep the old one for watching a value across a long
run where exactness does not matter.
