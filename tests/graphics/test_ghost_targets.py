"""Validate the synthetic replay texture's actual storage contract."""


import struct

from ghost_targets import capture


def test_offscreen_target_storage_covers_swizzled_addresses(tmp_path):
    path = tmp_path/'synthetic.d3dcap'
    capture(path, 160, 90)
    data = path.read_bytes()
    offset = 32
    textures = []
    while offset < len(data):
        kind, size = struct.unpack_from('<II', data, offset)
        offset += 8
        if kind == 11:
            texture, fmt, width, height, levels, usage, pitch, rows, count = struct.unpack_from('<9I', data, offset)
            textures.append(texture)
            assert fmt == 0x06  # D3DFMT_A8R8G8B8, swizzled
            assert width > 0 and width & (width - 1) == 0
            assert height > 0 and height & (height - 1) == 0
            assert (levels, usage) == (1, 1)
            assert pitch == width * 4 and rows == height
            assert count == pitch * rows and size == 36 + count
        offset += (size + 3) & ~3
    assert textures == [1] and offset == len(data)

import os
import subprocess
from pathlib import Path
import pytest
from native_compiler import native_compiler


@pytest.mark.parametrize('broken', [False, True], ids=['native-rhi', 'detect-windows-only'])
def test_live_shared_depth_target_preparation(tmp_path, broken):
    """Compile the real host adapter boundary as native RHI, without _WIN32."""
    root = Path(__file__).resolve().parents[2]
    record = (root/'external/ts3FPrecomp/src/hle/hle_d3d8_record.c').read_text(encoding='utf-8')
    start = record.index('#if defined(_WIN32) || defined(TSFP_RHI_RENDERER)', record.index('HRESULT host_SetRenderTarget('))
    end = record.index('        hr = t->lpVtbl->GetSurfaceLevel', start)
    adapter = record[start:end]
    if broken:
        adapter = adapter.replace('#if defined(_WIN32) || defined(TSFP_RHI_RENDERER)', '#ifdef _WIN32')
    code = r'''
#include <assert.h>
#undef _WIN32
#define TSFP_RHI_RENDERER 1
#define FAILED(x) ((x)<0)
static int g_device_depth=1,calls,fail;
static int xbox_D3D8PrepareSharedDepthTarget(int t,int depth) {
    assert(t==42 && depth==g_device_depth);calls++;return fail ? -1 : 0;
}
static int bind(int depth,int level) {
    int t=42,hr=0;
/* ADAPTER */
    return 7;
}
int main(void) {
    assert(bind(1,0)==7 && calls==1);
    assert(bind(0,0)==7 && calls==1);
    assert(bind(2,0)==7 && calls==1);
    assert(bind(1,1)==7 && calls==1);
    fail=1;assert(bind(1,0)==-1 && calls==2);
    return 0;
}
'''.replace('/* ADAPTER */', adapter)
    src=tmp_path/'shared-depth.c';src.write_text(code,encoding='utf-8')
    exe=tmp_path/('shared-depth.exe' if os.name=='nt' else 'shared-depth')
    compiler,env=native_compiler(tmp_path)
    args=[compiler,'/nologo','/std:c11',str(src),'/Fe:'+str(exe)] if os.name=='nt' else [compiler,'-std=c11',str(src),'-o',str(exe)]
    result=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert result.returncode==0,result.stdout+result.stderr
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
    assert (result.returncode!=0) if broken else (result.returncode==0),result.stdout+result.stderr
