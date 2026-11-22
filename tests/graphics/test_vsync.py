"""Shared presentation preference, failure recovery and real backend contracts."""
import os
import sys
import pytest
import subprocess
from pathlib import Path
from native_compiler import native_compiler
ROOT=Path(__file__).resolve().parents[2]

def test_vsync_persistence_and_present_policy(tmp_path):
    compiler,env=native_compiler(tmp_path)
    source=ROOT/'tests/graphics/vsync_settings.c'
    exe=tmp_path/('vsync.exe' if os.name=='nt' else 'vsync')
    runtime=ROOT/'external/ts3FPrecomp/src/config'
    includes=[ROOT/'src',runtime]
    args=([compiler,'/nologo','/std:c11','/W4','/WX','/D_CRT_SECURE_NO_WARNINGS',
           *['/I'+str(p) for p in includes],str(source),'/Fe:'+str(exe)] if os.name=='nt' else
          [compiler,'-std=c11','-D_POSIX_C_SOURCE=200809L','-Wall','-Wextra','-Werror',
           *['-I'+str(p) for p in includes],str(source),'-o',str(exe)])
    result=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert result.returncode==0,result.stdout+result.stderr
    for initial in (0,1):
        result=subprocess.run([str(exe),str(initial)],cwd=tmp_path,env=env,capture_output=True,text=True,timeout=15)
        assert result.returncode==0,result.stdout+result.stderr

def test_production_present_paths_use_live_preference():
    runtime=ROOT/'external/ts3FPrecomp'
    device=(runtime/'src/d3d/d3d8_device.c').read_text()
    assert 'rhi_present_kind(pc_vsync_present_interval(g_present_interval),RHI_PRESENT_GUEST)' in device
    assert 'rhi_present_kind(pc_vsync_present_interval(g_present_interval),RHI_PRESENT_INTERPOLATED)' in device
    assert 'setting && !pc_vsync_enabled() && InterlockedCompareExchange' in device
    gl=(ROOT/'src/native/gl_display.inc').read_text()
    assert gl.index('pc_vsync_present_interval')<gl.index('SDL_GL_SwapWindow')
    assert 'SDL_GL_SetSwapInterval(interval)' in gl
    vulkan=(runtime/'src/d3d/rhi_vulkan.c').read_text()
    assert 'if (interval)\n        return VK_PRESENT_MODE_FIFO_KHR;' in vulkan

@pytest.mark.skipif(sys.platform!='win32',reason='Windows DXGI swap-chain integration')
def test_vsync_real_swapchain(tmp_path):
    compiler,env=native_compiler(tmp_path)
    runtime=ROOT/'external/ts3FPrecomp/src'
    sources=[ROOT/'tests/graphics/vsync_rhi.c',*(runtime/'d3d'/name for name in
        ('rhi.c','rhi_shader_cache.c','d3d8_compile.c')),runtime/'config/recomp_config.c']
    exe=tmp_path/'vsync-rhi.exe'
    command=[compiler,'/nologo','/std:c11','/O2','/W3',
        *['/I'+str(p) for p in (ROOT/'src',runtime,runtime/'d3d',runtime/'config')],
        *map(str,sources),'/Fe:'+str(exe),'/link','d3d11.lib','dxgi.lib','dxguid.lib',
        'd3dcompiler.lib','user32.lib','gdi32.lib']
    result=subprocess.run(command,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=90)
    assert result.returncode==0,result.stdout+result.stderr
    env={k:v for k,v in env.items() if not k.startswith(('TSFP_','RECOMP_'))}
    env.update(RECOMP_SHADER_CACHE='0',RECOMP_DISPLAY_CONFIG=str(tmp_path/'runtime.ini'),
        TSFP_DISPLAY_CONFIG=str(tmp_path/'display.ini'),RECOMP_SAVE_DIR=str(tmp_path/'saves'))
    result=subprocess.run([str(exe)],cwd=tmp_path,env=env,capture_output=True,text=True,timeout=30)
    assert result.returncode==0,result.stdout+result.stderr


def test_native_gl_observes_toggle_and_context_without_per_frame_driver_calls(tmp_path):
    compiler,env=native_compiler(tmp_path)
    text=(ROOT/'src/native/gl_display.inc').read_text()
    start=text.index('static bool native_swap_window(void) {')
    # It is the final function in the production include.
    (tmp_path/'native_swap_under_test.inc').write_text(text[start:])
    source=ROOT/'tests/graphics/vsync_gl.c'
    exe=tmp_path/('vsync-gl.exe' if os.name=='nt' else 'vsync-gl')
    args=([compiler,'/nologo','/std:c11','/W4','/WX','/D_CRT_SECURE_NO_WARNINGS',
           '/I'+str(ROOT/'src'),'/I'+str(tmp_path),str(source),'/Fe:'+str(exe)] if os.name=='nt' else
          [compiler,'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'src'),
           '-I'+str(tmp_path),str(source),'-o',str(exe)])
    result=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert result.returncode==0,result.stdout+result.stderr
    result=subprocess.run([str(exe)],cwd=tmp_path,env=env,capture_output=True,text=True,timeout=10)
    assert result.returncode==0,result.stdout+result.stderr
