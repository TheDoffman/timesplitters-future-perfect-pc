"""GPU regression for shared scene depth and render-target feedback.

Uses synthetic coloured quads only: no game files or personal settings.
Run after building d3d8_replay. Pixel checks cover occlusion, feedback refresh,
resolution changes and supersampling through the production D3D8 renderer.
"""

# Direct-command imports share the repository's grouped helper paths.
import sys as _layout_sys
from pathlib import Path as _LayoutPath
_layout_tools = _LayoutPath(__file__).resolve().parents[2] / 'tools'
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

from pathlib import Path
import os
import struct
import subprocess
import sys
import tempfile


def capture(path, width, height):
    chunks = []
    def chunk(kind, data=b''):
        chunks.append(struct.pack('<II', kind, len(data)) + data + bytes((-len(data)) % 4))
    def words(kind, *values):
        chunk(kind, struct.pack('<' + 'I' * len(values), *values))
    def state(key, value):
        words(3, key, value)
    def viewport(w, h):
        chunk(6, struct.pack('<4I2f', 0, 0, w, h, 0., 1.))
    def clear(flags):
        chunk(2, struct.pack('<3IfI', 0, flags, 0xff000000, 1., 0))
    def quad(right, z, color):
        data = b''.join(struct.pack('<4fI2f', x, y, z, 1., color, u, v)
                        for x, y, u, v in [(0, 0, 0, 0), (right, 0, 1, 0),
                                           (0, height, 0, 1), (right, height, 1, 1)])
        chunk(9, struct.pack('<4I', 5, 2, 28, len(data)) + data)
    words(1)
    words(8, 0x144)  # XYZRHW, diffuse, one texture coordinate
    words(19, 0)
    words(28, 1, 0)  # full-canvas placement
    for k, v in [(7, 1), (14, 1), (23, 4), (22, 1), (27, 0), (15, 0),
                 (28, 0), (52, 0), (137, 0), (168, 15)]:
        state(k, v)
    for k, v in [(1, 2), (2, 0), (4, 2), (5, 0)]:
        words(4, 0, k, v)
    words(4, 1, 1, 1)
    viewport(width, height)
    clear(7)
    quad(width / 2, .25, 0xffff0000)  # wall in front of the left half
    # Swizzled A8R8G8B8 requires power-of-two dimensions. A 64x48 allocation
    # lets UnlockRect's Morton addressing read beyond the texture storage.
    tw, th = 64, 64
    payload = struct.pack('<9I', 1, 0x06, tw, th, 1, 1, tw * 4, th, tw * th * 4)
    chunk(11, payload + bytes(tw * th * 4))
    words(21, 1, 0, 0, 0xffffffff)  # retain the SAME scene depth
    viewport(tw, th)
    clear(1)  # colour only, as the ghost effect does
    quad(width, .5, 0xff00ff00)
    # Sample our own colour target twice, halving it each time. The second
    # draw must see the first draw's result, not an old or null texture.
    state(7, 0)
    state(14, 0)
    words(7, 0, 1)
    for k, v in [(1, 4), (2, 2), (3, 0)]:
        words(4, 0, k, v)
    quad(width, .5, 0xff808080)
    quad(width, .5, 0xff808080)
    words(21, 0, 0, 0, 0xffffffff)
    viewport(width, height)
    words(4, 0, 1, 2)
    quad(width, .5, 0xffffffff)
    # The offscreen model's depth writes must survive returning to the scene.
    # A new blue object behind both the wall and model must remain hidden.
    words(7, 0, 0)
    words(4, 0, 2, 0)
    state(7, 1)
    state(14, 1)
    quad(width, .75, 0xff0000ff)
    header = struct.pack('<8s6I', b'XBRD3D8\0', 8, 32, 1, width, height, len(chunks))
    path.write_bytes(header + b''.join(chunks))


def check_bitmap(path, width, height):
    data = path.read_bytes()
    offset = struct.unpack_from('<I', data, 10)[0]
    w, h = struct.unpack_from('<ii', data, 18)
    bits = struct.unpack_from('<H', data, 28)[0]
    assert (w, abs(h)) == (width, height) and bits in (24, 32), (w, h, bits)
    stride = ((w * bits + 31) // 32) * 4
    for fraction, expected in [(0.25, (0, 0, 0)), (0.75, (0, 64, 0))]:
        x, y = int(w * fraction), abs(h) // 2
        if h > 0:
            y = h - 1 - y
        pixel = data[offset + y * stride + x * (bits // 8):][:3]
        assert all(abs(a - b) <= 3 for a, b in zip(pixel, expected)), (fraction, tuple(pixel), expected)


def main(executable):
    executable = Path(executable).resolve()
    assert executable.is_file(), executable
    with tempfile.TemporaryDirectory(prefix='tsfp-ghost-targets-') as name:
        folder = Path(name)
        env = {k: v for k, v in os.environ.items() if not k.startswith(('RECOMP_', 'TSFP_', 'XDG_'))}
        for key in ('APPDATA', 'XDG_CONFIG_HOME', 'XDG_DATA_HOME'):
            env[key] = str(folder)
        env.update(RECOMP_DISPLAY_CONFIG=str(folder / 'runtime.ini'),
                   TSFP_DISPLAY_CONFIG=str(folder / 'display.ini'))
        (folder / 'display.ini').write_text('')
        for width, height, scale in [(128, 96, 1), (160, 90, 1), (344, 144, 1), (160, 90, 2)]:
            (folder / 'runtime.ini').write_text(f'resolution_scale = {scale}\nfullscreen = 0\n')
            source = folder / 'synthetic.d3dcap'
            capture(source, width, height)
            result = subprocess.run([str(executable), str(source), '--out', str(folder / 'out'), '--list-draws'], cwd=folder, env=env,
                                    capture_output=True, text=True, timeout=30)
            log = result.stdout + result.stderr
            assert result.returncode == 0 and '0 host calls failed' in log, f'exit={result.returncode}\n{log}'
            check_bitmap(folder / 'out000.bmp', width * scale, height * scale)
            print(f'PASS: shared depth and fresh feedback at {width}x{height}, scale {scale}')


if __name__ == '__main__':
    main(sys.argv[1])
