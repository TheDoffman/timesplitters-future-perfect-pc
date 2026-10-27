"""Asset-free capture/readback contract for the production frame replay tool."""

# Direct-command imports share the repository's grouped helper paths.
import sys as _layout_sys
from pathlib import Path as _LayoutPath
_layout_tools = _LayoutPath(__file__).resolve().parents[1]
_layout_root = _layout_tools.parent
if (_layout_tools / 'project_paths.py').is_file():
    _layout_sys.path.insert(0, str(_layout_tools))
    from project_paths import configure as _configure_paths, source_file as repo_file
    _configure_paths()
else:
    # Standalone source fixtures retain their original flat-folder behavior.
    _layout_root = _LayoutPath(__file__).resolve().parents[1]
    def repo_file(root, area, name):
        return _LayoutPath(root) / area / name

import struct
import subprocess
from pathlib import Path


def triangle_capture():
    """Red upper-left triangle on a blue 64x64 target, capture format v8."""
    chunks = []

    def chunk(kind, payload=b''):
        chunks.append(struct.pack('<II', kind, len(payload)) + payload
                      + b'\0' * (-len(payload) % 4))

    def words(kind, *values):
        chunk(kind, struct.pack('<' + 'I' * len(values), *values))

    chunk(1)
    chunk(2, struct.pack('<IIIfI', 0, 1, 0xff0000ff, 1, 0))
    chunk(6, struct.pack('<4I2f', 0, 0, 64, 64, 0, 1))
    for key, value in ((7, 0), (22, 1), (27, 0), (15, 0), (137, 0)):
        words(3, key, value)  # depth, culling, blending, alpha test, lighting
    for key, value in ((1, 2), (2, 0), (4, 2), (5, 0)):
        words(4, 0, key, value)  # select diffuse color and alpha
    words(8, 0x44)  # XYZRHW | DIFFUSE
    words(19, 0)  # fixed-function pixel shader
    vertices = b''.join(struct.pack('<4fI', x, y, 0, 1, 0xffff0000)
                        for x, y in ((0, 0), (64, 0), (0, 64)))
    chunk(9, struct.pack('<4I', 4, 1, 20, len(vertices)) + vertices)
    return struct.pack('<8s6I', b'XBRD3D8', 8, 32, 1, 64, 64, len(chunks)) + b''.join(chunks)


def verify_triangle(path):
    data = Path(path).read_bytes()
    if len(data) < 54 or data[:2] != b'BM':
        raise RuntimeError('Replay did not produce a BMP')
    offset = struct.unpack_from('<I', data, 10)[0]
    width, height, planes, bits, compression = struct.unpack_from('<iiHHI', data, 18)
    if (width, abs(height), planes, compression) != (64, 64, 1, 0) or bits not in (24, 32):
        raise RuntimeError('Unexpected replay BMP dimensions or format')
    stride = ((width * bits + 31) // 32) * 4
    if offset < 54 or len(data) < offset + stride * abs(height):
        raise RuntimeError('Truncated replay BMP')
    for x, y, expected in ((8, 8, bytes((0, 0, 255))), (56, 56, bytes((255, 0, 0)))):
        row = y if height < 0 else height - 1 - y
        start = offset + row * stride + x * (bits // 8)
        if data[start:start + 3] != expected:
            raise RuntimeError(f'Replay pixel ({x}, {y}) differs from the red-triangle/blue-clear contract')


def run(executable, directory, environment, backend='vulkan'):
    """One frame, <=30 seconds; caller owns isolated configuration/SDK routing."""
    directory = Path(directory).resolve()
    directory.mkdir(parents=True, exist_ok=True)
    environment = dict(environment)
    environment.update(RECOMP_DISPLAY_CONFIG=str(directory/'runtime.ini'),
                       TSFP_DISPLAY_CONFIG=str(directory/'display.ini'),
                       RECOMP_INPUT_CONFIG=str(directory/'controls.json'),
                       RECOMP_SAVE_DIR=str(directory/'saves'),
                       RECOMP_VK_VALIDATION='1', RECOMP_FRAME_INTERP='1',
                       RECOMP_FPS_CAP='0')
    capture = directory / 'triangle.d3dcap'
    capture.write_bytes(triangle_capture())
    image = directory / 'triangle-000.bmp'
    image.unlink(missing_ok=True)  # Never accept an earlier run's readback.
    result = subprocess.run([str(executable), str(capture), '--backend', backend,
                             '--out', str(directory / 'triangle-')],
                            env=environment, cwd=directory, capture_output=True,
                            text=True, timeout=30)
    output = result.stdout + result.stderr
    (directory / 'replay.log').write_text(output)
    result.check_returncode()
    verify_triangle(image)
    return output
