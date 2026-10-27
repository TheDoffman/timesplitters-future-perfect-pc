"""Shared keyboard reload delivery/priority; no game assets or host UI needed."""
import os
from pathlib import Path
import subprocess
import pytest
import re

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[1]


@pytest.mark.parametrize('mutation', [None, 'drop-taps', 'held-fire', 'drop-fire-taps'])
def test_reload_keys(tmp_path, mutation):
    compiler, env = native_compiler(tmp_path)
    output = tmp_path / ('reload.exe' if os.name == 'nt' else 'reload')
    source = ROOT / 'tests/reload_keys.c'
    include = ROOT / 'src'
    if mutation:
        include = tmp_path / 'input'
        include.mkdir()
        for name in ('pc_input.h', 'pc_input.c', 'pc_reload_input.h'):
            text = (ROOT / 'src' / name).read_text()
            if name == 'pc_reload_input.h':
                replacements = {
                    'drop-taps': ('pressed=1;', 'pressed=0;'),
                    'held-fire': ('r->blocked_fire=(*fire?2:0)|(*secondary?4:0);', 'r->blocked_fire=0;'),
                    'drop-fire-taps': ('if(pressed_fire&2)r->fire_pulse_until[0]=now+34;',
                                       'if(pressed_fire&2)r->fire_pulse_until[0]=now;'),
                }
                old, new = replacements[mutation]
                assert text.count(old) == 1
                text = text.replace(old, new)
            (include / name).write_text(text)
    args = ([compiler, '/nologo', '/std:c11', '/W4', '/I' + str(include),
             str(source), '/Fe:' + str(output)] if os.name == 'nt' else
            [compiler, '-std=c11', '-Wall', '-Wextra', '-I' + str(include),
             str(source), '-lm', '-o', str(output)])
    compiled = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True)
    assert compiled.returncode == 0, compiled.stdout + compiled.stderr
    result = subprocess.run([str(output)], capture_output=True, text=True, timeout=20)
    if mutation:
        assert result.returncode != 0, 'Regression fixture accepted ' + mutation
    else:
        assert result.returncode == 0, result.stdout + result.stderr


def test_sdl_reload_event_bridge(tmp_path):
    adapter = (ROOT / 'src/native/input_sdl.c').read_text()
    body = re.search(r'static void reload_key_event\(.*?\n\}', adapter, re.S).group()
    source = tmp_path / 'sdl-reload.c'
    source.write_text('''#include <assert.h>
#include "pc_input.h"
static PcInput camera_input;
static int focused=1,camera_lock;
#define atomic_load(p) (*(p))
#define pthread_mutex_lock(p) ((void)(p))
#define pthread_mutex_unlock(p) ((void)(p))
#define SDL_GetTicks() 100u
''' + body + '''
int main(void) {
    pc_reload_context(&camera_input.reload,1,1,0,100);
    reload_key_event('R',1);reload_key_event('R',0);
    assert(camera_input.reload.events['R']==3 && !camera_input.keys['R']);
    assert(!camera_input.focused); /* Key mirroring must not alter camera focus. */
    unsigned char roles[256]={0};roles['R']=1;
    int r=0,f=255,a=0;
    pc_reload_sample(&camera_input.reload,roles,1,101,&r,&f,&a);
    assert(r==255 && f==0);
    focused=0;reload_key_event('R',1);assert(!camera_input.keys['R']);
    focused=1;reload_key_event(0xa0,1);reload_key_event(0xa1,1);
    reload_key_event(0xa0,0);assert(camera_input.keys[0x10]);
    assert(camera_input.reload.events[0x10]==1);
    reload_key_event(0xa1,0);assert(!camera_input.keys[0x10]);
    assert(camera_input.reload.events[0x10]==3);
    reload_key_event(999,1);
    return 0;
}
''')
    compiler, env = native_compiler(tmp_path)
    output = tmp_path / ('sdl-reload.exe' if os.name == 'nt' else 'sdl-reload')
    command = ([compiler, '/nologo', '/std:c11', '/W4', '/I' + str(ROOT / 'src'),
                str(source), '/Fe:' + str(output)] if os.name == 'nt' else
               [compiler, '-std=c11', '-Wall', '-Wextra', '-I' + str(ROOT / 'src'),
                str(source), '-o', str(output)])
    compiled = subprocess.run(command, cwd=tmp_path, env=env, capture_output=True, text=True)
    assert compiled.returncode == 0, compiled.stdout + compiled.stderr
    result = subprocess.run([str(output)], capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, result.stdout + result.stderr
