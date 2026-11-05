"""Run actual native-HUD scene/depth ownership with asset-free captured draws."""
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys

from PIL import Image
import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]


def hud_capture(*, hint=True, partial=False, stencil_after=False, stencil_value=0xa5):
    chunks = []

    def chunk(kind, payload=b''):
        chunks.append(struct.pack('<II', kind, len(payload)) + payload + b'\0' * (-len(payload) % 4))

    def words(kind, *values):
        chunk(kind, struct.pack('<' + 'I' * len(values), *values))

    def draw(vertices, color, depth):
        payload = b''.join(struct.pack('<4fI', x, y, depth, 1, color) for x, y in vertices)
        chunk(9, struct.pack('<4I', 4, len(vertices) // 3, 20, len(payload)) + payload)

    def rectangle(x, y, width, height, color, depth):
        draw(((x, y), (x + width, y), (x, y + height),
              (x, y + height), (x + width, y), (x + width, y + height)), color, depth)

    chunk(1)
    chunk(2, struct.pack('<IIIfI', 0, 7, 0xff0000ff, 1, stencil_value))
    chunk(6, struct.pack('<4I2f', 0, 0, 32 if partial else 64, 64, 0, 1))
    for key, value in ((7, 1), (14, 1), (22, 1), (27, 0), (15, 0), (52, 0), (137, 0)):
        words(3, key, value)
    for key, value in ((1, 2), (2, 0), (4, 2), (5, 0)):
        words(4, 0, key, value)
    words(8, 0x44)
    words(19, 0)
    draw(((0, 0), (64, 0), (0, 64)), 0xffff0000, .25)
    if hint:
        words(28, 6, 0x0014f0b0)
    for key, value in ((53,1),(54,1),(55,2),(56,3),(57,stencil_value),(58,255),(59,255),(52,1)):
        words(3,key,value)
    # The first marker is behind world geometry; the second is over clear sky.
    rectangle(8, 8, 8, 8, 0xff00ff00, .75)
    rectangle(44, 44, 8, 8, 0xff00ff00, .75)
    words(3, 7, 0)
    # One-pixel stems isolate native HUD rasterisation from the world filter.
    for x in (42, 44, 46, 48, 50, 52, 54, 56):
        rectangle(x, 30, 1, 12, 0xffffffff, 0)
    if stencil_after:
        # A later stencil read observes the zero written by the visible marker.
        words(3,57,0)
        rectangle(42,42,12,12,0xff00ffff,0)
    if hint:
        words(28, 7, 0x0014f0b0)
    return struct.pack('<8s6I', b'XBRD3D8', 8, 32, 1, 64, 64, len(chunks)) + b''.join(chunks)


@pytest.fixture(scope='module')
def replay(tmp_path_factory):
    if os.name != 'nt':
        pytest.skip('Real Windows D3D11 renderer; native Vulkan GPU validation remains separate')
    folder = tmp_path_factory.mktemp('native-hud-build')
    _, env = native_compiler(folder)
    env['PATH'] = str(Path(sys.executable).parent) + os.pathsep + env['PATH']
    cmake = shutil.which('cmake', path=env['PATH'])
    assert cmake
    (folder / 'CMakeLists.txt').write_text('''
cmake_minimum_required(VERSION 3.20)
project(native_hud_contract C CXX)
set(CMAKE_C_STANDARD 11)
add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
set(XBOXRECOMP_VULKAN OFF CACHE BOOL "" FORCE)
add_subdirectory("${TSFP_RUNTIME_DIR}/src/platform" platform)
add_subdirectory("${TSFP_RUNTIME_DIR}/src/config" config)
add_subdirectory("${TSFP_RUNTIME_DIR}/src/d3d" d3d)
add_subdirectory("${TSFP_RUNTIME_DIR}/src/replay" replay)
add_executable(developer_overlay_replay "${TSFP_OVERLAY_TEST}"
    "${TSFP_RUNTIME_DIR}/src/hle/d3d8_capture.c"
    "${TSFP_RUNTIME_DIR}/src/kernel/nv2a_vsh.c"
    "${TSFP_SHARED_SOURCE}/textures/texture_mode.c")
target_include_directories(developer_overlay_replay PRIVATE
    "${TSFP_RUNTIME_DIR}/src/hle" "${TSFP_RUNTIME_DIR}/src/kernel"
    "${TSFP_SHARED_SOURCE}/interface" "${TSFP_SHARED_SOURCE}/input"
    "${TSFP_SHARED_SOURCE}/runtime")
target_compile_definitions(developer_overlay_replay PRIVATE
    TSFP_REPLAY_SOURCE="${TSFP_RUNTIME_DIR}/src/replay/d3d8_replay.c")
target_link_libraries(developer_overlay_replay PRIVATE xbox_d3d8 user32)
''', encoding='utf-8')
    build = folder / 'build'
    for args in (
        [cmake, '-S', str(folder), '-B', str(build), '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
         '-DTSFP_RUNTIME_DIR=' + (ROOT / 'external/ts3FPrecomp').as_posix(),
         '-DTSFP_SHARED_SOURCE=' + (ROOT / 'src').as_posix(),
         '-DTSFP_OVERLAY_TEST=' + (ROOT / 'tests/graphics/developer_overlay_replay.c').as_posix(),
         '-DTSFP_PC_CONTROLS_DIR=' + (ROOT / 'src').as_posix()],
        [cmake, '--build', str(build), '--target', 'd3d8_replay', 'developer_overlay_replay', '--parallel', '4'],
    ):
        result = subprocess.run(args, cwd=folder, env=env, capture_output=True, text=True, timeout=180)
        assert result.returncode == 0, result.stdout + result.stderr
    return build / 'replay/d3d8_replay.exe', env


def render(replay, folder, scale, samples=1, edge=0, smaa=0, **capture_options):
    exe, compiler_env = replay
    folder.mkdir()
    (folder / 'frame.d3dcap').write_bytes(hud_capture(**capture_options))
    (folder / 'display.ini').write_text('TSFP_DISPLAY_V1 0 0\n')
    (folder / 'runtime.ini').write_text('resolution_scale = 1\n')
    (folder / 'pc-graphics.ini').write_text(f'TSFP_GRAPHICS_V2 {samples} {edge} 1 0 0 {scale}\n')
    env = {k: v for k, v in compiler_env.items() if not k.startswith(('RECOMP_', 'TSFP_'))}
    env.update(RECOMP_DISPLAY_CONFIG=str(folder/'runtime.ini'), TSFP_DISPLAY_CONFIG=str(folder/'display.ini'),
               RECOMP_INPUT_CONFIG=str(folder/'controls.json'), RECOMP_SAVE_DIR=str(folder/'saves'),
               APPDATA=str(folder/'appdata'), RECOMP_SHADER_CACHE='0', TSFP_SMAA=str(smaa))
    result = subprocess.run([str(exe), str(folder/'frame.d3dcap'), '--out', str(folder/'out-'),
                             '--loops', '3', '--dump-every', '--backend', 'd3d11'], cwd=folder,
                            env=env, capture_output=True, text=True, timeout=45)
    log = result.stdout + result.stderr
    assert result.returncode == 0, log
    assert log.count('0 host calls failed, 0 malformed chunks, 0 unmapped handles, 0 unknown chunks') == 3, log
    images = [Image.open(folder/f'out-{i:03}.bmp').convert('RGB') for i in range(3)]
    assert images[0].tobytes() == images[1].tobytes() == images[2].tobytes(), 'HUD targets or depth leaked across frames'
    return images[-1], log


@pytest.mark.parametrize('scale,samples,edge,smaa', [(1,1,0,0), (2,1,0,0), (3,1,0,0), (2,4,0,0), (3,4,1,0), (1,1,0,1), (3,4,0,1)])
def test_native_hud_pixels_depth_and_frame_reset(replay, tmp_path, scale, samples, edge, smaa):
    baseline, _ = render(replay, tmp_path/'baseline', 0)
    image, log = render(replay, tmp_path/'native', scale, samples, edge, smaa)
    assert '[NATIVE-HUD] World' in log, log
    assert image.size == (64, 64)
    assert image.getpixel((10,10)) == (255,0,0), 'Occluded HUD marker lost world depth'
    assert image.getpixel((48,48)) == (0,255,0), 'Visible HUD marker lost depth/colour'
    assert image.crop((40,28,62,44)).tobytes() == baseline.crop((40,28,62,44)).tobytes(), 'Native stems blurred by supersampling or post-HUD FXAA/SMAA'


def test_native_hud_partial_view_fallback(replay, tmp_path):
    image, _ = render(replay, tmp_path/'fallback', 3, partial=True)
    assert image.size == (128,128), 'Partial views must retain the supersampled scene'


@pytest.mark.parametrize('samples,stencil_value',[(1,0xa5),(4,0x5a)])
def test_native_hud_stencil_history(replay,tmp_path,samples,stencil_value):
    image, _ = render(replay,tmp_path/'stencil',3,samples,stencil_after=True,stencil_value=stencil_value)
    assert image.size==(64,64)
    assert image.getpixel((48,48))==(0,255,255), 'Later HUD draw lost the preceding stencil write'
    assert image.getpixel((43,43))==(0,0,255), 'Stencil history leaked outside the preceding marker'
    assert image.getpixel((10,10))==(255,0,0), 'Depth/stencil copy changed the occluded marker'


def test_hint_free_capture_retains_supersampled_scene(replay, tmp_path):
    image, log = render(replay, tmp_path/'ordinary', 2, hint=False)
    assert image.size == (96,96) and '[NATIVE-HUD]' not in log


@pytest.mark.parametrize('samples',[1,4])
def test_developer_overlay_is_above_completed_scene(replay,tmp_path,samples):
    executable,env=replay
    overlay=(executable.parent.parent/'developer_overlay_replay.exe',env)
    baseline,_=render(replay,tmp_path/'baseline',2,samples=samples)
    scene,log=render(overlay,tmp_path/'developer',2,samples=samples)
    # DXGI flip invalidates the just-presented page. Inspect the actual final
    # target immediately BEFORE Present, rather than its next-buffer contents.
    image=Image.open(tmp_path/'developer/developer-before-present.bmp').convert('RGB')
    def white_pixels(im):
        data=im.tobytes()
        return sum(data[i]>200 and data[i+1]>200 and data[i+2]>200 for i in range(0,len(data),3))
    assert white_pixels(image)>8, 'Developer Tools glyphs lost during scene resolve\n'+log[-5000:]
    assert log.count('before present white=')==3, 'Every actual presentation must contain the overlay'
    assert scene.tobytes()==baseline.tobytes(), 'Host tools leaked into guest scene/captures'
