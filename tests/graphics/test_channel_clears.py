"""Check production Xbox clear mapping and the versioned capture container."""


import os
from pathlib import Path
import subprocess

import pytest
from native_compiler import native_compiler

ROOT=Path(__file__).resolve().parents[2]
RUNTIME=ROOT/'external/ts3FPrecomp'

@pytest.mark.parametrize('fixture',['mapping','capture'])
def test_channel_clears(tmp_path,fixture):
    compiler,environment=native_compiler(tmp_path)
    source=RUNTIME/'src'
    includes=[source,source/'d3d',source/'hle']
    if fixture=='mapping':
        test=tmp_path/'mapping.c'
        test.write_text('''
#include "d3d8_xbox_map.h"
#include <stdio.h>
#define CHECK(x) do { if(!(x))return 1; } while(0)
int main(void) {
    unsigned mask,depth;
    for(mask=0;mask<16;mask++)for(depth=0;depth<4;depth++) {
        DWORD flags=xbox_clear_flags_to_host((mask<<4)|depth);
        unsigned channels=(flags & D3DCLEAR_TARGET) ? 15 : (flags & D3DCLEAR_CHANNEL_MASK)>>4;
        CHECK(channels==mask);
        CHECK(!!(flags & D3DCLEAR_ZBUFFER)==!!(depth & 1));
        CHECK(!!(flags & D3DCLEAR_STENCIL)==!!(depth & 2));
    }
    CHECK(xbox_clear_flags_to_host(0x80)==D3DCLEAR_CHANNEL_ALPHA);
    CHECK(xbox_clear_flags_to_host(0x70)==(D3DCLEAR_CHANNEL_RED|D3DCLEAR_CHANNEL_GREEN|D3DCLEAR_CHANNEL_BLUE));
    puts("PASS: alpha-only reset, RGB-only preservation and independent depth/stencil clear flags");
    return 0;
}
''')
        sources=[test]
    else:
        sources=[RUNTIME/'tests/d3d8_capture/test_main.c',source/'hle/d3d8_capture.c']
    exe=tmp_path/('clear.exe' if os.name=='nt' else 'clear')
    if os.name=='nt':
        command=[compiler,'/nologo','/std:c11','/O2',*('/I'+str(p) for p in includes),
                 *map(str,sources),'/Fe:'+str(exe)]
    else:
        command=[compiler,'-std=c11','-O2',*('-I'+str(p) for p in includes),
                 *map(str,sources),'-o',str(exe)]
    built=subprocess.run(command,cwd=tmp_path,env=environment,capture_output=True,text=True,timeout=60)
    assert built.returncode==0,built.stdout+built.stderr
    result=subprocess.run([str(exe)],cwd=tmp_path,capture_output=True,text=True,timeout=10)
    assert result.returncode==0,result.stdout+result.stderr
