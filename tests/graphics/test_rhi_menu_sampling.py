"""The common RHI contract on Windows WARP; Mac runs the pinned Vulkan runner."""


import os
from pathlib import Path
import subprocess
import sys
import pytest
from native_compiler import native_compiler
ROOT=Path(__file__).resolve().parents[2]


@pytest.mark.skipif(sys.platform!='win32',reason='Windows common RHI D3D11 adapter')
@pytest.mark.parametrize('offsets',[True,False])
def test_shared_rhi_menu_warp(tmp_path,offsets):
    compiler,env=native_compiler(tmp_path)
    runtime=ROOT/'external/ts3FPrecomp/src'
    exe=tmp_path/'rhi-menu.exe'
    sources=[ROOT/'tests/graphics/rhi_menu_sampling.c',*(runtime/'d3d'/name for name in
        ('rhi.c','rhi_d3d11.c','rhi_shader_cache.c','d3d8_compile.c','d3d8_resources.c','d3d8_screencopy.c','d3d8_states.c')),
        runtime/'config/recomp_config.c']
    if not offsets:
        # Reproduce the old dropped-offset behavior while retaining every
        # other production translation, draw and real D24 GPU operation.
        original=runtime/'d3d/d3d8_states.c'
        dropped=tmp_path/'d3d8_states.c'
        source=original.read_text()
        mutated=source.replace('rd.depth_bias = (int32_t)bias;','rd.depth_bias = 0;')
        assert source!=mutated
        dropped.write_text(mutated)
        sources=[dropped if p==original else p for p in sources]
    args=[compiler,'/nologo','/std:c11','/O2','/W3',
          *['/I'+str(path) for path in (ROOT/'src',ROOT/'tests',runtime,runtime/'d3d',runtime/'config')],
          *map(str,sources),'/Fe:'+str(exe),'/link','d3d11.lib','dxgi.lib','dxguid.lib','d3dcompiler.lib','user32.lib','gdi32.lib']
    result=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=90)
    assert result.returncode==0,result.stdout+result.stderr
    env={k:v for k,v in env.items() if not k.startswith(('RECOMP_','TSFP_'))}
    env.update(RECOMP_SHADER_CACHE='0',RECOMP_DISPLAY_CONFIG=str(tmp_path/'runtime.ini'),
               TSFP_DISPLAY_CONFIG=str(tmp_path/'display.ini'),RECOMP_SAVE_DIR=str(tmp_path/'saves'))
    result=subprocess.run([str(exe)],cwd=tmp_path,env=env,capture_output=True,text=True,timeout=30)
    if offsets:
        assert result.returncode==0,result.stdout+result.stderr
        assert 'PASS: shared RHI HLSL' in result.stdout
    else:
        assert result.returncode!=0 and 'FAIL' in result.stderr,result.stdout+result.stderr
        assert 'Testing coplanar polygon offsets' in result.stdout
        assert 'fabsf(p[0]-r)' in result.stderr
        assert 'PASS: shared RHI HLSL' not in result.stdout
