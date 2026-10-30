"""Time patch validation in a disposable source-only snapshot; never edits runtime."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import statistics
import subprocess
import tempfile
import time
import types

ROOT = Path(__file__).resolve().parents[2]
SOURCE = 'tools/build/apply-patches.py'


def load_source(name, source):
    module = types.ModuleType(name)
    module.__file__ = str(ROOT/SOURCE)
    exec(compile(source, module.__file__, 'exec'), module.__dict__)
    return module


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--patches', type=Path, default=ROOT/'patches')
    parser.add_argument('--baseline', required=True, help='Trusted local Git revision')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    revision = subprocess.check_output(['git', 'rev-parse', '--verify', '--end-of-options',
                                       args.baseline+'^{commit}'], cwd=ROOT, text=True).strip()
    before = load_source('before_patches', subprocess.check_output(
        ['git', 'show', revision+':'+SOURCE], cwd=ROOT, text=True, encoding='utf-8'))
    after = load_source('after_patches', (ROOT/SOURCE).read_text(encoding='utf-8'))
    runtime = args.runtime.resolve()
    ledger_path = after.state_path(runtime)
    ledger, previous = after.read_state(ledger_path)
    patches = after.patch_records(after.active_patch_paths(args.patches))
    if previous != patches:
        raise ValueError('Benchmark requires an already applied matching patch stack')
    names = {line[6:] for patch in patches for line in patch['content'].splitlines()
             if line.startswith('+++ b/')}
    originals = {}
    for name in names:
        path = runtime/name
        if not path.resolve().is_relative_to(runtime) or not path.is_file():
            raise ValueError('Invalid runtime source path')
        originals[name] = path.read_bytes()
    (ROOT/'build').mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='patch-benchmark-', dir=ROOT/'build') as directory:
        snapshot = Path(directory)
        subprocess.run(['git', 'init', '-q', str(snapshot)], check=True)
        after.state_path(snapshot).write_bytes(ledger)
        for name, content in originals.items():
            target = snapshot/name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(content)
        def measure(module, **options):
            start = time.perf_counter()
            changed = module.apply_patches(snapshot, args.patches, **options)
            if changed:
                raise ValueError('Snapshot was not a no-op; benchmark is invalid')
            return time.perf_counter()-start
        old, cold, warm = [], [], []
        for index in range(3):
            # Alternate full-validation order to reduce background-load bias.
            if index % 2 == 0:
                old.append(measure(before))
                cold.append(measure(after, force=True))
            else:
                cold.append(measure(after, force=True))
                old.append(measure(before))
            warm.append(measure(after))
        if any((snapshot/name).read_bytes() != content for name, content in originals.items()):
            raise ValueError('Validation changed snapshot bytes')
    if (ledger_path.read_bytes() != ledger or
            any((runtime/name).read_bytes() != content for name, content in originals.items()) or
            after.patch_records(after.active_patch_paths(args.patches)) != patches):
        raise ValueError('Source changed during measurement; discard timings and retry')
    old_median, new_median = statistics.median(old), statistics.median(warm)
    report = dict(scope='No-op patch validation in disposable source-only snapshot; original runtime untouched',
                  baseline=revision, patches=len(patches), files=len(names),
                  source_bytes=sum(map(len, originals.values())), ledger_sha256=hashlib.sha256(ledger).hexdigest(),
                  before_seconds=old, cold_after_seconds=cold, warm_after_seconds=warm,
                  median_cold_seconds=statistics.median(cold),
                  median_before_seconds=old_median, median_warm_seconds=new_median,
                  seconds_saved=old_median-new_median, percent_faster=100*(1-new_median/old_median))
    output = json.dumps(report, indent=2)+'\n'
    if args.output:
        args.output.write_text(output, encoding='utf-8')
    print(output, end='')


if __name__ == '__main__':
    main()
