"""Validate the synthetic replay texture's actual storage contract."""


import struct

from ghost_targets import capture


def test_offscreen_target_storage_covers_swizzled_addresses(tmp_path):
    path = tmp_path/'synthetic.d3dcap'
    capture(path, 160, 90)
    data = path.read_bytes()
    offset = 32
    textures = []
    while offset < len(data):
        kind, size = struct.unpack_from('<II', data, offset)
        offset += 8
        if kind == 11:
            texture, fmt, width, height, levels, usage, pitch, rows, count = struct.unpack_from('<9I', data, offset)
            textures.append(texture)
            assert fmt == 0x06  # D3DFMT_A8R8G8B8, swizzled
            assert width > 0 and width & (width - 1) == 0
            assert height > 0 and height & (height - 1) == 0
            assert (levels, usage) == (1, 1)
            assert pitch == width * 4 and rows == height
            assert count == pitch * rows and size == 36 + count
        offset += (size + 3) & ~3
    assert textures == [1] and offset == len(data)
