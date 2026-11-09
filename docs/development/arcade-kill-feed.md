# Optional Arcade kill feed

Video Options in the frontend and pause menu contains Kill Feed, OFF by default.
The choice takes effect immediately and is saved beside the active display
configuration in pc-hud.ini (TSFP_HUD_V1). Failed saves retain the old choice.

The shared pc_kill_feed_guest.inc observes supported-US Arcade score columns
(actor stride 0xea0, kills +0xa10, name pointer +0x70). It does not invoke damage,
AI, scoring, or network sends. Existing stock player replication and the bot
score-column replication supply online observations. Patch 2240 wraps the
per-viewport HUD pass 0014F0B0; shared pc_kill_feed.h keeps five rows, expires
at five seconds and fades over the last second. Disable, join, roster or match
changes rebaseline without replaying historical kills. Leaving gameplay also clears the baseline in the menu adapter, because the
HUD does not run on results/loading screens. Older score snapshots
are ignored. Each viewport receives the feed with proportional text.
Text uses 40% of the native HUD font in authored coordinates (previously 65%).
The shared RIGHT/CENTRE HUD placement applies uniform resolution scaling;
no additional host-pixel multiplier is applied. Margins and row spacing use
that same transform, preserving proportions on ultrawide displays.

Names are colored relative to the current viewport: blue for yourself/allies,
red for enemies (other players in free-for-all). Patch 2480 observes weapon
projectile construction, hit-record creation and confirmed score increments,
while preserving the existing Story departure wrapper. The firing-scope patch
runs after the native timing wrappers and observes synchronous player (244F30)
and bot (1BB5C0) hits without changing their timing/network callbacks. The original weapon
silhouette appears between names when attribution is available. Allocation
reuse invalidates cached identity; collisions and unavailable remote metadata
use a neutral separator instead of guessing a weapon. Native online score
replication does not transmit this additional icon metadata yet. Unattributed environmental deaths are omitted.
Multiple kills in one snapshot appear in actor order, not reconstructed timing.
This is a local display preference: other players need not enable it.

Regression route: python -m pytest tests/graphics/test_kill_feed.py -q.
Covers events, retransmissions, wrap, roster/match changes, bounded rows, fades,
persistence/default/error handling, team-relative colors, weapon switching,
projectile reuse and attribution collisions. Windows full Build compiles the guest
adapter. Live online, split-screen, and native Mac acceptance are separate from
these synthetic tests; record actual validation in docs/project/STATUS.md.

Icon layout: a 24-unit gap reserves equal padding around the silhouette. Icons
fit within 20 units wide and 125% of text height, without stretching. Text and
icons share a row center; row spacing includes the taller icon. Native inventory
silhouettes are mirrored horizontally so the barrel faces the victim on the
right. Unknown-weapon separators are centered in the same gap.
