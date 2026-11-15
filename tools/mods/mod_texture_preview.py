"""Bounded DDS inspection using only integer arithmetic, without image libraries.

Sample the base level, never an author-supplied thumbnail mip. Output is at most
128x128 RGBA pixels. This is a review aid, not a safety or runtime-use verdict.
"""
import struct

from mod_package import validate_dds


def _rgb565(value):
    r, g, b = value >> 11, (value >> 5) & 63, value & 31
    return ((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2))


def _pixel(data, width, x, y, fourcc):
    if not fourcc:
        b, g, r, a = struct.unpack_from('4B', data, 128 + (y * width + x) * 4)
        return r, g, b, a
    dxt1 = fourcc == 0x31545844
    offset = 128 + ((y // 4) * ((width + 3) // 4) + x // 4) * (8 if dxt1 else 16)
    index = (y % 4) * 4 + x % 4
    color = offset if dxt1 else offset + 8
    c0, c1, indices = struct.unpack_from('<HHI', data, color)
    p0, p1 = _rgb565(c0), _rgb565(c1)
    if c0 > c1 or not dxt1:
        palette = (p0, p1, tuple((2*a+b)//3 for a,b in zip(p0,p1)),
                   tuple((a+2*b)//3 for a,b in zip(p0,p1)))
    else:
        palette = (p0, p1, tuple((a+b)//2 for a,b in zip(p0,p1)), (0,0,0))
    selector = (indices >> (index * 2)) & 3
    alpha = 0 if dxt1 and c0 <= c1 and selector == 3 else 255
    if fourcc == 0x33545844:
        alpha = ((int.from_bytes(data[offset:offset+8], 'little') >> (index*4)) & 15) * 17
    elif fourcc == 0x35545844:
        a0, a1 = data[offset:offset+2]
        if a0 > a1:
            alphas = (a0,a1) + tuple(((7-i)*a0+i*a1)//7 for i in range(1,7))
        else:
            alphas = (a0,a1) + tuple(((5-i)*a0+i*a1)//5 for i in range(1,5)) + (0,255)
        alpha = alphas[(int.from_bytes(data[offset+2:offset+8], 'little') >> (index*3)) & 7]
    return (*palette[selector], alpha)


def thumbnail(data):
    validate_dds(data)
    height, width = struct.unpack_from('<II', data, 12)
    fourcc = struct.unpack_from('<I', data, 84)[0]
    # Powers-of-two dimensions preserve the aspect ratio at this bound, except
    # extremely thin textures where the shorter dimension stays one pixel.
    scale = max(1, max(width,height)//128)
    out_width, out_height = max(1,width//scale), max(1,height//scale)
    rgba = bytearray()
    for y in range(out_height):
        for x in range(out_width):
            rgba.extend(_pixel(data,width,min(width-1,x*scale+scale//2),
                               min(height-1,y*scale+scale//2),fourcc))
    return dict(width=width, height=height, format={0:'BGRA8',0x31545844:'BC1',
                0x33545844:'BC2',0x35545844:'BC3'}[fourcc],
                preview_width=out_width, preview_height=out_height), bytes(rgba)
