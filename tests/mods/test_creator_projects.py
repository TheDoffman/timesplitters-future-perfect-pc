"""Creator CLI to real packages and native texture reader, using original artwork."""
import io
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import zipfile

from PIL import Image
import pytest

from mod_creator import create_project, import_texture, output_size
from mod_package import ModError, build_package, inspect_package
from test_texture_mod_mount import reader

ROOT = Path(__file__).resolve().parents[2]
KEY = '0123456789abcdef'


def project(tmp_path, template='textures'):
    target = tmp_path/'Creator project'
    create_project(target, mod_id='my-mode', name='My mode', author='creator', template=template)
    return target


def png(tmp_path, size=(8, 4), color=(80, 120, 160, 128)):
    source = tmp_path/'authored.png'
    Image.new('RGBA', size, color).save(source)
    return source


@pytest.mark.parametrize('template', ['gameplay', 'textures', 'mixed'])
def test_cli_new_import_pack_validate_workflow(tmp_path, template):
    target = tmp_path/'Creator project'
    tool = [sys.executable, str(ROOT/'tools/mods/mod-workshop.py')]
    result = subprocess.run(tool+['new', str(target), '--id', 'my-mode', '--name', 'My mode',
        '--author', 'creator', '--template', template], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    assert json.loads(result.stdout)['needs_texture_import'] == (template == 'textures')
    if template != 'gameplay':
        result = subprocess.run(tool+['import-texture', str(target), str(png(tmp_path)),
            '--key', KEY, '--format', 'RGBA8'], capture_output=True, text=True)
        assert result.returncode == 0, result.stderr
    archive = tmp_path/'my-mode.zip'
    result = subprocess.run(tool+['pack', str(target), str(archive)], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    result = subprocess.run(tool+['validate', str(archive)], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    manifest = json.loads(result.stdout)
    assert manifest['author'] == 'creator'
    expected = ({'scripts/main.lua'} if template != 'textures' else set()) | ({'textures/'+KEY+'.dds'} if template != 'gameplay' else set())
    assert set(manifest['files']) == expected
    with zipfile.ZipFile(archive) as packed:
        assert set(packed.namelist()) == {'mod.json'} | expected
        if template != 'textures':
            assert packed.read('scripts/main.lua') == (ROOT/'examples/mods/first-to-three/scripts/main.lua').read_bytes()
    assert json.loads((target/'mod.json').read_text())['files'] == {}  # Source manifest stays editable.


@pytest.mark.parametrize('fmt', ['DXT1', 'DXT5', 'RGBA8'])
def test_imported_png_is_accepted_by_actual_shared_native_reader(reader, tmp_path, fmt):
    target = project(tmp_path)
    source = png(tmp_path);before = source.read_bytes()
    result = import_texture(target, source, key=KEY, pixel_format=fmt)
    encoded = Path(result['path']).read_bytes()
    executable, env = reader
    env = dict(env);env.pop('TSFP_MOD_TEXTURE_DIR', None)
    native = subprocess.run([str(executable), result['path'], '8', '4', str(encoded[128])],
        env=env, capture_output=True, text=True, timeout=10)
    assert native.returncode == 0, native.stdout+native.stderr
    assert source.read_bytes() == before and result['source_preserved']
    assert struct.unpack_from('<I', encoded, 28)[0] == 4
    decoded = Image.open(io.BytesIO(encoded)).convert('RGBA')
    if fmt == 'RGBA8':
        assert decoded.tobytes() == Image.open(source).convert('RGBA').tobytes()
    elif fmt == 'DXT5':
        assert abs(decoded.getpixel((0, 0))[3] - 128) <= 1
    archive = tmp_path/'one.zip';build_package(target, archive)
    assert set(inspect_package(archive)['files']) == {'textures/'+KEY+'.dds'}


@pytest.mark.parametrize('channels', ['color', 'luminance', 'alpha'])
def test_explicit_resizing_and_channel_semantics(tmp_path, channels):
    result = import_texture(project(tmp_path), png(tmp_path), key=KEY, pixel_format='RGBA8',
        width=4, height=2, channels=channels)
    image = Image.open(result['path']).convert('RGBA')
    assert image.size == (4, 2)
    r, g, b, a = image.getpixel((0, 0))
    if channels == 'alpha':
        assert (r, g, b, a) == (255, 255, 255, 128)
    elif channels == 'luminance':
        assert r == g == b and a == 255
    else:
        assert a == 128 and abs(r-80) <= 1


def test_dds_import_preserves_exact_bytes_and_rejects_encoding_options(tmp_path):
    target = project(tmp_path)
    first = import_texture(target, png(tmp_path), key=KEY)
    source = tmp_path/'existing.dds';source.write_bytes(Path(first['path']).read_bytes())
    second = import_texture(target, source, key='0000000000000001')
    assert Path(second['path']).read_bytes() == source.read_bytes()
    with pytest.raises(ModError, match='preserves bytes'):
        import_texture(target, source, key='0000000000000002', pixel_format='DXT5')


@pytest.mark.parametrize('settings', [dict(key='../escape'), dict(key='ABCDEF0123456789'),
    dict(width=8), dict(width=7, height=4), dict(width=8, height=8),
    dict(pixel_format='DXT1', channels='alpha'), dict(pixel_format='unknown')])
def test_invalid_import_preserves_project_and_source(tmp_path, settings):
    target = project(tmp_path);source = png(tmp_path);before = source.read_bytes()
    with pytest.raises(ModError):
        import_texture(target, source, **(dict(key=KEY) | settings))
    assert not list((target/'textures').iterdir()) and source.read_bytes() == before


@pytest.mark.parametrize('kind', ['truncated', 'wrong-format', 'oversized-dimensions', 'animated', 'bad-dds'])
def test_rejects_unsupported_images_before_writing(tmp_path, kind):
    target = project(tmp_path);source = png(tmp_path)
    if kind == 'truncated':source.write_bytes(source.read_bytes()[:33])
    elif kind == 'wrong-format':Image.new('RGB', (4, 4)).save(source, format='JPEG')
    elif kind == 'oversized-dimensions':Image.new('RGB', (4097, 1)).save(source)
    elif kind == 'animated':Image.new('RGBA', (4, 4)).save(source, save_all=True, append_images=[Image.new('RGBA', (4, 4), 'red')])
    else:source.write_bytes(b'DDS '+b'\0'*128)
    with pytest.raises(ModError):import_texture(target, source, key=KEY)
    assert not list((target/'textures').iterdir())


def test_does_not_replace_existing_project_texture_or_manifest(tmp_path):
    target = project(tmp_path);source = png(tmp_path)
    first = import_texture(target, source, key=KEY)
    before = {p.relative_to(target):p.read_bytes() for p in target.rglob('*') if p.is_file()}
    with pytest.raises(ModError):create_project(target, mod_id='other', name='Other', author='creator')
    with pytest.raises(ModError, match='already exists'):import_texture(target, source, key=KEY)
    assert before == {p.relative_to(target):p.read_bytes() for p in target.rglob('*') if p.is_file()}
    assert Path(first['path']).exists()


def test_new_rejects_invalid_manifest_without_creating_destination(tmp_path):
    target = tmp_path/'invalid'
    with pytest.raises(ModError):create_project(target, mod_id='Bad ID', name='Name', author='creator')
    assert not target.exists()


def test_renderer_budget_is_checked_before_decoding_or_encoding(tmp_path):
    with pytest.raises(ModError, match='24 MiB'):output_size(4096, 4096, 'RGBA8')
    target = project(tmp_path)
    with pytest.raises(ModError, match='24 MiB'):
        import_texture(target, png(tmp_path, size=(4, 4)), key=KEY, pixel_format='RGBA8', width=4096, height=4096)
    assert not list((target/'textures').iterdir())


def test_case_collision_and_art_inside_project_are_preserved(tmp_path):
    target = project(tmp_path);source = png(tmp_path)
    (target/'textures'/(KEY.upper()+'.DDS')).write_bytes(b'keep')
    with pytest.raises(ModError, match='already exists'):import_texture(target, source, key=KEY)
    inside = target/'authored.png';inside.write_bytes(source.read_bytes())
    with pytest.raises(ModError, match='outside'):import_texture(target, inside, key='0000000000000001')
    assert inside.read_bytes() == source.read_bytes()


@pytest.mark.parametrize('where', ['source', 'project', 'textures'])
def test_linked_inputs_and_output_directories_are_not_followed(tmp_path, where):
    target = project(tmp_path);source = png(tmp_path)
    outside = tmp_path/'outside';outside.mkdir()
    link = tmp_path/'alias'
    try:
        if where == 'source':
            os.symlink(source, link)
            source = link
        elif where == 'project':
            os.symlink(target, link, target_is_directory=True)
            target = link
        else:
            (target/'textures').rmdir()
            os.symlink(outside, target/'textures', target_is_directory=True)
    except OSError as exc:
        pytest.skip('Host does not permit this test to create a symlink: '+str(exc))
    with pytest.raises(ModError, match='links'):
        import_texture(target, source, key=KEY)
    assert not list(outside.iterdir())
