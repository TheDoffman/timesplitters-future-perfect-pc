# Protected-mode flag stack images

Stage0252 in progress afterf49e53b8. Preceding comparison/string stage has
Windows generation/Release/Story acceptance. Both f49e53b8 workflows passed:
launcher41463409585 and runtime41463409584, including native and patch upgrade/
transition jobs on both hosts. See [comparisons](comparison-flags.md).

## Provider and execution contract

src/runtime/guest_status_flags.h owns stack image/restore helpers. Export merges
the string engine's authoritative g_df, forces bit1, clears reserved bits and
RF/VM, truncates only the16-bit form and leaves value/known/direction unchanged.
Unknown incoming arithmetic bits retain their stored concrete choice; exporting
does not advertise unknown state as known. This is not complete EFLAGS proof.

Restore applies protected-mode privilege restrictions using incoming IOPL,
preserves unwritable fields, clears RF and publishes supplied arithmetic bits.
Only32-bit restore changes AC/ID; onlyCPL0 changes IOPL; IF changes when
CPL<=oldIOPL. VM/VIF/VIP are not loaded. Invalid width/privilege or a current
virtual-8086 state rejects without mutation. The cell's DF is a saved value;
consumers/export use the separate live g_df that CLD/STD already own. Existing
full-cell callback scopes preserve control bits; arithmetic writers preserve them.

translation_lifting.py routes PUSHF/PUSHFD/POPF/POPFD under the production status
feature. Stack access has decoded width, preserves general registers and only
increments after successful restore. The runtime's flat protected-mode guest
usesCPL0; helpers accept explicitCPL0-3 for tests. Disabled legacy fixtures retain
unsupported handling. POPF invalidates old local descriptors while retaining
new persistent status for branches/carry. Unsupported mode uses the diagnostic
ABI, not a fabricated successful context restore.

This does not implement TF/AC fault delivery, IRQ scheduling, STI interrupt
shadow, virtual-8086 execution, resumable memory faults, context restore or
SEH/unwind. Opaque/unmodeled operations still prevent full-context claims.
Architectural references: [Intel basic architecture](https://cdrdv2-public.intel.com/843827/253665-sdm-vol-1-dec-24.pdf)
and [instruction reference](https://cdrdv2-public.intel.com/835781/325462-sdm-vol-1-2abcd-3abcd-4.pdf).

## Tests and pending acceptance

test_guest_stack_flags.py uses an independent per-bit model across allCPL/IOPL
combinations, random valid/invalid images and partial knowledge, unchanged-on-
rejection checks and four compiled wrong providers. flags_stack_x86.c restores
host flags before returning toC and avoids TF/AC/NT side effects.512 actual
Win32 x86 ring3 cases cover all64 arithmetic states, bothDF values, ID toggles,
16/32-bit forms and entire hardware images. IF/IOPL protection, RF/VM/VIF/VIP
and16-bit high preservation are checked. CPL0 uses the manual/model, not native
privileged execution. Hardware tests skip elsewhere; portable checks run both
hosts through runtime CI.

Actual FunctionTranslator output executes round trips, upper-register/whole-RAM
and exact stack-width preservation, carry consumers and overflow after an
intervening clobber. Canonical header/storage/helpers compile; unexpected
unsupported diagnostics abort. Initial combined status/production/new suite:
138 passed25.48s. Expanded regressions:299 passed69.84s; fresh/repeated install
and context:21 passed22.40s. Generation15838/15863 functions,0failures,6575724
lines in96.58s lift; generated bodies have5 saves/5 restores. These are static
counts, not proof the Story test executes them. Release, fresh Story and
new-head CI remain pending. Full context/exception and wider runtime replacement,
including snapshot-unavailable generation/build/package acceptance, remain open.

## Windows production acceptance

Release/built-ins/crash symbols and current generation/source/archive/header/
template audit pass. Executable SHA256:
33f5d2c6344ec6d17e46fd5a215f9b6e9754fa6eb1de5ba44226fbf86d8639d7.
Runner42302/controller44792 both exited0. Forward11.1728401285/back6.1848919536,
ammo8to6; pause39 frozen/resume58 advancing, last113.462s under240. Reviewed
three fresh captures, exact build input/run hashes,26 protected files unchanged,
no lock/dump/partition images. Original60/minimized synthetic/movies skipped;
not campaign/physical input/listening/native Mac title proof. Static generated
site counts do not establish dynamic coverage; helper and production fixtures
provide direct semantics checks. Evidence logs/control-flags-build.json,
game-check.json, final game/controller logs, accepted report/provenance and
captures. New-head CI pending; whole replacement remains unfinished.
