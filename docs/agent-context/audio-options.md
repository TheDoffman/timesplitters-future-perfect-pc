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

Mix now uses six native custom slider rows (AUDIO_MIX_DRAW), not choice lists.
Each label retains its exact percentage; native A/D/controller directional edges
change one percent. Physical Left/Right and LB/RB still change tabs. Six distinct
MENU_AUDIO_MIX slots feed the common drag ownership path; pointer values clamp
to0..100 and paint/capture use the same100x8 value-column bar. Existing atomic
sidecar saving and failed-save rollback run through audio_mix_change, which also
restores the caption after a failed save. Volume draft behavior stays separate.

`pc_settings_spacing.inc` scopes a modest extra native separator height through
style+30 for Video/Controls/Audio construction. Original79710 passes that height
to79680;73110 centres the line within it. The shared skin is restored after
construction. This reserves equal room on both sides of every silver divider,
including native Controller/Audio footers and keyboard pagination, without new
rows, selectable IDs or altered button actions. Tests verify skin restoration,
all six slider categories/endpoints, failed-save caption rollback, keyboard steps,
and shared paint/pointer routing. Host scene evidence is recorded in STATUS.

Vertical row alignment: pc_settings_row_alignment.inc scopes label+40 and choice+E0
through the shared76810 compact-font paint. The76150 caption callback measures
actual native fonts (3FA10/3FCF0); plain single-line glyph top bearings compensate
for line-box padding. Choice rectangles account for E8 minus68. All offsets and
font state are restored after the complete paint, including the later native
choice/arrows pass. Header/hidden rows and native bounds/hit geometry stay intact.
Audio Volume/Mix and Controller Turn Speed bars use the same row midpoint for
painting and pointer capture. fov_settings verifies measured caption/value centres,
bearing correction, repeated paints and exact restoration; menu_mouse checks
scoped Controller bar centring with original width/height and stack restoration.

Font consistency: settings_row_font uses each owned menu skin's ordinary option
preset (+48), overriding row+1C and choice+BC only for the complete paint scope;
original fonts/flags restore alongside the alignment offsets. Captions use the
native uppercase flag consistently, including Audio sidecar labels. Tabs use the
same preset and fit against four equal-width cells plus KEYBOARD & MOUSE on every
screen; three-tab Video/Controls cannot grow larger than four-tab Audio. Actual
hit rectangles and per-tab placement still use their real tab count. The shared
font test verifies normalization and exact restoration, including tab selection.

Native formatted captions (including keyboard scale escapes) retain their original
case flags. Uppercasing the escape command exposes its digits as visible text;
settings_row_plain_caption limits the shared uppercase treatment to plain ASCII
captions. The row/font fixture covers both plain normalization and escape retention.
