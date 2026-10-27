# Configuration

| Folder | Responsibility |
| --- | --- |
| `build/` | Checksum-pinned SDK, artwork and native-license metadata. |
| `textures/` | Optional-pack delivery and original-texture policy. |

Default controls/Discord files remain at this level beside the ignored personal
configuration paths used by existing installations. Preserve those personal
files during builds, upgrades and organization changes.

`texture-download.json` stays here as a public API: released launchers fetch
that exact URL. Publish updates to this single canonical manifest; moving it
requires compatibility with already installed launcher versions.
