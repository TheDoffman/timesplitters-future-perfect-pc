"""Real cache replacement transaction with injected GPU/allocation failures."""
import os
from pathlib import Path
import subprocess

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[1]


def test_surface_preservation_transaction(tmp_path):
    runtime = ROOT / 'external/ts3FPrecomp/src'
    text = (runtime / 'hle/hle_d3d8_texture.c').read_text(encoding='utf-8')
    start = text.rindex('IDirect3DTexture8 *hle_d3d8_render_texture(')
    function = text[start:text.index('\n}', start) + 2]
    harness = (ROOT / 'tests/surface_preservation.c').read_text(encoding='utf-8')
    source = tmp_path / 'preservation.c'
    for signature in ('void hle_d3d8_surface_precision_rate(', 'static uint32_t surface_host_format('):
        start = text.index(signature)
        function = text[start:text.index('\n}', start) + 2] + '\n' + function
    start = text.index('        /* Convert a changed-precision source')
    read_guard = text[start:text.index('        /* RECOMP_HLE_D3D8_TEX_EVERY_BIND', start)]
    source.write_text(harness.replace('/* PRODUCTION */', function).replace('/* READ_GUARD */', read_guard), encoding='utf-8')
    compiler, env = native_compiler(tmp_path)
    exe = tmp_path / ('preservation.exe' if os.name == 'nt' else 'preservation')
    includes = [runtime, runtime / 'd3d', ROOT / 'src']
    if os.name == 'nt':
        cmd = [compiler, '/nologo', '/std:c11', '/O2', *('/I' + str(p) for p in includes),
               str(source), '/Fe:' + str(exe)]
    else:
        cmd = [compiler, '-std=c11', '-O2', *('-I' + str(p) for p in includes),
               str(source), '-o', str(exe)]
    built = subprocess.run(cmd, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert built.returncode == 0, built.stdout + built.stderr
    ran = subprocess.run([str(exe)], cwd=tmp_path, capture_output=True, text=True, timeout=10)
    assert ran.returncode == 0, ran.stdout + ran.stderr
    print(ran.stdout)
