"""Exercise actual queued float overrides, capture bytes and replay validation."""
import os
from pathlib import Path
import subprocess

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[1]


def test_ordered_combiner_constants(tmp_path):
    runtime = ROOT / 'external/ts3FPrecomp/src'
    record = (runtime / 'hle/hle_d3d8_record.c').read_text(encoding='utf-8')
    state = (runtime / 'hle/hle_d3d8_state.c').read_text(encoding='utf-8')

    def function(text, name):
        start = text.index(name)
        return text[start:text.index('\n}', start) + 2]

    replay = (runtime / 'replay/d3d8_replay.c').read_text(encoding='utf-8')
    start = replay.index('    case D3D8CAP_PS_FLOAT_CONSTANT:')
    replay_case = replay[start:replay.index('    case D3D8CAP_PS_TOKEN:', start)]
    body = (ROOT / 'tests/combiner_constants.c').read_text(encoding='utf-8')
    body = body.replace('/* PRODUCTION */', '\n'.join([
        function(record, 'static void rec_ps_float_constant('),
        function(record, 'static void op_ps_float_constant('),
        function(record, 'void host_combiners_set_float_constant('),
        function(state, 'void hle_d3d8_surface_coefficients('),
        function(state, 'void hle_d3d8_surface_bias_step('),
        function(state, 'static void forward_surface_bias(')]))
    body = body.replace('/* REPLAY_CASE */', replay_case)
    source = tmp_path / 'constants.c'
    source.write_text(body, encoding='utf-8')
    compiler, env = native_compiler(tmp_path)
    includes = [ROOT / 'src', runtime / 'hle']
    sources = [source, runtime / 'hle/d3d8_capture.c']
    exe = tmp_path / ('constants.exe' if os.name == 'nt' else 'constants')
    if os.name == 'nt':
        command = [compiler, '/nologo', '/std:c11', '/O2',
                   *('/I' + str(p) for p in includes), *map(str, sources), '/Fe:' + str(exe)]
    else:
        command = [compiler, '-std=c11', '-O2', *('-I' + str(p) for p in includes),
                   *map(str, sources), '-lm', '-o', str(exe)]
    built = subprocess.run(command, env=env, cwd=tmp_path, capture_output=True, text=True, timeout=60)
    assert built.returncode == 0, built.stdout + built.stderr
    ran = subprocess.run([str(exe)], cwd=tmp_path, capture_output=True, text=True, timeout=10)
    assert ran.returncode == 0, ran.stdout + ran.stderr
    print(ran.stdout)
