# Modern control presets

`pc-controls.json` retains its complete custom `bindings` objects. Per-controller
`keyboard_preset` and `controller_preset` use absent/0 for Custom and1 for
Modern PC/Modern Controller. New configurations seed explicit1 from
config/pc-controls.default.json: keyboard/P1 controller plus the three other
controller ports. Existing files retain their selected presets; files predating
presets retain Custom without inferred upgrades or writes on load/open.
The updater keeps existing Settings files, and GameSettings uses the embedded
fresh defaults only when the controls file is missing. This distinguishes new
setups from existing users even when their old bindings match factory values.
Selecting Apply/launcher Save changes preset metadata; choosing Custom restores
the same backing bindings, including legacy four-source arrays. New configurations seed the Modern key table into their editable Custom bindings,
with custom_binding_semantics=1 for new-profile initialization. Keyboard gameplay
routing and prompts are semantic for all keyboard profiles, including existing
files without that marker; retained keys/presets/source arrays are not rewritten.
Existing native saved controller layouts remain unchanged.

`src/input/pc_control_presets.def` supplies the six Modern PC overrides to C and
an embedded launcher resource: E use without Space, F/Mouse4 melee, B/PageUp mode,
V/PageDown grenade type, Q/WheelUp previous and Right/WheelDown next weapon.
Other gameplay keys use factory defaults. Optional keyboard look and developer
shortcuts remain the user's assignments. Mouse options are independent.
Modern PC menus use Enter/Space confirm and Escape/Backspace back, retaining
arrows/WASD navigation, physical Left/Right tabs and A/D value adjustment.

`pc_bindings.inc` keeps custom state in g_ctl. The sampler takes a private snapshot
for Modern PC, accumulates keyboard and pad sources separately, then routes only
keyboard gameplay sources through native semantic masks. Patch2940 follows2630 and
all late reload/local-player/reconnect patches; apply-patches.py records this
ordering. Legacy/custom paths retain their magnitudes, per-port ownership,
focus/modal/capture guards and keyboard reload buffering. Preset activation
never replaces pad custom sources; Modern Controller samples its own physical
pad defaults while active and restores the old custom routing when disabled.

`pc_control_presets.h` maps Modern Controller LT aim, RT fire, RB grenade,
RS click melee, B/LS click crouch, X reload, A use, Y next weapon, LB mode;
D-pad weapon/scope controls and sticks remain available. Menus/editor input
uses original controller mappings, including A/B and LB/RB tab switching.
Shared controller glyph conversion reflects the changed gameplay roles.

The title's US18FFF0/190010 mask tables are at47DF00/47DF40. Native CF2D0 copies
22 control words from profile+4 to player+18. The guest reader in
pc_display_menu.c registers from pc_display_camera, resolves context+4 local input ownership
through6B93C0, and reads the live player's masks (base7B0C48, stride1584).
Native semantic fields are Fire18, Grenade1C, Use20, Reload24, Aim28,
Crouch2C, Zoom30/34, weapon cycle38/3C, weapon mode40/44, grenade type48,
and Melee4C. Native1C2F33 requests melee through4C/1C0FA0, 1C2F68 reads
Reload24, and D4D7C reads Reload24 to leave a fixed weapon. The live Xbox
manual-layout Test Lab confirms these fields independently of raw pad buttons.
Keyboard/controller prompt remapping first resolves the native physical glyph
through these masks, then selects the Modern action's key/icon; custom layouts
therefore do not display the wrong button. It does not rewrite native control profiles. Frontend, pause, menus and native
demo playback exclude gameplay routing. Fixed weapon state80 routes Use through
native Reload/leave-mount. Existing native layout choices and their unbound
native actions remain authoritative; presets do not manufacture native actions.

Controller row251 and keyboard Home row212 are native cycling choice rows,
built with the same7D480/717D0/75430 helpers as Video's Aspect Ratio. There is no
separate preset page. Native arrows and A/D or controller value input own cycling.
Dedicated keyboard/controller draft choices remain independent of applied settings.
Controller Accept commits its preset then retains the native profile Accept flow;
keyboard Home's footer Apply preset commits its draft without leaving the tab.
Both remain disabled when clean, and failed writes retain the active preset.
Tab changes collect the current choice before rebuilding and preserve its draft;
mouse/binding autosaves do not accidentally publish pending preset selections.
Keyboard binding editing is available under Custom; Modern shows preset values.
The F2 Win32/AppKit/GTK adapters likewise show Modern bindings read-only while
retaining mouse editing and explicit default reset. GTK carries the preset field
through its save draft; AppKit carries it separately from the original snapshot.
KB row IDs are not text-buffer indexes: extended IDs reuse bounded text slot30.

The shared launcher SettingsPage exposes Keyboard and four per-player Controller
choices, previews Modern keys, disables their binding editor and uses the existing
all-settings transaction. GameSettings keeps the full custom source arrays on
unrelated Save. The def resource is included by Windows and both native projects; it also participates in the Windows launcher build/test fingerprint.

Validation runs existing registered suites: controller_profile (both sampler host
adapters, local ownership, custom/unbound restoration, Modern roles/menu isolation),
keyboard_menu, weapon_slots, menu_mouse, fov_settings (guest role/port guards and
menu callbacks), menu_text, ui_layout and quit_menu. Windows Build's pc_settings
fixture checks overlay round trips and read-only-file failures. Shared launcher UI
checks all four slots, legacy four-source preservation and Apply/Save behavior.
See STATUS for actual host builds, live game checks and remaining physical-device
acceptance. No additional platform-specific preset policy is needed.

Inline preset labels, choice values and footer actions use the existing Controls
body scale in frontend and pause skins. Native disabled flag1 keeps Apply at the
same font size while clean. Controller ownership is captured in the first builder
after native open selects its profile. Non-menu editors keep custom keys/prompts.

Fresh/default migration checks: shared launcher UI exercises new model load without
writes, fresh Core.Apply seeding, and updates over markerless, explicit Custom and
mixed existing presets. Update preserves exact file bytes; unrelated settings Save
retains presets, four-source bindings, explicit Unbound, devices and unknown fields.
Windows Build additionally loads fresh1 and legacy0 configs through the real
pc_settings API in separate processes, asserting no disk write. Existing editable
binding UI cases explicitly choose Custom. pc_settings_game uses the legacy fixture
for its capture workflow; fresh Modern read-only behavior is separate.

New-player editable Custom seed: factory JSON carries Modern PC keys plus
custom_binding_semantics=1. This marker authorizes the known-new native profile
seed/targeted seed repair; it no longer gates gameplay keyboard routing or prompts.
It is never inferred or added to an existing configuration. Menus retain their
existing routing; all keyboard gameplay uses named actions without a Modern overlay. Modern/Custom switching only changes preset metadata.
The existing 69D90 profile-init hook (pc_unlock_profile_init) calls
controls_new_profile only for the same known-new identity.device==-1 boundary
that initializes controls/progress. With the new-setup marker, it selects native
editable layout0 and seeds the named actions in its first14 words using the inverse
shared Modern Controller table, retaining one physical button per native action
(the lowest set bit). Native Customize cannot represent combined masks; Modern
sampling still routes its additional sources through each action. The unused
action word stays unchanged. New Custom profiles also seed the8
axis words to native Default selectors2,1,4,3 (and their runtime copies).
Leaving the backing axis words empty while selecting layout0 disabled movement.
Existing profiles repair only when their layout0, complete Modern action seed and
eight zero or native Default axis words match the old combined-mask seed exactly.
Repair seeds empty axes and canonicalizes those button masks; other configured
axis sets and layouts/actions remain untouched. Repair is in memory during profile
initialization and persists through the normal native profile save flow.
Loaded/copied profiles bypass the seed, including existing saves used with a new
host config. Controller Customize and native Accept/save keep their original flow.
Tests cover new Custom key parity and edits across preset round trips, old update
byte preservation, both sampler adapter modes with an edited semantic Custom key,
and new/existing native profile guards, physical masks, seeded axes and repair guards.

Historical upgrade fixtures exclude patch2940 alongside its absent Tools/input
dependencies. Each complete-stack upgrade still installs2940; the previous-controls
fixture verifies Modern pad fallback and semantic accumulation after migration.

The extracted cinematic-camera fixture stubs input-reader registration (like its
existing sidebar registration boundary); full FOV/input fixtures test routing.

Modern active saves preserve editable optional-look and developer shortcut actions.
pc_settings_action_editable is the shared predicate. pc_settings_save replaces
only changed editable action members, retaining unrelated raw/legacy source arrays;
active bindings update only after a successful write. Preset activation keeps the
retained shortcut keys. The developer sidebar draws gameplay chips muted without
capture targets and guards stale/keyboard activation; shortcut chips remain editable.
pc_settings_assign rejects moves from fixed preset actions. Windows built-in
pc_settings tests cover shortcut persistence/live lookup, immutable preset keys,
conflict rejection and legacy multi-source retention; developer_tools covers chips.

Network/split-screen ownership: context+0 is the global slot, context+4 the local
input owner (UINT32_MAX for remote). controls_gameplay_masks validates +4 before
mapping through6B93C0, matching story_cine_local_context. The FOV fixture uses a
remote host(global0/local-1) and guest(global1/local0) with distinct fire/reload/aim
masks, plus remapped/split local ports, invalid owners and mounted-state isolation.

controls_context_port centralizes the +4 owner/6B93C0 map for gameplay masks and
active-context controller glyphs (7B0C7C), with invalid/remote owners returning4.
The guest/split fixture verifies both readers share the same local port policy.

Controller diagram glyphs are physical even when opened with KBM; the scoped
prompt override does not change gameplay bindings or the remembered device.

Keyboard axis independence: controls_gameplay_masks reads native per-player
Move/Strafe/Look selectors at +50/+54/+58/+5c. pc_preset_axis_masks exports their
physical directions separately from button masks. Semantic Custom/Modern PC
sources route movement and optional look keys through those selectors before
combining with pad input. Swapping controller Move/Look therefore preserves the
keyboard action labels and behavior; Custom physical pad sources are unchanged.
Mouse look already writes camera angles directly, independently of stick layout.
Sampler tests cover swapped sticks, optional keyboard look, edited Forward and
unchanged Custom pad input on both host adapters; FOV fixture covers live selectors.

Patch2960 extends axis independence to markerless legacy Custom keyboard files.
Reading live gameplay selectors no longer depends on preset/semantic metadata;
only keyboard axis sources originally used this unconditional route. Patch2965
extends it to all named keyboard actions; pad sources and menu input remain separate.
The regression runs swapped Run/Look, W/S, optional look, edited Forward and
menu fallback with custom_binding_semantics both absent/zero and enabled.

Keyboard/controller independence (2965): keyboard Fire/Use/Reload/Aim/Melee/
Crouch/grenade/weapon actions always follow their named roles, including legacy
Custom files without preset metadata. Physical Custom pad sources still follow
the controller layout. Old y/black/white JSON entries retain their keys as extra
Next weapon/Weapon mode/Throw grenade bindings, using the shared Modern action
table; labels in native menus, F2 and the launcher name those fixed roles.
Gameplay prompt remapping uses the same roles for legacy and new configurations.
Menus, editor input, weapon slots and developer shortcuts retain their own routes.

Keyboard editing never replaces non-key sources on the edited action. Runtime
MAX_SOURCES is6: two keyboard slots plus four retained legacy controller sources.
pc_binding_keyboard_edit preserves original pad tokens, prepares the next binding,
and publishes only after atomic save succeeds. Missing actions retain inherited
pad defaults from the shared PC_PAD_SOURCE table in pc_control_presets.def;
the registered parser contract compares every entry with the pinned runtime.
Shared launcher Binding exposes keyboard/none entries only;
SetKeyboardBinding retains pad tokens across assignments, conflict moves and
keyboard reset, which also leaves the selected device/controller presets intact.
This six-source combined format requires the updated runtime when all slots are used.

The sampler fixture cycles all11 native button roles through every button target
for14 keyboard sources, including aliases, verifies key edits leave the action
unchanged, and compares same-player physical pad output before/after key edits.
Native settings built-ins and shared launcher UI save/unbind/reset keys alongside
four pad sources. Historical partial-stack fixtures defer2965 with2960/2950/2940.

Active-player pad sharing (2970): saved device preferences reserve a physical pad
only while that logical port belongs to a joined local player. P1 keyboard/mouse
can borrow an unjoined P2 controller, including one connected after startup;
joining P2 reserves it again and leaving returns sharing. No preferences change.
pc_settings_active_ports snapshots its reader under the binding lock and invokes
it after unlocking; before guest registration it retains auto-port fallback.
controls_active_ports reads local count5236c4, active contexts790950/7b0c48 and
local-owner mapping6b93c0; frontend ownership74e914 excludes waiting state1 at
74f9ec. Remote contexts do not reserve pads. Both Windows and SDL sampler paths
use this ownership mask and retain focus/modal/free-camera input gates.

pc_prompts_controller_port caches activity independently for each port. Only an
active controller's disconnect restores keyboard glyphs; idle polls from other
ports do not. P1 activity selects gameplay prompts; joined local players can also
select controller prompts in menus without changing P1 gameplay HUD ownership.
Keyboard/mouse activity restores keyboard prompts; unchanged held pad input does
not steal them back. The original single-port prompt API wraps port0.

The routing fixture embeds production prompt code and checks late connection,
repeated disconnect/reconnect, keyboard/controller switching, menu glyph edges,
joined P2 reservation (including stale auto flags), leaving and unchanged settings.
The real SDL lifecycle test starts polling before virtual pads attach and permits
existing physical devices; it checks six reconnect cycles with another pad held.
Native keyboard-presence checks permit physical controllers to coexist. Virtual
SDL events and host-adapter fixtures do not establish physical gameplay acceptance.
Legacy keyboard isolation follow-up: patch2960 removes the new-profile marker gate
from gameplay routing. pc_settings_prompt_keys uses keyboard-device ownership for
semantic label lookup too. Markerless Custom keys therefore follow Move/Strafe/Look,
Fire/Reload/etc. after native controller remaps, just like new profiles. Explicit
key edits, presets and raw JSON bytes remain preserved; physical Custom pad sources
still follow their own configured/native layout. The production sampler fixture
injects OS key states on every host and checks markerless remaps on both adapter
modes, plus a negative control reinstating the old gate. Profile seed policy is
unchanged. This does not supply keyboard actions for native actions left unbound.

Release integration: main's2960-keyboard-gameplay-independence supersedes the
local axis-only2960 patch.2965 now follows that semantic route, adding fixed
legacy aliases and six-source preservation.2970 follows2955 and replaces all
per-adapter prompt notifications with the common per-port path; explicit P1
controller prompt behavior remains covered using production prompt code. The
fixture retains main's cross-host synthetic key evaluator and old-gate negative
control. No upstream input repair is dropped.

Cross-host integration checks use the runtime platform SRWLOCK declaration in the
production-prompt fixture; redeclaring it under non-Windows breaks Clang. Native
launcher JSON keeps '+' literal so preserved pad:ry+/rx+ sources remain readable
by the runtime parser. The shared UI inheritance regression covers this contract.
