# README visuals

- `online-host.png`, `online-join.png`, `online-invitation.png`: actual Windows
  Tk widgets rendered from `tools/network/online.py` at source revision `ed8bff1`,
  captured on 2026-10-17 for `docs/players/multiplayer.md`. Main-window captures are
  820 x 750; the invitation dialog is expanded to 700 x 330 so all buttons fit.
  These are documentation previews with isolated example data, not evidence
  of a connected match. Identity checks and hosting are stubbed only in an
  ignored capture harness; production source is unchanged. No game is launched,
  no network service starts, and personal settings/saves/clipboard are untouched.
  The example uses reserved address `203.0.113.10`, a dummy fingerprint and
  join code `DEMO01`. Images are direct client-area screenshots, without edits
  or resampling. Only these selected documentation captures are retained here.

- `launcher-preview.png`: launcher v1.4.0 Windows interface preview rendered on
  2026-10-30 from published source revision
  `2a2ffe369a28ba2541d54437af63002020487ef6` (`launcher-v1.4.0`). The maintained
  Windows builder used checksum-pinned artwork. `render-launcher-preview.ps1`
  loads that same build in preview mode with a new isolated root, selects Stable
  through the launcher's channel picker and labels the installed game as sample
  data. It is a direct 1152 x 772 client-area render with no image
  edits or resampling. Rendered application SHA-256:
  `9eb58f9a508e83634151c96cb42b1cf2f54ca385fc3321767b3ceb8ed446fe7d`.
  PNG SHA-256:
  `25c7d169ff9a12a85e1d7802a465b54c5d4a8af2a6a7373c22d5574c823ad486`.
  The screenshot shows the v1.4.0 branding, shared navigation and version label.
  Installation data is illustrative; Stable is the installation
  default and previews are optional. Rendering starts no game, performs no update
  or live network check, and includes no personal settings, paths or saves.
  Reproduce with Windows PowerShell 5.1 in STA mode (the script validates the
  application's hash and requires a new preview directory):

  ```powershell
  powershell.exe -NoProfile -STA -File docs/assets/render-launcher-preview.ps1 -ApplicationPath '<v1.4.0 build>/Bin/TimeSplitters.App.exe' -PreviewRoot '<new isolated directory>'
  ```
- `download-windows-launcher.svg` and `download-mac-launcher.svg`: the two
  platform download buttons used by the README. Links and accessible labels
  live in the README; the superseded generic button has been removed.

The PNG documents the interface, including its embedded branding. Raw source
artwork and original game assets remain private. Keep one launcher preview here
and replace it when the documented interface changes.
