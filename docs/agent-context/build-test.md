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

`tests/test_network_guest_dispatch.py` includes `network_bot_motion.c`: a
20 Hz snapshot stream must produce continuous 60 Hz display transforms, while
jitter, loss, angle wrapping, death/absence/respawn and authority remain bounded.
Its zero-duration mutation must fail the cadence assertion; its mixed-crossfade
mutation must fail the atomic clip-transition assertion. Frame sampling also
covers native 120/144 Hz without changing the simulation clock. Existing Windows
and macOS CI invoke this suite; the helper itself has no OS dependencies.
`tools/test-online-bot-motion.py --tag <name> --seconds 60` runs a Windows-only,
bounded production Online host/guest pair through a loopback directory/browser/
bridge, disposable Test Lab saves and separate input/runtime/display settings.
It checks the supported XBE and sealed build before launch, records executable
and compatibility identities, and recycles only its disposable partition images.
Its asynchronous ReadProcessMemory observations measure actor update holds,
not rendered frames, monitor scanout or Internet latency. Window captures are
limited to the test processes. Benchmark fixture junctions are staged locally
as five manifest files, then checked by the normal fixture validator.

Launcher changelog publication: tests/test_launcher_release.py covers explicit
previous stable launcher selection across paginated game/launcher history,
initial-release notes and refusal of incomplete history. It remains registered
in the existing Windows/Mac launcher build workflow. These are offline publisher
tests; no game rebuild or launcher UI rebuild is needed for this notes-only fix.
Additional launcher publication fixtures verify game/docs/housekeeping exclusion,
mixed PR inclusion, merge range, file pagination, renamed paths and incomplete
file-list refusal in the same existing dual-host test route.

Game changelog baselines: tests/test_publish_release.py covers first-Stable
bootstrap from the latest earlier published alpha, subsequent Stable baselines
across minor versions, mixed launcher/draft history and ancestor restrictions.
The suite is already registered for both hosts in the release checks; these
offline publisher fixtures need no game or launcher rebuild.

### Native vehicle integration and diagnostic checks

`tests/test_native_vehicle_timing.py` runs portable byte-load, physics-scope,
partition and lowering contracts on both CI hosts. With the verified private US
XBE it also lifts the original dispatcher, spring/linear/rest routines, rigid
clock/solver/integrator, final publisher, bounds, sleep/wake, camera and driver.
Private-XBE skips on CI do not certify those routines on Mac.

- `native_rigid_motion.c` checks forces, drag, quaternion/position publication,
  controlled support contacts, RNG isolation and secondary-step boundaries at
  60/120/144, slow/fast/irregular time and pause. Static contact cases compare an
  independent impulse equation, including penetration escape velocity. Mutations
  independently remove gates, timers, contacts, RNG and boundary corrections.
  Geometry, room/model notifications and broadphase services remain controlled.
- `native_vehicle_driver.c` checks original `17DA00/172EE0` throttle, brake,
  reverse/release and steering recurrence, including wheel eligibility, speed
  clamps and ABI. `legacy-driver` and `legacy-steering` must fail independently.
- `native_vehicle_camera.c` executes full `CA040` plus original spring/history
  helpers. Check fresh updates, aligned camera checkpoints, pause and ABI;
  gate/count mutations must fail before entering a zero-count decrement loop.
- `native_spawn_motion.c` executes original joint-history and spawn routines
  through real velocity/momentum setters. Check all carried-clock phases,
  signed/rotated inheritance, missing history/allocation and pause. The legacy
  mutation fails at native120; controlled services do not certify live models.
- `native_vehicle_actor_impact.c` connects original velocity publication to
  queued actor contact, impact and suspension helpers. Old division/gate-only
  mutations fail; geometry/effect callbacks are controlled.
- `native_contact_velocity.c` checks rotating-body/static/character contacts,
  axes, time scales and nullable output. Both legacy gate mutations must fail.
- `tests/test_native_timing.py` retains both physics-history handoff and boost
  resource cases alongside the coupled-surface fixture. Historical upgrade
  fixtures exclude the later vehicle/texture/panel patches where their earlier
  dependencies are absent; the current full stack must include all of them.

For live driving, use `--native-fps 0` for true Original60 and verify `native=0`
in a solver trace. `--native-fps 60` means native60: older `vehicle-*-60` labels
are invalid original-clock evidence. Default Y delivers logical exit bit0x40;
F/R deliver0x20/0x10. Verify camera24->0, weapon restoration, on-foot movement,
pause/resume and unchanged saves. In Siberia choose LEFT from Recommended to
None, verify roster257, and allow six seconds for vehicle entry before steering.
Camera/body observations need frame brackets; asynchronous captures alone are
not synchronized trajectories. Preserved live pillar/bank counterexamples and
corrected Windows acceptance records are in STATUS and
`docs/native-high-refresh-regressions.md`; neither proves broad handling parity.

Contact tracing requires isolated `TSFP_CONTACT_TRACE` path, pool slot0..1023
and finite inclusive `TSFP_CONTACT_TRACE_Z=min,max`. It records generated and
selected lists separately, including empty snapshots, up to16,000 rows/8MiB.
Discard capped partial snapshots. The allocated arena is observed after selection;
its normals can already be merged. `native_contact_trace.c` tests identity,
filters, invalid/unreadable values and limits; physics scope covers nesting and
Original/native paths. `--trace-vehicle-contact` separately enables bounded
original entry traces, incompatible with `--watch-input`; entries do not prove
impulse magnitude. Inherited diagnostic environment is stripped by the runner.

Pacing observations use complete capture-free Present intervals in
spawn+5..+55 seconds with clock neighbors and scene/pause checkpoints. Record
actual window visibility, foreground/client size and bot roster. Successful
presents are not GPU completion or scanout. External stack sampling reports
suspension cost and remains diagnostic; health restoration/keypress timing does
not identify hitch cause. An empty optional-pack control must not remove an
installed pack or edit personal settings. Performance evidence and exclusions
remain in STATUS; mixed bot outcomes prevent attributing gains to one fix.

Texture timing records travel through the bounded queue. The worker stamps its
own record; CSV initialization/completion stay on the renderer thread. Test stage
order with streaming on/off. Total duration includes queue/frame waits; read spans
are worker work, image/view spans renderer work. Cancelled requests neither
dereference their owner nor log on the worker.

Texture-upload trace tests compile the real loader and collector through the
existing both-host `tests/test_texture_runtime.py` registration. They verify
disabled clock/file inactivity, failed output, bounded output, ordered stages,
cached switches and missing/invalid/view-failure fallback. Use an isolated CSV
path and compare raw monotonic timestamps with Present intervals for live
diagnostics. Windows QPC and POSIX CLOCK_MONOTONIC match each host's presentation
clock domain; durations cover CPU work/waits, not GPU completion. Trace logging
can change frame timing. Actual native Mac gameplay remains a separate check.
Do not attribute a whole slow frame to a sampled upload: intersect the actual
attempt begin/end timestamps with each Present interval. The first live trace
accounts for only 2.215 ms of a 12.639-ms interval and has other slow intervals
without attempts. Hashing/cached-call costs and trace output are outside these
attempt spans; broader frame work remains open. STATUS retains the evidence.
The follow-up collector starts before source-key hashing and emits `cached`
rows as well. Loader tests require nonzero ordered hash boundaries and zero
read/resource stages on all twenty cached calls while retaining existing load
and fallback counts. The 8,192-row bound still applies; verify coverage before
using a live window. The earlier `2230901` data retains its narrower scope.

`stability_transitions_game.py --scenario crowd-venice --native-fps 144
--display-size 3440 1440 --fov 73 --no-native-trace --profile-presents
--no-captures --seconds 180` replays the tracked unlimited-score ten-bot route.
Use `--native-fps 0` for Original60. It requires active Venice checkpoints and
ten configured bots; it does not prove every bot is simultaneously visible.
`crowd-hotel` (scene39) and `crowd-disco` (scene56) use frequent move/fire/respawn
input. Crowd routes focus their window after startup; still verify focus below.
Record roster/window state separately from device resolution; unfocused/minimized
runs are not foreground acceptance. `--watch-actors` reads bounded actor positions/
health with pool rechecks; asynchronous proximity is not visibility. Analyze
complete spawn+5..+55s intervals (Disco +5..+50s, within the 20,000-row cap) with
clock neighbors and scene/pause/game clock evidence. Disable images/tracing. `--profile-cpu`
enables 200Hz sampling with bounded menu/spawn/combat snapshots. Concurrent rewrites
can yield partial snapshots: check before differencing. `--trace-textures` enables
bounded upload-stage timing. Both perturb pacing; neither is a clean benchmark.
`test_menu_text.py` covers scan-local VM reuse/expiry, invalid rows and unchanged
original text editing; eager-query/stale-cache mutations must fail. Both hosts'
existing runtime CI includes it. STATUS retains measured performance and limits.
`tests/test_trace_filter.py` compiles the actual runtime name-filter function,
checks zero label scans for absent/empty filters, and retains exact matching,
independent hit limits and zero/negative caps. Its eager-scan mutation must fail.
It is registered in both-host native runtime CI. For changes to this function,
apply the complete patch stack first; passing scan-count checks establish the
removed work, not a gameplay FPS improvement.
`tests/test_gpu_flush_ack.py` compiles the actual patched notifier and lifted
store. It checks post-write notification, exact instruction/register selection,
untouched guest words and lifecycle guards. Omitted/reordered notifications and
a direct-clear mutation must fail. Lifter cases include absolute/indexed/immediate
stores and excluded loads, narrow/segmented stores and register moves. The actual
worker wait is exercised with both host adapters, missing-event/timer-failure
fallback, the 50-ms diagnostic timeout ceiling and the original one-millisecond
busy window. Native `native_platform.c`
also tests real event timeout, prequeued/consumed signals, delayed thread wake and
manual-reset behavior through the microsecond adapter. The registered Python
suite compiles/runs this fixture with the actual compatibility layer on Mac; Windows adapter mocks do not establish Mac execution. Apply all patches,
regenerate/build, then test the previously failing match-loading route before
measuring performance.

`tests/test_text_read_scope.py` checks query reuse for ascending and descending
addresses within the same OS region, both mapping boundaries, nested layouts
and expiry between operations. `tests/pc_prompts.c` retains real Windows
protection/guard-page and scratch-write tests in the supported build. A passing
query-count fixture is not an FPS measurement; compare bounded gameplay runs
with captures disabled and identical resolution, FOV and bot settings.
The same Python suite extracts the production outlined-text wrapper and tests
both host feature defines, all five draws, guest ABI, invalidation and fresh
scratch writes. Removing the outer scope is a required failing mutation.
The panel variant extracts `pc_panel_text_original` and `sub_0012E3F0` too.
It models the panel's three measurement/fit calls and two outlined labels,
reducing same-region read queries from five to one while retaining all three
measurements, ten draws and thirteen fresh scratch-write checks. The mutation
removes only the panel scope; outlined scopes remain. Placement callbacks
assert they run outside the reused mapping lifetime. This is a boundary/ABI
fixture, not execution of the full original panel or a rendered comparison.
The UI placement-hook contract names `pc_panel_text_original` for `0x12E3F0`;
the original panel now runs inside that callback. Other placement targets keep
their direct original callbacks. The scope suite verifies callback execution.
The first native144 Venice panel comparison is recorded in STATUS: average FPS
and tail intervals move in different directions, so query savings alone must
not be reported as a pacing improvement. Exclude a measurement window when
another game starts during it, even if that run reports a better average.

`test_decal_projection.py` checks ABI/all matrix values, zero weapon/preview
queries and one same-region camera-source query, including split stack output.
Retained-source-snapshot mutation must fail. Local `projection_output.c` lifts
checksum-verified US151CF0, checks 16 writes/math/bounds/stack over 48 lenses;
missing-store mutation fails. Private-XBE cases skip CI. `display_memory.c`
tests real host read-only/no-access/boundary/overflow protection (also Win guard).
Registered projection/cutscene suites cover both hosts; query savings alone
do not establish FPS improvement.

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
`native_mounted_mouse.c` checks 60/120/144 with 125/1000/8000 Hz packets,
jitter, quarter speed, fresh/integrated displacement, pause/invalid clocks,
handoff and untouched player angles. Old-clock/batch mutations must fail.
These controlled-field tests do not certify live turret rendering/input.
Consult STATUS for current build hashes, not historical candidate paths.
`mouse_region.c` checks actual Windows/SDL mouse bodies with controlled maps:
one query for aiming, read-only FOV maps, expiry, terminal routing and bounds.
Persistent/uncached mutations fail in mounted pytest/CI on both hosts; mocked
locks/events are not physical-host validation. `auto_look_region.c` similarly
checks the pre-guest auto-look scope: one same-map query, split/read-only maps,
expiry and bit restoration. Its mutations run in registered `test_auto_look.py`.
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

## Texture streaming regression checks

The Windows native-runtime CI job installs requirements-release.txt before its
Online HTTPS tests, using the same pinned certificate provider as the packaged
companion. Without it, the suite fell back to the runner's Git OpenSSL, which
timed out during certificate creation on 9bf5630. This changes test dependency
setup, not the production hosting fallback. Changes to those pins trigger the
workflow and select native checks; Mac's selected suite does not run that test.

`tests/test_texture_runtime.py` compiles the actual shared queue, common RHI
loader and legacy GL loader with tiny real DDS files and counted GPU adapters.
It runs on both CI hosts. A deliberately stalled reader checks nonblocking
pump/cancellation, owner reuse, queue backpressure, staging limits and cleanup.
Both modes check Classic deferral without reads/allocations, content changes,
late Enhanced activation, cached switching and failures; GL also checks mips
and binding restoration. Deferred traces omit I/O; late requests omit hashing.
These are CPU/adapter tests, not GPU/scene acceptance. Run patch upgrades for
Present hook changes; measure pacing/pop-in separately at Original60/120/144.

The pytest loader fixture defines TSFP_TEST_BINDING_SYNC and extracts the actual
patched binding helper into its temporary include directory. The CMake loader
fixture runs without that extra helper; it requires no pytest-generated files.
Both routes default to background loading when no diagnostic override is set.

Historical controls-only and pre-toggle snapshots omit streaming along with its
texture prerequisites. Full-stack migration must install both the Present pump
and binding-revision hook, which the controls-upgrade test explicitly checks.

The texture fixture now compiles actual present_resolve, dev_Swap and dev_Present
bodies, verifies exactly one queue completion per presentation (none for a skip),
and finishes real DDS loads through Swap. It fails against the Present-only
prototype. Replay mode deliberately suppresses host shortcuts in pc_controls_message;
neither guest F10 injection nor posting PC_TEXTURE_MODE_COMMAND validates live
switching during replay. Use the in-game settings route or a non-replay session.
The host toggle logs transitions, but the menu's explicit tsfp_texture_set_4k
setter does not. For menu validation inspect staged/applied captures, Apply's
disabled state, and the actual value after closing/reopening the page (which
reads tsfp_texture_is_4k). Capture resumed gameplay in both modes separately from
frame-time measurements. The ineffective window command was removed.

## Game release channels and maintenance publication (2026-10-22)

`tools/release_channels.py` owns canonical X.Y.Z[-channel.N] package identity and
the minimum launcher 1.3.0. `tools/release-version.py` writes an explicitly chosen
VERSION only; no automatic increment/tag/publication. The next Stable source
version is 0.3.2. Both game packagers validate identity before staging and emit
release_channel in BUILD.json. Official packages continue optional texture delivery.

`publish-release.py` defaults to exact origin/main; --branch permits pushed feature
previews and Stable release/ or hotfix/ branches with a published Stable ancestor.
It validates monotonic versions within stable major/minor lines or exact preview
base/channel series. Notes choose an ancestor in that series, falling back to
Stable; unrelated/nonancestor experiments never become a baseline. Channel JSON
records exact source and included/latest published Stable; a missing latest
Stable is disclosed. --notes-file supplies reviewed feature notes for unmerged
branches. Prepared package reuse includes the channel metadata; publication
retains latest=false and existing package/installer/hash/rollback checks.

The release CI scope selects release_channels.py and release-version.py and
existing test_publish_release.py covers their identity, preparation, branch,
hotfix, unrelated-baseline and prepared-retry contracts using fake GitHub.
LauncherSharedUiChecks covers shared channel selection/preferences/metadata;
launcher_versions.cs covers paginated discovery. Run the existing Windows launcher
suite and both-host CI. See STATUS.md for measured local results; no runtime game
behavior or generated guest code changed, and no game build/publication is
established by these asset-free checks.


Grouped Preview UI: GameChannelPickerPage.cs is compiled automatically by both
hosts' existing *Page.cs includes. launcher_shared_ui.cs tests the actual shared
selector, including remembering a feature while returning through Stable, general
preview selection and unavailable-track refresh. Rebuild with build-launcher.ps1
-Test, then run build/launcher/TimeSplitters Launcher.exe. --render-preview produces
a synthetic named-track example without modifying preferences or calling GitHub.
No game build or publisher/tag-format change is required for this grouping.


Local Windows artwork selection: build-launcher.ps1 resolves explicit image
arguments first, then validates each build/launcher-art PNG against the shared
artwork pin, then uses the existing private legacy fallback if no cached file
exists. fetch-launcher-art.ps1 recovers the pinned banner/backdrop/logo without
executing downloaded code. -RequireArtwork checks the selected resources; normal
local rebuilds now reuse the same verified art as release CI when cached.


Beta publication (2026-10-23): release_channels.identity defaults to history parsing;
publishing=True rejects alpha/preview/rc/legacy suffixes. Version preparation,
both package entry points and publisher preflight enforce that mode. Beta's
canonical identity is beta/Beta; a custom Beta display name is rejected. Historical
identities remain readable for baselines without joining Beta's series. Publisher
fixtures now use beta.N, with explicit legacy rejection/history and independent
Beta baseline tests. Existing generic semantic-version ordering tests still cover
alpha/beta ordering separately from the new publication policy.

## Coupled water operator checks

`tests/stability_transitions_game.py --scenario surface-feedback --run-name
water-live-feedback --seconds 100 --no-captures` replays the committed Scotland
route and requests 16 completed GPU feedback triplets, with isolated Test Lab
saves/settings and executable/XBE provenance. Add `--native-fps 144` for the
native clock while retaining the original water feedback. `--no-captures` skips
ordinary screenshots; this scenario still explicitly reads its feedback fields.
`test_captured_live_feedback` in `tests/test_surface_shader_feedback.py` compares
that local capture with the original translated shaders and coupled one-tick,
two-half-tick and three-piece updates on local hardware. Capture Original60 in
`water-live-feedback-original` with `--native-fps 0` for the second case. Hardware
byte captures must not be used as WARP goldens: UNORM conversion differs between
those adapters, and the WARP suite uses its own original shader reference. Missing local capture
is a skip, never proof. `native_surface_capture.c` is in the dual-host CPU timing
group and verifies copied source order/identities and rejection without partial
publication. Actual Mac RHI readback and game execution remain separate checks.

`native_surface_coupled.c` runs on both CI hosts without assets: 65 controlled
modes, split composition, 60/120/144 trajectories, invalid-input nonmutation,
and positive/repeated/complex roots against independent original equations.
`test_original_surface_shader_feedback` compares fixed-neighbor and 64-square
spatial GPU cases with the unmodified game's float32 output. Separate controls
retain byte-storage differences. Shared GPU forward/inverse transforms are checked
against CPU asymmetric-corner, checkerboard and noise references. Removing the
velocity correction or breaking the inverse transpose must fail after compilation.
These are operator checks, not runtime/performance or Mac GPU acceptance.

`surface_vertex_definition.c` uses the production parser/emitter on private
microcode: the supported XBE's six-instruction vertex program and both original
pixel programs run with synthetic geometry and callback-2BF10 c17-c20 offsets.
Translation helpers use isolated configuration. Single-pass and gradient-only
byte controls isolate rounding; no controlled scalar/storage test alone establishes
a replacement for the original map. The pinned private XBE and Windows WARP are
required; missing-asset skips are not validation. Detailed history is in the
high-refresh regression guide.

The surface spatial fixture also runs at 128x128, matching the observed game
feedback target. Its wrapped case mirrors the live quad, viewport, screen-space
undo and linear wrapped sampling. A seam-adjacent impulse distinguishes the
periodic real Hartley basis from the clamped DCT control; forcing a clamped basis
must fail the original-shader comparison. `native_surface_basis.c` checks shared
basis orthonormality and diagonal-neighbor eigenvalues at sizes 1/7/64/128 under
both boundary rules, plus invalid input, through existing Windows/Mac timing CI.
Live draw evidence and its binary/source provenance are recorded in the high
refresh regression guide. The fractional candidate comparison still excludes
smoothing. `test_original_smoothed_surface_feedback` separately executes the
captured cardinal smoothing and both original feedback passes at 60 Hz, checking
the filter against explicit wrapped neighbors. A zero-offset mutation must fail.
This reference includes actual byte/half/float storage and mixed-format controls;
it excludes disturbances, final shading and game RNG. Do not infer gameplay
equivalence from the controlled float reference.

`surface_final_gpu.inc` executes original effect 22 (callback 2C320, definition
4BC318) on the captured cardinal offsets. It checks final RGB against explicit
height differences, through float and actual byte render targets. The candidate
uses shared `surface_state_shader.h` to materialize alpha-height after each fresh
inverse transform. A constant-alpha header mutation must fail the final shader
comparison. Final shader agreement with the float32 reference does not establish
original byte-storage appearance or scene rendering; both differences are reported.

`test_original_byte_rounding_flow` adds an independent 60-Hz original-shader
reference starting from an actual byte target. `surface_rounding_gpu.inc` holds
the first-pass clipping/quantization residual over each authored interval, executes GPU
fractional transforms/steps at every frame, and writes original byte state only
at interval boundaries. The frame schedule splits steps that cross a boundary.
It compares every boundary's G/B and final byte ripple RGB, with and without
original smoothing, at 60/120/144 and a mixed 144/120/60/144 sequence. The test
clock now uses the production `pc_native_timing_step` with a controlled
72-MHz counter and `pc_surface_interval_split`; the mixed sequence switches at
nonzero phase and produces 117 advancing frames over 60 ticks. A half-second
pause inserted during motion must produce no slices and identical final output.
This is not live host scheduling or resource integration. Mild and strong signed initial height pulses
plus near-limit velocity/height fields are covered. Missing forcing, missing
ambiguity handling, omission of gradient clipping, unlimited acceleration and
missing authored boundary limits are required failing mutations. Saturated
60-Hz final-color repeats must match the independently measured original count;
the higher-rate and mixed schedules still require fresh final colors every frame.
Direct guard cases reject a distant replacement and canonicalize nearby values
even away from ideal byte thresholds. This is controlled trajectory validation,
not runtime integration or measured game performance. The portable coupled CPU
test also checks held forcing across split steps and rejected-output preservation.

Rounding profile 3 (`surface_noise_gpu.inc`) runs original effect-19 vertex/pixel
programs on captured A8L8-to-RGBA source with linear wrap and ONE/ONE RGB-only
blending before smoothing. `surface_vertex_definition.c` checks the four vertex
instructions/input mask 0x010e. Both noise cases explicitly skip without the
private Original60 Scotland frame-3600 capture; no game bytes are tracked.
Controlled UVs replace RNG. Initial draws verify additive green and preserved
red/blue/alpha; duplicate candidate noise must fail boundary equality. Queued
splashes, live submission and whole-scene rendering remain separate acceptance.

`native_surface_interval.c` (dual-host timing suite) checks exact conservation
and boundary counts across three counter frequencies, presets, jitter, pause,
catch-up, reset, invalid nonmutation and the four-piece maximum. Reset-phase
mutation must fail. The real guest fixture checks publication, paused zero time
and Original-mode clearing; exact metadata avoids float-sum boundary loss.

`native_surface_mode.c`, in the same dual-host suite, checks immutable mode
packing against the existing affine operator and independent one-tick equations,
including real/repeated/complex roots, irregular split composition and rejected
output preservation. The GPU rounding fixture now uses the shared evolve shader
with two immutable mode textures and a 16-byte time uniform; it no longer creates
coefficient textures keyed by fractional duration. A fifth schedule alternates
144-Hz intervals by +/-0.1 ms while retaining one second of active time. Every
profile includes that jitter, smoothing off/on and pause preservation. Replacing
the evolve shader's first diagonal entry with 1 must fail the byte comparison.

Windows D3D11 hardware diagnostics: `python tests/test_surface_shader_feedback.py
--hardware-check --profile 0` checks all authored boundaries/freshness;
`--benchmark --profile 3` measures timestamp spans without per-frame readback,
checking each sequence's final byte image. Normal pytest uses WARP. Hardware
paths skip WARP-specific storage tolerances, print adapter identity and bind the
four captured wrap samplers before original draws. `surface_timing_gpu.h` reads
disjoint/timestamp queries after submission with a ten-second wait bound;
spans include submission bubbles, exclude setup/readback and are not game FPS.
RTX4070 SUPER exposed a byte conversion transition away from the ideal half-byte
threshold. Keep the shared arithmetic-distance guard; actual target conversion
owns thresholds. Unconditional replacement must fail the direct guard test.
Hardware profiles/schedules and endpoint checks passed on that adapter only;
see STATUS for measurements. They do not establish other-GPU or game acceptance.

The rounding fixture uses the shared GPU owner. `surface_owner_gpu.inc` checks
input rejection, reset, packed alpha-height, sentinel target preservation,
color/depth and multiple/empty viewport restoration, plus binding cleanup.
Removing reset/output restoration fails. `native_surface_gpu_owner.c` injects
failure at every allocation; `native_surface_gpu_cache.c` checks retained image
identity, transactional replacement, capacity and clear/recreation. These run
in the dual-host CPU suite; Windows shader checks do not prove Vulkan behavior.
`native_surface_native.c` checks phase, boundaries, pause/gaps and default/override
policy. `native_surface_render_guest.c` tests the actual default-enabled guest
adapters at 144 updates/60 events, ABI and Original60 recovery.
`native_surface_runtime.c` injects allocation failure into the real HLE adapter:
no repeated setup until scene/device reset, no bindings changed by empty reset.
All three run in dual-host native timing CI; this is not Mac GPU evidence.
`--scenario surface-native` uses Scotland. Native water is the common-RHI beta
default; `--original-surfaces` forces baseline water, `--native-surfaces` forces
on. `--native-fps 0` selects true Original60. `--trace-surface-images` reads final
GPU images for freshness, not pacing. `surface-native-long` adds a 90-second
stationary window then fire/pause/resume; use `--profile-presents --no-captures
--seconds 230` for host Present timings. Provenance records the explicit/default
policy. Historical patch fixtures exclude both tail surface patches.

Rounding trajectories now use `pc_surface_gpu_begin_bytes`; original-shader and
broken-gradient controls still test residual/clipping behavior. Captured replay
keeps independent unpack/residual setup. The Windows runner's
`--scenario surface-feedback-gpu --run-name water-runtime-gpu --native-fps 144
--seconds 100 --no-captures` invokes the shared solver in the deferred renderer.
Its `feedback.bin.gpu` stores 16 TSWGPU1 records: 92-byte identity plus two
128-square float4 fields. The third captured-feedback test checks matching
metadata, finite values and both states within 32 float epsilons; original byte
endpoints remain exact. A missing capture skips; an existing runtime capture
without its sidecar fails. Per-sample creation/readback precludes speed claims.

`--scenario surface-native-switch --native-fps 144 --native-surfaces
--trace-surface-images --trace-surface-switches --seconds 230` runs the committed
`scotland_switch` route through Video Options (before Keyboard and Mouse), then
requires 90 images each at 144/120/Original60/144 using the strict cycle validator.
The last segment may retain its initial image until its first authored boundary.
`native_surface_render_guest.c` also checks resuming between ticks, local time
mismatch and constructor cleanup; cache tests distinguish suspend/reuse without
allocations from full scene destruction. Image capture is not switch-latency proof.
