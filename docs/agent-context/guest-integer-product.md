# Original extended integer multiplication

Stage0241 replaces ordinal 281 RtlExtendedIntegerMultiply through
src/runtime/guest_integer_product.c/.h and guest_kernel_product.inc. The additive
0241-original-extended-product.patch removes the inherited bridge body and links
one shared provider on Windows/Mac. Dispatcher selection and 12-byte stdcall
cleanup remain unchanged. Division 282 and magic division 283 remain separate
replacement work; no claim about their signatures or fault behavior is made here.

The multiplicand is a 64-bit two's-complement bit pattern and the multiplier a
32-bit two's-complement bit pattern. Explicit unsigned sign extension followed
by unsigned multiplication produces the low 64 product bits. Signed overflow,
left-shifting negative values, implementation-defined narrowing to signed types,
native long widths and host pointer records are absent. No allocation, toolkit
implementation or native API dependency is introduced.

The import snapshots g_esp, obtains the VM once, resolves the complete 12-byte
read frame once and decodes its three little-endian words. It returns low/high
product words in EAX/EDX while preserving g_esp and guest memory. Null/missing,
truncated or wrapping frames leave return registers and memory unchanged because
this return type has no NTSTATUS result channel. This explicit invalid-frame
policy is not Xbox exception dispatch. Callers own stable mappings/access/data
throughout; resolution neither pins mappings nor triggers MMIO.

Public API contract reference:
[Microsoft obsolete RTL routines](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/rtlenlargedintegermultiply).
No third-party implementation is used. This computer's x64 ntdll does not export
these arithmetic routines; no native API oracle verification is claimed.

## Checks

tests/compiler/test_guest_integer_product.py compiles the real provider/import
with fatal warnings. A separate Python signed-big-integer model checks 20,080
products, including all combinations of selected signed endpoints and 20,000
seeded random cases. Both direct provider and actual import results must match.
The C fixture checks complete-frame reads, high guest aliases, exact memory
preservation, register/stack behavior, missing resolver, null/unmapped/truncated
and wrapping frames. Five intentionally broken providers/adapters must compile
before the tests reject their sign extension, word width/order, result high word
or frame length. Mac selects the same suite with undefined-behavior sanitizer;
actual Mac results require CI evidence.

Current/historical patch migration audits require the new provider/include once
and the old inline multiply body absent. Build/generation, packaging and bounded
gameplay acceptance are separate checks recorded in STATUS when actually run.
The supported US XBE import table contains none of ordinals 281/282/283.
Game validation therefore checks integration stability rather than execution of
this new provider; the compiled fixture exercises its actual import directly.

Whole-runtime independence and toolkit/snapshot-unavailable builds remain open.

Local acceptance on main a4ccf22d plus stage0241:44 focused and22 migration tests,
Windows Release/current generation/archive inspection and bounded Scotland
movement/fire/pause/resume passed on executable60c18311. Exact evidence and
limits are recorded in STATUS. New-stage CI/native Mac execution remain pending.

Source commit c7f276a7 is reviewable in [draft PR465](https://github.com/TheDoffman/timesplitters-future-perfect-pc/pull/465).
