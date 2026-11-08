"""Synthetic portal-edge coverage through the production D3D11/Vulkan replay.

No game assets: an orange background and a green room behind an Xbox scissor.
The bottom/right samples fail if inclusive guest edges become exclusive too soon.
"""
from pathlib import Path
import os
import struct
import subprocess
import sys
import tempfile


def capture(path, rect):
    chunks = []
    def words(kind, *values):
        data = struct.pack('<' + 'I' * len(values), *values)
        chunks.append(struct.pack('<II', kind, len(data)) + data)
    def chunk(kind, data):
        chunks.append(struct.pack('<II', kind, len(data)) + data)
    words(1)
    chunk(2, struct.pack('<3IfI', 0, 7, 0xffff8000, 1., 0))
    chunk(6, struct.pack('<4I2f', 0, 0, 640, 480, 0., 1.))
    words(8, 0x144)
    words(19, 0)
    words(28, 1, 0)
    for key, value in [(7, 0), (14, 0), (22, 1), (27, 0), (15, 0),
                       (28, 0), (52, 0), (137, 0), (168, 15)]:
        words(3, key, value)
    for key, value in [(1, 2), (2, 0), (4, 2), (5, 0)]:
        words(4, 0, key, value)
    words(4, 1, 1, 1)
    chunk(24, struct.pack('<II4i', 1, 0, *rect))
    vertices = b''.join(struct.pack('<4fI2f', x, y, .5, 1., 0xff00ff00, 0., 0.)
                        for x, y in [(0, 0), (640, 0), (0, 480), (640, 480)])
    chunk(9, struct.pack('<4I', 5, 2, 28, len(vertices)) + vertices)
    path.write_bytes(struct.pack('<8s6I', b'XBRD3D8\0', 10, 32, 1, 640, 480,
                                 len(chunks)) + b''.join(chunks))


def check_bitmap(path, width, height, rect):
    data = path.read_bytes()
    offset = struct.unpack_from('<I', data, 10)[0]
    w, h = struct.unpack_from('<ii', data, 18)
    bits = struct.unpack_from('<H', data, 28)[0]
    assert (w, abs(h)) == (width, height) and bits in (24, 32)
    stride = ((w * bits + 31) // 32) * 4
    # Exact integer arithmetic is independent of the renderer's float conversion.
    left, top = rect[0] * w // 640, rect[1] * abs(h) // 480
    right = ((rect[2] + 1) * w + 639) // 640
    bottom = ((rect[3] + 1) * abs(h) + 479) // 480
    for x, y in [(left, top), (right-1, bottom-1),
                 (left-1, top), (right, bottom-1), (left, bottom)]:
        if not (0 <= x < w and 0 <= y < abs(h)):
            continue
        row = abs(h)-1-y if h > 0 else y
        pixel = tuple(data[offset+row*stride+x*(bits//8):][:3])
        expected = (0, 255, 0) if left <= x < right and top <= y < bottom else (0, 128, 255)
        assert pixel == expected, (width, height, rect, x, y, pixel, expected)


def main(executable, environment=None):
    executable = Path(executable).resolve()
    with tempfile.TemporaryDirectory(prefix='tsfp-portal-scissors-') as name:
        folder = Path(name)
        env = dict(environment) if environment is not None else {
            k:v for k,v in os.environ.items() if not k.startswith(('RECOMP_', 'TSFP_'))}
        env.update(RECOMP_VK_VALIDATION='1', RECOMP_DISPLAY_CONFIG=str(folder/'runtime.ini'),
                   TSFP_DISPLAY_CONFIG=str(folder/'display.ini'))
        for key in ('APPDATA', 'LOCALAPPDATA', 'XDG_CONFIG_HOME', 'XDG_DATA_HOME'):
            env[key] = str(folder)
        (folder/'runtime.ini').write_text('resolution_scale = 1\nfullscreen = 0\n')
        for width, height in [(640,480), (1280,720), (1920,1080), (3440,1440)]:
            (folder/'display.ini').write_text(f'TSFP_DISPLAY_V2 {width} {height} 55\n')
            for rect in [(201,110,375,321), (3,4,3,4), (0,0,639,479)]:
                capture(folder/'portal.d3dcap', rect)
                (folder/'out000.bmp').unlink(missing_ok=True)
                result = subprocess.run([str(executable),str(folder/'portal.d3dcap'),
                                         '--out',str(folder/'out')], cwd=folder,env=env,
                                        capture_output=True,text=True,timeout=30)
                log=result.stdout+result.stderr
                assert result.returncode == 0 and '0 host calls failed' in log, log
                if '[RHI] vulkan:' in log:
                    from vulkan_sdk import require_clean_validation
                    require_clean_validation(log)
                check_bitmap(folder/'out000.bmp', width, height, rect)
            print(f'PASS: portal, single-pixel and full-frame scissors at {width}x{height}')


if __name__ == '__main__':
    main(sys.argv[1])
