"""Crash the Windows game before initialization, then independently resolve its dump."""

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

import hashlib
import json
import os
from pathlib import Path
import subprocess
import time
import shutil

ROOT = _layout_root


def main():
    if os.name != 'nt':
        raise RuntimeError('Windows MSVC build required')
    build = ROOT / 'build/windows/Release'
    run = ROOT / 'build/windows/crash-tests'
    run.mkdir(exist_ok=True)
    exe = build / 'futureperfect_recomp.exe'
    helper = build / 'tsfp_crash_helper.exe'
    results = []
    for mode in ('normal', 'handled', 'main', 'worker', 'concurrent'):
        area = run / mode
        area.mkdir(exist_ok=True)
        before = set(area.glob('crash-*'))
        env = {k: v for k, v in os.environ.items() if not k.startswith(('TSFP_', 'RECOMP_'))}
        env['TSFP_CRASH_DIR'] = str(area)
        env['PATH'] = str(ROOT / 'external/ts3FPrecomp/third_party/ffmpeg/bin') + os.pathsep + env.get('PATH', '')
        # A log larger than the permitted tail verifies bounded excerpts.
        log = run / 'fixture.err'
        log.write_bytes(b'old\n' * 20000 + b'crash-test-tail\n')
        env['RECOMP_LOG_FILE'] = str(log)
        process = subprocess.run([str(exe), f'--tsfp-crash-test={mode}'], cwd=run, env=env,
                                 capture_output=True, timeout=25)
        reports = set(area.glob('crash-*')) - before
        if mode in ('normal', 'handled'):
            assert process.returncode == 0, (mode, process.returncode, process.stderr)
            assert not reports, (mode, reports)
            results.append(dict(mode=mode, exit_code=process.returncode, reports=0))
            continue
        assert process.returncode == 0xC0000005, (mode, process.returncode, process.stderr)
        assert len(reports) == 1, (mode, reports)
        folder = reports.pop()
        for _ in range(30):
            if (folder / 'report.txt').exists():
                break
            time.sleep(.1)
        metadata = json.loads((folder / 'report.json').read_text())
        assert metadata['dump_saved'] and metadata['exception_code'] == '0xC0000005', metadata
        assert metadata['executable_sha256'] == hashlib.sha256(exe.read_bytes()).hexdigest()
        assert (folder / 'crash.dmp').stat().st_size <= 16 * 1024 * 1024
        tail = (folder / 'game-log-tail.txt').read_bytes()
        assert len(tail) == 65536 and tail.endswith(b'crash-test-tail\n')
        resolved = subprocess.run([str(helper), '--resolve', str(folder / 'crash.dmp'), str(exe),
                                   str(build), 'tsfp_crash_test_site'], capture_output=True, text=True, timeout=20)
        assert resolved.returncode == 0, resolved.stdout + resolved.stderr
        wrong_image = subprocess.run([str(helper), '--resolve', str(folder / 'crash.dmp'), str(helper),
                                      str(build), 'tsfp_crash_test_site'], capture_output=True, timeout=20)
        assert wrong_image.returncode != 0, 'A mismatched executable/PDB must not pass resolution'
        wrong_function = subprocess.run([str(helper), '--resolve', str(folder / 'crash.dmp'), str(exe),
                                         str(build), 'not_the_crash_function'], capture_output=True, timeout=20)
        assert wrong_function.returncode == 12, 'Unexpected function must fail verification'
        results.append(dict(mode=mode, exit_code=process.returncode, report=str(folder.relative_to(ROOT)),
                            resolution=resolved.stdout.strip(), executable_sha256=metadata['executable_sha256']))
    area = run / 'rotation'
    area.mkdir(exist_ok=True)
    (area / 'keep.txt').write_text('Unrelated files must survive rotation')
    env['TSFP_CRASH_DIR'] = str(area)
    for _ in range(7):
        process = subprocess.run([str(exe), '--tsfp-crash-test=main'], cwd=run, env=env,
                                 capture_output=True, timeout=25)
        assert process.returncode == 0xC0000005
    assert len(list(area.glob('crash-*'))) == 5, 'Serial capture must retain five incidents'
    assert (area / 'keep.txt').read_text() == 'Unrelated files must survive rotation'
    results.append(dict(mode='rotation', reports=5, unrelated_file_preserved=True))
    blocked = run / 'not-a-directory'
    blocked.write_text('Capture cannot create a directory here')
    env['TSFP_CRASH_DIR'] = str(blocked)
    process = subprocess.run([str(exe), '--tsfp-crash-test=main'], cwd=run, env=env,
                             capture_output=True, timeout=25)
    assert process.returncode == 0xC0000005, 'Capture failure must preserve the original exception exit'
    results.append(dict(mode='unwritable-destination', exit_code=process.returncode))
    absent = run / 'no-helper'
    absent.mkdir(exist_ok=True)
    copied = absent / exe.name
    shutil.copyfile(exe, copied)
    try:
        process = subprocess.run([str(copied), '--tsfp-crash-test=normal'], cwd=run, env=env,
                                 capture_output=True, timeout=10)
        assert process.returncode == 4, 'Missing helper must fail the diagnostic test safely'
    finally:
        copied.unlink()  # Only the exact temporary executable created above.
    results.append(dict(mode='missing-helper', exit_code=process.returncode))
    (run / 'validation.json').write_text(json.dumps(results, indent=2) + '\n')
    print(json.dumps(results, indent=2))


if __name__ == '__main__':
    main()
