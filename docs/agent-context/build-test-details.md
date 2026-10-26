# Validation fixtures and Story probes

The team fixture verifies the scoped33ACDB setter receives1 for Story humans,
with original arguments/result/stack restored and NPC/other-caller/Arcade
fallback. Supported-US checks confirm original Story selects that shared team.
The four-peer runner checks every loaded human actor retains team1.

The handoff authority fixture verifies host/original fallback, guest animation-
callback suppression, independent pistol/rifle queues, original co-op slot1
ownership and preservation of its existing pistol ammo. Original-US contracts
include the rifle grant call sites. The isolated opening test compares each
owned ammo total with the host to catch animation-triggered duplicate awards.

The Story pose fixtures enforce the original256-byte sender ceiling, assemble
reordered/missing physical/pose parts and reject mixed checksums before applying
SNP2 animation and bounds. They reject
non-finite channels/negative bounds and stale body generations, and preserve
duplicate protection. The display fixture executes the common pose renderer,
verifying local asset/bone handles, crossfade asset resolution and root-motion
flag clearing. Guest dispatch checks Story frozen evaluation as well as existing
bot behavior. Runtime world snapshots expose host/guest clip/time metadata;
source checks alone do not prove live NPC animation or hit synchronization.

Story `--combat-probe` extends the bounded four-peer runner after firearm checks.
It uses ordinary movement/mouse/fire inputs, checks the original NPC ID decoder
would resolve the same entity, searches a bounded opening route for a nearby live enemy marked locally visible,
and requires a host-received guest-source damage trace plus agreeing NPC health.
No target, health drop from unrelated AI combat, or camera/ammo-only evidence is
not acceptance. Read-only world snapshots now include class/team, pose metadata,
last-damage tick and first-ID-match evidence from the original entity pool.
The hit-trace C fixture verifies one unchanged original call, stack/result/health
effects, disabled tracing and invalid-backlink/other-mode fallback.

The Story loadout fixture executes production ammo/grant observers, reliable
serialization, receive dispatch, loading/ownership gates and original-API calls.
It checks replay does not refill ammo, other players remain untouched, existing
inventory is preserved, and malformed/non-host/stale-generation commands do not
grant items. Supported-US call-site checks bind both handoff pairs to their
original inventory functions; live four-peer ammo/fire acceptance is separate.

The extra-start fixture verifies distinct signed16 pad IDs, private template
positions with unchanged room/heading data, tag/inverse lookup, delayed cached
resolution, late remote setup after activation, authored slot0/1 selection, checkpoint release, allocation failure and other-scene
fallback. Complete-stack fresh/repeated and historical Mac migration must also
include the three new original-pad wrappers; source tests alone cannot establish
collision-free playable starts.

The Story fixture now exercises host NPC serialization and guest body creation
from production adapters, initial state/team/identity/list preservation,
duplicate/out-of-order messages, loading queues, absence and ordinary fallback.
It rejects non-host senders, short messages and non-finite state. The reliable
creation message replaces a reproduced null-target guest spawn, rather than
discarding the valid spawn. Fresh runtime construction/pose/combat still needs
the changed four-peer build. The fixture also checks periodic host publication,
same-tick/rate bounds, later roster removal, absence and loading suppression.
Historical Mac migration excludes the dependent
creation patch before applying the complete stack.

The pending-start fixture covers state4 after Arcade ready placement is skipped,
alongside16, and confirms the one-shot token prevents later respawn attempts.
Active2 and script-suspended256 contexts retain their original handling. The
adapter validates the full cached-pad span before clearing each initial pad.

The activity adapter fixtures cover an existing Story NPC, stale/backlinked
entities, player contexts, controller callers, mode/phase and pool bounds. Guest
dispatch tests retain controller enumeration and ordinary bot behavior. The
supported-US fixture executes3568C0/358560 to prove the original controller gate
rejects NPCs and the subsequent script-ownership gate accepts only the host.
They also cover a dormant script actor with no active body and rejection of an
obsolete prototype, and verify dispatch supplies the caller's original entity.

The isolated `story-local-load` route enables Local NPC lifecycle tracing for a
direct original-versus-Online comparison. Scene telemetry includes placed-NPC
count+30 and labels unverified allocation counters by their numeric addresses.
Those counters must not be interpreted as human population. Numeric guest
return candidates are observations, not a validated stack unwind.

The NPC lifecycle patch's four boundaries keep independent eight-call trace
budgets. Complete-stack migration checks the scripted-spawn hook installation.
Scene metadata comparison now proves identical NPC budgets and record counts
between Local and four-player Online; it does not prove the spawn events execute.

Story pair-load samples include the numeric active config tail60..9F, to compare
mission/variant initialization independently of the limited scene/difficulty
summary. The authored-record trace is separately bounded to24 masked records.
Neither a predicate fixture nor a generated wrapper proves NPC initialization;
the first changed runtime probe still failed firearm acceptance after60s.
Local and paired Story samples also record the scene setup pointer, authored
NPC budget index/count, bounded record counts and actor/human allocation counts.
The refreshed Local reference reaches six characters and weapon14 with budget
index8/count5; use it to distinguish resource availability from NPC replication.

The original-US Story fixture executes6B100 with the verified rule jump table:
stock four-player/co-op rejection, stock network exclusion, and retained Normal
versus Easy/Hard and solo-only restrictions. Adapter tests check restored argument
and return/stack behavior, co-op normalization, retained solo/difficulty masks
and Arcade pass-through. Complete-stack migration installs the new wrapper;
the historical Mac-only fixture excludes this dependent patch before migration.

The Story lobby fixture covers the guest intro-to-game pending copy while the
original receiver is still phase15, with no host broadcast from the guest.
Ready fixtures cover zero-actor transport for pending Story gameplay and
ordinary pass-through after that pending start is consumed. The supported-US
357540 fixture executes its real transport tail for gameplay flags80008 too.

The suspended-spawn trace source check also verifies the original context reset
is preserved with its return/stack effects and existing suspension flag. The
patch migration assertion includes the original D50E0 wrapper.

The Story source fixture verifies that suspend/restore tracing preserves the
original suspension flags/context and return/stack effects. The migration
fixture also verifies installation of both original script-control wrappers.
`--script-observe-seconds 20` adds a passive bounded wait only when a peer is
script-suspended; it retains the initial startup evidence and assertions rather
than substituting the later state as a passing start.

`tests/story_pair_game.py --load-probe --lobby-controls --players 4 --fire-probe`
extends the isolated route to four actual game processes and primary-fire input.
Weapon IDs, aim and muzzle telemetry use the existing bot-shot field contracts;
they are not ammo or hit counters. Fire-input completion is recorded separately
from load/movement acceptance and requires reviewed captures. Neither proves
Story NPC/script authority or coordinated next-level behavior. Spawn-pad source contracts also check scoped slot/ownership/stack preservation,
valid-result passthrough and inactive/non-Story guards. The migration check
asserts the separate CF060 wrapper is installed. Revised notes require the
header's name, year, difficulty/trophy, thumbnail and population.

`tests/test_quit_menu.py` also compiles `tests/story_lobby.c` against the
production Story lobby adapter with isolated guest RAM. It checks host/guest/mode
guards, host Options and each peer Progress routing, preserved network inputs and frontend mode,
and difficulty confirmation without a local-game launch.
`tests/story_pair_game.py --load-probe --lobby-controls` adds host Scotland/Hard,
guest reliable settings receipt and Progress/Back before GO and movement.
The controls route sends an ignored guest Confirm rather than Ready; host GO must
work using automatic original readiness. Review its guest-confirm-ignored capture
for only Back/Player Progress, alongside the live lobby authority assertion. The
runner removes TSFP_STORY_LOAD_PROBE so this exercises normal typed Story start.
The changed route needs its own runtime receipt and reviewed captures; the prior
startup-only receipt does not certify these controls. These callback
contracts do not establish live lobby rendering or synchronized Story play.
The isolated `story-host-controls` scenario extends `story-host` with Y/level/
difficulty/X Progress/Back. Expected descriptor checks stop the route if level
or difficulty navigation differs, before a subsequent confirm could press GO.
Run with native process access: restricted probes have failed before the menus
with an invalid-handle exception; native-access runs reached the lobby cleanly.
Its bounded pipe drain uses `read1` and flushes each chunk: live descriptor
assertions must not read a log buffered until child shutdown. A menu mismatch
after that fix is real routing evidence; inspect the captured screen and the
`PC-STORY-SELECT` trace rather than treating the generic exit check as acceptance.
The difficulty step chooses mission 1 and checks the live allocated descriptor
against its trace, plus mission retention and custom-builder entry. Review the
capture for actual Easy/Normal/Hard choices; tracing alone is insufficient.
The probe's raw `mission` field is guest frontend data, not the host-owned Story
choice in network menus. Retention is checked against the custom dialog's own
mission trace. Review the returned lobby header for the selected mission name
and thumbnail, in addition to confirming menu descriptors.

`native_walk_trace.c` tests the actual diagnostic bridge; `native_walk_policy.c`
checks scopes/guards and `native_walk_contact.c` executes original C54A0 with
recorded geometry-service boundaries and independent missing-hook controls.
They check acceptance/ABI rather than complete real geometry. See the
[contact trace schema](native-player-contact.md) before interpreting live CSVs.

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
by them. The in-game chooser is described in `systems.md`. `test_quit_menu.py`
compiles its production callbacks for Windows/shared-native fallback, covering
row routing, original Local descriptor, Back, cancelled/successful typed Story
connections and allocation failure. Its optional local US-image test checks the
original Story route and callback padding. `test_fov_settings.py` retains whole
menu translation-unit coverage. Live navigation and Story gameplay require
separate checks.

For bounded Windows entry navigation, use
`tests/stability_transitions_game.py --run-name online-story-entry --scenario
story-entry`. It visits Local/Back/re-entry and opens/cancels the Story directory
using isolated Test Lab and online settings. Local Back returns to the chooser;
another Back returns to Main Menu. The cancellation helper checks only this
game's `pythonw.exe` child chain, including the venv interpreter child, and the
exact Story window title. Checking only direct game children misses the window.
Passing this route establishes entry navigation, not lobby or gameplay acceptance.

The `story-host` scenario uses its own ephemeral loopback-only directory and
disposable companion automation command under `private/network-validation`,
reusing the same isolated Test Lab profile routing. It connects Story, visits
Start New Match and captures the stock host lobby with opt-in numeric tracing.
Successful runs established host role 1/lobby phase 11 and the menu callbacks
documented in `systems.md`; later local runs failed at `No renderer capture`.
Do not label those failures passed, infer that a stock Deathmatch lobby is Story
co-op, or change capture cadence based on an assumed meaning of `RECOMP_FPS`.

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

`tests/test_network_guest_dispatch.py` checks 20Hz snapshots rendered at
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

`tests/test_native_vehicle_timing.py` runs portable byte-load, physics-scope,
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
