# Unified Controls options

Keyboard/mouse prompts use shared outlined keycaps. Additive2340 scopes the
translated40FA0 draw and wraps its native40260 glyph callback, also routing
alternate402D0 callbacks through the shared menu lookup. The renderer replaces
recognized key-label brackets with rounded borders at actual glyph
positions; native text measurement/advances, font, grey tint, fades, clipping and
HUD placement remain authoritative. Controller glyphs and ordinary bracketed
text are retained. Mouse/wheel names use softer corners. Key vocabulary caches
all256 localized names, plus composite controls, without caching assignments.
Left/Right labels compact to visual arrow markers during both measurement and
drawing; the renderer paints directional arrows inside their keycaps.
Borders use solid-rectangle emitter1A2E0 with float coordinates, below1EFB0's
world-view clamp: pause footer text extends below that world rectangle. Native
host viewport/scissor still clips these quads just like the font glyphs.
Keyboard binding cells and assignment/conflict rows opt into the same renderer.
`tests/input/keycaps.c` exercises actual callback geometry/colour, mouse labels,
controller fallback and nested draw restoration; pc_prompts tests cover vocabulary.
The supported build refreshes generated code for the new native glyph wrapper.
Historical controls-only fixtures predate native-prompts/display wrappers;
2340 is installed during their complete migration, with both keycap hooks
asserted afterwards, rather than applied to that deliberately older subset.

`pc_settings_navigation.inc` shares section input, sounds and footer hints.
Physical Left/Right arrows switch sections from any root setting; native A/D
adjustment and header navigation remain. Controller Black/White C4/C8 masks
0x200/0x400 correspond to LB/RB through the common adapter; held shortcuts
latch across rebuilds and require release. Capture/list/modal pages are excluded.
The second footer line captures the native Select/Back glyph scales and final
colour before hover tinting, matching its size, gray tint and fade. It inherits
the footer font and switches
between keyboard and controller hints without
changing native Select/Back prompts. The launcher explains the same shortcuts.
Hover entry uses native sound-table slot0, actual section changes slot8, through
21780 with the native enable flag and table bounds. Repeated hover, held buttons,
same-page clicks, muted/missing sound tables do not produce extra feedback.
`TSFP_SETTINGS_INPUT_TRACE` records native C4/C8 masks only for diagnosis.

Visual refinement: the shared tab painter measures a common longest caption
(`KEYBOARD & MOUSE`) in both Video and Controls, preventing shorter Video labels
from growing. Owned setting-row focus uses a slightly brighter blue tint; header
backdrops remain suppressed. Short root sections distribute native leading among
their real rows rather than inserting blank footer spacers. General uses1.2,
Controller3.6, keyboard home1.44 and mouse3.34; footer actions restore1.2.
Video roots also retain their first finalized native Y field64 across rebuilds,
reset on activation and excluded from confirmation/restart prompts. Empty status
notes no longer reserve a row. These are shared presentation changes only.

`src/interface/pc_controls_menu.inc` owns the original Controller descriptor
US524A40, opened through native frontend2CBE50/pause2EC1A0 initialization.
Settings and pause now expose one Controls entry. Root tabs are General Controls,
Controller and Keyboard & Mouse, using the same measured tab painter as Video
(`pc_settings_tabs.inc`), 78% width, compact65% fonts and native silver dividers.
Video/Controls sections use flat uppercase labels: the current page stays blue,
and one unbordered, rounded native-colour highlight follows pointer or header focus.
Inactive labels have no background, gradient, bevel or edge strips. Keyboard labels use uppercase display copies
only; stored key names, assignments and persistence remain unchanged.
Section captions select the first visible setting row's native font (`row+1c`,
setter3FA10) before measuring and drawing, so frontend headers use the same game
font as pause headers. Flush batches and restore the enclosing font afterwards.

General has native IDs1 Crosshair,2 Auto Aim,4 Auto Lookahead,5 Aim Mode,
6 Crouch and7 Weapon Change. Controller has0 Control Layout,3 Inverse Look,
8 Vibration and9 Turn Speed. Both retain native10 Accept/11 Cancel and the
native profile draft, selected player, Customize and Back behavior.
Accept is disabled until the native draft differs from its session baseline;
restoring the original values disables it again. The baseline is31 dwords at
7BA364 + selected-player *1E0, matching the native open/copy contract. Capture it
once after initial construction, retain across tabs and Customize, reset on open.
Tick refreshes Accept's disabled flag; its callback also rejects clean sessions.
The Controller footer follows Turn Speed directly, without padding rows.
Tabbed roots retain their first section's top anchor through native Y offset
field64 (setter77320), retaining its first finalized value without adding rows above Accept.
Modal and binding-list pages retain native centring; new sessions reset anchoring.
Write the finalized Y field directly; calling77320 after771D0 remeasures pending
builder rows and changes the layout a second time.
Custom keyboard glyphs/column headings/names fit their measured cells. Draw plain
captions under explicit glyph scale, then flush/restore it: inline size escapes
would override the fit. This also keeps diagram letters inside their key boxes.
The shared native focus painter suppresses its full-row backdrop for the owned
Controls header130; the shared section painter owns its single highlight.

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
capture/modal pages. Back retains the existing keyboard page hierarchy through
native keyboard/controller Back; unified pages omit their on-screen Back rows.
Binding-list Previous/Next page actions follow the footer divider; groups with
no pagination omit that otherwise-empty divider.
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

Local player device work in progress: General Controls exposes Local Multiplayer
through the existing keyboard-page callback dispatcher. Four device rows edit a
draft; Apply validates exclusive assignments and persists them through
pc_settings_players_save. A button-driven identification flow reads raw pad
buttons, requires release, blocks normal menu actions during capture and times
out after twenty seconds. Keyboard/mouse remains Player 1 only. Main-menu and
pause use the same page; live presentation and split-screen gameplay still need
acceptance checks. Back leaves unapplied assignments unchanged.

Local player assignment uses pc_settings_players_read/save in pc_bindings.inc.
Keyboard/mouse ownership remains local slot zero and only a keyboard profile
receives raw camera motion. Controller profiles retain stored keyboard bindings
for switching back but do not execute them. Patch2350 excludes those key sources
and reserves disconnected SDL seats by device path for the current session.
Missing/changed paths do not silently reclaim a reserved seat; startup slot
identity across reordered enumeration still needs follow-up validation.

Local-player device assignment: controller_profile fixtures now simulate two
independent pads and actual SDL scan reconnect slot reservation; launcher_settings
covers assignment round trips and atomic duplicate rejection. Patch2350 follows
the late reload/radial sampler in apply-patches.py. Its policy must remain shared
between Windows and SDL. Live split-screen and physical hotplug checks are still
required in addition to those fixtures.

pc_settings native fixture also exercises live four-port publication, retained
bindings/unknown JSON, duplicate rejection and read-only save rollback. The FOV
menu fixture covers native player selectors, explicit Apply and identification
cancel. Both mouse adapter bodies test that controller ownership drains mouse
deltas without moving the camera. All remain in existing CI test entry points.
Patch2350 also distinguishes explicit device choices from implicit defaults
when profiles are reordered: parse_controller must never replace an explicit
pad index with the guest port index. Controller fixtures cover both array orders.
Identification tests include held-button rejection, already-owned pads, timeout
and draft-only capture until Apply.
The headless local_player_routing fixture compiles the actual sampler under both
host branches with explicit focus/capture inputs. It verifies assigned-pad
reservation, free-pad fallback, disconnect and focus gating; Windows also mocks
physical key polling to check independent keyboard movement/fire. Removing the
reservation guard must fail at the P1-neutral assertion. These are sampler tests,
not live OS event delivery or proof of a rendered two-player match.

Start-to-join uses shared pc_local_join.h and polls only the opening profile screen. Raw Start edges assign free seats in press order; keyboard interaction reserves P1. Seats are session-only; manual Apply disables auto joining. Focus loss re-arms edges. test_controller_profile.py covers ordering, held buttons, capacity and existing owners.
