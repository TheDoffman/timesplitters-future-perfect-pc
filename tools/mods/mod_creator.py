"""Local creator projects and bounded texture import; never executes mod source."""
import copy
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import re
import struct

from mod_package import (MAX_MANIFEST, ModError, digest, read_json, require,
                         validate_dds, validate_manifest)

ROOT = Path(__file__).resolve().parents[2]
MAX_IMAGE_BYTES = 32 * 1024 * 1024
MAX_DDS_BYTES = 24 * 1024 * 1024
KEY = re.compile(r'[0-9a-f]{16}\Z')


def local_path(path):
    """Refuse links/junctions rather than following them while editing a project."""
    path = Path(path).absolute()
    require('..' not in path.parts, 'Use a project path without parent traversal')
    for part in (path, *path.parents):
        require(not part.is_symlink() and not part.is_junction(), 'Creator paths must not contain links')
    return path


def bounded_input(path, limit):
    path = local_path(path)
    require(path.is_file() and 0 < path.stat().st_size <= limit, 'Creator input exceeds limits or is not a regular file')
    with path.open('rb') as stream:
        data = stream.read(limit + 1)
    require(0 < len(data) <= limit, 'Creator input exceeds limits')
    return data


def validate_project_manifest(manifest):
    # Source projects use files:{}; pack computes the real inventory and hashes.
    probe = copy.deepcopy(manifest)
    require(isinstance(probe, dict), 'Manifest must be an object')
    if probe.get('files') == {}:
        probe['files'] = {'scripts/main.lua': {'sha256': '0'*64, 'size': 1}}
    validate_manifest(probe)


def example_wave(period):
    """Small creator-owned integer triangle tone, identical on all hosts."""
    pcm=b''.join(struct.pack('<h',-8000+32000*min(i%period,period-i%period)//period) for i in range(22050))
    fmt=struct.pack('<HHIIHH',1,1,22050,44100,2,16)
    body=b'WAVEfmt '+struct.pack('<I',16)+fmt+b'data'+struct.pack('<I',len(pcm))+pcm
    return b'RIFF'+struct.pack('<I',len(body))+body


def create_project(destination, *, mod_id, name, author, template='gameplay'):
    require(template in ('gameplay', 'textures', 'mixed', 'audio'), 'Unknown creator template')
    destination = local_path(destination)
    manifest = dict(schema=1, id=mod_id, version='0.1.0', name=name, author=author,
        description='A community addition for TimeSplitters Future Perfect.',
        game='tsfp-us-45410066', api='tsfp-mod-v1-draft', dependencies={}, files={})
    validate_project_manifest(manifest)
    require(destination.parent.is_dir(), 'Create the parent directory first')
    require(not destination.exists(), 'Project destination already exists; choose a new folder')
    # Read the original SDK example before creating anything. No recovered game code.
    script = None
    if template != 'textures':
        example='audio-lab' if template=='audio' else 'first-to-three'
        script = bounded_input(ROOT/f'examples/mods/{example}/scripts/main.lua', 1024*1024)
    destination.mkdir()
    with (destination/'mod.json').open('xb') as stream:
        stream.write((json.dumps(manifest, indent=2, ensure_ascii=False)+'\n').encode('utf-8'))
    if script is not None:
        (destination/'scripts').mkdir()
        with (destination/'scripts/main.lua').open('xb') as stream:
            stream.write(script)
    if template in ('textures','mixed'):
        (destination/'textures').mkdir()
    if template=='audio':
        (destination/'audio').mkdir()
        for key,period in [('00000001',50),('00000002',25)]:
            with (destination/'audio'/f'{key}.wav').open('xb') as stream:
                stream.write(example_wave(period))
    return dict(project=str(destination), template=template, id=mod_id,
                needs_texture_import=template == 'textures')


def output_size(width, height, pixel_format):
    require(type(width) is int and type(height) is int and 0 < width <= 4096 and
            0 < height <= 4096 and not width & (width-1) and not height & (height-1),
            'Texture dimensions must be powers of two up to 4096')
    size = 128
    while True:
        size += width*height*4 if pixel_format == 'RGBA8' else ((width+3)//4)*((height+3)//4)*(8 if pixel_format == 'DXT1' else 16)
        if width == height == 1:
            break
        width, height = max(1, width//2), max(1, height//2)
    require(size <= MAX_DDS_BYTES, 'Texture exceeds the 24 MiB renderer limit; use DXT or smaller dimensions')
    return size


def import_texture(project, source, *, key, pixel_format=None, width=None, height=None, channels='color'):
    require(isinstance(key, str) and KEY.fullmatch(key), 'Texture key must be 16 lowercase hexadecimal characters')
    require(pixel_format in (None, 'DXT1', 'DXT5', 'RGBA8'), 'Unsupported texture format')
    require(channels in ('color', 'luminance', 'alpha'), 'Unsupported channel layout')
    require((width is None) == (height is None), 'Supply both width and height')
    project, source = local_path(project), local_path(source)
    require(project.is_dir(), 'Creator project does not exist')
    manifest = read_json(bounded_input(project/'mod.json', MAX_MANIFEST))
    validate_project_manifest(manifest)
    require(not source.is_relative_to(project), 'Keep source artwork outside the package project')
    folder = local_path(project/'textures')
    require(not folder.exists() or folder.is_dir(), 'Textures path is not a directory')
    destination = folder/(key+'.dds')
    if folder.exists():
        require(not any(p.name.lower() == destination.name for p in folder.iterdir()),
                'This texture key already exists; use a new project version to replace it')
    data = bounded_input(source, MAX_IMAGE_BYTES)
    dimensions = None
    if data.startswith(b'DDS '):
        require(pixel_format is None and width is None and channels == 'color',
                'DDS import preserves bytes; encoding options apply only to PNG')
        validate_dds(data)
        encoded = data
    else:
        require(data.startswith(b'\x89PNG\r\n\x1a\n'), 'Texture input must be PNG or a renderer-compatible DDS')
        try:
            from PIL import Image, UnidentifiedImageError
        except ImportError as exc:
            raise ModError('PNG import needs the pinned Pillow dependency from requirements.txt') from exc
        try:
            with Image.open(io.BytesIO(data), formats=('PNG',)) as image:
                require(0 < image.width <= 4096 and 0 < image.height <= 4096,
                        'PNG dimensions exceed 4096 pixels per axis')
                require(not getattr(image, 'is_animated', False), 'Animated PNG is not a texture')
                dimensions = image.size
                target = dimensions if width is None else (width, height)
                fmt = pixel_format or 'DXT5'
                expected = output_size(*target, fmt)
                require(dimensions[0]*target[1] == dimensions[1]*target[0], 'Resizing would change the aspect ratio')
                require(not (channels == 'alpha' and fmt == 'DXT1'), 'Alpha textures require DXT5 or RGBA8')
                # Reuse the existing game texture encoder/channel policy.
                spec = importlib.util.spec_from_file_location('tsfp_creator_texture_encoder', ROOT/'tools/textures/texture-pack.py')
                encoder = importlib.util.module_from_spec(spec);spec.loader.exec_module(encoder)
                rgba = encoder.channel_layout(image.convert('RGBA'), channels)
                if rgba.size != target:
                    rgba = rgba.resize(target, Image.Resampling.LANCZOS)
                encoded, _ = encoder.encode(rgba, fmt)
                require(len(encoded) == expected, 'Unexpected encoded texture size')
        except (OSError, ValueError, UnidentifiedImageError, Image.DecompressionBombError) as exc:
            if isinstance(exc, ModError):
                raise
            raise ModError('PNG could not be decoded within the texture contract') from exc
        validate_dds(encoded)
    # The package builder creates the manifest inventory. No sidecar/provenance
    # containing local paths is put into the distributable content tree.
    folder.mkdir(exist_ok=True)
    with destination.open('xb') as stream:
        stream.write(encoded)
    return dict(path=str(destination), key=key, sha256=digest(encoded), bytes=len(encoded),
                source_sha256=hashlib.sha256(data).hexdigest(), source_size=dimensions,
                source_preserved=True)
