# Runtime patches

Active patches are grouped by primary responsibility. Cross-cutting patches
stay together in the closest owning folder; do not split hunks to match the map.

| Folder | Responsibility |
| --- | --- |
| `compiler/` | XBE reading, disassembly, instruction translation and compiler fixtures |
| `diagnostics/` | Profiling, tracing, debug views, crashes and hangs |
| `gameplay/timing/` | Simulation, animation, scripts, camera, weapons and object timing |
| `gameplay/presentation/` | Frame scheduling, refresh settings and historical interpolation dependencies |
| `graphics/rendering/` | Backends, shaders, buffers, geometry, GPU state and surfaces |
| `graphics/textures/` | Loading, formats, streaming and Classic/Enhanced mode |
| `graphics/interface/` | Menus, text, HUD, overlays and screen placement |
| `input/` | Keyboard, mouse, controllers, prompts and menu interaction |
| `media/` | Audio output/streaming and video playback lifecycle |
| `network/` | Multiplayer services, synchronization and networked bots |
| `platform/` | Host boot/adapters, launcher integration, identity and desktop services |
| `storage/` | Save paths, host storage and profile persistence |
| `legacy/` | Five unwind-only migration inputs for previously built runtimes |

All sets are required; a retired feature does not make its patch safe to remove.

## Names and order

Use `NNNN-description.patch`, for example `platform/0400-mac-native-boot.patch`.
The four-digit number is a base-sort slot, not an issue number or version.
The initial slots preserve the old alphabetical order with gaps of ten.
Choose an unused slot between existing entries when insertion is necessary;
do not renumber the stack just to add a patch. Basenames must be globally unique
across active folders; duplicates fail before runtime files change. Keep descriptions lowercase
with hyphens. An active and legacy version of one patch share a basename so
the active version takes precedence during migration.

`tools/build/apply-patches.py:active_patch_paths` discovers active patches recursively,
excluding every `legacy/` directory. `patch_records` owns the **effective**
application order. It sorts basenames globally, regardless of folder, then applies dependency overrides
(network/native boot, presentation, and final translator replacements).
The generated [patch index](../docs/agent-context/patch-index.json) lists the
actual order and touched runtime files. Never assume the number alone is the
effective order or apply the files independently in a shell loop.

Moving patches between folders preserves ledger identities, payload hashes,
context options and effective order. Existing flat-layout ledgers remain valid
without changing runtime bytes. The generated index records each current
repository path separately from its stable basename.

The numbered names replace the old repeated `z` prefixes. Patch payloads and
context options are unchanged. Previously recorded runtime ledgers contain
their own names, bytes and order, so the loader can unwind those old names
before applying the renamed stack. Unrecorded runtimes use the retained legacy
patches. Git rename history supplies the old-to-new path correspondence.

## Updating a patch

`compiler/0197-independent-vector-support.patch` retires obsolete MMX/SSE
operation helpers after call frames, retaining an original `XMM_SCALAR`
constructor. The source-layout patch still runs last. Historical payloads are
unchanged; use the effective index rather than numeric sorting alone.

`compiler/0198-independent-scalar-support.patch` follows vector support, removes
obsolete numeric helpers and routes the preserved rounding assertions through
actual emitted instructions. MOVSX/MOVZX use direct project expressions, so
regeneration is required; the direction macro retains one argument evaluation.

1. Keep runtime changes here; preserve `vendor/ts3FPrecomp/source` exactly.
2. Update filename references in the loader, tests, fixtures and documentation.
3. Stage intended patches, run `python tools/repo-context.py refresh`, then
   `python tools/repo-context.py check --base <task-base-revision>`.
4. Run `tests/repository/test_patch_tracking.py`, `tests/repository/test_patch_updates.py` and
   `tests/repository/test_repo_context.py`, plus the tests for the behavior being changed.
   Prepare the local runtime with `python tools/build/prepare-runtime.py` first.

Historical patch-stack fixtures describe migration scenarios using stable
basenames. Tests resolve these against the active folders (or explicitly use
legacy versions). Keep their membership and ordering; do not turn them into
copies of the current stack.
