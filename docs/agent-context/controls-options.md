Controller layout diagram: US524A00 draw callback2E97C0 is wrapped by
`controls_diagram_draw`. A thread-local nested prompt scope forces physical
controller labels for its measurements/text/glyph draws, restoring the remembered
input device afterward. Normal menu/KBM prompts still follow last input. Scope
changes display only; native action configuration remains authoritative.

[Preset rollback](control-presets.md).

# Unified Controls options

Imported PNG prompts can replace recognized single-key labels through
`pc_input_icons.inc`, using generated `pc_input_icon_data.inc` lookup/span data.
`tools/graphics/import-input-icons.py` reads an attributed asset manifest, validates
128px source PNGs and virtual-key uniqueness, and preserves the original128px
RGBA pixels as lossless runs. No thumbnail downsampling remains.
Absent icon data or unmapped keys retain the original keycap renderer. Input
lookup uses cached localized key names; held states require both raw and focused
input. Imported images use native textured quads, placement, tint/fade and scissor;
original text advances remain authoritative.
Julio Cacko's CC0 1.4 pack supplies dark keyboard/mouse and default Xbox-style
controller art. The manifest has separate `keys` (VK) and `controller` (native
ASCII token) banks; equal numbers never alias the two namespaces. Compiled images
are shared between banks/states when source paths match. Missing keys/composites
and unsupported controller tokens retain native fallback. There is no new
controller-family detection. Slot0 is generic Mouse, never Unbound; F23/F24
are the binding model's wheel pseudo-keys. The author's duplicate3 filename
ending -1 is its4 key. Source PNGs stay private; --source-dir verifies hashes.
Native40FA0 supplies state+40 icon markers and state+38 zero-based digit/letter
indices. The glyph texture handle at state+3C identifies digit icons through table7E72A0;
atlas letter indices resolve independently, so clipped earlier tokens cannot
shift the selected image. This preserves
measurement, native advances, binding actions and layout. B/A/Y/X, LB/RB, Start,
triggers, sticks and directional tokens use the controller lookup. Unknown/null
icons retain their original callbacks. The settings footer now emits native
Black/White tokens instead of literal [LB]/[RB]. Both hosts use the same wrapper.
Import-time alpha bounds provide a capped face-height scale for wide keyboard
keys such as Space/Shift/Enter; the native label advance bounds their width.
Dark PNG margins require1.15 glyph-height scaling, compact1.0 for the profile
keyboard footer. Native controller bounds use1.15. No separate pressed art is
provided, so idle images are reused. Credits and modifications are in
`assets/ui/input-icons/ATTRIBUTION.md` and NOTICE.
Windows release/playable, native Mac and Linux packagers retain the complete credit
as `Licenses/Keyboard-mouse-icons.txt`.
The Linux payload verifier requires that notice; its shared icon renderer needs
no additional Linux adapter. Linux game visual acceptance is still pending.
The compiled span include ships in source; builds need no artwork download.

Patch2610 connects the shared host DrawPrimitiveUP boundary to
`pc_input_icon_gpu.inc`. A bounded256-entry cache is keyed by source and chosen
physical size (1..512 pixels). Premultiplied area coverage creates an exact-size
bitmap from the128px original; sizes above128 use nearest source pixels, adding
no detail. Each bitmap occupies the upper-left of a single-level power-of-two
Xbox A8R8G8B8 texture, Morton ordered. UV extent excludes padding. Point min/mag
sampling and no mip filter draw one bitmap texel per output pixel. No runtime
bilinear/trilinear softening. Cache exhaustion flushes deferred operations before
releasing their textures. Deferred draws retain their size-specific UV extent.
The title's textured1B7B0 quad supplies native geometry and viewport/scissor.
Its UV range is normalized in a copied vertex buffer; native RGBA7-bit white/
opaque0x7f expands before premultiplication. Original vertices are untouched.
The font batch flush stays inside begin/end. No font size or binding changes.
All writes use recorded host wrappers. Native deferred draws queue the complete
image/vertex operation so state queries/restoration run at execution time.
Texture, pixel shader, blend/alpha test and sampler/stage states are restored;
binding the previous texture precedes restoring COLOROP because SetTexture
can enable it. Failed upload retains the rectangle fallback. A device change
releases cached images; reuse avoids repeat upload. No OS-specific adapter.

`pc_input_icon_layout.h` selects aspect families: narrow/4:3 (1.0),16:10
(1.05),16:9 (1.10),21:9 (1.15) and superwide (1.20). Output height converts
the logical size to four-physical-pixel steps. The final canvas and origin
are pixel-aligned; width never stretches a square/circular input image.
Compact profile footers omit the aspect enlargement and retain their clip.
Binding-value cells start at22 units and grow only within row height minus4
and the measured column width. Space/Shift/Enter retain their alpha-bound aspect
compensation. Captions, key assignments, column metrics, original text advances,
footer/profile sizing and hit targets stay unchanged. This removes the former
extra shrink inherited from the small binding-caption font.
`input_icon_layout.c` verifies aspect families, physical-pixel steps, limits and
compact clipping at different resolutions. `input_icon_gpu.c`, in the existing
text-read-scope test route, verifies native
Morton coverage data, seven-bit tint, size-specific UVs/point sampling, cache/device/failure
paths, native quad geometry and state restoration with immediate/deferred draws.
The prediction-target replay fixture omits the adjacent icon GPU include; it
exercises replay writes with its own host stubs, while input_icon_gpu.c owns
texture upload/lifetime/state contracts. The span-only test configuration
explicitly ignores the GPU-only physical-size argument for strict compilers.
Importer tests retain a single original pixel at127,127. Keycap fixtures exercise
binding-cell enlargement separately from caption/footer sizing.

The profile picker has a fixed two-line text clip. `menu_mouse_profile_text`
scopes compact imported key faces (1.0 instead of1.15 glyph-height multiplier),
keeps Select/Back at native Y and moves Delete only2 layout pixels down. Normal/
hover draws and cached hit regions use the same baselines; the icon scope and
stack argument are restored. Controller text layout remains native; its icon images use the same glyph bounds. Fixtures cover
compact image bounds, shifted hit regions, stack restoration and scope lifetime.
Tests use synthetic image data, covering lossless packing, path bounds, duplicate
keys, pressed/idle selection and original mouse/controller fallback; CI runs both
the importer and shared glyph fixtures on its supported hosts.

Keyboard/mouse prompts use shared outlined keycaps. Additive2340 scopes the
translated40FA0 draw and wraps its native40260 glyph callback, also routing
alternate402D0 callbacks through the shared menu lookup. The renderer replaces
recognized key-label brackets with rounded borders at actual glyph
positions; native text measurement/advances, font, grey tint, fades, clipping and
HUD placement remain authoritative. Unmapped controller glyphs and ordinary bracketed
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
Its Adjust hint uses separate `[A] / [D]` labels so both keys use imported artwork;
native measurement still centers/fits the complete footer in main and pause.
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
Patch2360 also matches serial+GUID or an unambiguous disconnected model when
paths change or are absent. Ambiguous identical devices still need explicit
assignment; startup identity across reordered enumeration remains unverified.

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

Start-to-join is intrinsic to the native profile chooser, not a setting. The
192770 state-1 callback (pc_storage_menu.inc) reads raw physical Start through
pc_settings_players_join, excludes ports already in 74E914, and gives the first
waiting panel its port via native42D00/42870 before state2. 5236C4 limits the
selected player count. pc_local_join.h queues fresh edges, including disabled
ports; connection order never sets guest player order. Session mappings do not
rewrite controls JSON. No-waiting/focus loss clears pending joins. Manual device
settings remain; the former auto_assign_players key is ignored. Controller-model
and actual profile callback regressions cover unconfigured devices, P1 ownership,
repeat presses, reversed connection order and player-count bounds.

Presence detection now calls pc_settings_players_detect before computing the
250ms port mask. This exposes connected pads in unassigned (legacy none) ports
before native player-count selection; joining still uses the native Start order.
Session preferred ports survive manual unassignment, without rewriting JSON.
The UI calls none Automatic, since it no longer excludes connected controllers.
Patch2360 retains the SDL lock through reads so another scan cannot close the
handle mid-sample. Tests cover the real presence function plus changed/absent
paths and repeated SDL virtual-device detach/reattach (controller_hotplug.c).

Automatically detected ports remain provisional until native Start-to-join claims them; they do not reserve the controller away from Player 1 automatic fallback. Explicit device saves clear provisional flags. The production sampler regression checks both provisional sharing and exclusive joined ownership.

Live device Apply checks pc_settings_pad_status connectivity for every explicitly selected controller before publishing or saving. An unavailable selection leaves current input assignments intact and displays an inline connection error, preventing entry into the native reconnect modal merely by choosing an absent pad. pc_settings_pad_status now returns connection status independently of held buttons. Launcher/offline configuration remains possible. The native menu fixture covers rejected initial assignment, rejected replacement preserving two ports, and successful retry after connection. This prevents the absent-target case; a report involving two connected controllers still needs live reproduction.

Joined-player settings follow the native player-to-port mapping at 6B93C0,
limited by 5236C4, rather than assuming player number equals input port. The
Local Multiplayer page snapshots a complete port permutation: joined players
first, then unused ports. kb_players_apply translates choices back into those
same ports without changing native ownership. Automatic on a joined player
resolves to its existing connected controller (or a free connected pad), then
persists that concrete assignment; reopening displays the actual controller.
It must never publish DEV_NONE for a joined player. Unchanged disconnected
assignments on unused rows do not block Apply; newly selected unavailable
inputs and changes stranding joined players are rejected before persistence.
The native menu fixture reproduces P1 keyboard, old P2 pad1/old P3 pad0 with pad0
joining guest P2, Automatic retention, rejected theft, connected swap and reopen.

Frontend Settings input handoff: native Settings descriptor 523910 selects the
profile at 78AD34 (2CBEE0), while 73870 normally polls menu+3C player-mask bits.
menu_settings_input_owner checks that root or its bounded descriptor history
(7BA264) and temporarily accepts both P1 and the selected player while input is gathered. Native
42760 still maps logical players through 6B93C0. Menu+38 and profile/save globals
remain unchanged. Returning to main removes Settings from history, restoring
normal P1 input even if the selected-profile value remains stale. Pause, gameplay
and unrelated frontend menus are excluded. P1 retains mouse, text input, queued tab clicks and keyboard section shortcuts
while a secondary player is selected; that playerâ€™s native controller shoulders
also switch sections. Selection chooses the edited profile, not exclusive control. menu_mouse.c
covers P2/P3/P4, nonidentity controller mapping, submenu history, return to main,
invalid selection and gameplay exclusion. Physical controller acceptance remains
pending despite the automated route checks.

Shared Settings control regression also checks simultaneous P1/selected-player
input masks, P1 mouse clicks while P2 is selected, and P1 keyboard tab navigation
alongside P2 controller shoulders. Outside Settings, native P1-only control remains.

README local split-screen instructions now distinguish Start-order joining,
physical controller numbers versus player rows, shared Settings navigation with
P1 retained, Automatic resolving a joined playerâ€™s device, and Apply validation.
They identify this branchâ€™s behavior as not yet published.

The local-player sampler fixture uses external linkage for its reload stub to
match pc_input.h on Clang as well as MSVC.

#540 native prompt activity: patch2955 extends the native sampler's existing
keyboard-player automatic pad detection to explicit Player1 DEV_PAD assignments.
Only P1 updates the shared prompt state; focus/modal gates are respected. Button,
stick and trigger activity still use pc_prompts.c's existing thresholds and last-
input tracking; controller selection does not require disabling keyboard/mouse.
The headless production sampler fixture fails on the old native path and passes
on Windows and SDL after the patch. Native SDL self-test also exercises an explicit
P1 virtual-pad assignment, physical controller tokens, then restores its isolated
controls configuration. This is synthetic input, not physical Steam Deck acceptance.
