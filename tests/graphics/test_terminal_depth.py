"""Run production HLE depth clear propagation without a GPU or game assets."""


import os
from pathlib import Path
import subprocess

import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.parametrize('broken', [False, True], ids=['fixed', 'detect-missed-clear'])
def test_terminal_depth(tmp_path, broken):
    compiler, env = native_compiler(tmp_path)
    runtime = ROOT/'external/ts3FPrecomp/src'
    includes = [runtime, *(runtime/n for n in ('hle', 'd3d', 'kernel', 'config', 'platform'))]
    source = (runtime/'hle/hle_d3d8.c').read_text(encoding='utf-8')
    if broken:
        hook = '            shadow_depth_cleared(host_flags, z, stencil);'
        assert source.count(hook) == 1
        source = source.replace(hook, '            ; /* mutation: clear not propagated */')
    # Compile the exact production clear/cache functions in isolation. The
    # rest of this large HLE unit pulls in a window, audio and the guest CPU.
    def section(start, end):
        assert source.count(start) == 1
        return source[source.index(start):source.index(end, source.index(start))]
    declarations = r"""
#include "platform/xbox_winnt.h"
#include "hle.h"
#include "d3d8_xbox.h"
#include "d3d8_xbox_map.h"
#include "hle_d3d8_record.h"
static IDirect3DDevice8 *g_shadow;
static unsigned long g_shadow_clears, g_shadow_swaps;
static uint32_t g_shadow_last_color;
static HleDepthClears g_depth_clears;
static HleDepthKey g_depth_key;
static int g_depth_key_valid;
#define SHADOW_SCRATCH 8
static int shadow_trace_on(void) { return 0; }
static int original_missing(void (*fn)(void),const char *name) { return fn==NULL; }
static void first_call(int *seen,const char *name,uint32_t arg) { }
HLE_ORIGINAL(D3DDevice_Clear);
"""
    implementation = declarations
    implementation += section('/* One guest depth image may need', 'static void surface_measure')
    implementation += section('static IDirect3DSurface8 *depth_surface(', 'static int shadow_maps_on')
    implementation += section('HLE_EXPORT(D3DDevice_Clear)', "/* Where a frame's time goes")
    (tmp_path/'terminal_depth_impl.h').write_text(implementation, encoding='utf-8')
    includes.insert(0, tmp_path)
    exe = tmp_path/('depth.exe' if os.name == 'nt' else 'depth')
    if os.name == 'nt':
        args = [compiler, '/nologo', '/std:c11', '/O2', '/Gy', '/Gw', '/we4028', '/we4029']
        args += ['/I'+str(p) for p in includes]
        args += [str(ROOT/'tests/graphics/terminal_depth.c'), '/Fe:'+str(exe), '/link', '/OPT:REF', 'user32.lib']
    else:
        args = [compiler, '-std=gnu11', '-O2', '-Wl,-dead_strip']
        args += ['-I'+str(p) for p in includes]
        args += [str(ROOT/'tests/graphics/terminal_depth.c'), '-o', str(exe)]
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout+result.stderr
    result = subprocess.run([str(exe)], cwd=tmp_path, capture_output=True, text=True, timeout=10)
    if broken:
        assert result.returncode != 0, 'Test missed a lost depth clear'
        assert 'last_z==0.75f' in result.stderr
    else:
        assert result.returncode == 0, result.stdout+result.stderr
        assert 'PASS: terminal depth' in result.stdout
