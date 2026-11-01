# Story co-op beta launcher handoff

The [Windows game beta is published](https://github.com/TheDoffman/timesplitters-future-perfect-pc/releases/tag/v0.5.0-story-coop-beta.1)
from commit7262f76d. The compatibility source merged into main through
[PR444](https://github.com/TheDoffman/timesplitters-future-perfect-pc/pull/444),
including both launcher hosts' CI checks. Launcher publication is still delegated.

The game preview is `v0.5.0-story-coop-beta.1`, published from
`online-story-coop`. Its immutable tag-derived metadata channel remains
`story-coop-beta`, with display name **Story Co-op Beta Preview**. Keep the
Python release identity and game package metadata unchanged: existing published
launchers validate that exact relationship and can install this separate track.

The shared launcher source in `tools/launcher/ReleaseClient.cs` groups only
`story-coop-beta` into the existing `story-coop` track. Its UI name is
**Story Co-op Preview**. Other named tracks, including names ending in `-beta`,
stay separate. Metadata is still checked against the original tag-derived
channel before UI grouping; an alias does not permit mismatched metadata.

Saved `game_channel` and `game_preview_track` values migrate through the shared
preference policy. Reads do not write files; explicit saves store the canonical
track and preserve unrelated preferences, including launcher-preview enrollment.
Installed-version identity and update selection use the same mapping. Semantic
version ordering remains unchanged: subsequent previews should advance
`story-coop-beta.N` or their base version, rather than assume `story-coop.N` sorts
after a beta suffix on the same base.

The user assigned launcher publication to another user. This task does not bump
or publish a launcher version. That publisher should start from main containing
this change, select the next launcher version, and follow
[the launcher release workflow](launcher-releases.md), including both host builds,
tests and the complete verified asset set. Until that launcher is published and
installed, users must explicitly select **Story Co-op Beta Preview** as a separate
track. Existing Story Co-op selections will discover it after the launcher update.

`tests/launcher/launcher_shared_ui.cs` runs on both launcher hosts and checks mixed
release catalogs, single-track grouping, exact metadata rejection, installed
identity, alias selection, read-only preference migration and preservation.
Windows validation and CI results are recorded in [STATUS](../project/STATUS.md).
Native visual/menu acceptance remains distinct from the shared fixture checks.
