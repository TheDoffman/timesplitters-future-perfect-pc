"""Verify the production SMAA pipeline on WARP with disposable configuration."""
import hashlib
import json
import os
import shutil
from pathlib import Path
import subprocess
import sys
import pytest
from native_compiler import native_compiler

ROOT=Path(__file__).resolve().parents[2]


@pytest.mark.parametrize('source_variable', ['TSFP_PC_CONTROLS_DIR', 'TSFP_NATIVE_DIR'])
def test_reused_runtime_config_uses_active_source(tmp_path, source_variable):
    """Compile real settings with an unrelated dependency checkout and stale header."""
    upstream=ROOT/'external/ts3FPrecomp'
    compiler,env=native_compiler(tmp_path)
    import cmake as cmake_package
    import ninja as ninja_package
    cmake=Path(cmake_package.CMAKE_BIN_DIR)/('cmake.exe' if os.name=='nt' else 'cmake')
    ninja=Path(ninja_package.BIN_DIR)/('ninja.exe' if os.name=='nt' else 'ninja')
    assert cmake.is_file() and ninja.is_file(), 'Install the pinned CMake and Ninja packages'
    runtime=tmp_path/'dependency/runtime'
    for folder in ('src/config','tests/config_smoke','src/launcher'):
        shutil.copytree(upstream/folder,runtime/folder)
    active=tmp_path/'current checkout/src'
    (active/'graphics').mkdir(parents=True)
    (active/'native').mkdir()
    shutil.copy2(ROOT/'src/graphics/pc_smaa_settings.h',active/'graphics/pc_smaa_settings.h')
    stale=tmp_path/'src/graphics'
    stale.mkdir(parents=True)
    (stale/'pc_smaa_settings.h').write_text('#error stale dependency source selected\n')
    title=(upstream/'titles/futureperfect/CMakeLists.txt').read_text(encoding='utf-8')
    prelude=title[title.index('# Project source files are grouped'):title.index('# Add xboxrecomp as a subdirectory')]
    start=title.index('add_subdirectory(${XBOXRECOMP_DIR} ${CMAKE_BINARY_DIR}/xboxrecomp)')
    postlude=title[title.index('\n',start):title.index('if(WIN32 AND TSFP_PC_CONTROLS_DIR)',start)]
    source_path=active/'native' if source_variable=='TSFP_NATIVE_DIR' else active
    project=tmp_path/'driver';project.mkdir()
    base=('cmake_minimum_required(VERSION 3.20)\nproject(config_source C)\n'
          'set(CMAKE_C_STANDARD 11)\n'+f'set({source_variable} "{source_path.as_posix()}")\n'+prelude+
          f'add_subdirectory("{(runtime/"src/config").as_posix()}" config)\n')
    for name,body,success in [('current',base+postlude,True),('stale-control',base,False)]:
        (project/'CMakeLists.txt').write_text(body,encoding='utf-8')
        output=tmp_path/name
        configured=subprocess.run([str(cmake),'-S',str(project),'-B',str(output),'-G','Ninja',
            '-DCMAKE_MAKE_PROGRAM='+str(ninja),'-DCMAKE_C_COMPILER='+compiler],env=env,
            capture_output=True,text=True,timeout=60)
        assert configured.returncode==0,configured.stdout+configured.stderr
        built=subprocess.run([str(cmake),'--build',str(output),'--target','xbox_config'],env=env,
            capture_output=True,text=True,timeout=60)
        assert (built.returncode==0)==success,built.stdout+built.stderr
        if not success:
            assert 'stale dependency source selected' in built.stdout+built.stderr

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
