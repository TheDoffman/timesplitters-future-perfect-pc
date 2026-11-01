# README visuals

- `discord-community.svg`: clickable Discord community icon with Discord's
  mark from [Simple Icons](https://github.com/simple-icons/simple-icons/blob/develop/icons/discord.svg)
  (CC0). The same mark geometry is used in the launcher's shared XAML. Discord
  branding belongs to Discord; this icon links to the fan project's server.
  The SVG is self-contained; its destination and accessible label live in README.
  The icon sits at the top right of the README and uses Discord blurple on a
  transparent background so it works with GitHub light and dark themes.

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
- `readme-banner.svg`: a compact vector header using the launcher's navy panels,
  silver title, pale-blue accents and interrupted-ring motif. Its TSFP monogram
  is converted directly from the existing outlines in
  `tools/launcher/GameMonogram.cs` (Future TimeSplitters by Chequered Ink, free
  for all use; attribution remains in the SVG). The full TimeSplitters title
  uses outlined lettering from the same series-inspired typeface, replacing
  the former Georgia/Times New Roman text. The subtitle is also outlined, so
  neither line depends on fonts installed on the reader's device. Source OTF
  SHA-256: `8f09bd612fec0a1aca85b3e2dcd0031d0f726240f37735330de1944bcfce1eb7`.
  Typeface source: [Future TimeSplitters by Chequered Ink](https://www.dafont.com/future-timesplitters.font).
  This is fan-made lettering, not the official game logo. No raw game artwork
  or font file is included. The launcher preview in the README is always visible.
- `download-windows-launcher.svg` and `download-mac-launcher.svg`: flat pale-blue
  platform buttons with the launcher's three-pixel corners, thin border and
  line-icon weight. Links and accessible labels live in the README.
- `icon-play.svg`, `icon-settings.svg` and `icon-support.svg`: small section
  markers using the launcher's line-icon style; the settings glyph follows
  `Launcher.xaml`. Heading text remains Markdown with decorative empty image
  alt text. Explicit anchors preserve the original section links because GitHub
  adds a leading dash to automatically generated slugs for icon-first headings.
  Section and download SVGs are self-contained, with opaque backgrounds and
  contrasting foregrounds for both light and dark GitHub pages. They contain
  no scripts, remote resources or embedded raster artwork.

The PNG documents the interface, including its embedded branding. Raw source
artwork and original game assets remain private. Keep one launcher preview here
and replace it when the documented interface changes.
