"""Compile the real common renderer and exercise its OS adapter contract."""

from project_paths import source_file as repo_file

import os
from pathlib import Path
import subprocess
import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
RUNTIME = ROOT / 'external/ts3FPrecomp/src'
D3D = RUNTIME / 'd3d'


def test_host_contract(tmp_path):
    compiler, env = native_compiler(tmp_path)
    exe = tmp_path / ('host.exe' if os.name == 'nt' else 'host')
    sources = [str(ROOT / 'tests/graphics/rhi_host.c'), str(D3D / 'rhi.c')]
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/I'+str(D3D), '/I'+str(ROOT/'src'), *sources,
                '/Fe:'+str(exe), '/link', 'user32.lib','gdi32.lib']
    else:
        args = [compiler, '-std=c11', '-Wall', '-Wextra', '-Werror',
                '-I'+str(D3D), '-I'+str(ROOT/'src'), *sources, '-o', str(exe)]
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    run_env=dict(env or os.environ, TSFP_FRAME_PROFILE=str(tmp_path/'host-presents.csv'))
    result = subprocess.run([str(exe)], env=run_env, capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, result.stdout + result.stderr


@pytest.mark.parametrize('source', [
    'd3d8_device.c', 'd3d8_resources.c', 'd3d8_shaders.c', 'd3d8_states.c',
    'd3d8_combiners.c', 'd3d8_vsh.c', 'd3d8_screencopy.c', 'd3d8_movie.c',
    'd3d8_display.c', 'd3d8_compile.c', 'rhi_shader_cache.c', 'rhi.c'])
def test_common_renderer_compiles(tmp_path, source):
    compiler, env = native_compiler(tmp_path)
    includes = [ROOT / 'src', RUNTIME, D3D, RUNTIME / 'config']
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/Zs', '/DTSFP_PC_DISPLAY',
                '/DTSFP_TEXTURE_PACK', '/DTSFP_RHI_RENDERER']
        args += ['/I'+str(path) for path in includes]
    else:
        args = [compiler, '-std=c11', '-fsyntax-only', '-Werror=implicit-function-declaration',
                '-DTSFP_PC_DISPLAY', '-DTSFP_TEXTURE_PACK', '-DTSFP_RHI_RENDERER']
        args += ['-I'+str(path) for path in includes]
    result = subprocess.run([*args, str(D3D / source)], cwd=tmp_path, env=env,
                            capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr


def test_macos_text_bitmap(tmp_path):
    import sys
    if sys.platform != 'darwin':
        pytest.skip('CoreText host adapter requires macOS')
    compiler, env = native_compiler(tmp_path)
    exe=tmp_path/'text-bitmap'
    result=subprocess.run([compiler,'-I'+str(ROOT/'src/native'),
        str(ROOT/'tests/graphics/text_bitmap_macos.c'),str(ROOT/'src/native/text_bitmap_macos.m'),
        '-framework','CoreText','-framework','CoreGraphics','-framework','CoreFoundation',
        '-o',str(exe)],env=env,capture_output=True,text=True,timeout=60)
    assert result.returncode==0,result.stdout+result.stderr
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
    assert result.returncode==0,result.stdout+result.stderr


def test_interpolation_depth_cache(tmp_path):
    compiler, env=native_compiler(tmp_path)
    exe=tmp_path/('depth-cache.exe' if os.name=='nt' else 'depth-cache')
    source=str(ROOT/'tests/graphics/interpolation_depth_cache.c')
    if os.name=='nt':
        args=[compiler,'/nologo','/std:c11','/I'+str(D3D),'/I'+str(ROOT/'src'),source,'/Fe:'+str(exe)]
    else:
        args=[compiler,'-std=c11','-Wall','-Wextra','-Werror','-I'+str(D3D),'-I'+str(ROOT/'src'),source,'-o',str(exe)]
    result=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert result.returncode==0,result.stdout+result.stderr
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
    assert result.returncode==0,result.stdout+result.stderr


def test_present_metrics(tmp_path):
    import csv
    compiler, env=native_compiler(tmp_path)
    exe=tmp_path/('present.exe' if os.name=='nt' else 'present')
    source=str(ROOT/'tests/gameplay/present_metrics.c')
    if os.name=='nt':
        args=[compiler,'/nologo','/std:c11','/I'+str(ROOT/'src'),source,'/Fe:'+str(exe)]
    else:
        args=[compiler,'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'src'),source,'-o',str(exe)]
    result=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert result.returncode==0,result.stdout+result.stderr
    env=dict(env or os.environ, TSFP_FRAME_PROFILE=str(tmp_path/'present.csv'))
    result=subprocess.run([str(exe)],env=env,capture_output=True,text=True,timeout=10)
    assert result.returncode==0,result.stdout+result.stderr
    rows=list(csv.DictReader((tmp_path/'present.csv').open()))
    assert len(rows)==2
    assert rows[0]==dict(frame='2',elapsed_ms='7.000',frame_ms='7.000',swap_ms='2.000',
                        kind='interpolated',guest_frame='1',clock_ns='10000000')
    assert rows[1]['kind']=='other'
    assert rows[1]['clock_ns']=='12000000'


@pytest.mark.parametrize('name', ['interpolation_draw_key'])
def test_interpolation_present_outcomes(tmp_path,name):
    compiler, env=native_compiler(tmp_path)
    exe=tmp_path/('interp-present.exe' if os.name=='nt' else 'interp-present')
    includes=[tmp_path,ROOT/'src',RUNTIME,*[RUNTIME/name for name in ('d3d','config','hle','kernel','platform')]]
    if name=='interpolation_draw_key':
        recorder=(RUNTIME/'hle/hle_d3d8_record.c').read_text()
        begin=recorder.index('static void draw_key(')
        end=recorder.index('\n}\n',begin)+3
        (tmp_path/'draw_key.inc').write_text(recorder[begin:end])
    source=str(repo_file(ROOT, 'tests', f'{name}.c'))
    if os.name=='nt':
        args=[compiler,'/nologo','/std:c11',*['/I'+str(p) for p in includes],source,'/Fe:'+str(exe)]
    else:
        args=[compiler,'-std=gnu11',*['-I'+str(p) for p in includes],source,'-o',str(exe)]
    result=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert result.returncode==0,result.stdout+result.stderr
    result=subprocess.run([str(exe)],cwd=tmp_path,capture_output=True,text=True,timeout=10)
    assert result.returncode==0,result.stdout+result.stderr
