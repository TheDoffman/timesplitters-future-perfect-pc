# Shared project rules for every device

These rules apply to every checkout, computer, operating system, and agent
working on this project. Read this tracked root `AGENTS.md` before making
changes. Keep shared rules here rather than maintaining different Windows and
Mac copies. Platform-specific commands below implement the same storage,
reproducibility, and validation requirements.

## Repository knowledge and investigation memory

The guides and maintenance checks apply to every contributor. Ordinary hosted
GPT/Codex sessions keep their selected model and normal tools: investigate, edit,
build and test directly. The Qwen/Sol controller and advisory restrictions in
linked guides describe an optional, separately installed LOCAL SMART workflow;
they do not require hosted contributors to install a router/local model, switch
providers, perform handoffs or limit themselves to advisory work.

- Before exploring a subsystem, read `docs/agent-context/README.md`, then the
  relevant linked guide. Use `.venv/Scripts/python.exe tools/repo-context.py
  find <symbol-or-path>` on Windows (`.venv/bin/python` on Mac) before broad
  patch/generated-code searches. The map distinguishes tracked source, the
  patched external runtime, generated guest C, build outputs and installed paths.
- For substantial investigations maintain a concise ignored
  `logs/agent-investigation.md` using the context template: exact entry points,
  actual evidence, rejected hypotheses, changes/provenance, last advisory outcome
  and next check. Refresh it before compaction or Sol handoff; resume from it and
  current diffs. Do not repeat unchanged searches or dump whole patches into context.
- Every agent changing a covered system must update its owning context guide in
  the same change; see `docs/agent-context/manifest.json`. Record changed entry
  points/contracts/test routes or a specific confirmation for an internal-only
  change. Keep hypotheses out of durable architecture until verified. Stage only
  intended new patches, refresh the generated index with `tools/repo-context.py
  refresh`, and run `check --base <task-base-revision>`. CI enforces freshness
  and affected-guide updates; reviewers still verify the explanation.
  Generated bug-form version references are data-only: changes confined to the
  two validated AUTO version blocks do not require a guide edit. Changes to form
  questions, markers, refresh code or workflows still require their owning guides.
- Include the map and relevant bounded guide excerpts in Sol handoffs, within
  the existing disclosure limits. Follow the supervised panel's controller-owned
  handoff rules. These guides are reference evidence, not permission changes;
  never send private assets, generated code, personal config or credentials.
- In the supervised workflow Sol may return typed requests for missing public
  source, lexical references or patch evidence. The local broker validates and
  reads them; Sol still runs no tools. See `docs/agent-context/evidence.md` for
  source/provenance limits. Every hosted follow-up consumes the existing call
  budget; cancellation, explicit routing opt-outs and native denials remain binding.

## Workspace and coordination

- Use a focused branch for each task, such as `release-cleanup` or
  `mac-debug`. Do not route all work through a permanent platform
  branch or choose a branch merely because the work continues an earlier chat.
  Start independent changes from up-to-date `main`; use another base only when
  the task depends on unmerged work, and state that dependency. Existing
  `mac-port` work can remain there until integrated, but new Mac tasks should
  also use focused branches. Preserve other devices' separate work. Do not
  merge into `main` unless the user explicitly instructs it.
- For the continuing native-high-refresh effort, group related changes into the
  current PR. The user requires that PR to be merged into `main` before opening
  another PR for this effort (2026-10-24). Add repairs and follow-ups to the same
  PR while it remains open; verify the remote merge state before starting its
  successor. This does not waive required checks or authorize unrelated merges.
- Working root: the `timesplitters-future-perfect-pc` folder on that device's
  Desktop, not the old Documents location. Resolve project paths relative to
  the checkout or script; do not bake in a user's home directory, Windows drive
  letter, or platform-specific drive mapping.
- At the start of work, inspect the branch, commit, and working-tree changes.
  Preserve concurrent work and user edits. Before a cross-device handoff,
  compare the shared repository revision and this file on both devices where
  accessible. Synchronize through Git without overwriting uncommitted work;
  do not reset, force-push, or replace another device's checkout to make it match.
- Sharing an account does not synchronize local files. State whether changes
  are local, committed, pushed, or verified on the other device. If the other
  device is inaccessible, report that its synchronization is unverified.
- Source, patches, default configuration, launchers, and these rules travel
  through Git. Game data, generated game code, source artwork, binaries, logs,
  personal settings, and saves remain ignored and local. Transfer those only
  when needed for the task, preserving the authoritative copy and settings.

## Shared features and platform exceptions

- Implement ordinary new gameplay, input rules, rendering policy, settings and
  fixes once in shared source or the shared patch stack, for both Windows and
  native Mac. Build on the shared interfaces documented in
  `docs/shared-platform.md`; extend their host adapters when needed. Do not copy
  feature logic into a second Mac implementation or guard portable behavior
  behind `_WIN32` merely because it was first developed on PC.
- Keep OS events/windows, graphics resource access and file-system operations in
  thin platform adapters. Shared shader algorithms belong in `src/graphics`;
  shader-language adapters map syntax/intrinsics rather than duplicate policy.
  Shared debug setting fields, labels, keys and capability requirements belong
  in `src/pc_debug_options.def`, consumed by both dialogs and JSON persistence.
- Genuine platform dependencies or an explicitly PC-only task may be deferred.
  Record the missing capability, why common code cannot yet supply it, and the
  adapter/acceptance work in `MAC_PORT_BACKLOG.md` before reporting completion.
  Distinguish implementation from each host's validation. Preserve unsupported
  saved options; do not silently reset them when another host edits settings.
  A deferred exception is not permission to regress existing Mac support.
- Each entry must describe the PC behavior, source files and commit/PR when
  available (otherwise mark uncommitted), Mac support already present, remaining
  adaptation, and acceptance checks. Use the backlog's statuses. An explicitly
  scoped platform exception may complete on PC while its Mac adapter is queued.
  If no Mac adaptation
  is needed, record why and distinguish verified support from pending validation.
- When asked to port features to Mac, consult `MAC_PORT_BACKLOG.md`, implement
  the requested entries, and update their status and evidence. Do not mark an
  entry verified from a successful Windows test or shared-source change alone.
  Keep measured validation in `STATUS.md` and link it from the backlog.
- Implement platform-independent gameplay, input rules, asset/configuration
  handling and runtime fixes once in shared source or the shared patch stack.
  Keep operating-system event handling, native settings windows and graphics
  API calls in platform adapters. Avoid a second settings/model definition in
  native UI code; map shared capabilities to available host controls.
- Changes to code already shared by both builds must preserve existing platform
  support. Review affected build paths, run relevant shared tests and available
  host checks, and report unavailable checks. Preserve existing settings and
  supported defaults. Deferring a new feature does not authorize regressions in
  existing Mac functionality.
- Launcher changes follow the same rule in both directions: implement common
  behavior once, adapt Windows/macOS system calls, and run both host checks.
  `docs/launcher-compatibility.md` describes the API-free build/test workflow.
  Its passing check is evidence of its scope, not a guarantee of visual parity.
  CI does not generate OS adapters. Follow-up fixes stay in reviewed source
  changes; no AI API credentials or paid agent calls belong in this workflow.
- Every PR description must explain the problem or root cause and how the
  change fixes it, in plain language. Include the resulting behavior, relevant
  validation and known limits. Keep this explanation current when updating
  a PR; do not substitute a list of changed files for how the fix works.
- Link resolved issues with GitHub closing references (for example, `Closes #128`)
  and fill in `## Cause and fix` plus relevant validation/remaining limits. After
  merging into main, the Issue resolution notes workflow copies those sections
  to each linked issue in this repository. Do not post a duplicate manual
  resolution comment. If the workflow fails because the explanation is missing,
  correct the PR description and rerun the failed workflow; reruns update the
  existing bot comment. Merging source is not evidence of a published release.
- Use `.github/pull_request_template.md` to record PC results and link the Mac
  backlog entries. CI runs shared contracts and selected host checks on Windows/macOS and checks
  backlog ID/status integrity. Register new behavioral tests in that workflow.
  These checks do not detect every missing feature or prove visual parity;
  agents must still maintain the handoff and report unavailable host checks.
- Git revisions must contain the change and backlog update before another
  checkout can use them. Keep each task's changes on its focused branch;
  transferring changes between branches or devices must preserve their edits
  and follow the user's branch instructions. Shared-account access does not
  synchronize local files.

## Build and launch consistency

- Use the supported US Xbox executable, title ID `45410066`, with SHA-256
  `3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc`.
  Validate it before generation or game validation; do not silently substitute
  another region or revision.
- Use the upstream revision pinned in `tsfp.ps1`, dependency versions in
  `requirements.txt`, and the verified FFmpeg download in `tools/fetch-video.ps1`.
  Do not independently upgrade dependencies on one device. Intentional version
  changes must update the shared scripts/documentation and relevant checks.
- Native Mac builds select the common Xbox/Vulkan frontend on macOS 26+ by
  default (`--renderer auto`), using the SDK pinned in `config/vulkan-sdk.json`.
  Older hosts select legacy GL; `--renderer gl` is the explicit fallback on
  any supported Mac. Ordinary renderer features belong in the common frontend;
  legacy GL gaps are platform exceptions in the backlog, not duplicated policy.
  `--setup` installs the pinned SDK locally when Vulkan is selected; normal
  builds verify it before source generation/patching and never silently switch
  frontends on SDK failure. Preserve its checksum, receipt, licenses and manifest
  inside ignored `external/vulkan-sdk`. Upstream/Python pins are unchanged.
  `tools/test-rhi-mac.py` runs asset-free GPU contracts. Both launchers and the
  bounded runner select verified SDK loading from `BUILD.json`; new Vulkan
  builds record MoltenVK, while older manifests without a driver field retain
  KosmicKrisp. Use the shared `tools/vulkan_sdk.py` selection helper for both
  launchers and tests; temporary driver overrides must not edit saved settings.
  Prerequisite checks remain read-only. Keep GPU contracts, guest frames and reviewed scenes
  distinct. The shared recorder counts successful presentation API calls and
  separates guest/interpolated/other kinds; it does not measure GPU completion
  or monitor scanout. Skips/errors must not inflate FPS. Full visual fidelity,
  including Khallos sky rendering, remains unverified on the common renderer.
- Apply the complete local patch stack with `tools/apply-patches.py` before
  generation/build. Preserve existing patches and add subsequent runtime
  changes as separate patches. Never hand-edit generated game C; keep changes
  in source or reproducible patches and regenerate when wrappers change.
- For native Windows builds, use `setup.ps1` / `tsfp.ps1` and
  the documented MSVC x64 workflow. Keep its build output in `build/windows`
  and the normal Release executable in `build/windows/Release`.
- For native Apple Silicon builds, use `tools/build-native-mac.py`
  with the same upstream/XBE/Python pins and complete patch stack. Keep one
  native build in `build/macos`; retain `build/windows` for Windows development.
  Use `tools/test-native-mac.py` for bounded tests in `private/test-saves/native-mac`.
  Both `RECOMP_DISPLAY_CONFIG`
  (upstream runtime settings) and `TSFP_DISPLAY_CONFIG` (PC display settings)
  must be isolated, along with the input/save paths. Never let runtime defaults
  write into the user's home configuration directory during an automated run.
- The native renderer reaches menus and visible first-level gameplay, with
  movement, firing and pause/resume checks. Full-level completion and complete
  rendering fidelity remain unverified. Preserve
  the distinction between guest-frame counts, successful host presents, reviewed
  frame captures, menu navigation and gameplay. The first menu does not establish
  playability or complete rendering support. `--capture` keeps two rotating BMPs
  in `private/captures/native-mac`; retain selected evidence and recycle redundant
  captures. Native `--test` also runs bounded real-GPU and SDL input regression
  checks. `--test-keys` is explicitly synthetic SDL event input, not a physical
  device test; distinguish it from macOS event delivery and human playtesting.
- Both Windows and native Mac retain original 60 FPS timing by default and
  the original 60 Hz console interrupt clock. The user explicitly requested
  launcher settings for the new native 144 FPS work on 2026-10-20. This authorizes
  opt-in 120/144 FPS beta choices for compatible native-high-refresh game builds;
  it does not restore the retired interpolation/prediction engine or automatic
  monitor-rate switching. Label beta limitations and preserve 60 FPS (Original)
  as the recovery option. Legacy presentation keys still migrate to
  frame_interp=1, frame_target=0, frame_cap=60 and refresh_rate=0. Preserve unrelated
  preferences. Keep runtime gameplay validation separate from launcher setting
  tests; a selector does not establish that older games support native high FPS.
  Do not raise the console interrupt clock. Keep shared renderer correctness
  fixes and GPU contracts independent of the retired presentation engine.
- `Play-Native-Mac.command` is the interactive ARM64 launcher. It preserves
  personal controls/display settings and uses `private/saves`; native runtime settings stay in ignored
  `private/native-settings`. Its `--check` is read-only. Automated tests must
  use the bounded isolated runner, never the interactive launcher. Interactive
  logs reuse `logs/native-play.out` / `.err`, capped at 4 MiB each.
- Mac F2/Cocoa settings use the shared PC bindings and display models. Keep
  atomic POSIX file writes and OpenGL resource management in the native adapters;
  keep binding validation, display/camera rules, texture keys and DDS acceptance
  shared with Windows. Settings tests must use a disposable controls copy, check
  backup/preservation/failure behavior, and never edit the fixture or personal file.
- Native regression tests use `build/macos/path-tests`. Recycle only their
  known `Partition0.img` through `Partition5.img` files after each test pass;
  preserve test metadata and profiles. The shared cleanup helper validates the
  resolved test root and refuses symlinks and unknown image names.
- For a transferred or downloaded build, verify checksums and retain its
  licenses and available build manifest. Record its source revision and any
  known uncommitted source changes. Compare it with the checkout before
  claiming it includes current features; an older release can be used for
  explicitly labeled compatibility tests. Do not relabel an old binary as a
  build of the current source.
- Recreate required build dependencies for the host OS; do not copy virtual
  environments, compiler caches, or an entire build tree between operating
  systems. Transfer only the required runtime files and provenance when the
  receiving device only needs to run the game.
- Honor `.gitattributes`: Mac shell launchers, Python scripts, and patches
  retain LF line endings; Windows `.cmd` and `.ps1` scripts retain CRLF.
  Preserve executable permissions on Mac launchers.

## Validation and user data

- For routine Windows gameplay tests, use a disposable copy of the local
  `private/test-fixtures/unlocked` Test Lab profile with
  `python tools/testing_profile.py --play --seconds 120`. It unlocks story,
  Arcade/characters and challenges while preserving fresh collectible progress.
  The runner reuses the launcher's `RECOMP_XMV_PLAY=0` startup-video switch.
  Keep the canonical fixture unchanged. Tests of first-run, startup videos or
  unlock progression must explicitly use an appropriate fresh isolated save
  instead. See `docs/testing-profile.md` for creation and routing details.
  On Apple Silicon, use the same local fixture through
  `.venv/bin/python tools/test-native-mac.py --unlocked-profile --seconds 120`.
  This resets only its disposable nested copy; omit `--unlocked-profile` for
  fresh-profile, startup or unlock-progression tests. Never replace the canonical
  fixture or prepare an active test directory; the native runner holds run locks
  through child shutdown and partition cleanup.

- Preserve `private/saves`, `config/pc-controls.json`, and
  `config/pc-display.ini` on every device. Create defaults only when personal
  settings do not exist. Never use personal saves or settings for automated
  tests, and do not overwrite them during transfer, setup, or an upgrade.
- Run the checks relevant to changed code. Windows runtime changes require
  patch validation and the affected native/build tests. Mac launcher changes
  require launcher tests and its prerequisite check. If a required host or
  dependency is unavailable, report the unrun check; a test on another OS
  does not replace it. Documentation-only changes need no game rebuild.
- Use explicit isolated save, input-config, and display-config paths for game
  tests, and bound their duration and capture/log volume. Confirm test routing
  before starting the executable. Reuse validation directories and follow the
  cleanup rules below; the interactive launcher is not an automated test runner.
- Record the source/build revision, host OS/architecture, relevant tool and
  graphics-backend versions, test scope, and outcome in validation summaries.
  Record measured behavior and remaining gaps in `STATUS.md`; keep usage in
  `README.md` consistent with the actual scripts.
- Distinguish prerequisite checks, executable startup, rendered menus, and
  gameplay. A missing-data dialog, successful build, or Windows gameplay test
  does not prove Mac graphics, audio, input, fullscreen, or save/load work.
  Do not describe this experimental port as fully playable without evidence.

## Project storage and texture work

- Preserve enhanced texture loading, Classic/Enhanced controls and installed packs
  across game and launcher updates. The user's 2026-10-22 instruction replaces
  the earlier default-bundling policy: enhanced textures are an optional download
  through the shared Windows/Mac launcher. Game packages omit them by default.
  Never remove an existing pack merely because a newer game ZIP omits it.
- Keep sanitized DDS/JSON in assets/textures (DDS through Git LFS), and keep
  source artwork/private data ignored. Audit hydrated assets before optional-pack
  publication; missing LFS content must stop that publication. Ordinary source
  builds and game packaging need no texture hydration. The Repository texture
  pack check gates asset/auditor changes, not unrelated version bumps.
- Publish optional packs as immutable bounded parts with file hashes. Adopt
  matching installed files, merge new/changed keys after confirmation and retain
  unrelated custom keys, saves and settings. Test interrupted updates and game
  upgrade/repair/rollback preservation on both launcher hosts. Explicit offline
  bundles may use --with-textures; official game publishing follows the optional
  policy. Preserve the 6 GiB cap, one private art-production pack and unpushed
  LFS objects; avoid duplicate working copies.

The user explicitly requires a lean project folder on every device. Preserve
this preference on all further work, including texture remastering and testing.

- Keep one game-data copy (`private/game`), the Windows build (`build/windows`),
  and the native build (`build/macos`) only while actively developing this Mac port,
  the user's `private/saves`, `config/pc-controls.json` and `config/pc-display.ini`, source, patches, and
  required build dependencies. Never use personal saves for automated tests.
- Keep one active texture pack in `private/texture-remaster/pack`. Its compiler
  enforces a 6144 MiB (6 GiB) user-requested budget. Do not silently raise the budget. Use 4K
  selectively for major surfaces and smaller sizes for other assets.
- Check physical disk usage before each large texture batch. Count source art,
  not just compressed DDS files. Keep temporary texture outputs below 256 MiB;
  inspect and clean disposable outputs before proceeding beyond that limit.
- Also check free space before large dependency downloads, game-data transfers,
  builds, or packaging. Report physical allocation separately from sparse
  files' apparent size; the texture-pack budget is not a total disk-use budget.
- Do not create duplicate game extracts, full game ZIPs, release staging trees,
  or isolated worktrees unless needed for the current request.
- Reuse one isolated texture validation save directory. At the end of a test
  pass, remove its disposable virtual disk images so their sparse 4.9 GiB
  capacity does not inflate the project folder's apparent size again.
- Retain selected before/after screenshots and validation summaries; remove
  redundant frame dumps and runtime texture exports once mapped to the catalog.
- Recycle disposable files with explicit, resolved paths contained in the
  intended test directory. Keep authoritative game files, user saves/settings,
  source artwork, and recovery records. Never bypass a denied deletion.
- Generated images and original game assets stay under ignored `private/`.
  Keep prompts, actual source resolution, resampling, and coverage recorded.
  Do not describe resampled 4K files as native 4K art or a partial pack as a full
  game remaster. Use built-in image generation for artwork.

## Texture overhaul validation cadence

- Prioritize artwork production and offline validation. The user requests minimal
  gameplay testing until the overhaul is near completion.
- Check every batch offline for UV/atlas alignment, tiling, alpha, dimensions,
  mip chains, format compatibility, hashes, coverage and storage budgets. Use
  contact sheets and selected comparisons instead of launching after each batch.
- Before the final validation phase, launch only for a specific loader/format
  blocker that cannot be resolved offline or a runtime integration change that
  needs a brief smoke check. Reuse existing runtime evidence otherwise.
- Begin consolidated scene testing when at least 90% of the audited in-scope
  replacement textures are installed and pass offline checks, and all asset
  categories have been reviewed. Unsupported or undiscovered assets remain
  explicit gaps; do not shrink the scope merely to reach the threshold.
- Final validation must cover representative environments, characters, weapons,
  effects and UI across the game, plus 1080p, 4K and ultrawide rendering, memory
  use and fallback behavior. Track untested scenes and hardware honestly. A full
  campaign playthrough is separate from texture validation.

## Active texture issue branch

- The user requests original character selection/profile icons (2026-10-22).
  Keep the keys in `config/texture-originals.json` out of generation and installed
  packs unless the user changes this preference. Character model materials and
  full-size scene artwork remain separate and in scope.

- The user approved hybrid production: individually redesign prominent assets;
  enhance simpler materials in bounded batches grouped by material family.
  Inspect originals and alpha before assigning a batch. Keep text, UI, effects,
  reflections and identity-sensitive atlases in specialist review. Unknown
  assets remain in visual triage; routing is not completion or approval.
- Prefer 8-16 independent assets per ready batch, grouped offline comparisons,
  and shared technical audits. Defer failed drafts to focused correction batches
  instead of repeatedly interrupting production. Preserve the existing 6 GiB
  cap, temporary-storage limit and near-completion playtesting threshold.
- Refresh `tools/texture-worklist.py`, then `tools/texture-hybrid-plan.py` for
  the reproducible queue. This planner does not generate or install images.

- Continue texture overhaul work for issue #74 on
  `74-enhancement-gfx-overhaul-to-modernize-textures`. Keep its source, patches,
  tests and progress together. Private artwork and the single active pack remain
  ignored in this checkout; do not publish or duplicate them for a branch change.
