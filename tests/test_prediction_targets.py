"""Exercise production replay writes against held effect textures and depth."""
import os
from pathlib import Path
import subprocess

import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[1]


@pytest.mark.skipif(os.name != 'nt', reason='Windows CPU prediction')
@pytest.mark.parametrize('mutation', ['none', 'clear', 'draw', 'indexed', 'copy'])
def test_prediction_targets(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    runtime = ROOT / 'external/ts3FPrecomp/src'
    source = (runtime / 'hle/hle_d3d8_record.c').read_text()

    def section(start, end):
        assert source.count(start) == 1
        a = source.index(start)
        return source[a:source.index(end, a)]

    implementation = '''
#include "platform/xbox_winnt.h"
#include "d3d8_xbox.h"
#include "hle_d3d8_record.h"
static int hle_d3d8_interp_predicting;
static IDirect3DBaseTexture8 *g_target_texture;
static IDirect3DSurface8 *g_target_depth, *g_device_depth;
static int g_target_lost;
static IDirect3DDevice8 *hle_d3d8_shadow_device(void) { return NULL; }
'''
    implementation += section('static int prediction_holds_target(void)', 'static unsigned long g_draws')
    implementation += next(line for line in source.splitlines() if line.startswith('#define DEFER_ALIGN')) + '\n'
    implementation += section('#define DEFER_PART_A', '/* ------------------------------------------------- frame interpolation')
    parts = {
        'clear': section('typedef struct { DWORD count, flags;', 'HRESULT host_Clear('),
        'draw': section('typedef struct { DWORD type; UINT prims, stride;', 'typedef struct {\n    DWORD type;'),
        'indexed': section('typedef struct {\n    DWORD type;', 'HRESULT host_DrawPrimitiveUP('),
        'copy': section('typedef struct { IDirect3DTexture8 *dst, *source;', 'HRESULT host_CopyBackBufferToTexture('),
    }
    for name, code in parts.items():
        if mutation == name:
            guard = ('if (hle_d3d8_interp_predicting) return;' if name == 'copy'
                     else 'if (prediction_holds_target()) return;')
            assert code.count(guard) == 1
            code = code.replace(guard, '/* mutation: write leaks into shared target */')
        implementation += code
    (tmp_path / 'prediction_targets_impl.h').write_text(implementation)
    exe = tmp_path / 'prediction-targets.exe'
    includes = [tmp_path, runtime, *(runtime / p for p in ('hle', 'd3d', 'kernel', 'config', 'platform'))]
    args = [compiler, '/nologo', '/std:c11', '/O2', '/we4028', '/we4029']
    args += ['/I' + str(p) for p in includes]
    args += [str(ROOT / 'tests/prediction_targets.c'), '/Fe:' + str(exe)]
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
    if mutation == 'none':
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'PASS: prediction target preservation' in result.stdout
    else:
        assert result.returncode != 0 and 'FAIL' in result.stderr, 'Missed a shared-target write'
