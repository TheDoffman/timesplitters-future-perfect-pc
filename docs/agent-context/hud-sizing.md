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
