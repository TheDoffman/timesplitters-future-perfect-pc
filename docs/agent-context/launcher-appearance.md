## Coordinated color treatment (2026-11-25)

The launcher retains its navy/blue identity while echoing the game's permanent
contrast/saturation treatment documented in uplink-color.md. This is an art
direction choice, not the game's numeric shader curve applied to UI or artwork.
Shell #071321 and input #08182B deepen the foundation; secondary #173A59,
accent #65C7FF and primary/focus #85D4FF make actions clearer. Primary ink is
#06223B. Existing light text, semantic success/warning colors, disabled gray
Playing state, artwork bytes/opacity, serif fonts, geometry and commands stay
unchanged. Shared PageTheme/XAML/pages remain canonical; native adapters and the
layout generator use the corresponding colors, including startup/update dialogs,
release-note links, tutorial spotlight and Deck focus.

Windows builds and renders Home/settings/tour/updater/940x640. Keyboard-focus and
installation-specific Play checks pass. Full Windows updater/shared/appearance
suites are blocked by unrelated running game sessions; the unchanged serif build
reproduces the same appearance failure. Do not stop those sessions for this task.
Mac66/Linux68 shared UI groups and60 page/size renders per native host pass;
Linux package/helper preservation checks pass. No physical Deck acceptance or
release publication. See STATUS.md for exact scope and receipts.

## Serif typography preview (2026-11-25)

The local serif-typography branch changes launcher inheritance from Segoe UI to
Georgia, including the artwork fallback labels. Shared Ui.Serif supplies Georgia
on WPF and Georgia/Liberation Serif/DejaVu Serif fallbacks on Avalonia. Native
layout generation preserves that serif stack instead of substituting Arial.
Startup, updater and confirmation windows plus Windows release-note prose follow
the same type treatment. Console/report/code text retains explicit monospace;
Windows title-bar glyphs keep Segoe UI. Play uses18px on both presentation hosts.

This is a local visual revision, with no launcher version or game-setting change.
Use normal Windows/native build checks and the existing density/page-size render
matrix to check wrapping, navigation and action visibility. The original embedded
TimeSplitters artwork and palette are unchanged. No font binary is redistributed.

Windows full -Test passes70.59s; Linux68 shared UI groups and60 page/size checks
pass, including the handheld serif render. Font/layout propagation Python checks
pass6 tests. Home and settings previews are local visualization artifacts. Mac
compiled normally, but SDK ThirdPartyNotices.txt copying blocked in fcopyfile/read;
completion uses the identical94355-byte SDK notice from the checksum-verified
previous local ZIP, then the original signing/manifest/test/package tail. This
recovery is confined to generated build output; no SDK or installed app was changed.
Recovered Mac signing/package/tests complete successfully (66 shared UI groups);
Mac also passes60 native page/size appearance renders.

## Release-readiness and mouse focus correction (2026-11-25)

Both Launcher.xaml's window button style and PageTheme.xaml's page buttons had
IsKeyboardFocused borders that were also activated by mouse clicks. WPF now uses
NavigationFocus/FocusVisualStyle for keyboard-only blue adorners; pointer focus
keeps the original border. CheckBox focus uses the palette blue. Native
generate_layout.py skips WPF-only focus setters and maps the same blue to
:focus-visible; ordinary mouse selection does not acquire the keyboard border.
DeckControls retains its separate controller focus outline. No control is made
unfocusable and command/selection behavior stays unchanged.

launcher_appearance.cs focuses a real themed button and asserts its normal
border remains and keyboard focus style is available. Mac/Linux VerifyPackage
adds Settings/pc-controls.json, Data and unknown user-file sentinels through the
real packaged self-update/repair path, beyond the existing saves/BUILD assertions.
SteamOS Home and Deck help explicitly label support experimental. Packaged guide
matches that wording. Hardware acceptance remains pending.

## SteamOS handheld adaptation (2026-11-25)

[Steam Deck implementation and acceptance](launcher-deck.md): shared mode/input
policy, Linux SDL adapter, focus/touch/text/file UX, game lifecycle and native
render/test routes. Desktop/Windows/Mac settings and controls are preserved.

## Review follow-ups (2026-11-25)

Same-session Open switches completed setup into the overview only when setup is
ready and no longer pending; initial/resumed setup still gets its completion card.
GameVersionMenuPage now owns track metadata, installed markers, current/selected
comparison, confirmation and offline/busy guards for both frameworks. WPF
GameChannelMenu and native NativeAppearance provide only popup/input/install
adapters. Check success/failure drives AppearanceOffline/appearanceOffline;
changing connectivity closes stale confirmations while installed Play is kept.
Native Check failure also disables the stale Update action until the next check.

NativeStartup retains failures in the themed window with Retry, View logs and
Open installed launcher. Each retry owns a fresh cancellation source; explicit
fallback cancels the active check first and launches once. Check/launch/log
injection supports asset-free recovery regressions; production uses normal
ReleaseClient/SelfUpdate and NativeWindow. Shared UI checks cover replay, picker
parity/offline/stale selections and native failure/retry/logs/fallback.

## Update window presentation (2026-11-25)

UpdateWindowPage.cs shares the dark shell, compact brand/divider, typography,
scrollable status, slim blue progress and aligned actions for Windows game-helper
and native startup/game-helper windows. Both native startup progress phases and
Windows startup use transfer summaries, with verifying/preparing indeterminate.
WPF Bootstrap retains its independent named recovery controls and handlers while
matching the palette/layout; recovery actions form an even-width row and the
installed-launcher action is full-width. Dark title bars match its dark content.
The startup render canvas follows the new620x330 client area.

Update checks, timeout, cancellation, Retry/Open folder/Logs, copied-helper lock,
file preservation and restart remain host-owned. Existing startup long-error
layout checks and updater/helper preservation regressions cover these routes.
No files are replaced or downloads started by the splash render route.

## Optional tour after setup (2026-11-25)

The setup-ready card now offers a full-width Show me around action above the
Settings/Finish/Play row. Explore is available only after readiness, switches the
same shared page into its six-step returning-player overview and resets progress
and Back semantics. It clears setupPending without dismissing or launching any
host command. Subsequent host state refreshes cannot reset the overview. Existing
skip/completion persistence and direct Play/Settings/Finish choices remain intact.
Shared UI coverage checks premature requests, all six steps, Back, completion,
no host action, no unintended close and pending-marker cleanup on all hosts.

## Guided new-player setup (2026-11-25)

LauncherTutorialPage now selects a three-stage actionable setup flow for an
incomplete installation: install/check, ISO or extracted-folder import, then
explicit Play/Settings/Finish. Returning installations retain the six-card tour.
UpdateSetup receives host installation/readiness status; advancing never implies
success. Data already present but unready routes to review without overwriting it.
Windows uses cached validated data; native caches CheckData against XBE metadata
and asset directory timestamps, with launch preflight remaining authoritative.

SetupRequested dispatches through each host's existing commands. Adapters suspend
the scrim and restore normal Home input while the real download/picker operates;
then restore the guide using actual state. Cancellation/failure retains the step
and displays the host status. Download pause/retry and updater ownership remain
unchanged. Settings and Play require explicit clicks after readiness.

Beginning setup persists setupPending in the existing tutorial marker, without
marking the tour dismissed. Installer restarts resume at the next needed stage,
including manually replayed setup after a prior dismissal. Skip/Finish clear it;
unknown fields and newer schemas/revisions remain preserved. Render-only
PreviewStep never clicks commands or persists preferences. The spotlight and
input shield remain unchanged. Shared UI coverage includes install requests,
separate ISO/folder routes, cancellation/failure, existing-data review, restart,
readiness without auto-launch, completion, future markers and inert rendering.

## Tutorial copy refresh (2026-11-25)

The shared LauncherTutorialPage.Render copy now explains when each feature is
useful: playing/setup, regular versus preview builds, installation tools,
personal settings, backups/troubleshooting and community/release information.
Headings and location hints use conversational language. This is text-only;
targets, spotlight brightness, navigation, persistence and offer revision stay
unchanged, so existing users are not prompted to repeat the tour.

## Tutorial spotlight brightness (2026-11-25)

TutorialShade in LauncherTutorialPage.cs draws an even-odd scrim with a genuine
transparent opening around the live target. Both adapters use the same geometry.
The overlay's transparent parent remains an input shield across the opening;
background hit testing is suspended and keyboard focus stays in the card. The
home is no longer disabled by the tour, which avoids disabled-control tinting in
the spotlight. Existing action eligibility is preserved. WPF motion uses an
explicit Paused flag; native motion also excludes an open tutorial. Closing the
tour restores hit testing and normal animation policy. A shared geometry check
covers a clear target, dim background and full scrim on the welcome screen.

## First-use and returning-user tutorial (2026-11-25)

`LauncherTutorialPage.cs` is the single WPF/Avalonia tour model, copy, card layout
and persistence policy. The *Page.cs source glob includes it in every host. The
home adapters offer a skippable invitation once, after the launcher is active,
on Home and idle, with no game running. An existing installed game/data selects
the returning-user introduction; an unconfigured installation gets setup guidance.
The invitation leads to six steps covering Play/import, versions, tools, settings,
backups/support and release notes/community. It never invokes these operations.

Each adapter blocks background input, cycles keyboard focus in the card, outlines
the actual home target, follows layout/resize changes, pauses home motion and
handles Escape as Skip. The existing window controls remain usable. Launcher tour
in the Tools header replays it. Replay returns to Home through the existing
navigation path, preserving any native settings draft; it is disabled while busy
or playing and refuses an active settings prompt.

Dismissal/finish writes only `.launcher/tutorial.json` with schema1/revision1,
using Core.Write's atomic replacement. The offer decision is cached per window;
it is not read from disk every timer tick. Extra fields are retained and newer
schemas/revisions or oversized markers are preserved. A write failure closes the
tour with a nonblocking warning. Closing the app mid-tour leaves it available next
launch; explicitly choosing Not now/Skip or finishing prevents another offer for
this tour revision. No game, save or settings files are modified. Native render
fixtures use persist=false; automatic offers are suppressed in render routes.

Two launcher_shared_ui groups cover first use, Back/Next/Skip/finish, replay,
returning copy, layout-revision offers, future preferences, render no-write and
personal-file preservation on all hosts. Native --render-tutorial-preview and
--render-tutorial-stepN-compact produce isolated screenshots. Windows full -Test,
native shared suites and native scaled renders validate this change. No publish.

## Play eligibility and installation identity (2026-11-24)

`Core.GameRunning(root)` compares the candidate process executable to this
installation's LauncherPlatform.GamePath, using host path comparison rules. A
confirmed different installation is ignored; inaccessible/unknown live process
identity remains conservatively blocked. The parameterless global guard remains
unchanged for updater/recovery operations. Windows Play and native Play perform
this scoped check immediately before launch as well as for presentation.

The appearance timer now refreshes the native Play button from the existing
cached installation/data validation and busy state before mirroring it. It does
not hash game assets each second or force-enable an invalid installation. This
allows an externally started game to disable Play and its exit to restore Play
without reopening the launcher. Both WPF and Avalonia use this observation path.
`tests/launcher/launcher_play_state.cs` uses two inert owned child processes and
checks unrelated paths, stale disabled state, own start/exit, Playing, busy and
missing-data guards. Windows -Test compiles it and runs --play-state. It can also
run alone while an unrelated game test is active. No unrelated processes are
stopped. Global updater tests still require every game process to be closed.

## Shared native presentation and scaling (2026-11-24)

The continuing local branch now includes the Mac/Linux adapter in
`macos/NativeAppearance.cs`. Both native projects compile it. `NativeWindow`
retains its original command, lock, transfer, process, settings and storage
implementations; the adapter rehosts their controls and observes the same enabled
states. NativeMaintenance exposes its existing area and selected page to the
adapter without a second maintenance model. Versions are reached from the home
picker; native Install still performs the original exact-target confirmation.

`macos/generate_appearance.py` translates framework calls in the canonical
`appearance/MinimalSettings.cs`, `MinimalTools.cs` and `Restyle.cs`. Both host build
scripts generate NativeMinimal.cs under the build output and pass AppearanceSource
to MSBuild. Layout decisions and original settings controls stay in one source.
Brush adaptation recognizes ISolidColorBrush, including immutable XAML brushes;
rehosted Installation text/actions retain explicit dark-theme contrast.
Generated control-retention checks count TemplatedControl, since Avalonia's
Control also includes layout panels that WPF treats separately. The build receipts
include the generator and canonical helpers. Do not edit the generated file.

Native animations use finite cancellable RunAsync cycles (Avalonia11.3.22 refuses
infinite RunAsync). They stop while the home is hidden, minimized, a menu is open,
or high contrast is requested. NativeMotion reads NSWorkspace's reduce-motion
preference on Mac; Linux uses the available contrast preference. Window placement
is installed after FitLauncher's Opened handler so fitting cannot overwrite the
restored size. Saved normal geometry and maximization are bounded to current
screen work areas; render/test-session routes do not persist placement.

Windows FitWindow wraps the approved logical layout in a DownOnly Viewbox and
fits the initial window and DPI changes to the current monitor. It retains the
940x640 logical content minimum while allowing smaller physical work areas.
Native FitLauncher retains its existing uniform viewport scaling contract.
Neither check changes the user's operating-system display settings.

Native `--render-appearance-check OUTPUT_DIRECTORY` exercises real settings Back
and draft retention, Playing/channel locks, control retention, closed diagnostics,
equal tools tabs and visible primary/back controls across15 views and four client
sizes (940x640,1184x684,1600x900,800x600). It records actual RenderScaling and rendered
pixel sizes in checks.json, renders60 screenshots and advances/cancels a real
animation clock. Render failures print an exception and exit nonzero rather than
leaving a disabled fixture window open. Linux acceptance launches separate X11
processes with AVALONIA_GLOBAL_SCALE_FACTOR=1,1.25,1.5,1.75,2; the recorded host scale
must match the requested factor. Mac's available display reports1x. Windows
--appearance additionally checks density/work-area geometry at100–200%; those are
render-density and logical desktop simulations, not a physical monitor DPI switch.
See STATUS for measured results and remaining hardware acceptance limits.

## Local redesigned Windows presentation (2026-11-24)

Uncommitted branch `feature/launcher-redesign`, based on main
`b3083d68f`. `LauncherAppearance.cs`, `LauncherAppearanceBridge.cs` and
`appearance/*.cs` wrap the existing WPF controls. The bridge delegates to the
original LauncherWindow commands; it does not reimplement download validation,
update handoff, game/session monitoring, backup, restore or settings persistence.
The home action mirrors native eligibility, status and cancellation. Channel
selection comes from the real release catalog and invokes DownloadGame for the
confirmed exact target. A missing installation must not mark Stable installed.
Settings reuse the original draft and Save/Cancel flow; caches use weak keys.
Window placement lives under the installation's `.launcher/window-state.json`.
The old render-preview switch remains a baseline renderer, not this new shell.

Windows build/test source lists and cache inputs include the appearance folder.
`launcher-tests.exe --appearance` exercises actual UI commands against synthetic
HTTP, corruption rejection, recovery, disk settings, backup/restore, verification
and an inert process probe. Live isolated install and game-menu/Playing-state
acceptance are recorded in STATUS. Existing Avalonia hosts retain their current
presentation and shared policies; visual adapter parity is explicitly pending in
MAC_PORT_BACKLOG. No publication or installed-user-launcher replacement.

## Validation

## Working launcher appearance validation (2026-11-24)

`tools/build/build-launcher.ps1 -Test` compiles LauncherAppearance/bridge and
`appearance/*.cs` into Windows app/tests and runs `--appearance` after core
regressions. These files participate in cache identity. The new test drives real
WPF commands with isolated files and synthetic HTTP, including a first-install
Stable selection regression. Do not run update regressions during a real game
session: the existing global process guard intentionally refuses replacements.
Python launcher tests remain under `tests/launcher`. Native Linux checks use
`build-linux-launcher.py --test --package` in a separate WSL-native checkout;
passing them validates existing Avalonia behavior, not the new Windows layout.
Actual Mac execution, native visual adaptation and physical scaling acceptance
remain separate release checks. See STATUS and MAC_PORT_BACKLOG.
