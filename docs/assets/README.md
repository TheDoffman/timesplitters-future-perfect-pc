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

- `launcher-preview.png`: launcher **1.8.0** Windows home-screen preview rendered
  from the actual published Windows ZIP (`launcher-v1.8.0`), source
  `7ef572a99030672afb462fb24e003835cdfeaf3f`. The release's archive SHA-256 is
  `ecfbcc99f247c890b90f02c5298f72e1e52fac4cde122881d70517078f129649`.
  `render-launcher-preview.ps1` checks the published application hash, constructs
  the real `LauncherAppearance` adapter, selects Stable through the channel
  picker and supplies only in-memory sample game 0.7.2 state. It renders the
  normal 1184 x 684 desktop layout directly, without image editing or resampling.
  The release itself displays “LOCAL BUILD” in its header; the preview retains
  that text unchanged. Rendered application SHA-256:
  `eb9e3aad6531842b8b5ebb61415f7b5e8afa78f156eec18e92c2ee6d5addecb9`.
  PNG SHA-256:
  `0dae241711bed17e8a32c720b58a05991024cefbbdfdc6bcccfac851f73cfb81`.
  The sample installation is illustrative. Rendering uses a new isolated root,
  starts no game or live network check, performs no update and includes no
  personal settings, paths or saves. Reproduce with Windows PowerShell 5.1 STA:

  ```powershell
  powershell.exe -NoProfile -STA -File docs/assets/render-launcher-preview.ps1 -ApplicationPath '<extracted published 1.8.0 ZIP>/Bin/TimeSplitters.App.exe' -PreviewRoot '<new isolated directory>'
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
- `download-windows-launcher.svg`, `download-mac-launcher.svg` and
  `download-linux-launcher.svg`: flat pale-blue
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
