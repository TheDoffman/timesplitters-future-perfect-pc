# Floating and string comparison status

Stage0251 is in progress after96255dc2. It extends the original persistent
[arithmetic](guest-status-flags.md), [shift/rotate](guest-shift-flags.md) and
[integer producer](guest-integer-flags.md) state toward original saved context.
Preceding96255dc2 runtime41454608583 and launcher41454608604 CI both passed.
Actual context/exception continuation and the wider replacement remain open.

## Publication and preservation

translation_compare.floating_status emits one persistent status write from
already sampled operands. translation_sse.py uses it for COMISS/UCOMISS and
COMISD/UCOMISD. translation_x87.py uses it for FCOMI/FUCOMI and popping variants.
Unordered sets CF/PF/ZF; less sets CF; equal sets ZF; greater clears all three.
OF/SF/AF are explicitly cleared and all six arithmetic bits become known.
Traditional non-I x87 comparisons still change only their floating condition
state; they preserve integer flags. Sources, unaffected SIMD lanes and existing
stack/pop behavior remain intact.

This follows the existing binary32/binary64, masked-exception model. It does not
add guest MXCSR/unmasked floating exception delivery, extended intermediates or
stack-tag tracking. See [Intel's x87 instruction reference](https://cdrdv2-public.intel.com/868140/253666-089-sdm-vol-2a.pdf)
and [scalar SSE reference](https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-2b-manual.pdf).

translation_strings.py publishes full subtraction status from _str_value and
_str_other after each completed CMPS/SCAS comparison, before pointer/count updates
and repeat termination. No operand is read twice for publication. REP count0
performs no read and preserves incoming status. Strings that do not compare
(MOVS/STOS/LODS) preserve arithmetic flags, including their repeated and bulk
paths. The tests exposed that the previous persistent-state postpass discarded
knowledge after repeated non-comparison strings; validated preserves_flags
classification repairs that path without admitting unsupported REP forms.

translation_lifting.py retains these publishers instead of forgetting them.
Feature-enabled emitters lazily load original helpers; legacy standalone copied
SSE/x87/string fixtures keep their existing dependency/ABI contracts with tracking
disabled. Portable source is shared by both hosts; no new patch or snapshot edit,
and effective original patch tail remains54.

## Evidence and pending acceptance

test_guest_float_flags.py compiles actual FunctionTranslator output for19 SSE
and x87 forms.16728 IEEE/random requests include signed zero, finite boundaries,
subnormals, infinities and both signs of quiet/signaling NaNs. It checks all
arithmetic flags/knowledge, full SIMD payload preservation, floating condition
word, logical pop count and unchanged shadow stack bytes. Three compiled broken
publishers must fail. A real ML64 masked-exception oracle compares raw arithmetic
EFLAGS plus floating condition bits and normalized top index. This is host x86
evidence, not physical Xbox or full floating exception equivalence.

test_guest_string_flags.py compiles144 production forms across five operations,
three widths, repeats, DF directions,16-bit address wrapping and FS source base.
70120 portable requests check registers, count, flags/knowledge, all writable
windows and preservation elsewhere.17522 flat native requests independently
compare real x86 memory/flags/DF movement;16-bit guest wrapping and FS cases use
the portable model, not a fabricated host-segment oracle. Three compiled wrong
producers must fail. Invalid-memory count0 cases and branches after repeat
comparisons/moves and overwritten SIMD inputs execute the real production code.
The initial combined legacy/integration suite passed81 checks in13.96s.

Windows Story acceptance passes below; new-head CI remains pending. PUSHFD/POPFD/control state, saved CPU context and real
exception/unwind remain unfinished, as do other kernel/graphics/audio/input/
title/header/fixture replacement and final snapshot-unavailable production audit.

The expanded final suite passed307 checks in78.14s. Fresh/repeated installed-
stack and repository context checks passed21 in25.91s. No historical patch bytes
changed from the preceding fully green both-host upgrade/transition CI. New
production generation completed15838/15863 functions,0 failures,6,575,684 lines
in94.25s lift. Release and built-in checks pass. The current-generation archive,
canonical header/template and sampled producer audit pass:12046 floating,166
string comparisons and2395 repeated noncompare bodies. Executable SHA256:
cda96a1a09339b59fa0f228ef1466cd05813aba6e1f8668214bde1ce011f9ed9.
Windows runner4180/controller23703 both exited0: scene4/mission1, movement
11.1745492885/back6.1848918370, ammo8to6, pause39 frozen/resume58 advancing,
last115.162s under240. Three fresh captures reviewed, build input/run hashes
match,26 protected files unchanged, no lock/dump/partition images. Original60,
minimized synthetic input/movies skipped; no campaign/physical-input/listening/
native Mac title claim. Another checkout launched after our guarded startup;
functional checks pass without an exclusive performance claim. Prior operator-
missed deadline and guard-blocked retries are retained and not accepted.
Evidence logs/comparison-flags-build.json, game-check.json, final game/controller
logs and captures. New-head CI pending.

Stage0251 committed/pushed asf49e53b8. Exact-head runtime41463409584 and
launcher41463409585 both succeeded, including both-host native, upgrade and
transition jobs. This does not establish native Mac title gameplay.
