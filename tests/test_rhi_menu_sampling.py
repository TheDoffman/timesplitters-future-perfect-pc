"""The common RHI contract on Windows WARP; Mac runs the pinned Vulkan runner."""
import os
from pathlib import Path
import subprocess
import sys
import pytest
from native_compiler import native_compiler
ROOT=Path(__file__).resolve().parents[1]


@pytest.mark.skipif(sys.platform!='win32',reason='Windows common RHI D3D11 adapter')
def test_shared_rhi_menu_warp(tmp_path):
    compiler,env=native_compiler(tmp_path)
    runtime=ROOT/'external/ts3FPrecomp/src'
    exe=tmp_path/'rhi-menu.exe'
    sources=[ROOT/'tests/rhi_menu_sampling.c',*(runtime/'d3d'/name for name in
        ('rhi.c','rhi_d3d11.c','rhi_shader_cache.c','d3d8_compile.c','d3d8_resources.c','d3d8_screencopy.c')),
        runtime/'config/recomp_config.c']
    args=[compiler,'/nologo','/std:c11','/O2','/W3',
          *['/I'+str(path) for path in (ROOT/'src',ROOT/'tests',runtime,runtime/'d3d',runtime/'config')],
          *map(str,sources),'/Fe:'+str(exe),'/link','d3d11.lib','dxgi.lib','dxguid.lib','d3dcompiler.lib','user32.lib','gdi32.lib']
    result=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=90)
    assert result.returncode==0,result.stdout+result.stderr
    env={k:v for k,v in env.items() if not k.startswith(('RECOMP_','TSFP_'))}
    env.update(RECOMP_SHADER_CACHE='0',RECOMP_DISPLAY_CONFIG=str(tmp_path/'runtime.ini'),
               TSFP_DISPLAY_CONFIG=str(tmp_path/'display.ini'),RECOMP_SAVE_DIR=str(tmp_path/'saves'))
    result=subprocess.run([str(exe)],cwd=tmp_path,env=env,capture_output=True,text=True,timeout=30)
    assert result.returncode==0,result.stdout+result.stderr
    assert 'PASS: shared RHI HLSL' in result.stdout
