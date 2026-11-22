# Control preset rollback

The user requested a full rollback of the Modern preset feature, not merely
removal of its selectors. Reference: c511f1e9, immediately before2a7d92f4/PR528.
Patch2990 follows2970 and restores original direct keyboard/pad accumulation.
Historical2940/2960/2965 remain only as patch-stack migration inputs; their
sampler overlays and semantic accumulation are removed by2990. No factory-mode
metadata is read by production input, and the preset helper header/resource is
removed. MAX_SOURCES is the original four.

Factory config, PcSettings shape/read/defaults/save/assignment, keyboard menu,
keycaps, profile-init extension and all three native binding editors are restored
to the reference. Modern/Custom selectors and Apply preset are removed from the
game and shared launcher. The old defaults include E/Space Use, F/middle-mouse
Melee, arrows/Page Up/Page Down scope and weapon controls, and the original wheel
direction. New profiles use the title's original built-in layout initialization;
there is no Custom-profile button/axis seed or automatic seed repair.

Keyboard/controller remapping again uses the title's original fixed virtual-pad
roles. The later semantic keyboard/axis isolation and legacy action aliases are
also reverted. This deliberately restores the earlier behavior; it is not a new
device-independent binding design. The game's original Controller Layout and
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
