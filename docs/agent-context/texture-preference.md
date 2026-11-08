Texture style persistence: `src/textures/texture_mode.c` reads/writes
`pc-textures.ini` (`TSFP_TEXTURES_V1 0|1`) beside TSFP_DISPLAY_CONFIG,
falling back to RECOMP_INPUT_CONFIG and then config/. A valid saved choice
wins over launch defaults; absent/invalid files retain environment/pack-preview
fallback. Video Apply saves the texture choice last and rolls back preceding
AA/display/rate/graphics/window changes on failure. F10/menu-bar toggles also
save before changing live state. The low-level set_4k remains transient for
renderer fixtures. No texture assets are modified.

Texture preference regression: `tests/textures/test_texture_preference.py`
compiles production texture_mode.c and checks ON/OFF restart, F10 persistence,
launch-default precedence, input path fallback and failed saves without assets.
Registered for Windows/Linux CI; it also supports native Mac. FOV callback
fixtures check texture-save failure rollback including AA; loader toggle tests
use isolated settings paths. Shared launcher UI tests verify sidecar migration,
game/launcher synchronization and stale-save rejection.
