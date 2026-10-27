# Build and development guide

[Back to the README](../README.md)

For native Apple Silicon, start with the [Mac guide](macos.md). Run commands below from the repository root.

## Build on Windows

For a fresh checkout, install Git with Git LFS, Visual Studio 2022 with its C++ workload and
Windows SDK, and Python 3.12+, then run:

```powershell
git clone https://github.com/TheDoffman/timesplitters-future-perfect-pc.git
cd timesplitters-future-perfect-pc
.\setup.ps1 -Python 'C:\path\to\python.exe' -Iso 'C:\path\to\TimeSplitters - Future Perfect (USA).xiso.iso'
```

If you already have this checkout, run only the `setup.ps1` command. The script
validates the input paths and Python version, then runs Setup, Extract, Test,
Generate, and Build in order, stopping at the first failure. It requires internet
access for dependency downloads; it does not install Git, Python, or Visual Studio.
The default output is `build/windows/Release/futureperfect_recomp.exe`.
Build audits the compiled pack in `assets/textures`, committed through Git LFS.
Run `git lfs install` and `git lfs pull` after cloning if the files are still
LFS pointers. Source ZIP downloads may contain pointers; a Git LFS checkout is
the supported build input. No separate texture release is needed. Later builds
reuse these files offline. See the [texture build dependency](textures.md#texture-build-dependency).
Use `tsfp.ps1 Build -SkipTextures` for an explicitly lightweight build.
After setup succeeds, double-click `Play.cmd` to play from the repository.
The extracted files in `private/game` must remain; the ISO is no longer needed.
Use `-Configuration Debug` or `-Configuration RelWithDebInfo` for another build
configuration, and pass the same configuration to `tsfp.ps1 Run` to launch it.

The equivalent individual steps are still available for troubleshooting:

```powershell
.\tsfp.ps1 Setup -Python 'C:\path\to\python.exe'
.\tsfp.ps1 Extract -Iso 'C:\path\to\TimeSplitters - Future Perfect (USA).xiso.iso'
.\tsfp.ps1 Test
.\tsfp.ps1 Generate
.\tsfp.ps1 Build
```

Setup installs dependencies into `.venv`, prepares the bundled toolkit locally, initializes
SDL and XbSymbolDatabase, builds the symbol matcher, and fetches FFmpeg 8.1 for
intro movie playback. The FFmpeg download has a pinned SHA-256; if its mutable
upstream URL changes, setup stops for review. All tool downloads stay local.

### Bundled runtime source

The runtime, recompiler and Future Perfect target are tracked in
`vendor/ts3FPrecomp/source`, with original credits and licenses. Setup and CI
use `tools/prepare-runtime.py` to reconstruct the exact pinned runtime under
`external/ts3FPrecomp`, without downloading or contacting nemonicicon's repository.
The importer verifies every file, the complete Git tree and the original commit.
The reconstructed checkout is shallow and has no remote; historical patch tests
can still read pristine `HEAD` bytes and submodule pins remain unchanged.

Existing checkouts at the correct pin are reused without resetting local edits,
patch records, generated code or dependencies. Wrong revisions and incomplete
directories fail without replacement. Keep runtime changes in `patches/`; do not
edit the pristine snapshot or generated game C. The full patch stack still applies.

To prepare only runtime source (Python standard library and Git, no network):

```sh
python tools/prepare-runtime.py --verify-only
python tools/prepare-runtime.py
```

This removes the original runtime repository as a build dependency. Initial
installation still downloads pinned Python packages, SDL, XbSymbolDatabase,
FFmpeg and the selected host's SDK dependencies from their own sources.
The supported game files are still supplied privately by the user. Runtime
source is ordinary Git content, not Git LFS or a submodule, and is included in
the Windows/Mac release source ZIPs. Upstream links remain attribution only.

When intentionally changing the runtime pin, export pristine Git **blob** bytes
(not a patched working directory or an archive with checkout filters), file modes,
gitlinks and the raw commit into a new snapshot; update the shared pin and manifest
together. Verification rejects changed bytes or paths. Re-run snapshot, full patch
upgrade, both-host build and package checks before accepting the new baseline.
The launcher copies `config/pc-controls.default.json` to the ignored personal
`config/pc-controls.json` on first use. Existing personal bindings are preserved.

The runtime under `external/ts3FPrecomp` is shared when switching branches.
Each successful patch check records the applied patch contents, SHA-256 hashes,
order and context options in the runtime's Git metadata (`tsfp-patches.json`).
Subsequent builds unwind that recorded stack before applying the current branch's
patches, including patches removed, renamed or edited since the previous build.
This requires the updated loader on each branch. Branches running an older loader
do not update the record; update those branches before switching builds.

Existing runtimes without a record bootstrap using the active stack and the
unwind-only archive in `patches/legacy/`. The complete transition is validated on
temporary source copies before runtime writes. Unrelated edits are retained;
conflicting edits stop the update. Write failures restore changed sources and
leave the old record intact. The record contains patch text only, stays outside
tracked source, and requires no duplicate game data or build directory.

The **Runtime patch transitions** workflow runs on relevant pull requests to
`main` (opening, updating or reopening), relevant pushes to `main`, and manual
dispatch. Feature-branch pushes without a PR and documentation-only changes do
not need this check. There is no scheduled run: the same pinned sources and
patches do not need retesting throughout the day. Run it manually when preparing
a release or investigating a runner/toolchain change.

Windows and macOS keep the same source-only checks, scoped to the affected
components. Patch stacks, patch-loader tests, shared dependency/build pins and
workflow changes select the full suite. Runtime changes select native checks;
launcher and publisher changes select their own checks without replaying patch
upgrades. Shared display modes also select launcher checks because that file is
embedded in the launcher. Manual runs exercise all suites. Superseded runs on
the same PR/ref are canceled. Transition fixtures read pristine upstream bytes
once per test session while retaining a separate writable checkout for each
scenario; runtime regressions share one pytest invocation.
The loader applies each patch once inside its disposable validation tree,
relying on [Git's whole-patch failure behavior](https://git-scm.com/docs/git-apply#Documentation/git-apply.txt---reject)
instead of a separate dry-run command. The complete stack is still validated
before runtime writes; conflict checks, concurrent-edit checks and rollback
remain enabled. Legacy bootstrap keeps its applicability probes so write
failures cannot be mistaken for a legacy patch that is absent.

These checks need no ISO or personal saves and do not replace release packaging
checks or gameplay validation. Trigger/concurrency behavior follows GitHub's
[workflow syntax](https://docs.github.com/en/actions/reference/workflows-and-actions/workflow-syntax).

## Everyday workflow

Run in PowerShell from the repository root:

```powershell
.\tsfp.ps1 Status
.\tsfp.ps1 Test
.\tsfp.ps1 Generate
.\tsfp.ps1 Build
.\tsfp.ps1 SmokeTest -Seconds 30
.\tsfp.ps1 Run
```

`Run` starts `build/windows/Release/futureperfect_recomp.exe` with
`RECOMP_GAME_DIR` pointing to `private/game`. A smoke test captures output in
`logs` and ends the process after its time limit. Its report records crashes and
unresolved calls; a report command succeeding does not mean the game is playable.
Smoke tests use separate save folders in `private/test-saves` and capture frames
in `private/captures`. Interactive runs store saves in `private/saves`.

Double-click `Play.cmd` for the keyboard-and-mouse setup. Restart the game after
updating a build or editing the configuration file by hand. Changes saved from
the game's PC settings menu apply immediately.
`Play.cmd` closes its command window after starting a hidden launcher; game logs
continue to be written to `logs`, and launch failures appear in a dialog. The
portable release uses the same behavior after first-time game import. For terminal
diagnostics, use `tsfp.ps1 Run` (or `Launch.ps1` in a portable release) directly.

## Inputs and generated files

The supported `default.xbe` is 6,270,976 bytes with SHA-256:

```text
3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc
```

- `private/game/`: extracted game files.
- `private/pipeline/`: disassembly, function classification and translation reports.
- `external/ts3FPrecomp/titles/futureperfect/src/recomp/gen/`: generated C.
- `build/windows/`: MSVC build and executable.
- `logs/`: validation, build and bounded startup logs.

The original ISO is read without modification. Original assets, executable,
generated game code, tool checkouts and binaries are excluded from the outer Git
repository. Supply your own game dump when reproducing this setup.

## Sharing fixes between Windows and Mac

Both builds apply the complete `patches/` stack to the same pinned runtime and
use the same generated game code and binding parser. Put common behavior in
shared source, with OS-specific event handling and rendering in adapters; the
tracked `AGENTS.md` makes this the rule for every device.

The portable input model in `src/pc_input.c` is compiled by both builds. Its
movement normalization now runs on both platforms, so fixes to diagonal speed
use one implementation. The Mac SDL input regression checks the actual binding
path, while Windows retains its PC input integration tests. Native build
manifests include shared source hashes as well as Mac adapter hashes.

This does not make Windows-only features automatically available on Mac:
Advanced graphics effects, multiplayer and release packaging still need Mac
adaptation. Mac settings and debug tools have native implementations, with
remaining validation tracked in the backlog. Native Mac shares the camera input
model and button-prompt formatter, but the graphics backends remain separate. Record and test adapter
work before claiming a feature works on both platforms.

New Windows PC features can be completed on PC first. Before completing a PC
feature, add or update its entry in [the Mac port backlog](../MAC_PORT_BACKLOG.md)
with its behavior, source references, remaining Mac adaptation and acceptance
checks. Mac implementation happens in a later requested porting task; queued Mac
work does not block PC completion. Shared changes must still preserve existing
Mac functionality. Agents follow `AGENTS.md` and the pull-request checklist to
maintain this list; there is no automatic feature detector or CI enforcement.
Record measured validation in `STATUS.md` and link it from the backlog.

A change must reach the other checkout through Git and be rebuilt there. Use
a focused branch for each task, based on up-to-date `main` unless it depends on
unmerged work. Preserve other devices' uncommitted changes. See
[measured progress](../STATUS.md) for validation coverage.

## Upstream and licensing

The Windows runtime and Future Perfect target come from
[nemonicicon/ts3FPrecomp](https://github.com/nemonicicon/ts3FPrecomp), pinned to
`d90c92988a9f99d0580d2ca3f7efd6ec3a7b7845`, now bundled in this repository.
The snapshot and prepared checkout include its GPL-3.0,
MIT, third-party license and attribution files; retain them when distributing
any relevant runtime code. No game data is supplied by this repository.

This project's original work is licensed under GPL-3.0-only; see the
[README license section](../README.md#license), [LICENSE](../LICENSE) and
[NOTICE](../NOTICE). Verbatim license, attribution and contributor files for
the pinned upstream revision are preserved in [LICENSES](../LICENSES/README.md).
Retain the dependency notices and provide corresponding source and build
instructions when distributing GPL-covered binaries. The project license does
not grant rights to the original game or its translated code and assets.

`patches/save-routing.patch` fixes the runtime's device paths for Xbox profile
storage: Partition1's TDATA and UDATA now use the configured save directory.
Setup and Build apply it idempotently. Build also runs the native
`tests/save_paths.c` regression test against the actual runtime.
After all native build tests pass, Build recycles their temporary `build/path-tests`
data. Failed tests retain that directory for diagnosis; `private/saves` is unaffected.

`patches/pc-controls.patch` connects the window/input layer to `src/pc_controls.c`
and `src/pc_input.c`, and adds a manual wrapper around the US build's
`sub_000D2B80`. Its generated original body is retained as
`sub_000D2B80_gen`; the generated C is never hand-edited. Build refreshes the
generated code automatically when newly added wrappers are missing. The native
`tests/pc_controls.c` suite runs during Build.

`patches/recomp-float-joins.patch` preserves floating-point comparisons when
different code branches meet. This restores the original horizontal pickup-range
rejection in the supported US game. Windows Build also regenerates when the
generator sources change; native Mac builds request `--generate` when needed.
`tools/recomp-generation.py` records the source hashes beside the ignored game C.
The isolated `tests/test_recomp_float_joins.py` regression executes the actual
pickup routine when the supported XBE and a C compiler are available.

`patches/dsound-audio-timing.patch` fixes stream-clock handling after underruns,
packet submission, flush and pause; preserves refused buffer audio for retry;
resumes ADPCM at the exact decoded sample inside a block; and applies the
Xbox stream headroom setting so stereo audio retains its intended mix volume.
`patches/dsound-stream-lifecycle.patch` cancels queued host audio on stream
release and immediate/zero-time asynchronous FlushEx, preventing freed packet
memory from playing during level transitions. The build tracks HLE declarations
so adding audio hooks automatically regenerates the game wrappers.
The device clock
drives Windows stream completion, with the existing fallback retained for
backends that cannot report a play position. `tests/test_audio_timing.py` runs
the actual adapters and ADPCM decoder against a deterministic fake backend,
without game files or audio hardware. It discovers installed MSVC x64 tools on
Windows. The user confirmed the first-mission opening sounds clean after the
stream-lifecycle fix. Other levels and full A/V synchronization need playtesting.

`patches/pc-display.patch` adds the display menu and camera hooks separately
from the original controls patch, so existing checkouts can upgrade after a merge.
Setup, Test, Generate, and Build validate the complete patch stack in a temporary
source-only directory before changing runtime files. Conflicts name the failing
patch and leave the runtime checkout untouched. Existing patches should remain
stable; add subsequent changes as separate patches. Run the upgrade regression
tests with `.venv\Scripts\python.exe -m pytest tests/test_patch_updates.py tests/test_patch_tracking.py -q`.

`patches/pc-input-vocabulary.patch` keeps the Python input UI's key names in sync
with the mouse-button and F23/F24 names added to the runtime by the PC controls
patch. Setup, Test, and Build apply local patches idempotently.

`patches/pc-debug.patch` connects the renderer's frame statistics and F9 shortcut
to `src/pc_debug.c`. Build runs `tests/pc_debug.c` to check persistence, log
monitoring, live display options, and Debug dialog Save/Cancel behavior.
`tests/pc_debug_game.py` checks the real game's menu and fullscreen access using
isolated settings/saves and captures the dialog and overlay for review.
`patches/pc-window-shortcuts.patch` moves the runtime's frame capture to Ctrl+F11
so plain F11 does not trigger a capture.

`patches/pc-prompts.patch` connects input-device detection and shared player-1
controller input to `src/pc_prompts.c`. It wraps the supported US executable's
text measurement (`0x0003FCF0`) and rendering (`0x00040FA0`) functions using
`src/pc_prompt_guest.inc`, translating prompt codes before text layout. The
original strings are preserved for controller input. Build runs
`tests/pc_prompts.c`, including a virtual SDL controller test and guest-stack
checks. The virtual-controller check skips when a physical controller is attached.
Build automatically refreshes an older generated checkout when these wrappers are missing.

`patches/pc-fullscreen.patch` disables DXGI's automatic exclusive fullscreen
switch so the game's borderless fullscreen handler owns Alt+Enter and the
menu command. This prevents failed frame presentation and frozen video when
entering fullscreen. New Direct3D presentation errors are logged once per change.
`tests/pc_fullscreen_game.py` checks actual screen animation before and after both
fullscreen controls, using isolated settings and saves. Run it with
`.venv\Scripts\python.exe tests\pc_fullscreen_game.py` on an unlocked Windows
desktop, leaving the game in front until it exits. Screen captures and logs are
saved under `private/captures/fullscreen`; temporary test saves are removed.

The camera-field research also consulted the open-source
[MouseInjector TS3 driver](https://github.com/regilopes/MouseInjectorDolphinDuck/blob/master/games/ts3.c).
All hook addresses used here were checked against the supplied Xbox executable.

An independent [tsfp-recomp project](https://github.com/NeilSeligmann/tsfp-recomp)
supports the same executable hash, but its documented tested host is Linux.
Its status claims do not establish the behavior of our Windows build.

See `STATUS.md` for results measured in this workspace.
