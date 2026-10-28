# Build and release validation reuse

`tools/build/apply-patches.py` still validates ledger contents, target paths and
the complete ordered patch stack. After a successful full unwind/apply,
it writes `tsfp-patches-validated.sha256` beside the runtime Git ledger. Reuse
requires identical patch records, ledger bytes, touched source bytes, validator
bytes and Git version. Changes, missing/corrupt receipts and `--revalidate` run
the full transaction. Failure never earns a receipt. Source and ledger are
checked again before returning from reuse; unrelated runtime files are outside
patch validation, as before. This helper is shared by Windows and Mac.

`tools/build/build-launcher.ps1 -Test` caches successful Windows launcher builds
and regressions in `.validated-build.json` under its output directory. The key
covers launcher sources/resources, C# tests, ImportGame, controls/display defaults,
selected artwork, build/cache scripts, compiler/framework files, options and host
identity. Every declared output is rehashed. Missing/changed files invalidate;
untested or failed builds cannot create the receipt. An exclusive output lock
prevents simultaneous builders. `-Rebuild` forces compilation and regressions.
Artwork validation precedes reuse. Actual package installation is always tested
separately on the newly built game ZIP. Cache files are never player assets.

`publish-release.py` delegates patch validation to Build instead of calling it
twice. Its verifier checks paths, file inventory, source/version/channel, stage
hashes, archive checksum and each archived SHA-256. Reading each member to EOF
also checks CRC, eliminating a duplicate `testzip()` pass. One Publisher instance
reuses only successful decompression for an identical archive hash, metadata and
root name. All bytes are still rehashed at each boundary. A fresh process deep
checks once; no persistent receipt can bypass archive inspection.

The full release pytest suite, native build tests, real installer acceptance,
portable Online checks, clean/pushed source policy and upload checks remain.
Prepared-release reuse still requires the exact source/version/channel and asset
hashes. There is no broad test-result cache across different game candidates.

Release stages print timings and save `build/releases/last-run-timings.json`,
including failures. The suite prints its 20 slowest checks. Total includes nested
stages; do not add it to their durations. `benchmark-verification.py` compares four
verifier calls against trusted local Git source on an existing ZIP, without
extracting or publishing. `benchmark-patches.py` compares full and cached no-op
validation in a disposable snapshot of touched source files, preserving runtime
and game data. See [measurements](../development/release-performance.md).

Regression routes: `test_publish_release.py`, `test_launcher_cache.py`,
`test_patch_tracking.py`, packaging/launcher compatibility/release tests. Cover
same-size/mtime tampering, failed validation, metadata changes, CRC corruption,
receipt corruption, added inputs, forced revalidation and branch migrations.
Launcher CI runs the receipt tests on Windows; shared patch tests run on both
hosts. Windows results do not establish physical Mac execution.

## Full-suite scheduling

`Publisher(test_workers=None)` uses up to four CPU workers for the complete
`pytest tests` collection. `--test-workers 1` / `Publish-Release.ps1 -TestWorkers 1`
runs serially; no test-selection or last-failed filter is used. Pinned pytest-xdist
and execnet are installed from shared requirements. Native and patch CI use two
workers on both hosts; their existing required gates remain unchanged.

`tests/support/parallel_groups.py`, imported by `tests/conftest.py`, assigns
`loadgroup` markers before xdist adds scheduling suffixes. Most files stay on one
worker to reuse compiled fixtures. Patch migration, native timing and vehicle
timing cases use isolated temporary directories and can run separately. Explicit
groups are honored. Changes to this policy select all runtime CI suites.
Worker crashes are failures (`--max-worker-restart=0`); release packaging follows
only a successful entire test command. Runtime compilation and test results are
not cached. All original cases, mutation controls, timeouts and assertions remain.

`native_compiler.py` reuses successful Windows environment discovery inside one
process, keyed by incoming environment and working directory. It ignores only
pytest's diagnostic current-test label, which is refreshed on each returned copy.
The bounded eight-entry cache rejects missing compilers, does not retain failures
and never shares mutable dictionaries with callers. POSIX compiler discovery is
unchanged. This removes repeated vswhere/vcvars setup, not native compilation.

`clean_pytest_current_links` accepts pytest sibling aliases directly under the
release test root or a real `popen-gw[0-9]+` worker directory. It validates every
link before unlinking any and rejects redirected worker roots, cross-worker,
external and nested links. Targets remain untouched. Regression tests exercise
actual two-worker session-fixture cleanup and release failure propagation.

Full-suite measurement exposed older XBE fixture assumptions: cutscene/projection
constants were found by matching obsolete emitted C. `guest_constants.py` now
reads absolute data operands from the verified original instructions, including
SIMD widths; projection mutation targets the current final-store lowering.
Numerical output, bounds, stack and missing-write checks are preserved. This does
not change the translator or game. Historical baseline failures and the separate
shader correction are recorded in the performance measurements.

## Advisory performance history

`tools/testing/suite_timing.py` is loaded by the shared test conftest but records
only explicitly profiled runs. The publisher passes `--suite-profile=release-full`;
patch/native/release CI use distinct `ci-*` profiles. Platform, architecture,
processor, CPU count, host/runner identity, Python/pytest/xdist versions, worker
count and scheduler partition history. CI hosted runners use the image family
instead of disposable hostnames. No fixture or result reuse is added.

Each profile/environment retains a fixed median of its first three successful,
complete runs with matching test IDs and outcomes. Failed, interrupted, filtered,
collection-only and incomplete runs cannot train it. Existing skips are visible;
missing tests and newly skipped/non-passing tests warn immediately and cannot
improve the timing comparison. Successful added cases extend the coverage guard
without raising the time budget. Three consecutive eligible runs over 125% of
the fixed median warn; a good/ineligible run resets that streak. Review legitimate
growth before explicitly choosing a new versioned profile to recalibrate.

`build/test-performance/<profile>/<environment-key>/state.json` retains 60 run
summaries, calibration samples until established, and the coverage guard.
`last-run.json` retains complete case outcomes, per-run counts, subtest counts,
revision/dirty provenance, the 20 slowest tests and coverage differences. Durations
measure the pytest session, including setup/collection/fixture teardown, but exclude
interpreter startup and history writing. An exclusive write lock and atomic JSON
replacement protect concurrent writers. Corrupt state or an occupied/stale lock
produces an advisory warning without overwriting it or altering pytest's result.
Inspect the recorded directory before repairing a stale lock; never remove one
while another process is writing.

CI restores/saves timing-only history with unique immutable cache keys scoped by
OS, architecture and job; GitHub branch cache isolation still applies. JSON reports
are uploaded for 30 days, including failed runs, and warnings appear in the log
and as annotations. Cache/artifact outages are advisory. Cold/evicted cache history
needs calibration again; concurrent CI runs can restore the same earlier snapshot,
so persistent local release history is the primary per-machine comparison.
Native Mac CI and local release measurements never borrow Windows timing targets.

`test_suite_timing.py` covers calibration, fixed budgets, coverage guards, additions,
failed/partial sessions, host/worker separation, corrupt history, locks, teardown
failure, subtests, and real serial/two-worker pytest invocations. Existing grouping,
compiler-cache and release-failure tests remain. Monitoring changes select every
runtime CI suite. No performance warning bypasses a failing acceptance gate or
aborts an otherwise passing release.
