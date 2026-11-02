#!/usr/bin/env python3
"""Build/test the Linux x64 SDL/shared Vulkan foundation, without game assets."""
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--setup-sdk', action='store_true', help='Download/verify the pinned local Linux SDK')
    parser.add_argument('--prepare-runtime', action='store_true', help='Import the pinned snapshot and initialize renderer submodules')
    parser.add_argument('--build-only', action='store_true', help='Compile without requiring a display or Vulkan device')
    parser.add_argument('--driver-manifest', type=Path, help='Explicit Vulkan ICD, e.g. Mesa lavapipe for VM/CI checks')
    args = parser.parse_args()
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
    record = logs / 'rhi-linux.json'
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
        '-DTSFP_VULKAN_SDK=' + str(sdk))
    run(cmake, '--build', build, '--target', 'tsfp_rhi_vulkan_test', 'd3d8_replay', '--parallel', '4')
    exe, replay = build / 'tsfp_rhi_vulkan_test', build / 'replay/d3d8_replay'
    compiler = re.search(r'^CMAKE_C_COMPILER:[^=]+=(.+)$',
                         (build / 'CMakeCache.txt').read_text(), re.M)[1]
    summary = dict(source_revision=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                   working_tree_changes=subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True),
                   runtime_revision=revision, host=platform.platform(), architecture=platform.machine(),
                   compiler=subprocess.check_output([compiler, '--version'], text=True).splitlines()[0],
                   cmake=subprocess.check_output([str(cmake), '--version'], text=True).splitlines()[0],
                   sdk_version=linux_vulkan_sdk.pin()['version'], build_passed=True,
                   executable_sha256=sha256(exe), replay_executable_sha256=sha256(replay),
                   gpu_test='not run', replay_test='not run', passed=False,
                   scope='Asset-free shared Vulkan pixels/resources/presentation and frame replay; not a game build or gameplay validation')
    record.write_text(json.dumps(summary, indent=2) + '\n')
    if args.build_only:
        print('Linux renderer/replay compiled; GPU and gameplay validation NOT run.')
        return
    env = linux_vulkan_sdk.probe_environment(sdk)
    if args.driver_manifest:
        manifest = args.driver_manifest.resolve(strict=True)
        json.loads(manifest.read_text())
        env['VK_DRIVER_FILES'] = str(manifest)
        summary['driver_manifest'] = str(manifest)
        summary['driver_manifest_sha256'] = sha256(manifest)
    else:
        summary['driver_manifest'] = 'system discovery'
    summary['wsl'] = 'microsoft' in platform.release().lower()
    started = time.monotonic()
    try:
        result = subprocess.run([str(exe)], cwd=ROOT, env=env, capture_output=True, text=True, timeout=90)
        output = result.stdout + result.stderr
        (logs / 'rhi-linux.log').write_text(output)
        print(output)
        result.check_returncode()
        require_clean_validation(output)
        if 'PASS: shared RHI' not in output:
            raise RuntimeError('Shared RHI success marker missing')
        summary['gpu_test'] = 'passed'
        output = frame_replay_contract.run(replay, build / 'replay-contract', env)
        (logs / 'replay-linux.log').write_text(output)
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
