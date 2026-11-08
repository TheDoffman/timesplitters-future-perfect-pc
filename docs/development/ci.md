# CI and branch protection

The active `Base` ruleset requires **PR validation** on every pull request and
requires the branch to include the latest `main` (verified 2026-11-08). The
workflow keeps that required name. This change does not alter repository rules,
merge policy or reviewer requirements.

## Run runtime checks before pushing

Activate the checkout's host-local Python environment and install the pinned
`requirements.txt` and `requirements-release.txt` dependencies. From the checkout
root, run:

```sh
python tools/ci/run-runtime-checks.py all --prepare --failed-first
```

This prepares the bundled runtime and applies the complete patch stack, then
runs the same patch, native CPU and release test lists used by CI for your host.
It runs previous failures first and still executes all remaining cases. Failure
in one suite does not hide the next suite. The command exits nonzero if any
suite fails. It does not build or launch the game or touch personal saves/settings.

For a narrower implementation loop, choose `native`, `patches` or `release`
instead of `all`. Use `--workers 1` for serial diagnosis, `--list` to inspect
selected files, or `--collect-only` to check collection. Listing/collection is
not a passing execution. After targeted repairs, run the complete relevant
suite before pushing, rather than stopping after the first fixed test.

`tools/ci/runtime-suites.json` is the single source of these file lists; register
new regressions there. Native Windows-only cases remain explicitly listed.
This CPU preflight does not replace the other host's checks, launcher tests,
Linux foundation checks, Mac GPU contracts, private-XBE tests or gameplay
acceptance. Missing private game data keeps the existing explicit skips.

## Find failures in one CI run

The runtime plan always checks scope and guide maintenance. It selects affected
patch, native, GPU, release and launcher checks conservatively; unknown runtime
inputs still receive broad coverage. Documentation edits still get a completed
required gate without full game/launcher work.

Patch and native CPU suites each run in three independent jobs on Windows and
Mac, with two pytest workers per job and matrix fail-fast disabled. The shared
sharding plugin keeps module fixtures and explicit xdist groups together; only
the existing independently isolated migration/timing cases can split within a
file. Every original case belongs to exactly one shard. The local command can
reproduce a CI partition with `native --shard 2/3` (or `patches --shard 2/3`).
Omit `--shard` locally to execute the entire suite.

Mac GPU validation and release contracts have independent jobs that depend on
the plan, so CPU failures cannot skip them. Release contracts run on both
required desktop hosts. Existing launcher, Linux, issue and texture job scope
is retained. **PR validation** rejects failed, cancelled, missing or unexpectedly
skipped selected work, including the new independent jobs. No automatic test
retry, successful-result cache or weakened assertion is introduced.

Each CPU job groups repeated compiler/setup errors by diagnostic and uploads JUnit XML under
`test-results-*`, including setup/compilation errors. Timing reports remain
advisory, with separate profiles/cache/artifact identities for each shard and
host. Local reports live in `build/test-results`; timing history is in
`build/test-performance`. Existing unsharded timing baselines are not comparable
to the new partitions. See [performance history](release-performance.md).

The baseline Windows workflow at source `dfd6573122d9c9c967ceda558313ae07ccb4c613`
([run 42508588658](https://github.com/TheDoffman/timesplitters-future-perfect-pc/actions/runs/42508588658))
took about 17 minutes: Windows patch tests took 16 minutes and native CPU tests
11 minutes; Mac GPU work followed its CPU suite. Sharding and independent GPU
jobs shorten the critical path, but actual hosted speed depends on runner
availability and partition balance. Measure the first new CI runs before
claiming a wall-clock improvement; splitting jobs increases runner concurrency
and repeats per-job setup.

## Measured hosted result

[PR533's first full run](https://github.com/TheDoffman/timesplitters-future-perfect-pc/actions/runs/42525494549)
reached the required gate in **7m53s**, compared with **17m12s** for the
[matching main baseline](https://github.com/TheDoffman/timesplitters-future-perfect-pc/actions/runs/42508588658):
**54.2% less elapsed time**, including setup and runner queues. All22 selected
jobs passed on the first attempt. Per-host downloaded inventories confirmed no
missing/overlapping cases or newly non-passing baseline cases; Windows and Mac
each gained23 native scheduling/reporting regressions. Full measurements and
scope are recorded in [STATUS](../project/STATUS.md). This is an observed run,
not a guaranteed service time; runner availability and CPU/image differences vary.

## Merging and repository automation

If `main` advances, use **Update branch** or merge `main` into the PR branch,
then wait for checks on that candidate. Maintainers may enable auto-merge on an
individual PR. This change neither enables it globally nor merges any PR.
For a transient infrastructure failure, rerun the failed jobs on the same
candidate; code changes still require fresh validation of the new candidate.

Issue form/resolution tests run only when relevant files change. Their event
automation remains separate from PR validation. Issue labels run on issue
changes; issue form versions propose reference updates through a PR after
relevant release/main events; resolution notes run after merges using trusted
default-branch code. The known-issues discussion follows maintainer guidance.
No private game files, personal saves, new credentials or paid model calls are
needed for this CI workflow.

## Reading PR checks

Short check names identify the suite, platform and shard. `PR validation` remains
the required result. Its job summary groups the parallel shards into one row per
suite/platform, showing passed-job counts, elapsed execution span and failed-job
log links. Skipped checks appear in a collapsed section. This display reads the
current run attempt only; API/reporting errors cannot change acceptance.
