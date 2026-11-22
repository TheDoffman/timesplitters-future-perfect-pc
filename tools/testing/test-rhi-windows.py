"""Run identical production RHI pixels/resources/present checks on Windows hardware."""
import argparse
import json
import os
import shutil
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools/build'))
from windows_vulkan import verify, sha256


def run(root=ROOT, layer_path=None, selected_backend=None, opengl_recovery=False, gl41=False):
    verify(root)
    exe = root/'build/windows/rhi-compare/Release/tsfp_windows_rhi_test.exe'
    report = dict(executable_sha256=sha256(exe), backends={}, passed=False,
                  scope='Windows hardware RHI pixel/resource/swapchain contract; no gameplay/performance claim')
    logs = root/'logs'
    logs.mkdir(exist_ok=True)
    report_path=logs/('rhi-windows-opengl-gl41.json' if gl41 else 'rhi-windows-opengl-recovery.json' if opengl_recovery else f'rhi-windows-{selected_backend}.json' if selected_backend else 'rhi-windows.json')
    if gl41:report['scope']='OpenGL4.1 single-sample shader/resource/query compatibility; MSAA omitted, no native-host acceptance'
    report_path.write_text(json.dumps(report,indent=2)+'\n')
    modes=(selected_backend,) if selected_backend else ('default','d3d11','vulkan','vulkan-no-driver','default-no-driver','default-no-dxc','default-no-device','adopt-d3d11')
    capability_modes={'opengl-no-core':'core','opengl-no-s3tc':'s3tc','opengl-no-mirror-once':'mirror-once'}
    if opengl_recovery:modes=('default','opengl-recover-no-driver','opengl-recover-no-device','opengl-both-fail','opengl-partial-fail',*capability_modes)
    for mode in modes:
        startup_failure=mode in capability_modes or mode in ('vulkan-no-driver','default-no-driver','default-no-dxc','default-no-device','opengl-both-fail','opengl-partial-fail')
        recovered=mode in ('opengl-recover-no-driver','opengl-recover-no-device')
        backend=mode if mode in ('d3d11','opengl') else 'vulkan'
        expected='opengl' if recovered else 'd3d11' if mode=='adopt-d3d11' else backend
        folder = root/'build/windows/rhi-compare/runs'/mode
        folder.mkdir(parents=True, exist_ok=True)
        env = {k:v for k,v in os.environ.items() if not k.startswith(('RECOMP_', 'TSFP_'))}
        env.update(RECOMP_SHADER_CACHE='0', RECOMP_VK_VALIDATION='1',
                   RECOMP_DISPLAY_CONFIG=str(folder/'runtime.ini'), TSFP_DISPLAY_CONFIG=str(folder/'display.ini'),
                   RECOMP_SAVE_DIR=str(folder/'saves'), RECOMP_INPUT_CONFIG=str(folder/'controls.json'))
        if gl41:env['TSFP_TEST_GL41_VIEWS']='1'
        if not mode.startswith('default'):env['RECOMP_D3D8_BACKEND']=backend
        if startup_failure:env.update(TSFP_TEST_EXPECT_STARTUP_FAILURE='1',RECOMP_VK_VALIDATION='0')
        if recovered:env.update(TSFP_TEST_EXPECT_BACKEND='opengl',RECOMP_VK_VALIDATION='0')
        if mode=='adopt-d3d11':env.update(TSFP_TEST_ADOPT_D3D11='1',TSFP_TEST_EXPECT_BACKEND='d3d11')
        if layer_path:
            env['VK_LAYER_PATH']=str(Path(layer_path).resolve(strict=True))
            implicit=folder/'implicit-layers'
            implicit.mkdir(exist_ok=True)
            env['VK_IMPLICIT_LAYER_PATH']=str(implicit)
        if mode in capability_modes or mode in ('vulkan-no-driver','default-no-driver','opengl-recover-no-driver','opengl-partial-fail'):
            # Override discovery only for this child, without changing installed drivers.
            missing=str(folder/'missing-driver.json')
            env.update(VK_DRIVER_FILES=missing,VK_ICD_FILENAMES=missing,TSFP_TEST_EXPECT_BACKEND=expected,
                       RECOMP_VK_VALIDATION='0')
        if mode in ('default-no-device','opengl-recover-no-device'):env['VK_LOADER_VENDOR_ID_FILTER']='0xffff'
        if mode=='opengl-partial-fail':env['TSFP_TEST_EMPTY_DRAWABLE']='1'
        if mode in capability_modes:env['TSFP_TEST_GL_MISSING_CAPABILITY']=capability_modes[mode]
        executable=exe
        if mode in ('default-no-dxc','opengl-both-fail'):
            executable=folder/exe.name
            shutil.copy2(exe,executable)
            env['PATH']=str(Path(os.environ['SystemRoot'])/'System32')
        start = time.monotonic()
        try:
            result = subprocess.run([str(executable)], cwd=folder, env=env, capture_output=True, text=True, timeout=180)
        except subprocess.TimeoutExpired as error:
            def decoded(value):
                return value.decode(errors='replace') if isinstance(value,bytes) else (value or '')
            result=subprocess.CompletedProcess([str(exe)],-1,decoded(error.stdout),decoded(error.stderr)+'\nTIMEOUT\n')
        output = result.stdout+result.stderr
        log_name=f'rhi-windows-{mode}'+('-gl41' if gl41 else '')+'.log'
        (logs/log_name).write_text(output)
        no_fallback = 'using d3d11' not in output and 'BACKEND: d3d11' not in output
        if startup_failure:
            passed = (result.returncode==0 and no_fallback and
                      'PASS: Vulkan startup failed without renderer fallback' in output and
                      ('Vulkan and OpenGL initialization failed' if mode in capability_modes or mode in ('opengl-both-fail','opengl-partial-fail') else
                       'vulkan renderer startup failed; automatic renderer fallback is disabled') in output and
                      'PASS: shared RHI HLSL' not in output)
        else:
            passed = (result.returncode==0 and f'BACKEND: {expected}' in output and
                      'PASS: shared RHI HLSL' in output and '[VKVAL' not in output and
                      'could not start; using' not in output)
        if expected=='opengl' and not startup_failure:
            passed=passed and all(marker in output for marker in ('PASS: OpenGL typed null textures','PASS: shared mirror-once sampling beyond both texture edges','PASS: shared BC1/BC2/BC3 sampled colors and alpha'))
        if gl41:passed=passed and 'PASS: OpenGL4.1 subset/array/cube/volume/stencil/compressed views' in output and 'SKIP: MSAA/SSAO' in output
        if recovered:passed=passed and no_fallback and 'startup recovery selected opengl' in output
        if mode=='opengl-partial-fail':passed=passed and 'drawable target initialization failed' in output
        if mode in capability_modes:
            reason={'core':'OpenGL4.1 core','s3tc':'BC1/BC2/BC3 S3TC','mirror-once':'mirror-once sampler'}[capability_modes[mode]]
            passed=passed and 'required capability unavailable: '+reason in output and 'PASS: OpenGL capability rejection cleanup and context recreation' in output
        if mode in ('default-no-dxc','opengl-both-fail'):passed=passed and 'DXC compiler (dxcompiler) was not found' in output
        if mode in ('default-no-device','opengl-recover-no-device'):passed=passed and 'no Vulkan device' in output
        if layer_path and mode in ('default','vulkan'):
            passed=passed and 'validation on' in output
        report['backends'][mode] = dict(exit_code=result.returncode, passed=passed,
            elapsed_seconds=round(time.monotonic()-start,3),
            validation_available='validation on' in output,
            log='logs/'+log_name)
        print(mode, 'PASS' if passed else 'FAIL', flush=True)
        report_path.write_text(json.dumps(report,indent=2)+'\n')
    report['passed'] = all(item['passed'] for item in report['backends'].values())
    report_path.write_text(json.dumps(report, indent=2)+'\n')
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--validation-layer-path',type=Path)
    parser.add_argument('--backend',choices=('vulkan','d3d11','opengl'))
    parser.add_argument('--opengl-recovery',action='store_true',help='Require the fixture-only accepted OpenGL recovery build')
    parser.add_argument('--opengl-gl41',action='store_true',help='Force the single-sample4.1 view compatibility path in the Windows GL fixture')
    args=parser.parse_args()
    if args.opengl_recovery and args.backend:parser.error('Choose --backend or --opengl-recovery')
    if args.opengl_gl41:
        if args.opengl_recovery or args.backend not in (None,'opengl'):parser.error('The4.1 probe requires explicit OpenGL without recovery cases')
        args.backend='opengl'
    raise SystemExit(run(layer_path=args.validation_layer_path,selected_backend=args.backend,opengl_recovery=args.opengl_recovery,gl41=args.opengl_gl41))
