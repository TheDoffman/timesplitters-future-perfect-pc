# Measured progress

## Issue #17 spirit rendering (2026-10-17)

Production changes are on
`17-ghosts-in-mansion-of-madness-and-what-lies-below-are-nonvisible`, based
on main `5548b25`. Windows x64 / D3D11 / MSBuild 17.14.40 / Python 3.12.14 /
CMake 4.4.4; supported US XBE SHA-256 verified. The complete local patch stack
was applied before regenerating wrappers and building both Release game and
replay targets. Executable SHA-256:
`47ac8d2733d448aed126bd00453dfa83ea79ac7de24fe56c2ba1fd8e80397393`.

`src/pc_ghost_render_guest.inc` and the final `spirit-rendering.patch` replace
the temporary eye/head probes. The US spirit renderer scopes clockwise
culling to rigid model nodes; skinned ribbon trails remain two sided. Its
two eye-billboard call sites use zero clip-space depth bias and no fog only
for their flushed batches. Alpha blending, depth testing, depth writes and
animation remain authored. The shared HLE override saves/restores host state
and invalidates the guest-state cache on entry AND exit, including renderer
exit before the eye batch ends. No shader hashes, shader string rewriting,
geometry fingerprints, diagnostic switches or forced visibility are shipped.

These are title compatibility corrections, not general Xbox semantic changes.
The original spirit path requests no culling; back-facing skull triangles
then overdraw the front. Its eye bias 200 is multiplied by 256 and added to
normalized clip Z, clipping the sprites; the unwritten fog fallback also
replaces their textured red color. The original Xbox bias conversion remains
unresolved, so the correction is confined to these two spirit call sites.

Final production capture `build/issue-17-ghosts/production-live.d3dcap` has
252 draws: all five skull draws use clockwise culling, all 30 trail draws
retain no culling and fog, and all ten eye/eye-trail draws have c13.w = 0,
fog disabled and depth testing enabled. All retain blending and disabled
depth writes. Fog and culling restore between spirits. The reviewed live
`production-frame013.bmp` shows skull faces, wispy trails and glowing red eyes.
Routing contains no diagnostic flags. This bounded 1920x1080 session used
an isolated unlocked Test Lab copy, controls and both display/XDG configs;
personal saves/settings and the canonical fixture were preserved. The log
records normal user window closure and ExitProcess(0), not an exception.

Validation: 54 distinct targeted tests passed (40 native/state-cache,
framebuffer, channel-clear, CI routing, generation and patch-tracking tests;
14 patch-upgrade tests, with the two spirit tests repeated). The spirit harness
compiles the actual guest wrappers and HLE cache, checks both billboard call
sites, rigid/skinned selection, unrelated callers, ABI preservation, flush
ordering and state restoration; a deliberately broken cache-restore variant
must fail. It is included in the Windows/macOS runtime CI and local Test suite.
Full-stack patch verification changes zero runtime files. No complete game
regression suite or hosted CI run is claimed for this validation.

Before publishing, main `33fc619` was merged into the issue branch; both
validation records were preserved when resolving the STATUS conflict.
Full-stack patch application succeeded, followed by 39 passing spirit,
patch-upgrade/tracking, projection and UI-layout tests. The build and live
capture above precede this integration; the merged game was not rebuilt or
playtested again.

PR #84 CI follow-up: the first macOS ARM64 run passed patch transitions but
both spirit harness variants failed to link `GetTickCount`, called only by
HLE diagnostic counters. The standalone harness now supplies a deterministic
clock on both hosts, matching the framebuffer harness, instead of relying
on the Windows system library or linking the complete Mac compatibility layer.
Runtime/game behavior is unchanged. All four local Windows spirit/framebuffer
variants passed. Hosted run [39008025033](https://github.com/TheDoffman/timesplitters-future-perfect-pc/actions/runs/39008025033)
passed both jobs at fix commit `57324bc`: macOS ARM64 had 26 patch tests,
66 native regressions and 32 release tests pass (7 platform-specific skips
across those suites). Windows had 28 patch tests, 68 native regressions,
35 release tests, 99 launcher regressions, 25 launcher settings checks and
6 startup/packaging tests pass (2 native skips and 2 passing subtests).
These source-only CI results do not establish native Mac gameplay fidelity.

Earlier live comparisons with #9/#21 disabled did not identify either as the
fix for #17. A forced visibility-query probe worsened effects and was removed.
Eye/head probes restored eye glow and reduced back-face overdraw in identical
replays, but the user's head-opacity feedback was inconclusive. Old probes,
captures and comparisons remain ignored investigation records outside the
active patch stack. The production live draw states above supersede those
probes. A synthetic old-capture replay was not pixel-identical to its earlier
reference; it is not used as a fidelity pass.

Remaining validation: original-Xbox fidelity, later ghost-girl/What Lies Below
encounters, and native macOS GPU/interpreted-vertex paths. This is evidence for
the Mansion entrance-hall spirits, not a claim that every encounter in #17 is
verified. See [MAC-026](MAC_PORT_BACKLOG.md#mac-026--spirit-skull-and-eye-rendering-17).

## Arcade RANK widget placement (2026-10-17)

Follow-up to issue-22 commit `6485069` on
`22-character-selection-screen-is-stretched-when-using-higher-resolutions`, as
requested. The existing status icons were left-anchored, but the separate US
RANK call (`0012EC8B`, from ordinal/label renderer `sub_0012EC20`) to the shared
panel renderer `sub_0012E3F0` still inherited
the centred 4:3 canvas. Its caption,
ordinal and frame therefore drifted inward relative to the icons in widescreen.
The additional runtime wrapper routes the whole widget through the shared
`pc_display_ui` LEFT placement scope. Guest sprite batches are flushed on both
boundaries; original scaling and vertical coordinates remain owned by the game.
The existing guard preserves original/narrow aspects and split-screen placement.

Windows x64 / Python 3.14.2 / MSVC 19.42 / D3D11: the supported
`tsfp.ps1 -Action Build` workflow applied the complete patch stack, regenerated
the guest hook and compiled the game. Native network/GPU-clear/texture/display
checks passed. The first build completed all native checks, but final reruns
stopped at the unrelated virtual-pad prompt test's foreground-window assertion
(`pc_controls_accept_pad()`), as recorded for issue #22. Input code is unchanged.
Final source-level UI scope/viewport checks passed 5 tests; model-projection
checks passed 4 tests; patch application/upgrade/tracking checks passed 28 tests.
The widget regression exercises separate caption/ordinal/frame batches, preserves
adjacent HUD groups, excludes the timer's shared-renderer call, and checks 4:3
and split-screen bypasses. Executable SHA-256:
`455166dd4c885123e880e77934df9b58b1ff599973b0e3d40455db113ad9898c`.

Three bounded, 55-second D3D11 Arcade Vietnam sessions used disposable Test Lab
copies, synthetic keyboard input and isolated input/both display configurations.
Reviewed 1920x1080, 1280x960 and 3440x1440 captures show the complete RANK widget
above the status icons at its original left inset. At 1920x1080, its label moved
from X=125–146 to X=45–66 in the retained 640-pixel-wide window capture: exactly
the missing 80-pixel side-anchor offset, with the same 22-pixel text width.
The 4:3 layout is unchanged; 4th/5th rank labels were captured. The bounded
runtime trace confirms the actual panel caller is `0012EC8B`. Captures and logs
remain local in `private/test-saves/issue22`; each pass recycled its known
partition images, and the canonical fixture still verifies. Build seal/content
verification and `git diff --check` pass. Native Mac execution and full
split-screen/all-match-mode playtesting remain unverified.

See [MAC-027](MAC_PORT_BACKLOG.md#mac-027--arcade-rank-widget-left-placement)
for shared native support and pending Mac visual checks. Personal saves and
settings remain separate from the isolated Test Lab validation copies.

## Issue 22 model projections (2026-10-16)

Branch
`22-character-selection-screen-is-stretched-when-using-higher-resolutions`, based
on current main `5548b25`. The existing perspective wrapper now corrects the
fresh matrices built by US character picker `sub_002C44D0` (return `002C46EF`)
and first-person weapon lens `sub_001B73F0` (return `001B7482`). Those builders
bypass the world-camera hook. Horizontal scale follows the selected output
aspect while each builder retains its own vertical lens, depth and zoom.
Correction happens after each rebuild, including alternating frame buffers
during character changes. Decals still copy the world camera aperture; other
perspective builders and offscreen effects are unchanged.

Windows x64 / Python 3.14.2 / MSVC 19.42 / D3D11:

- Complete patch stack applied and generated sources refreshed by the supported
  `tsfp.ps1 -Action Build` workflow. The first build completed all native checks
  (network, real-GPU clears, textures, display, prompts, paths, controls, settings
  and debug). The outer PowerShell redirection reported a native-stderr warning
  about unavailable optional ABI analysis, despite the inner build/checks
  finishing. A later incremental build produced the identical executable but
  stopped at the prompt test's `pc_controls_accept_pad()` assertion because its
  window was not foreground. The isolated prompt retry also lacked foreground
  access; no input code was changed to bypass this host prerequisite.
- Combined patch-transition/tracking, projection, cinematic and display suites:
  **37 passed**. After strengthening non-default-aspect exclusions, all four
  projection cases passed again. Tests compile the actual wrapper for both
  `TSFP_PC_CONTROLS` and `TSFP_NATIVE_DISPLAY`, with/without a camera. They check
  repeated alternating-buffer builds at original/4:3, 5:4, 16:9, 21:9 and 32:9,
  preserve non-horizontal coefficients and guest ABI, and exclude unrelated
  callers. Patch-test temporary staging was routed inside the workspace after
  intermittent system-temp/file-access failures; final checks passed.
- Bounded isolated Test Lab sessions at 1920x1080 and 1280x960 loaded Arcade
  Vietnam, displayed character selection and first-person dual pistols. Twelve
  rapid character changes retained the corrected lens. At 1920x1080 the sampled
  picker X coefficient changed from `1.12042034` to `0.84031522`, exactly the
  required 3/4 factor; Y remained `1.73205078`. Reviewed captures show proportional
  models and weapons. At 4:3 no aspect correction is applied. World camera and
  CPU frustum remain unchanged. Captures/timing samples are local under
  `private/test-saves/issue22`; all six disposable disk images were recycled.
- Built executable SHA-256:
  `bd3532a8cf764f3062438847c67e5c382bf88ebf142e75c9f9b0bd3816ec56f6`.
  Build seal and multiplayer content verification pass; the supported US XBE
  hash was verified before game tests. Personal saves/settings were not used.

Ultrawide coverage is a wrapper test, not a reviewed game capture. Full
split-screen/aiming/zoom playtesting and native Mac execution remain unverified.
See [MAC-026](MAC_PORT_BACKLOG.md#mac-026--character-picker-and-weapon-proportions-issue-22).

## Runtime CI efficiency

Branch `ci/faster-runtime-checks`, based on main `9f65dcc`.
Inspected successful hosted run `38997584157`: Windows took 4m38s, including
2m42s for patch tracking/upgrades; macOS took 1m11s. The old workflow ran all
suites on matching pushes to every branch, including launcher-only changes.

CI now runs affected suites on PRs targeting main and relevant main pushes,
plus a complete manual run. Superseded runs on the same PR/ref are canceled.
Launcher/publisher-only edits omit upstream checkout, patch upgrades and native
regressions. Patch/loader/pin/workflow edits select full coverage; shared inputs
embedded by the launcher also select its tests. Native regressions use one
pytest session. Transition scenarios retain independent writable fixtures,
sharing only pristine upstream bytes. The loader removes redundant dry runs
for tracked unwinds and active applications inside the disposable validation
tree; legacy bootstrap probes, conflict checks and rollback are retained.

Windows x64 / Python 3.14.2 / pytest 9.1.1 / Git 2.47.0.windows.2:
baseline transition suite: 27 passed in 107.81s. Final transition/routing suite:
47 passed in 67.54s (about 37% less elapsed time, with added coverage).
An earlier final-version pass had one disposable-fixture file-access error in
the CRLF test; that test passed alone and the complete clean rerun above passed.
The fixture-only optimization had no material timing gain (107.73s); reducing
duplicate Git applications supplied the measured transition improvement.
Combined native invocation:
68 passed in 30.61s, including 19 CI routing tests. Actionlint 1.7.12 accepted
the workflow, and the complete pinned upstream patch stack verified with zero
runtime files changed. All tests used disposable source/native fixtures;
no personal settings or saves were used, and no game was launched or rebuilt.
Hosted execution/timing for this revision, macOS execution and other-device
synchronization remain unverified. See [MAC-014](MAC_PORT_BACKLOG.md#mac-014--patch-upgrades-across-windows-watermark-and-native-overlay-guards).

## Reusable testing profile (2026-10-16)

Branch `test-profile-fast-start`, based on main `041340a`.
`tools/testing_profile.py` created an ignored canonical Test Lab fixture from the
fresh disposable lobby-validation profile. It sets the supported US game's
story completion/award masks and 48 challenge grades while preserving all other
profile bytes, including fresh collectible/pickup progress. The runtime HMAC-SHA1
signature is regenerated; source saves and personal settings are untouched.

Windows x64 / Python 3.14.2: 12 save/launcher regressions passed. Coverage includes
signature rejection, version rejection, exact preservation outside unlock fields,
canonical fixture protection, disposable-copy restoration, personal-save refusal,
directory isolation and the existing startup-video option. An eight-second
`--play` run exited at its bound, capped its logs, and recycled its known virtual
disk images. The canonical manifest still verifies after game testing.

A separate bounded D3D11 menu probe loaded Test Lab successfully, showed all three
story difficulty completion marks, reached the late `Time To Split` mission
briefing, and displayed unlocked Challenge sets with their awards.
This was a compatibility test of the existing issue-21 build, SHA-256
`0cc1e596f8f4e9de381f45c0337340a1e99c0d92270aead800d6adb6cea3c982`
(source `f17026b`), not a build of this tools-only branch. The US XBE hash was
verified before execution. Actual pickup collection and every individual
character/map have not been playtested. No runtime patch or game rebuild was
needed. Native macOS execution and other-device synchronization are unverified;
see [MAC-025](MAC_PORT_BACKLOG.md#mac-025--reusable-unlocked-testing-profile).

At the user's request, a separate Test Lab copy was added to personal saves in
the previously unused `UserData/45410066/544553544C41` slot. Hash checks confirmed
all pre-existing title save files and both personal controls/display settings
were unchanged. This local copy is for manual play only; automated checks still
use disposable copies. No personal-save game session was launched for testing.
## Issue 21 lobby text masks (2026-10-16)

- Source: lobby text-mask fix in this revision on
  `21-online-lobby-lower-half-of-screen-is-all-white-and-unreadable`, based on
  `041340a`; pinned upstream `d90c929` and checksum-verified US XBE. Host:
  Windows x64 build 26300, MSVC 19.42, Python 3.14.2, CMake 4.4.4, D3D11.
  Release executable SHA-256:
  `0cc1e596f8f4e9de381f45c0337340a1e99c0d92270aead800d6adb6cea3c982`;
  multiplayer identity `406bfbba`.
- Reproduced the issue in an isolated System Link lobby: white fill covered
  overlapping information pages. These pages draw into destination alpha,
  then use it as a text mask. The old Xbox-to-host mapping dropped alpha-only
  clears, leaving both background alpha and previous page masks intact.
- Fix: preserve partial colour-clear flags, clear only selected channels through
  an RHI pass, and retain ordinary full/depth/stencil clears. The pass leaves guest
  state unchanged and restores its output targets/viewports. The existing Mac GL
  clear now honours the same channel flags and restores its render state.
  Capture version 8 records the flags; readers still accept versions 5–7.
- Validation: complete patch-stack validation, supported Windows Release build,
  all native build tests, and executable/content seal verification passed. The
  new real-GPU regression passed all 15 channel masks with pixel readback,
  unchanged unselected channels, inherited scissor ignored and output state
  restored. Portable mapping/capture, framebuffer, shared UI and patch-transition
  suites passed: 36 tests. The GPU test is part of the supported build; portable
  clear/capture checks also run in `tsfp.ps1 -Action Test` and patch CI.
- Live check: a bounded 1920x1080 System Link host lobby showed readable pages
  without white fill or stale text, including after changing match options and
  returning to the lobby. Backgrounds remained visible. The unmodified live
  capture now contains three `0x80` alpha clears where the baseline capture
  contained three zero-flag clears. Replaying the new capture succeeded with
  no failed host calls or malformed chunks and showed the corrected panel.
  No crash, unresolved-call or clear-setup error occurred in the live run.
- All save/input/runtime-display/PC-display routes were isolated under
  `private/lobby-validation`; personal settings and saves were preserved.
  Selected before/after captures and screenshots remain ignored there. Six
  disposable partition images were recycled: 5,243,406,848 apparent bytes but
  only 133,632 physically allocated bytes. Profiles and metadata were retained.
- Limits: no remote online peer, new gameplay run, Vulkan GPU run, or live
  default/4K resolution check was performed. Native Mac build/GPU validation and
  other-device synchronization remain unverified; see
  [MAC-007](MAC_PORT_BACKLOG.md#mac-007--screen-copy-effects-and-image-surfaces).

## Game release comparisons and readable titles (2026-10-16)

Branch `fix/game-release-history-titles`, based on main `e18a381`.
The publisher selects the most recently published valid game `v…` tag, including
prereleases, while retaining all release records for duplicate-tag protection.
New game titles use `TSFP v…`; manual launcher titles use `Launcher v…`.
Release notes retain generated PR/contributor content beneath "Changes since"
and omit the redundant "What's Changed" heading, source commit list and
"Build and validation" section. Packaging validation and verification assets
remain unchanged. The same cleanup was applied to published TSFP release notes.
Download instructions use separate launcher and portable ZIP subsections with
numbered steps, explicit update channel and a short requirements paragraph.
Windows x64 / Python 3.14.2: publisher regressions passed 31 tests with four
symlink-prerequisite skips. Coverage includes newer launcher releases, invalid
tags, generated-note baseline selection and preserving the launcher Latest flag.
No game build or runtime test was needed for this metadata change. macOS
execution is unverified; see [MAC-012](MAC_PORT_BACKLOG.md#mac-012--standalone-mac-runtime-distribution).

## Launcher v1.2.7 release preparation (2026-10-16)

Prepared on `release/launcher-v1.2.7` from main `1140c34`, including the banner
preservation fix merged in PR #75 (`f1011f8`). Version and assembly metadata are
1.2.7. The release preserves valid equal-version independent launchers during
game updates, repairs damaged equal versions, and requires artwork for future
public game packaging. Its ZIP contains the themed launcher; a newer launcher
version also replaces any valid fallback v1.2.6 installations affected by alpha.5.
The published game remains v0.2.0-alpha.5; no game package is being rebuilt.

Windows x64 / .NET Framework 4.8 / pinned Python 3.12.14: the required-artwork
build compiled both EXEs and passed 99 updater/bootstrap/cleanup checks and 25
settings checks. Six Python startup/packaging checks passed, including two
missing-artwork rejection subtests. Home, input, graphics and bootstrap splash
previews rendered; Home was visually inspected with banner/logo visible.
Tests used disposable fixtures and launched no actual game or personal settings.
MAC-021 remains queued; native Mac launcher execution is unverified. Publication
is pending hosted checks, a clean exact-main build and public-download plus
previous-client upgrade verification.

## Launcher artwork lost after game update (2026-10-16)

Fix on `fix/launcher-art-preservation`, commit `f1011f8`, based on main `5e5ca61`.
The affected installation had game v0.2.0-alpha.5 from source `d430e8b` and
launcher v1.2.6 binaries matching that game's BUILD.json, but differing from the
published themed launcher. Its application was 278016 bytes; the themed release
contains embedded artwork. Logs show launcher v1.2.6 installed before the alpha.5
game update. `Core.InstallPending` replaced equal-version launchers, and the
self-updater considered the resulting fallback launcher complete and current.

Restored the user's launcher through `SelfUpdate.Unpack` and `Core.ApplyLauncher`
using the verified public v1.2.6 archive (SHA-256
`57381c968a3cf481cb0b1e728c3486935ab697e0b5ba9acce20309b82ece0867`).
The loaded release assembly contains banner/logo resources; installed binary and
manifest hashes match the public release. Game executable, BUILD.json and settings
hashes were unchanged. The launcher reopened normally. An isolated preview of the
same public binaries was visually inspected; no automated game session was run.

The fix preserves a valid equal-version independent launcher during game updates;
a damaged equal version can still be repaired by the game package. Newer versions
continue to install, and game rollback preserves the independent launcher. Public
game packaging now requires artwork; developer/CI fallback builds remain supported.
Windows x64 / .NET Framework 4.8 / Python 3.12.14: 99 updater/bootstrap/cleanup
checks, 25 settings checks, and six startup/packaging Python checks passed. The
new regression exercises equal-version preservation, damaged-copy repair and
rollback with disposable fixtures. Missing-banner and missing-logo build subtests
both reject before compilation. MAC-021 tracks queued Mac release-launcher
adaptation; native Mac execution is unverified. This fix is not published yet.

## Launcher v1.2.6 release preparation (2026-10-16)

Prepared in PR #73 on `release/launcher-v1.2.6`, based on main `293295b` and
merged with main `9c87069` (PR #72 FOV fix). The release includes
PR #71 README/launcher copy cleanup and PR #70 resolution/aspect filters.
Version and assembly metadata are 1.2.6. Release compatibility review found
that the published alpha.4 game accepts heights only through 2160. The launcher
now filters presets and rejects unsupported custom dimensions before any settings
write for alpha.4 and earlier (or an unrecognized version); newer/unpackaged
games retain heights through 4320. Compatible landscape 4K and ultrawide modes
remain available. The game release remains v0.2.0-alpha.4.

Windows x64 / .NET Framework 4.8 / pinned Python 3.12.14: both launcher EXEs
compiled; all 98 updater/bootstrap/cleanup checks, 25 settings checks and five
startup/packaging Python checks passed. Coverage includes atomic rejection of
unsupported legacy resolutions, legacy preset filtering, retained 4K and newer
8K model saves. Home, keyboard/mouse settings, graphics settings and bootstrap
splash previews rendered successfully; Home and graphics were visually inspected.
Tests used disposable fixtures; no actual game session was launched or personal
settings changed. Native Mac launcher execution is unverified; adaptation and
acceptance checks remain queued in MAC-021. Publication is pending exact-main
build, hosted checks and public-download/previous-client verification.
## Falling gameplay FOV (issue #66) — 2026-10-16

Implemented on `66-bug-fov-returns-to-default-when-falling-from-a-ledge`,
based on main `2608580`. Windows x64, MSVC 14.44.35207, D3D11, pinned runtime
and complete patch stack; supported US XBE SHA-256 verified as
`3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc`.

The FOV camera hook accepted grounded player state 2 only. The original US
`sub_00258F50` switches to state 4 on lost ground contact (call at `0x0025954D`),
and `sub_000D4100` restores state 2 on landing. `src/pc_display_menu.c` now
accepts both states for the owned gameplay camera. Existing projection-call,
cinematic and camera-ownership exclusions remain. GPU projection and CPU frustum
continue to scale together without rewriting the guest weapon/zoom lens.

Validation:

- Added grounded/falling/repeated-falling/landing checks at both gameplay camera
  call sites in `tests/cutscene_letterbox.c`. The new regression failed against
  the old hook, then passed with the fix.
- Default and reported 90-degree falling FOV, unchanged guest state/lens,
  non-accumulating projection and excluded terminal/non-gameplay states covered.
- `test_cutscene_letterbox.py`, `test_decal_projection.py` and
  `test_fov_settings.py`: 9 passed, including the lifted original US camera path.
- Complete patch stack verified; `tsfp.ps1 Build` and all build-script checks passed.
- Isolated Windows test game opened at 3440x1440 and 90-degree FOV, with
  separate save/input/display paths, 30-minute limit and capped logs. The user
  confirmed FOV stays consistent through falling and landing on 2026-10-16.
  No native Mac execution performed.
  Shared hook needs no separate Mac adapter; follow-up is recorded in MAC-022.

## README and launcher copy cleanup (2026-10-16)

Copy-only implementation `b9a05b9` on `cleanup/readme-launcher-copy`, based on
main `2608580`.
Removed slogans, game-themed quips and promotional headings from the README,
launcher home screen, default banner, startup splash and runtime status messages.
Visible navigation now uses Home, Game and Updates; stable element IDs and event
handlers remain unchanged. The settings draft hint and launcher guide use Home.
The README screenshot was replaced with the actual updated development UI, with
its source-preview status and executable hash recorded in docs/assets/README.md.
Game branding, artwork, functional instructions and prerequisite notices remain.

Windows x64 / .NET Framework 4.8 / Python 3.12.14: both themed launcher EXEs
compiled. Main, keyboard/mouse settings, graphics settings and startup-splash
preview commands succeeded. A separate build without private banner/logo assets
compiled and its fallback preview rendered successfully. Main and fallback
previews were visually inspected. A source scan found no remaining removed
phrases; the README reference to the game having no jump action is functional
controls documentation and remains. No new tests were added for these text-only
edits; no game build, game execution, update or personal settings changes were
performed. Native Mac UI wording is unverified. Publication is not part of this
change.

## Resolution groups and adaptive UI (issues #27/#31) — 2026-10-16

Implemented on `27-add-aspect-ratio-169-and-more-resolutions`, originally based on main
`4124557`. Windows x64, MSVC 14.44.35207, D3D11, pinned runtime and complete
patch stack, supported US XBE SHA-256
`3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc`.

The guest menu and Windows launcher share 41 common render presets and six
aspect groups, supplemented by reported monitor modes. Bounds are 7680×4320,
33,177,600 pixels and 4:1; saved custom modes and V1/V2 FOV files remain supported.
Aspect selection filters/stages a resolution rather than stretching it. The guest
refresh restores choice indices by row ID, so the new filtered index is reapplied
after refresh to prevent accidental resets. Desktop display modes are unchanged.

Menu backgrounds use the complete output canvas. Text/buttons fit proportionally
on both wide and narrow screens. Single-player ammo groups retain their original
left/right handedness; radar placement starts after world target markers; status
and scope controls receive scoped placement tags. Guest sprite batches flush at
placement changes, and viewport/scissor transforms agree. Split-screen retains
its existing layout. Native Mac shares the catalog and layout rules; its separate
Cocoa aspect selector is deferred in MAC-024 and native execution is unverified.

Health/armor follow-up: the gauges were still using the centred canvas, and
opening a larger submenu increased their authored scale beyond its horizontal
bounds. `sub_0013AB60` now fits the full curve mesh with a four-percent inset.
The primitive-allocation hook separates the three left/right mesh passes before
they are batched, retaining the centred directional hit marker and restoring the
previous placement afterward. Single-player layout is shared with native Mac;
split-screen keeps its previous behavior. A user screenshot at 3440×1440
identified the clipped health/armor curves on Audio/Video Options.

Tutorial follow-up: the original card reserved 40 units for controller icons,
so translated mouse/key labels overlapped the action descriptions. The shared
title hook now measures the current bindings in each row's actual font, uses
the widest label plus padding for all normal action rows, and expands the panel
within the viewport. The original text engine wraps descriptions in the remaining
space and computes their height. A follow-up reduces single-player tutorial
text and glyphs to 90% in both axes, before measurement and wrapping. The font
scale is restored afterward, leaving other HUD/menu text unchanged. Controller
column placement, composite stick rows and split-screen retain their previous behavior; geometry/font/register state is
restored after the card. No literal tutorial strings or generated game C changed.

Shading-seam follow-up: the user capture isolated an animated textured overlay
(draw 327, a 128x128 noise texture over a 640x480 canvas). AUTO placement treated
it as a proportional picture because it was neither flat color nor a framebuffer
copy. The `fullscreen-effects` patch recognizes a complete first quad at the
canvas corners, including inclusive pixel bounds, while retaining proportional
placement for clipped panels and separate corner HUD widgets. It also measures
original fan/quad vertex counts rather than the expanded triangle-list count,
avoiding an out-of-bounds read during placement classification. Explicit placement
scopes still take precedence. The effect is preserved across the full output.

Edge-caption follow-up: the mission intro's date/title (`sub_0013FC50`) and
single-player tutorial cards (`sub_001476F0`) now use the same LEFT placement
as edge HUD widgets on wide displays. Each scope flushes existing sprite batches
before drawing, keeps the full text/outline or panel/key-label group together,
and restores previous placement afterward. Authored insets, vertical positions,
animations, tutorial 90% font sizing and measured columns are retained. Original
4:3/narrow and split-screen placement remains unchanged. The title hooks are
shared with native Mac, whose runtime validation remains pending.

Validation so far:

- PR #70 CI follow-up: Windows run `38981957451` passed patch tracking,
  application and UI checks, then failed compiling the standalone FOV fixture.
  Its production menu include now also compiles tutorial hooks, requiring the
  runtime input-header directory and five guest drawing functions plus the prompt
  device query. The fixture now supplies that include path and fail-fast stubs
  for the unrelated tutorial paths; production runtime behavior is unchanged.
  The exact failing display/FOV step and shared UI suite pass locally (7 cases).
  The original macOS CI job passed its shared tests; this does not establish
  native game compilation or visual support. The correction only changes the
  Windows fixture and requires no Mac adapter change.

- Main integration: feature commit `2d69459` was combined with main `9e08fa3`.
  Both branches wrapped `sub_0013AB60`; a subsequent reproducible patch now
  routes Objectives fitting and ordinary health draws through the shared
  placement helper exactly once, preserving the upstream panel and split-view
  sizing. The combined Windows Release build and all eight build checks pass;
  twenty-six Objectives/UI/patch-transition cases pass. The launcher rebuild
  under Windows PowerShell 5.1 passes 98 regressions and 24 settings checks.
  Logs: `build/issue-27-display/build-main-integration.log`, `merged-tests.log`
  and `launcher-main-integration.log`. Earlier isolated visual captures below
  precede this integration; the combined build has automated validation only.
  Native Mac compilation/playtesting and actual 8K GPU allocation remain unrun.

- Edge captions: the Release rebuild and all eight build checks pass, as do all
  nineteen UI/patch-transition cases. A bounded isolated 3440x1440 Windows run
  reached first-mission gameplay. Reviewed `caption-edge-0.png` and
  `tutorial-edge-2.png` in `build/issue-27-display`: the date/title stays at the
  lower-left inset, while the complete weapon tutorial panel occupies the upper
  left with separated key/action columns, wrapped text and a clear central view.
  Render tags confirm LEFT placement; the mission caption spans x=20..155.6 in
  the guest canvas. Controller/narrow/split guards and placement restoration are
  covered by shared tests; native Mac visual validation remains pending.

- Shading seam: the user's frame 1540 (350 draws) replays successfully at
  3440x1440 with zero malformed/unmapped/failed commands. Stretching only draw
  327 removes the boundary; the fixed renderer matches that diagnostic image
  within one color value in 121 channels. Mission text retains its position and
  proportions. Selected evidence: `build/issue-27-display/seam-before.png` and
  `seam-fixed.png`; capture/checksums are recorded in `seam-validation.json`.
  The Release build, all eight build checks, five shared UI cases and fourteen
  patch-transition cases pass. The historical-controls fixture initially failed
  because it included this display-dependent patch before display support; its
  old-state patch selection was corrected and the affected test passed on rerun.
  The user confirmed the transparent rectangle is gone in the rebuilt isolated
  game during the first-mission opening at 3440x1440. Native Mac validation remains
  pending.

- Tutorial follow-up: all eight Release build checks, four shared UI cases
  (including production tutorial hooks) and fourteen patch-transition cases pass.
  The isolated first-mission run at 3440×1440 shows distinct key/action columns
  and a fully contained two-line fire-mode description. Reviewed capture:
  `build/issue-27-display/tutorial-fixed-3440x1440.png`. Longer rebound labels,
  bounded panel growth, wrapping space, original controller/split-screen geometry
  and guest stack/font/argument restoration are covered by the regression fixture.
  Temporary prompt diagnostics were removed. The subsequent 90% tutorial-size
  adjustment passes all eight build checks and four UI cases, including scoped
  scale restoration; its
  smaller appearance awaits user review. Native Mac remains unverified.

- Health/armor follow-up: Release compilation succeeded. Four UI/FOV cases
  and fourteen patch upgrade/transition cases passed. Seven of eight build
  checks passed; the virtual-controller prompt fixture twice failed its
  `GetForegroundWindow` acceptance assertion (line 55). Input code was unchanged;
  those intermediate runs were not all-green. The subsequent tutorial rebuild
  passed all eight checks without changing input code.
- A bounded isolated 3440×1440 run using the existing input-state test interface
  reached first-mission gameplay, pause Options and Audio/Video Options. Reviewed
  complete, separate left/right gauges with clear panel spacing in
  `health-pause-3440x1440.png` and `health-audio-video-3440x1440.png` under
  `build/issue-27-display`. Recorded mesh bounds are 25.6–217.0 on the left and
  423.0–614.4 on the right in the 640-unit guest canvas. Return to gameplay
  retained the centred crosshair and right-edge ammo. Earlier timed attempts
  stopped at the briefing/profile editor and are not gameplay evidence.
  Human confirmation of the revised gauges and native Mac validation remain.
- Windows game Release build and its eight native/build checks pass after the
  final picker correction. One intermediate run hit the existing foreground-focus
  prompt-test assertion; subsequent full runs passed without input-code changes.
- Windows PowerShell 5.1 launcher build: 71 launcher regressions and 22 settings
  checks passed. WPF coverage includes aspect filtering, current-ratio reporting,
  custom modes, selection/save/reload and the existing FOV slider. Reviewed the
  graphics preview in `build/issue-27-display/launcher.png`.
- Eight targeted cases passed for display discovery, FOV/menu behavior, cinematic
  framing and framebuffer sampling. Three UI cases pass: production widget scopes,
  verified hook sites, and extracted production viewport/scissor code across seven
  sizes and all anchors. Radar world markers remain outside the corner scope.
- Fourteen complete patch upgrade/transition cases pass, including historical
  controls and texture-branch round trips. The historical fixture excludes these
  later UI patches until their display dependencies are installed.
- Initial 1280×720 actual menu capture confirms a full-width animated background,
  proportional text/panel/FOV slider and a functioning aspect selector. An
  86-second recorded-input guest run then filtered 16:9 modes, confirmed 960×540,
  previewed 1280×720, timed out back to 960×540 and returned to the original menu.
- A fresh isolated 135-second first-mission run at 2560×1080 reached gameplay.
  Reviewed the right-edge ammo counter, centred crosshair and pause Options panel;
  recorder tags confirm the ammo group uses RIGHT and the next menu returns AUTO.
  The first attempt used an existing test profile and stopped at its name editor;
  only the fresh-save run is gameplay evidence. Its tutorial key-label overlap
  prompted the separate measured-column correction described above.
  Radar and sniper scope placement still need a representative manual playtest.
- Reviewed final 1280×1024 (5:4) and 2560×720 (32:9) menu captures: full-width
  animated backgrounds, proportional text/buttons/FOV slider, and unclipped panels.
  Executable provenance and selected evidence are recorded in
  `build/issue-27-display/validation.json`. Actual 8K GPU allocation and native Mac
  execution were not tested. An isolated 1920×1080 build is prepared for user
  radar/sniper and resolution-switching playtests; no personal settings were changed.
## Launcher v1.2.5 release preparation (2026-10-16)

The user requested stable publication of the game-version cleanup in PR #69.
Version and assembly metadata are bumped to 1.2.5. The cleanup implementation is
unchanged from validated `6e49e0a`; the release requires CI on the new revision,
a clean build from its main merge commit, public checksum verification and the
previous v1.2.4 client compatibility check. This is a launcher-only release; the
game remains v0.2.0-alpha.4. Native Mac launcher execution is unverified.

## Game version cleanup (2026-10-16)

Implementation commit `d1f4e42` on `fix/game-update-cleanup`, based on main
`2b3ada8`.
The previous game snapshot is kept until a session has shown a game window and
exited with code zero. Installed runtime files must still match BUILD.json before
cleanup; personal controls and independently updated launcher files are excluded
from game checksums. Confirmation is bound to the manifest hash, so a successful
old version does not authorize deletion of a newer version's recovery copy.
Incomplete installations, failed starts and unfinished transactions keep recovery.
The existing UI window-observation/exit path supplies the observation; this does
not claim verified gameplay or full rendering correctness.

Confirmed cleanup removes the previous game snapshot and obsolete/incomplete
staging only. A retired snapshot is renamed before recursive deletion, so a
partially locked copy is not offered as a usable rollback; startup retries handle
it for the same confirmed build. Cleanup is serialized with other launcher work
and runs off the UI thread. Runtime installation/rollback still refuse while a
game runs. ISOs in managed folders, linked paths and personal backup directories
block deletion. Root saves, settings, ISO, Data and Textures remain untouched.
Newer/unknown staged releases are retained. Unrelated/manual game folders are not
cleanup candidates.

During an update, unchanged old manifest-owned files absent from the new manifest
are removed through the existing transaction journal; rollback restores them.
Modified and untracked files, all personal settings and independent launcher files
remain. Missing/invalid old manifests skip optional pruning. The restore button
tooltip explains the retention boundary.

Windows x64 / .NET Framework 4.8 / Python 3.12.14: local synthetic validation passed
98 updater/bootstrap/cleanup checks, 23 settings checks and five startup/packaging
checks. Ten focused game cleanup cases cover confirmation, changed/corrupt builds,
transactions, staged versions, ISO/personal backup protection, locked-file retry,
junction refusal and safe staging replacement. Two full updater cases cover
obsolete file removal/rollback, modified/untracked file preservation, and ISO
protection in the old snapshot. One settings run encountered a transient Windows
file-replacement IOException; its isolated rerun passed all 23 checks. The final
build passed all 98 updater and 23 settings checks.
Windows CI on `6e49e0a` passed all 98 updater checks and 23 settings checks;
the Mac shared-check job also passed. Tests use tiny synthetic packages and fake
ISO markers only; no real game data, personal saves, or installed runtime were
changed and no actual game session was launched. MAC-021 adaptation is queued;
native Mac execution and other-device synchronization remain unverified.

## Launcher v1.2.4 release preparation (2026-10-16)

Version and assembly metadata are bumped to 1.2.4 for stable launcher-only
publication from main through PR #68. The cleanup implementation is unchanged
from validated `a7246b5`; publication requires the new revision to pass CI and
checks of the public assets and previous v1.2.3 update path. The published game
remains alpha.4.

## Launcher update cleanup (2026-10-16)

Implementation commit `dc019bf` on `fix/launcher-update-cleanup`, based on
main `b1860c6`.
The release launcher previously retained one old launcher snapshot and its helper
copies; snapshots were replaced at each update, rather than accumulating an
unlimited history. Successful normal main-window startup now verifies the current
launcher and removes that prior snapshot, obsolete/incomplete launcher staging,
and helpers no longer needed by an update. Failed preparation cleans its staging
without replacing the original failure. Cleanup errors are logged and retried
on subsequent startups. No cleanup runs during developer render previews.

Recovery transactions and unverified installations prevent cleanup. Newer staged
launcher releases and unknown staging markers remain available; pending game work
keeps its helper. The game rollback, installed game, settings, saves, data,
preferences, logs and unrelated downloads remain untouched. Cleanup rejects
linked paths through the existing checked launcher-owned deletion API.

Windows x64 / .NET Framework 4.8 / Python 3.12.14: both launcher binaries compiled,
nine isolated cleanup checks and all 23 settings checks passed. Cleanup checks
include three consecutive upgrades, protected files, damaged installation,
unfinished transactions, semantic version comparisons, newer/unknown staging,
locked files with retry, pending game work, and a real directory junction.
Five startup/packaging checks also passed. Windows CI on `a7246b5` passed all
86 updater/bootstrap/cleanup checks and 23 settings checks, including checksum,
invalid-archive and cancellation preparation failures. The Mac CI job also
passed its shared checks. Full local updater validation was blocked by an active game;
that session and the installed launcher were preserved. No game build/playtest
is required for this Windows launcher change. MAC-021 adaptation remains queued;
native Mac execution and other-device synchronization are unverified.

## Bootstrap argument diagnostics (2026-10-16)

Launcher v1.2.3 release preparation on `fix/launcher-bootstrap-arguments`, based on
main `de23272`. The installed launcher log records `Unknown bootstrap arguments`
at 2026-10-16 21:45:56 UTC (17:45:56 local). The resolution-development task ran
`TimeSplitters Launcher.exe --render-settings-graphics ...`, although that flag
was only accepted by `Bin/TimeSplitters.App.exe`. Its subsequent command used
the correct EXE. This establishes a concrete trigger for the recorded failure;
the old log did not retain actual arguments for distinguishing other reports.
Both installed launcher files matched their local v1.2.2 manifest. They were
local development builds, not byte-identical to the published v1.2.2 assets.

`Bootstrap.cs` now delegates only the three known application preview modes
before taking the startup mutex, preserves relative/spaced output paths and
child exit codes, and bounds preview execution to 30 seconds. Normal startup,
startup-splash rendering and updater routes retain their behavior. Other
arguments remain rejected, with bounded JSON-escaped argument details in both
the log and error. Startup logging now includes version/count/arguments.

Windows x64 / .NET Framework 4.8 / Python 3.12.14: both binaries compiled. Three
isolated bootstrap checks passed using real synthetic child processes, covering
all forwarded modes, quoting, relative paths, exit status, preserved startup and
helper command routes, and bounded/escaped diagnostics. All 23 settings checks
passed. Real bootstrap execution rendered both the delegated graphics preview
and its own splash successfully into ignored `build/bootstrap-validation`.
The full updater suite stopped at the existing game-running safety guard; the
active game session and installed launcher/user settings were left untouched.
No game runtime source changed, and no new game build/playtest was performed.
Publication is pending the release PR and hosted Windows updater checks.
Compatibility review found that published alpha.4 reads only V1 display settings.
The newer main-branch FOV controls are disabled for installed releases through
alpha.4, and model validation rejects custom FOV before writing any settings.
Two additional checks cover compatible resolution saves, atomic rejection, the
newer-version path, and disabled controls with explanatory text. Native Mac adaptation remains queued under
MAC-021; other-device synchronization is unverified.

## Launcher settings CI startup budget — 2026-10-16

- PR #65's Windows run `38969601167` passed patch transitions and all 71 launcher
  updater checks, then stopped in the first FOV WPF UI check at the ten-second
  STA-thread deadline. Local runs passed, including the CI fallback without
  private banner/logo resources. This is a test-harness change, not game behavior.
- `tests/launcher_settings.cs` now allows up to 60 seconds for a UI worker,
  reports a timeout distinctly, preserves the original worker exception stack,
  and prints each check's name and duration. Cold initialization remains bounded;
  UI assertions and save/cancel checks are retained.
- Windows x64 local `tools/build-launcher.ps1 -Test` passed 71 updater and 21
  settings checks. Hosted Windows validation of the updated commit is pending.
  The previous PR revision's macOS patch-transition job passed; this WPF-only
  harness has no Mac adapter impact. No game data or personal settings were used.

## Configurable gameplay FOV (issue #29) — 2026-10-16

Implemented on `29-fov-support-in-video-settings`, starting from
main `ef17e2a`. Windows x64, MSVC 14.44.35207, the pinned runtime and
complete verified patch stack; supported US XBE SHA-256
`3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc`.
The shared display model now retains a vertical FOV preference (40–100°, default
55°), reading legacy display files and the explicit V2 format. The guest Display
settings menu and Windows launcher graphics tab expose sliders with numeric
values, reset and existing save/cancel behavior. Combined resolution previews
revert both fields. Gameplay projection and CPU culling use the same tangent
scale without changing the original weapon zoom state. Scripted cinematic and
terminal cameras keep their authored lens.

Validation on Windows:

- Six targeted pytest cases passed: shared cinematic/camera production wrapper
  tests (including mutation checks and the original US cinematic routines),
  display mode discovery, and production FOV settings/menu callbacks. The latter
  covers old/new files, range limits, slider drawing/input, unsaved drafts,
  reset, read-only save failure, keep and resolution timeout rollback.
- Windows launcher built with Windows PowerShell 5.1/.NET Framework; all 71
  launcher regressions and 21 settings checks passed. The added checks exercise
  the WPF slider, numeric label, reset, cancel/save, legacy defaults and V2
  persistence. Reviewed `build/issue-29-fov/launcher.png` from its preview renderer.
- MSVC Release game and native test targets compiled and linked successfully.
  Network, texture format, display, save paths, PC controls, settings and debug
  checks passed. The build runner stopped at the existing controller prompt
  fixture's foreground-window assertion (`tests/pc_prompts.c:55`); the same
  assertion failed when rerun directly. No input/prompt implementation changed.
  The remaining checks were run individually, and executable identity was sealed.
- A bounded 65-second isolated game run navigated the actual guest menu with
  recorded input, moved the slider 55 → 58, and saved
  `TSFP_DISPLAY_V2 1280 720 58`. Reviewed the menu capture and found both the
  number and original-style slider inside the panel. Evidence is in
  `build/issue-29-fov/menu-{47,55,62}.png` and `menu.err`.

- First-mission gameplay with isolated 1280×720 / 75° settings produced a
  vertical CPU frustum of `0.76732695` (tan(75°/2)) and projection Y coefficient
  `1.30322552` (its reciprocal), while the original game zoom field stayed 55°.
  This confirms the requested lens reaches the real projection and culling
  paths. The diagnostic run collected 117.25 seconds before its screenshot
  watcher hit a rotating-file race and stopped its owned game in `finally`;
  the game had not crashed. Retained evidence: `gameplay-camera.json`,
  `gameplay-result.json`, `gameplay-final.png` under `build/issue-29-fov`.

The user confirmed that the FOV slider works in the rebuilt game during a
separate isolated playtest (155 seconds, normal exit).

Personal saves and settings were not used. Shared/native Mac integration is
source-reviewed only; Mac build, physical controller navigation, all weapons
and split-screen gameplay are unverified. See MAC-022 in `MAC_PORT_BACKLOG.md`.

## Terminal mouse routing (issue #7) — 2026-10-16

Implemented on `7-breaking-and-entering-camera-turret-not-working`, based on
main `696421f` merged at `093e30f`. On Windows x64, a bounded isolated run
of the supported US XBE reproduced the Breaking and Entering terminal failure:
player mode was `0x2000`; physical mouse movement changed hidden player angles
while the active console pointer stayed fixed. The user confirmed the terminal
menu pointer, rather than a selected camera view, was blocked. Disassembly of
US `sub_003700D0` identifies local-index console ownership, pointer coordinates,
4–251 bounds, inactivity reset, scripted input and pause gates.

The shared `src/pc_terminal.h` now routes counts to that pointer and consumes
terminal input without changing player angles, including invalid transitions.
Coordinates stay in the game's logical space, independent of screen resolution,
FOV or aiming inversion. Windows and SDL adapters use the same helper.
The guarded-memory regression and full MSVC x64 Release build passed on Windows,
including patch-stack validation and the real Windows adapter regression in
`tests/pc_controls.c`. The user confirmed physical pointer movement, but left-click initially still
sent Fire instead of the terminal's Confirm action. The follow-up shared input
context and `zzzzzzzzzzzzzzzzz-terminal-mouse-select.patch` now suppress the
normal left-button binding while selecting and emit A/Confirm. A held button
must be released across either transition to prevent accidental selection or
firing. Windows regression covers the native adapter-to-binding path with bounded synthetic input; the shared
fixture covers click state, and the native SDL fixture has equivalent checks.
The updated MSVC x64 Release build and native checks passed, including Confirm
without Fire and release-before-fire on terminal exit. All 27 patch tracking/
transition cases passed (the historical-upgrade fixture was updated to exclude
this new dependent overlay, then rerun). The shared terminal regression passed.
The user confirmed the final physical playtest: pointer movement, left-click
camera selection, and ordinary aiming/firing after leaving the terminal work.
The read-only capture also records return to mode `0x2` with no active terminal.
Native Mac compilation/execution remain
unverified; see MAC-001.

## Terminal mouse routing (issue #7) — 2026-10-16

Implemented on `7-breaking-and-entering-camera-turret-not-working`, based on
main `696421f` merged at `093e30f`. On Windows x64, a bounded isolated run
of the supported US XBE reproduced the Breaking and Entering terminal failure:
player mode was `0x2000`; physical mouse movement changed hidden player angles
while the active console pointer stayed fixed. The user confirmed the terminal
menu pointer, rather than a selected camera view, was blocked. Disassembly of
US `sub_003700D0` identifies local-index console ownership, pointer coordinates,
4–251 bounds, inactivity reset, scripted input and pause gates.

The shared `src/pc_terminal.h` now routes counts to that pointer and consumes
terminal input without changing player angles, including invalid transitions.
Coordinates stay in the game's logical space, independent of screen resolution,
FOV or aiming inversion. Windows and SDL adapters use the same helper.
The guarded-memory regression and full MSVC x64 Release build passed on Windows,
including patch-stack validation and the real Windows adapter regression in
`tests/pc_controls.c`. The user confirmed physical pointer movement, but left-click initially still
sent Fire instead of the terminal's Confirm action. The follow-up shared input
context and `zzzzzzzzzzzzzzzzz-terminal-mouse-select.patch` now suppress the
normal left-button binding while selecting and emit A/Confirm. A held button
must be released across either transition to prevent accidental selection or
firing. Windows regression covers the native adapter-to-binding path with bounded synthetic input; the shared
fixture covers click state, and the native SDL fixture has equivalent checks.
The updated MSVC x64 Release build and native checks passed, including Confirm
without Fire and release-before-fire on terminal exit. All 27 patch tracking/
transition cases passed (the historical-upgrade fixture was updated to exclude
this new dependent overlay, then rerun). The shared terminal regression passed.
The user confirmed the final physical playtest: pointer movement, left-click
camera selection, and ordinary aiming/firing after leaving the terminal work.
The read-only capture also records return to mode `0x2` with no active terminal.
Native Mac compilation/execution remain
unverified; see MAC-001.

## Issue 19 decal projection (2026-10-16)

- Source: decal fix on `19-floating-decals` (this revision), based on
  `696421fd080a5bd57cb894b76302080f59f52485`; pinned runtime `d90c929` and
  checksum-verified US XBE. Windows x64 build 26300, MSVC 19.42.34435.0,
  Python 3.14.2, CMake 4.4.4, D3D11.
  Release executable SHA-256:
  `ada6ab457f3369c64c89d3a46f4d6ed29a698e5a3861364d876b6076e0e6048d`;
  multiplayer identity `85180d72`.
- Cause: the two decal render paths independently reconstruct a depth-biased
  perspective matrix, bypassing the adjusted camera. A baseline 1920x1080
  Arcade capture used horizontal scale `1.440737` for biased effects versus
  `1.080552` for the widened scene. The fixed capture uses `1.080552` for
  both, retaining the distinct depth terms `-0.10050558` and `-0.10000556`.
  This corrects the guest projection rather than changing HUD classification.
- Validation: the complete patch stack applies; supported `tsfp.ps1 -Action
  Build` regeneration/build and all native build tests passed. Executable seal
  and game-content verification passed. The decal, cinematic and patch-transition
  suites passed (35 cases; one historical fixture was updated and its failing
  case rerun). Wrapper checks cover both decal callers, default/wide/ultrawide
  and cinematic apertures, unchanged depth and other callers, missing camera,
  guest return state, and both Windows/native display build flags.
- Live check: a bounded Hotel Arcade match at 1920x1080 with no bots reached
  gameplay; fired wall impacts stayed aligned while strafing and rotating the
  camera. No crash or unresolved-call report occurred. All save, input and both
  display configuration routes were isolated under `private/decal-validation`.
  Selected screenshots and captures remain ignored there; disposable partition
  images were recycled after the runs. Personal settings and saves were preserved.
- Remaining checks: other Arcade maps, explosion impacts, split screen, and live
  default/4:3/ultrawide rendering were not exercised. Mac source uses the same
  wrapper, but native build/rendering and other-device synchronization remain
  unverified. See [MAC-010](MAC_PORT_BACKLOG.md#mac-010--in-game-resolution-preview-and-rollback).

## Resolution release preparation — October 16, 2026

Game v0.2.0-alpha.4 and launcher v1.2.0 are prepared together on
`release/resolution-v0.2.0-alpha.4`, based on main `92384df`. This integrates
launcher settings commits `caa6f5e`/`3b93941` and issue #28 fix `e3e99c1`
without replacing newer main runtime/Online/audio/rendering changes. The release
package includes both launcher executables and their self-update manifest,
alongside the current portable Online companion, licenses and source bundle.

Earlier issue-branch validation passed 71 updater and 14 settings checks, native
display tests and an 86-second real D3D11 guest-menu replay covering left/right
selection, 720×480 apply/keep/save, Original preview, timed rollback and return.
Both native value-column and WPF graphics-tab captures were reviewed. The US XBE
hash was verified; those earlier tests used an isolated relink. The main-based
full MSVC x64 Release build and native checks subsequently passed, along with
138 Python tests (5 platform/prerequisite skips), 2 launcher packaging tests,
71 updater checks, 14 settings checks and 3 launcher lifecycle checks.
The full executable passed the same isolated 86-second D3D11 resolution replay:
720×480 apply/keep/save, Original preview, 15-second rollback and menu return.
Fresh disposable saves and explicit input/display/runtime paths were used.
Both GitHub patch-transition jobs (Windows and macOS) passed on `647a42a`;
these source-level Mac checks do not establish native Mac game execution.
Publication follows `Publish-Release.ps1` on clean main, including a fresh
package manifest/checksum and portable Online validation. Final artifact/source
hashes and publication records stay under ignored `build/releases`.
The first preparation stopped on pytest's retained `*current` aliases.
Publisher cleanup now validates and unlinks only those direct sibling aliases
inside its own test tree; targets and arbitrary/external/nested links remain
protected. Passed per-test fixtures are discarded by pytest so intentional
hostile-link test cases do not remain in the cleanup tree. Regression fixtures
cover accepted/rejected aliases and a real nested pytest session with retained
session-cache targets. The earlier diagnostic fixtures are preserved separately.
Native Mac execution and paused-Options gameplay traversal remain unverified for
this change; adaptation is tracked in MAC-010 and MAC-021.

## Release regression compiler discovery — 2026-10-16

- Source base `cc35200`, changes on `fix-release-msvc-tests`.
  Host: Windows x64, CPython 3.14.2, Visual Studio 2022 Build Tools / MSVC
  19.42.34435.0. No game or renderer behavior changed.
- The old `vswhere` queries excluded standalone Build Tools. Audio discovery
  also preferred a stale mixed-case `Path` entry after reading uppercase `PATH`.
  `tests/native_compiler.py` now includes all Visual Studio products, loads
  `vcvars64.bat`, normalizes Windows environment names, and shares the resulting
  compiler/headers/libraries across audio, cutscene, framebuffer and lifted-float
  regressions. A ready x64 developer shell is preserved; POSIX retains `CC`/`cc`.
- The 40 affected native regressions and four discovery checks passed from an
  ordinary PowerShell environment. Native checks include the original supported
  US cutscene/pickup/controller routines; the tests verify the executable hash.
  Complete patch-stack validation passed with zero runtime files changed.
- The complete release Python suite subsequently passed from ordinary
  PowerShell: **129 passed, 9 skipped in 116.38 seconds**. Test output is local
  at `logs/release-msvc-validation.log`; disposable fixtures were isolated under
  `build/release-validation-tests`. No existing settings or saves were used.
- Native/full-suite fixture trees were recycled. Recycling the small
  `build/compiler-discovery-tests` directory was denied/unsupported, so it was
  retained; no alternative deletion or permission change was attempted.
- No package, tag, release, main-branch merge, or publication was performed.
  Mac execution and synchronization with the other device remain unverified;
  see [MAC-012](MAC_PORT_BACKLOG.md#mac-012--standalone-mac-runtime-distribution).

## Adaptive cutscene letterboxing (issue #33) — 2026-10-16

- Source base `5c9effd`, branch `33-remove-the-letter-boxes-in-cut-scenes`.
  The requested behavior is adaptive cinematic bars, rather than removal.
  Story routine `0x0006F150` draws its top/bottom matte at return PCs
  `0x0006F322`/`0x0006F34B`; scripted scene routine `0x001396B0` uses
  `0x0013970E`/`0x0013972E`. All four call solid rectangle routine `0x0001EFB0`.
- Added a reproducible manual wrapper and shared `src/pc_cutscene.h` geometry.
  Preserve the aperture of the original 4:3 presentation (approximately 1.88:1,
  with the game's original integer rounding). Adjust bar height for the selected
  output aspect and retain the original animation fraction. 4:3/legacy output
  keeps its original layout; wider-than-cinematic output uses side bars.
  This changes only identified bar rectangles and restores their guest stack
  arguments after drawing. Subtitles, full-screen fades, menu rectangles,
  scopes and prerecorded startup videos retain their existing paths.
- The first live test exposed orange scenery around the opening space backdrop.
  Capture frame 9140 reproduces this: draw 37 is the authored 3D starfield plane;
  the widened camera looks past its edges. Replaying with camera projection
  constants scaled to fit the cinematic aperture eliminates the exposed scenery.
  `pc_display_camera` now adjusts both projection axes and CPU frustum extents
  along with the matte, retaining normal gameplay widening outside cutscenes.
  Source-only checks cover story, no-matte special mode, animated scenes,
  4:3 and ultrawide framing, and detect removing the camera correction.
- The Windows Release build and all build-script native checks passed.
  All 27 patch tracking/upgrade checks and four adaptive-letterbox checks passed.
  The focused suite includes mutations that remove either bar adaptation or
  camera correction, and detects both regressions.
  The new regression compiles the actual wrapper, checks bar anchoring and
  unchanged unrelated rectangle arguments/ABI, and detects bypassing the filter.
  A private-XBE test executes both original routines across legacy, 4:3, 16:9,
  16:10 and ultrawide modes plus three animation phases, comparing original and
  adaptive draws, fade rectangles, subtitle invocations and render-state calls.
  It uses disposable guest RAM and skips in public CI without game data.
- Host: Windows x64, MSVC 14.44.35207, SDK 10.0.26100.0; pinned runtime
  `d90c929`, supported US XBE SHA-256 verified. Executable SHA-256:
  `b42b8511c2abd4df6314fe7059fcb574993ee78e856e659ac8c5a0787b6eaca4`.
  Evidence: ignored `logs/issue-33-build.log`, `logs/issue-33-tests.log` and
  `build/issue-33-letterbox`. In an isolated 1280x720 Windows run, the user
  confirmed the first mission space scene now looks correct. That run lasted
  48.73 seconds and exited successfully. The original diagnostic capture and
  corrected replay are retained; no second capture of the corrected live space
  shot was received. Live ultrawide and other scripted scenes remain unverified.
  Mac execution remains unverified (MAC-020). Personal settings and saves are
  preserved. Source changes are on `33-remove-the-letter-boxes-in-cut-scenes`.

- PR #37 CI follow-up: macOS Clang rejected the test fixture's unsigned
  `g_fp_top`, which conflicts with the runtime header's signed `int` declaration.
  Corrected the fixture type and made MSVC C4142 fatal in the focused test
  compiler. The stricter Windows check reproduced the original failure, then
  all four cutscene tests passed after correction (4.88 s). Production game
  code and the validated executable are unchanged. Hosted CI is pending rerun.

## Khallos Express see-through scenery (issue #9) — 2026-10-16

- Reproduced the transparent train walls and weapon in an isolated Windows
  Khallos Express session. Captured frame 3376 samples a framebuffer texture
  for background blur at draws 142–143, then samples that same texture again
  for additive glow at draws 462–463, after drawing the train and weapon.
  The HLE refreshed this texture only once per swap, so the final effect
  painted the earlier background over otherwise solid geometry.
- Added `patches/zzzzzzz-framebuffer-sampling-passes.patch`. Switching away
  from a framebuffer texture invalidates its cached copy. Rebinding it captures
  the scene including intervening geometry; consecutive taps retain a stable
  snapshot. Frame-zero initialization, changed texture identity and failed-copy
  retry are also handled. This is shared HLE logic, with no generated game C edits.
- `tests/framebuffer_sampling.c` executes the actual texture adapter with a
  fake copy device, through its texture-binding path. Tests cover the two-pass
  sequence, stable adjacent taps, a new frame, unbinding on another sampler,
  changed backing data and copy failure/retry. Deliberately removing the binding
  invalidation makes the regression fail. Added the test to `tsfp.ps1 Test` and
  CI on supported renderer hosts; Linux skips this Windows/macOS HLE test.
- Full Windows `tsfp.ps1 Build` and its native checks passed. All 29 patch
  tracking/upgrade and framebuffer regression checks passed in 93.29 seconds;
  repeated complete-stack application changed zero files. The historical upgrade
  fixture excludes this later patch until the complete-stack upgrade step.
- macOS CI subsequently found a test-only signature mismatch: the mock
  `host_CubeLockRect` had six parameters instead of the declared four. Corrected
  the mock and promoted MSVC parameter-mismatch warnings C4028/C4029 to errors.
  Both Windows framebuffer checks pass with the stricter compiler flags;
  restoring the old signature now fails compilation with C4029 on Windows too.
  The next macOS CI run exposed a missing `GetTickCount` link dependency in the
  standalone mock; its diagnostic clock now uses a deterministic test function
  on both hosts, without linking the operating-system compatibility layer.
  This changes the test harness only, not the runtime fix or game binary.
- The user confirmed **walls now stay solid** during movement/turning in the
  rebuilt game. Reviewed the resulting train-interior screenshot: opaque walls
  and weapon, with the window still visible. The isolated fixed run lasted
  88.11 seconds and exited with code 0. No full-level completion or other-level
  visual regression sweep was performed; Mac execution remains unverified.
- Source base `800687d`; fix delivered on branch
  `9-khallos-express-level-is-transparent`. Windows x64 build 26300,
  MSVC 14.44.35207, SDK 10.0.26100.0,
  SDL 3.4.16, Direct3D 11, NVIDIA RTX 4070 SUPER driver 32.0.16.1742. Upstream
  `d90c929`; supported US XBE SHA-256 verified. Build executable SHA-256:
  `ad897ef0e868de013a28eaf5515e86ece7c182470959a9a027babba48ac061df`.
- Ignored evidence: `build/issue-9-render/{before,after}.bmp`, two adjacent
  `sequence_0337*.d3dcap` reference frames, build/test logs and `validation.json`.
  Replay alone did not reproduce the live ghosting because the capture format
  omits GPU screen-copy contents; live screenshots and playtesting establish the
  visual result. Test saves/configs were isolated under
  `private/test-saves/pickup-range`; personal settings and saves were untouched.
  Redundant captures and disposable test partitions were removed. See MAC-019.

## Temporary diagnostic-output comparison — 2026-10-16

- User confirms the angle improvement but persistent remote strafe lag, with
  FPS dips about every five seconds even while stationary. The cause remains
  unresolved. Added an opt-in Windows quiet experiment on the existing issue
  branch; no personal debug preferences or gameplay clocks are changed.
- `Play-Quiet.cmd` sets `TSFP_DEBUG_QUIET=1` for its child launch only. The
  Windows entry point routes stdout/stderr to `NUL`, the kernel logger skips
  its separate `xbox_kernel.log`, and Debug skips log polling/performance-log
  writes. FPS sampling and overlay options remain active. `tools/play.py` also
  avoids creating play logs in this mode. A normal launch restores reporting.
  Internal periodic report formatting still runs; this isolates reporting I/O
  and polling rather than eliminating all diagnostic computation.
- Sources are `src/pc_debug_output.h`, `src/pc_debug.c`, `tests/pc_debug.c`,
  `tools/play.py`, `Play-Quiet.cmd` and the separate reproducible
  `patches/zzzzzzzzzzzzzzzzz-pc-debug-quiet.patch`. The entry-point define is
  source-specific so generated game C does not need unrelated recompilation.
  MAC-013 records the deferred native startup adaptation.
- Supported Windows Release build completed all native build-script checks
  after earlier attempts hit the existing foreground-window prompt assertion.
  The affected Debug regression additionally verifies suppressed log reads,
  continuing FPS updates, unchanged persisted options and restored polling.
  Windows x64 build 26300, Direct3D 11, pinned upstream and verified US XBE;
  build identity `e73e10e8`, executable SHA-256
  `df66f4ef4a3da023926263310e739c616dd60daf24e4e7e374fe18d108545c72`.
  Evidence: `logs/issue-20-quiet-final-build.log` and
  `logs/issue-20-quiet-debug-test.log`. Final patch-upgrade/transition suite:
  14 passed in 76.18 s (`logs/issue-20-quiet-patch-tests.log`). The preceding
  full suite had 37 passes and one temporary-file PermissionError in a patch
  transition case; the final patch suite covers that case successfully.
- An isolated 42-second quiet startup/menu run produced zero captured stdout
  bytes and zero stderr lines; the existing kernel log was not changed. A fresh
  22-second normal process restored diagnostic output without changing options.
  Both runs used disposable controls and explicit save/runtime-display/PC-display
  paths. These are startup/menu checks, not evidence that live multiplayer FPS
  dips are fixed. Evidence: ignored
  `private/network-validation/issue20-quiet-validation.json`,
  `issue20-quiet-profile-pipe.json` and `issue20-restored-profile-pipe.json`.
- Personal controls/display and the pre-existing tracked input-fixture edit
  and its backup retain their pre-task hashes. They were not repaired or used
  as mutable automated-test settings. Changes remain local and uncommitted;
  native Mac execution and other-device synchronization are unverified.

## Multiplayer strafe updates (issue #20) — 2026-10-16

- **Further candidate, local and uncommitted:** The full-state sampler
  `0x003588D0` compares six controller bytes and otherwise refreshes only after
  60 match ticks. Direct host mouse look bypasses those bytes; independent
  stalls also change prediction without changing a held key. A separate patch
  now wraps the sampler and requests the original full-state packet after
  three match ticks (20 Hz). The original input-change trigger remains active.
  The wrapper preserves the original encoder and ABI, handles up to four
  local players per peer, and retains disabled/lobby/native fallback behavior.
  This bounds state age; it does not diagnose or eliminate host FPS dips.
- Isolated menu pipe/file logging tests did not reproduce periodic clock gaps
  after startup. Two subsequent System Link tests, including a disposable 4K
  client configuration, also had no >30 ms clock gaps in their 15-second
  timing-only windows (largest about 22 ms). Players were at different spawn
  locations rather than mutually visible. These tests do not rule out a
  renderer/input/terminal stall in the user's specific situation. Process-local
  sampling profiles are retained under the isolated host/client directories.
- A further controlled baseline used held strafe keys, repeated mouse turns
  and three deliberate 120 ms suspensions of each test-owned sender. Full-state
  receive gaps were typically 61 ticks and reached 68. Maximum near-simultaneous
  position error was 0.772 client-to-host and 1.466 host-to-client; maximum
  yaw mismatch was about 9.6 degrees. This baseline includes the first faster
  send gate. Input, save and both display paths were isolated; suspensions
  affected only child processes launched for this experiment. Evidence:
  `private/network-validation/issue20-baseline-stall.jsonl` and
  `issue20-stall-comparison.json`.
- The candidate repeated the same input/pause schedule in a separate match
  start with different spawn positions. Full-state receive gaps had median
  three ticks and maximum ten across injected pauses; maximum sampled sender
  full-state age was three ticks. Maximum yaw mismatch fell to 1.60 degrees.
  Maximum position error was 0.642 client-to-host and 0.817 host-to-client;
  maximum remote sampled steps were 0.734 and 0.933 respectively. These are
  improvements, not elimination of the pause-associated snaps. Both tests use
  a default-resolution host and an isolated 4K client, with the same original
  simulation clocks. Evidence: `issue20-refresh-stall.jsonl`,
  `issue20-stall-comparison.json`, `issue20-refresh-validation.json` in ignored
  `private/network-validation`. This does not establish performance with both
  players mutually visible, physical input, a full room or WAN latency/loss.
  The cause of the user's approximately five-second live stalls remains
  unresolved; hidden-console menu testing did not reproduce that periodicity.
- The further candidate regenerated and compiled successfully through the
  supported Windows build; all build-script checks and completed executable/
  content verification passed. Identity `969f9c4a`, executable SHA-256
  `54bd194371473bd6afd051a3eb668869e8d5ca96c1914ac559157d6821cf983e`.
  The redirected outer PowerShell invocation returned 1 while its log completed
  all checks; executable sealing and explicit verification succeeded. All 38
  affected Python/native-wrapper regressions passed in 88.62 s after routing
  pytest to an existing workspace temporary parent. Initial attempts hit an
  inaccessible system temp root, then a missing workspace temp parent; a relay
  timing test also failed during the initial concurrent compile. Evidence:
  `logs/issue-20-refresh-build.log`, `logs/issue-20-refresh-tests-final.log`.
- **Still unresolved in live play:** A subsequent user-driven, 12-second
  observation of both running Windows instances reproduced substantial remote
  corrections with players facing each other on build identity `a6bb2366`.
  Read-only position sampling caught a remote actor step of 1.314 world units
  and maximum near-simultaneous replication error of 1.314; the moving player's
  largest local sampled step was 0.346. The other player remained stationary
  during this capture, so it establishes only one direction of this live case.
  Large corrections coincided with advances of the last full-state receive
  timestamp, despite frequent movement updates. This points to remaining
  prediction/state-reconciliation behavior; it does not establish its cause.
  Eighteen window-only screenshots per instance and samples are retained in
  ignored `private/network-validation/issue20-live-*` and `issue20-live.jsonl`.
  Observation sent no inputs, wrote no game memory, and changed no settings or
  saves. The earlier isolated measurements below do not establish a complete
  fix for issue #20.
- **Frame-drop follow-up:** Two further 15-second, read-only observations used
  memory sampling only, without window captures or injected inputs. The first
  recorded a 0.651-unit correction about 5 ms after a 131 ms pause in sampled
  game-clock progress. The user-cued repeat recorded a 0.939-unit correction
  about 11 ms after the other process resumed from a 55 ms pause. Other jumps
  had no nearby pause, so causality and the complete trigger remain unresolved.
  The repeat's largest game-clock gaps were 116/114 ms, compared with median
  intervals of 16.5 ms. One process had approximately 105/105/116 ms gaps at
  elapsed 1.05/6.05/11.05 seconds, suggesting periodic work worth profiling.
  These are sampled clock-progress gaps, not directly measured GPU presents.
  The network frame counter can be reset by incoming synchronization packets;
  it must not be treated as an independent monotonic render-frame counter.
  Evidence: ignored `private/network-validation/issue20-live-timing.jsonl`,
  `issue20-live-timing.summary.json` and the earlier retained
  `issue20-live-timing-initial.summary.json`. No further runtime change was made
  from this correlation alone.
- Local, uncommitted on `20-player-desync-when-strafing-multiplayer`, based on
  fetched main `c8f8b31`. User clarified that this is the other player's
  movement in multiplayer, not a Story-mode problem; only keyboard/mouse had
  been tested. No Story movement, console clock or rendering changes are made.
- Reproduced with two isolated Windows System Link processes in Vietnam
  Deathmatch. Forty alternating A/D holds of 150 ms produced remote position
  jumps up to 1.338 world units at approximately 20 ms sample intervals; the
  sender's largest sampled step was 0.218. Maximum near-simultaneous replicated
  position error was 1.493. The guest receive timestamps advanced seven ticks
  at a time even over loopback: about 117 ms between movement opportunities.
- The US pump `0x003559A0` computes an eight-Hz network epoch by dividing the
  game clock by `60 / 8`, truncated to seven. The active-match state (15) in
  `0x003544B0` waits for that epoch before sampling movement. The new wrapper
  opens only that send gate once per guest tick; it keeps the original pump,
  packet encoding, network epoch, heartbeat/timeout calculations and idle
  full-state refresh. Both PC transports share this path. Duplicate calls in
  one guest tick, inactive states and disabled/native fallback are covered.
- After rebuilding, the same rapid-reversal input on the same map reduced
  the remote maximum sampled step to 0.153 (sender 0.153) and maximum measured
  replication error to 0.0844. Receive timestamp gaps had median one tick,
  maximum two. Reversing roles and moving the host produced maximum replicated
  error 0.1154 on the client. These are separate match starts with different spawn locations,
  not an identical-position replay. The comparison establishes substantially
  smaller corrections under this loopback workload, not zero jitter on WANs.
- Windows x64 build 26300, MSVC 14.42.34433, SDK 10.0.22621.0, Python 3.14.2,
  SDL 3.4.16 and Direct3D 11. Supported US XBE checksum and pinned upstream
  `d90c929` verified. Complete patch stack applied; generated code refreshed
  through `tsfp.ps1 -Action Build`. Release build and all build-script checks
  passed. The completed executable/content compatibility check passed before
  the fixed run (identity prefix `a6bb2366`, baseline `1d13b6bc`).
- All 38 targeted guest-boundary, identity, Online relay and patch upgrade/
  tracking regressions passed. The historical pre-network Mac fixture now
  excludes the new networking-dependent patch until the upgrade step.
  Native-fallback checks preserve original calls without host transport symbols.
  Evidence: `logs/issue-20-tests-final.log`.
- Tests use `private/network-validation/host` and `client` saves with explicit
  disposable controls and both display configuration paths. Personal saves,
  personal settings and the tracked input fixture were preserved. Evidence:
  `logs/issue-20-build.log`, `private/network-validation/issue20-before.jsonl`,
  `issue20-before-network.jsonl`, `issue20-net-samples.jsonl` and
  `issue20-host-net-samples.jsonl`, `issue20-comparison.json` and
  `issue20-validation.json` in that validation directory. Both processes stopped
  and their disposable virtual disks were recycled. Input is synthetic;
  live Online gameplay, physical controllers, other PCs, WAN conditions and
  sustained/full-capacity matches remain pending. The faster send opportunities
  increase network traffic; full-room bandwidth was not measured. Native Mac
  build/execution and synchronization remain unverified; see
  [MAC-013](MAC_PORT_BACKLOG.md#mac-013--system-link-and-online-multiplayer).

## Controller sensitivity slider (issue #24) — 2026-10-16

- Source base `c17336b` (fetched `origin/main`); this revision adds regression
  coverage/documentation on `24-controller-sensitivity-slider-not-working-correctly`.
  The runtime correction is already present in `837692d`, through the shared
  `patches/recomp-float-joins.patch`. No additional runtime change is required.
- Located the original controller sensitivity callback `0x002EAB30` and its
  shared clamp at `0x002EABEB`. Older generation discarded the comparison when
  decrement and increment paths met, causing the clamp to overwrite intermediate
  values with an endpoint. Restoring that older join behavior in an isolated
  translation reproduces the failure; the current translator preserves it.
- Added `tests/controller_sensitivity.c`, invoked by
  `tests/test_recomp_float_joins.py`: compile the actual checksum-verified US
  routine, retaining its original 0.05 step and clamp constants. All 188 checks
  passed: 0.5 to 0.45/0.55, full ascending/descending ranges on four profiles,
  both endpoints, near-boundary clamping, no input and guest register/stack
  preservation. UI and audio callees are stubs; this is callback execution,
  not live menu or physical-controller validation. No saves/settings are used.
- All 44 float-comparison and patch upgrade/tracking tests passed under MSVC;
  the final float-comparison suite also passed independently (17 tests).
- Host: Windows x64 build 26300, MSVC 14.42.34433, SDK 10.0.22621.0,
  Python 3.14.2, pinned upstream `d90c929`, SDL 3.4.16. The supported US XBE
  SHA-256 and full patch stack were verified. `tsfp.ps1 -Action Build` compiled
  Release successfully; network, DDS and display checks passed before its
  prompts test failed at `pc_controls_accept_pad()` because its test window
  did not obtain foreground input. The targeted Windows controls test passed
  separately with disposable save/display paths. Do not count the full build
  script as passing. Evidence: `logs/issue-24-build.log` and
  `logs/issue-24-tests.log`.
- No game process or renderer was exercised; live menu/controller checks and
  native Mac compilation/execution remain pending. Personal settings, saves and
  the tracked input fixture were preserved. See [MAC-016](MAC_PORT_BACKLOG.md#mac-016--shared-pickup-range-comparison-fix-issue-6).

## Nonzero-player mouse look (issue #18) — 2026-10-16

- Implemented in `8dee97f` on `18-mouse-input-only-works-for-player-1`, based on
  `4189924` (matching fetched `origin/main`). The shared camera helper rejected
  every player whose first guest word was nonzero. Removed that restriction
  from `src/pc_camera.h`; current-player selection remains in the existing
  guest wrapper, and memory/FOV validation remains intact.
- Reproduced the failure in the Windows controls regression by setting the
  guest player ID to 1 while using keyboard port 1. Before the fix the camera
  stayed still; afterward 100/20 mouse counts at sensitivity 0.1 produced
  -10/-2 degrees yaw/pitch. The same regression checks camera field selection
  for IDs 0–15, null player/FOV pointers, invalid FOV and single consumption.
- Windows x64, OS build 26300, MSVC 14.42.34433, Windows SDK 10.0.22621.0,
  SDL 3.4.16, Python 3.14.2; pinned upstream `d90c929` and supported US XBE
  SHA-256 verified by `tsfp.ps1 -Action Build`. Full Release build and all
  build-script regressions passed, including controls, settings, prompts,
  display, save routing, texture formats, debug and network checks. Complete
  runtime patch stack validation passed; all 27 patch upgrade/tracking Python
  regressions also passed. Evidence: `logs/issue-18-build.log`.
- These are adapter/model and build checks; no game was launched and no
  rendering backend or live Online/System Link mouse behavior was validated.
  Confirm mouse look on both host and non-host clients in an actual match,
  including controller input and pause/focus transitions. Personal saves and
  settings were preserved; disposable build test data was recycled by the
  supported build script. Native Mac build/execution and other-device
  synchronization remain unverified; see [MAC-001](MAC_PORT_BACKLOG.md#mac-001--direct-camera-mouse-look).

Tested on Windows and Apple Silicon macOS on October 14-15, 2026. This is an experimental port bring-up;
full gameplay and campaign completion have not been verified.

## Mac branch update before publishing — October 15

- Incorporated the newer Mac camera/prompt/multistage-renderer commit `6e48241`
  into the prepared main compatibility merge, preserving both lines of work.
  Shared prompt wrappers use the new guarded guest-memory helpers while still
  allowing the Online text rewrite in controller mode. Network backlog moved to
  `MAC-013` so existing audited `MAC-001` through `MAC-012` keep their identities.
- Windows/shared validation passed: 47 Python/compiler checks, with five
  Mac-specific checks skipped. Recompiled the shared controls, prompt model,
  guest prompt regression and display-menu sources with MSVC x64; linked the
  prompt regression against the existing input/SDL libraries and passed its
  controller/keyboard switching, guest ABI and Online-label checks.
- A full Windows game build and native Apple Silicon execution of this combined
  revision remain pending; prior evidence is retained below. No game or personal
  settings/saves were used for this merge.

## Mac/main merge compatibility — October 15

- Reconciled `mac-port` at `93dc422` with main at `16fcea5` for PR #10.
  The shared display menu retains POSIX memory-query compatibility and the
  Windows Online/System Link entries without linking the Windows host adapter
  into native Mac. Native network wrappers delegate to their original guest
  functions until a Mac transport is implemented; see backlog `MAC-013`.
- Kept existing runtime patch files intact. The updater applies the existing
  network patches before native boot, retaining its adjacent context around the
  SDK block and reversing the same order. Regression tests cover fresh installs,
  repeated builds, older Windows/Mac stacks, CRLF context and local-edit refusal.
- On Windows, 47 selected shared/native-tool/network tests passed; five Mac
  compiler/configuration/Trash checks were skipped. Compiled the affected shared
  display-menu source with MSVC x64, and compiled/executed guest-ABI tests for
  both the host-network route and native fallback without host-network symbols.
- Native Apple Silicon build, GPU/menu and multiplayer execution are not verified
  by this merge check. No game launches or personal settings/save changes were
  required; the existing Windows texture branch and overhaul files were preserved.

## Shared Windows and Mac feature policy/input model — October 15

- Added tracked shared-development rules to `AGENTS.md`: common behavior has one
  implementation wired into both builds; native API work stays in adapters, and
  unimplemented or untested platform support must be recorded explicitly.
  Git synchronization and rebuilding are still required between devices/branches.
- Found and corrected a concrete gap: the Windows diagonal-movement speed fix
  was behind `TSFP_PC_CONTROLS`, so the Mac binding path skipped it. The new
  `zzzzzzz-shared-input-model.patch` compiles the same `src/pc_input.c` on Mac
  and invokes its normalization from the shared binding parser. Windows's
  existing implementation/build path is unchanged. The Mac SDL regression now
  checks W+D speed normalization and release back to full straight-line speed.
- Native manifests now include shared source hashes. README documents the shared
  paths and remaining differences: Windows settings UI, direct-camera mouse path,
  display/texture-pack hooks and separate graphics APIs do not become portable
  merely by compiling the input model. No complete feature-parity claim is made.
- Rebuilt on macOS 27.0.1 ARM64 / Apple M5 Pro, AppleClang 21, pinned SDL 3.4.16
  and OpenGL 4.1 Metal -91.7. Supported US XBE and upstream `d90c929` verified.
  Source base `ea1979a` plus the prior interpolation repair and this change.
  Three native CTests, real-GPU/SDL checks (including diagonal input) and 25
  tool/patch tests pass; two Windows-only tests skipped. No mission run was
  required for this shared binding change. Windows execution and the other
  computer's synchronization remain unverified.
- Evidence: `logs/shared-pc-mac-build.log`, `logs/shared-pc-mac-tests.log`,
  `logs/shared-pc-mac-validation.json` and `build/macos/BUILD.json`.
  Changes remain local and uncommitted on `mac-port`; personal saves/settings
  were untouched, and disposable native regression disks were recycled.

## Native interpolation mesh-layout repair — October 15

- Investigated the user's missing first-mission terrain/character screenshots.
  Found a shared replay-state bug: vertex declarations changed during a guest
  frame, but interpolation retained neither those changes nor the initial
  declarations. Replaying with the final layout could misread vertices or skip
  a draw entirely. Immediate-mode effects temporarily use a sixteen-float4
  layout, then return to their normal stream layout.
- Added `zzzzzz-interpolation-layouts.patch` after the existing patch stack.
  It snapshots active declarations at frame start and copies successful layout
  changes into the replay in order, preserving the final guest state. The native
  executable was rebuilt; temporary culling/depth/CPU diagnostic edits were
  removed. No generated game C was edited.
- Before the fix, a moving first-level guest/midpoint pair showed fire/smoke
  disappearing in the midpoint; logs also reported a skipped vertex declaration.
  After the fix, both frames retain those effects, terrain and the nearby soldier.
  Reviewed further movement/turn captures retain terrain. **The exact complete
  terrain disappearance in the user's photograph was not reproduced, so its
  full resolution remains unverified.** No full-level playthrough was attempted.
- The final 145-second isolated 120-FPS run ended normally at its time limit,
  with no native fault, sampled GL error or skipped-declaration warning.
  Movement/turning used synthetic SDL keys. In seconds 115–135 after the first
  host present, the stationary sample measured **120.00 host FPS**: 1,200 guest
  plus 1,200 interpolated presents (60.00 FPS each); p95 11.47 ms, worst 13.34 ms.
  The moving/capture sample at seconds 95–110 measured 116.81 host FPS
  (59.67 guest + 57.14 interpolated). These are presents, not display scanout.
- Validation: 25 portable/native-tool tests passed, 2 Windows-only tests skipped;
  all three native CTests and real-GPU/SDL regressions passed. The new regression
  compiles the actual shared recorder with a mock renderer and checks layouts at
  draw boundaries, copied payloads, rejected changes and final restoration. It
  fails when declaration recording is deliberately removed. Complete patch-stack
  checks and the read-only native launcher prerequisite check pass. Native Windows
  execution remains untested for this shared recorder change.
- Build base `ea1979a` plus the new patch, upstream `d90c929`, supported US XBE
  verified. Host: macOS 27.0.1 ARM64, Apple M5 Pro, AppleClang 21, SDL 3.4.16,
  OpenGL 4.1 Metal -91.7. Evidence and full hashes are in
  `logs/render-layout-validation.json`, `logs/render-layout-fixed.*`,
  `logs/render-layout-build.log` and `build/macos/BUILD.json`. Selected before/after
  PNGs are `private/captures/native-mac/render-layout-*.png`; redundant diagnostic
  PNGs were removed. Isolated controls were restored and disposable test disks
  recycled. Personal saves/settings were untouched. Changes are local and
  uncommitted on `mac-port`; the other computer's synchronization is unverified.

## CrossOver integration removed — October 15

- Removed the compatibility launcher, Python implementation, dedicated tests
  and setup instructions. The native launcher owns its game-data/settings
  validation directly; it has no dependency on the removed launcher. Shared
  `AGENTS.md` now documents native Mac and Windows workflows.
- Moved the dedicated TimeSplitters compatibility-test bottle and obsolete
  Python caches to Trash. The bottle contained 309.5 MiB of physical allocation
  (305.2 MiB logical); recycling preserves recovery and does not free that space
  until Trash is emptied. The shared application, Windows build, native binary,
  original game data, personal settings and saves were preserved.
- On macOS 27.0.1 ARM64 with Python 3.12, 24 portable tests pass and 2
  Windows-only checks are skipped. Native launcher `--check`, shell syntax and
  diff whitespace checks pass. No game rebuild or gameplay run was needed:
  runtime source/patches are unchanged from native build `024d9be` (AppleClang
  21, SDL 3.4.16, OpenGL 4.1 Metal -91.7). Source base is `024d9be` plus this
  launcher/documentation removal. Evidence: `logs/remove-crossover-tests.log`,
  `logs/remove-crossover-cleanup.json` and `logs/remove-crossover-validation.json`.
- Changes remain local on `mac-port`; synchronization on the other computer
  is unverified. Historical validation logs remain as recovery records.

## Native Mac mouse-look — October 15

- Clicking inside the focused native game window now enables relative
  mouse-look. The capture click is consumed; subsequent clicks use the shared
  fire binding. F1 remains an alternative toggle. Escape and focus loss release
  capture, clear pending look samples and release held mouse buttons. Disabling
  mouse-look also releases capture on the main thread.
- Corrected the vertical sign after a first-level check showed upward mouse
  movement looking down with the default profile. Native input still feeds the
  original right-stick look path, so profile look speed/inversion also applies;
  it does not yet use the Windows direct-camera mouse path.
- The first 150-second isolated run reviewed horizontal/vertical turning,
  capture without an ammunition decrease, and pause/resume/recapture, and exposed
  that vertical-direction bug. The final 135-second run reviewed horizontal
  turning and upward motion looking up after the correction. Additional live
  input and focus changes interrupted its later scripted sequence: the final
  down/pause/resume/fire sequence is **not** counted as a controlled pass.
  Both runs reached their time limit without a native fault or sampled GL error.
  Mouse actions used macOS event delivery; navigation used synthetic SDL keys.
  This does not establish a completed human hardware playtest.
- Real SDL-window regression checks pass for click capture, swallowed capture
  click, subsequent fire, immediate neutralization on release, F1, focus loss,
  disabled mouse-look, sensitivity and inversion. Three native CTests and GPU
  regressions pass; portable checks report 19 passed, 2 Windows-only skipped,
  and 7 subtests. The read-only native launcher check passes. Windows was not run.
- Build base `9b00846` plus these native source changes; pinned runtime
  `d90c929` and supported US XBE verified. Apple M5 Pro, macOS 27.0.1 ARM64,
  AppleClang 21, SDL 3.4.16, OpenGL 4.1 Metal -91.7. Full source/binary hashes,
  test routing and limitations are in `logs/native-mouse-validation.json` and
  `build/macos/BUILD.json`; evidence includes `logs/mac-mouse-*` and selected
  `private/captures/native-mac/mouse-*.png`. Personal saves/configuration were
  absent and remain absent; disposable test disks were recycled. Changes are
  local on `mac-port`; the other computer's synchronization is unverified.

## Native Mac up-to-120 FPS mode — October 15

- Native launches now default to **2x render interpolation**, retaining the
  60 Hz console clock. `--fps 60` / `--fps 120` override only that launch;
  existing `frame_interp = 1` preferences remain respected. Windows behavior
  and personal saves/settings are unchanged.
- Compiled the decoded NV2A vertex programs into cached OpenGL vertex shaders,
  retaining the portable CPU interpreter as fallback/reference. Native game
  C now uses `-O2` with debug symbols and vectorization disabled for the scalar
  MMIO fault handler. Changes to pinned upstream sources are reproduced by
  `zzzzz-mac-high-refresh.patch`; generated game C was not edited.
- Connected the shared draw replay/matrix interpolation with preservation of
  the completed guest image/depth. Cleared intermediate depth separately after
  visual comparison exposed missing fire/smoke, and added a real-GPU regression
  for depth isolation and restoration. Late cinematic frames consume an already
  delivered vblank rather than waiting an extra tick for interpolation.
- The final **165-second** isolated run's stationary first-level canyon window
  (120–145 seconds after the first present) measured **120.00 host FPS**:
  **1,500 guest + 1,500 interpolated presents**, each **60.00 FPS**. Median host
  interval **10.02 ms**, p95 **13.47 ms**, p99 **15.61 ms**, worst **16.82 ms**;
  none exceeded 33.33 ms. Captures were enabled, with explicit paired readback
  outside the measurement window. These are completed host-present intervals,
  not monitor scanout measurements; spacing remains uneven rather than locked
  to 8.33 ms. Background focus during this window is recorded in the evidence;
  a separate foreground input check is also retained.
- A separate 110-second foreground run measured **119.90 host FPS** (60.00
  guest + 59.90 interpolated) in seconds 80–90: median 9.81 ms, p95 11.76 ms,
  worst 16.00 ms. Reviewed fresh captures of movement, the pause menu, resumed
  gameplay and ammunition decreasing after firing. Keyboard events were synthetic
  SDL input; firing used macOS mouse-event delivery, not a human playtest. The
  paired moving frames retain fire/smoke and show different geometry pixels.
- Host: Apple M5 Pro (16 GPU cores), macOS 27.0.1/26A434 ARM64, built-in display
  reporting 120 Hz; scene 640x480, AppleClang 21, pinned SDL 3.4.16,
  OpenGL 4.1 Metal -91.7 / GLSL 4.10. Build base `83491ef` plus this change,
  pinned runtime `d90c929`, and the supported US XBE; full hashes/tool versions
  are in `build/macos/BUILD.json` and `logs/native-120-fps-validation.json`.
- GPU/CPU transform-feedback comparisons cover MAC/ILU operations, paired
  reads, output/temporary aliasing and relative constants. All GPU/input tests
  and three native CTests pass. Portable checks: **19 passed, 2 Windows-only
  checks skipped, 7 subtests**; read-only native launcher check passes. No native
  fault or sampled GL error in the final timed benchmark.
- This is an up-to-120 target, not a campaign-wide guarantee. Cinematics,
  transitions, unmatched draws and insufficient rendering time can hold frames;
  some particles remain at the guest rate. Full-level completion, save/load,
  physical-controller play and complete rendering fidelity remain unverified.
  Changes are local on `mac-port`; the other computer has not been verified.
  Evidence: `logs/mac-120-verified.*`, `logs/mac-120-input-*`,
  `logs/mac-120-build.log`, `logs/mac-120-portable-tests.log`, and selected
  `private/captures/native-mac/native-120-*.png` / `interpolation-*.png`.

## Earlier 120/144 FPS feasibility check (before implementation) — October 15

- Tested `frame_cap = 0` only in the isolated runtime settings, keeping the
  console vblank clock at 60 Hz. The log confirmed the flip gate was off.
  In the same stationary canyon window (120–145 seconds), 1,500 host-present
  intervals still measured **60.00 FPS**, median **16.57 ms**, p95 **18.99 ms**,
  and worst **31.63 ms**. Removing that gate is not a high-refresh solution for
  this build. The 150-second run had no native fault or sampled GL error.
- The current Apple M5 Pro's only reported display is its built-in Color LCD,
  configured at **120.00 Hz**. Displaying 144 distinct frames per second would
  need an appropriate higher-refresh display; 120 is the relevant built-in target.
- Source inspection confirms numeric caps are divisions of the console vblank
  clock, and at that revision the native Mac interpolation entry points were unimplemented.
  Increasing the console clock may change game speed and was not used as a
  substitute for rendering support. The follow-up CPU sample placed 1,193 of
  2,633 main-thread samples in the CPU vertex interpreter. GPU vertex shaders
  and render-only high-refresh presentation were still development work at that
  point. The later 120 FPS implementation is recorded above; 144 is not a target.
- Restored the isolated runtime settings exactly and recycled disposable test
  images. Personal saves/settings were not used. No runtime source or binary
  changed; only findings/documentation were updated. Evidence:
  `logs/native-higher-fps-validation.json`, `logs/mac-perf-uncapped.csv`,
  `logs/mac-perf-uncapped-boot.json`, and `logs/mac-perf-uncapped-sample.txt`.

## Native frame-time investigation and upload fix — October 15

- Confirmed the reported slowdown on `mac-port`. Measured host-present intervals
  in the stationary first-level canyon, 120–145 seconds after the first present,
  excluding menus, loading, the opening animation and CPU sampling. Three bounded
  150-second runs used the same isolated profile and menu input sequence.

  | Run | Average FPS | Median frame | 95th percentile | Worst frame |
  | --- | ---: | ---: | ---: | ---: |
  | Before, rotating captures enabled | 14.63 | 68.34 ms | 71.61 ms | 80.61 ms |
  | Before, captures disabled | 14.85 | 67.28 ms | 70.48 ms | 74.60 ms |
  | After, rotating captures enabled | 60.00 | 16.44 ms | 19.09 ms | 33.80 ms |

- Captures were not the main cause. CPU sampling placed 1,442 of 1,942 main-thread
  samples in `glBufferSubData`, including driver waits for earlier GPU work.
  The renderer reused offset zero in one transient vertex/index buffer on every
  draw. Replacing each buffer's storage with `glBufferData` lets the driver retain
  earlier uploads while the GPU finishes them. The follow-up sample has no
  `glBufferSubData` calls. CPU vertex interpretation remains a performance limit.
- Compiler settings stayed unchanged to isolate this fix: Debug game code and
  the runtime libraries' existing `-O2`. `BUILD.json` now records the build type
  and complete configure command. The result is specific to this scene and Mac;
  it does not establish 60 FPS throughout the campaign or monitor scanout pacing.
- Added opt-in `--profile-frames` (20,000 CSV rows maximum, no GPU readback) and
  a frame-statistics tool that selects a known gameplay interval. The corrected
  1,500-frame measurement had one interval over 33.33 ms and none over 50 ms.
- Added real-GPU coverage for 128 queued, changing vertex/index uploads before
  readback, including nonindexed draws and both 16/32-bit indices. All graphics,
  input and three native CTests pass; portable tests: **17 passed, 2 Windows-only
  checks skipped, 7 subtests**. The read-only native launcher check passes.
  The final timed run reached at least 7,800 guest frames and host presents,
  with no native fault or sampled GL error. Reviewed the world/weapons/HUD capture.
- Test partition images were recycled; personal saves/settings were not used.
  Evidence: `logs/native-frame-performance-validation.json`,
  `logs/mac-perf-*.csv`, `logs/mac-perf-*-sample.txt`,
  `logs/mac-frame-fixed-build.log`, `logs/mac-frame-portable-tests.log`, and
  `private/captures/native-mac/native-frame-performance.png`.

## Focused native combat smoke test — October 15

- Continued on `mac-port` with the same native binary and isolated test profile.
  Traversed the initial canyon beyond the spawn area, reached the first enemy
  encounter, fired both handguns, observed ammunition consumption and enemies
  falling, and saw the accompanying marines advance. The world remained visible.
  This was an automated smoke test, not a human playthrough.
- Used temporary I/J/K/L aim and O fire bindings in the isolated test configuration
  for reproducible SDL input. Restored the fixture afterward. Personal saves and
  settings were not used or created. During inspection, waited for a fresh
  rotating frame; stale captures had made some turns appear inconsistent.
- The user narrowed the scope before full-level completion. The test was stopped
  intentionally with native exit 0; the runner reports an early `process_exit`
  rather than a passed timed probe. No new gameplay/rendering blocker was confirmed
  in this coverage. The 477-second session reached at least 8,400 guest frames
  and host presents, with no native fault or sampled GL error.
  Full-level completion, checkpoint reload, campaign save/load,
  audio fidelity and physical-controller play remain unverified.
- Extended the runner's explicit duration ceiling to 30 minutes, retaining its
  15-second default, isolated paths, two rotating captures and 8 MiB log limit.
  Portable regressions: **16 passed, 2 Windows-only checks skipped, 7 subtests**.
  No renderer/runtime source changed, so the native binary was not rebuilt.
- Evidence: `logs/native-combat-validation.json` and
  `private/captures/native-mac/first-combat-smoke.png`. Disposable partition images
  were recycled after the run.

## Visible native first-level gameplay — October 15

- Fixed the black world on `mac-port`. A per-draw GPU trace showed the world
  rendered correctly, then a later clear erased it. The game switches to a
  **160×120 offscreen texture** for that pass; the Mac backend previously returned
  no surface from `GetSurfaceLevel` and ignored `SetRenderTarget`, so the clear
  landed on the visible back buffer.
- Added real texture surfaces, OpenGL framebuffers and depth/stencil attachments,
  reference-counted target ownership, a persistent device depth surface, target
  viewport reset, rectangle/viewport clear clipping, back-buffer readback while
  an offscreen target is bound, and correct texture orientation on sampling.
  Failed/incomplete target changes preserve the prior target. These changes are
  confined to the native Mac backend; shared upstream patches and generated
  game C are unchanged.
- Reviewed actual first-level captures after the opening animation: canyon,
  enemies, dual pistols, crosshair and ammunition HUD stay visible. Forward
  input changed the viewpoint; firing reduced both pistols from **16 to 15**.
  Captured OS mouse deltas turned the view from the canyon toward a rock wall.
  Pause/resume returned to the visible world. These are bounded checks using
  SDL test input and macOS CoreGraphics events, not a human full-level playthrough.
- Added GPU regressions for offscreen clear isolation, top/bottom sampling,
  rectangle clears and preserving the old target after an incomplete attachment.
  All GPU/input checks and three native CTests pass. Portable checks:
  **16 passed, 2 Windows-only checks skipped, 7 subtests passed**. Windows
  build/runtime checks remain unrun on this host.
- Added `Play-Native-Mac.command` for interactive play with the native binary.
  It verifies ARM64/XBE/build hashes, preserves personal settings, strips stale
  test overrides and bounds logs to 8 MiB total. Its read-only readiness check
  passes. The interactive launcher was not used for automated game testing;
  personal save/settings paths remain untouched.
- The 120-second exact-binary gameplay probe completed at least **2,700**
  guest frames and host presents, with no native fault or sampled GL error.
  Its last render-target summary recorded **16,206 switches, zero failures**.
  Disposable test images were recycled; selected captures stay under `private/`.
- Evidence: `logs/mac-world-build-and-tests.log`, `logs/mac-world-portable-tests.log`,
  `logs/native-world-validation.json`, `logs/native-fixed-world-draw.txt`, and
  `private/captures/native-mac/world-playtest.png` / `world-resumed.png`.
- Remaining limits: full-level completion, campaign save/load, audio fidelity,
  physical controllers and complete renderer fidelity are unverified. The
  vertex programs still execute on the CPU; general multitexturing, cube/depth
  textures and screen-copy effects need further work. This is a first-level
  gameplay milestone, not a finished Mac release.

## Native Mac controls and first story-level load — October 15

- Added a Mac-only SDL event bridge to the shared JSON binding reader through
  `zzzz-mac-input.patch`. Keyboard/mouse state is published atomically for guest
  worker polls. Main-thread Cocoa events handle focus loss, mouse capture and
  window close. A new launch raises the game window once; it does not steal
  focus back during play. Windows control code and generated game C are unchanged.
- Confirmed the real menu flow: player selection, isolated profile creation,
  storage selection, main menu, Story, **2401: Time to Split**, difficulty,
  briefing and loading. Up/down changed selection. The isolated profile survived
  a new process and was selected again. This is profile persistence only, not
  campaign save/load validation.
- SDL event injection exercised the binding-to-XInput path. Separately, macOS
  CoreGraphics keyboard events delivered through the OS opened the in-level
  pause menu with Escape, moved its selection with Down and resumed with Escape.
  This verifies OS event delivery, not a human keyboard/controller playtest.
- The first level rendered its opening 3D scene and reached an ammunition HUD,
  but the gameplay world became black. No full playability, movement/collision,
  combat or rendering-fidelity claim is made. Renderer gaps remain the immediate
  blocker after menu navigation and level startup.
- F1 toggles relative mouse capture; Escape/focus loss releases it. The shared
  mouse enabled/sensitivity/invert settings are read at startup. The first free
  SDL controller shares keyboard player 1 without taking pads assigned to other
  players. Physical mouse look and hardware-controller play remain unverified.
- Native regression checks pass for key press/release, arrow/WASD bindings,
  focus neutralization, mouse buttons/deltas/settings and SDL virtual-controller
  hotplug/sharing. All three CTests and the real-GPU regression pass. Portable
  checks: **13 passed, 2 Windows-only checks skipped, 7 subtests passed**.
  Windows build/runtime tests were not run on this Mac.
- Repeated bounded 120-second menu/level probes completed without a native fault
  or sampled GL error. The final focused 100-second run logged at least 1,800
  guest frames and host presents and again reached the level. Window-close
  handling now exits cleanly at the probe's
  time limit (exit 0), without the former forced kill. Each probe uses
  `private/test-saves/native-mac`; known disposable partition images are recycled.
  Personal saves and settings remain unused and unchanged.
- Evidence: `logs/mac-input-build-and-tests.log`, `logs/mac-input-patch-tests.log`,
  `logs/native-input-validation.json`, `logs/native-level-summary.json`,
  `logs/native-boot-summary.json`,
  `private/captures/native-mac/menu-navigation.png` and
  `private/captures/native-mac/native-pause-navigation.png`,
  `private/captures/native-mac/first-native-level.png` and
  `private/captures/native-mac/native-gameplay-hud.png`.

## First visible native Mac menu — October 15

- On `mac-port`, connected the existing shared HLE graphics hooks to an Apple
  Silicon SDL3/Cocoa OpenGL 4.1 backend. The driver reports **GL 4.1 Metal - 91.7,
  GLSL 4.10**. The executable is native ARM64 Mach-O, with no Wine/Rosetta process.
- Visually reviewed actual GPU back-buffer captures: the TimeSplitters logo,
  animated menu background, **1 PLAYER / 2 PLAYER / 3 PLAYER / 4 PLAYER**, and
  **A Select** are readable. Repeated bounded runs reached this menu without a
  native fault. The final 45-second probe logged at least 2,100 guest frames
  and host presents, with no sampled GL errors; termination at the limit needed
  SIGKILL after the three-second grace period. This establishes the first visible
  menu; no input was injected,
  and menu navigation or gameplay is not claimed.
- Reused the shared vertex declarations, guest state mapping and texture cache.
  Added CPU execution of NV2A vertex programs with screen-space conversion,
  GLSL register-combiner evaluation, quad triangulation, Xbox texture unswizzling,
  DXT and A8 font texture uploads, and GPU readback. The loading worker exposed
  a missing current-context fault; a recursive lock now hands the context between
  guest threads. Cocoa window creation/event pumping remain on the main thread.
- Added a separate reproducible `zzz-mac-renderer.patch`; prior patches and
  generated game code remain unchanged. The Mac backend is `src/native/renderer_gl.c`;
  the existing Windows and Linux backend source selection remains intact.
  Shared portable format helpers compile from the pinned upstream source.
- Real GPU regression checks pass for quad coverage, A8 unswizzling and alpha
  blending, a synthetic NV2A vertex program, per-stage combiner constants,
  readback, and worker-thread context handoff. The three native CTest suites
  pass. Portable patch/launcher/isolation checks: 13 passed, 2 Windows-only
  PowerShell checks skipped, 7 subtests passed. The Windows executable was not rebuilt or tested.
- The probe supports `--capture`, with two rotating BMPs, isolated save/settings
  paths, bounded run/log size and validated partition-image recycling. Its JSON
  separates guest frames, sampled successful host presents, sampled GL errors,
  and capture hashes; visual menu recognition is recorded separately. Personal
  saves/settings remain unused. Project physical allocation remains about 8.5 GiB,
  with a 258 MiB native build and about 2 MiB of selected/rotating captures.
- Evidence: `private/captures/native-mac/first-native-menu.png`,
  `logs/native-boot-summary.json`, `logs/native-renderer-validation.json`,
  `logs/mac-renderer-build-and-tests.log`, and `logs/mac-renderer-patch-tests.log`.
- Remaining renderer gaps include general multi-texturing, offscreen render
  targets, screen-copy effects, movie playback and full 3D fidelity. Native
  controls/navigation, audible quality/latency, PC display/texture features,
  gameplay, fullscreen and save/load are not verified. Vertex programs currently
  execute on the CPU; a menu result is not a performance claim for gameplay.

## First native Apple Silicon execution — October 15

- Built `build/macos/futureperfect_recomp` as a native ARM64 Mach-O executable
  on macOS 27.0.1 with Apple Clang 21, Python 3.12 and the shared pinned runtime,
  Python dependencies and SDL submodule. No Wine, Rosetta or Windows process
  participates in the native runs. The verified US XBE was regenerated locally.
- A bounded 30-second run loaded all 23 XBE sections, resolved all 151 kernel
  imports, initialized the game's graphics device successfully, loaded game
  assets, ran worker threads and completed at least 1,800 guest frame calls.
  No native fault was recorded. The runner stopped it at the time limit;
  SIGTERM did not exit within the three-second grace period, so it used SIGKILL.
  This is **game-code execution without a connected host renderer**: no menu
  image, game window, gameplay, physical controls or save/load was verified.
- The first attempt exposed missing related address mappings. The Mac now
  reserves one 4 GiB virtual range and maps only its own unused pages; all 28
  low-RAM aliases, contiguous memory, device apertures and tiled alias mapped.
  It does not weaken macOS protections or overwrite unrelated host mappings.
- Implemented native relative/absolute and periodic waitable timers, and an
  ARM64 signal handler for supported scalar MMIO loads/stores. Protected NV2A
  interrupt writes and AC97 reset writes have real-signal regression coverage;
  unsupported instructions stop with diagnostics instead of being skipped.
  Read/write aliases let handlers update device backing without opening the
  protected guest page to concurrent writes. APU accesses use the existing
  device model, whose DSP implementation remains incomplete upstream.
- Implemented POSIX sparse partition images with the existing retail geometry,
  partition-device path routing, volume-directory opens and relative file opens.
  Existing image contents are preserved. These fixed the guest's early request
  to return to the Xbox dashboard. Isolated title metadata was created under
  `private/test-saves/native-mac`; personal saves were not used.
- Added an SDL3 PCM output backend and reused the portable controller-binding
  path. SDL audio output opened and PCM submissions occurred. Audible quality,
  latency and physical input remain unverified; audio uses the existing
  wall-clock fallback rather than claiming a hardware playback cursor. Movies
  are skipped because the native movie/renderer path is not connected.
- The initial graphics probe exposed an additional upstream settings fallback
  at `~/.config/xboxrecomp/titles/45410066.conf`. Its newly created default was
  moved into the isolated test directory. The probe now explicitly routes
  `RECOMP_DISPLAY_CONFIG`, `TSFP_DISPLAY_CONFIG`, input, saves and XDG roots;
  regression tests cover stale override removal and protection of personal paths.
- Three native CTest suites pass: mapping/threads/events/timers, ARM64 MMIO,
  and save/partition routing. Portable checks: 12 passed, 2 Windows-only
  PowerShell tests skipped, 7 subtests passed. Fresh application, upgrades,
  CRLF inputs, conflicts and repeat application of the complete patch stack
  pass. The actual Windows executable was not rebuilt or tested on this Mac.
- The shared rules now distinguish the retained Windows compatibility build
  from one active native build in `build/macos`. Native build provenance is
  recorded in its ignored `BUILD.json`; runtime changes are reproducible from
  `patches/zz-mac-native-boot.patch`, applied after the existing feature stack.
- Native tests recycled their known disposable partition images. The game
  probe's six images had 5,243,406,848 logical bytes but only 135,168 allocated
  bytes; the images are no longer in the project. Metadata/configuration and
  diagnostic logs remain. Physical project allocation is approximately 8.5 GiB,
  including the original ISO, the single game extract, dependencies and builds;
  `build/macos` is approximately 256 MiB.
- Evidence: `logs/native-boot.out`, `logs/native-boot.err`,
  `logs/native-boot-summary.json`, `logs/mac-native-build-and-tests.log`,
  and `logs/mac-native-patch-tests.log`. Earlier feasibility notes below are
  historical and are superseded by this native execution result.

### Next native milestone

The first-menu renderer connection is complete (see the newer result above).
Validate physical input and menu navigation, then extend rendering support for
an in-game scene. Movie presentation, PC settings/display/texture features and
save/load remain separate milestones. The earlier execution-only observations
in this section describe the state before the renderer connection.

## Native Mac feasibility and ISO preparation — October 15

- Work is on `mac-port`; the user requires changes from this chat to stay on
  that branch unless explicitly directed to `main`.
- Validated the supplied 4,047,372,288-byte ISO's embedded executable against
  the supported US SHA-256. Moved the original into ignored `private/discs`
  and extracted one copy: 412 files, 4,046,110,016 bytes, in `private/game`.
  Extraction paths/sizes were checked and the extracted executable rehashed.
  `private/disc.json` and `logs/iso-verification.json` record the checks.
- Game data is now available locally; no Mac gameplay validation was performed
  in this ISO preparation pass.
- Checked out the exact pinned runtime revision under `external/ts3FPrecomp`.
  Found a reproducible patch-installation failure on its CRLF CMake source.
  Made patch context whitespace handling explicit, avoiding reliance on the
  Windows machine's Git settings. The complete patch stack now applies and a
  second pass changes zero files. Conflict protection and CRLF tests pass.
- Added `mac-platform.patch`: the upstream Apple anonymous-mapping function
  returned descriptor 0 instead of creating a backing file. The native test
  failed before this fix and passes afterward. The patch uses an owned file
  descriptor, leaves no named backing file, and is restricted to `__APPLE__`.
- Compiled and ran an actual ARM64 Mach-O test with Apple Clang 21 on macOS
  27.0.1. It verifies 64 MiB aliased mappings, bidirectional data coherence,
  non-destructive collision rejection, a worker thread and event signaling.
  This runs natively. The initial low-address-linker
  experiment was terminated before useful output; the passing test uses the
  normal executable layout and OS-selected addresses. It does not establish
  that the full Xbox address-space mapping is ready.
- The native path-routing source passes a Clang syntax check. The memory-layout
  and kernel-bridge sources still fail on Windows process-affinity/timer APIs.
  Further source review found Windows-only title startup/graphics hooks,
  missing POSIX HLE audio/input implementations, an incomplete OpenGL renderer,
  a Windows-wired Vulkan backend, and Linux `.so` names in the non-Windows
  FFmpeg loader. Native game boot, rendering and gameplay are **not built or
  verified**. No platform support claim is inferred from the upstream README.
- Portable checks: 8 passed, 2 Windows-only PowerShell tests skipped, 7 subtests
  passed, using Python 3.12 and pinned pytest 9.1.1. Native evidence is in
  `logs/native-platform-before.*`, `logs/native-platform-after.*`, and
  `logs/native-kernel-compile-summary.json`. Personal saves and settings were
  not used. No disposable virtual disk images were created.

## Display settings and texture preview — October 15

- The working build now adds **Display settings** through the US game's own
  button builder in main Settings and paused Options. Resolution selection,
  Apply, Keep and Revert all use guest menu widgets and navigation.
- 1080p, 1440p, 4K and monitor-reported modes are offered. Rendering targets and
  depth buffers resize live; a 15-second unconfirmed preview returns to the old
  resolution. Confirmed settings persist beside the PC input configuration.
- The US camera wrapper updates the perspective matrix and horizontal CPU
  frustum together. The HUD retains its proportions in a centered 4:3 region.
  Front-end menus retain the original layout. Fullscreen remains Alt+Enter.
- The actual-menu replay verified 1080p apply/save, 1440p preview, timeout
  rollback, and return to the original menu. Native tests cover resolution
  bounds, monitor-mode deduplication, failed allocation handling at the API
  boundary, aspect calculations and persistence. Original game files, saves
  and keyboard/mouse configuration passed 424 protected-file hash comparisons.
- First-mission runs rendered 3840×2160 and 3440×1440 scene buffers and reached
  paused Options without a recorded crash or unresolved indirect call. The
  resolution checks use GPU render targets on the available display; a physical
  ultrawide monitor, split screen, and a full campaign are not validated.
- The expanded texture preview contains 11 replacements, approximately 44 MiB,
  covering selected terrain, masonry, timber, concrete, floor tiles, metal panels
  and one weapon atlas. Four replacements loaded in a 174-second opening-mission
  run at 1920×1080; gameplay and paused Options were reached without a recorded
  crash or unresolved call. The remaining seven need scene-specific runtime checks.
- DDS files have complete mip chains: two 4096×4096, eight 2048×2048, and one
  2048×1024. Square source art is 1254×1254; stone masonry is 1774×887. These are
  resampled outputs, not native 4K generated art. A character draft shifted facial
  and ear UV details and was kept out of the active pack.
- The coverage audit tracks 5,547 supported texture keys: 11 installed, one draft
  requiring correction/review, and 5,535 awaiting art. Another 753 cataloged
  payloads use unsupported formats. Embedded textures may remain undiscovered.
  This is not a completed game remaster. Eight offline tool tests pass; installed
  DDS hashes, dimensions and mip chains pass the audit. All 427 protected game,
  save and settings files matched their pre-test hashes; temporary disks and
  frame captures were recycled. Exact new prompts and real source sizes are saved.
- Pack compilation enforces the user's 6 GiB cap including metadata. Runtime
  replacement residency remains capped at 512 MiB. The supported-asset size plan
  is approximately 2.5 GiB before source art and unsupported-format additions;
  the plan is not completed artwork or a guarantee of final full-game size.
- Runtime changes are reproducible from the pinned upstream revision using
  `pc-controls.patch`, `save-routing.patch`, and `texture-pack.patch`; the last
  patch includes the renderer wiring shared by display and texture features.
  Fresh application in setup order and repeat-build checks pass.

## Completed

- Online source-build integration: a new network-menu entry, HTTPS browser with
  certificate fingerprint checking, public/private rooms, compatible-only
  listing/joining, peer credentials and leases, and a virtual-address bridge
  using the game host's forwarded UDP port. Guests establish outbound paths;
  the host relays guest-to-guest traffic. Two local instances used Online and
  an HTTPS directory to join a 2/16-player lobby and enter the same Shrink match.
  Cross-internet routing and full-match stability remain unverified. Campaign
  co-op remains a separate future effort.
  Automatic hosting now detects public IPv4 over HTTPS, starts/stops a local
  directory with the bridge, reuses its certificate and offers a copyable
  invitation containing the directory address, fingerprint and join code.

- Experimental PC System Link: native framed UDP, strict build/content identity
  checks, default-protocol discovery sockets, and XNADDR/key mapping. Two isolated
  game instances discovered a match, joined a 2/16-player lobby, and loaded into
  the same Thief match. This is a same-PC loopback test, not a cross-network test
  or a completed-match validation. See [multiplayer status](docs/multiplayer.md)
  for the later Online integration and remaining cross-network validation.

- Read the supplied desktop Xbox ISO without modifying it; extracted 412 files.
- Confirmed the US executable, title ID `45410066`, entry point `0038024D`,
  XDK 5849 and the expected SHA-256 documented in `README.md`.
- Identified 379 Xbox library symbols, including six upstream manual symbols.
- Generated 15,838 C function bodies with zero reported translation failures,
  totaling 2,507,843 lines across 16 chunks plus support files.
- Built `build/windows/Release/futureperfect_recomp.exe` with Visual Studio
  2022 / MSVC 19.44.35224 for Windows x64.
- Compiled and loaded FFmpeg 8.1 movie support.
- Fixed native profile device paths that bypassed `RECOMP_SAVE_DIR`, and
  preserved the fix in `patches/save-routing.patch`. Ten native path regression
  checks pass, including asset routing and switching between save directories.
- Added player-1 keyboard/mouse controls: normalized WASD movement, Windows raw
  relative mouse motion, direct camera-angle updates, mouse buttons and wheel,
  editable sensitivity/invert-Y settings, focus-loss clearing and cursor
  capture/release. The launcher loads `config/pc-controls.json`.
- Preserved the host integration and camera wrapper in `patches/pc-controls.patch`;
  the generated original `sub_000D2B80` body remains available as `_gen`.

## Validation

- Upstream Python suite: 547 passed, 41 skipped, 87 subtests passed.
- Upstream unittest discovery: 289 tests passed, including 26 skipped.
  This overlaps the pytest suite and is not an additional independent total.
- CPU differential conformance: 5,741 instruction vectors and 211 function
  vectors, zero mismatches.
- First 35-second startup test: decoded the EA and Free Radical intro movies
  (88 and 81 pictures), submitted PCM movie sound, reached the rendered
  player-count menu, and remained running until the test stopped it.
  No crash or unresolved indirect call was logged. Frame captures were
  inspected, including the player-count menu.
- Second 35-second input test: scripted A presses reached profile name entry;
  the captured name-entry screen was inspected.
- A subsequent 70-second test reached Story difficulty selection. It also
  exposed profile contamination through the original runtime's save-path bug.
  Earlier test profiles are preserved in `private/pre-fix-test-saves`.
- After fixing save routing, a fresh 100-second run created its profile under
  the requested test save directory, reached the first mission briefing, loaded
  `2401: Time to Split`, and rendered its opening cutscene and the landing scene.
  No crash or unresolved indirect call was logged; 3,578 function entries were
  observed. These scenes were inspected in captured frames.
- A 260-second fresh-save run reached first-person gameplay in the first mission
  and remained running until the test's time limit, with no logged crash or
  unresolved indirect call. It reached 3,855 instrumented functions.
  Scripted movement/look input changed the viewpoint, and trigger input reduced
  the displayed dual-weapon ammunition from 16 rounds per weapon to 14 and 13.
  Level geometry, textures, weapons, crosshair and ammunition display were
  inspected in the captured frames. This is a short gameplay check, not a
  completed mission or campaign playthrough.
- The final incremental build and native save-path tests also passed through
  Windows PowerShell 5.1, the same shell used by `Play.cmd`.
- Native PC input tests cover input clearing on focus/capture changes, consuming
  a recapture click, wheel press/release sequencing, equal mouse counts across
  different polling intervals, FOV scaling, vertical limits, inverted Y,
  normalized diagonal movement, and the actual bindings/guest-camera adapter.
- A 155-second run driven by the PC input model navigated menus with Enter,
  reached first-person gameplay, moved with W, applied raw-count camera changes,
  fired/reloaded, and switched weapons with the wheel. It ended at the test
  limit without a logged crash or unresolved indirect call. This used the
  adapter's deterministic event replay, not physical desktop input injection.
- A fresh-checkout fixture verifies that both local patches apply in Setup's
  order and reproduce all seven modified upstream files.
- A second 150-second run used the corrected aim/crouch bindings and reached
  3,874 instrumented functions with no logged crash or unresolved indirect call.
  Right-mouse aiming reduced the FOV from 55 to 30.25; a 20-count mouse event
  then changed yaw by 1.1 degrees, confirming the expected 0.55 zoom multiplier.
- The final build, both native test suites, and a six-second launcher startup
  test passed through Windows PowerShell 5.1 after the cursor activation fix.

Test output is in `logs/toolchain-tests.log`, `logs/pytest.log`,
`logs/conformance.log`, `logs/build.log`, and the timestamped smoke-test logs.
`logs/gameplay-smoke-summary.log` records the 260-second run;
`logs/final-build-check.log` records the final build and path checks.
PC input work is recorded in `logs/pc-final-bindings-build.log`,
`logs/pc-gameplay-summary.log`, and the newer `logs/pc-final-gameplay-summary.log`.
The final delivered build checks are `logs/pc-release-validation.log` and
`logs/pc-launcher-smoke.log`.
Original graphics captured by tests remain local in `private/captures`.

## Remaining work

- Verify mission progression, combat against enemies, control fidelity,
  profile reloads/checkpoints and sustained play beyond the short first-mission
  test. File creation and save isolation have been tested; a complete save/load
  gameplay round trip has not.
- Investigate translation coverage where execution requires it: the generator
  reports 50 unresolved direct-call targets emitted as diagnostic stubs, plus
  730 unsupported instruction occurrences emitted as comments/no-ops across
  30 mnemonics. Zero translation failures does not imply equivalent behavior.
- Check rendering and audio fidelity against the original game. Captured menu
  frames and host audio submissions are narrower evidence than a full playtest.
- Keyboard/mouse support currently targets player 1's first-person gameplay.
  Vehicle aiming, split-screen, MapMaker mouse navigation and extended hardware
  playtests remain unverified. Focus/capture behavior has automated state and
  activation-policy checks; no physical mouse/Alt-Tab playtest was performed.
- Aim/crouch toggle behavior follows the original game's profile. Mouse
  sensitivity/inversion is controlled by the PC configuration, independently
  of controller turn speed. The supplied PC profile selects keyboard input;
  controller input requires a controller binding configuration.
- Multiplayer and online behavior have not been tested here.

The generated C is a machine-code translation, not recovered original source.
Runtime code is pinned upstream; no claim of a completed decompilation or a
finished PC release is made.

## Interactive launcher correction (2026-10-15)

Normal play now launches through `tools/play.py`, which waits for the game and
writes its output directly to `logs/play-*.out` and `logs/play-*.err` using native
file handles. Interactive runs disable the generated code's default 400,000-line
function trace; diagnostic profiling remains available through SmokeTest.
The previous PowerShell GUI launch could return before the game, and inherited
or redirected output could stall startup. The corrected normal launch produced
captured startup logos and advancing swaps, with keyboard/mouse configuration
loaded. The user also confirmed that the logos appeared. Main-menu and gameplay
interaction for this hardware playtest is still to be tried.

## PC controls menu (2026-10-15)

Added a PC Settings → Keyboard & Mouse entry to the native game window's menu
bar, with F2 access in windowed and fullscreen modes. The owned settings dialog
lists all 24 Xbox actions with four keyboard/mouse binding slots each, mouse
sensitivity, inversion, mouse enable/disable, defaults, and Cancel. Capture
supports keys, five mouse buttons, and wheel directions; conflicting assignments
can be moved after confirmation. F1/F2/F9–F11 and Windows keys are reserved.
Gameplay input is blocked while editing, but simulation is not paused: pause
the mission first.

Saving applies bindings and mouse options immediately. Polls copy an immutable
binding snapshot under an SRW lock; editor input cannot become gameplay input.
Configuration changes preserve other ports and unrelated JSON members. Writes
use a temporary sibling file and atomic replacement, retaining the previous
configuration as `.bak`; failed writes retain the active bindings. Build checks
use a separate default fixture so user-customized bindings do not affect tests.

Validation: native input and save-path suites passed, as did the new settings
suite for live sampling, persistence and fresh-process reload, backup, unrelated
JSON retention, read-only failure, invalid values/reserved keys, defaults, native
dialog Cancel, key/wheel capture, and conflict reassignment. The UI was rendered
and visually inspected. A bounded real-game integration check verified both
entry points, F2 in fullscreen, input ownership/return, rendering after closing,
and that Cancel left the user's file unchanged. Both patches reproduced all
seven modified upstream files from pristine pinned sources. Evidence is in
`logs/pc-settings-release-validation.log` and `logs/pc-settings-game-tests.log`.

## In-game menu integration reverted (2026-10-15)

At the user's request, removed both the original-menu entry and the subsequent
game-rendered editing pages. Restored the original Xbox Settings/Options menus.
Keyboard/mouse gameplay controls, the separate F2 / PC Settings editor, and
user bindings/saves remain. Integration source is archived under
`private/rollback-in-game-menu`; generated code and the executable are rebuilt
without the guest menu wrapper or callback hooks.

Rollback validation: regeneration and Release build succeeded; save-path, PC
input/camera, and settings suites passed. A real-game UI check verified F2,
the PC Settings menu, and F2 in fullscreen; closing the editor returned input
and rendering to the game. The user configuration hash is unchanged. Logs:
`logs/revert-menu-build.log`, `logs/revert-menu-settings-ui.log`.

## Native Mac settings and graphics adapters (2026-10-15)

Local, uncommitted work on `mac-port` over `ea1979a4b6279045d0dc9c7c19472cb75e8a2216`.
Main and the other device were not changed; Windows build and cross-device
synchronization remain unverified. The native executable and every source hash
match `build/macos/BUILD.json` (executable SHA-256
`b6344c2b09bd415905d8675c05947a99f2063a069f5a361764d7954dce47b8ac`).
Host: Apple Silicon macOS 27.0.1, Apple Clang 21.0.0, SDL 3.4.16,
OpenGL 4.1 Metal 91.7 / GLSL 4.10. Shared upstream and supported XBE pins unchanged.

- F2 and the Mac Game Settings menu open a Cocoa Keyboard & Mouse / Graphics
  editor. Bindings, validation, defaults, unknown JSON preservation and display
  state use the PC implementation; POSIX atomic writes retain a controls backup.
  Sensitivity/inversion and bindings apply live. Conflicting keys are rejected
  with the two action names; Mac does not yet offer Windows' move-binding dialog.
  Closing the editor releases its input and raises the SDL game window.
- Native render targets support the shared resolution list, high-resolution
  scenes, widened projection/CPU frustum, and screen-position HUD classification.
  F11 toggles fullscreen; fullscreen is a session preference. The original
  Settings/Options menu's existing Display Settings integration is compiled on
  Mac too. Its 15-second preview logic is shared; the page's timeout interaction
  has not yet been exercised end-to-end on Mac.
- OpenGL replacements use the same texture keys and DDS loader/acceptance rules
  as Windows: full mip chains, BC1/2/3, aspect/no-downscale checks, 512 MiB resident
  replacement budget, and original-texture fallback. Launch with `--texture-pack`.
  Synthetic replacements were sampled on the real GPU and released correctly;
  this is not verification of a complete remaster pack.
- Integration found and fixed nested native settings initialization blocking
  input startup, and Cocoa presenting black while an offscreen FBO was bound.
  Both guest and interpolated swaps now bind the window drawable during swap.
  The GPU regression checks the drawable as well as guest readback.

Validation: complete patch-stack fresh/repeated/historical-upgrade tests;
28 Python tests passed, 2 Windows-only tests skipped; three native CTests passed;
real-GPU renderer/vertex/texture/resolution/offscreen/interpolation tests,
SDL input and shared settings tests passed. Settings checks cover live binding
sampling, exact backup, unrelated members/other controllers, invalid/reserved
values, symlink/read-only refusal and preserving active state on failed writes.
Standalone shared DDS-format tests and the interactive launcher's read-only
prerequisite check passed. Native MSVC/Direct3D tests cannot run on this host.

A bounded 130.15-second isolated game run at 1920x1080 with the 120-present target
reached the first mission and performed short movement checks. Reviewed both
rotating guest captures and an actual macOS window screenshot showing the world,
weapons, NPC, fire and HUD; full campaign completion was not attempted. At least
6,900 completed guest frames and 10,011 host presents were logged, with no sampled
GL errors or native fault. The 90–110-second movement interval measured 86.84
host presents/s: 56.17 guest + 30.66 interpolated, p95 19.32 ms, p99 31.48 ms.
Captures were enabled; these are host intervals, not display scanout or a broad
performance benchmark. The 60 Hz guest clock and personal FPS preferences remain
unchanged. The earlier pre-fix 1080p run established offscreen rendering only;
use the final run for on-screen evidence.

Separate bounded UI checks visually verified windowed/fullscreen menus,
resolution save/reload at 1080p, F2, controls save/reopen, Cancel, and keyboard
navigation after closing Settings. All tests used isolated saves/settings;
personal controls retain SHA-256
`cf44293910a77fb8f397ced933a1cfd5098e1949c93db9cc0571ed438b39f64a` and the absent
personal display file remains absent. Disposable partition images were recycled.
Evidence: `logs/mac-features-build.log`, `logs/mac-features-pytest.log`,
`logs/mac-features-final.json`, `logs/mac-features-ui.json`,
`logs/mac-features-final-frame-times.csv` and selected captures under
`private/captures/native-mac/mac-features-*` / `mac-settings-*`.

Remaining: general multi-texturing, movies/screen-copy effects and exhaustive
HUD placement/rendering fidelity; native direct-camera mouse behavior and PC
prompt/debug features are not claimed as equivalent. This work does not establish
that the previously reported world-disappearance case is completely resolved.


## Mac backlog renderer, camera and prompts (2026-10-15)

Validation performed before commit on `mac-port`, based on
`93dc422139bae4acb64d1e24c528cf3869a4ce20` plus the changes accompanying this
record. The other computer was not accessible for verification. Pinned upstream remains `d90c92988a9f99d0580d2ca3f7efd6ec3a7b7845`.
Host: macOS 27.0.1 arm64, Apple M5 Pro, Apple Clang 21.0.0, SDL 3.4.16,
OpenGL 4.1 Metal 91.7 / GLSL 4.10. The supported US XBE hash was checked:
`3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc`.
The complete patch stack was applied and wrappers regenerated through
`tools/build-native-mac.py --generate`; no generated C was hand-edited.
The final build manifest's recorded source hashes match the current sources.
Executable SHA-256:
`8f50deaa32247b94266b725fc6a00911b81236858743932be8e14edbfdf918e3`.

Implemented and checked in this batch:

- MAC-001: SDL relative counts now use the shared direct-camera model and verified
  US guest fields, preserving sensitivity, inversion, zoom scaling and pitch
  limits without adding mouse input to the controller stick. Native fixtures
  checked sensitivity/inversion/FOV, no stick injection, capture-click suppression,
  release and focus behavior. A live first-mission wrapper logged synthetic
  8/-2 count events producing -1.6/+0.4 degree yaw/pitch changes at sensitivity
  0.2 and FOV 55. Equal-count/polling-rate checks, complete live transition checks
  and physical-device testing remain pending on Mac.
- MAC-002: Mac compiles the shared prompt formatter and guest wrappers, and SDL
  activity selects the active prompt device. Real Mac VM region/protection checks
  replace the compatibility VirtualQuery stub for guest text/camera access;
  Windows retains its VirtualQuery implementation. Native fixtures passed
  custom/unbound/mouse labels, held/release/drift suppression, virtual-controller
  switching, live saved binding changes, guarded memory and guest stack/ABI
  restoration. Reviewed menus show [E] Select and [F] Back. Literal tutorial
  text still says START; artwork and non-token strings are not rewritten.
  Representative gameplay hints and physical-device switching remain unverified.
- MAC-005: all four 2D texture inputs and coordinates now reach fixed vertex,
  GPU vertex-program and CPU fallback paths. Tests cover distinct stages/UVs,
  projected coordinates, linear texture scaling and GPU/CPU agreement. FVF
  coordinate-size flags no longer accidentally select the programmable path.
  Reviewed first-mission captures show improved terrain, weapons, NPCs and
  debris. Dependent/dot texture modes, cube/volume resources and complete
  material fidelity remain unfinished.
- MAC-007: current-scene full/rectangular copies and image surfaces support
  32-bit ARGB/XRGB and linear variants. Real-GPU fixtures passed full/partial
  pixels, clipped offsets, orientation, preserved surrounding pixels/FBO state,
  and image-surface full/subrect read/write, double-lock and invalid-rect checks.
  Original and 1920x1080 scene sizes passed. Other formats explicitly return
  E_NOTIMPL; an actual title effect using the copy path remains to be checked.
- MAC-008: the first inclusive scissor rectangle now clips draws with target
  scaling, GL origin conversion and the existing HUD transform. GPU fixtures
  verified inside/outside pixels, disabling and clear behavior at original/1080p
  targets. Multiple/exclusive rectangles remain unsupported, matching the
  audited Windows limitation. This limited contract is marked Verified.
- MAC-010: the actual paused Options menu previewed 2560x1440, displayed the
  15-second countdown and automatically restored 1920x1080. The isolated saved
  mode stayed 1920x1080 and the game returned to visible first-mission gameplay.
  Keep/save/reopen, explicit Revert and broader aspect/HUD checks remain pending.

Validation commands/results:

- `python3 tools/build-native-mac.py --test`: all three native CTests passed,
  plus real-GPU renderer/multitexture/scissor/copy/image-surface/vertex/texture-pack/
  resolution/offscreen/interpolation/drawable tests and SDL input, prompt and
  settings regressions. Evidence: `logs/mac-backlog-build.log`.
- `.venv/bin/python -m pytest -q tests/test_patch_updates.py tests/test_native_interp_declarations.py tests/test_native_config_files.py tests/test_native_mac_tools.py tests/test_native_mac_launcher.py`:
  26 passed, two Windows-only checks skipped. Fresh, repeated and historical
  patch-upgrade coverage passed. Evidence: `logs/mac-backlog-python.log`.
- Final isolated runner: `--seconds 270 --capture --test-keys --profile-frames
  --fps 120`; 270.12 seconds, normal bounded exit, at least 13,800 completed guest
  frames and 15,530 successful host presents, no sampled GL errors/native fault.
  Captures were reviewed separately; frame counters alone do not establish
  gameplay. The final capture shows terrain, weapons, HUD and the opening
  tutorial. The run included menus, cinematics, resolution preview and brief
  synthetic input; it was not a full-level playthrough or physical-device test.
  Evidence: `logs/mac-backlog-final-summary.json`, `.err`, `-frames.csv`.

The final whole-run timing sample includes loading/cinematics and the higher
resolution preview: 58.33 host presents/s, comprising 51.69 guest and 6.64
interpolated presents/s, p95 25.56 ms and p99 37.18 ms. These are host CPU
intervals, not display scanout or a matched performance comparison. The 120
presentation setting and 60 Hz guest clock remain intact; sustained 120 FPS is
not established. Future renderer work needs scene-specific profiling as well as
fidelity checks.

Selected reviewed images in `private/captures/native-mac`:
`backlog-render-opening.png` (earlier renderer),
`backlog-multitexture-landing.png`, `backlog-multitexture-gameplay.png`,
`backlog-keyboard-prompts.png`, `backlog-paused-display.png`,
`backlog-resolution-countdown.png`, `backlog-final-landing.png`, and
`backlog-final-gameplay.png`. These are guest readbacks, not new macOS window
screenshots. Earlier cinematic border/outside-viewport artifacts and the user's
reported world-disappearance route are not claimed resolved by this batch.

One intermediate probe lost window focus and was stopped; it is not completion
evidence. The isolated SDL input file now accepts `motion`, `button` and `focus`
commands as well as key presses/releases. Synthetic focus allowed reliable menu
checks without changing normal interactive focus handling. This does not test
physical macOS event delivery. Runtime/input/display/save paths stayed isolated;
personal controls retain SHA-256
`cf44293910a77fb8f397ced933a1cfd5098e1949c93db9cc0571ed438b39f64a`, and the personal
display file remains absent. Known disposable partition images were recycled;
none remain in either native test directory.

MAC-003/004/006/009/012 remain queued. MAC-011 needs the real texture pack: its
local directory is absent, and the user confirmed it has not yet been transferred.
Synthetic pack tests passed again, but they do not verify actual artwork coverage.
Windows MSVC/Direct3D runtime checks are unavailable on this Mac; changes to the
shared headers/wrappers must still be checked on Windows. See
[MAC_PORT_BACKLOG.md](MAC_PORT_BACKLOG.md) for per-entry remaining acceptance work.


## Mac binding capture and camera checks (2026-10-15)

Validation performed before commit on `mac-port` based on `30653e3` (the checkout
advanced to the shared merge revision before edits began). Shared root rules are
unchanged. No work was moved to main; the other computer was not verified.
Host remains macOS 27.0.1 arm64 / Apple M5 Pro, Clang 21.0.0, SDL 3.4.16,
OpenGL 4.1 Metal 91.7. Upstream remains pinned to `d90c929`; the complete patch
stack was applied and generated wrappers rebuilt after the merge. The supported
US XBE SHA-256 was verified before generation:
`3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc`.

MAC-004 now has a Record button for each Cocoa binding slot. The capture box
accepts keys, modifiers, five mouse buttons and both wheel directions. Escape
cancels; Use Escape and Unbind provide explicit alternatives. Reserved and
unsupported inputs show feedback. A conflicting capture offers Cancel or Move
binding, with Cancel as the default. All edits remain drafts until Save; a
binding change does not discard pending sensitivity/inversion edits. Existing
unsupported saved keys remain selected and labeled unavailable on Mac.

Cocoa hardware keys and SDL gameplay input use the same portable virtual-key
mapping. Added punctuation/OEM keys, keypad digits/operators, lock keys and
system-key recognition. Windows and Mac share `pc_settings_key_allowed` and
`pc_settings_assign`; the latter removes the chosen key from old slots only
after explicit conflict approval. The Windows editor retains its existing
confirmation UI and uses the shared assignment function. Its native runtime
checks remain unavailable on this host.

Validation:

- `python3 tools/build-native-mac.py --generate --test`, followed by the final
  incremental `--test`: three CTests and all native GPU, input, prompt, settings
  and Cocoa capture checks passed. Final executable SHA-256:
  `c9dee18525ec447939bd1ecec5a6bcd0de431489fbb82d1db266bcb432d050ea`.
  `build/macos/BUILD.json` source hashes match the final sources.
- Cocoa responder fixtures cover key/modifier press/release, repeat suppression,
  five mouse buttons, both wheel directions, unsupported/reserved rejection and
  Escape cancellation. Actual modal capture dialogs test initial keyboard focus,
  Return through NSWindow event dispatch, Use Escape, Unbind and Cancel. These
  are application-generated tests, not physical macOS input validation.
- Shared assignment tests cover conflict refusal without mutation, explicit move,
  removing old occurrences, reserved/invalid rejection and unbinding. Disposable
  settings save/reload tests verify punctuation/keypad input reaches the correct
  guest sticks, live prompts update, unrelated JSON and other controllers remain
  intact, backups match, and failed read-only writes preserve active settings.
- MAC-001 native guest-camera fixtures now compare one batch of counts with 100
  individual updates, both pitch limits and stale motion suppression. Existing
  zoom, inversion, focus, capture-click and no-stick-injection checks still pass.
  This extends synthetic adapter coverage; physical mouse and complete live
  pause/settings transition checks remain pending.
- Relevant Python suites: 28 passed, two Windows-only checks skipped, including
  patch fresh/repeated/historical-upgrade, interpolation declarations, config
  preservation, isolated runner and launcher tests. `git diff --check` passed
  with the project's CRLF whitespace convention.

Evidence: `logs/mac-capture-build.log`, `logs/mac-capture-python.log`. A 90-second
isolated probe reached a reviewed rendered menu and dispatched F2 into Settings.
The automation surface did not expose the unbundled game's Cocoa window, so no
visual claim is made for the new editor layout. The runner force-stopped the
open modal after its timeout (93.17 s including shutdown grace, exit -9); no
native fault or sampled GL error was recorded. This probe used the build before
the final unsupported-key label adjustment and is not normal-shutdown evidence.
Its summary/error log are `logs/mac-capture-game-summary.json` and
`logs/mac-capture-game.err`; reviewed guest menu: `private/captures/native-mac/mac-binding-menu.png`.

The pre-build disk check showed 224 GiB available and 158 MiB physically allocated
for `build/macos`. All checks used the existing isolated native paths. Personal
controls retain SHA-256
`cf44293910a77fb8f397ced933a1cfd5098e1949c93db9cc0571ed438b39f64a`; the personal
display file remains absent. Disposable partition images were recycled through
the shared cleanup helper. MAC-004 is Awaiting validation for visual/editor and
physical-device checks (including non-US keyboards). The remaining rendering,
debug, movies, multiplayer and packaging entries stay in the backlog. Actual
texture-pack validation still awaits the user's asset transfer.

A final 35.16-second isolated menu probe used the final executable above and
exited normally at its bound (exit 0), with at least 1800 completed guest frames
and 1993 successful host presents, no sampled GL errors or native fault.
Evidence: `logs/mac-capture-final-summary.json` and `logs/mac-capture-final.err`.
No gameplay or physical-input claim is made from this short startup check.


## Mac debug and performance tools (2026-10-15)

Published the preceding binding/camera work as `e83e074` to `origin/mac-port`.
The debug adaptation below is local and uncommitted over that revision. No main
merge was performed; the Windows computer's checkout remains unverified.

MAC-003 now uses shared `pc_debug.c` statistics, options, formatting and log
classification. Native adapters provide F9, an F12/Cocoa panel, CoreText/OpenGL
text rendering, POSIX log reading and Finder log-folder access. Save & Close
uses the existing atomic controls-JSON writer and backup behavior. Cancel
preserves active values; Clear counters skips old log text. The log reader
handles incomplete lines, truncation and inode replacement, rejects non-regular
files and consumes at most 64 KiB per main-thread poll. Performance samples are
optional and emitted no more than twice per second. Both launch paths override
inherited log paths to their own bounded stderr files.

The overlay is drawn after the scene blit and stays out of guest captures,
screen copies and interpolation history. The Game count is guest presentations;
Shown includes successful interpolated host presents. The panel also displays
their difference. Average guest interval is derived from these counts; neither
counter measures monitor scanout or changes the 60 Hz guest clock.

Validation on macOS 27.0.1 arm64 / Apple M5 Pro, AppleClang 21.0.0, SDL 3.4.16,
OpenGL 4.1 Metal 91.7 / GLSL 4.10, upstream `d90c929` and the complete patch stack:

- Supported US XBE SHA-256 verified before building and the bounded run:
  `3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc`.
- Final `python3 tools/build-native-mac.py --test` passed all three CTests plus
  real-GPU, input, prompts, binding/settings and debug suites. Debug checks cover
  log rotation/truncation/partial lines, classifier exclusion of its own samples,
  counter reset, invalid samples, Cocoa modal Save/Cancel, read-only failure,
  unrelated JSON/other-controller preservation and exact backups. SDL F9 tests
  verify key repeat and release do not toggle a second time.
- Real-GPU overlay checks verify visible text pixels, disabled overlay, restored
  framebuffer/scissor state and unchanged guest scene. Reviewing the rendered
  fixture caught upside-down CoreGraphics text; the blit now corrects row
  orientation. Final fixture `build/macos/path-tests/debug-overlay.png` is upright
  and readable. Its 60/120 numbers are controlled test values, not measured FPS.
- Relevant Python checks: 28 passed, two Windows-only tests skipped. Launcher
  `--check` passed, source hashes match `build/macos/BUILD.json`, and
  `git diff --check` passed. Final executable SHA-256:
  `40f27d687a9a8bfe5ea1538c74a64171e5e161c55929aa122b1f39ba3f82e290`.
- A 45.14-second isolated run completed at its time limit with exit 0, no native
  fault and no sampled GL errors. It dispatched synthetic F9 and F11 toggles,
  including F9 between fullscreen requests. Its profiler recorded 2,237 guest
  and 852 interpolated presents, 68.78 total presents/s over loading/title
  rendering and transitions (49.81 guest + 18.97 interpolated/s), with p95
  interval 25.38 ms. The debug logger produced 88 bounded samples and kept guest
  and shown rates distinct. This is a shortcut/integration smoke test, not a
  steady gameplay or 120 FPS performance certification.
- That integration run used executable `c699b3e482218f050a2749511bd4055c860c3b9faceeb232612a44d6d751223d`,
  before the orientation correction and added F9 regression assertion; final
  GPU/SDL suites above validate those subsequent changes. No additional
  full-level or campaign run was performed.

Evidence: `logs/mac-debug-build.log`, `logs/mac-debug-python.log`,
`logs/mac-debug-native-boot-summary.json`, `logs/mac-debug-native-boot.err`,
`logs/mac-debug-native-frame-times.csv`, and the small rendered fixture above.
The test controls were restored; personal controls retained SHA-256
`cf44293910a77fb8f397ced933a1cfd5098e1949c93db9cc0571ed438b39f64a`;
personal display settings remained absent, and personal saves were unused.
Both isolated test roots have no remaining disposable partition images.

MAC-003 remains Awaiting validation: physical F9/F12 input, visual Cocoa layout,
Finder action and live fullscreen panel/focus behavior need review. Synthetic
SDL events and application-driven modal tests do not establish physical macOS
event delivery. Windows MSVC/runtime checks remain unavailable on this host.
MAC-011 still awaits the user's real texture pack; no artwork was substituted.

## 2026-10-15 — Windows release patch preparation

Local, uncommitted change on `mac-port` base `e83e074`; release patch inputs
were read from local `main` revision `5cf4866`. Host: Windows 11 x64
10.0.26300, Python 3.14.2, Git 2.47.0.windows.2. The native boot overlay guard
invalidated the Windows watermark patch's original surrounding context.
The loader now requires one adjacent context line for that specific patch
when native boot is present; added/removed code must still match.

`python -m pytest tests/test_patch_updates.py -q`: 11 passed. Added regression
cases cover fresh application, an existing watermark, repeated application,
and rejection of conflicting watermark code without writes. Separate temporary
source-only fixtures using all patches from main reproduced the original error
before the fix and passed afterward for fresh upstream sources and a copy of
the existing Windows runtime (34 and 28 files updated respectively); a second
application changed nothing. The actual runtime was not modified. No game
build, packaging, publishing, or gameplay test was needed or performed for
this Python loader fix. No game data or personal settings were used.

MAC-014 tracks pending macOS validation. Changes have not been committed,
pushed, or merged to main; synchronization with the other device is unverified.

## 2026-10-15 — Windows texture-dump compilation fix

Reported during release preparation on main `38f1a2f`. Fix is local and
uncommitted on `mac-port` base `724b1a1`: restore the missing `FILE *f`
declaration in `src/texture_pack.c::dump_original`. No runtime patch or generated
game source changes are required. MAC-011 records the platform impact.

On Windows x64, the installed VS 2022 BuildTools MSVC compiler accepted the
corrected translation unit with `/Zs /TC` and the Windows renderer include paths
and feature defines. The existing `tsfp_texture_format_test.exe` passed DDS
validation checks; that executable was not rebuilt and does not exercise the
dump function. Texture-tool Python tests could not collect because Pillow is
not installed in this virtual environment. No dependency pins were changed.
No full game build, packaging, game launch or macOS validation was performed;
personal settings and saves were unused. Other-device synchronization remains
unverified. This fix must reach main before release preparation can use it.

Patch validation: `python -m pytest tests/test_patch_updates.py -q` passed all
11 tests on Python 3.14.2. `git diff --check` passed.

## 2026-10-15 — Release cleanup and Windows test privileges

Uncommitted on `fix/release-cleanup`, based on main `2ef7536`. The existing
changes were carried intact from `mac-port` onto this focused branch following
the user's updated branching preference. Host: Windows 11 x64, Python 3.14.2,
Windows PowerShell 5.1.
Symlink test fixtures now skip only Windows error 1314; real symlink assertions
still run on capable hosts and other creation errors propagate. Release work
cleanup uses direct deletion after the existing exact-path and reparse checks,
so disposable test/staging files do not require Recycle Bin support. Release
archives and metadata are retained. A cleanup failure during failed tests is
reported without replacing the original test failure.

Targeted checks: `pytest tests/test_publish_release.py
tests/test_native_mac_tools.py -q -p no:cacheprovider --basetemp
build/cleanup-validation`: 22 passed, three skipped. Two skips are Windows
symlink privileges and one is the existing macOS allocation check. Actual
PowerShell cleanup tests passed for disposable test and staging directories,
rejected a personal-save-shaped path, and preserved an adjacent archive.
The original-error/cleanup-error regression passed. The first sandbox attempt
could not execute scripts; validation succeeded outside the sandbox using an
isolated build directory, without changing execution policy.

No game build, release preparation/upload, game launch, or Mac checks were
performed. Personal saves/settings and existing release outputs were untouched.
MAC-015 tracks Mac validation. Changes are local, uncommitted and unpushed;
other-device synchronization remains unverified.

## Pickup range comparison fix — October 16, 2026

Issue [#6](https://github.com/TheDoffman/timesplitters-future-perfect-pc/issues/6):
the US pickup tick at `0x000C15B0` compares the original radius squared with
horizontal distance on two paths (`COMISS xmm3,xmm0` and `COMISS xmm0,xmm1`).
At their shared branch, `0x000C17D9`, the translator discarded the differing
operand states and emitted `if (_flags)` with `_flags` initialized to zero.
The horizontal rejection therefore never ran. Eligibility and vertical checks
still ran, explaining early pickups rather than indiscriminate item grants.

`patches/recomp-float-joins.patch` merges SSE comparison snapshots across those
paths, retaining their original ordered/unordered conditions. Pickup radii,
weapon grants and input bindings are unchanged. No generated game C was edited.
`tools/recomp-generation.py` records generator source hashes; Windows Build
regenerates stale C, while native Mac requests `--generate`. The normal Windows
Test command includes the new regression. Existing generated checkouts migrate
on their next build.

Local validation baseline `f847832ef3588000b6d4251de657ea34f308fac1`, in
the existing `codex/integrate-display-textures` checkout; branch, HEAD and user
edits were preserved. The fix is isolated on `fix-pickup-range` over newer main for the user-requested push. No binary release is included.
Runtime pin `d90c92988a9f99d0580d2ca3f7efd6ec3a7b7845`; the supported US XBE SHA-256
was verified before generation and execution. Host: Windows x64, Python 3.12.14,
MSVC 19.44.35224.0, Release, Direct3D 11.

Validation:

- The pinned original translator reproduced a real out-of-range grant in the
  compiled pickup tick: radius 100, position `(101,0,0)`, grant 1 instead of 0.
  Before the patch, 13 new structural/integration checks failed; after it, all
  15 new tests pass. The actual translated pickup function passes 208 isolated
  cases covering both shape paths, 32 directions, near/far and exact-radius
  boundaries, vertical separation, NaN/infinite coordinates and guest register,
  stack and x87-stack preservation. Only eligibility/grant callbacks are stubbed.
- 25 patch-stack and upstream flag/float checks passed. Mac build-tool/launcher
  tests: 16 passed, one Windows symlink-privilege skip. Native Mac compilation
  and gameplay were unavailable and remain unverified.
- All Windows build targets compiled. The full Build command stopped at the
  unchanged controller-prompt test's foreground-window check (line 78), also
  reproduced on retry. This is **not an all-tests-passed build**. Network, DDS,
  texture switching and display regressions passed; save routing, PC controls,
  settings and debug tests then ran separately and passed. The executable's
  network identity was sealed after those checks; the focus-test failure remains
  recorded rather than hidden.
- A bounded 178-second isolated run loaded the first mission opening sequence at
  1280x720. Synthetic left/right strafe keys were injected and 12 raw mouse yaw
  updates were logged. Reviewed captures show the opening animation; this does
  not establish a normal gameplay route, on-scene pickup collection, every story
  level or the separately reported body-collision behavior. No crash or unresolved
  call was logged. Personal controls/display hashes remained unchanged; saves
  and both runtime/PC display configuration paths were isolated.
- Recycled 18 disposable test partition images after their test passes, retaining
  profiles, settings and diagnostics. Their apparent size was 15,730,220,544 bytes;
  physical allocation was only 138,752 bytes. No personal saves were used.

Evidence: `build/collision-validation/VALIDATION.json`, `baseline.json`,
`build.log`, the native test logs, `game.json`, `game-150.png`, `game-172.png`
and `disk-cleanup.json`. Fixed executable:
`build/windows/Release/futureperfect_recomp.exe`, SHA-256
`4e6c801ed83e123fcfe43e32d3ace1332ac62daeaa5f1367d3c30e330bebf929`.
The launcher-installed running game was left alone; it retains its previous
executable until updated. Mac regeneration/validation and cross-device Git
synchronization remain pending under MAC-016.

Publication source note: this focused change is based on main `8f5e1bd2b500ad1d87379a81254ad936df10df3f`. The executable and gameplay evidence above were produced in the
earlier local checkout, not from this publication commit. Its source-only
patch stack and regressions passed 51 checks before the push, including the
208-case compiled pickup regression; one Windows symlink-privilege check skipped.


## Runtime patch branch-switch repair — October 16, 2026

Branch-switch fix on `fix/runtime-patch-branch-switch`, based on main
`837692d3ffbd81fbb6252ff819ff219f8e3ce097`. The same runtime directory had been
built on `codex/integrate-display-textures`. Its texture-toggle patch remained
applied after switching to main, preventing the installer from unwinding the
controls dependency. A disposable source-only copy reproduced the reported
`pc-controls.patch conflicts with the runtime checkout` error before the fix.

The loader now recognizes exact patches in `patches/legacy/` only for unwinding,
then applies the current branch's active stack. An active copy takes precedence
when the same patch is present in both locations. The archived texture-toggle
patch matches the texture branch exactly. No upstream reset or manual generated
C edit was used; unrelated runtime edits remain intact. Conflicting edits still
fail before runtime writes.

Validation: Windows 11 x64 10.0.26300, Python 3.12.14, MSVC x64 14.44.35207,
Release / Direct3D 11; pinned upstream `d90c929`. The supported US XBE SHA-256
was verified as `3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc`.

- Patch-loader regression suite: 14 passed in 75.84 seconds. New checks cover
  texture-to-main-to-texture transitions with LF and CRLF, repeated no-op
  application, preservation of an unrelated edit and rejection of a modified
  legacy patch without writes.
- Full `tsfp.ps1 Build` passed after the user closed the running development
  executable. Seven runtime source files were updated by the validated stack.
  Native checks passed for UDP/guest network routing and packet compatibility,
  texture formats, display persistence, virtual-controller prompts, save paths,
  controls, settings and debug. Disposable build/path-tests data was recycled
  by the normal build command. Existing compiler warnings remain.
- Rebuilt executable SHA-256: `625769a2fdfa5497a5bdf735b960f5c3100882d3d3b6b68389f515877e04176a`.
  Its network-build stamp was sealed by the successful build. Controls/display
  hashes and the pre-existing package-playable.py edit match the pre-build
  record. Personal saves were not used by the isolated build checks.

Evidence: `logs/runtime-patch-switch-build.log` and
`build/patch-switch-validation/VALIDATION.json`; a small runtime recovery patch
was retained before the transition. No game launch, end-to-end multiplayer
session, packaging or release publication was performed. MAC-014 tracks pending
macOS validation; other-device synchronization is unverified.


## Automatic runtime patch tracking — October 16, 2026

Implemented on `fix/automatic-patch-tracking`, based on main `979a16c`, for
commit and publication at the user's request. Each successful loader invocation
records exact patch bytes, SHA-256 hashes, order and context options in the
runtime's per-worktree Git metadata (`tsfp-patches.json`). The next invocation
unwinds that recorded stack before applying the current branch's stack, so
removed, renamed or edited patch files no longer require a manual archive.
Existing runtimes bootstrap through the active/legacy recognition path once.
Branches need the updated loader to maintain this record.

The entire transition is validated on disposable source copies. Invalid records
and conflicting edits fail before runtime writes. Concurrent source/record edits
observed during validation are preserved. Failed source writes or metadata
publication roll back source changes and retain the old record. Successful
records are replaced atomically; repeated no-op runs do not rewrite metadata.
No game data, generated C or build tree is duplicated.

Validation on Windows 11 x64 10.0.26300, Python 3.12.14, MSVC 14.44.35207,
Release / Direct3D 11, pinned runtime `d90c929` and the verified supported US XBE:

- Existing patch-upgrade and new tracking tests: 27 passed. Real Git branch
  switches cover unknown patch removal, edited patch content, both directions,
  LF/CRLF and unrelated edits. Additional cases cover rename/empty stacks,
  exact-byte/hash retention, corrupt/unsupported records, partial write and
  record-publication rollback, and concurrent edit detection.
- Native Mac tools/launcher Python suites on Windows: 16 passed, one skip for
  Windows symlink privilege. These are shared-tool checks, not Mac game tests.
- The actual runtime enrolled 27 patches in a 152,426-byte record without source
  changes. A subsequent Build used the record successfully. Compilation and
  network, texture and display checks passed; Build then stopped at the existing
  tests/pc_prompts.c:55 foreground-window assertion. A direct retry failed the
  same assertion. Save-path, controls, settings and debug checks passed when run
  separately. The executable SHA-256 remains
  `625769a2fdfa5497a5bdf735b960f5c3100882d3d3b6b68389f515877e04176a`, identical to
  the preceding successful build; multiplayer content verification passed.
- Personal controls/display and the existing package-playable.py edit retain
  their baseline hashes. Disposable native test data was recycled after checks.

The Runtime patch transitions GitHub Actions workflow uses pinned actions,
reads the upstream/pytest pins from shared configuration, and runs both patch
suites on Windows, Linux and macOS without game data or submodules. Remote
matrix results are available in GitHub Actions. `tsfp.ps1 Test` includes the new tracking suite.
Evidence: logs/patch-tracking-tests.log, logs/patch-tracking-unit.log,
logs/patch-tracking-shared-tests.log, logs/automatic-patch-tracking-build.log,
logs/patch-tracking-prompt-retry.log and build/patch-tracking-validation/VALIDATION.json.
No gameplay session, binary packaging or release publication was performed.
MAC-014 retains pending native Mac validation and other-device synchronization.


## Issue #8 audio timing and buffering — October 16, 2026

Local, uncommitted work on `fix/issue-8-audio-timing`, based on main
`4189924038729030d890faa93edacdeccc74a1e1`. The user clarified that delayed
cutscene audio concerns story cutscenes. The separate XMV video path was not
changed. `patches/dsound-audio-timing.patch` fixes four reproduced runtime bugs:
stream completion advancing through unheard audio after queue starvation;
flush retaining the previous voice's timeline origin; refused buffer submissions
discarding PCM; and ADPCM resume copying from the wrong offset inside a block.
Process, pause and format changes now synchronize to the available device clock.
The existing fallback remains for backends without a playback-position report.

Validation host: Windows 11 x64 10.0.26300, Python 3.12.14, MSVC 14.44.35207
(compiler 19.44), Release / Direct3D 11. Runtime remains pinned to
`d90c92988a9f99d0580d2ca3f7efd6ec3a7b7845`; supported US XBE SHA-256 verified as
`3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc`.

- Compiled tests execute the actual stream/buffer adapters and Xbox ADPCM
  decoder with deterministic clocks, guest memory and a fake output device.
  Original pinned sources fail eight of thirteen cases; patched sources pass
  all thirteen, including starvation, submission, pause, flush, exact completion,
  fallback/device loss, diagnostic drift, partial ADPCM blocks and refused
  looping/nonlooping buffer submissions. Together with patch upgrade/tracking
  tests, 40 checks passed in 93.17 seconds. The final clean patch's audio suite
  also passed all thirteen checks. CI hooks were added for Windows/Linux/macOS;
  those new remote matrix checks have not run.
- Final Windows compilation and linking succeeded. Network, texture and display
  checks passed, but the full Build command stopped at the existing
  `tests/pc_prompts.c:55` foreground-window assertion on two attempts. Save-path,
  controls, settings and debug checks passed separately. The network executable
  stamp was sealed separately after these checks and content verification
  passed. This is not a passing final full Build. Final executable SHA-256:
  `44f92c618c768b98db0d0600bd2c491ea3f883d5c3fc87b032f0f00d3a18c143`.
- A bounded 165.12-second run used isolated saves and both isolated display
  configurations, with the real audio device active and muted output. It rendered
  profile/menu screens without a crash or unresolved call. Inspection of the
  final capture confirms profile-name entry; the probe did not reach story or
  gameplay. It cannot establish audible quality or story A/V synchronization.
  Probe executable SHA-256 was
  `da1ed1873074bffaa97b532d96b1015f6433b96b6294e0e156bef93872aa0b7b`, before a
  comment-encoding cleanup and rebuild, with identical audio implementation.
- Personal controls/display and the pre-existing unrelated package-playable.py
  edit retain their baseline hashes. No personal saves were used. Disposable
  build path-test data and six known isolated test partition images were
  recycled, preserving profiles and metadata. Those six images represented
  5,243,406,848 logical bytes but only 133,632 physically allocated bytes.

Evidence: `build/issue-8-audio/VALIDATION.json`, `baseline-full.json`,
`partition-cleanup.json`, `game.json`, `game-160.png` in the same directory;
`logs/issue-8-final-regressions.log`, `logs/issue-8-build-verified.log`, and
`logs/issue-8-game-smoke.log`. Story listening, repeated level starts and A/V
synchronization remain pending. MAC-018 records SDL device-position adaptation
and native Mac validation. Other-device synchronization is unverified. No
commit, push, binary packaging or release publication was performed for #8.

### Follow-up: first-mission distortion and missing stream headroom

The user reports harsh static/distortion at the first mission's start with the
initial build above. That playtest establishes that the initial timing changes
did not resolve the reported issue. No audible-fix claim is supported by those
earlier passing regression tests.

Read-only inspection of the isolated game's stream settings found a further
volume mismatch: the XDK stereo voice records 600 hundredths of a dB of headroom
and subtracts it from SetVolume, but the host stream used the raw SetVolume
argument. Stereo audio was therefore about twice the intended amplitude. The
shared stream adjustment now includes headroom alongside mix-bin/3D attenuation;
it still falls back unchanged outside the checked XDK layout. A new regression
fails before this correction and passes afterward, including a headroom change,
mix-bin attenuation, and unsupported-layout fallback.

- Audio suite: 14 passed in 2.42 seconds. The full Windows `tsfp.ps1 Build`
  succeeded, including the formerly blocked controller-prompt check and all
  remaining native tests. Its normal cleanup and executable stamp completed.
  Network-content verification and repeated patch application (zero updates)
  passed. Executable SHA-256:
  `c645c07abd7076dd78122498990868a09b3adb730d991704d1619f3c0f5cc4f7`.
- A bounded isolated replay of the preceding executable reached the first
  mission's opening (a capture was visually reviewed, then overwritten by a
  later menu probe). Read-only process
  inspection retained twelve compressed packets. All their ADPCM block headers
  were valid; none of their independently decoded samples clipped individually.
  These observations do not rule out mixed-output overload or prove audible
  correctness. Subsequent profile-navigation attempts remained at name entry,
  so the headroom build has menu/device-level evidence, not another completed
  story-cutscene probe. Its menu music diagnostic reflects the extra 6 dB
  attenuation (`-1737` versus the preceding `-1137`).
- The probes used the existing isolated pickup-range settings/saves, muted real
  device output, fixed time/capture/log bounds and read-only PDB/memory access.
  They did not change personal saves/configuration or the executable in memory.
  Local diagnostic source/data remain ignored in `build/issue-8-audio`.

Evidence: `logs/issue-8-headroom-baseline.log`,
`logs/issue-8-audio-headroom-tests.log`, `logs/issue-8-headroom-build.log`, and
`build/issue-8-audio/HEADROOM-VALIDATION.json`. The headroom mismatch is fixed;
its effect on the user's reported distortion still needs an audible retest.
Changes remain local and uncommitted; no release was published. MAC-018 still
requires native Mac validation.

### Follow-up: stale music packets at level transitions (2026-10-16)

The user's headroom-build retest still had harsh distortion. A bounded,
read-only capture attached to their human-controlled Windows playtest (PID 7616,
headroom executable `c645c07a...`) isolated a separate fault. Music stream slot
266 retained three old 73,728-byte packet addresses across the menu-to-story
transition. Their memory now contained unrelated game data. Respectively
1,642, 1,605 and 1,770 of 2,048 stereo ADPCM headers were invalid; independent
decoding clipped 16.04%, 13.48% and 18.30% of samples. Captured host PCM matched
that corrupted input. The next five music packets had valid headers and no
clipped decoded samples. Captured cutscene/dialogue PCM matched its source.
Earlier probes only saved the 22,042 Hz story streams, missing this 32 kHz
music stream. Reducing headroom could not repair its corrupted input.

The shared adapter intercepted ordinary Flush but neither FlushEx nor Release.
The pinned XBE's stream stop routine calls FlushEx with zero timestamp and ASYNC;
its Release method can destroy the stream while the host still retains packets.
The new `dsound-stream-lifecycle.patch` retires the host voice and packet model
only on final Release, preserving non-final references and allowing the allocator
to reuse the same interface address with fresh format/queue state. It handles
immediate FlushEx and zero-time ASYNC (packet callbacks completed in DoWork),
blocking submission/status completion while the asynchronous flush is pending.
Other scheduled/envelope FlushEx modes retain the existing XDK fallback and
remain outside this fix's validated scope.

Three lifecycle tests reproduced stale host queues before the hooks; 17 audio
cases passed after the initial implementation. The async regression also checks
that intervening GetStatus/Process calls cannot complete cancelled packets from
the wall clock. Build discovery initially missed the new HLE wrappers, producing
unresolved original-body symbols. `tools/recomp-generation.py` now fingerprints
HLE export/original/import declarations, with a regression for addition/removal
and body-only edits. Wrappers are regenerated from source, never edited by hand.

Read-only inspection did not modify the running process, inputs, personal
settings or saves; the user navigated and closed the game. Captures are capped
at 12 MiB and 240 seconds in ignored `build/issue-8-audio/live` with source/PCM
metadata and analysis. Current local rebuild and post-fix listening results are
recorded below when available. Changes remain uncommitted on
`fix/issue-8-audio-timing`; native Mac listening/build and other-device
synchronization are unverified (MAC-018).

Lifecycle validation: 45 checks passed in 187.94 seconds (17 actual audio-adapter
cases, one HLE-generation invalidation case, and 27 patch/branch-transition
checks). Full `tsfp.ps1 Build` passed all native checks and recycled its disposable
path-test data. Generated wrappers now intercept Release at `0x00407286` and
FlushEx at `0x004074EC`, with their original bodies retained. Network game-content
verification passed. Windows Release executable SHA-256:
`1482f8ca68663f6980474efb801193fa0726424e6b92678f71be0a5919e33852`.
Evidence: `logs/issue-8-lifecycle-baseline.log`,
`logs/issue-8-lifecycle-regressions.log`, `logs/issue-8-lifecycle-build.log`,
`build/issue-8-audio/live/music-corruption.json`.

The user retested the rebuilt first-mission opening and confirmed: "The opening
sounds clean." Read-only capture of that playtest retained eight 32 kHz music
packets, all with valid ADPCM headers and zero clipped decoded samples, versus
three corrupted stale packets before the fix. The new run logged FlushEx calls
(43 by the sampled mission opening, versus zero previously), confirming the
previously missed stop path now reaches the host adapter. The user then closed
the game normally. This verifies the reported first-mission distortion; other
levels, long sessions and full A/V synchronization remain broader playtest gaps.
Post-fix evidence is in `build/issue-8-audio/live-fixed/packet-analysis.json` and
`logs/play-20261016-030301.err`. The audio changes are being committed to the
user-requested `8-audio-issues` branch; no main merge or release publication is
part of this update.

Branch integration: the audio commit was applied cleanly to `8-audio-issues`
on top of `c8f8b31`, preserving the newer mouse-player and controller-sensitivity
changes. The full branch Build passed on retry after the known foreground-window
controller assertion failed once; no source changes were needed for that retry.
Its native checks, path-test cleanup and network-content verification passed.
Logs: `logs/issue-8-branch-build.log` and `logs/issue-8-branch-build-retry.log`.
Branch executable SHA-256: `7807b189c890a31fcb265dc4395ec67ad453837ea035efd30f287c9b8cc95710`.
The audio implementation is unchanged from the clean human playtest above.

## Launcher startup skipping (2026-10-16)

Launcher v1.2.1 implementation `bad108c` on `feat/launcher-skip-startup`, based on published main
`d83ca5e57d36da163680f7c21e0b5f52dd785faf`. Game settings → Graphics & resolution
now has a Startup checkbox, **Skip startup splash screens**, default off.
Saving persists `launcher.skip_startup_splashes` in portable controls JSON with
the existing backup, external-edit protection and rollback rules. Both the
direct launcher and newly packaged `Launch.ps1` explicitly set the existing
`RECOMP_XMV_PLAY` switch after sanitizing inherited overrides. This skips all
file-based XMV playback (opening EA/Free Radical logos and the idle attract
movie); story scenes use the game's separate rendered-cutscene system. The UI
explains the attract-video effect. No runtime patch or game rebuild is needed.

Windows x64, .NET Framework 4.8 C# compiler file version 14.8.10001.0,
Python 3.12.14: launcher
build passed 71 updater checks and 16 settings checks, including default-off,
save/reopen/disable, unknown-field preservation, malformed-value rejection,
inherited-override sanitization and actual child-process consumption. Five
pytest checks passed (three real PowerShell/synthetic-child launches covering
missing/false/true preferences plus two packaging checks). The WPF graphics
preview was rendered and visually reviewed; the checkbox/help text fit the
existing blue/silver layout. Evidence: ignored `build/launcher/startup-settings.png`.

`tests/launcher_startup_game.py` ran two bounded 20-second launches against the
verified published alpha.4 executable, SHA-256
`bc7cbb26ab90fc0af47eb8f8bff90d91c4074893cf443352d4e0c5bf41076345`, using its
existing D3D11 backend and pinned FFmpeg libraries. The supported US XBE hash
was reverified before testing. Normal mode played both opening movies; skip
mode completed both XMV objects with end-of-file status without decoding movies.
Both runs remained alive and rendered the player-selection screen, confirmed in
reviewed captures. No crash or unresolved indirect call appeared in their logs.
All input, saves and both display-config paths were isolated; this invocation's
disposable saves/frame dumps were removed. Evidence: ignored
`private/startup-validation/{normal,skip}/startup.png`, `game.err`, `game.out`
and `runtime/provenance.json`. This is compatibility validation of the published
game, not a rebuild of concurrent runtime work. Story gameplay/cutscene playback
was not retested. Mac UI/playback and other-device synchronization remain
unverified (MAC-021). Release preparation targets stable `launcher-v1.2.1` on
main, using the documented launcher-only packaging channel. `Publish-Release.ps1`
publishes the game `VERSION`; no new game version is needed for this option.
The Windows CI workflow now runs the launcher and portable-script regressions.

## Embedded launcher settings (2026-10-16)

Windows launcher v1.2.2 implementation `5eea0ca` on `feat/launcher-inline-settings`,
based on main `696421fd080a5bd57cb894b76302080f59f52485`. `SettingsPage.cs`
replaces the separate settings window: controls, graphics, startup options,
binding capture and confirmations stay in the main content area with the sidebar
visible. `Launcher.xaml`/`Launcher.cs` switch between Command center and settings
with active navigation styling. Home keeps the draft/tab and dismisses capture;
Cancel discards the draft; Save validates and keeps the page open. Atomic file
save, backup, stale-edit detection and rollback remain in the existing model.
The preview commands now render the whole launcher rather than a separate window.

Windows x64 / .NET Framework 4.8 / Python 3.12.14: both launcher binaries compiled.
The full launcher build passed 71 updater checks and 19 isolated settings
checks, including actual WPF sidebar/page navigation, 1000 × 720 minimum and
1152 × 772 layout sizes, draft retention/cancel, inline binding-conflict cancellation and
reassignment, successful/invalid save, and existing settings/environment checks.
Five portable-script and packaging checks passed. Both full-shell 1152 × 772
controls/graphics previews were visually reviewed; captures are in ignored
`build/launcher/inline-controls.png` and `inline-graphics.png`.

Evidence for the complete build/test pass: ignored
`build/launcher/inline-build-test.log`. No game runtime source changed or game
playtest was needed. Release preparation targets stable `launcher-v1.2.2` on
main through the documented launcher-only channel; no game update is required.
Native Mac settings/navigation adaptation remains
queued under MAC-021; other-device synchronization is unverified.

## Story Objectives health and shield (issue #11) — 2026-10-16

- Implemented on `11-objectives-tab-not-showing-health-and-shield`,
  based on main `d83ca5e`. Windows x64, CPython 3.14.2, MSVC x64, D3D11.
  Supported US XBE SHA-256 verified before generation and isolated testing.
- The before capture shows the Story Status/Objectives panel without indicators.
  A read-only sample of the user's open Objectives screen found menu flags
  `0x191`, a valid health/shield data pointer, and the display enable flag set.
  The original quick-menu opener sets `0x100`; the original menu renderer tests
  this bit and skips `sub_0013AB60`, its health/shield renderer.
- The shared `story-objectives-health` patch temporarily masks `0x100` only while
  drawing active Story quick menus, then restores it and transient `0x80`.
  The original indicator data, animation and remaining visibility guards stay
  in use. Input/pause state and other game modes retain their behavior.
- The first corrected live capture confirmed both indicators and calls to the
  original renderer, but the user reported clipped sides. Replay identified
  indicator vertices at guest x=-48.64 in a 640-wide viewport. The new
  `src/pc_objectives.h` and health-draw wrapper shrink the menu indicators to
  the current viewport with a 4% inset on each side, retaining a common scale
  for both axes and smaller animation phases. Guest viewport coordinates map
  through the existing renderer to any selected output resolution. The scale
  is recomputed each draw, supports split-screen, and leaves normal gameplay
  HUD draws alone. Caller arguments are restored afterward.
- The user confirmed the viewport fit, then reported the crescents covering
  the Objectives panel edges. `build/windows/Release/frame001.d3dcap` reproduced
  this with a nonempty first-mission objective. The panel still used its original
  80% width while the gauges had shrunk. The shared Objectives builder now uses
  the same fit ratio for both container and text-measurement widths, restoring
  the shared US constants immediately afterward. Font size remains unchanged;
  long objectives use the original wrapping/scrolling layout. Live verification
  of this additional spacing correction initially failed: the next capture was
  unchanged. The builder wrapper incorrectly treated the camera base as its
  viewport, so invalid dimensions bypassed sizing. The US menu renderer uses
  the viewport embedded at camera+0x224. The wrapper now uses that same offset,
  and the fixture separates the camera base from its embedded viewport.
  Seven focused tests pass, including a mutant reproducing the missing offset;
  the user confirmed the rebuilt correction is working on 2026-10-16.
- Seven focused regressions passed: production visibility and sizing wrappers,
  deliberately broken variants, and the original checksum-verified US menu
  routine. Tests cover full pause/inactive menus, other modes, invalid indices,
  hidden widgets, display-disabled state, guest stack/register preservation,
  changing 4:3/16:9/16:10/ultrawide/portrait dimensions, local viewports 1–4,
  off-center animation, matching panel/text-measurement width and restoration
  of shared constants. A deliberately unscaled panel reproduces the regression.
  Portable tests run in the Windows/macOS transition
  workflow; the original-XBE test skips in CI without private game data.
- Final camera-offset correction: the complete Windows Release build and all
  native build checks passed, including the prompts/focus test. Seven focused
  Objectives tests passed. Before this last offset correction, all 34 focused
  and patch-transition regressions passed together in 207.45 seconds. The
  historical upgrade fixture excludes the new patch when constructing its
  pre-letterbox state, then upgrades to the complete stack. `git diff --check`
  passed. Earlier sizing builds intermittently failed the unchanged prompts
  test's foreground-window assertion; both later full builds passed it.
- The user confirmed the indicators fit inside the screen and, after correcting
  the camera+0x224 viewport lookup, confirmed the panel-spacing fix is working.
  This is user-reported Windows visual validation. Automated UI verification
  was stopped by the user's Escape key. Live coverage of all resolutions,
  split-screen and long/multiple objectives remains unverified; numerical
  layout cases cover the viewport sizes listed above. Mac compilation/gameplay
  remain unverified; see MAC-023. Local captures and test provenance are under
  `build/issue-11-objectives`; the final full-build log is
  `logs/issue-11-build.log`, and the earlier combined 34-test result is in
  `logs/issue-11-regressions.log`.
- PR #55 conflict resolution integrates main `de23272` while retaining the
  Objectives implementation and fixtures byte-for-byte. CI keeps the Objectives,
  cinematic/decal, terminal, FOV and launcher checks; the historical-upgrade
  fixture retains all dependent-patch exclusions. Objectives is now MAC-023,
  preserving main's MAC-022 FOV entry and all existing validation notes.
  On Windows x64 / Python 3.12.14 / MSVC x64, all 44 merged Objectives, cinematic,
  decal, terminal, FOV and patch-transition checks passed in 129.91 seconds.
  The supported US XBE hash was reverified. The complete patch stack was applied
  to an isolated 3,986,182-byte source fixture from pinned upstream `d90c929`;
  no game assets were copied, and the other task's runtime/build was untouched.
  No full game rebuild or new live playtest was performed for this conflict-only
  update. Evidence: ignored `build/pr55-validation/tests.log` and
  `fixture-provenance.json`. The PR diff passes `git diff --check origin/main`.
  The follow-up main merge adds release documentation and the bounded cold-WPF
  startup allowance; native sources/patches are unchanged from the 44-test pass.
  The current launcher build passes 71 updater and 21 settings checks
  (`build/pr55-validation/launcher.log`). Objectives notes are appended here so
  new status entries at the top do not repeatedly conflict with this PR.
