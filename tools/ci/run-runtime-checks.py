"""Run the same complete test lists locally and in CI; retain all failures."""
import argparse
import html
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tests/support'))
from runtime_shards import parse_shard

SUITES = ('patches', 'native', 'release')


def suite_paths(suite, system=None):
    manifest = json.loads(Path(__file__).with_name('runtime-suites.json').read_text())
    paths = list(manifest[suite])
    if suite == 'native' and (system or platform.system()) == 'Windows':
        paths.extend(manifest['native_windows'])
    if not paths or len(paths) != len(set(paths)):
        raise ValueError(f'Empty or duplicate paths in {suite}')
    for path in paths:
        if not path.startswith('tests/') or not (ROOT / path).is_file():
            raise ValueError(f'Missing test path: {path}')
    return paths


def failure_message(child):
    # A fixture's assertion often begins with a compiler banner. Prefer the
    # actual diagnostic so many dependent setup errors expose their one cause.
    text = (child.text or '') + '\n' + child.get('message', '')
    for line in text.splitlines():
        if re.search(r'error (?:C|LNK)\d+|fatal error|undefined reference|unresolved external|'
                     r'error:|PermissionError:|FileNotFoundError:', line):
            return re.sub(r'^E\s+', '', line.strip())[:300]
    return next(iter(child.get('message', '').splitlines()), child.tag)[:300]


def report(suite, result, path):
    lines = [f'### {suite}: ' + ('passed' if result == 0 else f'FAILED (exit {result})')]
    try:
        cases = list(ET.parse(path).getroot().iter('testcase'))
        failed = [(case, child) for case in cases for child in case
                  if child.tag in ('failure', 'error')]
        skipped = sum(case.find('skipped') is not None for case in cases)
        lines.append(f'{len(cases)} cases; {len(failed)} failures/errors; {skipped} skipped.')
        groups = {}
        for case, child in failed:
            name = case.get('classname', '') + '::' + case.get('name', '')
            groups.setdefault(failure_message(child), []).append(name)
        for message, names in list(groups.items())[:50]:
            lines.append(f'- {len(names)} failure(s): <code>{html.escape(message)}</code>')
            lines.append('  Affected: ' + ', '.join('<code>' + html.escape(name) + '</code>'
                                                   for name in names[:4])
                         + (f' (and {len(names)-4} more)' if len(names) > 4 else ''))
        if len(groups) > 50:
            lines.append('Further failures are in the attached JUnit XML and job log.')
    except (OSError, ET.ParseError):
        lines.append('No readable test report; inspect setup/collection output in the job log.')
    text = '\n'.join(lines) + '\n\n'
    print(text, flush=True)
    summary = os.environ.get('GITHUB_STEP_SUMMARY')
    if summary:
        with open(summary, 'a', encoding='utf-8') as stream:
            stream.write(text)


def run_suite(suite, args):
    paths = suite_paths(suite)
    suffix = '-s' + args.shard.replace('/', '-of-') if args.shard else ''
    label = suite + suffix
    output = ROOT / 'build/test-results' / (label + '.xml')
    output.parent.mkdir(parents=True, exist_ok=True)
    output.unlink(missing_ok=True)  # Never report an earlier run as this result.
    command = [sys.executable, '-m', 'pytest', *paths, '-q', '--tb=short',
               '--durations=20', '--junitxml=' + str(output),
               '-o', 'cache_dir=' + str(ROOT / 'build/test-cache' / label),
               '--suite-profile=' + args.profile_prefix + '-' + label]
    if args.workers > 1:
        command += ['-n', str(args.workers), '--dist=loadgroup', '--max-worker-restart=0']
    if args.shard:
        command += ['--runtime-shard=' + args.shard]
    if args.failed_first:
        command += ['--failed-first']  # Reorders failures; never omits other cases.
    if args.collect_only:
        command += ['--collect-only']
    print('Running ' + label + ': ' + str(len(paths)) + ' test files', flush=True)
    result = subprocess.run(command, cwd=ROOT).returncode
    if args.collect_only:
        print(f'{label}: collection only (exit {result}); no tests executed.', flush=True)
    else:
        report(label, result, output)
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('suite', choices=(*SUITES, 'all'))
    parser.add_argument('--shard', help='CI partition, INDEX/TOTAL; omit locally to run every case.')
    parser.add_argument('--workers', type=int, default=2)
    parser.add_argument('--profile-prefix', default='local', help='Separate local and CI timing history.')
    parser.add_argument('--prepare', action='store_true', help='Prepare/apply the bundled runtime before tests.')
    parser.add_argument('--failed-first', action='store_true', help='Run previous failures first, then all other cases.')
    parser.add_argument('--collect-only', action='store_true')
    parser.add_argument('--list', action='store_true', help='Print test file lists without preparing or executing.')
    args = parser.parse_args(argv)
    if args.workers < 1:
        parser.error('--workers must be positive')
    if not re.fullmatch(r'[a-z0-9][a-z0-9-]{0,15}', args.profile_prefix):
        parser.error('--profile-prefix must be 1..16 lowercase letters, digits or hyphens')
    if args.shard:
        try:
            parse_shard(args.shard)
        except ValueError as error:
            parser.error(str(error))
        if args.suite in ('all', 'release'):
            parser.error('--shard is supported only for patches or native')
    suites = SUITES if args.suite == 'all' else (args.suite,)
    if args.list:
        for suite in suites:
            print(suite + ':\n' + '\n'.join(suite_paths(suite)))
        return 0
    prepared = True
    if args.prepare and any(suite != 'release' for suite in suites):
        prepared = subprocess.run([sys.executable, 'tools/build/prepare-runtime.py'], cwd=ROOT).returncode == 0
        if prepared and 'native' in suites:
            prepared = subprocess.run([sys.executable, 'tools/build/apply-patches.py',
                                       'external/ts3FPrecomp', 'patches'], cwd=ROOT).returncode == 0
    results = []
    for suite in suites:
        if not prepared and suite != 'release':
            print(f'{suite}: runtime preparation failed; tests cannot run.', flush=True)
            results.append(1)
            continue
        results.append(run_suite(suite, args))
    # Independent suites still execute after an earlier failure. No automatic
    # retry, last-failed selection, or successful result reuse can turn it green.
    return 1 if any(results) else 0


if __name__ == '__main__':
    raise SystemExit(main())
