# Original extended integer division

Stage0242 selects src/runtime/guest_integer_division.c/.h and
guest_kernel_division.inc for ordinals282/283. Additive patch0242 removes both
inherited bridge bodies and links one provider for Windows/Mac. The existing
original guest_integer_fault.c moves from title sources into xbox_kernel so
utility consumers can resolve the new kernel fault reference too. No duplicate
fault implementation is added. This patch runs after2090 layout and2140 native
Online routing; older ledger bytes remain valid unwind inputs. It follows
[original multiplication](guest-integer-product.md). No third-party implementation
is used; standard integer C and the existing project VM/fatal boundary suffice.

## Arithmetic and guest ABI

Ordinal282 divides an unsigned64-bit bit pattern by an unsigned32-bit divisor.
The pure provider publishes quotient/remainder together only on success; zero
or missing host result leaves it unchanged. The import uses a complete checked
16-byte little-endian frame (dividend low/high, divisor, optional guest remainder
pointer). A zero divisor enters existing tsfp_guest_divide_error with the guest
caller address and reason0, before writing guest memory or result registers.
The old bridge's silent zero quotient/remainder fallback is removed. This is
fatal host divide-error handling; guest SEH dispatch remains unfinished.

For a nonzero divisor, a nonnull remainder address must have a checked writable
four-byte span with no guest wrap. Input words are snapshotted before output
resolution/publication, permitting remainder aliases of argument storage and
high guest addresses. Only four little-endian remainder bytes and EDX:EAX change;
null remainder is allowed. Null/missing/truncated/wrapping frames or inaccessible
remainder storage preserve memory and result registers. These structural-error
choices do not implement guest access-violation dispatch. g_esp remains unchanged;
dispatcher owns16-byte cleanup. Caller owns stable mappings/access/data; resolving
neither pins ownership/lifetime nor performs MMIO.

Ordinal283 uses five words: dividend low/high, fractional factor low/high and
CCHAR shift byte. Dispatcher cleanup is corrected from16 to20 bytes. The inherited
ordinary-divide alias and invented remainder-pointer write are removed. The new
provider multiplies the unsigned magnitude of the signed dividend by the unsigned
64-bit factor, takes the high64 product bits, shifts them right and restores the
sign modulo2^64. Four32x32 products and explicit carries avoid int128 and signed
overflow, including INT64_MIN. The low argument byte is the shift; values>=64
produce zero. There is no output pointer and no ordinary integer division here.
Only EDX:EAX changes after a checked complete20-byte argument frame.

The public Microsoft RTL signature (LARGE_INTEGER dividend/factor, CCHAR shift)
and real32-bit Windows stdcall calls establish the proposed20-byte ABI, rather
than the inherited comment. The supported US XBE imports none of281/282/283,
so no Xbox caller trace is claimed; Windows reference evidence is not complete
Xbox kernel compatibility. This stage preserves supported-title integration
with separate bounded game checks.

API contract references only:
[Microsoft RTL catalog](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/rtlenlargedintegermultiply),
[Microsoft Windows2000 DDK reference, printed page465](https://bitsavers.trailing-edge.com/pdf/microsoft/windows_2000/097-0002734_Windows_2000_Driver_Development_Reference_Vol_2_2000.pdf).

## Verification

tests/compiler/test_guest_integer_division.py runs the real provider/import with
an independent Python big-integer model and complete256-byte RAM comparison.
12440 cases cover signed endpoints, carry boundaries, unsigned quotients,
optional/aliased/readonly/wrapping outputs and all shift-byte patterns. Literal
fixture guards cover absent resolvers, null/unmapped/truncated/wrapping frames,
null result and unchanged zero-divisor outputs; a captured fatal-boundary call
checks site/reason and unchanged registers/memory. Eight intentionally broken
providers/adapters must compile before their results or frame checks reject them.
An extracted actual stdcall_args_for_ordinal switch is compiled and executed to
check12/16/20-byte cleanup for281/282/283.

On Windows,6440 independent vectors also call actual x86 ntdll exports in a
separate MSVC/WoW64 process. Native pointer records stay native; results serialize
back as numeric bits. The probe checks pre/post stack pointers, uses real host
SEH for divide-by-zero C0000094 and compares untouched result/remainder sentinels.
The portable math/import cases run on Mac with undefined-behavior sanitizer;
the Windows-only oracle skips there. Existing fatal divide-boundary tests cover
actual Windows exception and POSIX signal behavior separately.

Current/historical patch audits require one provider/include, no old inline
bodies and20-byte magic cleanup. Build/archive/generation, packaging, native Mac
title and bounded game evidence remain separate checks recorded in STATUS.
Whole-runtime and toolkit/snapshot-unavailable acceptance remain unfinished.

Historical pre-layout texture/main subsets omit0242 with2090/2140 because they
predate the grouped title paths. Their switch test additionally executes full
current install, reverse migration to the historical snapshot with unrelated
edits intact, then current install again, asserting original division and single
fault ownership. This tests the new dependency instead of mixing future source
layout into a historical subset.

Local acceptance on ad9f434c plus0242:57 focused cases and22 migration cases
verified (19 initially passed,3 fixture repairs/reruns passed with stronger
forward/reverse checks). Release, utility/built-in tests, archive/current-generation
inspection and bounded Scotland movement/fire/pause/resume passed on6cfbb400.
All generated title bytes match the accepted product build. See STATUS for exact
hashes/measurements and limits. New division-stage CI/native Mac title execution
remain pending; earlier ad9f product CI passed on both hosts.
