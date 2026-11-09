# Original guest stack diagnostics

Stage0247 replaces264 Assert,266 CaptureStackBackTrace,288 GetCallersAddress and
319 WalkFrameChain with guest_stack_trace.c/.h, guest_kernel_stack_trace.inc and
an original assertion logger in guest_kernel_rtl.c. Additive0247-original-stack-
diagnostics.patch selects the provider once, removes four inherited stubs/bodies,
and adds a thin xbox_GetGuestStackBounds adapter. It follows0246; historical
pre-layout stacks omit it and complete forward/reverse upgrades audit it.

This is stack diagnostics, not completed guest exception delivery. RtlCaptureContext
still zeroes its record, RtlRaiseException still lacks real guest dispatch, and
Unwind retains inherited behavior. Accurate capture/restore needs complete guest
flags and continuation support; translation_conditions.py explicitly documents
that its producer ABI lacks full EFLAGS across calls/joins. Do not substitute a
host CONTEXT or advertise synthesized partial flags as a complete guest context.

## Frame collection and publication

The host-only view snapshots ESP, EBP, kernel call site, live stack [low,high) and
executable [code_low,code_high). Position0 is the kernel call site, followed by
saved return PCs from guest EBP pairs. Frames are four-byte aligned, at/above ESP,
fully inside the supplied extent, and advance at least8 bytes. Mapped/read access
is checked for each complete pair; null/end sentinels, cycles, backward/overlapping
records, holes and bad tails stop collection. Selected PCs must be in the supplied
executable range. Skipped PCs need not be valid code addresses. This walks guest
EBP frames, not every translated native call or frame-pointer-omitted guest frame.

Collection finishes in host storage before output. Frame/output/argument aliases
therefore work. Only actual output bytes are resolved; bad output or hash aliasing
rejects without any publication. The optional four-byte hash is the wrapping sum
of captured PCs, including0 for an empty valid result. Invalid views/outputs or
host staging allocation failure return0 unchanged. Caller owns stable mappings,
view storage and stack lifetime; no metadata query pins them. The mathematical
maximum number of nonoverlapping pairs bounds staging and traversal, so huge
requested counts/skips cannot wrap arithmetic or follow cycles indefinitely.

Capture skips the requested prefix, clamps count to the USHORT return range and
returns actual captured count. Its true ABI is FOUR arguments/16 cleanup bytes;
the old dispatcher incorrectly popped20. Walk returns actual count, uses the
high flag bits as a skip count and returns0 for nonzero low flag byte. The public
Walk interface is reserved/undocumented: native references validate flags0 only,
not equivalence for every undocumented flag. Native high-skip return-count quirks
are not reproduced as uninitialized output. GetCallersAddress skips the kernel's
immediate call site and writes the next two parent PCs, zero for missing parents.
Both outputs are optional. Exact aliases publish the second value last; partial
aliases reject. A zero known kernel call site can still be skipped for parents.

## Live stack bounds and assertion behavior

tsfp_thread_stack_bounds reads original thread-storage metadata under its existing
lock. Bootstrap space is clipped at live worker slices; an active worker gets its
exact256-KiB slice. With no workers, the original full bootstrap extent remains
available. Heap stacks require one of the tracked live512-KiB thread owners;
unrelated/freed heap storage rejects. Low-image relocation is retained. Cloned
TIB fields intentionally retain other title ABI hints and are not trusted as
current-stack limits. Outputs are distinct stable host words outside the service;
failure preserves them. The lock protects metadata, not mapping/lifetime teardown.

Assertion imports read all FOUR arguments, including optional Message, and clean
16 bytes; the old bridge read three/cleaned12. Existing bounded tsfp_rtl_text copies
expression/file/message into host buffers before fixed-format logging. Null/bad
strings become '?', long strings use its explicit truncation marker, and guest
format directives stay literal text. The existing debug-only breakpoint policy
uses the shared logger's Windows/POSIX leaf. Release logging neither mutates guest
memory nor fabricates successful exception dispatch.

Fixed argument frames: Capture16, Assert16, GetCallers8, Walk12. VM is obtained
once; argument reads cover each full frame before service publication. Stack
helpers snapshot guest registers and query bounds once; assertions need no bounds.
Only EAX changes (count or void0); other guest registers and ESP are retained.
Malformed frames preserve guest bytes and return0. Bounds/VM callbacks preserve
architectural state and run with stable caller-owned inputs.

Primary interface declarations:
[Microsoft backtrace](https://learn.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-rtlcapturestackbacktrace),
[Xbox public declarations](https://github.com/XboxDev/nxdk/blob/master/lib/xboxkrnl/xboxkrnl.h).
The native probe uses real x86 ntdll functions over controlled native stack pairs,
not copied implementation code or guest/native pointer casts.

## Verification and remaining work

tests/compiler/test_guest_stack_trace.py runs6128 seeded independent complete
4096-byte RAM cases through the real provider and imports. Cases cover skip/count
limits, malformed/overlapping/wrapping chains, executable ranges, aliases, hashes
including wrap, optional outputs, empty prefixes, frame arguments and all register
preservation. Ten compiled bad walkers/adapters must fail at runtime. A previously
untested alignment counterexample now supplies an otherwise-valid misaligned
record; no assertion was weakened. Installed selection and executed real dispatcher
switches verify all four argument sizes.

Windows adds527 real x86 ntdll calls over0..16 controlled frames. Native pointers
are normalized separately; native hash checks use actual addresses before
normalization. Stack-pointer equality checks native calling conventions. Native
Caller ordering and flags0 traces agree with direct/import guest results. Portable
cases run with undefined-behavior sanitizer on Mac; the native probe skips there.
Thread-storage contracts additionally exercise relocated/bootstrap/worker/heap
bounds, release/stop/foreign-owner exclusion, null/alias outputs and four compiled
bad bounds queries. Actual mapped logger tests verify optional message, literal
%n, invalid strings and untouched guest bytes. Existing concurrent storage and
critical-section tests remain selected. Exact build/migration/Story/CI evidence is
in STATUS. Native Mac title gameplay, full context/SEH, other kernel/graphics/audio/
input/title/header/fixture replacement and old-checkout/snapshot-unavailable
production generation/build/package acceptance remain unfinished.
