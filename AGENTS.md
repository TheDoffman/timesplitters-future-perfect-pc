# Shared project rules for every device

These rules apply to every checkout, computer, operating system, and agent
working on this project. Read this tracked root `AGENTS.md` before making
changes. Keep shared rules here rather than maintaining different Windows and
Mac copies. Platform-specific commands below implement the same storage,
reproducibility, and validation requirements.

## Workspace and coordination

- Native Mac development and changes continued from this chat belong on
  `mac-port`. Do not move or merge them to `main` unless the user explicitly
  instructs it. Preserve other devices' separate Windows work.
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

## PC features and deferred Mac adaptation

- New Windows PC features may be implemented and completed on PC first. Do not
  automatically implement their Mac adapters in the same task unless requested.
  Before reporting a new PC feature complete, add or update its entry in
  `MAC_PORT_BACKLOG.md` so later Mac work has a durable handoff. Also record
  behavioral fixes that need corresponding Mac work.
- Each entry must describe the PC behavior, source files and commit/PR when
  available (otherwise mark uncommitted), Mac support already present, remaining
  adaptation, and acceptance checks. Use the backlog's statuses; a queued Mac
  adaptation does not block completion of the PC feature. If no Mac adaptation
  is needed, record why and distinguish verified support from pending validation.
- When asked to port features to Mac, consult `MAC_PORT_BACKLOG.md`, implement
  the requested entries, and update their status and evidence. Do not mark an
  entry verified from a successful Windows test or shared-source change alone.
  Keep measured validation in `STATUS.md` and link it from the backlog.
- Implement platform-independent gameplay, input rules, asset/configuration
  handling and runtime fixes once in shared source or the shared patch stack.
  Keep operating-system event handling, native settings windows and graphics
  API calls in platform adapters. Preserve reusable common logic while allowing
  new Mac integration to be deferred to the backlog.
- Changes to code already shared by both builds must preserve existing platform
  support. Review affected build paths, run relevant shared tests and available
  host checks, and report unavailable checks. Preserve existing settings and
  supported defaults. Deferring a new feature does not authorize regressions in
  existing Mac functionality.
- Use `.github/pull_request_template.md` to record PC results and link the Mac
  backlog entries. These instructions and the review checklist require agents
  to maintain the list; they are not an automated feature detector or CI gate.
- Git revisions must contain the change and backlog update before another
  checkout can use them. Keep this chat's work on `mac-port`; transferring changes
  between branches or devices must preserve their edits and follow the user's
  branch instructions. Shared-account access does not synchronize local files.

## Build and launch consistency

- Use the supported US Xbox executable, title ID `45410066`, with SHA-256
  `3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc`.
  Validate it before generation or game validation; do not silently substitute
  another region or revision.
- Use the upstream revision pinned in `tsfp.ps1`, dependency versions in
  `requirements.txt`, and the verified FFmpeg download in `tools/fetch-video.ps1`.
  Do not independently upgrade dependencies on one device. Intentional version
  changes must update the shared scripts/documentation and relevant checks.
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
- Native Mac defaults to up to 120 host presents per second through 2x render
  interpolation while retaining the 60 Hz guest clock. Preserve saved
  `frame_interp = 1` preferences; use `--fps 60` / `--fps 120` on the launcher
  or isolated runner for a temporary override. Report guest and interpolated
  presents separately. Never raise the console clock as an FPS shortcut.
  The shared patch stack enables optimized scalar native game code with debug
  symbols; GPU vertex programs retain the CPU interpreter as fallback/reference.
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
