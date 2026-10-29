"""Compare four release-verifier calls against a local Git revision, read-only.

Uses an existing release ZIP and BUILD.json sidecar; does not build, extract,
upload, tag or change a release. Run only with a trusted local source revision.
"""
import argparse
from contextlib import redirect_stdout
import io
import json
from pathlib import Path
import statistics
import subprocess
import time
import types

ROOT = Path(__file__).resolve().parents[2]
SOURCE = 'tools/release/publish-release.py'


def load_source(name, source):
    module = types.ModuleType(name)
    module.__file__ = str(ROOT/SOURCE)
    exec(compile(source, module.__file__, 'exec'), module.__dict__)
    return module


def measure(module, archive, metadata, calls):
    build = json.loads(metadata.read_text())
    publisher = module.Publisher(ROOT)
    publisher.archive = archive
    publisher.checksum = archive.with_suffix('.zip.sha256')
    publisher.metadata = metadata
    # A nonexistent path selects sidecar metadata; no extraction or staging copy.
    publisher.stage = ROOT/'build/verification-benchmark-no-stage'
    if publisher.stage.exists():
        raise ValueError('Benchmark stage path must not exist')
    publisher.name = archive.stem
    publisher.commit = build['source_commit']
    publisher.tag = build['version']
    publisher.texture_identity = build.get('textures')
    publisher.texture_files = {name: sha for name, sha in build['files'].items()
                               if name.startswith('Textures/')}
    timings = []
    with redirect_stdout(io.StringIO()):
        for _ in range(calls):
            start = time.perf_counter()
            publisher.verify_package()
            timings.append(time.perf_counter()-start)
    return timings


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', required=True, help='Trusted local Git revision before optimization')
    parser.add_argument('--archive', type=Path, required=True)
    parser.add_argument('--rounds', type=int, default=3)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if args.rounds < 1:
        parser.error('--rounds must be positive')
    revision = subprocess.check_output(['git', 'rev-parse', '--verify', '--end-of-options',
                                       args.baseline+'^{commit}'], cwd=ROOT, text=True).strip()
    before = load_source('baseline_publisher', subprocess.check_output(
        ['git', 'show', revision+':'+SOURCE], cwd=ROOT, text=True, encoding='utf-8'))
    after = load_source('current_publisher', (ROOT/SOURCE).read_text(encoding='utf-8'))
    archive = args.archive.resolve()
    metadata = archive.with_suffix('.BUILD.json')
    samples = {'before': [], 'after': []}
    for index in range(args.rounds):
        # Alternate ordering to reduce systematic filesystem-cache bias.
        order = [('before', before), ('after', after)]
        for label, module in order if index % 2 == 0 else reversed(order):
            samples[label].append(measure(module, archive, metadata, 4))
    medians = {label: statistics.median(sum(run) for run in runs)
               for label, runs in samples.items()}
    report = dict(scope='Four verifier calls; existing ZIP without staging, warm filesystem, no build or upload',
                  baseline=revision, archive=archive.name, bytes=archive.stat().st_size,
                  samples_seconds=samples, median_seconds=medians,
                  seconds_saved=medians['before']-medians['after'],
                  percent_faster=100*(1-medians['after']/medians['before']))
    output = json.dumps(report, indent=2)+'\n'
    if args.output:
        args.output.write_text(output, encoding='utf-8')
    print(output, end='')


if __name__ == '__main__':
    main()
