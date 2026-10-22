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

The rigid-motion fixture additionally checks 576 overlap/rebound cases using
the original contact-term/solver/integrator functions: signed/capped depths,
restitution, unit and quantized normals, centered/eccentric points, static kinds
0/2 and regular/irregular timing. Expected impulse includes the original escape
velocity (12 * clamped overlap), with authored drag in the linear bound; energy
must not grow for the tested closing contacts. `missing-penetration` removes only
the escape-velocity term and must fail. This fixture controls geometry, uses
closing speed12/inverse mass0.5/unit inverse inertia, and requires the private XBE.
It is not a live corner replay. STATUS records the subsequent successful144
walking detour and the different front/side contact normals versus Original60.

Live vehicle reference correction: `--native-fps 60` means native60, not
Original60. Existing `vehicle-*-60` camera, steering and terrain recordings use
that setting, confirmed by their startup logs and solver flag. Their old
Original60 labels are invalid; do not reuse them as original-clock acceptance.
The bounded runner now accepts `--native-fps 0` for explicit Original60;
verify `native=0` in a live solver trace. Omitting the option also uses the
isolated default. Values 60/120/144 explicitly select the native clock.
Offline original-routine fixtures explicitly disable the native policy and are
not affected. See the correction at the top of STATUS.

The solver-entry contact diagnostic requires `TSFP_CONTACT_TRACE` (CSV path),
`TSFP_CONTACT_TRACE_SLOT` (rigid-body pool slot, 0..1023), and
`TSFP_CONTACT_TRACE_Z` (finite inclusive `min,max` world-Z window). Set these
only in an isolated test child's environment after the runner strips inherited
debug variables. It records a summary even for zero matching contacts, then
generated and selected records separately. It stops at 16,000 rows or 8 MiB;
the last snapshot may be partial when capped, so discard it for comparisons
unless completeness is independently established. The allocated arena is read
after selection has run; merged normals can already differ from producer output.
The registered asset-free `native_contact_trace.c` fixture checks filter/identity,
unreadable data, finite values, list membership, zero-contact snapshots, disabled
and invalid configuration, opening failure, and both output limits. The physics
scope fixture checks observation in Original, native and nested solver calls.
Live contact evidence still requires a build containing this optional hook.
Build `20dc25d` and the three accepted solver runs (original60/native60/native120)
are now recorded in STATUS. Check the CSV's `native` flag rather than inferring
the clock from a filename or FPS counter. The failed144 walking approach never
reaches the jeep and is excluded. Do not interpret these captures as pacing tests.

`native_rigid_motion.c` also covers 864 identical zero-depth static-contact
cases against an independent frictionless inelastic impulse equation: signed
axes, centered/eccentric points, inverse masses, closing/separating/rest speeds
and regular/irregular native intervals. Its linear error bound accounts for
original `16CA00` drag `0.001*v` over one authored tick; angular response and kinetic
energy have separate checks. `missing-wall` drops only these contact solves
and must fail the analytical response. Broadphase/contact selection and contact
offsets remain fixture-controlled. This private-XBE test cannot close the live
pillar discrepancy; source inspection is not geometry acceptance.

The registered vehicle suite now lifts full `CA040` with original spring,
history-average and angle helpers. Controlled unobstructed collision queries
and CRT math isolate time consumption. It compares aligned two-second camera
checkpoints on both horizontal axes through reversal, slow/normal/fast and
regular/irregular native intervals; it checks fresh updates, pause and ABI.
Gate/count mutations fail independently; check a missing substep count before
entering the original decrement loop to avoid an underflow hang. Three
asset-free real-lowering cases cover both clock sites and an unrelated one.
For live camera comparisons, read camera and body positions in one bounded
observation with frame brackets and verify active camera ownership/mode/FOV.
A similarly timed renderer capture alone is not a synchronized pose trace.
Integrated `fb261e1` passes the supported Windows build and selected both-host
CI. The `vehicle-integrated-camfix-{60,120,144}` bounded Windows runs use that
same executable with isolated Test Lab profiles. Their matched-position
camera distances remove the prior large lag; steering and settled pause pass.
See STATUS for values and provenance. Keep one-run asynchronous/captured
behavior evidence separate from exact trajectories, physical input and pacing.
The normal playable pair is restored after each run; candidate objects remain
in the root build cache, so preserve the pair and force a fresh link next build.
The later `vehicle-integrated-terrain-*` route removes pre-drive steering and
extends throttle toward the ice pillar. At built source `fb261e1`, 60/144 stop
but120 reaches the bank; a second60 also stops with a different pose. Its
asynchronous position/quaternion/velocity readings show approach differences,
so neither an isolated collision cause nor exact handling parity is proven.
Preserve this counterexample and use controlled contact evidence before editing
mechanics. STATUS records checkpoints and rejected broad conclusions.
Main `acf4464` integration retains keyboard-menu/weapon-slot exclusions in
the historical controls fixture alongside vehicle physics. Full-stack apply
and all four current/historical upgrade checks pass on the combined source.

When integrating the vehicle branch with main `7545d4b`, preserve both
`physics_history_transitions` and `boost_resource_clock` in the guest clock
fixture. The three historical patch fixtures exclude vehicle physics alongside
surface coefficients/full-float and flip handoff; the complete stack includes
all of them. Run combined timing/input and historical upgrades before treating
earlier separate-branch validation as applicable to the merged candidate.
Integrated `d0116a7` passes the supported Windows build, 147 combined checks,
four historical/current upgrades and 88 context/backlog/scope checks (draft
PR #307). The retained candidate is `vehicle-d0116a7`; older `vehicle-464f850`
and `vehicle-60e5b36` executable/symbol pairs have been superseded and recycled,
while diagnostic evidence remains. Build provenance and live exclusions are
in STATUS. In particular, the earlier `60-empty` route retained six bots:
verify both roster and captures before treating a route as an empty-map test.
The first integrated144 observer produced no data, and the second run lost
the player before jeep entry; neither establishes steering parity.
For the corrected Siberia route, choose LEFT from Recommended to None,
verify roster257 (zero bots), and approach the observed jeep using normal
controls rather than the old six-bot spawn path. Allow six seconds after
entry; an earlier right-input probe was consumed during the climb-in animation.
The three `vehicle-integrated-settled-*` runs pass signed-input and settled
pause checks with unchanged profiles. Keep their asynchronous measurements
separate from exact response and pacing acceptance. A 180-frame dump cadence
has different wall-time age across presets; camera-distance comparisons need
synchronized capture/position observations before diagnosing a regression.

`native_spawn_motion.c`, registered in `test_native_vehicle_timing.py`, lifts
original joint-history copy `57040`, object spawn `1727A0` and its real
velocity/momentum setters and matrix helpers. Allocation, owner transform,
Euler extraction, bounds/wake and the CRT floor service are controlled.
Independent spawn events test all carried-clock phases at 60/120/144,
signed inheritance, rotated transforms, vertex offsets, pause, missing
history and allocation failure. `legacy-spawn-motion` must fail at native120.
This private-XBE fixture does not certify live model rendering or trajectories.
Supported Windows build `3e1ee87` includes this fix and `6232685` contact
velocity; the full build and built-in checks pass. The retained diagnostic
pair was superseded by `private/build-recovery/vehicle-60e5b36`. Normal playable `4adb6b0`
is restored, so force a fresh link when building from the candidate object
cache again. Live spawn/contact and native Mac checks are still pending.
`native_vehicle_actor_impact.c` in the same Python suite links original
`A7360` velocity publication to `17CAE0`, both directly and through original
registration/queue delivery `A5CE0/16C070/16C0D0`. Original `166060/1665F0`,
`17BC90`, and `174EC0` with its mass/matrix helpers verify contact speed,
selected impulses and suspension component changes. Movement/geometry and
effects are controlled. Old divide and gate-only counterfactuals fail at120;
all preset clock phases and mismatched producer/event intervals are covered.
The complete Windows build and built-in checks pass at `60e5b36`, as do all
115 combined timing/vehicle/numeric-join/generation checks. Live contact
acceptance and Mac execution remain separate requirements.
For a bounded live probe, `stability_transitions_game.py --trace-vehicle-contact`
explicitly enables the three original contact/impulse entry traces (64 each)
after environment isolation. Inherited `RECOMP_*` flags are intentionally
stripped by `prepare_run`; they cannot enable these traces. The option cannot
be combined with `--watch-input`. Entry traces identify callers and registers,
not exact impulse arguments or matched full handling.
The `vehicle-actor-trace-60e5b36-144` live probe reaches `17CAE0` from the
player collision caller and passes pause/resume. Its entry-only trace does
not establish the callback's taken branch or impact magnitude. That candidate
EXE/PDB was superseded by `vehicle-d0116a7`; its evidence is retained.

`tests/test_native_vehicle_timing.py` runs the shared byte-load timer contract
without assets on both CI hosts. With the supported local US XBE it also lifts
the complete vehicle dispatcher, spring helper, linear adjustment, rest routine
and rigid-body clock. `native_rigid_motion.c` additionally runs the original
solver, body integrator, quaternion/matrix helpers and support-contact resolver
(`171FB0 -> 171560 -> 16CA00`, plus `1667C0/16FC00/168720`). It checks actual
position/velocity/orientation with applied force, drag, a controlled upward
contact, and pause at 60/120/144. The full original final publisher, bounds
transform, sleep/wake and vehicle-component updater now execute too. Tests
check entity position/velocity, cleared force accumulators, component
acceleration/clamping, sleep duration and immediate fractional wake motion.
Broadphase geometry, material/allocation, contact damage, room membership and
model notifications remain controlled services.
These are routine checks, not live driving or geometry acceptance. Negative
controls independently remove the gate, linear-rate, rest-timer or rigid-clock
correction; further controls omit contact resolution, RNG isolation or the
secondary-step boundary correction or body sleep cadence and must fail their
relevant cases. Synthetic memory-DEC checks cover all byte values, stored
result flags and preserved carry at the sleep site and an unrelated site.
The same suite checks the production manual scanner retains the `171FB0`
original body; a comma-separated `_gen` extern passes C compilation but the
scanner misses it and the final link fails. The motion fixture now also executes the original RNG with three controlled
contacts and checks slow, fast and irregular native intervals. Asset-free
`native_physics_scope.c` checks nested/private/shared draws and cleanup; the
same Python test file checks 20,000 secondary-step partitions. Those portable
cases run on CI even when the private original-routine cases skip.
`tests/native_timing_guest.c` checks physics-history handoff through the actual
clock wrapper, including paused return and slow motion. Candidate vehicle and
multi-contact/vehicle integration still require broader acceptance before replacing
the existing playable build. The supported Windows build at `ba4f0bf` passes,
but matched live Siberia throttle replays show lower travel at native rates.
Use the recorded input sequences, observations and captures linked in the
high-refresh regression record to investigate before accepting handling.
The added `native_vehicle_driver.c` fixture runs original `17DA00/172EE0` and
the XBE's seat switch table. It reproduces half the intended integrated
throttle at native120 before the `17DA03` gate correction; `legacy-driver`
must still fail. It checks normal/slow/fast/irregular intervals, fresh analog
throttle, braking, reverse, release, pause and the guest ABI. Wake, model and
seat-action services are controlled; full handling requires the live replay.
The steering extension executes those same complete original routines with
left/right/released targets, three vehicle-speed inputs including the target
clamp, quarter/normal/triple time, 60/120/144 and irregular intervals. It checks
the original-60 recurrence, fresh steering outputs, wheel eligibility, pause
and unchanged whole/catch-up arithmetic. `legacy-steering` independently removes
only the new coefficient correction and must fail at native120. The asset-free
synthetic SSE coefficient test in the same Python suite runs on both CI hosts;
it checks real lowering, untouched upper lanes, Original-mode arithmetic and
an unrelated instruction site. Full wheel/terrain forces remain outside it.
Steering source `464f850` passes the supported Windows build and built-in
checks. Its candidate pair was superseded by `vehicle-d0116a7`; the normal
`4adb6b0` pair is restored. The object/generated cache is newer than the normal
executable, so preserve that pair and force a fresh link before the next build.
The `67c1864` replay corrects the original throttle deficit (initial travel
within 0.1% across presets); later pillar/bank paths still diverge. Inspect both
the sampled input window and subsequent captures before declaring acceptance.
`native_contact_velocity.c` now lifts original `1667C0/166060` to check rotating
body point velocity, body/static/character contacts, all movement axes and
directions, clock speeds, irregular intervals, nullable output, pause and ABI.
The motion fixture's moving support additionally executes `16E780/16DA90`
and the original contact-kind table through a controlled body callback. With
zero material coefficients, damage/effects stay disabled. It verifies fresh
reported normal/vector velocity while body motion stays consistent. Negative
`legacy-contact-velocity` and `legacy-character-contact` controls independently
restore the missed gate and fail at native120. These private-XBE cases skip on
asset-free CI; CI registration is not evidence of their execution on Mac.
`tests/test_text_read_scope.py` checks query reuse for ascending and descending
addresses within the same OS region, both mapping boundaries, nested layouts
and expiry between operations. `tests/pc_prompts.c` retains real Windows
protection/guard-page and scratch-write tests in the supported build. A passing
query-count fixture is not an FPS measurement; compare bounded gameplay runs
with captures disabled and identical resolution, FOV and bot settings.
The same Python suite extracts the production outlined-text wrapper and tests
both host feature defines, all five draws, guest ABI, invalidation and fresh
scratch writes. Removing the outer scope is a required failing mutation.

`tests/test_decal_projection.py` checks the production projection wrapper's
guest ABI and all matrix elements, plus one VM query per same-region correction.
Its split camera/stack case requires two queries and covers the actual decal
callers' temporary stack output ordering.
A retained-snapshot mutation must fail, proving each invocation queries afresh.
`tests/display_memory.c` uses real Windows/macOS VM protection for read-only,
no-access, boundary-crossing and overflow rejection (Windows also guard pages).
The existing registered projection/cutscene CI suites run these checks on both
hosts. The cutscene fixture shares a controlled query adapter for geometry;
neither query counts nor successful geometry tests establish FPS improvement.

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
route. The optional-pack policy no longer hydrates LFS for version-only changes. A version bump alone is not evidence of a published update.

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

The 2026-10-22 optional texture policy supersedes default bundling from `c932bee`.
Windows and native Mac builders no longer require asset hydration when repository
`config/texture-pack.json` declares optional delivery. Both game packagers omit
textures by default; explicit `--with-textures` / `--texture-pack` still audit and
stream an offline bundle. `write_source_files` omits compiled DDS/JSON from the
nested source ZIP. Test these defaults without downloading real artwork.

`testing_profile.prepare_run` explicitly supplies `TSFP_TEXTURE_PACK_DIR` from
the checkout's `assets/textures` and `TSFP_TEXTURE_MODE=classic` after filtering
inherited diagnostics. Without that pack path F10 toggled modes but loaded no
replacements. Both Windows Test Lab and the native Mac unlocked runner consume
this shared environment; no runtime adapter change is needed. Asset-free
coverage lives in `tests/test_testing_profile.py` and
`tests/test_native_mac_tools.py` (both already selected by native CI). Windows
runtime loading must be checked separately; native Mac visual validation remains
pending. Missing packs retain the loader's Classic fallback for source-only tests.

Official game publishing reads the repository delivery policy. Optional delivery
requires no `Textures/` members and does not hydrate LFS. `textures` is null
or carries only `delivery: optional` and the exact-hash retired-icon policy.
`optional_identity` reads that small policy without touching DDS files. Packages
with retirements retain the published launcher 1.2.24+ requirement. Legacy bundled-policy fixtures still enforce complete asset identity
and launcher compatibility. `tools/texture-release.py` separately audits and
publishes immutable schema-3 parts plus per-file hashes; the public index is
`config/texture-download.json`, distinct from the repository catalog. The initial
unpublished placeholder is replaced only after successful publication. Asset CI
hydrates LFS for pack/auditor changes, not game or launcher version bumps.

Regression routes: `test_texture_release.py`, `test_build_textures.py`,
`test_publish_release.py`, `package_launcher.py`, `test_native_mac_package.py` and
both launcher hosts' `--textures` / shared UI checks. Indexed-update tests cover
adopting bundled files without receipts, changed/missing parts, cancellation,
checksum/index rejection and custom data preservation. Existing installer tests
cover rollback, invalid paths and corruption. Real Mac execution remains a host
check; a Windows compile or portable packaging test is not Mac acceptance.

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
`native_mounted_mouse.c` in that same registered suite checks 60/120/144
updates with 125/1000/8000 Hz packet streams, regular/jittered intervals and
quarter-speed game time. It asserts integrated raw displacement and fresh
aim on every update carrying motion, plus paused/invalid clocks, no replay,
mode handoff and untouched player angles. Separate old-clock and old-batch
counterfactuals must fail. It executes shared host input logic with controlled
guest fields; it does not establish live turret rendering or physical input.
Windows source `a5e0dc2` passes the supported build and built-in checks, plus
159 broader regressions. Its candidate binary is retained separately; the
normal playable executable is restored. Consult STATUS for hashes before
testing and force a fresh link after preserving the playable pair on rebuild.
`tests/pc_controls.c` and the SDL `--input-self-test` now exercise mounted
mode through the actual adapters, changing the native rate without recreating
input capture. The focused Windows `tsfp_pc_controls_test` target passes with
the isolated fixture config and scratch directory. SDL physical-host execution
is still pending. The portable packet fixture explicitly includes `<stddef.h>`
for `ptrdiff_t`; Mac Clang does not supply it through `<stdlib.h>` as MSVC did.

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

Launcher v1.2.24 version preparation uses `tests/test_launcher_release.py` and
the Mac backlog integrity check; the dual-host launcher workflow validates the
actual packages. Require the Repository texture pack check for this version bump
and publish both launchers before the icon-removal game update.

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

The third `test_surface_shader_feedback.py` case, `surface_damping_gpu.inc`,
executes one second of original-program WARP displacement damping at 60/120/144.
Float32 uniform inputs and zero coupling isolate coefficient packing. The
packed fractional coefficients are a required negative control; a precise
power of the original packed one-tick damping must retain the 60-step result.
This needs the local supported XBE; hosted skips are not GPU acceptance. It
proves a scalar arithmetic contract, not full spatial water/byte-storage parity.

The damping case first executes all of `sub_003D9520` through the current lifter
and passes its packed bytes to the GPU fixture. Do not substitute hand-rounded
constants: the current recompiled conversion truncates. `surface_coefficient_upload.c`
checks the stack/callee-saved-register boundary and supplies sufficient command
space; no generated game code is checked in. `test_ordered_combiner_constants`
also covers the production coefficient setter/clear/cache path. The GPU scalar
case now uses the shared coefficient helper for its corrected values.

The coefficient integration was exercised with the existing bounded Scotland
route at 60/120/144 and `--trace-surface-images`. Use the image validator for
consecutive-frame/hash/freshness checks; compare common elapsed game-time
windows rather than treating 360 samples as equal time at different rates.
Unpaired RNG and synchronous GPU readback prevent mechanics/pacing claims.
Current fresh images and reviewed stills are not complete wave acceptance;
the measured amplitude difference remains recorded in STATUS.

`surface_coupling_gpu.inc` exercises the unchanged second feedback shader with
persistent G/B and a fixed neutral neighbor at 60/120/144, separately storing
outputs in float32, half-float and UNORM8 targets. Identical controlled inputs
and per-pixel equations separate timestep coupling from actual GPU storage
rounding; it deliberately excludes smoothing, random forcing and spatial waves.
The earlier scalar damping case alone cannot prove coupled equivalence.
Historical upgrade fixtures exclude the later coefficient patch together with
its surface-constant dependency before installing the complete current stack;
including only the newer patch creates an impossible historical checkpoint.

`surface_spatial_gpu.inc` executes both original feedback pixel programs over a
64x64 Gaussian disturbance with explicit diagonal offsets traced from 0x2BF10.
Every intermediate/final pixel is checked against the controlled stencil using
actual float32, half-float or UNORM8 storage. It omits the original vertex
program, smoothing and disturbances; it is not full gameplay equivalence.
`rhi_surface_preservation.h` now checks byte/half and byte/full-float conversion,
channel/swizzle order, exact capture reconstruction, output state and rollback
prerequisites on the existing Windows and native Mac GPU test route. Old-stack
fixtures exclude the new full-float patch until its prerequisites are installed.

Optional texture publication changes (`tools/texture-release.py` and its test)
select the release suite even when changed alone. Manual Windows upgrades carry
Textures alongside Data, Settings and Saves to retain downloaded/custom packs.

## Issue-form refresh regression checks

`test_issue_forms.py` checks that an empty or category-incomplete release result
cannot partially erase known references, including check-only mode and first-use
empty repositories. `test_repo_context.py` exercises the ownership gate with real
Git diffs: generated version data is exempt, structural edits and empty-list
regressions are not. Existing issue-form/runtime-plan CI runs these asset-free
Python cases; there is no game or native launcher behavior change.

## Weapon-slot selection checks

`tests/test_weapon_slots.py` compiles the shared key-state and production guest
selector fixture on Windows/macOS. It covers press edges, held/focus/menu input,
expiry, empty slots, grouped weapon variants and local-player/state guards.
The supported Windows build also runs `tests/pc_settings.c` for migration and
last-slot rebinding/persistence. Launcher settings tests cover defaults, custom
conflicts, explicit Unbound, moving keys and reset. Original input-hook changes
require regenerating the guest C through the supported build. These fixtures
are not a substitute for actual weapon-switch animation/gameplay validation.

## Themed keyboard editor checks

The historical old-controls upgrade fixture excludes keyboard-settings-menu and
keyboard-weapon-slots until the shared-input locks/display wrappers exist. The
final full-stack upgrade must contain the capture pump, two-binding limit and
weapon-slot sampling/guest hook, so both new patches are still validated.

`tests/test_keyboard_menu.py` compiles the portable capture/page model: opening
input release, one event per press, cancel/focus/timeout, complete action
coverage and the sensitivity range. Run alongside `test_menu_mouse.py`,
`test_weapon_slots.py` and `test_quit_menu.py`. Supported game Build checks
exercise native settings persistence and two-slot bounds; launcher settings
tests cover importing legacy four-binding profiles, rejecting a third slot,
and saving two slots. Live acceptance must separately check main/pause access,
binding capture without shortcut leakage, conflict/clear/defaults, mouse
sensitivity, two-column layout and return to gameplay.

## Storage selection checks

Run test_storage_menu.py for actual Windows volume queries, bounded labels,
free/total formatting, atomic selection, routing rollback, unavailable paths, preserved
profile files and test-session locks. test_fov_settings.py exercises inline drive
rows, alphabetical drive-letter/volume-name ordering independent of the active drive, no child menu, failure/success feedback, console
index isolation, profile-list refresh and original save dispatch. Its Mac-only compile contracts
cover the production menu and prompts/storage translation units. The Windows and
Mac bounded runners set TSFP_SAVE_LOCATION_LOCK=1; use temporary test folders for
persistence checks rather than selecting a real save drive in automated play.
For a user-requested manual drive-selection session, the Windows runner accepts
`--play --allow-save-drive-changes`. This explicit option disables that launch's
lock and respects save-location.txt beside its isolated controls file. Selecting
a drive activates it immediately and persists the choice; future manual launches
use the same preference. Preparation preserves this preference and never copies
the Test Lab fixture onto the selected external drive. Ordinary automated sessions
stay locked regardless of inherited environment settings.
Mac runtime/UI validation remains separate from shared fixtures and Windows builds.

Unlocked Test Lab fixtures use their game-created directory ID 1AD19156651F,
not the original Player 1 seed folder. prepare_run remaps legacy fixtures and
backs up only untouched seeded copies outside UserData. Modified legacy profiles
require a fresh isolated run. This prevents ERROR_ALREADY_EXISTS when creating
Player 1 without deleting a profile or altering personal saves.
