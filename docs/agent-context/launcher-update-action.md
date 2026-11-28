# Home update action and installed-version opt-out

Implemented locally on feature/launcher-update-now, based on main6d74f6015.
The original Launcher Redesign checkout has concurrent Console/focus changes and
is left untouched; this work uses the separate Launcher Update Now worktree.

GameUpdateActionPage.NewerOnCurrentBranch is the shared pure eligibility rule.
The compatible catalog release must be strictly newer than the installed semantic
version, and both the release and selected track must match the installed tag's
canonical branch. Equal/older/other-branch releases, invalid/unknown installed
versions and first installs do not acquire the Update now caption. Initial
installation and explicit channel/version switching keep their original flows.

WPF AppearanceCommand and native PrimaryAction promote the existing UpdateButton
command ahead of Play when eligible. Downloads keep Pause/cancel priority; game
running state keeps disabled Playing. Busy/check/offline/recovery guards remain
with their existing host commands. Native Check clears its previous candidate and
disables the Update command before awaiting HTTP, preventing stale offers during
checks. Native Refresh also recomputes update availability after game exit, so
a candidate checked during gameplay becomes actionable once the game closes.
There is no background automatic installation.

The shared PlayCurrentVersion text button appears above the main action only
while an eligible update and a playable current installation are available. Its
event routes to the existing guarded Play command on each host; it neither writes
branch preferences nor dismisses the update for future launches. Missing data,
busy/download/game-running states hide it. WPF keeps existing recovery priority.
The action dock grows only when the link is visible. SteamOS gets44px touch height
and the native version popup adjusts its lower margin and available height.

Regression routes: shared UI checks cover Stable/Beta/named tracks, equal/older,
other selected/release branch, malformed/missing metadata and guarded opt-out
button dispatch. The focused Windows --play-state test covers real installation
process identity, Update now/Play/Playing/offline state and opt-out visibility.
Native --test-ui exercises the actual native main and secondary controls through
update, running, offline and up-to-date states with an isolated synthetic root.
No network download or game launch occurs in those new UI state tests. Existing
installer/download/session tests remain the underlying command validation.

## Local validation evidence

Windows full -Test passes95.12s, plus the focused --play-state check verifies the
new main/opt-out states while retaining its unrelated process-identity probe.
The first Windows run was blocked by an unrelated game; it was not stopped by
this task. A later normal run passed. Linux --test/--test-ui/package passes71
shared UI groups, including native post-game restoration, and real native
package install/repair/helper restart/personal-file preservation checks. Python
layout/startup checks and native page/size render checks are run separately.
Mac-specific execution is not newly verified by this task; its shared Avalonia
adapter and pure policy are exercised on Linux. Physical Deck checks remain
outside this local change. Preview state uses isolated synthetic release tags;
no user game is downloaded, started or updated for the screenshot.

Implementation and validation use the separate worktree, preserving the other
Console/styling work. Source integration and release publication are separate;
this change does not publish a launcher or game release.
