# Build, run and validation map

See [build-test-details](build-test-details.md) for detailed adapter contracts and validation routes.

<!-- packet:start -->
Windows: `powershell -NoProfile -File tsfp.ps1 -Action Build` applies patches,
checks fingerprints, regenerates, builds and tests MSVC x64. Output:
`build/windows/Release/futureperfect_recomp.exe`. Never edit generated C.
Mac: `tools/build-native-mac.py` (`--generate` for stale code, `--setup` for setup).
Output: `build/macos/futureperfect_recomp`. Auto selects Vulkan on macOS26+,
otherwise GL. SDK pin: `config/vulkan-sdk.json`; driver provenance:
`tools/vulkan_sdk.py` and `BUILD.json`.
Isolated gameplay: `tools/testing_profile.py --play --seconds 120` (Windows),
`tools/test-native-mac.py --unlocked-profile --seconds 120` (Mac), with .venv.
Root AGENTS owns assets/isolation/storage. Build, startup and gameplay are
different evidence; unavailable hardware/private data remain validation gaps.
For movement input changes, sampler and guest-only fixtures do not prove
gameplay speed. Use an isolated Story run and compare actual player-position
displacement for straight and diagonal travel. Mouse capture checks should
also cover focus loss, stale held buttons, menu return and a clip released by
another component while the game remains foreground.
<!-- packet:end -->

Native SDL input validation checks both active Confirm slots (E and Space) and
rejects the retained third fixture entry (Return), matching the shared keyboard
profile's two-slot cap. Run `tools/build-native-mac.py --test` on the actual host;
this self-test correction does not change gameplay bindings or certify physical
keyboard input.

PR CI now offers a candidate `PR validation` gate in `runtime-patches.yml`.
Its plan selects runtime and launcher work from the same head, reuses the
Windows/Mac workers and checks selected results with `ci-pr-gate.py`. Existing
required check names continue to report until the branch ruleset migrates.
Unrequired issue-form and issue-resolution workflows are scoped to their own
source/templates, saving two jobs on unrelated PRs. `test_ci_pr_gate.py` covers
missing/failed/skipped results; `test_ci_runtime_scope.py` checks workflow wiring.

`test_push_gather.py` compares complete production gathering against its frozen
original:24,000 calls, fresh bytes, mixed formats/strides/offsets, holes/overlaps,
invalid declarations/readability and allocation failure. Guard pages bound source
and destination for sizes1..64; over-read/wrong-stride mutations must fail.
Both native CI hosts run it. Its optional six-pair benchmark uses2million96-vertex
draws per round, alternating methods; that CPU kernel result is not game FPS.

`test_zero_base_indices.py`: old/new HLE parity, fresh indices, failures, stream
dispatch and copy/rebase/leak mutations. Both hosts; mocked draws.

Reload #337: `test_native_timing.py` covers CMP policy/mutations; the private-US
`native_reload_input.c` runs decoder/consumer plus425 buffered taps.
`reload_phase_sweep.h`:864 schedules/1,728 presses against policy and US routines.
`test_reload_keys.py`:1,360 host tap schedules at60..240, held/fresh Fire, expiry,
lifecycle, both triggers/alternate bindings and equal-timestamp Reload/Fire
release/repress ordering. Windows `pc_controls.c` tests the real event bridge.
Both CI hosts run these; missing XBE skips originals. Physical keys/Mac gameplay
remain separate checks.

`test_model_transform.py` compares memory/GPR/SSE in four rounding modes and
callback mutations. Fast results and finite fallback results are bit-exact;
fallback arithmetic destinations allow quiet-NaN payload variation (also seen
with fast entry disabled). Rejected inputs leave all bits/FP flags unchanged.
Public fixtures/mutations run on both hosts; US-body tests need the private XBE.
Camera/mothership/spawn/rigid fixtures include the shared helper.

Crash breadcrumb/health tests: `test_crash_capture.py` checks Windows capture,
helper death and the portable ring; `test_native_crash.py` checks Darwin output.
Both hosts run these and shared UI identity/failure/ZIP tests.


`stability_transitions_game.py --scenario rocket-impact --native-fps 240
--watch-actors --watch-effects --seconds 180`: Temple floor rocket, cleanup,
respawn/walk;0 selects Original60. `sample_wait` observes at most20Hz; effects
remain non-atomic; missing player reads are counted/excluded. `rocket_impact_stats.py` checks shot/death/lifetime/recovery;
counterexamples run on both CI hosts. Lethality is not damage-magnitude parity;
captures are not synchronized to memory. See STATUS for live evidence.

The Windows runner's optional `--watch-native-clock` brackets read-only timing-
call counter7497B8 and level ticks with perf_counter_ns/QPC timestamps plus scene/
match identity checks. This marker increments in the timing routine before
gameplay returns; it is independent of accepted-Present CSV counters and is not
completed simulation or GPU work. Compare successive bounded observations with
Present clocks, allowing one in-flight call. Exclude resets, transitions, pause
and wide read spans. Capture-free runs retain lightweight snapshots; full native
traces and renderer captures belong in separate diagnostic runs.144 remains a
legacy diagnostic rate; the menu presets are60/120/180/240.

`native_tree_motion.c` (via `test_native_timing.py`) executes the original US
TREE callback1D88B0, with only its two effect services replaced by spies. It
covers phase/pose, crossing and retirement times, single events, pause, slow
time, carried phases and exact whole/catch-up fallback through240; removing
the gate correction fails. The asset-free native clock fixture checks this
policy on both CI hosts. A private-XBE skip is not physical Mac/scene evidence.

`native_trap_motion.c` executes the original TRAP callback and sine-table init.
40 rate/slow/phase schedules require fresh pre-sound poses, an Original one-tick
pose envelope, aligned endpoints, event/lifetime bounds, terminal state, phase, pause and ABI.
12 mixed-rate/jitter schedules exercise live switches. Whole/catch-up state and
events remain byte-equal. Gate, both conversions, both coupled operators and
contact normalization are independent failing controls. The asset-free clock
fixture covers coupled force/travel/contact and GPU-store composition; existing
`test_gpu_flush_ack.py` retains actual-store/guard/fallback coverage. See
[script clocks](native-script-clocks.md) for numerical and live-acceptance limits.

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

See [choose the cheapest relevant check](build-test-details.md).

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

Windows `tests/stability_transitions_game.py --run-name issue105 --scenario
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
## Story load and paired-lobby probes

`tests/story_pair_game.py --load-probe` is a separate, opt-in experiment using
`online-story-load-pair-host/guest` disposable roots and `story-load-pair` evidence.
It retains one exact executable/PDB archive as `probe-symbols.zip` BEFORE launch,
validates its BUILD.json executable hash against the seal and records the archive
hash in the receipt. A subsequent build must not be substituted when resolving a
probe dump: the resolver checks the dump's PDB GUID/age. This snapshot prevents
routine rebuilds from discarding the crash's matching symbols.
The optional `--receipt-id` caller nonce is echoed in the saved receipt. This
identifies a diagnostic invocation across Windows venv launcher/interpreter PID
redirection; parent PID equality must not substitute for this invocation check.
Offline resolution additionally matches the joined game PID and executable hash,
so an older startup crash cannot be mistaken for the loading fault.
After roster join it sends guest Ready and host GO, currently tests mode4/phase15,
matching Story scenes, two distinct actors and one local view per instance,
then checks both actor positions changed after separate host/guest input.
World position is entity+0x64 (matching the replication adapter), not bounds
centre+0x1A8. The runner samples twice per second during input/command waits
(ten times per second while unpaused in a Story scene),
bounded to2400 samples, recording health, actor life/flags, game tick and pause
flags, original main-update state/timer and network end state. These observations
also include config rule/variant and the original result flags/failure timer.
Engine context count/pool and up to four context states at+114 distinguish those
contexts from healthy actor bodies. The asset-free Story fixture checks the
pending-load spawn adapter calls the original D50E0(context,1) once per admitted
context, retains the original frame call/stack and never retries later deaths.
distinguish entry into failure state8 from propagation of network end status2;
they do not change either state machine. Before movement the paired probe rejects paused/failure menus or nonliving
actors. It does not send E after direct gameplay load: doing so could retry a
Game Over menu and obscure the first failure. Inspect states.json for the first
spawn/health/pause transition; actor allocation alone does not establish play.
Review both load-before/load-after captures: positions alone do not prove camera
control, mutual interaction, Story script/NPC authority or next-level flow.
Default paired-lobby behavior still sends no GO. No build or probe may run while
an unrelated interactive game uses the executable. Guest-dispatch tests use the
repository native compiler discovery on Windows rather than skipping when cl is
absent from PATH; their passes are ABI evidence only.

`tests/story_pair_game.py` checks the executable seal and normalized source
identity, then runs two disposable Story-connected instances against one loopback
directory with separate bridge ports. Success requires both rosters to contain
two players, host phase11 and guest phase12, and clean runner shutdown. This is
a lobby transport precondition only; it sends no GO and does not prove Story play.

`tests/stability_transitions_game.py --run-name online-story-load-baseline
--scenario story-local-load` exercises the original Local Story path, captures
named frontend/active configuration fields and writes `story-load-baseline.json`
only after observing Story mode, a gameplay scene and an actor pool outside a
cinematic. Review `local-game.png` as well as the assertion. This diagnostic needs
no rebuild after Python-only changes; it does not test network loading. Both
routes retain the runner's isolated settings, fixture and cleanup guards.

The changed Story detail-row route requires visual review of host and guest
returned lobbies: selected localized briefing must replace Arcade details without
covering the roster, and header year/contrast must be assessed from captures.
Constructor source checks alone cannot certify text layout or visible year.

`story_pair_game.py --load-probe --lobby-controls --lobby-only` stops after guest
Confirm in the lobby. It verifies settings/Progress/authority and captures the
changed UI while avoiding an unnecessary repeated level load. Receipt scope and
status distinguish this from gameplay; old load-before/after PNGs are stale.

Story paired harness --players4 launches one host and three guests, each with a
separate disposable run/bridge port, and requires all four original rosters plus
matching selected settings. Four-player evidence is kept in story-four, distinct
from the prior paired proofs. --lobby-only can verify admission without GO.

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
one query for either address order, forward page spans, split/read-only/inaccessible
FOV maps, expiry, terminal routing and bounds. Persistent/uncached/unordered
mutations must fail. `camera_region_host.c` validates real OS protection changes
on Windows/macOS. Mocked locks/events are not physical input. `auto_look_region.c`
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

Story/main merge (2026-10-22): the bounded transition runner retains all Story
scenarios, stop requests and load observations alongside native FPS, LAN and
surface probes. Both `hold_seconds` (Story callers) and `hold` command fields
are accepted; observations share the perf-counter clock. Menu fixtures retain
Story stubs alongside the separator/font behavior required by Video Options.

The revised Story acceptance route defaults to Time to Split (mission 0), Normal.
`story_pair_game.py --load-probe --lobby-controls --players 4 --fire-probe`
selects that mission through host Options. Use `--mission 1 --difficulty 2` for
the historical Scotland/Hard diagnostic. Receipts retain requested selection;
scene agreement is required across all four peers, not only the first pair.
Intro, combat/world authority, separate starts and host restart acceptance remain
independent requirements; movement-only success does not complete Story co-op.

First-level movement diagnostics retain per-slot context flags/local port, action binding masks, local/network
pad axes and original input-block/source selectors during key holds. These are observations
of original fields, not input or state overrides; separate body movement assertions
remain mandatory. This distinguishes collision/startup state from missing input.

`tests/story_lobby.c` checks the revised header metadata against a sparse
roster and localized year and verifies font/stack restoration and invalid
difficulty rejection. It does not establish visual layout or trophy rendering.

The Story lobby C fixture also checks that six authored character entries
survive Arcade-only pruning while ordinary removal, Arcade and lobby paths
still invoke the original function. Patch migration checks the separate roster
wrapper; live NPC state and interactions remain separate acceptance gates.

The Story factory fixture now requires original stage1 for direct gameplay
and checks the stage on guest activation. Runtime probes record Story stage
and active character count alongside pad diagnostics.

The changed pad probe confirms actual nonneutral local/network axes during
four-peer movement with input-block0 and passes its narrow movement assertions.
Two earlier first-level probes failed with stationary bodies; the later pass
therefore does not establish a fix for those failures. Fresh positions remain
spread across the scene, and firing input is not proof of shots or hits.

Explicit `world_probe` command captures inspect at most 64 original active
characters, including original type2 network object IDs, positions and health.
Regular sampling records only the active count to bound overhead/storage.
The paired route requests these at load/movement/fire boundaries; membership
and matching IDs alone do not certify synchronized NPC actions or damage.

The Local Story diagnostic also records settled startup, forward/back input and
fire captures using the same isolated profile. Its character-list probe accepts
both low guest heap addresses and high Xbox aliases; requiring high addresses
incorrectly omitted every low-heap character. Reads remain bounded kernel RPM.

The gameplay stage fixture invokes the actual stage-builder entry65940 with
a config argument. Frontend factory2D2C70 resets stage0 unconditionally; merely
setting stage1 before that factory did not fix configuration construction.

The pending-load fixture requires the original missing-pad sentinel before
D50E0. This guards against using Arcade staging pads for initial Story spawns;
existing mixed-state and no-resurrection checks retain active/later contexts.
World character entity reads now follow the first word, as in the established
opening-handoff observer; character+8 is an actor-record link, not its entity.

The registered quit/Story suite now lifts and executes complete original65940
from the hash-verified US image with original first-mission metadata. It checks
stage0 intro versus stage1 gameplay, local co-op characters, full gameplay field
initialization and guest stack/callee-saved registers. CI skips this asset-bound
case without the XBE; callback fixtures remain asset-free. Generated C stays in
the disposable test directory.

The first-level route now moves each owner backward in sequence: the measured
Local start has a wreck blocking forward motion. It retains strict displacement,
living contexts and peer agreement. Local input ownership is checked explicitly.
`--fire-probe` requires each first-level owner to receive weapon14 and consume
actual ammunition, summing both magazines and shared reserve once per type;
unarmed punches or reloads cannot count as gunfire. World hits remain separate.

The roster fixture covers pending-load preservation while the active scene
is still the lobby and pre-play phase14. Ordinary lobby pruning remains live.
The bounded PC-STORY-ROSTER trace measures whether the corrected boundary is
actually reached; list preservation alone does not prove NPC replication.

The first-level paired route requires fresh intro captures on all four peers
before sending host skip, then retains strict gameplay/movement/fire checks.
Story callback fixtures cover intro metadata/difficulty retention, guest advance
rejection, host authority, occupied-header preservation and generation255->1
through the reliable config sender. Original65940 fixture distinguishes real
intro/gameplay construction. Neither source fixture certifies live video or
coordinated loading; inspect the changed intro-before and gameplay captures.

Intro readiness accepts the observed cinematic phase10 (also14/15) as distinct
from gameplay15. Original64CE0 returns false for flags20000000, so357540 does
not activate ordinary gameplay transport there. The first four-peer intro run
reached scene24/flags20000000 on every peer while remaining phase10. The Story transition retains that
boundary and sends a new reliable gameplay start; no forced phase15 write is
used to make an intro pass a gameplay assertion.

The cinematic-ready fixture requires the original transport routine to see
zero actors and ordinary eligibility only within that call, then verifies
restored cinematic flags/count, original return/stack effects and peer-ready
side effects. This tests the boundary; actual four-peer long movie and handoff
acceptance must come from a changed build. One earlier phase10 diagnostic peer
returned to menu before cleanup, so its four captures are not a passing intro.

The original-US fixture also executes64CE0/3533F0/357540: an ordinary movie
cannot emit ready, while the scoped eligible/no-actor call emits the stock
class10 ready packet and peer-state updates without any player-body helper.
This checks the real transport tail rather than reproducing it in test code.

The default first-level acceptance route now lets the intro finish naturally,
with an extra fresh progress capture after25s and a450s parent bound. This
exercises cinematic connection lifetime instead of masking a timeout by early
skip. `--skip-intro` is an explicit shorter diagnostic. After the original intro,
the route gives the original scripted firearm/control handoff up to60s, recording
first gameplay separately and rejecting Game Over/failure/disconnection promptly.
It never forces control/life/result flags; all four must genuinely equip14.

Live current-state samples use a temporary file and atomic replacement, retaining
the previous complete sample if a Windows reader briefly prevents replacement.
The Story parent retries unavailable snapshots before asserting disconnection or
opening failure. The cinematic-ready run's empty guest2 dictionary was a read
race: its final on-disk sample still showed phase15 and network enabled, matching
the other peers. That run does not establish natural intro completion.

Human-team contracts bind originalD7570 direct initialization evidence and
exercise frame normalization for all four slots in loading/gameplay, plus the
D7550 pump, non-Story and invalid-backlink fallbacks. The four-peer load probe requires every human team to be1;
matching each local team alone does not pass this gate.

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

## Release and launcher validation routes

Channel/version/artwork/build policy and detailed fixture routes are in
[launcher context](launcher.md), [game releases](../releasing.md) and
[launcher releases](../launcher-releases.md). Release CI selects release_channels.py,
release-version.py and test_publish_release.py for identity, legacy history,
Beta publication, pushed sources, hotfixes, ancestor notes and prepared reuse.
Both packagers record identity and omit optional textures from official packages.
LauncherSharedUiChecks covers grouped channels/preferences/metadata;
launcher_versions.cs covers paginated discovery. Run build-launcher.ps1 -Test
and both host checks. GameChannelPickerPage.cs uses shared *Page.cs compilation.
--render-preview is synthetic, with no preferences/GitHub changes.
Windows artwork selection uses explicit arguments, hash-verified cache, then
legacy private art; fetch-launcher-art.ps1 recovers pinned art without executing
it, and -RequireArtwork checks selection. Shared tests/version edits do not prove
publication, gameplay or host visual/lifecycle acceptance.

Story/main merge (2026-10-23): test_quit_menu.py verifies the relocated Story
callback padding in the supported US executable. The shared display fixture
retains pose, keyboard and host drive contracts without duplicate stubs. Bot
motion mutation fixtures copy pc_character_pose.h with the shared state header
now required by Story's numeric pose model. Run quit/menu, FOV, guest dispatch,
bot results, keyboard/storage/weapon fixtures and the supported optimized build.
Native gameplay/Online and four-peer Story acceptance remain separate checks.

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

## Temporal Uplink regression route

`tests/rhi_screen_copy.h` exercises the production GPU screen-copy pass by
copying a saved texture region after clearing the source scene. Run
`python -m pytest tests/test_rhi_menu_sampling.py -q` on Windows. The shared
runtime patch stack is checked by `tests/test_patch_updates.py`; a native GL
build and live Scotland the Brave Uplink view remain separate platform and
visual validation routes. Replay captures alone cannot verify this effect
because the host copies are not serialized into replay frames.

## Coupled water operator checks

See [water and procedural-surface validation](water-tests.md) for the complete
CPU/GPU fixtures, live capture routes, controls, and acceptance limits.

Story/main integration (2026-10-26): preserved Story adapters with current main
timing/render/input contracts; 15 menu/Story/guest fixture passes,139 Online/
context/native timing passes, optimized Windows build/native checks completed.
The transition runner retains both scenario families and input publication.
Fresh four-peer acceptance and native Mac Online remain separate.

Story restart fixtures exercise host send/original return, guest authority rejection, validated reliable receive, duplicate/short/non-host/checkpoint rejection, epoch isolation and Arcade fallback. Runtime coordinated reload and changed first-mission combat remain separate acceptance checks.

The team fixture now reproduces D7570 assigning slot+1 after a frame normalization, verifies immediate team1 correction with original ABI/result, and rejects incomplete body/context backlinks. A real four-peer startup must retain all human teams before the reload probe.

First-mission reload/combat route: `tests/story_pair_game.py --load-probe
--players 4 --fire-probe --restart-probe --combat-probe --skip-intro`. Intro
skipping is diagnostic; natural intro acceptance remains a separate run. The
registered test_story_evidence.py accepts corpse retirement only for a dead or
already absent host target, rejects missing live targets/non-finite health and
retains the requirement for a host-received, guest-attributed damage trace.

Story damage fixtures cover owned human/direct and projectile-owner requests, remote/NPC replay suppression, incoming host damage suppression, stale spawn/match and host/Arcade fallback. The guest dispatch fixture requires this authority guard before tracing, preventing instrumentation from executing a replica death path. Preserve the exact crash-symbol archive beside the retained guest dump before replacing the build.

Changed Windows four-peer acceptance (2026-10-26) passed opening, shared human
teams, owned movement/ammo, host-only level reload with fresh handoff, and
original host receipt/application of guest NPC hits through death. All runners
exited0 after the damage-authority correction. Full mission completion,
checkpoint/Game Over handling and native Mac/Internet validation remain separate.
