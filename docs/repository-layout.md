# Repository layout and naming

Use one primary checkout with focused task branches. The checkout can have any
folder name; commands and scripts resolve paths relative to its root. An old
Documents staging folder, a release installation and a Git worktree are
different things. Confirm `git rev-parse --show-toplevel` and `git status`
before reorganizing files.

## Tracked files

| Location | Purpose |
| --- | --- |
| Root | README, contributor/legal files, version/dependency pins and supported command entry points |
| `src/` | Project-owned runtime adapters and shared game behavior; graphics adapters are in `src/graphics/` |
| `patches/` | Numbered runtime patches; see [ordering and migration](../patches/README.md) |
| `patches/legacy/` | Required inputs for unwinding older builds |
| `tools/` | Build, packaging, analysis and maintenance commands and their Python helper modules |
| `tools/launcher/` | Shared/Windows launcher source; `macos/` holds the native Mac adapter |
| `tools/release/` | Scripts and importer source copied into release packages |
| `tools/router/` | Optional local advisory integration |
| `tests/` | Regression suites and native harnesses; `fixtures/` holds intentional test inputs |
| `config/` | Tracked defaults, SDK pins and policy; personal configuration is ignored |
| `assets/textures/` | Published texture keys and metadata; DDS payloads use Git LFS |
| `docs/` | Stable player/developer guides; [browse by task](README.md) |
| `docs/agent-context/` | Maintained subsystem maps and generated patch index |
| `docs/assets/` | Selected documentation images and buttons |
| `vendor/ts3FPrecomp/` | Verified pristine runtime snapshot and provenance; preserve original names and bytes |
| `LICENSES/` | Third-party notices required by source and packaging |
| `.github/` | Workflows, issue forms and pull-request template |

## Local files

| Location | Purpose and cleanup boundary |
| --- | --- |
| `external/` | Prepared/patched runtime and dependencies; contains generated guest C and may contain work in progress |
| `build/windows/`, `build/macos/` | Supported build outputs; retain the active builds |
| Other `build/` subfolders | May include packaging output **or Git worktrees**; inspect before cleanup |
| `logs/` | Diagnostics and investigation notes; retain selected evidence and recovery records |
| `private/` | Game input, saves, source artwork, test profiles and captures; never treat this whole directory as disposable |
| `.venv/` | Host-specific Python environment |
| `__pycache__/`, `.pytest_cache/` | Regenerable Python/test caches; remove only when their jobs are stopped |

A release installation uses `Bin/`, `Support/`, `Data/`, `Settings/`, `Saves/`
and other launcher-managed directories. Do not merge that layout into the
source checkout or rename its folders as a source cleanup.

## Naming conventions

- Patches: four-digit base-sort slot plus a descriptive hyphenated name.
  Preserve migration inputs and inspect the effective loader order.
- Documentation and command scripts: descriptive lowercase hyphenated names.
  Existing root commands remain stable entry points for users and automation.
- Python importable modules: `snake_case.py`; tests discovered by pytest:
  `test_snake_case.py`. Hyphens and underscores here serve different purposes.
- C/C++: preserve the existing `snake_case` family names. `pc_` identifies the
  project adapter layer and does not by itself mean Windows-only.
- C# and XAML: keep class-oriented PascalCase names and platform subfolders.
- Texture filenames: retain their machine lookup keys. Renaming them for
  readability would break catalog/runtime matching.
- Keep backups, temporary outputs and personal configuration out of Git.
  Use a task directory under ignored `build/` or `logs/` for disposable work.

## Cleanup findings (2026-10-27)

At main revision `9f40dc21`, the repository had 6,303 tracked files. Most were
texture assets (4,522) or the pristine runtime snapshot (625). File count alone
is not evidence that these are unused.

This cleanup gives all 212 patch files readable numbered names while keeping
their bytes and effective order, removes the unused generic
`docs/assets/download-launcher.svg` and accidental
`tests/fixtures/pc-controls.json.bak`, and removes a duplicate ignore rule.
The Windows/Mac download buttons and canonical controls fixture remain.
Backup tests create disposable backups at runtime.

The five legacy patches remain necessary. Historical presentation patches
also remain dependencies of later fixed-60/native changes. Root launch commands,
release templates, licenses and the vendor snapshot remain supported inputs.
The documentation index groups existing guides without breaking public paths.

Local inspection also found older staging locations and multiple Git worktrees.
They are not tracked excess files. Check each worktree's status, unpushed commits
and ignored data before removing it with Git; do not recursively delete a
`build/` tree or assume an old folder name proves it is disposable. Keep recovery
records and the authoritative game, save, settings and artwork copies.
