# Build and release performance

The normal build and release commands now avoid repeated work while retaining
release acceptance checks. No extra speed flag is needed.

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

No full game compilation, new game package, network upload, broad game-release
suite or native Mac execution was performed. The benchmark uses an existing
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

These measurements do not establish an end-to-end release speedup or reduce the
roughly 21-minute historical full-suite result. Compilation, new-candidate tests
and network upload remain separate costs. No game was published for this work.
