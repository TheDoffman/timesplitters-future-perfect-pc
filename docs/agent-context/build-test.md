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

`tests/test_text_read_scope.py` checks query reuse for ascending and descending
addresses within the same OS region, both mapping boundaries, nested layouts
and expiry between operations. `tests/pc_prompts.c` retains real Windows
protection/guard-page and scratch-write tests in the supported build. A passing
query-count fixture is not an FPS measurement; compare bounded gameplay runs
with captures disabled and identical resolution, FOV and bot settings.
The same Python suite extracts the production outlined-text wrapper and tests
both host feature defines, all five draws, guest ABI, invalidation and fresh
scratch writes. Removing the outer scope is a required failing mutation.

Native `--input-self-test` must reset the synthetic terminal context between
its fractional-time test and mouse-capture/fire test. The stale context caused
a false failure during alpha.2 validation; clearing it within the self-test
passes the actual Mac input, renderer, settings/debug checks and CTests with
isolated configurations. Use a real local build/test root for partition cleanup;
a runtime-root whose build directory redirects elsewhere is rejected.

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

Standalone HLE fixtures need both the patched runtime and repository `src/`
include roots. Framebuffer/ghost mocks cover the current feedback-preservation
and float-combiner host interfaces; unexpected calls fail rather than silently
succeed. Camera fixtures extract through the perspective function's closing
brace, excluding adjacent menu allocation code. Keep the stale-copy, state-leak
and camera mutation cases enabled when updating those harness dependencies.

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
Both game packagers use `write_source_files` in that helper to omit
`assets/textures/` from the nested source-code ZIP while preserving code,
configuration, other assets and the runtime patch. Compiled DDS/JSON are shipped
once through the playable `Textures/` entries. The source README explains how
to reuse them at `assets/textures` for a rebuild. `test_build_textures.py` checks
the nested source ZIP and complete outer texture delivery together; these
asset-free tests run in both host release-validation jobs.

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
for both Windows and Mac assets. Packs declaring retired replacements instead
require stable launcher 1.2.24+ on both hosts, covering transactional icon
retirement; its version regression runs in `test_publish_release.py`.
Local texture-free/artist-override packages are
development outputs; the official publisher rejects them. Regression coverage:
`tests/test_publish_release.py` plus `tests/test_build_textures.py` in the existing
Windows/macOS release-validation CI step; no game assets are used by those tests.

`tools/bundle_online.py:bundle_standard_library` keeps `ssl.py` and top-level
stdlib PEM trust resources together as real files in the Windows portable
runtime. A CPython distribution may load adjacent policy during SSLContext
construction; placing the module in a ZIP while dropping its PEM files broke
alpha.2 packaging. Trust-policy bytes are preserved unchanged. The relocated
SSLContext regression runs in `test_publish_release.py`; the normal real-package
probe still checks Tk, TLS and certificate generation with the bundled Python.

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

## Mounted gun input regression

`python -m pytest tests/test_mounted_mouse.py tests/test_terminal_mouse.py -q`
compiles guarded host-memory C fixtures on Windows/macOS. Mounted tests cover
AGUN ownership/local port, single-consumption relative motion, gun-specific
pitch sign, FOV/inversion/speed, authored clamps/fixed axes/full-circle wrap,
paused/stale/focus rejection and returning to ordinary look. Runtime CI includes
both tests. Physical turret aiming/firing and native host delivery remain
separate gameplay checks; see #182 in STATUS and MAC-001.

## Launcher stalled-network regression

`tests/launcher_updates.cs` holds a fake HTTP response pending until fixture
cleanup. Timeout and Continue Offline checks require the launcher task to finish
with the expected exception, cancel its request, and leave that response pending.
A ten-second deadlock guard bounds failures; the test no longer races a 500 ms
fake response against a 350 ms runner-speed assertion. Both launcher host builds
run this shared fixture; production timeout and cancellation policy is unchanged.

## PR #285 integration checks

Keep both native FPS test registrations and main's mounted mouse, text entry,
Auto display, crane/Ghost Gun and scoped-text checks. The historical controls
upgrade fixture defers both late-frame and native tail patches until full-stack
migration; fresh stacks apply all of them. `stability_transitions_game.py` retains
the weapon-set scenario alongside bounded native-rate/surface/network options
and uses perf-counter timestamps for command/presentation alignment.

The Windows prompt fixture now verifies real page protections and scratch ABI
without trying to count read queries in its own translation unit: scoped reads
live in `pc_prompts.c`. `test_text_read_scope.py` retains explicit query-count,
nesting and fresh-write checks. Merging source does not establish fresh Mac or
whole-game high-refresh acceptance.

The PR #285 CI index check also caught three pre-existing Windows patch files
with residual CRLF bytes despite their LF attributes. They were normalized to
the exact committed Git blobs and the index refreshed. Patch record hashes
identify actual bytes: refresh from canonical attribute-compliant files, keeping
the explicitly binary watermark patch unchanged. Local index success alone
does not prove a fresh checkout has the same patch bytes.

## Exporting updates from a pointer-only art checkout

`tools/texture-repository.py --pointers-only` audits the private pack, stores DDS
objects through Git LFS and writes sanitized metadata plus small pointers. Its
clean command reads stdin without a destination filename: an existing pointer
filename can supply the wrong size and truncate the streamed DDS. Exact pointer
hash/size comparison remains mandatory. `tests/test_build_textures.py` covers
first export and updating an existing pointer with a DDS larger than 1 KiB.
Use `tests/test_texture_release.py` for format/hash/privacy and package limits.
No game launch is required for this export-only change.

## Original character icon policy

`config/texture-originals.json` excludes user-retained original icons from
`texture-worklist.py` and `texture-hybrid-plan.py`. Offline regression coverage
is in `tests/texture_pack_tools.py` (including rejection of accidental reinstall)
and `tests/test_texture_hybrid_plan.py`. These tests are already registered in CI.
Asset removal also requires pack count/size audit and unchanged-file hashes;
no game rebuild is needed.

Retired-icon upgrade coverage also runs through `tests/launcher_textures.cs`
in Windows and native Mac launcher checks. `tests/test_build_textures.py` verifies
that the exact retirement identities enter both package manifests and rejects
reintroduced original-only keys. Use `tools/build-launcher.ps1 -Test` for Windows;
CI provides native Mac launcher compilation and shared fixture execution.

## Live FPS handoff regression

`test_frame_clock.py` extracts the actual native observer and Arm bodies into
`flip_gate_deadline.c`. Five within-vblank native phases cover a first return
to Original 60; following cheap frames must still wait. The no-op observer
mutation must fail the first-return assertion. Existing cheap/late/no-signal
cases remain. This suite already runs on Windows and macOS runtime CI.
`tests/fixtures/live-fps-pacing-commands.json` enters Video Options with one Down
from Continue after the pause-menu reorder. Confirm actual NATIVE-TIMING rate
changes and saved preference, not replay labels alone: stale navigation can
finish successfully while selecting Controls and never changing FPS.

Historical upgrade fixtures exclude the new handoff patch along with the later
native timing tail when constructing old stacks, then apply it via the complete
current loader. This preserves the historical dependency graph rather than
inserting the new HLE call into a runtime that predates native timing.

The handoff follow-up also updates `test_repo_context.py` to check surface-live
before native-flip-handoff and the new tail. Its old final-patch assertion
blocked CI scope selection even though the generated index was current.

## Walking and boost timing audit

`tests/native_locomotion.c` is lifted through the existing hash-verified US XBE
route in `test_native_timing.py`. It executes the complete walking routine,
with controlled input displacement and flat-floor world services. Tests cover
60/120/144 displacement, diagonal cap, stops/reversals, boost resource, selected
player/mode/button eligibility, depletion, slow/fast time and catch-up. A
runtime mutation restores the old subtraction and must fail resource parity.
`native_timing_guest.c` also checks the actual clock-to-resource calculation
without game assets in existing Windows/macOS CI. The XBE-dependent fixture
is skipped when assets are absent; do not count that skip as gameplay coverage.

`test_original_fractional_dispatch[native_escort_recovery.c]` executes the
complete original match/mode/recovery dispatch chain from the supported XBE,
including original switch tables. It tests recovery delay/rate, pause,
slow/fast-time, catch-up, selected actor and existing clamps. Its no-fix mutation
must double recovery at 120 FPS. The asset-free `native_timing_guest.c` recovery
contract remains in the existing Windows/macOS CI route; neither fixture proves
live Escort input activation or network transport behavior.
