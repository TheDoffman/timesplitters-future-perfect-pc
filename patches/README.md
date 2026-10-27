# Runtime patches

Active patches live directly in this directory. `legacy/` contains five
unwind-only migration patches for previously built runtimes. Both sets are
required; a retired feature does not make its patch safe to remove.

## Names and order

Use `NNNN-description.patch`, for example `0400-mac-native-boot.patch`.
The four-digit number is a base-sort slot, not an issue number or version.
The initial slots preserve the old alphabetical order with gaps of ten.
Choose an unused slot between existing entries when insertion is necessary;
do not renumber the stack just to add a patch. Keep descriptions lowercase
with hyphens. An active and legacy version of one patch share a basename so
the active version takes precedence during migration.

`tools/apply-patches.py:patch_records` owns the **effective** application order.
It starts with filename sorting and applies existing dependency overrides
(network/native boot, presentation, and final translator replacements).
The generated [patch index](../docs/agent-context/patch-index.json) lists the
actual order and touched runtime files. Never assume the number alone is the
effective order or apply the files independently in a shell loop.

The numbered names replace the old repeated `z` prefixes. Patch payloads and
context options are unchanged. Previously recorded runtime ledgers contain
their own names, bytes and order, so the loader can unwind those old names
before applying the renamed stack. Unrecorded runtimes use the retained legacy
patches. Git rename history supplies the old-to-new path correspondence.

## Updating a patch

1. Keep runtime changes here; preserve `vendor/ts3FPrecomp/source` exactly.
2. Update filename references in the loader, tests, fixtures and documentation.
3. Stage intended patches, run `python tools/repo-context.py refresh`, then
   `python tools/repo-context.py check --base <task-base-revision>`.
4. Run `tests/test_patch_tracking.py`, `tests/test_patch_updates.py` and
   `tests/test_repo_context.py`, plus the tests for the behavior being changed.
   Prepare the local runtime with `python tools/prepare-runtime.py` first.

Historical patch-stack fixtures describe migration scenarios. Keep their
membership and ordering when updating paths; do not turn them into copies of
the current stack.
