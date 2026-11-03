# Unified Controls options

`src/interface/pc_controls_menu.inc` owns the original Controller descriptor
US524A40, opened through native frontend2CBE50/pause2EC1A0 initialization.
Settings and pause now expose one Controls entry. Root tabs are General Controls,
Controller and Keyboard & Mouse, using the same measured tab painter as Video
(`pc_settings_tabs.inc`), 78% width, compact65% fonts and native silver dividers.

General has native IDs1 Crosshair,2 Auto Aim,4 Auto Lookahead,5 Aim Mode,
6 Crouch and7 Weapon Change. Controller has0 Control Layout,3 Inverse Look,
8 Vibration and9 Turn Speed. Both retain native10 Accept/11 Cancel and the
native profile draft, selected player, Customize and Back behavior.
The Controller footer follows Turn Speed directly, without padding rows.
The shared native focus painter suppresses its full-row backdrop for the owned
Controls header130; individual tab backgrounds/active state remain visible.

The original2EA3F0 builds and initializes every native record. The button scope
marks the other tab's fields hidden/disabled and clears row+B0 before native
incremental measurement, preventing invisible rows from advancing the cursor.
A bounded48-record table retains the complete list. Layout/input see filtered
flat/sibling links and counts; tick2EAB30, Accept/Cancel, Customize, Back and
rebuild cleanup temporarily receive both original full link chains. Hidden records must
not be freed early or omitted from native2E9BC0/2E9A80 readers.

Keyboard uses the same descriptor but preserves existing capture/conflict/reset,
live preview and250ms debounced autosave rules. Its IDs are offset200 only in
this owned menu, preventing native refresh choice restoration across overlapping
Controller IDs. Tabs appear on keyboard home/mouse pages, not binding lists or
capture/modal pages. Back retains the existing keyboard page hierarchy.
The immutable Controls entry caption uses its own text slot, independent of
modal titles. Deeper native Controller Customize keeps its original layout.

Tabs draw/select use verified unused INT3 slots1205E/1205F in the supported US
XBE. Mouse edges share Video's direct/latest-target queue and deferred flush;
held presses do not repeat, targets expire after1s, and modal/closed menus reject
them. Keyboard/controller left/right cycles the three root tabs.

`tests/graphics/test_fov_settings.py` executes the shared callbacks with native
row fixtures: section membership, hidden layout contribution, complete reader/
action lists, frontend/pause entry, keyboard ID isolation, live mouse preview,
debounce, failed-save rollback and direct queued tab clicks. `test_ui_layout.py`
checks owned Controls font scale/restoration alongside Video. Input fixtures
continue to cover binding/model and pointer behavior. Native Mac uses the same
source; Windows evidence does not establish native Mac visual acceptance.
