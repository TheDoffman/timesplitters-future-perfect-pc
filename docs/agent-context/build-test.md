# Build, run and validation map

<!-- packet:start -->
Windows entry point is `tsfp.ps1` (Setup/Extract/Test/Generate/Build/Run/SmokeTest);
`setup.ps1` orchestrates first setup. Use `powershell -NoProfile -File tsfp.ps1
-Action Build` from the checkout. The builder uses pinned upstream and `.venv`,
applies the full patch stack, checks guest-generation fingerprints/wrappers,
regenerates when needed, configures MSVC x64 and builds/tests native targets.
Output: `build/windows/Release/futureperfect_recomp.exe`. Do not bypass a failed
patch phase with direct edits or suppress the first compiler/linker diagnostic.

Mac: `.venv/bin/python tools/build-native-mac.py`; `--generate` for stale guest
code, `--setup` only for explicit setup. Output `build/macos/futureperfect_recomp`.
Default renderer auto chooses common Vulkan on supported macOS26+ and GL on
older hosts; pinned SDK `config/vulkan-sdk.json`, loader `tools/vulkan_sdk.py`.
`BUILD.json` records selected driver/provenance. Do not copy Windows build trees.

Routine bounded Windows gameplay: `.venv/Scripts/python.exe tools/testing_profile.py
--play --seconds 120`; Mac: `.venv/bin/python tools/test-native-mac.py
--unlocked-profile --seconds 120`. Both use disposable fixture copies, not
personal saves. Source-only tests do not certify gameplay. Missing supported
game data or an unavailable host is a reported validation gap, not a reason to
invent results. Root AGENTS.md owns isolation, supported XBE and storage rules.
<!-- packet:end -->

## Choose the cheapest relevant check

Online v2 discovery now has immutable `arcade`/`story` session types. Omitted
types retain Arcade; Story capacity is four including the host. The directory
checks type before admission (including private codes), serializes final-seat
joins under its existing lock, and rejects type changes on either heartbeat.
`Session.create/join`, automatic hosting and the companion's `--mode` propagate
this type; Story fails closed against old directories and releases rejected
registrations. Existing `test_online.py` covers mode isolation, limits, concurrent
admission, expiry/departure and old-directory cleanup; the real loopback relay
and `test_online_host.py` TLS invitation checks run for both modes. These are
transport/admission checks only: no Story guest loading or gameplay is established
by them. No Story entry is exposed in the game menu by this infrastructure change.

Hosted-only contributors need no local model, Ollama service or installed router.
The context/maintenance checker uses Python and Git. The optional broker's Node
contract tests run in CI with disposable/mocked fixtures; they invoke no hosted
or local models. Running those contracts locally needs Node, while the supported
game builders retain their existing dependencies. Guide/index upkeep applies
equally to human and model-assisted changes and can produce a new CI failure
when a covered system changes without its documentation update.

| Change | First checks | Further evidence |
| --- | --- | --- |
| Agent context/index | `python tools/repo-context.py check`; `python -m pytest tests/test_repo_context.py -q` | Changed-path maintenance check against base |
| Patch loader/order | `tests/test_patch_tracking.py`, `tests/test_patch_updates.py` | Full stack transition fixtures; supported build |
| Guest generation/wrappers | `tests/test_recomp_generation.py`, float/vehicle join tests | Regenerate using verified local XBE, build |
| Settings/input/menu | Affected `tests/test_*` and native C fixtures | Relevant native target, bounded isolated acceptance |
| Renderer/shader/resources | Shared source contracts, `tests/test_renderer_host.py`, `tests/test_rhi_menu_sampling.py` | Windows WARP / Mac `tools/test-rhi-mac.py`; scoped scene evidence |
| Launcher | `python tools/launcher-compatibility.py --local` | Host build/lifecycle and affected `tests/launcher_*` fixtures |
| Issue automation | `tests/test_issue_forms.py`, `tests/test_issue_resolution.py` | Read-only fixtures, workflow checks; no posting for tests |

Enhancement routing uses the Ideas contact link in the issue chooser. The existing
`test_issue_forms.py` refresh fixture verifies that release updates preserve that
link and do not restore the removed enhancement issue form. The existing Windows
`launcher_maintenance.cs` draft checks cover the shared launcher Ideas destination,
empty diagnostics payload and unchanged bug-report round trip. These run in the
already registered issue-form and launcher workflows; native Mac builds consume
the same `DiagnosticIssueDraft.cs` URL policy.
The mocked version-refresh PR creation check also verifies that the generated
description preserves Ideas routing instead of referring to a removed form.
The v1.2.23 release bump changes only shared version metadata; existing launcher
release-policy, compatibility and both-host package checks remain the acceptance
route. The version-triggered Repository texture pack check must also pass before
merge. A version bump alone is not evidence of a published update.

Read each harness before assuming it is asset-free. Patch transition tests can
need pristine pinned upstream files and Git access. Use existing dependencies;
do not download/setup everything for a documentation edit. On Windows call
`.venv/Scripts/python.exe` explicitly if `python` resolves to the Windows alias.
In PowerShell each tool call has fresh process state: set `workdir`, use
`login=false` / `-NoProfile`, absolute or root-relative paths, and check
`$LASTEXITCODE` for native tools. A later `Write-Output` or pipeline can hide a
failed native exit code. A profile warning alone does not prove filesystem
operations are forbidden.

## Build failures and provenance

Capture bounded logs in ignored `logs/`, locate the first true error and inspect
the failing target's compile/link command. Keep a blocker separate from the
original game hypothesis. Do not try arbitrary `/JMC`, `/ASSEMBLY`, debug-format
or exception flags. Preserve existing working runtime edits; compare the failed
hunk with the ledger and source before any recovery. Do not reset an ignored
runtime assuming it contains no valuable work.

Check root revision and dirty paths, runtime revision and applied ledger,
generation fingerprint, executable path/hash/time and available `BUILD.json`.
Do not label an old binary as the current source. Avoid unnecessary complete
rebuilds or repeating unchanged failures; re-run when the inputs change.

## CI and cross-host limits

`.github/workflows/runtime-patches.yml` calls `tools/ci-runtime-scope.py` to
select transition/native/GPU/release checks. Launcher builds have separate
owners. New tests must be registered in the correct workflow; adding a test
file alone does not run it in CI. The lightweight plan also checks context
freshness and changed-system guide updates. Preserve required check names.
The Windows/macOS native step includes `tests/test_ghost_actor_overlay.py` for
the living-actor Ghost Gun composite. Its C fixtures test seven resolutions,
placement restoration, deferred draw flushing, unchanged primitive arguments
and mutations that remove the fix or break flushing/restoration. Wrapper checks
read the pinned runtime after the complete patch stack has been applied and
compile both Windows/native-Mac configurations. The original-US-routine case
requires the local checksum-verified `private/game/default.xbe` and skips in CI
without it. These contracts do not establish Mac scene fidelity or replace the
recorded Windows in-game confirmation; no personal saves are used.
The optional router context-reader contracts use Node's built-in test runner
(`node --test tests/repository-context.test.mjs`); game building still uses the
existing Python/native dependencies. Patch-index loader fingerprints normalize
text line endings so an older Windows CRLF worktree agrees with fresh LF clones.
`tests/repository-evidence.test.mjs` adds disposable Git/junction, source
provenance, redaction, budget and cancellation contracts for the optional broker;
the same lightweight plan runs both Node test files. No game build or GPU is
needed for these helpers. Native controller regressions and signed-in model
acceptance are machine-local checks, separate from repository CI.

From `c932bee`, builders audit repository `assets/textures` by default. A source-only
checkout can retain LFS pointers, but normal build/package must stop rather than
bundle pointers. For an explicitly texture-free development build use Windows
`-SkipTextures` or native Mac `--without-textures`; do not download a duplicate
pack just to run source/documentation checks. Release packaging includes the
audited committed replacements by default; see `tools/build_textures.py`.

`testing_profile.prepare_run` explicitly supplies `TSFP_TEXTURE_PACK_DIR` from
the checkout's `assets/textures` and `TSFP_TEXTURE_MODE=classic` after filtering
inherited diagnostics. Without that pack path F10 toggled modes but loaded no
replacements. Both Windows Test Lab and the native Mac unlocked runner consume
this shared environment; no runtime adapter change is needed. Asset-free
coverage lives in `tests/test_testing_profile.py` and
`tests/test_native_mac_tools.py` (both already selected by native CI). Windows
runtime loading must be checked separately; native Mac visual validation remains
pending. Missing packs retain the loader's Classic fallback for source-only tests.

Official Windows publication through `Publish-Release.ps1` /
`tools/publish-release.py` requires the complete repository pack, including
prepare-only, draft and resumed runs. Preflight audits hydrated DDS/mips,
sanitized JSON and the bundled manifest path/count/size using the shared texture
helper. Finished ZIP validation compares every DDS/JSON name and hash against
that pack, even after staging cleanup and immediately before upload. A missing
texture identity cannot bypass the published stable launcher 1.2.22+ requirement
for both Windows and Mac assets. Local texture-free/artist-override packages are
development outputs; the official publisher rejects them. Regression coverage:
`tests/test_publish_release.py` plus `tests/test_build_textures.py` in the existing
Windows/macOS release-validation CI step; no game assets are used by those tests.

`STATUS.md` holds measured results; `MAC_PORT_BACKLOG.md` records missing adapter
or validation work. Neither is an architectural index or a guarantee that all
listed historical tests still describe current policy. Current rules retain
original 60 FPS/60 Hz defaults, compatible native high-FPS opt-ins (separate
from retired interpolation), and distinguish API presents, GPU completion and scanout.
Use [testing-profile](../testing-profile.md), [building](../building.md) and
[macos](../macos.md) for full commands. Never auto-test through personal saves,
interactive launchers or persistent user settings.

## Bounded Arcade weapon-set route

On Windows, `tests/stability_transitions_game.py --run-name issue105 --scenario
weapon-set` reuses the isolated Test Lab runner, visits Arcade/Deathmatch/Vietnam,
opens weapon set, changes selection and backs out, then exits in about 100 seconds.
Review its named captures to confirm screen identity. `weapon_set_metrics` checks
settled five-second guest-swap windows against 50 FPS, saving JSON results.
The complete repair passes this unchanged threshold on Windows; partial cache
variants failed it. Counters measure guest swaps, not GPU completion or scanout.
`tests/test_text_read_scope.py` exercises query reduction, nesting, boundaries,
scope invalidation and actual prompt-hook ABI/fallbacks. `tests/test_frame_clock.py`
extracts the real Arm body for cheap/late/no-signal pacing contracts. Both run in
runtime CI. Mac runtime checks remain
unavailable on this Windows host.

## Compatibility repair controller

`tools/compatibility-worker.py` runs on the designated Mac outside CI. Install
with `--auto-merge` only for authorized repair follow-through. It publishes a
repair PR against its original branch, waits for both host workflows/checks and
reviews, merges the exact repair SHA, then requires fresh parent CI before its
SHA-conditional merge into protected main. It refuses missing effective gates,
changed revisions and failed follow-up CI. Legacy state is not automatically
enrolled. `status` includes the installed controller hash and merge mode.
`upgrade` replaces installed source/config under the worker lock after preflight,
preserving an existing dashboard LaunchAgent, pause and attempt history; it keeps
one previous source/config pair. Tests cover preservation and failed-write rollback.
A fresh install resets automatic merging to its explicit flag, including after
uninstall; an in-place upgrade preserves the installed mode.

A stale source is updated from main before a repair attempt so the agent sees
the same source as CI; conflicts and post-repair base drift need inspection.
`tests/test_compatibility_worker.py` simulates these boundaries with local Git
and mocked GitHub/Codex, under the existing Windows/macOS gate jobs. No game
rebuild is needed. See `docs/launcher-compatibility.md` for installation/recovery.

## Text-entry regression route (#136)

`tests/test_menu_text.py` runs in the Windows/macOS runtime workflow. Its asset-free
fixture covers queue bounds, owner changes, character/command routing and embedded
keyboard recognition. An optional local case translates original insertion,
deletion and cursor routines from the checksum-verified US XBE and checks real
editing/length/font-width behavior; CI without that asset skips this case.
`tests/pc_controls.c` covers Windows message delivery and focus changes;
`src/native/menu_input_selftest.inc` adds SDL event/text-session checks. Shared
fixture success does not establish physical Mac typing. Live Windows Create
Profile typing and Enter were confirmed by the user with isolated Test Lab saves.

## Auto display regression checks

`test_pc_display_modes.py` also compiles shared Auto policy through stub SDL
snapshots on Windows and Mac; this tests policy, not Cocoa or actual atomics.
The Windows-specific fixture compiles the model against deterministic
monitor/window adapters: secondary fullscreen desktop size, windowed resize,
minimize/query failure, allocation rollback/retry, persistence and preview
rollback, manual override, and Auto ratio filtering. `tests/pc_display.c` retains
existing manual scene/camera/HUD tests. `test_cutscene_letterbox.py` exercises
the effective-output hook after the complete patch stack is applied. Launcher
settings checks cover Auto UI/filter/save, fresh defaults, existing manual
settings, and older package capability gating. Native Cocoa's graphics self-test
recognizes the Auto choice; physical Retina/multi-display validation needs Mac.

The historical controls-only fixture in `test_patch_updates.py` excludes
`-auto-display.patch` until the complete-stack migration installs its display,
cutscene and fullscreen-effect prerequisites. It then asserts effective-size
hooks in the migrated title and renderer, preservation of local edits, and
idempotent reapplication. Fresh/current-stack tests still apply the Auto patch.

## Crane vignette validation (#221)

`tests/test_crane_overlay.py` checks manual wrapper registration and, when the
checksum-verified local US XBE is available, executes its original vignette
routine using both platform wrapper defines. `crane_overlay` in
`tests/test_ui_layout.py` exercises the production placement helper across nine
shapes, batch boundaries, partial viewports and multiplayer guards without game
assets. Both run in the Windows/macOS runtime workflow. Original-code tests
skip without the proprietary executable; fixture execution is distinct from
a live crane encounter or native Mac graphics validation (MAC-058).

`crane_scissor.c` executes the production legacy GL clip adapter and verifies
full edge coverage plus nonmatching-tag/partial-clip exclusions. The production
RHI extraction in `ui_viewport.c` checks the same captured 639,479 clip at seven
ratios and asserts stored guest state is unchanged. These fixture checks run
on both CI hosts; they are not native Mac gameplay validation.

The historical controls-only upgrade fixture defers `-crane-vignette.patch`
until the complete stack installs the display/viewport prerequisites. The
post-upgrade assertions require both the crane wrapper and renderer clip hook,
alongside preservation of local edits and idempotent reapplication. This does
not omit the patch from fresh/current-stack or renderer regression tests.
