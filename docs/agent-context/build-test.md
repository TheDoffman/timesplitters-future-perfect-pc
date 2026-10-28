# Build, run and validation map

Launcher UI/JSON recovery use shared UI, native parser and package checks.

Grouped paths: [layout](../development/repository-layout.md).
Python/CI imports use `tools/project_paths.py`; fixtures use `tests/conftest.py`.
Portable probes retain isolated imports.

Patch discovery/migration: [folder rules](../../patches/README.md).

RHI and Mac crash/IPC fixtures use grouped includes; replay `--help` checks imports.

[Guest services, headers, compiler and execution contracts](independent.md).
Recorder/state/ABI mutations must compile under Clang `-Werror`; see linked contracts.

Setup: [snapshot](runtime.md). [Wind](native-wind.md).

Sampler, trace/thread/idle and state-slot tests: [runtime](runtime.md).

Shader upload tests: [RHI fixture](vsh-constants.md).

Opening replay: [intro versus gameplay](native-opening-checks.md).

Local aim/animation CPU observations: [completed-update trace](native-local-trace.md).

Vertex conversion: [tiled full-function parity, guards and CPU timing](vertex-expand.md).
Windows Reload fixture: [clock-domain expiry check](vertex-expand.md#controls-fixture-clock).

Windows diagnostics and `test_sample_pc_locations.py`: see [runtime](runtime.md).
Broken variants must compile before their runtime checks reject them.

State-cache binding/failure and WARP/Vulkan pixel fixtures:
[runtime](runtime.md). Vulkan defines `TSFP_RHI_RENDERER`.

`test_gpr_refs.py` covers four-site selection, integer/vector live-cell references, callbacks, aliasing,
mixed scopes and threads; see [contract](gpr-refs.md). The dependent generator
patch follows model-entry policy; old fixture stacks omit both until upgrade.
Only generated unused labels are exempted from this fixture's fatal warnings.

`native_walk_trace.c` tests the actual diagnostic bridge; `native_walk_policy.c`
checks scopes/guards and `native_walk_contact.c` executes original C54A0 with
recorded geometry-service boundaries and independent missing-hook controls.
They check acceptance/ABI rather than complete real geometry. See the
[contact trace schema](native-player-contact.md) before interpreting live CSVs.

<!-- packet:start -->
Windows: `powershell -NoProfile -File tsfp.ps1 -Action Build` applies patches,
checks fingerprints, regenerates, builds and tests MSVC x64. Output:
`build/windows/Release/futureperfect_recomp.exe`. Never edit generated C.
Mac: `tools/build/build-native-mac.py` (`--generate` for stale code, `--setup` for setup).
Output: `build/macos/futureperfect_recomp`. Auto selects Vulkan on macOS26+,
otherwise GL. SDK pin: `config/build/vulkan-sdk.json`; driver provenance:
`tools/build/vulkan_sdk.py` and `BUILD.json`.
Isolated gameplay: `tools/testing/testing_profile.py --play --seconds 120` (Windows),
`tools/testing/test-native-mac.py --unlocked-profile --seconds 120` (Mac), with .venv.
Root AGENTS owns assets/isolation/storage. Build, startup and gameplay are
different evidence; unavailable hardware/private data remain validation gaps.
For movement input changes, sampler and guest-only fixtures do not prove
gameplay speed. Use an isolated Story run and compare actual player-position
displacement for straight and diagonal travel. Mouse capture checks should
also cover focus loss, stale held buttons, menu return and a clip released by
another component while the game remains foreground.
`test_window_theme.py` runs `pc_mouse_capture.c` with Win32 API doubles for
center bounds, repeated repair, negative origins, resize and foreground/errors.
`tsfp_pc_prompts_test` checks OS confinement/hit-testing and Fire release across
repair; it explicitly skips without foreground focus. `pc_debug_game.py` uses
center bounds too. `test_mounted_mouse.py` covers split-screen mouse
adapters; live gameplay remains unverified.
<!-- packet:end -->

SDL tests accept Confirm E/Space and reject the third fixture slot Return
(shared two-slot cap). Run `tools/build/build-native-mac.py --test` on Mac; this
synthetic check neither changes bindings nor certifies physical keyboard input.

CI: `runtime-patches.yml` selects work; `ci-pr-gate.py` requires both hosts.
Native/patch-upgrade jobs allow 30 minutes. Required check names remain.
`test_ci_pr_gate.py` rejects missing/failed/skipped results;
`test_ci_runtime_scope.py` checks wiring. Issue jobs select their sources.

`test_push_gather.py`: full original-reference gathering, fresh bytes, failures,
guard pages, large buffers and32-bit wrapping. Both native CI hosts run it.
See [pushed-array checks and timing scope](vertex-expand.md#pushed-array-gathering).

`test_zero_base_indices.py`: old/new HLE parity, fresh indices, failures, stream
dispatch and copy/rebase/leak mutations. Both hosts; mocked draws.

Reload/Fire #337: `test_native_timing.py` covers the US decoder;
`reload_phase_sweep.h` covers 864 schedules/1,728 presses. `test_reload_keys.py`
covers 1,360 Reload and 680 Fire schedules at 60ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã…â€œ240, priority, remaps and
lifecycle. `native_reload_input.c` adds 425 original-decoder Fire schedules
(X surrogate). Windows `pc_controls.c` checks the event bridge.
`native_refresh_game.py --native --fps 180 --short-fire --seconds 116` checks
seven 1 ms clicks by ammo use.
Both CI hosts run these; missing XBE skips originals. Physical keys/Mac gameplay
remain separate checks.

`test_model_transform.py` compares memory/GPR/SSE in four rounding modes and
callback mutations. Fast results and finite fallback results are bit-exact;
fallback arithmetic destinations allow quiet-NaN payload variation (also seen
with fast entry disabled). Rejected inputs leave all bits/FP flags unchanged.
Public fixtures/mutations run on both hosts; US-body tests need the private XBE.
Camera/mothership/spawn/rigid fixtures include the shared helper.

Crash capture/heartbeat and bounded Windows sharing retries: `test_crash_capture.py`.
Darwin: `test_native_crash.py`. Both hosts run shared UI identity/failure/ZIP tests.


`stability_transitions_game.py --scenario rocket-impact --native-fps 240
--watch-actors --watch-effects --no-captures --no-native-trace --seconds 180`: Temple floor rocket, cleanup,
respawn/walk;0 selects Original60. Other games block launch. Focus precedes input.
Checkpoints/effect samples require visible foreground windows; missing/background/
minimized windows fail. Reads are non-atomic, at most20Hz; unreadable players are
excluded/count-reported. CI runs counterexamples. Lethality is not damage parity;
captures are asynchronous. Back settles before setup navigation. See STATUS.

The Windows runner's optional `--watch-native-clock` brackets read-only timing-
call counter7497B8 and level ticks with perf_counter_ns/QPC timestamps plus scene/
match identity checks. This marker increments in the timing routine before
gameplay returns; it is independent of accepted-Present CSV counters and is not
completed simulation or GPU work. Compare successive bounded observations with
Present clocks, allowing one in-flight call. Exclude resets, transitions, pause
and wide read spans. Capture-free runs retain lightweight snapshots; full native
traces and renderer captures belong in separate diagnostic runs.144 remains a
legacy diagnostic rate; the menu presets are60/120/180/240.

Script clock fixtures cover original dispatch/timers/events/reset, RNG,
pose/voice queues, pause and old-gate controls through240; see the script-clock guide.

Original TREE motion coverage: [script-clock fixtures](native-script-clocks.md).

Original TRAP motion coverage: [script-clock fixtures](native-script-clocks.md).

`native_trap_transform.c` connects the original initializer/callback, A7360
post-update invalidation, A14B0 Euler cache/matrix helpers and151E80 point
transform in80 rate/slow/phase/Original-native cases. It checks changed-angle
invalidation, cached reuse, fresh off-axis points, independent sine/cosine
coordinates, point fast/original bit parity, pause, clock restoration and ABI. Registration/collision/sector
services are spies. Broken dirty-store, angle input, matrix copy or point
transform independently fails. This is fixture-driven CPU propagation; normal
frame scheduling, actual mesh/GPU rendering and gameplay activation remain open.

`native_player_statistics.c` runs original D1FD0, clock getter, eligibility and
float accumulator through the production D2CB7 callee scope.160 rate/slow/phase/
hour schedules cover60..240, movement-distance inputs, pause, uneven intervals,
eligibility and exact whole/catch-up fallback. Ten-minute native240 time totals
must be bit-equal to Original60; per-frame float addition fails this precision
check. Independent missing-scope and missing-pause-guard controls fail. The public
clock fixture covers dispatcher/restoration on both CI hosts. Private US XBE
cases skip without the asset; they do not establish physical Mac gameplay.
Copied numeric-policy fixtures must resolve all relative helper includes,
including `pc_native_statistics_guest.h`, before mutation compilation.

## Choose the cheapest relevant check

Launcher publisher tests require full checkout history for release notes.

`test_pushbuffer_policy.py` compiles the full scanner: disabled/scan/execute/both,
presence semantics, decoding/reporting, and repeated-lookup/executor mutations.

`test_window_theme.py`: menu drawing, mnemonics, inserted/reordered rows (#333).

`test_audio_output_gain.py` / `test_audio_timing.py`: XAudio/SDL, fake devices,
gain overrides, mute, live volume and voice parity.

`test_controller_profile.py`: devices/maps/disconnect and SDL virtual input;
physical pads/Mac gameplay remain unverified.

`test_native_demo_timing.py`: guards, mutations, ABI, pause/EOF/cancel, catch-up;
eight recording-phase and32native/Original60/resume cases through240. Missing
XBE skips originals; live/file/Mac playback and sub60 remain open.

`test_native_movie_file.py`: two XMV intros, Original60/native60..240 including144;
exact decoded pictures/timestamps/PCM and duration. Controlled
and Windows clocks; private-data skips, mocked GPU/audio output. Surface-trace
fixtures use240/180/Original60/120, matching the strict live-cycle validator.

`test_online_theme.py` isolates Tk lifecycles. `test_framebuffer_sampling.py`
checks refresh/stale copies and first-entry/any-framebuffer alias lookup against
the original searches, including wrong-first/last mutations (Mac GL/RHI, Windows RHI).
`test_trace_filter.py` compiles real trace dispatch: quiet calls, explicit
diagnostics, budget, idle callback and bypass-removal mutation.

180/240 coverage includes settings, clocks, input, animation, weapons, networking,
movies, demos and legacy144 migration, not sustained FPS. `test_native_timing.py`
covers original-US movement/aim/physics/projectiles, effects/menus/cameras/lights,
network/recovery/HUD, weapon sounds and countdowns through240. Script events cover
every cadence phase at normal/quarter speed, retaining the legacy missed-event
control. Keep references/tolerances and verify rate coverage when extending.
Mouse cadence/surface math cover240; math is not live water. Mounted/terminal
tests check permissions, bounded queries, 125..8000Hz input and clock/cache/consume
mutations. Private-XBE skips do not establish Mac execution of lifted routines.

`test_sample_chains.py`: depth/saturation/counts and mutation controls on both
hosts; Mac does not execute Windows stack walking.
Runner timing includes16ms test-input polling; normal play does not. Its
`replay_tick` file opens are harness overhead.

`tests/network/test_network_guest_dispatch.py` checks 20Hz snapshots rendered at
60/120/144/180/240Hz, mixed 60..240Hz intervals, jitter/loss, wrapping, death/respawn, authority and atomic clip
transitions, including cadence/crossfade mutations. Both CI hosts run it.
Windows `test-online-bot-motion.py`: isolated 0/120/180/240 peers.
`--require-results`: matching records; `--short-match`: five points via menu readback.
Results transitions: `--post-results-seconds`/`--inspect-results-tabs`/`--rematch`.
`--require-rematch-results` compares round-two results after reset checks.
`test_network_bot_results.py`: US routines and AI mutations.
Async reads do not measure latency.

Dual-host `test_launcher_release.py` / `test_publish_release.py` cover
Stable/alpha baselines, pagination/completeness, notes and scope/merge/rename/
ancestry. Offline publisher tests need no game/UI rebuild.

`test_auto_look.py`: real records/permission expiry. `test_decal_projection.py`:
four-page batches, larger fallback, permissions and mutation controls.
`test_discord_presence.py`: production read bounds/copies, fresh permissions,
read-only/noaccess, large/cold fallback and retained/full-query mutations.

Character fixtures run original dispatcher/timers and status3 scale/rotation/
composition/cleanup through240, slow/pause/jitter/live switches and old-path
controls. Scale tests use real matrix helpers; allocation/draws are absent.
`native_character_trace.c` tests copied status3 update pairs, identity/frame/
scene guards, removal, invalid/unreadable values and8192row/8MiB bounds. The
original scale fixture also runs with tracing enabled and a failed file open;
stdout/behavior stay identical and open failure is latched. The clock fixture
matches production's manual TLS include order so missing register declarations
fail locally. Paired matrices surround126029->120AB0, after timer advancement;
read-cache lifetimes end before/after guest calls. Captured actual scoped deltas
support per-update growth checks; async samples and an ideal constant60 curve
are insufficient. It is an opt-in diagnostic, not a GPU/pacing acceptance route.
`native_material_motion.c` compares180 fractional partitions with an independent
one-step forced/damped affine map, including weak/zero drag, slow/fast time,
mixed pieces and untouched rejection. `native_surface_cache.c` executes original
127500 with nonzero held forcing/drag at60/120/144/180/240 and mixed rates,
normal/quarter speed and pause, checking U/V velocity/travel against a closed
form authored recurrence. Independent mutations remove damping, force or travel
normalization; GPU calls remain spies. Force-table sampling is controlled, so
these checks do not certify live varying forcing or rendered-material parity.

### Native vehicle integration and diagnostic checks

`tests/gameplay/test_native_vehicle_timing.py` runs portable byte-load, physics-scope,
partition and lowering contracts on both CI hosts. With the verified private US
XBE it also lifts the original dispatcher, spring/linear/rest routines, rigid
clock/solver/integrator, publisher, bounds, sleep/wake, camera and driver at
60/120/144/180/240.
Private-XBE skips on CI do not certify those routines on Mac.

- `native_rigid_motion.c` checks forces, drag, quaternion/position publication,
  controlled support contacts, RNG isolation and secondary-step boundaries at
  all five rates, slow/fast/irregular time and pause. Static contacts compare an
  independent impulse equation, including penetration escape velocity. Mutations
  independently remove gates, timers, contacts, RNG and boundary corrections.
  The rigid/rigid extension independently checks 432 frictionless impulse cases:
  both masses, existing spin, three-dimensional normals and lever arms, diagonal
  and rotated world inverse-inertia tensors, approaching/separating bodies,
  Original60/native120/144/180/240 and an extra 0.7ms secondary step. It verifies
  relative point velocity and both force/torque impulses before integration,
  including linear and world-origin angular impulse conservation. Independent
  mutations remove the second mass or its torque response. Original166060 is
  lifted for both-body point velocity; original16DF60 damage/effect notification
  is a checked service boundary. This is distinct from full clock scheduling,
  live terrain selection and post-impact handling acceptance.
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
- `tests/gameplay/test_native_timing.py` retains both physics-history handoff and boost
  resource cases alongside the coupled-surface fixture. Historical upgrade
  fixtures exclude the later vehicle/texture/panel patches where their earlier
  dependencies are absent; the current full stack must include all of them.

For live driving, use `--native-fps 0` for true Original60 and verify `native=0`
in a solver trace. `--native-fps 60` means native60: older `vehicle-*-60` labels
are invalid original-clock evidence. Default Y delivers logical exit bit0x40;
F/R deliver0x20/0x10. Verify camera24->0, weapon restoration, on-foot movement,
pause/resume and unchanged saves. In Siberia choose LEFT from Recommended to
None, verify roster257, and allow six seconds for vehicle entry before steering.
`testing_profile.publish_input_state` atomically replaces input beside its source,
retrying Windows reader sharing for at most0.5s.
Never truncate a live input-state file: the adapter polls no sooner than16ms and treats
an empty read as key release. Writer timestamps mark publication, not guest consumption. `test_testing_profile.py` checks publication, sharing retries, failure preservation
and temporary cleanup on both hosts.

Bracket camera/body reads by frame and player-camera identity; globals can differ
midframe. Retain excluded samples. STATUS/regression records preserve
counterexamples; async reads do not prove completed-frame trajectories or
broad handling parity. Current-build Siberia contact acceptance (STATUS,
2026-10-25) also finds large post-pillar variation between two true Original60
runs with the same sampled entry pose. Treat that route as nondeterministic;
matching host key durations and initial pose do not fix decoded input boundaries,
all physics state, contact selection or captured-frame stalls. Native180/240
exit/walk/pause pass, but endpoint differences alone cannot identify a native
collision defect. Use controlled off-center geometry/coherent solver state for
the next comparison rather than tuning to one live endpoint.

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

`surface-native-long` restores focus before `water-stable-start`, then settles.
Its pacing observations must retain foreground visibility throughout the chosen
window. Reject a background run even when its recorder has complete clocks;
do not shorten the window or substitute a background FPS claim. Scotland
standing-water results do not replace crowded-combat or campaign acceptance.

Texture traces use a bounded queue: workers stamp reads; the renderer owns CSV
and image/view stages. Total spans include waits. Cancelled requests neither
dereference owners nor log on workers. Both-host `test_texture_runtime.py` tests
streaming stage order, disabled clocks/files, output failure/bounds, cached
switches and load/view fallbacks. Current rows include key hashing and cached
calls; cached rows must have zero read/resource stages. Verify the 8,192-row
coverage before analysis. Earlier `2230901` traces excluded hashing/cache work.
Use isolated CSVs and intersect attempt timestamps with Present intervals;
QPC/CLOCK_MONOTONIC share the host presentation clock domain. These spans measure
CPU work/waits, not GPU completion, and tracing perturbs timing. STATUS records
partial slow-frame attribution; native Mac gameplay remains separate acceptance.

`stability_transitions_game.py --scenario crowd-venice --native-fps 144
--display-size 3440 1440 --fov 73 --no-native-trace --profile-presents
--no-captures --seconds 180` replays the tracked unlimited-score ten-bot route.
Use `--native-fps 0` for Original60. It requires active Venice checkpoints and
ten configured bots; it does not prove every bot is simultaneously visible.
`crowd-hotel` (scene39), `crowd-temple` (scene49) and `crowd-disco` (scene56)
use frequent move/fire/respawn input. `--crowd-weapons rockets` selects the normal
Rockets preset; use spawn+5..35s to fit the recorder cap. Routes focus at startup.
`--fullscreen` starts borderless via isolated settings and records the request.
Check actual client size/foreground state. `--watch-actors` reads position/health
with pool rechecks; proximity is not visibility.
Use complete spawn+5..55s windows (Disco/Temple end50s), clock neighbors and
scene/pause evidence. `--profile-delay-seconds 0..600` delays CSV rows, retaining
the 20,000-row cap; record it in provenance and verify endpoint coverage.
The present-metrics fixture checks parsing, delayed intervals and cap/disable behavior.
Disable images/tracing for pacing. `--profile-cpu` enables 200Hz diagnostic
snapshots; concurrent rewrites may be partial. `--trace-textures` records bounded
upload-stage timing. Check completeness before differencing either diagnostic.
`test_menu_text.py` covers scan-local VM reuse/expiry, invalid rows and unchanged
original text editing; eager-query/stale-cache mutations must fail. Both hosts'
existing runtime CI includes it. STATUS retains measured performance and limits.
`tests/diagnostics/test_trace_filter.py` compiles the actual runtime name-filter function,
checks zero label scans for absent/empty filters, and retains exact matching,
independent hit limits and zero/negative caps. Its eager-scan mutation must fail.
It is registered in both-host native runtime CI. For changes to this function,
apply the complete patch stack first; passing scan-count checks establish the
removed work, not a gameplay FPS improvement.
`tests/graphics/test_gpu_flush_ack.py` compiles the actual patched notifier and lifted
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

`test_text_read_scope.py` checks ascending/descending reuse, boundaries, minimum
spans, nesting and expiry. `guest_span.c` covers real page/cross-page reads;
`pc_prompts.c` checks crossing strings/argument frames, guard/protection changes
and scratch-write ABI in Windows Build. Compare capture-free gameplay with
matched resolution/FOV/bots; query counts alone do not measure FPS.
The suite extracts outlined-text wrappers for both host defines, checking five
draws, ABI, invalidation and fresh scratch writes; removing the scope must fail.
Panel coverage extracts `pc_panel_text_original`/`sub_0012E3F0`: three fit calls,
two outlined labels, one read query, ten draws and thirteen scratch-write checks.
Its mutation removes only the panel scope, retaining outlined scopes. Placement callbacks
assert they run outside the reused mapping lifetime. This is a boundary/ABI
fixture, not execution of the full original panel or a rendered comparison.
`0x12E3F0` uses `pc_panel_text_original`; others keep original callbacks.
`test_ui_layout.py` checks RANK/SCORE left anchoring, batch restoration and
unchanged split-screen/4:3. The scope suite checks callback lifetime.
The first native144 Venice panel comparison is recorded in STATUS: average FPS
and tail intervals move in different directions, so query savings alone must
not be reported as a pacing improvement. Exclude a measurement window when
another game starts during it, even if that run reports a better average.

`test_decal_projection.py` checks ABI/all matrix values, zero weapon/preview
queries and one same-region camera-source query, including split stack output.
Retained-source-snapshot mutation must fail. Local `projection_output.c` lifts
checksum-verified US151CF0, checks 16 writes/math/bounds/stack over 48 lenses;
missing-store mutation fails. Private-XBE cases skip CI. `display_memory.c`
tests real host protection/bounds, multi-page spans and the production display
adapter; retaining its snapshot across calls must fail. Win guard is included.
`guest_span.c` checks private/mapped pages, permission changes, cold/cross-page
fallback and released mappings. Windows additionally checks failed page queries
and disabled-fast-path/write-permission/page-boundary negative controls.
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
| Agent context/index | `python tools/repo-context.py check`; `python -m pytest tests/repository/test_repo_context.py -q` | Changed-path maintenance check against base |
| Patch loader/order | `tests/repository/test_patch_tracking.py`, `tests/repository/test_patch_updates.py` | Full stack transition fixtures; supported build |
| Guest generation/wrappers | `tests/compiler/test_recomp_generation.py`, float/vehicle join tests | Regenerate using verified local XBE, build |
| Settings/input/menu | Affected `tests/input/` suites and native C fixtures | Relevant native target, bounded isolated acceptance |
| Renderer/shader/resources | Shared source contracts, `tests/graphics/test_renderer_host.py`, `tests/graphics/test_rhi_menu_sampling.py` | Windows WARP / Mac `tools/testing/test-rhi-mac.py`; scoped scene evidence |
| Launcher | `python tools/ci/launcher-compatibility.py --local` | Host build/lifecycle and affected `tests/launcher/` fixtures |
| Issue automation | `tests/repository/test_issue_forms.py`, `tests/repository/test_issue_resolution.py` | Read-only fixtures, workflow checks; no posting for tests |

Enhancement routing uses the Ideas contact link in the issue chooser. The existing
`test_issue_forms.py` refresh fixture verifies that release updates preserve that
link and do not restore the removed enhancement issue form. The existing Windows
`launcher_maintenance.cs` draft checks cover the shared launcher Ideas destination,
empty diagnostics payload and unchanged bug-report round trip. These run in the
already registered issue-form and launcher workflows; native Mac builds consume
the same `DiagnosticIssueDraft.cs` URL policy.
The mocked version-refresh PR creation check also verifies that the generated
description preserves Ideas routing instead of referring to a removed form.
Version-only bumps use launcher release-policy, compatibility and both-host
package checks without LFS hydration. They do not prove publication.

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

`.github/workflows/runtime-patches.yml` calls `tools/ci/ci-runtime-scope.py` to
select transition/native/GPU/release checks. Launcher builds have separate
owners. New tests must be registered in the correct workflow; adding a test
file alone does not run it in CI. The lightweight plan also checks context
freshness and changed-system guide updates. Preserve required check names.
The Windows/macOS native step includes `tests/graphics/test_ghost_actor_overlay.py` for
the living-actor Ghost Gun composite. Its C fixtures test seven resolutions,
placement restoration, deferred draw flushing, unchanged primitive arguments
and mutations that remove the fix or break flushing/restoration. Wrapper checks
read the pinned runtime after the complete patch stack has been applied and
compile both Windows/native-Mac configurations. The original-US-routine case
requires the local checksum-verified `private/game/default.xbe` and skips in CI
without it. These contracts do not establish Mac scene fidelity or replace the
recorded Windows in-game confirmation; no personal saves are used.
The optional router context-reader contracts use Node's built-in test runner
(`node --test tests/repository/repository-context.test.mjs`); game building still uses the
existing Python/native dependencies. Patch-index loader fingerprints normalize
text line endings so an older Windows CRLF worktree agrees with fresh LF clones.
`tests/repository/repository-evidence.test.mjs` adds disposable Git/junction, source
provenance, redaction, budget and cancellation contracts for the optional broker;
the same lightweight plan runs both Node test files. No game build or GPU is
needed for these helpers. Native controller regressions and signed-in model
acceptance are machine-local checks, separate from repository CI.

The 2026-10-22 optional texture policy supersedes default bundling from `c932bee`.
Windows and native Mac builders no longer require asset hydration when repository
`config/textures/texture-pack.json` declares optional delivery. Both game packagers omit
textures by default; explicit `--with-textures` / `--texture-pack` still audit and
stream an offline bundle. `write_source_files` omits compiled DDS/JSON from the
nested source ZIP. Test these defaults without downloading real artwork.

`testing_profile.prepare_run` explicitly supplies `TSFP_TEXTURE_PACK_DIR` from
the checkout's `assets/textures` and `TSFP_TEXTURE_MODE=classic` after filtering
inherited diagnostics. Without that pack path F10 toggled modes but loaded no
replacements. Both Windows Test Lab and the native Mac unlocked runner consume
this shared environment; no runtime adapter change is needed. Asset-free
coverage lives in `tests/repository/test_testing_profile.py` and
`tests/gameplay/test_native_mac_tools.py` (both already selected by native CI). Windows
runtime loading must be checked separately; native Mac visual validation remains
pending. Missing packs retain the loader's Classic fallback for source-only tests.

Official game publishing reads the repository delivery policy. Optional delivery
requires no `Textures/` members and does not hydrate LFS. `textures` is null
or carries only `delivery: optional` and the exact-hash retired-icon policy.
`optional_identity` reads that small policy without touching DDS files. Packages
with retirements retain the published launcher 1.2.24+ requirement. Legacy bundled-policy fixtures still enforce complete asset identity
and launcher compatibility. `tools/textures/texture-release.py` separately audits and
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

`tools/network/bundle_online.py:bundle_standard_library` keeps `ssl.py` and top-level
stdlib PEM trust resources together as real files in the Windows portable
runtime. A CPython distribution may load adjacent policy during SSLContext
construction; placing the module in a ZIP while dropping its PEM files broke
alpha.2 packaging. Trust-policy bytes are preserved unchanged. The relocated
SSLContext regression runs in `test_publish_release.py`; the normal real-package
probe still checks Tk, TLS and certificate generation with the bundled Python.

`docs/project/STATUS.md` holds measured results; `docs/project/MAC_PORT_BACKLOG.md` records missing adapter
or validation work. Neither is an architectural index or a guarantee that all
listed historical tests still describe current policy. Current rules retain
original 60 FPS/60 Hz defaults, compatible native high-FPS opt-ins (separate
from retired interpolation), and distinguish API presents, GPU completion and scanout.
Use [testing-profile](../development/testing-profile.md), [building](../development/building.md) and
[macos](../players/macos.md) for full commands. Never auto-test through personal saves,
interactive launchers or persistent user settings.

## Bounded Arcade weapon-set route

Windows `tests/gameplay/stability_transitions_game.py --run-name issue105 --scenario
weapon-set` uses isolated Test Lab saves and visits Vietnam's Arcade weapon-set
menu. Captures identify screens; `weapon_set_metrics` checks settled five-second
guest-swap windows against 50 FPS. The repair passes; partial caches failed.
Counters do not measure GPU completion or scanout.
`test_text_read_scope.py` checks query reduction, nesting, boundaries, invalidation
and prompt-hook ABI/fallbacks. `test_frame_clock.py` extracts the Arm body for
cheap/late/no-signal pacing checks. Both run in runtime CI.
`test_shader_source_lookup.py` checks fresh shader contents and avoided hashing.
`test_texel_sample_lanes.py` checks scalar checksum/read-footprint parity across
lengths, strides and unaligned sources. Both extract production code, require
negative controls on both CI hosts; neither proves rendered parity. Sample-load
instrumentation preserves libc copy macros; a redefinition mutation must fail.

## Compatibility repair controller

`tools/ci/compatibility-worker.py` runs on the designated Mac outside CI. Install
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
`tests/repository/test_compatibility_worker.py` simulates these boundaries with local Git
and mocked GitHub/Codex, under the existing Windows/macOS gate jobs. No game
rebuild is needed. See `docs/release/launcher-compatibility.md` for installation/recovery.

## Text-entry regression route (#136)

`tests/graphics/test_menu_text.py` runs in the Windows/macOS runtime workflow. Its asset-free
fixture covers queue bounds, owner changes, character/command routing and embedded
keyboard recognition. An optional local case translates original insertion,
deletion and cursor routines from the checksum-verified US XBE and checks real
editing/length/font-width behavior; CI without that asset skips this case.
`tests/input/pc_controls.c` covers Windows message delivery and focus changes;
`src/native/menu_input_selftest.inc` adds SDL event/text-session checks. Shared
fixture success does not establish physical Mac typing. Live Windows Create
Profile typing and Enter were confirmed by the user with isolated Test Lab saves.

## Auto display regression checks

`test_pc_display_modes.py` also compiles shared Auto policy through stub SDL
snapshots on Windows and Mac; this tests policy, not Cocoa or actual atomics.
The Windows-specific fixture compiles the model against deterministic
monitor/window adapters: secondary fullscreen desktop size, windowed resize,
minimize/query failure, allocation rollback/retry, persistence and preview
rollback, manual override, and Auto ratio filtering. `tests/graphics/pc_display.c` retains
manual scene/camera/HUD tests. `test_cutscene_letterbox.py` checks effective
output and air-lift FOV (#339), including a guard-removal mutation. With a local
US XBE it executes all four lift regions through the production camera hook.
Launcher tests cover Auto UI/filter/save, defaults and older packages. Cocoa's
self-test recognizes Auto; physical Retina/multi-display checks need Mac.

The historical controls-only fixture in `test_patch_updates.py` excludes
`-auto-display.patch` until the complete-stack migration installs its display,
cutscene and fullscreen-effect prerequisites. It then asserts effective-size
hooks in the migrated title and renderer, preservation of local edits, and
idempotent reapplication. Fresh/current-stack tests still apply the Auto patch.

## Crane vignette validation (#221)

`tests/graphics/test_crane_overlay.py` checks manual wrapper registration and, when the
checksum-verified local US XBE is available, executes its original vignette
routine using both platform wrapper defines. `crane_overlay` in
`tests/graphics/test_ui_layout.py` exercises the production placement helper across nine
shapes, batch boundaries, partial viewports and multiplayer guards without game
assets. Both run in the Windows/macOS runtime workflow. Original-code tests
skip without the proprietary executable; fixture execution is distinct from
a live crane encounter or native Mac graphics validation (MAC-058).

`crane_scissor.c` executes the production legacy GL clip adapter and verifies
full edge coverage plus nonmatching-tag/partial-clip exclusions. The production
RHI extraction in `ui_viewport.c` checks the same captured 639,479 clip at seven
ratios and asserts stored guest state is unchanged. Fixtures run on both CI
hosts, not in native Mac gameplay.

The historical controls-only upgrade fixture defers `-crane-vignette.patch`
until the complete stack installs the display/viewport prerequisites. The
post-upgrade assertions require both the crane wrapper and renderer clip hook,
alongside preservation of local edits and idempotent reapplication. This does
not omit the patch from fresh/current-stack or renderer regression tests.
`test_ui_layout.py` checks #358 shield placement.

## Mounted gun input regression

`python -m pytest tests/gameplay/test_mounted_mouse.py tests/input/test_terminal_mouse.py -q`
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
one query for either address order, forward page spans, split/read-only/inaccessible
FOV maps, expiry, terminal routing and bounds. Persistent/uncached/unordered
mutations must fail. `camera_region_host.c` validates real OS protection changes
on Windows/macOS. Mocked locks/events are not physical input. `auto_look_region.c`
checks the pre-guest auto-look scope: one same-map query, split/read-only maps,
expiry and bit restoration. Its mutations run in registered `test_auto_look.py`.
`tests/input/pc_controls.c` and the SDL `--input-self-test` now exercise mounted
mode through the actual adapters, changing the native rate without recreating
input capture. The focused Windows `tsfp_pc_controls_test` target passes with
the isolated fixture config and scratch directory. SDL physical-host execution
is still pending. The portable packet fixture explicitly includes `<stddef.h>`
for `ptrdiff_t`; Mac Clang does not supply it through `<stdlib.h>` as MSVC did.

## Launcher stalled-network regression

`tests/launcher/launcher_updates.cs` holds a fake HTTP response pending until fixture
cleanup. Timeout and Continue Offline checks require the launcher task to finish
with the expected exception, cancel its request, and leave that response pending.
A ten-second deadlock guard bounds failures; the test no longer races a 500 ms
fake response against a 350 ms runner-speed assertion. Both launcher host builds
run this shared fixture; production timeout and cancellation policy is unchanged.

## Native integration checks

Retain native FPS and main's mounted mouse, text entry, Auto display, crane/Ghost
Gun and scoped-text tests. Historical controls upgrades defer dependent native
patches until full-stack migration; fresh stacks apply all patches.

`test_text_read_scope.py` covers scoped queries, nesting, fresh writes and
mutations; Windows prompt fixtures verify actual page protections and ABI.

The PR #285 CI index check also caught three pre-existing Windows patch files
with residual CRLF bytes despite their LF attributes. They were normalized to
the exact committed Git blobs and the index refreshed. Patch record hashes
identify actual bytes: refresh from canonical attribute-compliant files, keeping
the explicitly binary watermark patch unchanged. Local index success alone
does not prove a fresh checkout has the same patch bytes.

## Exporting updates from a pointer-only art checkout

`tools/textures/texture-repository.py --pointers-only` audits the private pack, stores DDS
objects through Git LFS and writes sanitized metadata plus small pointers. Its
clean command reads stdin without a destination filename: an existing pointer
filename can supply the wrong size and truncate the streamed DDS. Exact pointer
hash/size comparison remains mandatory. `tests/textures/test_build_textures.py` covers
first export and updating an existing pointer with a DDS larger than 1 KiB.
Use `tests/textures/test_texture_release.py` for format/hash/privacy and package limits.
No game launch is required for this export-only change.

## Original character icon policy

`config/textures/texture-originals.json` excludes user-retained original icons from
`texture-worklist.py` and `texture-hybrid-plan.py`. Offline regression coverage
is in `tests/textures/texture_pack_tools.py` (including rejection of accidental reinstall)
and `tests/textures/test_texture_hybrid_plan.py`. These tests are already registered in CI.
Asset removal also requires pack count/size audit and unchanged-file hashes;
no game rebuild is needed.

Retired-icon upgrade coverage also runs through `tests/launcher/launcher_textures.cs`
in Windows and native Mac launcher checks. `tests/textures/test_build_textures.py` verifies
that the exact retirement identities enter both package manifests and rejects
reintroduced original-only keys. Use `tools/build/build-launcher.ps1 -Test` for Windows;
CI provides native Mac launcher compilation and shared fixture execution.

Launcher v1.2.24 version preparation uses `tests/launcher/test_launcher_release.py` and
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

`tests/gameplay/native_locomotion.c` is lifted through the existing hash-verified US XBE
route in `test_native_timing.py`. It executes the complete walking routine,
with controlled input displacement and flat-floor world services. Tests cover
60/120/144 displacement, diagonal cap, stops/reversals, boost resource, selected
player/mode/button eligibility, depletion, slow/fast time and catch-up.
Keyboard W+D versus W travel uses the sampler's exact stick
components over one second at original 60 and native 60/120/144/180/240 FPS.
The controlled flat-floor results stay within 0.00005 world units at every rate.
A runtime mutation restores the old subtraction and must fail resource parity.
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

Optional texture publication changes (`tools/textures/texture-release.py` and its test)
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

`test_weapon_slots.py` checks shared key state and guest selection on both hosts:
press/held/focus/menu input, expiry, empty/grouped slots and local-player guards.
Windows Build's `pc_settings.c` checks migration and last-slot persistence;
launcher tests cover defaults, conflicts, Unbound, moving keys and reset.
Input-hook edits require supported regeneration; animations need gameplay checks.

## Themed keyboard editor checks

The historical old-controls fixture excludes keyboard-settings-menu,
keyboard-weapon-slots and controller-profile-routing until shared-input dependencies exist. The
final full-stack upgrade must contain the capture pump, two-binding limit and
weapon-slot sampling/guest hook, so both new patches are still validated.

`test_keyboard_menu.py` checks capture/page release, single presses,
cancel/focus/timeout, action coverage and sensitivity. Run with menu_mouse,
weapon_slots and quit_menu tests. Build checks native persistence/two-slot bounds;
launcher checks legacy four-slot import, third-slot rejection and two-slot save.
Live checks cover main/pause access, shortcut-free capture, conflict/clear/default,
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

`tests/textures/test_texture_runtime.py` compiles the actual shared queue, common RHI
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

`tools/release/release_channels.py` owns canonical X.Y.Z[-channel.N] package identity and
the minimum launcher 1.3.0. `tools/release/release-version.py` writes an explicitly chosen
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
suite and both-host CI. See docs/project/STATUS.md for measured local results; no runtime game
behavior or generated guest code changed, and no game build/publication is
established by these asset-free checks.


Grouped Preview UI: GameChannelPickerPage.cs uses both hosts' *Page.cs includes.
tests/launcher/launcher_shared_ui.cs covers tracks, Save state and launcher_saves.cs
(sparse backups, update protection, restore failures). Run
tools/build/build-launcher.ps1 -Test. --render-preview uses synthetic tracks.
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

## Temporal Uplink regression route

`tests/support/rhi_screen_copy.h` exercises the production GPU screen-copy pass by
copying a saved texture region after clearing the source scene. Run
`python -m pytest tests/graphics/test_rhi_menu_sampling.py -q` on Windows. The shared
runtime patch stack is checked by `tests/repository/test_patch_updates.py`; a native GL
build and live Scotland the Brave Uplink view remain separate platform and
visual validation routes. Replay captures alone cannot verify this effect
because the host copies are not serialized into replay frames.

## Water and impact-spark checks

See [water tests](water-tests.md) for CPU/GPU validation and live limits; see
[spark timing](native-impact-sparks.md) for the original guest regression route.

CI discovers unittest in each owning test folder. Changes to project_paths.py
or conftest.py select all runtime and launcher suites.
