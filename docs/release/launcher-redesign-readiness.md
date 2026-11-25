# Local launcher redesign readiness — 2026-11-25

This is the unmerged local redesigned redesign, based on b3083d68f. Existing
staged work is preserved. These checks do not publish, merge, change the release
version or replace the original installed launcher. Native builds use isolated
Mac/WSL checkouts, not the owner's normal installations.

## Results

| Area | Evidence | Result |
| --- | --- | --- |
| Windows build and regression run | Full build-launcher.ps1 -Test, 68.24s | Pass |
| Fresh install / upgrade | Windows local synthetic runtime-package install and older-version upgrade; real produced launcher ZIP install | Pass |
| File preservation | Save/settings/data/unknown-file transaction tests; real Mac/Linux launcher package self-repair sentinels | Pass |
| Download interruption and recovery | 28 download/recovery checks, including cancellation and invalid/truncated payloads; 15 startup recovery checks | Pass |
| Settings and existing features | 63 Windows, 66 Mac and 68 Linux shared UI groups; Windows54 settings and20 maintenance checks | Pass |
| Packaging and publication tooling | Python launcher suite:296 passed,5 skipped,52 subtests | Pass for executed cases; skips are not passes |
| Native package lifecycle | Linux ZIP install/corruption/repair/copied-helper restart; Mac signed package repair, managed migration and game-helper restart | Pass |
| Native layout parity | Actual Mac and Linux app renders,60 page/size checks each | Pass |
| Windows click and keyboard focus | Regression on real themed button, then manual automation of refreshed preview: tools tab selection and Tab navigation | Pass |
| Steam Deck hardware | Physical Gaming/Desktop, built-in controls, suspend/resume, overlay, SD cards and docked output | Deferred; experimental |

The install/upgrade fixtures exercise real installer code with synthetic game
payloads. They are not a fresh real-data installation on every native OS or a
campaign-play test. The earlier Windows real-data first-install/game-start journey
is recorded separately in STATUS; it was not repeated during this pass. Actual
published old-launcher discovery of a future public release cannot be accepted
before that release exists. No on-device Deck acceptance is claimed.

## Corrections included

Mouse clicks used to trigger an IsKeyboardFocused white border in both Windows
button themes. Buttons now use WPF's keyboard-only FocusVisualStyle in palette
blue. Native translation maps that cue to :focus-visible. Selection underlines,
hover feedback and controller focus remain. No buttons are made unfocusable.

SteamOS Home, Deck help and the packaged player guide label support experimental.
Mac/Linux package tests now explicitly retain custom settings, game data and
unknown files through self-update/repair, alongside existing saves/BUILD checks.

## Local evidence and next release steps

Ignored build/integration-notes/release-readiness-{windows,linux,python}.log
contain the local checks. Remote Mac logs are build/mac-readiness.log,
build/mac-readiness-package.log and build/mac-readiness-appearance.log. Linux
render evidence is build/readiness-appearance. Refreshed local Windows preview
and native ZIP copies live under D:/CodexBuilds/TSFP-launcher-redesign.

Before publication, allocate/verify the intended unused release version, write
release notes for the complete redesign, and run the normal three-host workflow
on the reviewed merged revision. Follow launcher-releases.md. Public asset hashes
and update discovery are post-publication checks. Physical Deck testing remains
explicitly deferred until after release at the owner's request.

## CI publication-verifier follow-up

The initial three-host PR run passed Windows and Mac, and Linux native tests,
but Linux release stamping rejected the new Support/Launcher/SteamDeck.txt guide.
The publication allowlist now includes that exact Linux-owned path, matching the
installer. A real ZIP verifier regression accepts the guide and rejects nearby
unowned filenames. Targeted publication tests:42 passed,23 subtests. Required PR
validation is rerun before merge; no package publication is performed here.
