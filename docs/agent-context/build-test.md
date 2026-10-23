# Build, run and validation map

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
