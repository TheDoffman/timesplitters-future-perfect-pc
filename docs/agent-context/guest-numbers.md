# Original integer/string conversions

Stage0240 selects src/runtime/guest_numbers.c/.h and guest_kernel_numbers.inc
for four imports:267 ANSI-to-integer,309 counted-Unicode-to-integer,292 integer-
to-ANSI and293 integer-to-Unicode. Their dispatcher cleanup stays12 bytes except
292 at16. Fixed frames are read as complete checked12/16-byte spans after taking
a local g_esp snapshot and obtaining the VM once. Only EAX changes. Missing or
wrapping frames return C000000D before service calls; g_esp and EDX are retained.
All four old bridge bodies and their private digit tables are removed by patch
0240; no native RTL module is reintroduced. Old arithmetic/exception/charset and
string creation/freeing bodies remain separate replacement work.

## Original provider contract

Numeric bases are0/2/8/10/16. Formatting0 means decimal. Parsing0 recognizes
lowercase0x/0o/0b; otherwise it uses decimal (012 becomes12). Optional sign follows
leading nonzero ASCII control/space units through32. Digits stop at NUL, a unit
outside ASCII0-9/a-f/A-F or a digit outside the radix. Accumulation is modulo2^32
and a minus sign applies unsigned negation. No native long width, locale, signed
overflow, host pointer records or wchar_t size participates. The extended ANSI
signed-char whitespace quirk in Windows ntdll is deliberately not reproduced:
high bytes are not ASCII whitespace. Native-reference ANSI cases use ASCII;
portable literal cases establish the high-byte boundary. Uppercase base prefixes
remain ordinary digit-prefix termination rather than being recognized aliases.

ANSI source reads use checked bytes only as far as parsing needs. A missing span
before termination/invalid digit preserves output and returns C000000D. Counted
Unicode uses an8-byte little-endian Length16/MaximumLength16/Buffer32 descriptor:
Length defines the whole claimed read span, not MaximumLength. Odd Length, missing
record/body/output, readonly output or address wrap preserves all outputs. At the
counted end an implicit stop prevents reads past Length, without requiring a NUL.
Input/output overlap, including aliases, works because parsing publishes only its
final four-byte little-endian value. Empty valid Unicode and an unsupported base
with structurally valid Unicode spans return C000000D and publish value0; other
unsupported bases preserve output. Those semantic-error outputs differ from
structural/mapping failures, which remain unchanged. Null raw source is invalid.

Formatting treats all32 bits as unsigned, including decimal UINT32_MAX. Digits
are uppercase for hexadecimal. ANSI capacity is an upper bound, not a requested
padding width: exact digit capacity writes no terminator; spare capacity writes
one NUL; no other bytes change. Unicode requires all digits plus a two-byte NUL
within MaximumLength and sets Length to digit bytes only. Body and descriptor
outputs must not overlap in either guest addresses or actual mapped host bytes.
Insufficient capacity returns80000005 (BUFFER_OVERFLOW), preserving body/header.
ANSI validates only its actual digit/optional-NUL write span, so a huge declared
capacity does not allocate, zero-fill or require mapping the entire declaration.
The provider allocates nothing. Caller owns stable mapping/access/data throughout;
resolving is not a pin, owner, lifetime authority or an MMIO operation.

This replaces old zero-padding and ANSI one-byte-past-capacity writes, unsigned
Unicode-as-negative-decimal formatting, Unicode terminator overrun, base0 octal
inference/formatter rejection and saturating overflow status. Supported ordinary
contracts follow public Microsoft API documentation and real Windows oracles,
not copied third-party implementation. Bounds/error publication choices are
explicit above; passing these fixtures is not complete Xbox RTL/locale proof.

API references (contracts only):
[ANSI parsing](https://learn.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-rtlchartointeger),
[Unicode parsing](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-rtlunicodestringtointeger),
[Unicode formatting](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-rtlintegertounicodestring).

## Verification routes

tests/compiler/test_guest_numbers.py compiles real providers/adapters with fatal
warnings.14000 seeded independent Python big-integer/byte-model cases compare
status and complete512-byte memory images, including unsigned endpoints, signs,
base prefixes, long overflow strings, aliases, malformed/readonly/hole/wrap spans,
odd lengths, capacity boundaries and untouched guards. Eight broken providers
must compile before failing those vectors. Literal cases independently cover
counted end without terminator, source MaximumLength not being a read bound,
semantic-zero versus unchanged structural errors, alias record/body rejection,
exact ANSI capacity, huge declared capacity and Unicode terminator room. Fixture
startup also checks null VM/resolve paths. Binary test streams explicitly use
Windows binary mode; CRT newline/control-Z translation is not an API failure.

Windows additionally compares6000 requests with the actual ntdll four APIs,
constructing host pointer records separately and serializing only results back
to guest-width memory. Both native/portable oracles compare full output memory,
not implementation-returned expectations. Mac runs the portable cases and skips
the Windows-only reference. The import fixture links the real module and include:
complete frames, all four shapes, high-address aliases, truncation/wrap/null stack,
register preservation and four compile-first bad adapters. Selection checks one
provider/include, no old bodies and unchanged ordinal cleanup. Current/historical
upgrades must install the same provider. CI selects the suite on both hosts.

Production Release/archive/generation, isolated Windows Story, packaging and
native Mac title checks remain separate. See STATUS for exact measured revision,
hashes and limits; the larger replacement objective is incomplete.
