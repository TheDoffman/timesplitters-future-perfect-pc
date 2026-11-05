# Discord cutscene/graphical report investigation (unresolved)

Report: https://discord.com/channels/1566295768489263136/1567356289946886374
Branch: `fix/discord-cutscene-graphics`, based on main `831c324a`.
No runtime fix or resolution comment: all parts of the report are still open.

All six replies and the supplied images were reviewed. The report contains
three acceptance targets: Mission 4 metal shutters/gas trap failing to deploy;
partial-width widescreen black bars around a thermal display; and blank moving
displays, including missile-puzzle endpoints and a U-Genix laptop. The reporter
already reduced to 60 FPS and still saw the missile display failure. Treating
all of these as a high-refresh timer issue would contradict that evidence.

Existing production camera-feed placement and cinematic-letterbox tests passed
13 cases, including the locally checksum-verified original US routines.
Crane/remote-view overlay tests passed four further cases. These check known
coordinates, camera scaling, scopes and batching; passing them does not prove
the reported displays or shutter script work. The existing cinematic fixture
mocks rectangle drawing and cannot catch a renderer squeezing the final matte
into a centered 4:3 area.

Actual Windows main-baseline game used SHA256
`550f988c6ec45e9504d74bcecd9486874928b5272712683b7e9e8b9152ac5936`,
Classic textures, 960x540, one sample, Original 60, isolated Test Lab. Three
ordinary Down inputs selected the fourth Story mission and normal confirmation
loaded its train opening and gameplay. An opening frame showed a thin black
bar only across the middle 720 of the 960 output pixels (roughly x120..839),
with world visible at both sides. It disappeared by ordinary gameplay. This
is an actual additional reproduction of partial-width cinematic coverage;
it is not the reporter's thermal-display checkpoint, and no common cause has
yet been established.

Source inspection: `sub_0001EFB0` adapts height for the two exact cinematic
caller pairs, while other rectangles are delegated. `pc_display_camera_feed`
keeps texture-camera layers in guest coordinates using explicit placement and
batch flushes. Cinematic height adaptation alone is not proof of final
full-width coverage. A scoped placement/batch intervention with a captured
bad draw is the next discriminating test; globally stretching every rectangle
would break scope artwork, subtitles and split-screen views.

GitHub #49 / PR #195 already repaired an older U-Genix left-strip/scanline
placement defect. That is different from this report's entirely blank laptop
and does not justify declaring this post resolved. The independent MSAA depth
repair might be relevant only if target binding actually fails at a reported
display; the reporter's sample setting is unknown, and that link is unproven.

Local evidence: `logs/cutscene-baseline-observation` (first complete bounded
mission startup, provenance, route, capture and gameplay image),
`logs/cutscene-opening-observation` (repeat opening snapshots),
`logs/discord-cutscene-baseline-tests.log`, and
`logs/discord-cutscene-overlay-tests.log`. The supported baseline build used the
recorded local compiler workaround for seven oversized generated units.
No personal save was used. The gas-trap, puzzle and laptop checkpoints were
not reached or independently verified; this remains an investigation branch.

Next: reproduce each checkpoint separately, record its script state or camera
copy/target/depth operations, then prove a targeted change with a before/after
case and 4:3, widescreen, partial-view and split-screen controls. Keep the post
open until its complete set of reported symptoms is checked.
