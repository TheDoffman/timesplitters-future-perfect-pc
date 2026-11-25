"""Production permanent color grade pixels and final-output gating."""
import os
from pathlib import Path
import subprocess
import pytest
from native_compiler import native_compiler
ROOT=Path(__file__).resolve().parents[2]

@pytest.mark.skipif(os.name!='nt',reason='WARP pixels; native RHI gameplay acceptance is separate')
def test_color_grade(tmp_path):
    compiler,env=native_compiler(tmp_path);runtime=ROOT/'external/ts3FPrecomp/src';exe=tmp_path/'color-grade-pixels.exe'
    sources=[ROOT/'tests/graphics/color_grade.c',runtime/'config/recomp_config.c']
    sources += [runtime/'d3d'/n for n in ('rhi.c','rhi_d3d11.c','rhi_shader_cache.c','d3d8_compile.c')]
    command=[compiler,'/nologo','/std:c11','/O2','/D_CRT_SECURE_NO_WARNINGS',
             *['/I'+str(p) for p in (ROOT/'src',runtime,runtime/'d3d',runtime/'config')],
             *map(str,sources),'/Fe:'+str(exe),'/link','d3d11.lib','dxgi.lib','dxguid.lib',
             'd3dcompiler.lib','user32.lib','gdi32.lib']
    built=subprocess.run(command,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=90)
    assert built.returncode==0,built.stdout+built.stderr
    env={k:v for k,v in env.items() if not k.startswith(('RECOMP_','TSFP_'))}
    env.update(TSFP_DISPLAY_CONFIG=str(tmp_path/'display.ini'),RECOMP_DISPLAY_CONFIG=str(tmp_path/'runtime.ini'),
               TSFP_PICTURE_FILE=str(tmp_path/'picture.ini'),RECOMP_SHADER_CACHE='0')
    (tmp_path/'picture.ini').write_text('TSFP_PICTURE_V1 50 200 0 50\n')
    result=subprocess.run([str(exe)],cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert result.returncode==0,result.stdout+result.stderr
    assert 'PASS:' in result.stdout
    assert (tmp_path/'picture.ini').read_text()=='TSFP_PICTURE_V1 50 200 0 50\n'
