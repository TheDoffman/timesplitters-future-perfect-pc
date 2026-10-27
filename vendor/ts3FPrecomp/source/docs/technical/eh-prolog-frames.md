# `__EH_prolog`: the lifter drops the frame pointer it is handed

Found 20 September 2026 while working out why Outrun 2 passed a null
`std::map` pointer. It is not an Outrun 2 bug. It is a codegen bug that will
hit **any title compiled with MSVC C++ exception handling**, which is most of
them, and it corrupts silently rather than crashing at the scene.

## The shape of it

MSVC does not inline the SEH frame setup for a function with a `try` block.
It emits a call to a shared helper, `__EH_prolog`, with the handler address in
`eax`. In Outrun 2 that helper is `sub_00185AD0`, 31 bytes:

```
push -1
push eax              ; the handler
mov  eax, fs:[0]
push eax              ; link the exception registration record
mov  eax, [esp+0xC]   ; the return address
mov  fs:[0], esp
mov  [esp+0xC], ebp   ; overwrite the return slot with the caller's old ebp
lea  ebp, [esp+0xC]   ; <-- build the CALLER's frame pointer
push eax              ; put the return address back
ret
```

The last three lines are the point. `__EH_prolog` **returns a frame pointer in
`ebp`**, and everything the calling function does with its locals is relative
to it. It also deliberately leaves twelve bytes of exception registration on
the stack, so it does not obey the ordinary calling convention and is not
meant to.

The lifter gives every frameless function a local C variable for `ebp`:

```c
void sub_00185AD0(void)
{
    uint32_t ebp = 0;
    ebp = g_ebp;
    ebp = g_seh_ebp;        /* fpo_leaf: inherit caller's frame */
    ...
    ebp = esp + 0xC;        /* <-- writes a local, then returns */
    PUSH32(esp, eax);
    esp += 4; return;
}
```

So the frame pointer `__EH_prolog` computed is assigned to a local and thrown
away. The caller, which captured its own `ebp` before the call and never
reloads it, carries on using a frame pointer inherited from somewhere up the
stack.

## Why it is so quiet

The caller is internally consistent. It writes `[ebp-40]` and later reads
`[ebp-40]`, and both land in the same wrong place, so the function appears to
work. What it is actually doing is writing its locals **into an outer
function's frame**. The damage only shows up when that outer function reads
its own locals back, which may be many calls later and in unrelated code.

The stack is wrong too. `__EH_prolog`'s twelve bytes are never accounted for,
so the caller's `pop esi` / `pop edi` / `pop ebx` epilogue reads from the
wrong slots and returns rubbish in the callee-saved registers.

In Outrun 2 that is exactly what happened. `sub_001C33AF` computed
`esi = this + 0x28` — a `std::map` member — called four functions, and by the
time it used `esi` again the value was zero. It passed that to a red-black
tree lookup which read `[0+8]` and faulted. Nothing wrote the null; a `pop`
read the wrong slot.

## How it was found

`RECOMP_ABI_CHECK` — a CMake option that already existed and had never been
turned on for this title:

```
cmake -S titles/outrun2 -B titles/outrun2/build-abi -G "Visual Studio 16 2019" \
      -A x64 -DRECOMP_ABI_CHECK=ON
```

It wraps every call and reports any callee that fails to restore `ebx`, `esi`
or `edi`, or that returns with the wrong stack pointer:

```
[ABI] sub_00185434: esp-too-low
[ABI] sub_0018546F: ebx esi edi esp-too-high
[ABI] sub_00184A40: esp-too-low
[ABI] sub_00185AD0: esp-too-low
[ABI] sub_001BF79E: esi esp-too-low
[ABI] sub_001BF8BE: esi esp-too-low
```

Reading that list is the whole trick. The functions in the `0x0018xxxx` band
are the CRT's SEH helpers and their violations are **by design** —
`__EH_prolog` is supposed to leave the stack low. The two in the `0x001BFxxx`
band are ordinary game functions, and those are real. Both begin with a call
to `sub_00185AD0`.

**Turn `RECOMP_ABI_CHECK` on early for any new title.** It costs a rebuild and
a few percent of run time, and it converts a class of bug that otherwise
surfaces as inexplicable corruption into a list of function names.

## The probe, and what the real fix is

`scripts/probe_eh_prolog.py` makes two edits to the generated C, which is
enough to prove the diagnosis:

1. In `__EH_prolog`, publish the frame it built: `g_ebp = g_seh_ebp = ebp`.
2. At all 46 call sites, reload `ebp = g_seh_ebp` afterwards, because the
   caller captured `ebp` before the call.

With both, the two false violations disappear from the `[ABI]` list and Outrun
2 runs considerably further into the same function — the fault moves from
`sub_001C40A6+0x3AC` to `sub_001C40A6+0x9C5`.

**This is a probe and should not be shipped as one.** Generated code is
gitignored and a re-lift wipes it, which is why the script exists. The fix
belongs in `tools/recomp`, and there are two candidate shapes:

- **Narrow:** recognise the `__EH_prolog` body by signature — it is small,
  fixed, and per-XDK, exactly the kind of thing the project already matches —
  and have the translator emit the frame assignment into the caller.
- **General:** stop giving frameless functions a private `ebp`. If a callee
  can change the caller's frame pointer then `ebp` has to live where both can
  see it, the way `esp` already does (`#define esp g_esp` in
  `recomp_types.h`; `ebp` is deliberately not a macro). This is the honest fix
  and the more invasive one, and it interacts with the register-model change
  in `CLAUDE.md`'s planned work.

The narrow one is worth doing first because it is testable against two titles
today. The general one should be folded into the register-model work rather
than done twice.

## The counterpart, and what it shows

`sub_0018546F` is `__EH_epilog`, the other half of the pair. It restores the
exception registration, pops `edi`, `esi` and `ebx` **for its caller**, and
unwinds the frame:

```
mov  ecx, [ebp-16]    ; the saved fs:[0]
mov  fs:[0], ecx
pop  ecx              ; its own return address
pop  edi              ; the caller's saved registers
pop  esi
pop  ebx
mov  esp, ebp
pop  ebp
push ecx
ret
```

So `[ABI] sub_0018546F: ebx esi edi esp-too-high` is entirely by design, the
same way `__EH_prolog`'s `esp-too-low` is. Both helpers exist to break the
convention on the caller's behalf.

What matters is the last line of the lifted version:

```c
    POP32(esp, ebp); /* leave */
    PUSH32(esp, ecx);
    g_seh_ebp = ebp; esp += 4; return; /* ret */
```

It **does** publish the frame pointer. The epilogue half of the pair was
handled and the prologue half was not, and `g_ebp` is not published even
here. That makes this a gap rather than a design decision, and it is why the
narrow fix is the right first move: the lifter already knows these helpers
are special, it just does not finish the job in one of them.

## The same family, one layer down

With the probe in, Outrun 2 runs further and stops again, and the `[ABI]`
list has moved with it:

```
[ABI] sub_001C425B: edi esp-too-low
[ABI] sub_001C48FE: esi edi esp(epilogue never ran)
```

These are not a new problem. `sub_001C48FE` is eleven lines long and provably
correct: it pushes `esi`, makes two calls, pops `esi`, and its `esp += 8`
matches its `ret 4` exactly. Its violation is **inherited** — it calls
`sub_001C425B`, which returns with the stack low, so the `pop esi` reads the
wrong slot. Read the `[ABI]` list innermost-first, the way you would a stack.

`sub_001C425B` is the interesting one, and it is the same function that
produced the garbage pointer this whole investigation started with. It calls
`__EH_prolog`, pushes `ebx`, `esi` and `edi`, and then has exactly one exit in
250 lines of C:

```c
    g_seh_ebp = ebp; sub_001BB79F(); return; /* tail jmp 0x001BB79F */
```

A tail `jmp` into a shared epilogue. The real function never returns at all —
it hands control to a helper that pops its registers and does the `ret` on its
behalf. The lifter turns that into an ordinary C call followed by a return,
so the helper's stack effects land in the wrong frame.

That is the third variant of one problem: **MSVC shares prologue, epilogue and
unwind code between functions, and the lifter models each of those shared
helpers as an ordinary call.** Any fix should cover all three together rather
than special-casing `__EH_prolog` alone.

## What is still unexplained

One violation in the list is not accounted for and was not investigated:

```
[ABI] sub_0018B960: ebx
```

It loses `ebx` only, keeps the stack balanced, and is in the same CRT band.
It may be another helper that restores registers on a caller's behalf, or a
real epilogue bug. It deserves the same reading the other two just got.
