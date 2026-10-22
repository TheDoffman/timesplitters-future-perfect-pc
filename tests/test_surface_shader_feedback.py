"""Run the original water feedback shader on WARP, including zero-step drift.

The shader definition comes from the pinned local XBE; its HLSL is generated
by the current production translator. This isolates shader arithmetic from
texture filtering, storage rounding, random disturbances and scene rendering.
"""
import hashlib
import json
import subprocess
import sys
from pathlib import Path

import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[1]
RUNTIME = ROOT / 'external/ts3FPrecomp/src'


@pytest.mark.skipif(sys.platform != 'win32', reason='Windows WARP shader execution')
@pytest.mark.parametrize('address,case,stage', [(0x4bbf10, 'surface_feedback_gpu.inc', 7),
                                                (0x4bbc40, 'surface_filter_gpu.inc', 4),
                                                (0x4bbf10, 'surface_damping_gpu.inc', 7),
                                                (0x4bbf10, 'surface_coupling_gpu.inc', 7)])
def test_original_surface_shader_feedback(tmp_path, address, case, stage):
    sys.path.insert(0, str(RUNTIME.parent))
    from tools.recomp import config
    xbe = ROOT / 'private/game/default.xbe'
    if not xbe.is_file():
        pytest.skip('Supported US XBE required; game shader bytes are not distributed')
    image = xbe.read_bytes()
    assert hashlib.sha256(image).hexdigest() == '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    previous = dict(vars(config))
    try:
        config.configure_from_xbe(str(xbe))
        offset = config.va_to_file_offset(address)
        (tmp_path / 'feedback.bin').write_bytes(image[offset:offset + 240])
        if case in ('surface_damping_gpu.inc', 'surface_coupling_gpu.inc'):
            import struct
            from tools.recomp.translator import FunctionTranslator
            from test_recomp_float_joins import compile_and_run
            start=0x3d9520;info={'end':0x3d97e7,'name':'sub_003D9520'}
            code=FunctionTranslator(image,{start:info}).translate_function(start,info)
            constants='\n'.join(f'MEM32(0x{a:x})=0x{struct.unpack_from("<I",image,config.va_to_file_offset(a))[0]:08x};'
                for a in range(0x5496c0,0x549720,4))
            fixture=(ROOT/'tests/surface_coefficient_upload.c').read_text()
            packed=compile_and_run(tmp_path,fixture.replace('/* GENERATED_UPLOAD */',code)
                                   .replace('/* ORIGINAL_CONSTANTS */',constants))
            print(packed)
            rows=[dict(field.split('=') for field in line.split()[1:])
                  for line in packed.splitlines() if line.startswith('upload ')]
            assert [r['fps'] for r in rows]==['60','120','144']
            (tmp_path/'surface_upload.h').write_text('static const unsigned packed_coefficients[3][3]={' +
                ','.join('{'+','.join(r[k] for k in ('a','b','d'))+'}' for r in rows)+'};\n')
    finally:
        vars(config).update(previous)

    compiler, env = native_compiler(tmp_path)
    include = ['/I' + str(p) for p in
               (ROOT / 'src', ROOT / 'tests', RUNTIME, RUNTIME / 'd3d', RUNTIME / 'config')]

    def run(args, environment=env):
        result = subprocess.run(args, cwd=tmp_path, env=environment,
                                capture_output=True, text=True, timeout=90)
        assert result.returncode == 0, result.stdout + result.stderr
        return result.stdout

    translator = (RUNTIME / 'd3d/d3d8_combiners.c').read_text(encoding='utf-8')
    # Compile the production parser/generator through its end, without the
    # device/cache integration below it. No generated shader text is rewritten.
    end = translator.index('\n#undef EMIT') + len('\n#undef EMIT')
    source = tmp_path / 'translate.c'
    source.write_text(translator[:end] + '\n' +
                      (ROOT / 'tests/surface_shader_definition.c').read_text(encoding='utf-8'),
                      encoding='utf-8')
    generator = tmp_path / 'translate.exe'
    run([compiler, '/nologo', '/std:c11', '/O2', *include, str(source),
         '/Fe:' + str(generator)])
    hlsl = run([str(generator), 'feedback.bin'])
    assert f'Stage {stage}' in hlsl and 'float4 main(' in hlsl
    (tmp_path / 'surface_shader.h').write_text(
        'static const char surface_hlsl[]=' + json.dumps(hlsl) + ';\n', encoding='utf-8')

    setup = (ROOT / 'tests/rhi_menu_sampling.c').read_text(encoding='utf-8')
    setup = setup[:setup.index('    CHECK(menu_sampling_cases(render));')]
    if case in ('surface_damping_gpu.inc', 'surface_coupling_gpu.inc'):
        setup = setup.replace('#include "rhi.h"','#include "rhi.h"\n#include "surface_upload.h"')
    setup = setup.replace('#include "rhi.h"',
                          '#include "rhi.h"\n#include "d3d8_combiners.h"\n#include "surface_shader.h"\n#include "graphics/combiner_constants.h"\n#include "pc_surface_bias.h"')
    source = tmp_path / 'surface.c'
    source.write_text(setup + (ROOT / 'tests' / case).read_text(encoding='utf-8'),
                      encoding='utf-8')
    exe = tmp_path / 'surface.exe'
    sources = [source, *(RUNTIME / 'd3d' / name for name in
               ('rhi.c', 'rhi_d3d11.c', 'rhi_shader_cache.c', 'd3d8_compile.c',
                'd3d8_resources.c', 'd3d8_screencopy.c')), RUNTIME / 'config/recomp_config.c']
    run([compiler, '/nologo', '/std:c11', '/O2', '/W3', *include, *map(str, sources),
         '/Fe:' + str(exe), '/link', 'd3d11.lib', 'dxgi.lib', 'dxguid.lib',
         'd3dcompiler.lib', 'user32.lib', 'gdi32.lib'])
    isolated = {k: v for k, v in env.items() if not k.startswith(('RECOMP_', 'TSFP_'))}
    isolated.update(RECOMP_SHADER_CACHE='0', RECOMP_DISPLAY_CONFIG=str(tmp_path / 'runtime.ini'),
                    TSFP_DISPLAY_CONFIG=str(tmp_path / 'display.ini'), RECOMP_SAVE_DIR=str(tmp_path / 'saves'))
    output = run([str(exe)], isolated)
    print(output)
    assert 'PASS: original translated feedback shader' in output
