# Original context records and caller-cleaned handlers

Stage0253 committed asa0d4fd9a; full guest context/SEH replacement is NOT
complete. [Flag-stack contracts](stack-flags.md) remain the preceding stage.
The new codec and callback convention are prerequisites, not a dispatcher.
RtlCaptureContext265 still has its inherited zeroing stub; raise302/unwind312
are also still inherited. Do not report their removal from this stage.

## Original context interchange

src/runtime/guest_x86_context.c/.h encode/decode all six groups in a716-byte
little-endian i386 CONTEXT, independently of host CONTEXT layout/pointer size.
The state uses explicit32-bit words plus opaque112-byte x87 and512-byte extended
images. Control includes EBP/EIP/CS/EFLAGS/ESP/SS; integer has the other six GPRs,
segments have GS/FS/ES/DS; debug stores DR0-3/6/7. Raw FPU/extended groups preserve
every bit, without converting to the current double-backed execution model.

Encode writes selected groups and flags only. Decode preserves unselected state.
Both snapshot source/collect results before destination publication, supporting
overlapping state/record storage. NULL/short/unsupported architecture or group
flags reject unchanged. i386 with no selected group is valid. Inputs, outputs
and mappings remain caller-owned and stable. This codec validates format, not
segment descriptors/debug controls/EFLAGS legality or CPU restoration. It captures
no live registers, issues no host NtContinue and does not add x87 tag/80-bit
execution or guest MXCSR. [Microsoft's x86 record declaration](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-context-r2)
is the ABI reference; no external implementation body is incorporated.

Additive0253-original-context-records.patch selects the provider in the shared
kernel archive. apply-patches.py appends it after0249, leaving historical patch
bytes intact; upgrade fixtures omit it in older checkpoints and require one
selection in the final stack. An archived codec does not establish a live
RtlCaptureContext adapter. The permitted Windows SDK is used only by the native
layout oracle; production provider includes standard C headers alone.

## Original cdecl callback contract

guest_callback_call.c/.h adds tsfp_guest_callback_cdecl for at most four32-bit
arguments. It shares checked preparation and architectural capture/restore with
the existing stdcall path; cdecl expects the callee to consume only its return
address, then the wrapper restores the incoming state/stack. Malformed cleanup
rejects result publication. Result receives pre-restoration EAX only on a balanced
normal return. Rejected preparation changes no guest bytes/state. A real native
guest longjmp bypasses restoration; target continuation owns registers. Caller
still owns operation scopes across nonlocal transfer. This invokes a validated
translated routine, not an arbitrary host function, and dispatches no exception.

## Native evidence and checks

test_guest_x86_context.py compares8710 deterministic requests against an
independent byte model: all64 group combinations, partial buffers/bad flags,
all bytes of unselected groups and arbitrary FP/debug/selector payloads.
Each compiled fixture checks768 alias layouts and whole-arena guards plusNULL
rejection. Five wrong providers must compile then fail. A real32-bit Windows SDK
oracle assigns named CONTEXT fields independently and compares complete images;
it proves i386 layout, not guest CPU capture or physical Xbox behavior.

test_guest_cdecl_callback.py executes both status ABIs, argument counts0-4,
both actual cleanup conventions, full frame-byte guards, malformed-result
preservation, rejected entry/mapping/args/stack, representative architectural
state and native nonlocal transfer. Four compiled wrong providers must fail.
Existing complete callback-state/negative tests remain selected.

test_native_unwind_contract.py compiles original temporary FS registrations and
calls actual Win32 x86 RtlUnwind in16 cases. Target is retained and skipped;
lower nodes run in order, flags are ORed, handlers receive nonNULL context/
dispatcher, and each handler remains immediately below an internal wrapper on
the chain. ReturnValue appears in context EAX before callbacks. Editing context
EIP/EAX causes a real changed continuation and is visible to later handlers.
In these measured x86 cases TargetIp itself is ignored, despite the generic
[API description](https://learn.microsoft.com/windows/win32/api/winnt/nf-winnt-rtlunwind);
do not generalize to other architectures or physical Xbox. The probe restores
its own FS head and native stack before returning; SafeSEH is disabled solely
for this original test executable's manually registered handler. It supplies
evidence for the next implementation, not a substitute for a guest dispatcher.

Initial combined31 checks pass13.49s. Production generation/Release pass below. Full migrations passed except the
old54-tail repository assertion; it was extended to55 with0253, and the affected
context suite rerun passes. Fresh Story and new-head CI remain pending. Native SDK/NT probes skip on Mac;
portable codec/callback/negative checks are selected in both-host CI.
Next: live architectural snapshots (including missing selector/FP semantics),
translated-PC continuation/lifetime support, then real raise/unwind integration.
Full renderer/audio/input/title/header/fixture/provenance replacement and
snapshot-unavailable production generation/build/package audit remain required.

Stage0253 production checkpoint:309 broader checks pass71.63s; generation
15838/15863/0fail,6575724lines103.75s lift. Release/built-ins/crash symbols pass.
Audit selects one original context provider and finds codec/cdecl symbols in
the kernel archive; all generated C/header bytes match the preceding stage.
Executable SHA256:
0001c4388b43f599560d02807104533f105d172f928ea93226acf5945a7806fc.
All26 protected files unchanged. Full migrations remain live; fresh Story
acceptance waits for a separate TSFP Launcher game session to finish. This is
not yet new-stage gameplay or CI acceptance; live context/SEH adapters remain
inherited and full replacement remains incomplete.

Full migration/tracking/context run:78 passed, one old54-tail assertion failed,
1209.87s. Its complete ordered expectation now includes0253 as entry55; all20
repository-context checks pass on rerun. No provider/patch/loader bytes changed
after that migration run; no second full migration run was needed.

Stage0253 final Windows local acceptance:309 focused checks71.63s plus59
patch migration/tracking cases and20 repository-context checks now pass.
Original combined migration run78pass/one stale54-tail assertion1209.87s;
its full55-entry order repaired and all20 context checks rerun5.80s.
Generation15838/15863/0fail,6575724lines103.75s lift; Release/built-ins/crash
symbols pass. Canonical header/template/feature and current source/archive
audit pass; codec selected once, codec/cdecl symbols archived, generated bytes
unchanged. Executable SHA256:
0001c4388b43f599560d02807104533f105d172f928ea93226acf5945a7806fc.
Runner37205/controller21885 both exited0: scene4/mission1/inputenabled,
forward11.1762388966/back6.1848863931, ammo8to6, pause38 frozen/resume58
advancing, last112.909s under240. Reviewed3fresh captures; source inputs and
actual run executable provenance match. All26 protected files unchanged,
profiles/fixture valid, no lock/dump/Partition images. Windows11 Pro10.0.26300
x64/MSVC19.44.35224.0, D3D11 flip, Classic/Original60/minimized synthetic/
movies skipped; guest presentation57.7/57.9/58.4 FPS, not scanout. Not physical
input/listening/full-campaign/native Mac title proof. Evidence logs/context-
foundation-{build,game-check}.json, final game/controller logs, accepted report/
provenance/run0.err and captures.
Standalone test-header CI repair is committed/pushed asc573b863; runtime
41475286277 and launcher41475286281 remain live at this checkpoint. No test
weakening or production flag change; new-stage CI remains required.
This stage provides original context-record/cdecl primitives, not live265/302/
312 replacement. Translated-PC continuation, missing live architectural state
and actual exception/unwind/IRQ support remain open, as does the full runtime/
tooling/title/header/fixture/provenance and toolkit-unavailable production goal.

Exacta0d4fd9a runtime41476378361 failed one callback mutation on both hosts:
test_io_lifetime's wrong-EAX mutation still expected the pre-cdecl full balance
statement. The actual provider correctly includes caller_cleanup now. The
mutation matches only the unique value=g_eax;balanced= prefix and still replaces
callee EAX with saved caller EAX; compiled runtime rejection is still mandatory.
No production behavior or negative assertion is removed. Windows2739 andMac2644
other native cases passed; Windows patch upgrades passed, Mac upgrades cancelled,
and transition/PR gates failed with the native result. New-head CI remains
required. The original context/cdecl tests themselves passed in both native jobs.

Next prerequisite: [native execution checkpoints](context-execution.md).

[Canonical EBP](canonical-ebp.md) documents the macro-safe r_* state members;
the712-byte state and716-byte interchange offsets are unchanged.

The following [original registration-chain dispatcher](guest-exceptions.md)
implements raise/unwind bodies and a translated edited-PC integration fixture;
actual kernel import binding and native ownership/barriers remain unfinished.
