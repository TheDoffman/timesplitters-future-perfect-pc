## Game update progress (issue #558)

PR588 native CI follow-up: NativeStartup.PrepareGameHelper owns a GUID-scoped
.launcher/game-helper-* directory, copying the host and Mac bundle-side *.dylib UI
libraries before launch. This isolates the progress UI from bundle replacement.
Program cleans only that validated helper directory after the UI exits, preserving
unknown entries. Native package tests use the production preparation path, verify
copied Mac library hashes and rendered progress, and check resource cleanup. Linux
waits up to20s for a real ready frame while its parent stays alive, then releases the
parent explicitly; a4s cold-start latency assertion was inappropriate for CI.

`UpdateProgress` in UpdateRestart.cs carries optional shared phase/percentage reports
and logs phase durations. Core.Unpack/InstallPending/Apply/Rollback and the automatic
save backup forward them without changing hashes, journals or recovery policy.
WPF and Avalonia show extraction progress. NativeUpdateWindow owns the copied game
helper UI (including parent wait, recovery, install and restart); Program dispatches
helper arguments through Avalonia before ordinary startup/storage checks. HandoffGame
copies the helper asynchronously; cancellation ends before the staged handoff.
Windows uses a thin WPF helper window over the same worker. Progress callbacks cannot
abort a transaction. Tests: launcher_restart.cs and LinuxTests.UpdateHelper exercise
phase order, preserved files, bad payloads, observer failure, cancellation and rendered
helper/restart lifecycle. Native Mac execution remains unverified.

Normal game updates compare eligible installed runtime files against the verified
incoming hashes before making their change list. Equal files keep their timestamps
and are omitted from both replacement and the rollback journal; every replacement/removal,
including BUILD.json, retains a recovery entry. Native executable modes
are corrected on retained files. Settings/textures/independent launchers keep their
existing preservation rules. Staged hashes are still all checked before reuse; an
old manifest alone is never evidence. Core.ApplyScope resolves its validated change
list by dictionary instead of scanning the list for each journal entry. Shared
restart fixtures cover unchanged files, corrupted targets, staged tampering,
executable permissions and exact previous-byte rollback of changed targets.
