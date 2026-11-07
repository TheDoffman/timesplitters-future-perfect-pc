# Tabbed Audio Options

`pc_audio_tabs.inc` keeps native descriptor US52377C (build2C3E20,
tick2C3720, custom native draw2C4190). The third callback is drawing, not
cleanup; it must remain original. Frontend2CBE10 and pause2EC1F0 still run
the original profile initializer once per open. Tab changes rebuild the same
descriptor, never reopen/reinitialize the profile draft.

Volume retains native IDs0–3, hidden subtitle5, Accept6 and Cancel7. The builder
scope adds header140 before ID0, using78% width, compact automatic rows, the
existing65% settings font, native separators and the common flat tab painter.
Mix has IDs30–35 (six percentages); Enhancements15–19; Listening39/40.
Sidecar setters in pc_audio_menu.inc retain live saving and failure rollback.
Accept/Cancel and native Back keep their profile save/discard behavior across
all tabs. They do not roll back the separately persisted sidecars.

Hidden native records remain allocated. Construction clears their B0 height;
layout/input use filtered flat/sibling links and counts. The32-record bounded
snapshot restores both original link chains for native tick/readers, refresh
cleanup and exit actions. Native tick always needs IDs0/2/5, even off Volume.
Exit actions own the restored full chain; do not relink freed records afterwards.
Opening first measures Mix (the largest page), then rebuilds Volume before any
draw. Its finalized Y64 is retained across tabs, without spacer rows, and reset
on activation. Native profile row alignment stays0x2000 after the custom header.
The header uses a blank native caption; only the common painter draws its labels.
Volume IDs1/3 use AUDIO_VOLUME_DRAW and the existing814B0 settings bar in the
right value column (100x8 logical units). The same bounds feed native slider
mouse capture; original16-step globals, preview and Accept/Cancel are unchanged.

The shared painter/navigation/footer also recognizes header140. Physical arrows
and LB/RB use the existing held-input latch; A/D/native value adjustment is
unchanged. Pointer hit cells use the actual painted rectangle. The latest click
is queued through native transition/Confirm cooldown, expires after1s, and is
flushed after the outer input dispatcher releases row pointers. Same-tab clicks
never cycle. Hover and actual tab changes reuse native sound-table slots0/8.

`test_fov_settings.py` executes the production adapter with native widget doubles:
record preservation/filtering, fixed anchor, native actions, same-tab and rapid
click queue in frontend/pause. `test_audio_output_gain.py` executes exact choice
population, live persistence and failed-save rollback. These are shared policy
checks, not physical controller or native Mac/Linux scene acceptance.
