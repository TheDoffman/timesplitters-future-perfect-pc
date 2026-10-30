"""Exercise the actual game watchdog before opening game data, saves or settings."""

# Direct-command imports share the repository's grouped helper paths.
import sys as _layout_sys
from pathlib import Path as _LayoutPath
_layout_tools = _LayoutPath(__file__).resolve().parents[1]
_layout_root = _layout_tools.parent
if (_layout_tools / 'project_paths.py').is_file():
    _layout_sys.path.insert(0, str(_layout_tools))
    from project_paths import configure as _configure_paths, source_file as repo_file
    _configure_paths()
else:
    # Standalone source fixtures retain their original flat-folder behavior.
    _layout_root = _LayoutPath(__file__).resolve().parents[1]
    def repo_file(root, area, name):
        return _LayoutPath(root) / area / name

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time

ROOT = _layout_root


def completed(area, before):
    reports = []
    for folder in set(area.glob('crash-*')) - before:
        try:
            if not (folder / 'report.txt').is_file():
                continue
            metadata = json.loads((folder / 'report.json').read_text())
            reports.append((folder, metadata))
        except (OSError, ValueError):
            pass
    return reports


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=['unarmed', 'startup', 'responsive', 'inhibited', 'transient',
                                         'hang', 'recover', 'twice', 'then-crash'])
    args = parser.parse_args()
    if os.name != 'nt':
        raise RuntimeError('Windows build required')
    build = ROOT / 'build/windows/Release'
    run = ROOT / 'build/windows/hang-tests'
    run.mkdir(exist_ok=True)
    exe, helper = build / 'futureperfect_recomp.exe', build / 'tsfp_crash_helper.exe'
    executable_sha256 = hashlib.sha256(exe.read_bytes()).hexdigest()
    results = []
    for mode in [args.mode] if args.mode else ['unarmed', 'startup', 'responsive', 'inhibited', 'transient',
                                            'hang', 'recover', 'twice', 'then-crash']:
        area = run / mode
        area.mkdir(exist_ok=True)
        before = set(area.glob('crash-*'))
        environment = {k: v for k, v in os.environ.items() if not k.startswith(('TSFP_', 'RECOMP_'))}
        environment['TSFP_CRASH_DIR'] = str(area)
        environment['PATH'] = str(ROOT / 'external/ts3FPrecomp/third_party/ffmpeg/bin') + os.pathsep + environment.get('PATH', '')
        process = subprocess.Popen([str(exe), '--tsfp-hang-test=' + mode], cwd=run, env=environment,
                                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            if mode == 'hang':
                deadline = time.monotonic() + 15
                while time.monotonic() < deadline and not completed(area, before):
                    assert process.poll() is None, 'The watchdog must leave the hung game running'
                    time.sleep(.1)
                reports = completed(area, before)
                assert len(reports) == 1, reports
                folder, metadata = reports[0]
                assert metadata['kind'] == 'suspected_hang' and metadata['dump_saved'], metadata
                assert metadata['exception_code'] is None and metadata['guest_is_cached']
                assert metadata['host_rip'] is None and metadata['host_rsp'] is None
                resolved = subprocess.run([str(helper), '--resolve-hang', str(folder / 'crash.dmp'), str(exe),
                                           str(build), str(metadata['thread_id']), 'tsfp_hang_test_site'],
                                          text=True, capture_output=True, timeout=20)
                assert resolved.returncode == 0, resolved.stdout + resolved.stderr
                time.sleep(2.2)
                assert process.poll() is None and len(completed(area, before)) == 1, 'One report per ongoing stall'
                results.append(dict(mode=mode, left_running=True, reports=1, resolution=resolved.stdout.strip()))
            else:
                code = process.wait(timeout=20)
                assert code == (0xC0000005 if mode == 'then-crash' else 0), (mode, code)
                reports = completed(area, before)
                expected = 2 if mode in ('twice', 'then-crash') else 1 if mode == 'recover' else 0
                assert len(reports) == expected, (mode, reports)
                kinds = sorted(metadata['kind'] for _, metadata in reports)
                if mode == 'then-crash':
                    assert kinds == ['suspected_hang', 'unhandled_exception'], kinds
                elif expected:
                    assert kinds == ['suspected_hang'] * expected, kinds
                assert all(metadata['dump_saved'] for _, metadata in reports), reports
                results.append(dict(mode=mode, exit_code=code, reports=len(reports), kinds=kinds))
            print('PASS', mode, flush=True)
        finally:
            if process.poll() is None:
                process.terminate()  # Only this deliberately hung diagnostic process.
                process.wait(timeout=10)
    for result in results:
        result['executable_sha256'] = executable_sha256
    (run / ('validation-' + args.mode + '.json' if args.mode else 'validation.json')).write_text(json.dumps(results, indent=2) + '\n')
    print(json.dumps(results, indent=2))


if __name__ == '__main__':
    main()
