# Controls without presets

## Current keyboard/controller separation

Follow-up2995 runs after2990 and restores only device-independent keyboard
action routing. No preset UI, Modern defaults, profile seeding or saved-layout
rewrites return. `pc_keyboard_actions.h` translates keyboard actions through the
active local player masks/axis selectors supplied by `controls_gameplay_masks`
and `pc_settings_gameplay_reader`. Physical controller contributions stay on
the original path; menu input remains physical. Gameplay prompts resolve the
configured action back to its keyboard key.

Keyboard saves/launcher edits replace only key sources, retaining explicit
controller sources on the same action. Internal source capacity is six (two
keyboard slots plus up to four retained pad sources); the editor still exposes
only two key slots. Old preset metadata is ignored. Factory defaults and native
controller profile initialization remain those restored by2990.

Production sampler tests swap Move/Look, remap each named button action and
change keyboard keys, then verify controller input and stored mappings remain
unchanged. A shared-layout negative control must fail. The same fixture runs
Windows and SDL defines. Shared C/managed save tests retain explicit pad sources.

### Seated exit actions

The gameplay reader returns the actual player control mode through its context
output. `pc_keyboard_context_masks` routes Interact through Reload in stationary
gun modes40/80 (original B6A67/258BE2), and through fixed Y in vehicle mode400
(original D2557 produces exit packet40). Transition mode20 and ordinary on-foot
Use retain their native masks. Vehicle-only Y alias routing keeps a configured
legacy Y key working. Vehicle button actions use the original fixed D2470
protocol, so on-foot controller remaps cannot turn Fire/Next weapon into exit. Gameplay prompts
use the same contextual mapping; controller input and stored profiles are unchanged.
`PcKeyboardContextState` waits for release when a held Interact/Y key crosses a
control-mode boundary, preventing a virtual-button change from manufacturing an
exit or immediate re-entry. This state is transient and never saved.

The real sampler and guest reader check2/20/40/80/400, remapped Reload/Use,
legacy Y, Next weapon and unchanged profiles. An old-context mutation must fail.
The supported-XBE vehicle fixture executes D2470 for all five seat types at
60/120/180/240, observing exit packets at17DA00; removing context routing loses
the request. Camera/math/vehicle completion services are controlled boundaries,
so this proves an exit request, not a rendered dismount in every mission.

## Full rollback in release0.6.2

The user requested a full rollback of the Modern preset feature, not merely
removal of its selectors. Reference: c511f1e9, immediately before2a7d92f4/PR528.
Patch2990 follows2970 and restores original direct keyboard/pad accumulation.
Historical2940/2960/2965 remain only as patch-stack migration inputs; their
sampler overlays and semantic accumulation are removed by2990. No factory-mode
metadata is read by production input, and the preset helper header/resource is
removed. MAX_SOURCES was restored to four by2990;2995 now retains mixed-source edits as described above.

Factory config, PcSettings shape/read/defaults/save/assignment, keyboard menu,
keycaps, profile-init extension and all three native binding editors are restored
to the reference. Modern/Custom selectors and Apply preset are removed from the
game and shared launcher. The old defaults include E/Space Use, F/middle-mouse
Melee, arrows/Page Up/Page Down scope and weapon controls, and the original wheel
direction. New profiles use the title's original built-in layout initialization;
there is no Custom-profile button/axis seed or automatic seed repair.

Keyboard/controller remapping again uses the title's original fixed virtual-pad
roles. The later semantic keyboard/axis isolation and legacy action aliases are
also reverted. That rollback caused the release0.6.2 shared-layout regression; the current
2995 follow-up restores keyboard separation without presets. The game's original Controller Layout and
Customize choices remain. Restore defaults in keyboard settings/launcher uses
the pre-feature defaults. Existing settings and saves are not replaced/reset by
this change; obsolete fields in existing JSON are simply ignored. A stored custom
layout remains custom, including layouts saved while the preset feature existed.
Players can explicitly choose Default/Restore defaults rather than have personal
configuration silently overwritten. Runtime rejects unsupported source counts
without rewriting their files, as it did before the feature.

Unrelated newer features are retained: movement/aim deadzone settings (2950),
explicit/native prompt correction (2955), joined-player pad sharing/per-port
activity and reconnect ownership (2970), persistent texture preference and the
main-menu update/Online-opening notices. controls_active_ports and the small
pc_controls_in_menu boundary still support ownership/prompt handling, not semantic
keyboard translation. Shared GameSettings retains its texture/update code and
per-player deadzones while its binding implementation is back to the reference.

The real sampler fixture retains local-player ownership/hotplug/focus/deadzone
checks on both host defines. Factory-default and retired-metadata tests verify
old keys and that an explicitly unbound/custom pad is not overridden. The FOV
fixture retains Online/texture/update/deadzone checks, with preset-only stubs and
assertions removed. Windows Build runs the original binding-editor contract.
Full launcher tests cover the restored editable settings UI and native storage
policies. Fresh/repeat/previous-controls patch-upgrade tests cover2990; historical
partial-stack fixtures exclude it along with the preset patches they predate.
See STATUS for measured checks; native Mac and physical Linux/Deck remain distinct
acceptance targets. Personal files are never test fixtures.

Latest main integration retains the launcher Controls tab and collapsible
per-player deadzone sliders. Only preset selectors/model resources are removed;
slider drafts, readouts and persistence keep their existing shared UI coverage.
