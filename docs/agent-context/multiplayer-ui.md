# Multiplayer companion presentation

`tools/network/online_theme.py` owns the launcher-colored Tk browser and invitation
view (`build_browser`, `build_invitation`). `online.py` supplies its existing
variables and callbacks; identity verification, session/bridge lifetime, port
validation, invitation parsing and persistence remain in that controller. The
Play online tab separates joining/browsing and hosting; Directory settings holds
the shared directory URL and fingerprint. The navy/blue interior is independent
of Windows app light/dark preference, with native colors in high contrast.
Small viewports scroll and focus traversal reveals offscreen controls. No new
companion module, package dependency or protocol is introduced. Online remains
a Windows adapter; native Mac Online support is still deferred.

## Validation

`tests/network/test_online_theme.py` now exercises the real browser/invitation
widgets at 100/150/200% Tk scaling, native accessibility theme fallback, action
callbacks, tab switching, minimum bounds and keyboard focus visibility in a
short scrolled viewport. It remains in the existing Windows runtime CI suite.
Use `python tools/network/online_theme.py` (optionally `--invitation`) for an
offline preview without game data, saved settings or network operations.
The companion is already included in bundling and compatibility hashes; source
changes need a normal matching game build/package before multiplayer use.


## Project icon

`OnlineTheme.track` loads `multiplayer.ico` beside the packaged companion or from
`build/online-icon` in a checkout. It assigns the icon to both windows and as the
root's default for later popups. `build-online-icon.ps1` renders the vector source
in `IconBuilder.cs` at 16/24/32/48/64/128/256 pixels. Both the normal game resource
build and `bundle_online.bundle_icon` generate it; packages copy it beside the
companion. It requires no private artwork or additional Python libraries. The
header now reads “Find your friends. Start a match.” without the extra tagline.


PR #399 CI exposed clipped directory fields at 150/200% scaling on a 1280x768
Windows runner. `scrollable_panel` now serves the Play tab, Directory tab and
invitation popup, including focus-driven scrolling. The real-widget checks cap
the reported screen size to that runner geometry even on large developer
monitors and verify that the certificate field and popup Return action can be
brought into view. The original assertions remain in place.
