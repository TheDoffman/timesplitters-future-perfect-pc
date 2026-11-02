"""Verify the production SMAA pipeline on WARP with disposable configuration."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import pytest
from native_compiler import native_compiler

ROOT=Path(__file__).resolve().parents[2]

def test_pinned_smaa_and_embedded_shader():
    subprocess.run([sys.executable,'-X','utf8',str(ROOT/'tools/build/embed-smaa.py'),'--check'],check=True)
    pins=json.loads((ROOT/'vendor/smaa/provenance.json').read_text())
    for name,digest in pins['sha256'].items():
        assert hashlib.sha256((ROOT/'vendor/smaa'/name).read_bytes()).hexdigest()==digest

@pytest.mark.skipif(sys.platform!='win32',reason='Windows D3D11 WARP adapter')
@pytest.mark.parametrize('hardware',[False,True],ids=['warp','hardware'])
def test_smaa_rhi_pixels_and_fallback(tmp_path,hardware):
    compiler,env=native_compiler(tmp_path)
    runtime=ROOT/'external/ts3FPrecomp/src'
    sources=[ROOT/'tests/graphics/smaa_rhi.c',*(runtime/'d3d'/name for name in
        ('rhi.c','rhi_d3d11.c','rhi_shader_cache.c','d3d8_compile.c')),
        runtime/'config/recomp_config.c']
    exe=tmp_path/'smaa.exe'
    command=[compiler,'/nologo','/std:c11','/O2','/W3',
        *['/I'+str(p) for p in (ROOT/'src',runtime,runtime/'d3d',runtime/'config')],
        *map(str,sources),'/Fe:'+str(exe),'/link','d3d11.lib','dxgi.lib','dxguid.lib',
        'd3dcompiler.lib','user32.lib','gdi32.lib']
    result=subprocess.run(command,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=90)
    assert result.returncode==0,result.stdout+result.stderr
    env={k:v for k,v in env.items() if not k.startswith(('TSFP_','RECOMP_'))}
    env.update(RECOMP_SHADER_CACHE='0',RECOMP_DISPLAY_CONFIG=str(tmp_path/'runtime.ini'),
        TSFP_DISPLAY_CONFIG=str(tmp_path/'display.ini'),RECOMP_SAVE_DIR=str(tmp_path/'saves'))
    if hardware: env['SMAA_TEST_HARDWARE']='1'
    else: env.pop('SMAA_TEST_HARDWARE',None)
    result=subprocess.run([str(exe)],cwd=tmp_path,env=env,capture_output=True,text=True,timeout=90)
    if hardware and result.returncode==77: pytest.skip(result.stdout.strip())
    assert result.returncode==0,result.stdout+result.stderr
    assert 'PASS: SMAA edges' in result.stdout
