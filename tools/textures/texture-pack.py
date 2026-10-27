"""Compile an authored PNG into a fully mipmapped texture override.

This performs encoding/resampling, not artwork generation. Originals are never
overwritten. Runtime supports powers of two up to 4096 pixels on each axis.
"""

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

import argparse
import hashlib
import io
import json
import re
import struct
from pathlib import Path
from PIL import Image


def encode(image, pixel_format='DXT5'):
    w,h=image.size
    if not (w and h and w<=4096 and h<=4096 and not w&(w-1) and not h&(h-1)):
        raise ValueError('Dimensions must be powers of two, at most 4096.')
    pieces=[]
    while True:
        if pixel_format=='RGBA8':
            # Legacy BGRA8 DDS for small gradients where BC1 color quantization
            # causes visible bands. Keep lossless pixels at every mip level.
            words=[124,0x2100F,h,w,w*4,0,1]+[0]*11
            words += [32,0x41,0,32,0xff0000,0xff00,0xff,0xff000000]
            words += [0x1000,0,0,0,0]
            data=b'DDS '+struct.pack('<31I',*words)+image.tobytes('raw','BGRA')
        else:
            buf=io.BytesIO();image.save(buf,format='DDS',pixel_format=pixel_format)
            data=buf.getvalue()
        if not pieces: header=bytearray(data[:128])
        pieces.append(data[128:])
        if image.size==(1,1): break
        image=image.resize((max(1,image.width//2),max(1,image.height//2)),Image.Resampling.LANCZOS)
    struct.pack_into('<I',header,8,0x2100F if pixel_format=='RGBA8' else 0xA1007)
    struct.pack_into('<I',header,20,w*4 if pixel_format=='RGBA8' else len(pieces[0]))
    struct.pack_into('<I',header,28,len(pieces))
    struct.pack_into('<I',header,108,0x401008)
    return bytes(header)+b''.join(pieces),len(pieces)


def channel_layout(image, channels):
    """Encode source color into the original texture's channel semantics.

    L8 is opaque luminance. A8 uses only authored alpha; the existing shader
    supplies white RGB. This is format conversion, not artwork generation.
    """
    if channels == 'luminance':
        return image.convert('L').convert('RGBA')
    if channels == 'alpha':
        result = Image.new('RGBA', image.size, (255,255,255,255))
        result.putalpha(image.getchannel('A'))
        return result
    return image


def original_alpha(path, expected_sha256, size):
    """Resample a verified source channel; never invent or threshold opacity."""
    data = path.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if digest != expected_sha256:
        raise ValueError('Alpha source SHA-256 mismatch')
    with Image.open(io.BytesIO(data)) as source:
        if 'A' not in source.getbands():
            raise ValueError('Alpha source must have an explicit alpha channel')
        if source.width * size[1] != source.height * size[0]:
            raise ValueError('Alpha source aspect ratio differs from output')
        alpha = source.getchannel('A')
        native = source.size
        bounds = alpha.getextrema()
        if native != size:
            # Bilinear avoids ringing beyond the original opacity range.
            alpha = alpha.resize(size, Image.Resampling.BILINEAR)
    return alpha, dict(source=str(path.resolve()), sha256=digest,
                       source_size=native, alpha_range=bounds,
                       resampled=native != size, resampling='bilinear',
                       policy='Original alpha retained; DDS compression may be lossy')


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('image',type=Path)
    ap.add_argument('--key',required=True)
    ap.add_argument('--output',type=Path,default=Path('private/texture-remaster/pack'))
    ap.add_argument('--width',type=int,required=True)
    ap.add_argument('--height',type=int,required=True)
    ap.add_argument('--format',choices=['DXT1','DXT5','RGBA8'],default='DXT5')
    ap.add_argument('--channels',choices=['color','luminance','alpha'],default='color',
                    help='Original texture channel semantics; recorded in metadata')
    ap.add_argument('--alpha-source', type=Path,
                    help='Verified original PNG whose alpha replaces authored alpha during encoding')
    ap.add_argument('--alpha-source-sha256',
                    help='Required SHA-256 of alpha-source; both options must be supplied together')
    ap.add_argument('--allow-one-pixel-aspect-rounding', action='store_true',
                    help='Explicitly accept at most one source pixel of long-axis rounding for opaque color sources at least 512 pixels on both axes')
    ap.add_argument('--budget-mib',type=int,default=6144,help='Maximum total active pack size (default 6 GiB)')
    a=ap.parse_args()
    if not re.fullmatch('[0-9a-f]{16}',a.key):ap.error('key must be 16 lowercase hexadecimal characters')
    if a.budget_mib<=0:ap.error('budget must be positive')
    if a.channels=='alpha' and a.format=='DXT1':ap.error('Alpha-only sources require DXT5 or RGBA8')
    if bool(a.alpha_source) != bool(a.alpha_source_sha256):
        ap.error('Supply both --alpha-source and --alpha-source-sha256')
    if a.alpha_source:
        if not re.fullmatch('[0-9a-f]{64}', a.alpha_source_sha256):
            ap.error('Alpha source SHA-256 must be 64 lowercase hexadecimal characters')
        if a.channels != 'color' or a.format == 'DXT1':
            ap.error('Original alpha requires color channels and DXT5 or RGBA8')
    if (a.output/f'{a.key}.dds').exists():ap.error('This key already exists; preserve or explicitly retire the prior version first')
    w,h=a.width,a.height
    if not(w>0 and h>0 and w<=4096 and h<=4096 and not w&(w-1) and not h&(h-1)):
        ap.error('Dimensions must be powers of two up to 4096')
    expected=128
    while True:
        expected+=(w*h*4 if a.format=='RGBA8' else ((w+3)//4)*((h+3)//4)*(8 if a.format=='DXT1' else 16))
        if w==h==1:break
        w=max(1,w//2);h=max(1,h//2)
    if expected>24*1024*1024:ap.error('Texture exceeds the runtime 24 MiB per-file limit; reduce resolution or use DXT')
    used=sum(p.stat().st_size for p in a.output.rglob('*') if p.is_file()) if a.output.exists() else 0
    if used+expected>a.budget_mib*1024*1024:
        ap.error(f'Pack budget exceeded: existing {used} + new {expected} bytes; reduce asset resolution or retire unused assets')
    image=Image.open(a.image).convert('RGBA');native=image.size
    image=channel_layout(image,a.channels)
    aspect_delta = abs(native[0]*a.height-native[1]*a.width)
    aspect_rounding = None
    if aspect_delta:
        if not a.allow_one_pixel_aspect_rounding:
            ap.error('Resizing would change the aspect ratio')
        if (aspect_delta > min(a.width,a.height) or min(native) < 512
                or a.channels != 'color' or a.alpha_source
                or image.getchannel('A').getextrema() != (255,255)):
            ap.error('Aspect rounding requires opaque color, no alpha override, source axes >=512, and at most one long-axis source pixel')
        aspect_rounding = dict(source_long_axis_error_pixels=aspect_delta/min(a.width,a.height),
                               explicitly_allowed=True, resampling='LANCZOS')
    if a.alpha_source:
        # Generated opacity must not premultiply/darken the replacement RGB.
        image = image.convert('RGB').convert('RGBA')
    if native!=(a.width,a.height):image=image.resize((a.width,a.height),Image.Resampling.LANCZOS)
    alpha_meta = None
    if a.alpha_source:
        try:
            alpha, alpha_meta = original_alpha(a.alpha_source, a.alpha_source_sha256, image.size)
        except (ValueError, OSError) as exc:
            ap.error(str(exc))
        image.putalpha(alpha)
    data,mips=encode(image,a.format)
    meta=dict(key=a.key,source=str(a.image.resolve()),source_size=native,
        output_size=[a.width,a.height],resampled=native!=(a.width,a.height),
        format=a.format,channels=a.channels,mips=mips,bytes=len(data),sha256=hashlib.sha256(data).hexdigest())
    if alpha_meta is not None:
        meta['original_alpha'] = alpha_meta
    if aspect_rounding is not None:
        meta['source_aspect_rounding'] = aspect_rounding
    metadata=(json.dumps(meta,indent=2)+'\n').encode('utf-8')
    # The user's cap includes provenance files, not only GPU payloads.
    if used+len(data)+len(metadata)>a.budget_mib*1024*1024:
        ap.error('Pack budget exceeded including texture metadata')
    path=a.output/f'{a.key}.dds'
    if path.with_suffix('.json').exists():
        ap.error('Metadata for this key already exists; inspect it before replacing')
    a.output.mkdir(parents=True,exist_ok=True)
    with path.open('xb') as f:f.write(data)
    with path.with_suffix('.json').open('xb') as f:f.write(metadata)
    print(json.dumps(meta,indent=2))


if __name__=='__main__':main()
