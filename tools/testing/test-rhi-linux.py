#!/usr/bin/env python3
"""Build/test the Linux x64 SDL/shared renderer contracts, without game assets."""
import argparse
import json
import platform
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from project_paths import configure
configure()
import linux_vulkan_sdk
from vulkan_sdk import sha256, require_clean_validation
import frame_replay_contract



def configure_gl_environment(env, driver, adapter=None, restricted=False):
    """Select a driver only in the isolated child, never the desktop session."""
    env = env.copy()
    env['RECOMP_D3D8_BACKEND'] = 'opengl'
    for key in ('GALLIUM_DRIVER', 'MESA_LOADER_DRIVER_OVERRIDE',
                'LIBGL_ALWAYS_SOFTWARE', 'MESA_D3D12_DEFAULT_ADAPTER_NAME',
                'TSFP_TEST_GL41_VIEWS'):
        env.pop(key, None)
    if driver == 'software':
        env['GALLIUM_DRIVER'] = 'llvmpipe'
    elif driver == 'd3d12':
        env['GALLIUM_DRIVER'] = 'd3d12'
        env['LD_LIBRARY_PATH'] = '/usr/lib/wsl/lib' + (
            ':' + env['LD_LIBRARY_PATH'] if env.get('LD_LIBRARY_PATH') else '')
    if adapter:
        env['MESA_D3D12_DEFAULT_ADAPTER_NAME'] = adapter
    if restricted:
        env['TSFP_TEST_GL41_VIEWS'] = '1'
    return env


def require_gl_contract(output, driver, restricted=False):
    markers = ('BACKEND: opengl', 'PASS: OpenGL typed null textures',
               'PASS: OpenGL native main-thread dispatch',
               'PASS: OpenGL worker/context recreation and repeated cleanup')
    if any(marker not in output for marker in markers) or '[RHI] ERROR' in output:
        raise RuntimeError('OpenGL resource/dispatch/cleanup checks missing or renderer errors')
    if driver == 'd3d12' and 'D3D12 (' not in output:
        raise RuntimeError('Requested D3D12 GL driver was not observed')
    if driver == 'software' and 'llvmpipe' not in output:
        raise RuntimeError('Requested software GL driver was not observed')
    if restricted and 'PASS: OpenGL4.1 subset/' not in output:
        raise RuntimeError('Restricted GL view checks missing')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--setup-sdk', action='store_true', help='Download/verify the pinned local Linux SDK')
    parser.add_argument('--prepare-runtime', action='store_true', help='Import the pinned snapshot and initialize renderer submodules')
    parser.add_argument('--build-only', action='store_true', help='Compile without requiring a display or Vulkan device')
    parser.add_argument('--compat12', action='store_true', help='Force Vulkan 1.2 compatibility resources/state on a newer driver')
    parser.add_argument('--driver-manifest', type=Path, help='Explicit Vulkan ICD, e.g. Mesa lavapipe for VM/CI checks')
    parser.add_argument('--backend',choices=('vulkan','opengl'),default='vulkan')
    parser.add_argument('--gl-driver',choices=('system','software','d3d12'),default='system',help='Child-only Mesa driver selection for GL')
    parser.add_argument('--gl-adapter',help='Child-only D3D12 adapter name preference')
    parser.add_argument('--gl41',action='store_true',help='Disable view/storage extensions in the single-sample GL fixture')
    args = parser.parse_args()
    if args.backend == 'opengl' and (args.compat12 or args.driver_manifest):
        parser.error('Vulkan controls require --backend vulkan')
    if args.backend!='opengl' and (args.gl_driver!='system' or args.gl_adapter or args.gl41):parser.error('GL controls require --backend opengl')
    if platform.system() != 'Linux' or platform.machine().lower() not in ('x86_64', 'amd64'):
        parser.error('Run inside Linux x64 (WSL 2/Ubuntu, a Linux PC, or Steam Deck desktop mode)')
    if sys.version_info < (3, 12):
        parser.error('Use Python 3.12+ and the shared requirements.txt environment')
    if shutil.disk_usage(ROOT).free < 2 * 1024**3:
        parser.error('Renderer build needs at least 2 GiB free')
    # A failed setup/configure/build must never leave a prior passing receipt
    # looking like evidence for the current attempt.
    logs = ROOT / 'logs'
    logs.mkdir(exist_ok=True)
    suffix = ('-opengl-' + args.gl_driver if args.backend=='opengl' else '')+('-gl41' if args.gl41 else '')+('-compat12' if args.compat12 else '')
    record = logs / ('rhi-linux' + suffix + '.json')
    record.write_text(json.dumps(dict(host=platform.platform(), build_passed=False,
                                      gpu_test='not run', replay_test='not run', passed=False,
                                      scope='Linux foundation attempt; not gameplay'), indent=2) + '\n')
    sdk = linux_vulkan_sdk.setup() if args.setup_sdk else linux_vulkan_sdk.verify_install()
    runtime = ROOT / 'external/ts3FPrecomp'
    run = lambda *command: subprocess.run([str(a) for a in command], cwd=ROOT, check=True)
    if args.prepare_runtime:
        run(sys.executable, ROOT / 'tools/build/prepare-runtime.py')
    revision = re.search(r"\$revision = '([a-f0-9]{40})'", (ROOT / 'tsfp.ps1').read_text())[1]
    actual = subprocess.check_output(['git', '-C', str(runtime), 'rev-parse', 'HEAD'], text=True).strip()
    if actual != revision:
        raise RuntimeError('Runtime checkout differs from the shared pin')
    run(sys.executable, ROOT / 'tools/build/apply-patches.py', runtime, ROOT / 'patches')
    if args.prepare_runtime:
        run('git', '-C', runtime, 'submodule', 'update', '--init', '--depth', '1',
            'third_party/SDL', 'third_party/Vulkan-Headers', 'third_party/volk',
            'third_party/VulkanMemoryAllocator')
    tools = Path(sys.executable).parent
    cmake, ninja = tools / 'cmake', tools / 'ninja'
    if not cmake.is_file() or not ninja.is_file():
        raise RuntimeError('Install requirements.txt into the Linux virtual environment first')
    build = ROOT / 'build/linux/rhi-tests'
    run(cmake, '-S', ROOT / 'tests/rhi', '-B', build, '-G', 'Ninja',
        '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_MAKE_PROGRAM=' + str(ninja),
        '-DTSFP_VULKAN_SDK=' + str(sdk),'-DTSFP_TEST_OPENGL='+('ON' if args.backend=='opengl' else 'OFF'), '-DTSFP_TEST_OPENGL_RECOVERY=OFF')
    run(cmake, '--build', build, '--target', 'tsfp_rhi_vulkan_test', 'd3d8_replay', 'tsfp_native_mmio_test', '--parallel', '4')
    run(build / 'tsfp_native_mmio_test')
    exe, replay = build / 'tsfp_rhi_vulkan_test', build / 'replay/d3d8_replay'
    compiler = re.search(r'^CMAKE_C_COMPILER:[^=]+=(.+)$',
                         (build / 'CMakeCache.txt').read_text(), re.M)[1]
    summary = dict(source_revision=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                   working_tree_changes=subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True),
                   runtime_revision=revision, host=platform.platform(), architecture=platform.machine(),
                   compiler=subprocess.check_output([compiler, '--version'], text=True).splitlines()[0],
                   cmake=subprocess.check_output([str(cmake), '--version'], text=True).splitlines()[0],
                   sdk_version=linux_vulkan_sdk.pin()['version'], build_passed=True,
                   compatibility_forced=args.compat12,backend=args.backend,gl_driver=args.gl_driver,gl_adapter=args.gl_adapter,restricted_views=args.gl41,
                   executable_sha256=sha256(exe), replay_executable_sha256=sha256(replay),
                   gpu_test='not run', replay_test='not run', passed=False,
                   scope=('Asset-free common OpenGL pixel/resource/presentation contract; no replay/game/performance acceptance' if args.backend=='opengl' else 'Asset-free shared Vulkan pixels/resources/presentation and frame replay; not a game build or gameplay validation'))
    record.write_text(json.dumps(summary, indent=2) + '\n')
    if args.build_only:
        print('Linux renderer/replay compiled; GPU and gameplay validation NOT run.')
        return
    env = linux_vulkan_sdk.probe_environment(sdk)
    if args.compat12:env['RECOMP_VK_COMPAT12']='1'
    if args.backend == 'opengl':
        env = configure_gl_environment(env, args.gl_driver, args.gl_adapter, args.gl41)
    if args.driver_manifest:
        manifest = linux_vulkan_sdk.select_driver(env, args.driver_manifest)
        summary['driver_manifest'] = str(manifest)
        summary['driver_manifest_sha256'] = sha256(manifest)
    else:
        summary['driver_manifest'] = 'system discovery'
    summary['wsl'] = 'microsoft' in platform.release().lower()
    started = time.monotonic()
    try:
        result = subprocess.run([str(exe)], cwd=ROOT, env=env, capture_output=True, text=True, timeout=90)
        output = result.stdout + result.stderr
        (logs / ('rhi-linux' + suffix + '.log')).write_text(output)
        print(output)
        result.check_returncode()
        if args.backend == 'vulkan':
            require_clean_validation(output)
        if 'PASS: shared RHI' not in output:
            raise RuntimeError('Shared RHI success marker missing')
        if args.backend=='opengl':
            require_gl_contract(output, args.gl_driver, args.gl41)
            summary['gpu_test']='passed';summary['replay_test']='not run (Vulkan-only replay contract)';summary['passed']=True
            print('PASS: native OpenGL RHI contracts; gameplay and recovery remain unverified.')
            return
        summary['gpu_test'] = 'passed'
        output = frame_replay_contract.run(replay, build / 'replay-contract', env)
        (logs / ('replay-linux' + suffix + '.log')).write_text(output)
        require_clean_validation(output)
        summary['replay_test'] = 'passed'
        summary['passed'] = True
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        summary['error'] = str(error)
        raise
    finally:
        summary['seconds'] = round(time.monotonic() - started, 3)
        record.write_text(json.dumps(summary, indent=2) + '\n')
    print('PASS: Linux shared Vulkan and production frame replay; gameplay remains unverified.')


if __name__ == '__main__':
    main()
