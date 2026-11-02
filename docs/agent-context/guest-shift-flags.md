# Persistent shift and rotate status

Stage0249 after0b9d2456 on feature/guest-arithmetic extends
[the persistent arithmetic state](guest-status-flags.md) toward original saved
CPU context. It does not replace the still-inherited context/exception/unwind
services. Do not advertise partial flag knowledge as complete guest EFLAGS.

## Defined status and compiler emission

guest_status_flags.h adds original pure tsfp_status_shift and tsfp_status_rotate
helpers. translation_bits.py calls them after producing the destination, before
leaving its sampled-operand scope. translation_lifting.py no longer discards
their published status afterward. All portable source is shared by Windows and
native Mac under the existing TSFP_RECORD_STATUS_FLAGS definition.

SHL/SAL/SHR/SAR and SHLD/SHRD publish SZP from the masked result. Nonzero shifts
make AF unknown; OF is defined only for masked count1. Logical shift CF is not
advertised when count is at least operand width; SAR keeps its last sign bit.
Double shifts allow word/dword forms. Their undefined narrow counts retain the
existing no-write destination policy but invalidate all arithmetic knowledge.
Dynamic masked zero counts preserve state. Inputs are evaluated once at width.

ROL/ROR publish CF even when a nonzero masked count completes a whole operand
rotation. They preserve SZPAF. RCL/RCR use the persistent known CF in preference
to stale local carry after a call. Their ring distance uses operand width+1;
nonzero masked counts with zero ring distance preserve CF while leaving OF
unknown. Masked count0 preserves all flags. OF is defined only for masked count1,
not a larger count reduced modulo width. RCR with unknown incoming CF publishes
its known outgoing operand bit but leaves OF unknown; RCL single-bit OF depends
only on the original top two operand bits.

[Intel's instruction reference](https://cdrdv2-public.intel.com/774492/325383-sdm-vol-2abcd.pdf)
defines the count masks, flag preservation and undefined results. The persistent
known mask distinguishes those undefined bits from deterministic fallback values.
Legacy block snapshots remain available to standalone ABI consumers. Full CPU
context and other flag producers (products, scans/bit tests, exchanges, floating
compares, strings and PUSHFD/POPFD) remain unfinished.

## Evidence and CI repair

test_guest_shift_flags.py compiles the actual original emitters against canonical
recomp_types.h, guest_status_flags.h and guest_state.c with tracking enabled.
An independent one-bit-at-a-time model checks280288 requests, all widths and
masked counts, signed/unsigned boundaries, partial register preservation, carry
values and known-mask combinations. Six compile-first broken helpers must fail.
An ML64 raw-EFLAGS oracle compares274070 valid native cases; architecturally
undefined double-shift counts are excluded, undefined flag values are not compared
as defined. Portable model/negative controls run on both hosts; hardware skips
outside Windows. Production FunctionTranslator routes execute all25 operation/
width combinations through seven dynamic counts and both cross-call carry rotates.
The initial integration suite passed116 checks in24.65s; the final expanded
compiler/header/vehicle-join suite passed195 checks in43.04s.

The preceding0b9d2456 launcher workflow41444560191 passed. Runtime41444560160
reported Mac native failure:2496 passed,373 skipped,18 subtests, two failures.
One test confused scalar _flags with g_status_flags; vehicle-join checking now
matches the exact identifier. The other correctly found the installed runtime
header template missing the canonical status ABI block. Additive0249-status-
header-template.patch repairs the template rather than weakening its equality
test. The effective original tail becomes54, with historical exclusions updated;
complete install audits now require exact canonical template text. No pristine
snapshot bytes are edited. Preceding runtime workflow41444560160 completed with failure; new-stage CI
acceptance remains pending.

New-stage195 focused and55 full migration/context tests pass, with fresh
production generation/build and isolated Story acceptance below. Keep the
accepted stage0248 evidence separate. New-head CI remains pending.

Windows native CI also failed the same two cases (2584 passed,295 skipped,
18 subtests); downstream transition jobs rejected failed native/release. Both
upgrade jobs and all launcher/package jobs passed. New production generation
completed15838/15863 functions,0 failures,6,582,869 lines in93.72s lift; Release
build and full migrations are running. These are not gameplay acceptance.

## Windows production acceptance

Release/built-ins terminal0. Build audit records11062 shift and43 rotate producer
calls, exactly one per emitted instruction body across the16 regenerated units;
some batches correctly contain no rotate instructions. It also checks unchanged
canonical generated-header bytes, synchronized template text, feature selection,
archive state symbol, current generation and input hashes. Executable SHA256:
9c0f5a5945f4a84020859f05e0dc244e0edde2f978e8f0d8f4b1806f59dcaade.

Runner82090/controller48170 both exited0. Clean scene4/mission1, input enabled,
ammo8-to6, forward11.1745451650/back5.8669620283, pause39 frozen/resume58
advancing, last checkpoint117.155s under240s. Fresh start/fire/pause captures
reviewed; input hashes and executable provenance match the build. All26 protected
files retain hashes/sizes/mtimes; no run lock/dump/partition images. Playable FPS
samples57.9/58.1/57.9 at105.1/110.1/115.1 seconds are guest presentation counters,
not monitor scanout. Full migration/context suite subsequently passed55 checks in1080.73s.
This is minimized synthetic Original60/movie-skipped Story acceptance; native Mac
execution, physical controls, audio listening and full campaign are unverified.
Evidence logs/shift-flags-{build,game-check}.json, shift-flags-captures and logs.
