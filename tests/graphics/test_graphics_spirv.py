"""Compile production SSAO HLSL through the native Vulkan DXC adapter.

Set VULKAN_SDK to a provisioned SDK. Missing SDKs skip local runs; a configured
SDK with broken headers, libraries, or shaders fails rather than silently skips.
This verifies shader compilation, not rendering on a Vulkan device.
"""
import os
from pathlib import Path
import shutil
import subprocess
import sys

import pytest

ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.skipif(sys.platform not in ('linux', 'darwin'), reason='native POSIX DXC adapter')
def test_graphics_spirv(tmp_path):
    sdk_value = os.environ.get('VULKAN_SDK')
    if not sdk_value:
        pytest.skip('VULKAN_SDK is required for native shader compilation')
    sdk = Path(sdk_value)
    headers = sdk / 'include' / 'dxc'
    library = sdk / 'lib' / ('libdxcompiler.dylib' if sys.platform == 'darwin' else 'libdxcompiler.so')
    assert (headers / 'dxcapi.h').is_file(), f'Missing DXC headers: {headers}'
    assert library.is_file(), f'Missing DXC library: {library}'
    compiler = shutil.which(os.environ.get('CXX', 'clang++'))
    assert compiler, 'A C++ compiler is required with a configured Vulkan SDK'
    runtime = ROOT / 'external' / 'ts3FPrecomp' / 'src' / 'd3d'
    executable = tmp_path / 'graphics-spirv'
    args = [compiler, '-std=c++17', '-pthread',
            *['-I' + str(p) for p in (headers, ROOT / 'src', runtime)],
            '-DRHI_DXC_LIBRARY="' + library.as_posix() + '"',
            str(ROOT / 'tests/graphics/graphics_spirv.cpp'),
            str(runtime / 'rhi_vulkan_dxc.cpp'), '-o', str(executable)]
    if sys.platform == 'linux':
        args.append('-ldl')
    result = subprocess.run(args, cwd=tmp_path, capture_output=True, text=True, timeout=90)
    assert result.returncode == 0, result.stdout + result.stderr
    result = subprocess.run([str(executable)], cwd=tmp_path, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    for samples in (1, 2, 4, 8):
        for entry in ('vs_main', 'ps_main', 'composite_main', 'multiply_main'):
            assert f'PASS SPIR-V AO depth{samples} {entry}' in result.stdout
