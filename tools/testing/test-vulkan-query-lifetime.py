#!/usr/bin/env python3
"""Observe valid query-pool retirement after timeline waits, without game assets.

This is a driver diagnostic, not proof that a historical game crash is fixed.
All output is new-only; --record-only records failures without failing the job.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shlex
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
if __name__ == '__main__':
    sys.dont_write_bytecode = True
sys.path.insert(0, str(ROOT / 'tools/build'))
import vulkan_sdk


def plain(path, directory=False):
    path = Path(path).absolute()
    for member in (path, *path.parents):
        if member.is_symlink() or getattr(member, 'is_junction', lambda: False)():
            raise ValueError('Aliased diagnostic path refused')
    if directory and not path.is_dir():
        raise ValueError('Diagnostic parent is unavailable')
    return path


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def text_tail(value):
    if isinstance(value, bytes):
        value = value.decode('utf-8', errors='replace')
    return (value or '')[-8192:]


def observe(executable, environment, iterations, *, seconds=45):
    try:
        result = subprocess.run([str(executable), str(iterations)], env=environment, stdin=subprocess.DEVNULL,
                                 capture_output=True, text=True, timeout=seconds)
    except subprocess.TimeoutExpired as error:
        return dict(status='timeout', returncode=None, stdout_tail=text_tail(error.stdout), stderr_tail=text_tail(error.stderr))
    record = dict(status='failed', returncode=result.returncode,
                  stdout_tail=text_tail(result.stdout), stderr_tail=text_tail(result.stderr))
    if result.returncode:
        record['status'] = 'unsupported' if result.returncode == 77 else 'crashed' if result.returncode < 0 else 'failed'
        return record
    try:
        value = json.loads(result.stdout)
        if not (isinstance(value, dict) and type(value.get('schema')) is int and value['schema'] == 1
                and value.get('phase') == 'query-lifetime-probe' and type(value.get('completed')) is int
                and type(value.get('iterations')) is int and value['completed'] == value['iterations'] == iterations
                and value.get('query_type') == 'occlusion-reset' and value.get('timeline_wait') is True
                and isinstance(value.get('device'), str) and 0 < len(value['device']) <= 1024):
            raise ValueError('Incomplete native observation')
    except (ValueError, TypeError):
        return record
    record.update(status='passed', observation=value)
    return record


def execute(output, *, iterations=4096, runs=4, setup=False, sdk_source=None, driver_library=None, driver_sha256=None):
    if platform.system() not in ('Darwin', 'Linux') or not (type(iterations) is int and 1 <= iterations <= 16384
            and type(runs) is int and 1 <= runs <= 8):
        raise ValueError('Use a supported host and bounded diagnostic limits')
    output = plain(output)
    plain(output.parent, directory=True)
    if output.exists() or shutil.disk_usage(output.parent).free < 2 * 1024**3:
        raise ValueError('Choose a new diagnostic directory with two GiB staging headroom')
    if bool(driver_library) != bool(driver_sha256):
        raise ValueError('Supply both candidate library and its recorded fingerprint')
    if driver_library:
        driver_library = plain(driver_library)
        if not driver_library.is_file() or driver_library.stat().st_nlink != 1 or not re.fullmatch('[0-9a-f]{64}', driver_sha256) or digest(driver_library) != driver_sha256:
            raise ValueError('Candidate driver fingerprint differs')
    compiler = shutil.which('clang') or shutil.which('cc')
    if not compiler:
        raise ValueError('A native C compiler is required')
    environment = {k: v for k, v in os.environ.items() if not k.startswith(('VK_', 'DYLD_', 'SDL_VULKAN', 'RECOMP_', 'TSFP_'))}
    metadata = {}
    if platform.system() == 'Darwin':
        if setup and sdk_source is not None:
            raise ValueError('Choose SDK setup or verified source reuse')
        sdk = vulkan_sdk.setup() if setup else vulkan_sdk.verify_install(source_root=sdk_source)
        flags = ['-I' + str(sdk / 'include'), '-L' + str(sdk / 'lib'), '-Wl,-rpath,' + str(sdk / 'lib'), '-lvulkan']
        environment.update(vulkan_sdk.gpu_environment(sdk, 'moltenvk'))
        metadata['sdk_version'] = vulkan_sdk.pin()['version']
        metadata['baseline_driver_sha256'] = digest(sdk / 'lib/libMoltenVK.dylib')
        versions = (sdk.parent / 'VERSIONS.txt').read_text(encoding='utf-8')
        metadata['baseline_moltenvk_revision'] = next((line.split(' : ', 1)[0] for line in versions.splitlines()
                                                    if line.endswith(' : https://github.com/KhronosGroup/MoltenVK.git')), None)
    else:
        if setup or sdk_source is not None:
            raise ValueError('SDK setup/reuse is specific to the Mac adapter')
        flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'vulkan'], text=True, timeout=15))
    output.mkdir(mode=0o700)
    executable = output / 'query-lifetime'
    source = ROOT / 'tests/graphics/vulkan_query_lifetime.c'
    subprocess.run([compiler, '-std=c11', '-Wall', '-Wextra', '-Werror', '-O1', '-g', str(source), *flags, '-o', str(executable)],
                   check=True, capture_output=True, timeout=60)
    if driver_library:
        descriptor = output / 'candidate-icd.json'
        descriptor.write_text(json.dumps(dict(file_format_version='1.0.0', ICD=dict(library_path=str(driver_library),
                              api_version='1.2.0', is_portability_driver=True))), encoding='utf-8')
        environment['VK_DRIVER_FILES'] = str(descriptor)
        metadata['candidate_driver_sha256'] = driver_sha256
    for name in ('cache', 'config', 'data'):
        (output / name).mkdir(mode=0o700)
    environment.update(XDG_CACHE_HOME=str(output / 'cache'), XDG_CONFIG_HOME=str(output / 'config'), XDG_DATA_HOME=str(output / 'data'))
    results = [observe(executable, environment, iterations) for _ in range(runs)]
    report = dict(schema=1, phase='vulkan-query-lifetime-observation', host=platform.platform(), iterations=iterations,
                  source_revision=subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip(),
                  probe_source_sha256=digest(source), executable_sha256=digest(executable), probes=results,
                  passed=all(row['status'] == 'passed' for row in results), game_assets_used=False,
                  historical_game_crash_fixed=False, driver_candidate=bool(driver_library), **metadata)
    with (output / 'report.json').open('x', encoding='utf-8') as stream:
        json.dump(report, stream, indent=2)
        stream.write('\n')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--iterations', type=int, default=4096)
    parser.add_argument('--runs', type=int, default=4)
    parser.add_argument('--setup-sdk', action='store_true')
    parser.add_argument('--sdk-source-root', type=Path)
    parser.add_argument('--driver-library', type=Path)
    parser.add_argument('--driver-sha256')
    parser.add_argument('--record-only', action='store_true')
    args = parser.parse_args()
    report = execute(args.output, iterations=args.iterations, runs=args.runs, setup=args.setup_sdk, sdk_source=args.sdk_source_root,
                     driver_library=args.driver_library, driver_sha256=args.driver_sha256)
    print(json.dumps(dict(passed=report['passed'], statuses=[r['status'] for r in report['probes']], report=str(args.output / 'report.json'),
                          historical_game_crash_fixed=False)))
    return 0 if report['passed'] or args.record_only else 2


if __name__ == '__main__':
    raise SystemExit(main())
