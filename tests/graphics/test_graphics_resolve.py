import os
import subprocess
from pathlib import Path
import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.skipif(os.name != 'nt', reason='D3D11 WARP pixel test; native Vulkan validation separate')
def test_graphics_resolve(tmp_path):
    compiler, env = native_compiler(tmp_path)
    runtime = ROOT / 'external/ts3FPrecomp/src'
    exe = tmp_path / 'resolve.exe'
    sources = [ROOT / 'tests/graphics/graphics_resolve.c', runtime / 'config/recomp_config.c']
    sources += [runtime / 'd3d' / name for name in (
        'rhi.c', 'rhi_d3d11.c', 'rhi_shader_cache.c', 'd3d8_compile.c', 'd3d8_display.c')]
    args = [compiler, '/nologo', '/std:c11', '/O2', '/D_CRT_SECURE_NO_WARNINGS',
            *['/I' + str(p) for p in (ROOT/'src', runtime, runtime/'d3d', runtime/'config')],
            *map(str, sources), '/Fe:' + str(exe), '/link', 'd3d11.lib', 'dxgi.lib',
            'dxguid.lib', 'd3dcompiler.lib', 'user32.lib', 'gdi32.lib']
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=90)
    assert result.returncode == 0, result.stdout + result.stderr
    env = {k: v for k, v in env.items() if not k.startswith(('RECOMP_', 'TSFP_'))}
    cache_home = tmp_path/'appdata'
    cache_home.mkdir()
    env.update(APPDATA=str(cache_home), TSFP_DISPLAY_CONFIG=str(tmp_path/'display.ini'),
               RECOMP_DISPLAY_CONFIG=str(tmp_path/'runtime.ini'))
    # New processes exercise disabled, cold, and warm disk-cache startup.
    # Only this subprocess sees the temporary APPDATA path.
    for label, enabled in [('disabled', '0'), ('cold', '1'), ('warm', '1')]:
        env['RECOMP_SHADER_CACHE'] = enabled
        result = subprocess.run([str(exe)], cwd=tmp_path, env=env,
                                capture_output=True, text=True, timeout=60)
        assert result.returncode == 0, label + '\n' + result.stdout + result.stderr
        assert 'graphics transition combinations, six consecutive frames each' in result.stdout
        print(label + ': ' + next(line for line in result.stdout.splitlines()
                                 if 'graphics transition combinations' in line))
        entries = list(cache_home.rglob('*.bin'))
        if label == 'disabled':
            assert not entries
        elif label == 'cold':
            assert entries, 'Cold run did not populate the isolated shader cache'
            cache_contents = {p: p.read_bytes() for p in entries}
        else:
            assert {p: p.read_bytes() for p in entries} == cache_contents
