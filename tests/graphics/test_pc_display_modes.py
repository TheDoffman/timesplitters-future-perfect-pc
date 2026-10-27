"""Compile and exercise the Windows display model with isolated settings."""


from pathlib import Path
import subprocess
import sys

import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.skipif(sys.platform != 'win32', reason='Windows display adapter test')
def test_primary_display_modes(tmp_path):
    compiler, environment = native_compiler(tmp_path)
    executable = tmp_path / 'display-modes.exe'
    subprocess.run([compiler, '/nologo', '/std:c11', '/O2', '/MD',
                    str(ROOT / 'tests/graphics/pc_display_modes.c'),
                    '/Fe:' + str(executable), '/Fo:' + str(tmp_path / 'modes.obj'),
                    '/link', 'user32.lib'], check=True, cwd=tmp_path, env=environment)
    subprocess.run([str(executable), str(tmp_path / 'display.ini')], check=True,
                   cwd=tmp_path, timeout=10)


def test_auto_drawable_policy(tmp_path):
    """Compile production shared policy with deterministic SDL host snapshots."""
    compiler, environment = native_compiler(tmp_path)
    header = tmp_path / 'SDL3/SDL.h'
    header.parent.mkdir()
    header.write_text(r'''#include <stdint.h>
typedef uint64_t Uint64;
typedef struct {int value;} SDL_AtomicInt;
static void SDL_SetAtomicInt(SDL_AtomicInt *p,int v){p->value=v;}
static int SDL_GetAtomicInt(SDL_AtomicInt *p){return p->value;}
static Uint64 fake_ticks;
static Uint64 SDL_GetTicks(void){return fake_ticks;}
typedef unsigned SDL_DisplayID;
typedef void SDL_Window;
typedef struct {int w,h;} SDL_DisplayMode;
static SDL_DisplayID SDL_GetDisplayForWindow(SDL_Window *w){(void)w;return 1;}
static SDL_DisplayID SDL_GetPrimaryDisplay(void){return 1;}
static const SDL_DisplayMode *SDL_GetDesktopDisplayMode(SDL_DisplayID d){(void)d;return NULL;}
static SDL_DisplayMode **SDL_GetFullscreenDisplayModes(SDL_DisplayID d,int *n){(void)d;*n=0;return NULL;}
#define SDL_free free
''')
    executable = tmp_path / ('auto.exe' if sys.platform == 'win32' else 'auto')
    source = ROOT / 'tests/graphics/pc_display_auto.c'
    includes = [tmp_path, ROOT / 'src/native']
    if sys.platform == 'win32':
        command = [compiler, '/nologo', '/std:c11', '/O2', '/MD',
                   *['/I'+str(p) for p in includes], str(source),
                   '/Fe:'+str(executable), '/Fo:'+str(tmp_path/'auto.obj')]
    else:
        command = [compiler, '-std=c11', '-O2', *['-I'+str(p) for p in includes],
                   str(source), '-lm', '-o', str(executable)]
    subprocess.run(command, check=True, cwd=tmp_path, env=environment, timeout=60)
    environment['TSFP_DISPLAY_CONFIG'] = str(tmp_path / 'auto.ini')
    subprocess.run([str(executable)], check=True, cwd=tmp_path, env=environment, timeout=10)
