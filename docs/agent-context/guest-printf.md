# Original narrow guest formatting

Stage0239 selects `src/runtime/guest_printf.c/.h` and guest_kernel_printf.inc for
RtlSnprintf/Sprintf/Vsnprintf/Vsprintf, ordinals361..364. The legacy formatter,
two raw cursor helpers and four bridge bodies are removed. All four remain cdecl:
dispatcher stack cleanup is0. The adapters capture g_esp, resolve/copy the complete
8/12/16-byte fixed frame, then parse from private32-bit words. Direct argument
lists follow that frame; V variants read the explicit32-bit list pointer. Frame
wrap, missing callback/span and null V list preserve guest output and return0.
Only EAX changes; no host va_list or native pointer record is cast over guest data.

## ABI and compatibility boundary

The cursor consumes4-byte scalars/pointers and8-byte integers/binary64 floats at
the current position, with no extra8-byte alignment. tests/compiler/printf_x86_probe.c
is compiled with the actual Microsoft x86 toolchain and executed under WoW64.
Its raw va_list bytes must match an independent packed32/64/double/32 record.
This establishes the alignment that the old bridge got wrong. Scalar values and
IEEE doubles decode from little-endian bytes; floating bits are copied rather
than numerically converting the integer representation. Guest long/size_t/ptrdiff
remain32 bits on both Windows and LP64 Mac. I64/ll/q/j consume64 bits; h/hh narrow
integer values explicitly. L floats use the guest MSVC binary64 convention.

This preserves this project's existing bounded-output policy: return the bytes
actually stored, always append NUL for nonzero capacity, and stop processing once
capacity-1 bytes are stored. It is neither C99 would-have-written semantics nor
old MSVC negative-on-truncation semantics. Sprintf/Vsprintf retain the historical
65536-byte output ceiling. Capacity0 performs no memory reads/writes. Invalid
input, unrepresentable width/count, mapping failure or private-allocation failure
returns0 without publishing any guest output. Empty valid output also returns0.

The parser implements d/i/u/o/x/X, c/s/p/n and f/F/e/E/g/G/a/A, flags, literal and
star width/precision, h/hh/l/ll/I32/I64/q/z/t/j integer lengths and L floats.
Negative star width selects left alignment; negative precision is omitted.
The host standard snprintf renders typed private atoms with normalized lengths;
it never receives guest pointers or argument cursors. Float spelling/rounding and
locale follow that host C library. Large atoms use their real length instead of
the old fixed511-byte clipping. Embedded NUL from %c contributes a stored byte.

Strings and characters remain narrow even with l, matching the prior runtime
policy. S/C/Z and other unknown conversions remain literal '%<specifier>' without
consuming a value (star arguments still consume slots). Unterminated trailing
percent/length stops at the valid prefix. Pointer formatting renders the32-bit
guest value with %08x or %#08x: lowercase hex and minimum field width8, including
any optional prefix. Pointer width/precision modifiers consume their arguments
but do not change that compatibility representation. This is not
a complete MSVCRT wide-string, counted-Z, positional or locale implementation.
Those absent facilities are not inferred from a passing narrow-format test.

Standard references, not implementation sources:
[x86 argument conventions](https://learn.microsoft.com/en-us/cpp/cpp/argument-passing-and-naming-conventions?view=msvc-170),
[standard versus legacy snprintf](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/snprintf-snprintf-l-snwprintf-snwprintf-l?view=msvc-170),
[vsnprintf semantics](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/vsnprintf-vsnprintf-vsnprintf-l-vsnwprintf-vsnwprintf-l?view=msvc-170).
The header lists the actual local contract; no host CRT behavior is silently
substituted for the runtime's clipped return/termination policy.

## Validation, staging and publication

All format/string/argument/count accesses use full checked spans, including
unaligned and aliased guest storage. Source string precision limits reads; zero
precision does not dereference its pointer. Null %s uses '(null)' as before.
The cursor uses a wider position so advancing past the address space cannot wrap
to another guest slot. The host output buffer grows only after its actual guest
write footprint validates, rather than allocating caller capacity up front.

Output and %n actions are private until parsing/rendering succeeds. %n stores
1/2/4/8-byte counts as selected by its length; null targets remain no-ops. A byte
overlay lets subsequent string/argument/format reads see staged count writes in
encounter order, including distinct guest addresses mapping to the same host
bytes. Counts are then published in that order. A count target overlapping the
actual destination output, including aliases, rejects the transaction. Input/
output overlap is buffered; the host CRT sees private input/output objects.

Every failure releases private buffers and preserves destination and all count
targets. The optional host allocator follows realloc ownership/failure semantics;
it may not call guest code or change mappings. Mapping/access/data stability is
the caller's responsibility for the whole call; resolving is not a lifetime pin.
No guest allocation owner is created, released or inferred by these operations.

## Verification routes

`test_guest_printf.py` compiles the real provider/adapters with fatal warnings.
14000 seeded typed host-CRT atom oracles compare clipped counts and whole RAM
images across guest integer lengths, full-width integer endpoints, floating
representations, field flags/stars, strings and character bytes. Independent
literal cases check mixed4/8-byte cursors, negative zero/precision, atoms over512
bytes, embedded NUL, typed count widths, staged read-after-count, aliases, input/
output overlap, clipped argument nonconsumption, missing/readonly/wrapping spans,
bad widths, allocation failures, unchanged output and allocation balance.

The actual import fixture maps complete fixed frames and exercises all four cdecl
shapes, explicit lists, high guest addresses, pointer aliases, frame truncation/
overflow, null lists and unchanged g_esp. Nine bad providers and four bad adapters
must compile before failing at runtime. The genuine x86 probe runs on Windows;
Mac runs the portable cursor/oracle cases and skips that Windows-only probe.
Selection audit verifies one provider/include, absent legacy helpers and zero
cleanup. Fresh/historical upgrades must install that same route. Production build,
archive/generation, isolated Story and native Mac title acceptance remain separate;
see STATUS for exact measured scope. The overall replacement goal is incomplete.
