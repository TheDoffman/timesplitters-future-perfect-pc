# Canonical EBP during ordinary guest execution

This production change prepares live context snapshots for the unfinished SEH
replacement. [Instruction continuations](context-continuations.md) remain opt-in;
RtlCaptureContext265/RtlRaiseException302/RtlUnwind312 are still inherited.

## Architectural cell and compatibility

Regenerated normal functions bind ebp through a constant pointer to the existing
thread-local g_ebp cell declared by src/runtime/recomp_types.h. Frame setup,
scratch EBP use, POP EBP and LEAVE all operate on that cell. There is no local
register value to flush before an opaque helper or repair after returning.
Pointer bindings are function scoped and do not copy register values; the
existing GPR reference policy is independent. Setjmp resumes with the same
unmodified pointer; saved guest registers still come from the original nonlocal
jump provider. Ordinary C optimizations remain possible between observable calls;
this does not add asynchronous host fault/register capture.

The shared header/storage ABI is unchanged and no capability flag is needed.
Existing generated bodies retain their old behavior until regenerated. New
bodies require the existing g_ebp declaration and never replace actual incoming
EBP with g_seh_ebp. The latter remains a legacy helper/frame cache, not
architectural state. Function-local alias macros are undefined after each body.
Guest stack operations and RET cleanup are unchanged; canonical EBP is restored
by the actual guest epilogue rather than a host convention that could erase a
context edit. The shared header's local-EBP comment refers to alias scope in
regenerated bodies, whose register storage is now the existing TLS cell.
The menu tab adapter still reads its saved index at the existing ESP-relative
call-site slot; its old local-EBP comment was corrected without behavior changes.

## Macro-safe context state

The original context state's ten register members are r_eax/r_ebx/r_ecx/r_edx/
r_esi/r_edi/r_esp/r_ebp/r_eip/r_eflags. Bare architectural tokens can be generated
C macros, including pointer expressions in reference mode; the prefix prevents
those macros rewriting structure declarations or field accesses. The state has
the same712-byte layout and the guest record remains716 bytes at the same offsets.
All codec fields, checkpoint operations and native fixtures use the prefixed API.
This does not add live selector/debug/80-bit FPU/MXCSR modeling or a SEH adapter.

## Evidence

test_canonical_ebp.py compiles both framed and FPO callers, with ordinary and
actual stable-reference modes. A native callee checks live scratch EBP, while
return and complete saved-stack cells prove original EBP restoration. These four
cases fail against the prior local-cache generator. A compiled provider with a
local copied EBP cell must fail at runtime. A separate divide fault check retains
the existing correct EBP publication to the fault helper; zero-divisor reason0
was verified from the provider before accepting that check.

The continuation fixture now explicitly enables RECOMP_GPR_REFS_SUPPORTED when
requesting reference mode. Previously it set USE/ANCHORS without the capability,
so both modes took direct aliases. The real reference mode exposed context-member
macro collisions at compilation. Prefixing the members repairs that interface;
actual edited-context native checkpoint resumption runs in both modes now.
Focused context/checkpoint/codec/canonical checks initially pass62 cases16.90s,
including the actual Win32 SDK layout oracle. Broader native contracts, production
generation/Release/gameplay and new-head CI are separate gates recorded in STATUS.
The normal generated C changes with this stage; do not reuse the preceding stage's
unchanged-generated-file claim or its executable as new gameplay acceptance.

A build audit exposed an unnecessary prototype capability macro present only in
the generator header, not the shared runtime template. The final implementation
uses the existing ABI directly, so source/template/generated headers remain
identical. The prototype binary is not accepted for this final source. During its
full rebuild a reference-thread test hit its unchanged10s wait; after compilation
became idle all27 installed-header/nonlocal/generation checks passed10.99s. The
final simplified generator still requires fresh generation/build/game acceptance.

Final acceptance:246 broad native cases59.23s,46 title/control cases8.88s and116
repository/generation cases50.93s pass. Generation15838/15863/0fail6595934lines
126.09s, Release/built-ins/crash symbols and exact source/template/header/archive
provenance pass;15838 scopes/6447 live bindings,16 changed units. Executable
SHA256a7fc10be9fb228d6ff4b7959babe6746d4ad1892b74f4b581021a9e1bbc669d5.
First game attempt remains rejected (intro at frontend,2.4-3 FPS). Both an unchanged
profiled retry and final unprofiled run passed; cause of the initial slowdown is
unproven, not a source fix. Final99.188s Story check verifies movement11.17624/back
6.18489, ammo8to6, pause39/resume58, three reviewed captures and26 protected files.
See STATUS for exact Windows/synthetic/Original60 scope and new-head CI.
Inherited Windows capture_crash_guest still reports g_seh_ebp, unlike native Mac's
actual g_ebp capture; the remaining title/diagnostic adapter audit must resolve it.
It is a crash snapshot, not an interrupt save/restore path. No claim of complete
live context/SEH, native Mac gameplay or whole-runtime independence is made here.
