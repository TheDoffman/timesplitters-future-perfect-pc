# README visuals

- `online-host.png`, `online-join.png`, `online-invitation.png`: actual Windows
  Tk widgets rendered from `tools/online.py` at source revision `ed8bff1`,
  captured on 2026-10-17 for `docs/multiplayer.md`. Main-window captures are
  820 x 750; the invitation dialog is expanded to 700 x 330 so all buttons fit.
  These are documentation previews with isolated example data, not evidence
  of a connected match. Identity checks and hosting are stubbed only in an
  ignored capture harness; production source is unchanged. No game is launched,
  no network service starts, and personal settings/saves/clipboard are untouched.
  The example uses reserved address `203.0.113.10`, a dummy fingerprint and
  join code `DEMO01`. Images are direct client-area screenshots, without edits
  or resampling. Only these selected documentation captures are retained here.

- `launcher-preview.png`: UI preview rendered from `cleanup/readme-launcher-copy`
  (implementation `b9a05b9`) using the launcher's `--render-preview` command after removing slogans and
  themed status quips. This is a source/development preview, not a published
  launcher screenshot. Its installed version/channel values are illustrative;
  the README installation steps use Stable by default. Preview channels are
  optional testing choices; the screenshot does not prescribe an update channel.
- Rendered application SHA-256:
  `5763c056ea3da1f6779bbd901524afcb4a91dda3ee1fb5d77f2acc10291e579f`.
  The launcher was built on Windows x64 with the existing private banner/logo
  resources. Rendering opens no game, performs no update, and includes no
  personal settings, paths or saves. Screenshot resolution: 1152 x 772.
- `download-windows-launcher.svg` and `download-mac-launcher.svg`: the two
  platform download buttons used by the README. Links and accessible labels
  live in the README; the superseded generic button has been removed.

The PNG documents the interface, including its embedded branding. Raw source
artwork and original game assets remain private. Keep one launcher preview here
and replace it when the documented interface changes.
