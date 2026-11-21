"""Run identical production RHI pixels/resources/present checks on Windows hardware."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools/build'))
from windows_vulkan import verify, sha256


def run(root=ROOT, layer_path=None):
    verify(root)
    exe = root/'build/windows/rhi-compare/Release/tsfp_windows_rhi_test.exe'
    report = dict(executable_sha256=sha256(exe), backends={}, passed=False,
                  scope='Windows hardware RHI pixel/resource/swapchain contract; no gameplay/performance claim')
    logs = root/'logs'
    logs.mkdir(exist_ok=True)
    report_path=logs/'rhi-windows.json'
    report_path.write_text(json.dumps(report,indent=2)+'\n')
    for mode in ('d3d11', 'vulkan', 'vulkan-fallback'):
        backend = 'vulkan' if mode=='vulkan-fallback' else mode
        expected = 'd3d11' if mode=='vulkan-fallback' else backend
        folder = root/'build/windows/rhi-compare/runs'/mode
        folder.mkdir(parents=True, exist_ok=True)
        env = {k:v for k,v in os.environ.items() if not k.startswith(('RECOMP_', 'TSFP_'))}
        env.update(RECOMP_D3D8_BACKEND=backend, RECOMP_SHADER_CACHE='0', RECOMP_VK_VALIDATION='1',
                   RECOMP_DISPLAY_CONFIG=str(folder/'runtime.ini'), TSFP_DISPLAY_CONFIG=str(folder/'display.ini'),
                   RECOMP_SAVE_DIR=str(folder/'saves'), RECOMP_INPUT_CONFIG=str(folder/'controls.json'))
        if layer_path:
            env['VK_LAYER_PATH']=str(Path(layer_path).resolve(strict=True))
            implicit=folder/'implicit-layers'
            implicit.mkdir(exist_ok=True)
            env['VK_IMPLICIT_LAYER_PATH']=str(implicit)
        if mode=='vulkan-fallback':
            # Override discovery only for this child, without changing installed drivers.
            missing=str(folder/'missing-driver.json')
            env.update(VK_DRIVER_FILES=missing,VK_ICD_FILENAMES=missing,TSFP_TEST_EXPECT_BACKEND=expected,
                       RECOMP_VK_VALIDATION='0')
        start = time.monotonic()
        try:
            result = subprocess.run([str(exe)], cwd=folder, env=env, capture_output=True, text=True, timeout=180)
        except subprocess.TimeoutExpired as error:
            def decoded(value):
                return value.decode(errors='replace') if isinstance(value,bytes) else (value or '')
            result=subprocess.CompletedProcess([str(exe)],-1,decoded(error.stdout),decoded(error.stderr)+'\nTIMEOUT\n')
        output = result.stdout+result.stderr
        (logs/f'rhi-windows-{mode}.log').write_text(output)
        passed = (result.returncode==0 and f'BACKEND: {expected}' in output and
                  'PASS: shared RHI HLSL' in output and '[VKVAL' not in output and
                  ('using d3d11' not in output if mode!='vulkan-fallback' else
                   'could not start; using d3d11' in output))
        if layer_path and mode=='vulkan':
            passed=passed and 'validation on' in output
        report['backends'][mode] = dict(exit_code=result.returncode, passed=passed,
            elapsed_seconds=round(time.monotonic()-start,3),
            validation_available='validation on' in output,
            log=f'logs/rhi-windows-{mode}.log')
        print(mode, 'PASS' if passed else 'FAIL', flush=True)
        report_path.write_text(json.dumps(report,indent=2)+'\n')
    report['passed'] = all(item['passed'] for item in report['backends'].values())
    report_path.write_text(json.dumps(report, indent=2)+'\n')
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--validation-layer-path',type=Path)
    args=parser.parse_args()
    raise SystemExit(run(layer_path=args.validation_layer_path))
