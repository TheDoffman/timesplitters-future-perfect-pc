"""Readback must distinguish rendered output from stale/empty/flipped images."""


from pathlib import Path
import os
import shutil
import struct
import subprocess
import sys
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]/'tools'))
import frame_replay_contract as contract


def bmp(path, *, top_down=False, bits=24, wrong=False):
    stride = 64 * (bits // 8)
    pixels = bytearray(stride * 64)
    for x, y, color in ((8, 8, b'\0\0\xff'), (56, 56, b'\xff\0\0')):
        row = y if top_down else 63-y
        start = row*stride + x*(bits//8)
        pixels[start:start+3] = b'\0\0\0' if wrong else color
    header = struct.pack('<2sIHHI', b'BM', 54+len(pixels), 0, 0, 54)
    header += struct.pack('<IiiHHIIiiII', 40, 64, -64 if top_down else 64,
                          1, bits, 0, len(pixels), 0, 0, 0, 0)
    path.write_bytes(header+pixels)


@pytest.mark.parametrize('top_down,bits', [(False,24),(True,24),(False,32),(True,32)])
def test_readback_orientation_and_channels(tmp_path, top_down, bits):
    path=tmp_path/'frame.bmp'
    bmp(path,top_down=top_down,bits=bits)
    contract.verify_triangle(path)
    bmp(path,top_down=top_down,bits=bits,wrong=True)
    with pytest.raises(RuntimeError,match='pixel'):
        contract.verify_triangle(path)


def test_truncated_readback_rejected(tmp_path):
    path=tmp_path/'frame.bmp'
    bmp(path)
    path.write_bytes(path.read_bytes()[:-1])
    with pytest.raises(RuntimeError,match='Truncated'):
        contract.verify_triangle(path)


def test_successful_process_without_new_readback_cannot_pass(tmp_path, monkeypatch):
    image=tmp_path/'triangle-000.bmp'
    bmp(image)
    def no_readback(args, **kwargs):
        env=kwargs['env']
        assert kwargs['timeout']==30
        assert env['RECOMP_DISPLAY_CONFIG']==str(tmp_path/'runtime.ini')
        assert env['TSFP_DISPLAY_CONFIG']==str(tmp_path/'display.ini')
        assert env['RECOMP_INPUT_CONFIG']==str(tmp_path/'controls.json')
        assert env['RECOMP_SAVE_DIR']==str(tmp_path/'saves')
        return contract.subprocess.CompletedProcess(args,0,'','')
    monkeypatch.setattr(contract.subprocess,'run',no_readback)
    with pytest.raises(FileNotFoundError):
        contract.run('unused',tmp_path,{'TSFP_DISPLAY_CONFIG':'personal.ini'})


@pytest.mark.skipif(os.name != 'nt', reason='Windows replay CMake dependency regression')
def test_windows_replay_links_without_input_library(tmp_path):
    """Build the production renderer/replay with texture packs but no game/input."""
    from native_compiler import native_compiler
    root = Path(__file__).resolve().parents[2]
    runtime = root / 'external/ts3FPrecomp'
    _, environment = native_compiler(tmp_path)
    # The publisher invokes .venv Python directly, without activating its PATH.
    environment['PATH'] = str(Path(sys.executable).parent) + os.pathsep + environment['PATH']
    cmake = shutil.which('cmake', path=environment['PATH'])
    assert cmake, 'Install the pinned CMake dependency from requirements.txt'
    # No generated game code, SDL, original assets or input library is needed.
    (tmp_path / 'CMakeLists.txt').write_text('''
cmake_minimum_required(VERSION 3.20)
project(replay_link_regression C CXX)
set(CMAKE_C_STANDARD 11)
add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
set(XBOXRECOMP_VULKAN OFF CACHE BOOL "" FORCE)
add_subdirectory("${TSFP_RUNTIME_DIR}/src/platform" platform)
add_subdirectory("${TSFP_RUNTIME_DIR}/src/config" config)
add_subdirectory("${TSFP_RUNTIME_DIR}/src/d3d" d3d)
add_subdirectory("${TSFP_RUNTIME_DIR}/src/replay" replay)
''', encoding='utf-8')
    build = tmp_path / 'build'
    commands = [
        [cmake, '-S', str(tmp_path), '-B', str(build), '-G', 'Ninja',
         '-DCMAKE_BUILD_TYPE=Release', '-DTSFP_RUNTIME_DIR=' + runtime.as_posix(),
         '-DTSFP_SHARED_SOURCE=' + (root / 'src').as_posix(),
         '-DTSFP_PC_CONTROLS_DIR=' + (root / 'src').as_posix()],
        [cmake, '--build', str(build), '--target', 'd3d8_replay', '--parallel', '4'],
    ]
    for command in commands:
        result = subprocess.run(command, cwd=tmp_path, env=environment,
                                capture_output=True, text=True, timeout=180)
        assert result.returncode == 0, result.stdout + result.stderr
    result = subprocess.run([str(build / 'replay/d3d8_replay.exe')], cwd=tmp_path,
                            env=environment, capture_output=True, text=True, timeout=10)
    assert result.returncode == 2 and 'usage: d3d8_replay' in result.stderr
