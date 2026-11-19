# Compact launcher settings

SettingsPage keeps the existing six categories and shared GameSettings save,
validation, conflict, capability and texture-operation behavior. Changes are
presentation only. The Graphics heading is shorter so all six tabs fit at the
launcher minimum width; settings TabItems use compact local padding.

Shared SettingsTiles measures and arranges cards in two independently flowing
columns at720 logical pixels of content width, otherwise one. The binding card flows beneath Local multiplayer while collapsed, and spans
both columns only when expanded. Cards keep their children mounted, so collapse and
resize preserve edits and keyboard focus; expansion is excluded from Draft().
No host-specific policy or saved layout setting is added. Viewport scrolling and
the fixed Save/Cancel footer remain available when content grows.

SettingsPage.Section builds compact card expanders. Input, deadzones, display,
frame rate, quality, volume, listening, language, camera, HUD and startup start
open. Local multiplayer, key bindings, textures, Enhanced Audio and developer
tools start closed. Player1 deadzones still start open and Players2–4 closed.
Fields use inline labels with smaller local control padding; descriptions are
short, with the full description available on hover. Long performance/listening
help stays in details. Existing control names, values and handlers are retained.

The shared UI suite checks wide/narrow reflow, presentation-only collapse,
pending-value retention and unchanged saving. The supported Windows launcher
suite checks the actual settings and update flows. Visual captures cover all six
tabs at1000x720,1152x772 and1440x900. Native Linux cross-build verifies compilation;
physical Mac/Linux resizing, keyboard and visual acceptance remain pending.
