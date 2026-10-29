# Build and release performance

The normal build and release commands now avoid repeated work while retaining
release acceptance checks. No extra speed flag is needed.

## Keep future runs measurable

The release publisher automatically records its full pytest run under
`build/test-performance/release-full/`. A profile is separated by host/platform,
hardware, Python/test-tool versions, worker count and scheduling mode. Windows
and Mac do not share a time target. Patch/native/release CI each use their own
profile and persist timing history through GitHub's branch-scoped cache; reports
are retained as artifacts for 30 days. CI caches contain measurements, not test
results that can substitute for execution.

The first three successful complete runs with identical test IDs and outcomes
establish a fixed median. Three consecutive comparable successful runs more than
25% slower produce a warning with the baseline and the slowest tests. The monitor
never silently raises that baseline. A single slow run remains visible in history
without triggering a sustained-regression warning. This is advisory; existing
test failures still block release, and a timing warning alone does not.

Missing tests and new skips/non-passing outcomes warn immediately. Failed,
filtered, interrupted or incomplete runs cannot train the baseline or earn a
better speed comparison. Added passing tests join the coverage guard while the
time budget remains fixed. Review intentional test growth before adopting a new
versioned profile; retain the old history as evidence. Monitoring cannot detect
every weakened assertion, so existing mutation controls and code review remain.
For publisher/CI recalibration, change the profile name in the tracked command
after reviewing the growth; a manual diagnostic profile does not reset theirs.

For a measured manual full run, use the same four-worker policy:

```powershell
.\.venv\Scripts\python.exe -m pytest tests -q -n 4 --dist=loadgroup --max-worker-restart=0 --suite-profile=release-full --durations=20 -p no:cacheprovider -o tmp_path_retention_policy=failed
```

Each environment folder contains `state.json` (baseline and latest 60 summaries)
and `last-run.json` (full outcomes, coverage differences, revision, counts and 20
slowest tests). Times cover the pytest session; interpreter startup and report
writing are outside that measurement. Missing/corrupt history, occupied locks or
CI cache eviction are reported and may require calibration again. Inspect history
before any deliberate reset; changing the profile name preserves older evidence.
No wall-time guarantee is made under changing machine load or expanding coverage.

## What is reused

- Patch validation: identical patch records, runtime source bytes, ledger,
  validator and Git version reuse the completed validation. Changed inputs use
  the full unwind/apply transaction. This applies to Windows and Mac builds.
- Windows launcher: `build-launcher.ps1 -Test` reuses successful compilation and
  regressions only when input and output hashes, options and host identity match.
  Artwork, compiler/framework, resources and compiled test sources are included.
- Package verification: each archive member is decompressed once per publisher
  process, checking CRC and SHA-256 together. Later checks always rehash the ZIP
  and staged files, and repeat decompression if either archive or metadata changes.
- Release orchestration: Build owns patch validation; the publisher no longer
  repeats it immediately before calling Build.

The full release test suite still runs for each new candidate. A prepared
candidate retains the existing exact-source/version/channel/asset reuse policy.
Native tests, installer acceptance of the actual game ZIP, portable Online,
private-file exclusion, source identity and upload checks remain in place.

To force fresh checks:

```powershell
.\tools\build\build-launcher.ps1 -Test -RequireArtwork -Rebuild
.\.venv\Scripts\python.exe tools/build/apply-patches.py external/ts3FPrecomp patches --revalidate
```

The shared Python patch command is the same on Mac with `.venv/bin/python`.
The Mac launcher builder retains its existing behavior; its compilation cache
has not been changed.

## Measurement method

Windows patch/archive measurements compare trusted local source revision
`2e678ac1fc82e94a0c5cdd4adc41af15591e4d36` with this change. Archive comparisons
alternate old/new order for three rounds using the same existing 89,979,629-byte
v0.3.3 release ZIP. Four verifier calls model fresh prepare and publish, without
a staging tree. No archive is extracted or uploaded. Filesystem cache is warm.

Patch measurements use the current local applied stack (212 patches, 127 touched
files, 3,299,958 source bytes), including ongoing audio work, in a disposable
source-only snapshot. The original runtime and game data remain untouched. Three
full baseline runs are compared with three warm receipt runs; cold validation
is measured separately. This is validation cost, not game compilation time.

Launcher measurements compare a forced full build and all launcher regressions
with reuse of the exact resulting artifacts, with required artwork supplied.

Measured on 2026-10-28:

| Stage | Before / forced fresh | After successful reuse | Reduction |
| --- | ---: | ---: | ---: |
| Patch stack validation (median, three paired rounds) | 18.361 s | 0.241 s | 98.7% |
| Launcher build and regressions (fresh run vs median of three reuse runs) | 35.51 s | 0.32 s | 99.1% |
| Four archive verifier calls (median, three alternating rounds) | 6.175 s | 1.000 s | 83.8% |

The new **cold** patch-validation median was 18.410 s, essentially unchanged
from the old 18.361 s. Baseline patch samples were 18.361/18.372/18.239 s;
new cold samples 17.807/19.281/18.410 s; reuse 0.241/0.248/0.238 s. Earlier
unpaired runs varied considerably, so the table uses the final paired samples.
Launcher reuse samples were 0.32/0.31/0.32 s. Archive totals were
6.369/6.175/6.112 s before and 1.008/1.000/0.937 s after.

The publisher also removes one entire redundant patch-validation call; this
benefit is separate from the per-call row above. These stage measurements must
not be interpreted as a percentage improvement for an entire game release.

Windows validation passed:

- 268 focused Python tests plus 40 subtests: release orchestration, receipt
  invalidation, packaging, compatibility/CI selection, backlog integrity and
  patch transaction/rollback behavior (24.54 s).
- Four real-stack tests: fresh/repeated builds, texture-branch switches with LF
  and CRLF, and conflict preservation (208.59 s). The existing fixture read
  pristine files with `git archive` from the active runtime, then performed all
  changes in disposable test repositories using this branch's patches/installer.
- Actual Windows launcher compilation and all native launcher regression groups.
- Repository context/ownership, generated patch index and whitespace checks.

The first optimization pass did not run a full game compilation, new game
package, network upload, broad game-release suite or native Mac execution. The benchmark uses an existing
release and source-only snapshots; it does not establish fresh release acceptance.
Raw local evidence is under `logs/patch-benchmark-paired.json`,
`logs/verification-benchmark.json`, `logs/launcher-cold-final.log` and
`logs/launcher-warm-final-*.log`. These logs and all binaries remain ignored.

Reproduce the read-only/snapshot benchmarks using a trusted local baseline:

```powershell
.\.venv\Scripts\python.exe tools/release/benchmark-verification.py --baseline 2e678ac1fc82e94a0c5cdd4adc41af15591e4d36 --archive build/releases/TimeSplitters-Future-Perfect-PC-v0.3.3-windows-x64.zip
.\.venv\Scripts\python.exe tools/build/benchmark-patches.py --baseline 2e678ac1fc82e94a0c5cdd4adc41af15591e4d36 --runtime external/ts3FPrecomp
```

Archive verification needs the matching `.BUILD.json` and `.zip.sha256` sidecars.
Patch measurements require an already applied stack; `--patches` selects its
source directory. Both tools accept `--output` for a JSON measurement report.

## Future release timings

`Publish-Release.ps1` prints per-stage durations and saves the latest run in
`build/releases/last-run-timings.json`, even if a stage fails. The test suite
reports its 20 slowest checks. Total includes the individual stages; do not add
it again. The launcher prints fresh/reused status and its own elapsed time.

Those stage measurements do not establish an end-to-end release speedup. The
full-suite follow-up below measures regression time separately; game compilation
and network upload remain separate costs. No game was published for this work.


## Full regression suite (2026-10-28)

Implementation and measured source: local commit `e3a50ed7` on `perf/release-speed`.

The current suite saved **32 min 35 s per complete run
(77.0% less elapsed time, 4.35x speedup)** on this Windows machine.
This is the current 2,423-case baseline, larger than the historical 21-minute
suite; it is not a measurement of a complete successful game release.

| Measurement | Serial baseline | Optimized, four workers |
| --- | ---: | ---: |
| Wall time | 42 min 20 s (2539.763 s) | 9 min 44 s (584.488 s) |
| Collected cases, all executed | 2423 | 2457 |
| Passed | 2360 | 2397 |
| Skipped (same cases) | 36 | 36 |
| Failures | 27 | 24 |

Every original test ID was executed again. No original case was deleted, no
passed case became a failure, and no skip outcome changed. The optimized run
adds 34 passing regression cases. Three previously failing original-image
fixtures now pass after repairing their constant extraction and mutation target;
their numerical, memory-bound, stack and missing-write assertions remain.
All existing mutation controls, timeouts and release acceptance gates are kept.

At that measured revision, the remaining 24 failures were the same baseline cases in
`tests/graphics/test_surface_shader_feedback.py`. The translated original upload
routine emits 27/166/250 while the shared coefficient helper truncates to
26/165/249. Determining the correct rounding/state contract requires a separate
correctness investigation. The performance change did not modify translation or
rendering to make those checks pass. Those failures blocked release packaging;
the shader follow-up below records their resolution.

The speed changes are:

- Reuse successful MSVC environment discovery within each worker, with environment
  and working-directory invalidation, fresh returned dictionaries and no reuse of
  failures. Native fixtures are still compiled and executed.
- Run independent work concurrently. Keep most files together to share compiled
  fixtures; split only audited patch-migration, native-timing and vehicle-timing
  cases that own independent temporary directories.
- Use up to four workers in the release publisher and two in Windows/Mac CI.
  Worker crashes fail the command, and safe cleanup recognizes worker directories.
  Use `Publish-Release.ps1 -TestWorkers 1` for serial diagnostics.

### Method and limits

Baseline source: `9a32375f58848b9d1f04f6acbd662b471e9a6f34`, containing the earlier
patch/launcher/archive optimizations. Host: Intel Core i7-13700K, 24 logical CPUs,
32 GB RAM, Windows, Python 3.12.14 and pytest 9.1.1. The optimized runner uses
pytest-xdist 3.8.0 and execnet 2.1.2. Both runs used the same pinned patched runtime,
supported US XBE, two intro movies, SDL/FFmpeg headers and decoder DLLs. No personal
saves or settings were used. The original checkout and its environment were not
modified; added test tools were installed only in this worktree's ignored build
directory.

This is one local before/after observation without flushing filesystem caches,
not a multi-run confidence interval. The serial baseline initially stopped after five
existing failures at 1,378.115 s; the remaining 771 cases then ran serially in
1,161.648 s. The table sums both measured invocations, including their collection
overhead. Finished cases were not rerun in that continuation. The optimized
measurement is one complete invocation, including startup and collection, with
all 2,423 original IDs plus the added checks. No timing is inferred from the
historical 21-minute result. The baseline failures mean this is a validation-cost
comparison, not successful release acceptance. Native Mac execution, CI duration,
fresh game compilation, packaging and upload time are unmeasured.
All 246 input-file hashes still match the baseline manifest; both runs also
passed the same 109 subtests.

Focused validation passed 262 cases in 12.86 s, including real two-worker temporary
fixture cleanup, failure propagation, cache invalidation, scheduling policy and
the repaired graphics fixtures. The changed workflows parse, PowerShell syntax
checks pass, and repository context/ownership and whitespace checks pass.
Local evidence is retained in `logs/suite-before.*`, `logs/suite-before-tail.*`,
`logs/suite-after.*`, `logs/suite-comparison.json`, `logs/suite-focused.*` and
`logs/suite-inputs.json`. Private inputs, generated C, binaries and logs stay ignored.

For a direct full run after installing the shared requirements:

```powershell
.\.venv\Scripts\python.exe -m pytest tests -q -n 4 --dist=loadgroup --max-worker-restart=0 -p no:cacheprovider -o tmp_path_retention_policy=failed --durations=20
```

Omit the three worker options for a serial diagnostic run. The publisher enables
the parallel policy automatically; a bare manual pytest invocation remains serial.

## Shader correctness follow-up (2026-10-28)

Local commit `08ccb2ed` on `fix/shader-coefficient-regressions` depends on the unmerged
performance work through `d1226d25`. Disassembly of the hash-verified supported US
XBE confirms four rounded `CVTSS2SI` instructions in the uploader at `3D9520`.
The current translator correctly uses `nearbyint`; three shared water coefficient
paths still truncated through C casts. One common helper now matches the clamp,
binary32 multiplication and host-mode rounding used by that translated uploader.
Nearest/even produces bytes 27/166/250 instead of 26/165/249. This does not claim
independent Xbox MXCSR emulation; see [the contract](../agent-context/surface-coefficients.md).

The complete optimized Windows rerun collected 2,460 cases: **2,424 passed,
36 skipped, zero failed**, plus **109 passing subtests**, in **468.403 s
(7 min 48 s)** including startup and collection. Comparison by stable test ID
confirms all 2,457 previous cases were retained, every one of the 24 shader
failures passed, no formerly passing case regressed, and no skip changed. Three
new passing cases exercise independent mathematical/native SSE references and
require truncation and ties-away mutations to fail. Both-host CI includes them.
All 246 private/runtime input hashes still match the original manifest.

The first full attempt took 799.766 s with 2,423 passing cases, the same 36 skips,
and one unrelated runtime-snapshot failure: Windows denied the atomic temporary
directory rename, then denied cleanup of a read-only Git object. The same check
passed in isolation and in the complete unchanged rerun. The failed evidence is
retained; no test retries, exclusions or importer changes were used to obtain
the clean run. Other independent test/game processes were active during the first
attempt; the cause of the access denial is not established. These variable host
conditions mean the faster final time is not an additional measured speedup
attributable to the coefficient fix.

Focused WARP, combiner, rounding and native-surface validation passed 45 cases
with five existing prerequisite skips in 116.64 s. The strengthened native adapter
case also passed: it checks the actual GPU configuration, non-finite rejection,
resource failure and reset behavior. Full patch revalidation passed without
changing runtime files. Original GPU thresholds, shader mutation controls,
memory/stack checks, timeouts and release acceptance gates remain in place.
Workflow YAML, repository context/ownership, backlog and whitespace checks pass.

Evidence remains local in `logs/shader-focused.*`, `logs/shader-adapter.log`,
`logs/shader-patch-validation.log`, `logs/suite-shader-fixed.*`,
`logs/suite-shader-final.*` and `logs/shader-suite-comparison.json`. No private
assets or generated code are committed. Native Mac GPU/gameplay, complete scene
parity, a fresh game build, packaging and publication remain unverified.

## Monitor calibration (2026-10-28)

Implementation: local commit `c43b3139` on `perf/suite-regression-monitor`, based
on the unmerged performance/shader fixes through `d71137ac`. The source remained
unchanged throughout calibration. The release profile ran the entire suite with
four workers, `loadgroup`, zero worker restarts and no selection filters.

| Successful sample | Full invocation | Monitor session | Results |
| --- | ---: | ---: | --- |
| 1 | 559.164 s (9m19s) | 558.887 s | 2447 passed, 36 skipped, 109 passing subtests |
| 2 | 494.623 s (8m14s) | 494.439 s | Same |
| 3 | 474.894 s (7m54s) | 474.727 s | Same |

The automatically learned fixed median is **494.439 s (8m14s)**. A sustained
warning requires three consecutive eligible runs above **618.049 s (10m18s)**.
These observations support a local comparison, not a guarantee under changing
machine load. The invocation profiler starts before `pytest.main`; the monitor
starts during pytest configuration. Both include collection and test-fixture
setup/teardown. Neither measures a complete game release. No additional speedup
is attributed to the monitor.

Host: Windows AMD64, Intel i7-13700K, 24 logical CPUs, Python 3.12.14,
pytest 9.1.1 and pytest-xdist 3.8.0. The stable test-ID comparison retained every
previous 2,460 ID with identical outcomes, including the same 36 prerequisite
skips, and added 23 passing monitor/CI-selection cases. All 246 baseline input
hashes still match. Existing assertion thresholds, mutation controls, timeouts,
compiler-cache protections and release failure propagation are unchanged.

An earlier full attempt took 773.794 s and recorded 2,446 passes, 36 skips,
109 passing subtests and one failure: `native_vehicle_actor_impact`'s
`open-gate-only` mutation exceeded its existing 15-second timeout. The monitor
retained the complete failed record and did not create a calibration sample.
The exact case passed in isolation (7.45 s for the complete selected test) and
in all three unchanged full reruns. No timeout, exclusion, assertion or retry
plugin was changed. The cause of the one-off timeout is not established.

Focused tests passed 248 cases in 5.75 s, covering calibration and warning
boundaries, immutable baselines, missing/newly skipped cases, added-case guards,
history retention, platform/worker separation, locks/corruption, subtests,
teardown failures, real serial/parallel/filter/failure runs and CI/release wiring.
A simulated sustained slowdown prints a warning while pytest still exits zero;
a real failing fixture still exits nonzero. Context/backlog, workflow YAML and
whitespace checks pass. Actual hosted CI history/cache/artifact execution and
native Mac execution are not verified locally.

Evidence: `logs/suite-monitor-{1,2,3,4}.*`, `logs/monitor-vehicle-diagnosis.log`,
`logs/suite-monitor-focused-r2.log`, `logs/suite-monitor-comparison.json` and
the local `build/test-performance/release-full/` history. Only code, tests and
documentation are committed. Other checkouts or machines need their own history;
the CI workflows learn independently after integration.
