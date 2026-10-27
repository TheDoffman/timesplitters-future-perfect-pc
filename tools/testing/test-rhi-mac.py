#!/usr/bin/env python3
"""Build/run the shared Vulkan renderer contract without any game assets."""

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
import json
import platform
from pathlib import Path
import re
import subprocess
import sys
import time
import vulkan_sdk
import frame_replay_contract

ROOT = _layout_root


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--setup-sdk', action='store_true', help='Download and verify the pinned checkout-local SDK')
    parser.add_argument('--driver', choices=['moltenvk', 'kosmickrisp'], default=vulkan_sdk.DEFAULT_DRIVER)
    args = parser.parse_args()
    if platform.system() != 'Darwin' or platform.machine() != 'arm64':
        parser.error('Requires Apple Silicon macOS')
    if int(platform.mac_ver()[0].split('.')[0]) < vulkan_sdk.pin()['minimum_macos']:
        parser.error('The pinned validation SDK requires macOS 26 or later; the GL game path is unchanged')
    sdk = vulkan_sdk.setup() if args.setup_sdk else vulkan_sdk.verify_install()
    upstream = ROOT / 'external/ts3FPrecomp'
    revision = re.search(r"\$revision = '([a-f0-9]{40})'", (ROOT/'tsfp.ps1').read_text())[1]
    actual = subprocess.check_output(['git','-C',str(upstream),'rev-parse','HEAD'],text=True).strip()
    if actual != revision:
        raise RuntimeError('Runtime checkout differs from the shared pin')
    subprocess.run([sys.executable,str(ROOT/'tools/build/apply-patches.py'),str(upstream),str(ROOT/'patches')],check=True)
    subprocess.run(['git','-C',str(upstream),'submodule','update','--init','--depth','1',
        'third_party/SDL','third_party/Vulkan-Headers','third_party/volk','third_party/VulkanMemoryAllocator'],check=True)
    tools = Path(sys.executable).parent
    cmake, ninja = tools/'cmake', tools/'ninja'
    if not cmake.is_file() or not ninja.is_file():
        raise RuntimeError('Install the shared cmake/ninja requirements beside this Python interpreter')
    build = ROOT/'build/macos/rhi-tests'
    subprocess.run([str(cmake),'-S',str(ROOT/'tests/rhi'),'-B',str(build),'-G','Ninja',
        '-DCMAKE_BUILD_TYPE=Release','-DCMAKE_OSX_ARCHITECTURES=arm64',
        '-DCMAKE_MAKE_PROGRAM='+str(ninja),'-DTSFP_VULKAN_SDK='+str(sdk)],check=True)
    subprocess.run([str(cmake),'--build',str(build),'--target','tsfp_rhi_vulkan_test','d3d8_replay','--parallel','4'],check=True)
    exe=build/'tsfp_rhi_vulkan_test'
    env=vulkan_sdk.probe_environment(sdk,args.driver)
    started=time.monotonic()
    result=subprocess.run([str(exe)],env=env,cwd=ROOT,capture_output=True,text=True,timeout=60)
    output=result.stdout+result.stderr
    logs=ROOT/'logs';logs.mkdir(exist_ok=True)
    (logs/('rhi-'+args.driver+'.log')).write_text(output)
    validation_error=None
    try:
        vulkan_sdk.require_clean_validation(output)
    except RuntimeError as error:
        validation_error=str(error)
    passed=result.returncode==0 and 'PASS: shared RHI' in output and validation_error is None
    summary=dict(source_revision=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
        working_tree_changes=subprocess.check_output(['git','status','--porcelain'],text=True),
        runtime_revision=revision,host=platform.platform(),sdk_version=vulkan_sdk.pin()['version'],
        driver=args.driver,executable_sha256=vulkan_sdk.sha256(exe),seconds=round(time.monotonic()-started,3),
        returncode=result.returncode,passed=passed,validation_error=validation_error,scope='Asset-free common RHI shader/pixel/resource/presentation contract; not gameplay')
    (logs/('rhi-'+args.driver+'.json')).write_text(json.dumps(summary,indent=2)+'\n')
    print(output)
    if not passed:raise SystemExit('Common RHI validation failed; see logs/rhi-'+args.driver+'.log')
    replay_executable = build/'replay/d3d8_replay'
    summary['replay_executable_sha256'] = vulkan_sdk.sha256(replay_executable)
    summary['replay_passed'] = False
    try:
        replay_output = frame_replay_contract.run(replay_executable,
                                                  build/'replay-contract', env)
        vulkan_sdk.require_clean_validation(replay_output)
        summary['replay_passed'] = True
    finally:
        summary['passed'] = passed and summary['replay_passed']
        (logs/('rhi-'+args.driver+'.json')).write_text(json.dumps(summary,indent=2)+'\n')
    print('PASS: production frame replay red triangle / blue clear (one frame)')


if __name__=='__main__':
    main()
