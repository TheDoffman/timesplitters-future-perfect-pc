# Persistent product, bit and exchange status

Stage0250 afterf6edb895 extends the original
[persistent flag state](guest-status-flags.md) and [shift/rotate producers](guest-shift-flags.md)
toward replacing saved CPU context and guest exception services. Those inherited
services remain unfinished; this stage does not claim complete guest EFLAGS.

## Producers and preservation

translation_product.py publishes multiplication's defined CF/OF from its sampled
wide product and makes SZPAF unknown. One-operand unsigned/signed byte/word/dword
and two/three-operand signed word/dword forms retain existing destination rules.
No caller operands are reread. DIV/IDIV status remains undefined and invalidated;
their existing checked fault/quotient behavior is unchanged.

translation_bitops.py publishes the selected old bit as CF for BT/BTS/BTR/BTC,
preserving ZF and making OF/SF/AF/PF unknown. Register indices wrap at width;
memory/register indices remain signed bit-string offsets. Narrow memory writes
now cast explicitly to their guest width, avoiding strict compiler truncation
warnings. BSF/BSR publish ZF from the original source before their scan modifies
the temporary value. Other arithmetic flags become unknown; the existing zero
source destination-retention policy is not claimed as an architecturally defined
result.

translation_exchange.py calls the original arithmetic-status helper with sampled
old/source/accumulator values for XADD and CMPXCHG, after the atomic/register
operation. XADD records old+source; CMPXCHG records accumulator-old regardless of
success. Overlapping registers retain their existing write order. Memory forms
retain the existing stronger sequentially consistent host atomic policy.

translation_lifting.py preserves these recorded producers instead of discarding
them. Explicit LOCK XCHG now preserves status like implicit-lock memory XCHG.
Only supported LOCK XCHG/XADD/CMPXCHG forms get that treatment; unsupported LOCK
arithmetic/bit families invalidate status knowledge rather than retain stale
known bits. No new unsupported instruction is advertised as implemented.

These are portable changes to project-owned modules. No snapshot bytes or new
patch installation are needed; effective original patch tail remains54. The
existing canonical-header ABI and shared production compile definition select
the same implementation on Windows and native Mac.

## Evidence and remaining work

test_guest_integer_flags.py compiles the actual producers with canonical
recomp_types.h, guest_status_flags.h and guest_state.c, with tracking and strict
warnings enabled. An independent arbitrary-precision/byte-array model checks
198368 requests across64 forms, including exhaustive8-bit MUL/IMUL pairs,
bounded negative memory bit offsets, all17 surrounding memory words, partial
registers, successful/failed CAS, explicit locks and overlapping registers.
Five compile-first broken providers must fail. A real ML64 raw-EFLAGS oracle
checks the same requests; undefined flags and zero-scan destination values are
excluded from the native defined-result comparison. The portable model still
checks the explicit zero-destination policy. Native evidence is host x86, not a
physical Xbox claim.

test_status_flags_translation.py compiles full FunctionTranslator output for
all64 forms, three boundary/data/knowledge inputs each. Its memory fixture has
guaranteed alignment for real atomics. The initial integer/legacy/production
suite passed176 checks in31.89s; the final expanded compiler/header/vehicle-join/
status suite passed281 checks in67.74s. Fresh/repeated installed-stack and
repository-context checks passed21 in23.75s. Historical patch bytes are unchanged
from the preceding fully green both-host migration CI. Both new and existing suites are selected for
shared Windows/Mac CI; hardware oracle skips off Windows. Precedingf6edb895 CI
runtime41449486367 and launcher41449486247 completed successfully.

Production generation/build, fresh installed-stack/context checks and isolated
Story acceptance pass for this stage; new-head CI remains pending. Floating compares, string compare
status, PUSHFD/POPFD/control state and actual context/exception continuation remain
unfinished, alongside the wider kernel/graphics/audio/input/title/header/fixture
replacement and final snapshot-unavailable production audit.

New production generation completed15838/15863 functions,0 failures,6,582,869
lines with98.28s lift. Release/built-ins and source/generation/archive audit pass. Fresh game evidence
for this executable is recorded below.

## Accepted Windows production evidence

Windows11 Pro10.0.26300 x64, MSVC19.44.35224.0, D3D11 flip-model backend;
Classic/Original60, minimized synthetic input and startup movies skipped.
Audit checks one persistent producer per generated instruction body:6589 product,
2 bit-test,4 scan and26 exchange, with16 translated units changed. Canonical
header/template and existing state-provider/feature selection remain valid.
Current source hashes, generation fingerprint and executable provenance match.
Executable SHA256:
fe25b7b49984b75418602695957fa881f5581fad70659893aee661a5b78928a2.

Runner69080/controller6482 both exited0. Scene4/mission1 initialized unpaused,
input enabled,8 rounds. Forward11.1542059606/back6.1848884405; fire8-to6 rounds;
pause39 frozen samples, resume58 advancing; last checkpoint139.604s under240s.
Fresh start/fire/pause captures reviewed. Profiles/fixture valid;26 protected
files retain hashes/sizes/mtimes; no run lock/dump/partition images. Playable guest
presentation samples57.8/57.9/57.9 FPS do not measure monitor scanout.
Evidence logs/remaining-integer-flags-build.json, game-check/fps JSON and captures.
No physical input/audio listening/full-campaign/native Mac gameplay claim.
