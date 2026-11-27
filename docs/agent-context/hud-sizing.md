# Crosshair sizing

Local feature branch feature/hud-crosshair-size, base1375e144. HUD Scale was
removed from the game, launcher and rendering pipeline at the user's request.
Original automatic HUD aspect correction remains unchanged.

Crosshair Size is25-150% in5% steps, default100%. Main/pause Video Options >
View & HUD stages ID15 through FOV_DRAW_CB/MENU_CROSSHAIR_SIZE. Apply persists,
Back discards, and save failure restores the previous value. No restart.
Body leading0.9 fits the shared pause-menu top inset without font changes.

pc_display.c reads V1-V3 as100%. V4 fields remain width height worldFov
weaponPercent legacyHud crosshair; valid old preview HUD values are ignored,
and saves normalize that compatibility slot to100. Crosshair100 saves the
existing V1/V2/V3 format. Malformed or trailing fields reject the entire record.
No HUD size state, rendering APIs, widget anchor helper or patch2732 remains.

pc_display_ui.inc/pc_ui_layout.h scale audited common/alternate/turret reticle
primitive bounds about their midpoint after existing aspect fitting. Projected
aim position, rotating zoom borders and scope masks retain existing behavior.
100% is an exact no-op. Existing single-local-view guard remains.

Shared launcher GameSettings/SettingsPage expose one percentage slider in
View & HUD, with draft dirty tracking, Save/Cancel and atomic transactions.
Tests cover legacy preview migration, bounds, persistence, slider dirty/Cancel,
absence of the removed HUD slider, and reticle midpoint sizing. Windows rebuild
and live main/pause acceptance are in progress; Mac/Linux visual checks pending.

References: [video options](video-options.md), [pause menus](pause-settings.md).

Weapon degree changes use V5 without the removed HUD slot; see
[weapon aperture](video-options.md). Crosshair state is preserved in both formats.

Launcher crosshair controls require installed BUILD.json display_settings_version
>=4; new degree records require>=5. Native package capability follows build
receipts; older binaries never advertise it just from the packaging revision.

Isolated prompt rendering probes may set TSFP_TEST_PROMPT_DEVICE=controller or
keyboard only with TSFP_PC_INPUT_STATE or TSFP_NATIVE_TEST_KEYS. This selects
glyph rendering without changing bindings or normal last-device state; ordinary
launches ignore it. pc_prompts.c retains controller-diagram override precedence.

## World-light coronas

World-light coronas (Discord mansion rectangle report, November26) use the
existing `pc_display_aim_primitive` float-rectangle hook. Exact US callers17941
and179BC in17810 select full-view placement before each coloured/neutral layer,
then flush and restore the enclosing scope. These are projected world glows;
near lights can exceed the canvas and must not inherit the centered HUD viewport.
Keep original size, UVs, colour and intensity; this scope does not normalize GPU
visibility samples or alter light attenuation. Original4:3 and split views retain
the existing path. See [query scopes](fidelity-comparison.md#guest-draw-query-scopes-mansion-lighting-november26)
for the separate visibility contamination repair and STATUS for live acceptance.

`test_ui_layout.py` includes `corona_overlay.c`: both original corona call sites,
ordinary/oversized bounds, aspect ratios, guest ABI/UV/colour preservation,
deferred flush/restoration and unrelated/split-view guards. Removal controls
reject old placement, missing flush and missing restoration. These CPU fixtures
verify the placement boundary; real mission captures establish visible coverage.
