# Discord help-and-bug-reports sweep, 2026-11-05

Eight active posts were opened and investigated, including their supplied
visual evidence and replies. The two marked Older Posts (mech mouse input and
exFAT launch) were excluded at the user's instruction. A separate branch was
created and pushed for every active post. No branch was merged into main and
no release was published.

| Report | Branch | Outcome |
| --- | --- | --- |
| MSAA breaks rendering | [`fix/discord-msaa-rendering`](https://github.com/TheDoffman/timesplitters-future-perfect-pc/tree/fix/discord-msaa-rendering) | Confirmed fix; Windows actual Chinese 8x scene plus GPU regression, negative control and native checks. Head c0c63c6f. |
| Uplink cannot grab at high FPS | [`fix/discord-uplink`](https://github.com/TheDoffman/timesplitters-future-perfect-pc/tree/fix/discord-uplink) | Confirmed fix; original-XBE regression plus actual barrel hold/release at 120 and 180 FPS. Head 2aff082b. |
| Texture flickering | [`fix/discord-texture-flickering`](https://github.com/TheDoffman/timesplitters-future-perfect-pc/tree/fix/discord-texture-flickering) | Unresolved. Polygon-offset experiment did not remove the broad kitchen flicker; not a verified report fix. Head f600d316. |
| Neophyte Lucian LOD | [`fix/discord-lucian-lod`](https://github.com/TheDoffman/timesplitters-future-perfect-pc/tree/fix/discord-lucian-lod) | Unresolved. Close appearance captured; distant target not reliably visible, so no verified LOD repair. Head 9673869a. |
| Rare client cannot shoot/switch | [`fix/discord-client-weapon-desync`](https://github.com/TheDoffman/timesplitters-future-perfect-pc/tree/fix/discord-client-weapon-desync) | Unresolved. Two local LAN cohorts, 16 normal and 48 stress samples, did not reproduce host/client weapon disagreement. Head 98b8179c. |
| Audio/music glitching | [`fix/discord-audio-music`](https://github.com/TheDoffman/timesplitters-future-perfect-pc/tree/fix/discord-audio-music) | Unresolved. Actual minigun bursts and round result; 59 existing buffer/device checks. Pitch/reset churn is an unproven lead. Head aa4dcc1c. |
| Bots through Chinese walls | [`fix/discord-chinese-wall-visibility`](https://github.com/TheDoffman/timesplitters-future-perfect-pc/tree/fix/discord-chinese-wall-visibility) | Unresolved. Clip confirms a body over brick. Local six-bot Original60 observation and 12 depth/state tests did not reproduce it. Head 65f807f2. |
| Minor cutscene/graphical bugs | [`fix/discord-cutscene-graphics`](https://github.com/TheDoffman/timesplitters-future-perfect-pc/tree/fix/discord-cutscene-graphics) | Unresolved. Fourth-mission startup shows partial-width opening bar; reported shutter/thermal/puzzle/laptop checkpoints not verified. 17 existing camera/overlay tests pass. |

Only the MSAA and Uplink posts received exactly:

> issue resolved and will be in the next game update

Those comments were visually confirmed after sending. No duplicate comments
were added and the six unconfirmed posts were not marked resolved.

All runtime verification was Windows-local using isolated save roots and a
checksum-verified US XBE. Physical Mac/Linux acceptance remains unverified.
The supported Windows builds required the documented local compiler workaround
for seven oversized generated units; do not describe them as default-flags
Release passes. Source changes and investigation notes are pushed; game assets,
binaries, captures and logs remain local/ignored. Original user settings/audio
edits were preserved in the original checkout.

The rejected polygon-offset candidate must not be merged as a texture-flicker
fix. The other five unresolved branches contain investigation documentation,
not speculative runtime repairs. Their linked branch documents give specific
reproduction gaps and next discriminating checks.
