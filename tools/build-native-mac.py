#!/usr/bin/env python3
"""Build the experimental Apple Silicon runtime from the shared pinned sources."""
import argparse
import hashlib
import json
import os
import platform
from pathlib import Path
import re
import shutil
import subprocess
import sys
from native_mac_common import recycle_test_disks

ROOT = Path(__file__).resolve().parents[1]
UPSTREAM = ROOT / 'external/ts3FPrecomp'
PYTHON = ROOT / '.venv/bin/python'
CMAKE = ROOT / '.venv/bin/cmake'
NINJA = ROOT / '.venv/bin/ninja'
BUILD = ROOT / 'build/macos'


def run(*args, cwd=ROOT):
    subprocess.run([str(a) for a in args], cwd=cwd, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--setup', action='store_true', help='Install shared pinned Python requirements and build the symbol scanner')
    parser.add_argument('--generate', action='store_true', help='Regenerate game C from the verified local XBE')
    parser.add_argument('--test', action='store_true', help='Run native platform, MMIO and path regression tests after building')
    args = parser.parse_args()
    if platform.system() != 'Darwin' or platform.machine() != 'arm64':
        parser.error('The native bring-up target currently supports Apple Silicon macOS only.')
    branch = subprocess.check_output(['git', 'branch', '--show-current'], cwd=ROOT, text=True).strip()
    if branch != 'mac-port':
        raise SystemExit('Native work belongs on mac-port; switch explicitly before building.')
    pins = (ROOT / 'tsfp.ps1').read_text()
    revision = re.search(r"\$revision = '([a-f0-9]{40})'", pins)[1]
    expected = re.search(r"\$expectedHash = '([A-F0-9]{64})'", pins)[1].lower()
    xbe = ROOT / 'private/game/default.xbe'
    if hashlib.sha256(xbe.read_bytes()).hexdigest() != expected:
        raise SystemExit('Unsupported XBE; preserve the supported US build.')
    if shutil.disk_usage(ROOT).free < 2 * 1024**3:
        raise SystemExit('Native generation/build needs at least 2 GiB of free disk space.')
    if not PYTHON.exists():
        if sys.version_info < (3, 12):
            raise SystemExit('Run this script with Python 3.12 or newer to create .venv.')
        run(sys.executable, '-m', 'venv', ROOT / '.venv')
    if args.setup:
        run(PYTHON, '-m', 'pip', 'install', '-r', ROOT / 'requirements.txt')
        if not UPSTREAM.exists():
            run('git', 'init', UPSTREAM)
            run('git', '-C', UPSTREAM, 'remote', 'add', 'origin', 'https://github.com/nemonicicon/ts3FPrecomp.git')
            run('git', '-C', UPSTREAM, 'fetch', '--depth', '1', 'origin', revision)
            run('git', '-C', UPSTREAM, 'checkout', '--detach', revision)
    actual = subprocess.check_output(['git', '-C', str(UPSTREAM), 'rev-parse', 'HEAD'], text=True).strip()
    if actual != revision:
        raise SystemExit('Runtime checkout differs from the shared pin; no changes made.')
    run(PYTHON, ROOT / 'tools/apply-patches.py', UPSTREAM, ROOT / 'patches')
    if args.setup:
        run('git', '-C', UPSTREAM, 'submodule', 'update', '--init', '--depth', '1',
            'third_party/SDL', 'third_party/XbSymbolDatabase')
        db = UPSTREAM / 'third_party/XbSymbolDatabase'
        run(CMAKE, '-S', db, '-B', db / 'build', '-G', 'Ninja',
            f'-DCMAKE_MAKE_PROGRAM={NINJA}', '-DCMAKE_BUILD_TYPE=Release', '-DXBSDB_BUILD_UNITTEST=OFF')
        run(CMAKE, '--build', db / 'build', '--target', 'XbSymbolDatabaseCLI', '--parallel', '4')
    generated = UPSTREAM / 'titles/futureperfect/src/recomp/gen/recomp_funcs.h'
    if args.generate:
        run(PYTHON, '-m', 'tools.xdk_symbols', xbe, cwd=UPSTREAM)
        run(PYTHON, 'scripts/recompile.py', xbe, '--work-dir', ROOT / 'private/pipeline',
            '--project', 'titles/futureperfect', '--game-name', 'TimeSplitters: Future Perfect',
            '--trace-all-entries', cwd=UPSTREAM)
    if not generated.exists():
        raise SystemExit('Generated game sources are missing; run with --generate.')
    manual = (UPSTREAM / 'titles/futureperfect/src/recomp_manual.c').read_text()
    missing = [name for name in re.findall(r'extern void (sub_[0-9A-Fa-f]+_gen)\(void\)', manual)
               if name not in generated.read_text()]
    if missing:
        raise SystemExit('Generated wrappers are stale; regenerate with --generate.')
    command = [CMAKE, '-S', UPSTREAM / 'titles/futureperfect', '-B', BUILD, '-G', 'Ninja',
               f'-DCMAKE_MAKE_PROGRAM={NINJA}', '-DCMAKE_BUILD_TYPE=Debug',
               '-DCMAKE_OSX_ARCHITECTURES=arm64', f'-DTSFP_NATIVE_DIR={ROOT / "src/native"}']
    # Use an installed crypto dependency, without independently upgrading it.
    if Path('/opt/homebrew/opt/openssl@3').exists():
        command.append('-DOPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl@3')
    run(*command)
    run(CMAKE, '--build', BUILD, '--target', 'futureperfect_recomp', '--parallel', '4')
    run('file', BUILD / 'futureperfect_recomp')
    # Include shared PC source hashes as well as the native adapters.
    sources = sorted((ROOT / 'patches').glob('*.patch')) + sorted((ROOT / 'src').rglob('*'))
    manifest = dict(source_revision=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                    runtime_revision=revision, host=platform.platform(), xbe_sha256=expected,
                    build_type='Debug', native_game_optimization='-O2 -fno-vectorize -fno-slp-vectorize', configure_command=[str(arg) for arg in command],
                    compiler=subprocess.check_output(['clang', '--version'], text=True).splitlines()[0],
                    executable_sha256=hashlib.sha256((BUILD / 'futureperfect_recomp').read_bytes()).hexdigest(),
                    source_sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources if p.is_file()},
                    python_packages=subprocess.check_output([str(PYTHON), '-m', 'pip', 'freeze'], text=True).splitlines(),
                    runtime_libraries=subprocess.check_output(['otool', '-L', str(BUILD / 'futureperfect_recomp')], text=True))
    (BUILD / 'BUILD.json').write_text(json.dumps(manifest, indent=2) + '\n')
    if args.test:
        run(CMAKE, '--build', BUILD, '--target', 'tsfp_native_platform_test', 'tsfp_native_mmio_test',
            'tsfp_native_paths_test', '--parallel', '4')
        try:
            run(CMAKE.parent / 'ctest', '--test-dir', BUILD, '--output-on-failure')
            env = {k:v for k,v in os.environ.items() if not k.startswith(('RECOMP_', 'TSFP_'))}
            test = BUILD / 'path-tests'
            env.update(RECOMP_INPUT_CONFIG=str(ROOT / 'tests/fixtures/pc-controls.json'),
                       RECOMP_SAVE_DIR=str(test), RECOMP_GAME_DIR=str(ROOT / 'private/game'),
                       RECOMP_DISPLAY_CONFIG=str(test / 'runtime.conf'),
                       TSFP_DISPLAY_CONFIG=str(test / 'pc-display.ini'),
                       XDG_CONFIG_HOME=str(test / 'xdg-config'), XDG_DATA_HOME=str(test / 'xdg-data'))
            for check in ('--renderer-self-test', '--input-self-test', '--settings-self-test'):
                if check == '--settings-self-test':
                    settings = json.loads((ROOT / 'tests/fixtures/pc-controls.json').read_text())
                    settings['keep_test_member'] = {'value': 42}
                    settings['controllers'][1] = {'port': 2, 'device': 'none', 'keep': 'second-player'}
                    control_path = test / 'settings-test.json'
                    control_path.write_text(json.dumps(settings))
                    before = control_path.read_bytes()
                    env['RECOMP_INPUT_CONFIG'] = str(control_path)
                subprocess.run([str(BUILD / 'futureperfect_recomp'), check],
                               cwd=ROOT, env=env, check=True, timeout=20)
                if check == '--settings-self-test':
                    saved = json.loads(control_path.read_text())
                    assert saved['keep_test_member'] == settings['keep_test_member']
                    assert saved['controllers'][1:] == settings['controllers'][1:]
                    assert Path(str(control_path) + '.bak').read_bytes() == before
        finally:
            print('Test disk cleanup:', recycle_test_disks(ROOT, BUILD / 'path-tests'))


if __name__ == '__main__':
    main()
