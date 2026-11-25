## Release-readiness wording (2026-11-25)

The handheld Home header and Deck help explicitly say SteamOS support is experimental, as does the packaged player guide. Physical acceptance remains deferred; do not infer hardware support from automated runs.

# SteamOS launcher adaptation

This extends the local redesigned launcher work on the same unmerged branch.
The requested modes are SteamOS Gaming Mode and Desktop Mode, not Windows on
Deck. Physical Deck acceptance remains separate from automated Linux checks.

## Entry points and policy

`HandheldPage.cs` defines pure `HandheldProfile` mode detection and
`HandheldPadState` edge/repeat/neutral gating. Device detection uses `SteamDeck=1`
or Jupiter/Galileo/Steam Deck DMI product names; Gamescope session environment
selects Gaming Mode. `--deck`, `--gaming-mode` and `--desktop-mode` provide explicit
test overrides. Native update helpers forward only these allowlisted, unique
options; `LauncherPlatform.RestartInfo` preserves them after restart. Unknown or
duplicate helper tail options are refused.

Linux `DeckControls.cs` and `DeckGamepad.cs` are thin Avalonia/SDL adapters. SDL3
comes from the installed game or host, with host SDL2 fallback. The launcher
opens only the gamepad subsystem, uses standard mapped buttons/axes, creates no
SDL video window, makes no input grab and changes no Steam Input/game bindings.
Unavailable libraries/controllers leave touch and trackpad navigation working.
Polling occurs only in the active launcher window, never during gameplay.
Neutral gating resets on focus loss, device replacement and long poll gaps.
No on-device controller/suspend/overlay result is inferred from synthetic input.

## Presentation and navigation

NativeWindow wraps its content with the Deck interaction layer only in handheld
mode. The scope is the active tutorial, version picker, settings prompt or page;
inline dialogs take precedence over all background actions. Controller A invokes
the original UI command, B cancels/backtracks, LB/RB step through controls, and
D-pad/stick navigation brings focused controls into view. Sliders and dropdowns
retain their original models. A focus outline follows the selected control.

The handheld layout uses at least14px small text and44px action targets. Home
reserves extra space above Play for the version selector; tools tabs get48px
height. Gaming Mode is fullscreen without saved desktop placement. Desktop Mode
remains windowed and keeps its own size/position. Native game monitoring avoids
minimizing the launcher in Gaming Mode, keeps the launcher alive throughout the
child game, and restores its prior state on exit. Game settings, frame limits,
controller assignments and performance presets are not automatically rewritten.

## Text and files

The built-in keyboard edits a draft and applies only on Done. B/Cancel preserves
the original field. Keyboard-binding capture has a separate key chooser, routing
through the existing capture handler. Steam/Quick Access buttons are untouched.

Deck ISO/extracted-folder imports, save-folder choices and report export
destinations use an overlay in the same launcher window. Home, Downloads, mounted
media and Up provide navigation without typed paths. Original importer/hash and
save-location validators remain authoritative. Existing data is never replaced.
Report ZIP replacement requires confirmation and does not upload anything.
Desktop/Mac continue to use their normal storage pickers.

Deck help lives in the tools header. The Linux ZIP includes the player guide as
the explicitly owned `Support/Launcher/SteamDeck.txt`, hashed in its manifest and
allowed by the Linux launcher resource policy. No system package installation,
Steam shortcut mutation, SteamOS protection change or new SDK version is needed.

## Validation

`launcher_shared_ui.cs` exercises detection/overrides, helper-option validation,
held-button suppression, repeat timing, focus reset, actual button dispatch,
inline picker cancellation/selection and keyboard draft/apply/cancel. Linux adds
the real Avalonia DeckControls test; the pure policy runs on every host.

Use native `--render-appearance-check <dir> --deck --desktop-mode` for tools and
settings layouts, and `--render-tutorial-step1-deck <png> --deck` for the tour.
Capture1280×800 and docked desktop sizes; rendering is evidence of layout only.
Run standard Windows/native build and package/helper regressions after changes.

Physical acceptance must still cover both SteamOS modes, native/Steam Input
controls, touch, SD-card data/saves, text entry, fullscreen/child focus and return,
Steam overlay, suspend/resume, updates and docked output. The Linux game package's
driver/glibc requirements remain its own compatibility gate; neither WSL nor a
handheld launcher render establishes game performance or Deck Verified status.
