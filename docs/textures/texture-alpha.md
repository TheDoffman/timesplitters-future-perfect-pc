# Texture overhaul alpha

This is an early source alpha for [issue #74](https://github.com/TheDoffman/timesplitters-future-perfect-pc/issues/74).
It adds the live Classic/replacement switch and the tools used to build,
review and audit the texture overhaul. The complete remaster is still in progress.

**Git contains source, tests, documentation and a small download manifest.**
Compiled replacements are an optional GitHub release pack offered through the
launcher's **Game settings → Graphics & resolution → Download 4K texture pack**.
Private source artwork stays local; checking out this branch does not download textures.
Original game files, saves and personal settings are not included.

## What is available

- In the game's **Settings / paused Options > Display settings**, use
  **Textures: Classic / Enhanced**. The native arrows change the mode immediately;
  **Apply settings** still controls resolution/FOV. This choice lasts for the
  current game session. Windows also supports **F10** or
  **PC Settings > 4K Textures**. Texture mode and render resolution
  are independent; the menu label does not mean every texture is 4K.
- Load full-mip DXT1, DXT3, DXT5 and compatible RGBA DDS replacements. Missing,
  invalid or over-budget files fall back to originals.
- Catalog supported archive formats, investigate embedded/non-PAK payloads,
  group materials for production, and review actual encoded DDS output offline.
- Preserve verified original alpha when appropriate, record source sizes and
  resampling, and enforce bounded pack and temporary storage.

For setup and local preview, follow [the texture guide](textures.md#preview-an-installed-local-pack).
An existing local pack and a runtime built with this branch's patch stack are
required. Restart after changing pack files.

## Local development snapshot

Recorded on October 19, 2026, after correction batch 188 at `514ea67`.
These figures describe the development snapshot. The first optional public alpha
uses these 1,242 DDS files with sanitized metadata (about 990 MiB installed).

| Measure | Audited state |
| --- | --- |
| Installed, approved offline | 1,242 / 6,812 supported keys (18.23%) |
| Needs artwork | 5,477 keys |
| Held for correction | 93 keys |
| Visually classified originals | 3,353 keys |
| Supported catalog payloads | 7,147 |
| Active pack, including metadata | 990.15 MiB / 6,144 MiB |
| Temporary texture files | 152.85 MiB / 256 MiB |
| Texture workspace, including source art | 5.60 GiB allocated on Windows |
| Replacement residency limit | 512 MiB; current-pack GPU behavior unverified |

The latest native audit accepted all 1,242 installed DDS files for format,
dimensions and mip chains. Accepted artwork has offline comparison evidence;
that does not establish appearance in every scene. Source art uses appropriate
sizes, with up to 4K reserved for major surfaces. Resampled output is recorded
explicitly and is not described as native 4K artwork.

## Alpha limits and next gates

Other encodings, embedded/procedural assets and broader discovery remain open.
Supported-key coverage is therefore not proof of complete game coverage.
Held drafts are excluded from the active pack and from completed counts.

Consolidated playtesting remains deferred until at least 90% of audited in-scope
replacements pass offline review and every category has been reviewed. Final
checks must cover representative environments, props, weapons, characters,
effects and UI; scene UV alignment, transparency, 1080p/4K/ultrawide modes,
memory, fallback and stability. A full campaign playthrough is a separate check.

Windows live-switch and loader checks do not prove Mac parity. Windows and native
Mac use the same in-game Display menu and mode policy. Both Mac renderer paths
consume that policy; the separate Cocoa settings/F10 shortcut remains pending.
Native Mac interaction and real-art scene validation remain pending.
Legacy GL accepts compressed replacements and falls back to originals for RGBA
replacements. Platform validation is tracked under
[MAC-011](../project/MAC_PORT_BACKLOG.md#mac-011--real-texture-pack-gameplay-coverage).

See [the batch 188 audit](../project/STATUS.md#texture-overhaul-issue-74-correction-batch-188)
for measured progress and the [texture guide](textures.md) for production and
storage rules. This alpha does not close issue #74.
