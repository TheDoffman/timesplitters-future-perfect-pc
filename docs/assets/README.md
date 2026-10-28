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
  2026-10-28 from published source revision
  `2a2ffe369a28ba2541d54437af63002020487ef6` (`launcher-v1.4.0`). The maintained
  Windows builder used checksum-pinned artwork, then the local source build ran
  `--render-preview`. It is a direct 1152 x 772 client-area render with no image
  edits or resampling. Rendered application SHA-256:
  `9eb58f9a508e83634151c96cb42b1cf2f54ca385fc3321767b3ceb8ed446fe7d`.
  PNG SHA-256:
  `adf61e42899aeb7aac05b8aecd058d570673e20baf0e06c8d3c26c4ba71d4882`.
  The screenshot shows the current branding, shared navigation and version label.
  Game version/channel values are illustrative; Stable is the installation
  default and previews are optional. Rendering starts no game, performs no update
  or live network check, and includes no personal settings, paths or saves.
- `download-windows-launcher.svg` and `download-mac-launcher.svg`: the two
  platform download buttons used by the README. Links and accessible labels
  live in the README; the superseded generic button has been removed.

The PNG documents the interface, including its embedded branding. Raw source
artwork and original game assets remain private. Keep one launcher preview here
and replace it when the documented interface changes.
