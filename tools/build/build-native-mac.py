#!/usr/bin/env python3
"""Build the experimental Apple Silicon runtime from the shared pinned sources."""

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
import platform
from pathlib import Path
import re
import shutil
import subprocess
import sys
from native_mac_common import recycle_test_disks
import vulkan_sdk
from build_textures import ensure_pack

ROOT = _layout_root
UPSTREAM = ROOT / 'external/ts3FPrecomp'
PYTHON = ROOT / '.venv/bin/python'
CMAKE = ROOT / '.venv/bin/cmake'
NINJA = ROOT / '.venv/bin/ninja'
BUILD = ROOT / 'build/macos'


def select_renderer(requested, macos_version, minimum_macos):
    """Use the common frontend where its pinned SDK supports the host."""
    if requested == 'gl':
        return 'gl'
    major = macos_version.split('.')[0]
    if not major.isdigit():
        raise RuntimeError('Cannot determine macOS version; use --renderer gl explicitly for the legacy build.')
    supported = int(major) >= minimum_macos
    if requested == 'vulkan' and not supported:
        raise RuntimeError(f'The pinned common renderer requires macOS {minimum_macos}+; use --renderer gl on this host.')
    return 'vulkan' if supported else 'gl'


def run(*args, cwd=ROOT):
    environment = dict(os.environ, TSFP_PROJECT_ROOT=str(ROOT))
    subprocess.run([str(a) for a in args], cwd=cwd, check=True, env=environment)


def main():
    global UPSTREAM, PYTHON, CMAKE, NINJA, BUILD
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--setup', action='store_true', help='Install pinned build dependencies (including the common-renderer SDK when selected) and build the symbol scanner')
    parser.add_argument('--generate', action='store_true', help='Regenerate game C from the verified local XBE')
    parser.add_argument('--test', action='store_true', help='Run native platform, MMIO and path regression tests after building')
    parser.add_argument('--without-textures', action='store_true', help='Skip texture setup for an explicitly lightweight/offline build')
    parser.add_argument('--renderer', choices=('auto','gl','vulkan'), default='auto',
                        help='Default: common Xbox/Vulkan frontend on supported macOS; legacy GL on older hosts')
    parser.add_argument('--vulkan-driver', choices=tuple(vulkan_sdk.DRIVERS), default=vulkan_sdk.DEFAULT_DRIVER,
                        help='Pinned Vulkan driver recorded in the build manifest (default: MoltenVK)')
    parser.add_argument('--runtime-root', type=Path, default=ROOT, help='Reuse this same-host checkout’s ignored dependencies, game data and single native build')
    args = parser.parse_args()
    runtime_root=args.runtime_root.resolve()
    UPSTREAM=runtime_root/'external/ts3FPrecomp'; PYTHON=runtime_root/'.venv/bin/python'
    CMAKE=runtime_root/'.venv/bin/cmake'; NINJA=runtime_root/'.venv/bin/ninja'; BUILD=runtime_root/'build/macos'
    if platform.system() != 'Darwin' or platform.machine() != 'arm64':
        parser.error('The native bring-up target currently supports Apple Silicon macOS only.')
    pins = (ROOT / 'tsfp.ps1').read_text()
    revision = re.search(r"\$revision = '([a-f0-9]{40})'", pins)[1]
    expected = re.search(r"\$expectedHash = '([A-F0-9]{64})'", pins)[1].lower()
    xbe = runtime_root / 'private/game/default.xbe'
    if not xbe.is_file():
        raise SystemExit('Missing private/game/default.xbe; import the supported US game before building.')
    if hashlib.sha256(xbe.read_bytes()).hexdigest() != expected:
        raise SystemExit('Unsupported XBE; preserve the supported US build.')
    if shutil.disk_usage(ROOT).free < 2 * 1024**3:
        raise SystemExit('Native generation/build needs at least 2 GiB of free disk space.')
    try:
        renderer = ('gl' if args.renderer == 'gl' else
                    select_renderer(args.renderer, platform.mac_ver()[0],
                                    vulkan_sdk.pin(runtime_root)['minimum_macos']))
        # Reject missing/changed dependencies before patching or generating code.
        # Only explicit --setup may install; never silently switch a capable host
        # back to a different frontend when its SDK fails verification.
        sdk = None
        if renderer == 'vulkan':
            sdk = vulkan_sdk.setup(runtime_root) if args.setup else vulkan_sdk.verify_install(runtime_root)
    except (OSError, ValueError, RuntimeError) as error:
        raise SystemExit(f'Native renderer prerequisite: {error}') from error
    print(f'Native renderer: {renderer} (requested {args.renderer})', flush=True)
    if not PYTHON.exists():
        if sys.version_info < (3, 12):
            raise SystemExit('Run this script with Python 3.12 or newer to create .venv.')
        run(sys.executable, '-m', 'venv', runtime_root / '.venv')
    if args.setup:
        run(PYTHON, '-m', 'pip', 'install', '-r', ROOT / 'requirements.txt')
        run(PYTHON, ROOT / 'tools/build/prepare-runtime.py', '--destination', UPSTREAM)
    actual = subprocess.check_output(['git', '-C', str(UPSTREAM), 'rev-parse', 'HEAD'], text=True).strip()
    if actual != revision:
        raise SystemExit('Runtime checkout differs from the shared pin; no changes made.')
    textures = None
    if not args.without_textures and json.loads((ROOT/'config/textures/texture-pack.json').read_text())['delivery'] == 'bundled':
        _, _, textures = ensure_pack(ROOT)
    run(PYTHON, ROOT / 'tools/build/apply-patches.py', UPSTREAM, ROOT / 'patches')
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
        run(PYTHON, 'scripts/recompile.py', xbe, '--work-dir', runtime_root / 'private/pipeline',
            '--project', 'titles/futureperfect', '--game-name', 'TimeSplitters: Future Perfect',
            '--trace-all-entries', cwd=UPSTREAM)
        run(PYTHON, ROOT / 'tools/build/recomp-generation.py', UPSTREAM, '--record')
    if not generated.exists():
        raise SystemExit('Generated game sources are missing; run with --generate.')
    if subprocess.run([str(PYTHON), str(ROOT / 'tools/build/recomp-generation.py'), str(UPSTREAM)]).returncode:
        raise SystemExit('Generator sources changed; regenerate with --generate before building.')
    manual = (UPSTREAM / 'titles/futureperfect/src/recomp_manual.c').read_text()
    missing = [name for name in re.findall(r'extern void (sub_[0-9A-Fa-f]+_gen)\(void\)', manual)
               if name not in generated.read_text()]
    if missing:
        raise SystemExit('Generated wrappers are stale; regenerate with --generate.')
    # Both hosts derive compatibility from the same source, runtime and content.
    run(PYTHON, ROOT / 'tools/build/network-identity.py', '--runtime', UPSTREAM,
        '--game', runtime_root / 'private/game', '--output', BUILD / 'generated')
    command = [CMAKE, '-S', UPSTREAM / 'titles/futureperfect', '-B', BUILD, '-G', 'Ninja',
               f'-DCMAKE_MAKE_PROGRAM={NINJA}', '-DCMAKE_BUILD_TYPE=Debug',
               '-DCMAKE_OSX_ARCHITECTURES=arm64', f'-DTSFP_NATIVE_DIR={ROOT / "src/native"}']
    # Use an installed crypto dependency, without independently upgrading it.
    if Path('/opt/homebrew/opt/openssl@3').exists():
        command.append('-DOPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl@3')
    command.append('-DTSFP_NATIVE_RHI='+('ON' if renderer=='vulkan' else 'OFF'))
    if renderer=='vulkan':
        command.append('-DTSFP_VULKAN_SDK='+str(sdk))
    run(*command)
    run(CMAKE, '--build', BUILD, '--target', 'futureperfect_recomp', '--parallel', '4')
    run('file', BUILD / 'futureperfect_recomp')
    run(PYTHON, ROOT / 'tools/build/network-identity.py', '--output', BUILD / 'generated',
        '--seal-executable', BUILD / 'futureperfect_recomp')
    run('dsymutil', BUILD / 'futureperfect_recomp', '-o', BUILD / 'futureperfect_recomp.dSYM')
    # Include shared PC source hashes as well as the native adapters.
    sources = sorted(p for p in (ROOT / 'patches').rglob('*.patch')
                     if 'legacy' not in p.relative_to(ROOT / 'patches').parts)
    sources += sorted((ROOT / 'src').rglob('*'))
    manifest = dict(auto_display_resolution=True, source_revision=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                    textures=textures,
                    working_tree_changes=subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True),
                    runtime_revision=revision, host=platform.platform(), xbe_sha256=expected,
                    compatibility_identity=json.loads((BUILD / 'generated/network-identity.json').read_text())['compatibility'],
                    renderer=renderer, vulkan_driver=args.vulkan_driver if renderer=='vulkan' else None, renderer_requested=args.renderer, build_type='Debug', native_game_optimization='-O2 -fno-vectorize -fno-slp-vectorize', configure_command=[str(arg) for arg in command],
                    symbol_uuid=subprocess.check_output(['dwarfdump','--uuid',str(BUILD/'futureperfect_recomp.dSYM')],text=True).split(' (arm64)')[0],
                    symbols_sha256=hashlib.sha256((BUILD/'futureperfect_recomp.dSYM/Contents/Resources/DWARF/futureperfect_recomp').read_bytes()).hexdigest(),
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
                       RECOMP_SAVE_DIR=str(test), RECOMP_GAME_DIR=str(runtime_root / 'private/game'),
                       RECOMP_DISPLAY_CONFIG=str(test / 'runtime.conf'),
                       TSFP_DISPLAY_CONFIG=str(test / 'pc-display.ini'),
                       XDG_CONFIG_HOME=str(test / 'xdg-config'), XDG_DATA_HOME=str(test / 'xdg-data'),
                       XDG_CACHE_HOME=str(test / 'xdg-cache'))
            if renderer=='vulkan':
                env=vulkan_sdk.with_probe_gpu_environment(env,vulkan_sdk.verify_install(runtime_root),vulkan_sdk.select_driver(manifest))
                env['RECOMP_VK_VALIDATION']='1'
            for check in ('--renderer-self-test', '--input-self-test', '--settings-self-test', '--debug-self-test'):
                if check in ('--settings-self-test', '--debug-self-test'):
                    settings = json.loads((ROOT / 'tests/fixtures/pc-controls.json').read_text())
                    settings['keep_test_member'] = {'value': 42}
                    settings['controllers'][1] = {'port': 2, 'device': 'none', 'keep': 'second-player'}
                    control_path = test / ('debug-test.json' if check == '--debug-self-test' else 'settings-test.json')
                    control_path.write_text(json.dumps(settings))
                    before = control_path.read_bytes()
                    env['RECOMP_INPUT_CONFIG'] = str(control_path)
                result=subprocess.run([str(BUILD / 'futureperfect_recomp'), check],
                               cwd=ROOT, env=env, timeout=20,
                               capture_output=True,text=True)
                print(result.stdout,end='');print(result.stderr,end='',file=sys.stderr)
                result.check_returncode()
                if renderer=='vulkan' and check=='--renderer-self-test':
                    vulkan_sdk.require_clean_validation(result.stdout+result.stderr)
                if check in ('--settings-self-test', '--debug-self-test'):
                    saved = json.loads(control_path.read_text())
                    assert saved['keep_test_member'] == settings['keep_test_member']
                    assert saved['controllers'][1:] == settings['controllers'][1:]
                    assert Path(str(control_path) + '.bak').read_bytes() == before
        finally:
            print('Test disk cleanup:', recycle_test_disks(runtime_root, BUILD / 'path-tests'))


if __name__ == '__main__':
    main()
