# C++ exceptions: the throw is not implemented, and it used to be silent

Found 20 September 2026, at the end of the chain that started with Outrun 2's
null `std::map`. It is the reason that title still does not render, and it
will stop any title that throws.

## What happens

MSVC compiles `throw x` into a call to `_CxxThrowException(&object, &throwinfo)`,
which never returns. Because it never returns, the compiler puts an `int3`
after it as a guard.

This runtime has no C++ exception support. The throw helper is lifted like any
other function, the call inside it does nothing useful, and control falls
through to the `int3`. The lifter used to emit that as a comment:

```c
    /* int3: debug-trap slide byte, stepped over */
```

So the throw returned. Execution carried on from the middle of a function that
was never meant to continue, with a stack nothing had unwound and callee-saved
registers nobody restored. Everything after that is wreckage, and none of it
mentions an exception.

## In Outrun 2

`sub_001C425B` ends its 250 lines with a tail jump into `sub_001BB79F`, which
is the throw:

```c
    PUSH32(esp, ecx);
    PUSH32(esp, 0x285FC0);      /* the _ThrowInfo */
    eax = ebp + -1;
    PUSH32(esp, eax);           /* the exception object, one byte */
    MEM8(ebp + -1) = 0x21;
    call sub_001838A6           /* _CxxThrowException(obj, info) */
    /* int3 */
```

The descriptor at 0x285FC0 decodes cleanly as a `_ThrowInfo`: attributes 0, no
unwind function, and a catchable-type array at 0x285FB8 holding one entry
whose `type_info` name is `.D`. In MSVC's mangling `.D` is **`char`**. The
object is one byte and it is set to 0x21.

So Outrun 2 throws `(char)0x21` during start-up. A one-byte sentinel like that
is a control-flow throw, not a crash: the title expects to catch it somewhere
and carry on. We neither throw nor catch it, so it does neither.

Everything downstream is explained by this. The heap walk that faults reading
0xFF5007E2 is `sub_000CD040` reading its argument from a stack slot that holds
a return address, because the stack was left where the throw abandoned it.
0xFF5007E2 is not a pointer at all; it is four bytes of instruction encoding,
which is why it appears 34 times in the image.

## What changed

The trap is still stepped over. A real `__debugbreak()` there kills the process
with `STATUS_BREAKPOINT`, exit code 3 and no message, which is how Wreckless
died after a full boot. But it now says so, once per address:

```
[INT3] lifted code reached a debug trap at 0x001BB7B5 and stepped over it.
       MSVC emits one after a call it thinks cannot return, so this is
       usually a C++ throw that this runtime did not unwind. Execution
       continues on a stack nothing cleaned up, so treat anything odd
       after this line as a consequence, not a new bug.
```

The `int3` after a kernel `int 0x2d` debug print is the kernel's slide byte and
is filtered out at run time, so it stays quiet. Padding between functions is
never executed and never reported. What is left is traps the title genuinely
reached.

## What the runtime does now

The throw is identified and reported at the point it happens, before the
stack is wrecked.

`detect_cxx_throw` in `tools/recomp/lifter.py` finds `_CxxThrowException` by
the static `EHExceptionRecord` template it copies onto its own stack, then
locating the small function that references that template's address. Matching
the magic number alone would not work: `0x19930520` occurs 146 times in Outrun
2, once in every function's exception state table, while the record's
exception code occurs eight times. The translator emits one call at that
function's entry, where the two `__stdcall` arguments are still on the stack.

```
[THROW] the title threw a C++ exception from 0x001BB7B5, type ".D"
        object at 0x00F7F0A7, first dword 0xF7F12C21
        This runtime cannot unwind, so the throw will RETURN and
        execution continues on a stack nothing cleaned up. Treat
        anything odd after this line as a consequence, not a new
        bug. RECOMP_THROW_FATAL=1 stops here instead.
```

`.D` is MSVC's mangling for `char` and the object's first byte is 0x21, which
is what the static decode above predicted. Reported once per throw site, since
a throw in a loop is one bug and the first one is the only one that happened
on an intact stack.

`RECOMP_THROW_FATAL=1` exits at the throw instead of carrying on. On Outrun 2
that turns a thirty-second run ending in a confusing access violation into a
clean stop with a 29 KB log and no crash at all. Use it as soon as a `[THROW]`
line appears: everything after it is untrustworthy.

The default is to continue, because that is what the runtime did before and a
title that throws somewhere harmless should not be stopped by a diagnostic.

## The hook for real support already exists

The throw reaches the kernel bridge. The same run prints:

```
  [KERNEL] RtlRaiseException: record=0x00F7F004 code=0xE06D7363 (#1)
```

`bridge_RtlRaiseException` in `src/kernel/kernel_bridge.c` receives the fully
built `EHExceptionRecord`, with the C++ exception code, the magic number, and
the object and `_ThrowInfo` pointers as its parameters. That is where an
implementation belongs: everything a real unwinder needs is already handed to
it, and nothing else has to be intercepted.

## What it would take to support throws

The first step is done. The rest are not started:

1. ~~**Identify `_CxxThrowException`.**~~ Done, 20 Sep 2026, as above.
2. **Walk the exception registration chain.** `fs:[0]` already holds it: the
   SEH prologue links a record on every function with a try block, and this
   runtime already models that well enough for the frame pointers to be right.
3. **Match the thrown type against each frame's catch table**, using the
   `_ThrowInfo` and the funclet tables MSVC emits.
4. **Transfer control to the catch block.** This is the hard part. Each lifted
   function is a real C function, so resuming inside one after abandoning the
   frames between means moving the native stack as well as the guest one. The
   lifter already has this problem for `longjmp` and solves it there; that code
   is the place to start.

Step 1 alone is worth doing before any of the rest, because it turns "the title
threw and we carried on" into "the title threw and we stopped", which is a
diagnosable failure instead of silent corruption.

## Why Outrun 2 throws: as far as it is narrowed

The throw is the last thing `sub_001C425B` does, and it is guarded:

```c
loc_001C47D7:  cmp  MEM32(ebp - 56), ebx      /* ebx is 0 */
               jne  loc_001C442B              /* -> no throw */
loc_001C47E0:  cmp  MEM32(ebp - 88), ebx
               je   loc_001C486A              /* -> no throw */
loc_001C47E9:  tail jmp the throw helper
```

So it throws when the local at `ebp - 88` is **non-zero**. The registers at
the throw confirm `ebx = 0`, so that reading is exact and not a guess about
what `ebx` held.

The frame is locatable from the report. With `esp = 0x00F7F098` at the throw
and the two vtable words `0x257398` sixteen bytes apart in the stack window,
`sub_001C425B`'s `ebp` is `0x00F7F12C`, so the slot is `0x00F7F0D4`. It holds
**8**, and the word below it at `ebp - 92` holds **0x8288E7C0**, a pointer
into the contiguous window that `edx` is also carrying at the throw.

Both words are set to zero at function entry and **nothing in the function's
thousand lines writes either of them again**. So a callee fills them through
a pointer it was given, and the pair reads like {buffer, count} or
{data, size}: the function throws when the count is not zero at the end.

That is where the next session should start. The watchpoint cannot help --
see the note in `docs/technical/memory-watchpoints.md`, a stack page is far
too hot to trap on -- so the route is to read the six calls that take
`ecx = ebp - 84` and find which one is handed the pair.

## The general lesson

A throw that does nothing is worse than a throw that crashes. Three separate
investigations in this title — a null map pointer, garbage assembled from
overlapping stores, and a corrupt heap walk — were all the same missing
unwind, seen from three different distances. Wherever the runtime cannot do
something the title asked for, it should say so at the point of the request,
not let the consequences surface somewhere unrelated.
