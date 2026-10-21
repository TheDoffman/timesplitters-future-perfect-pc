"""Portable package boundary checks; actual Mach-O/loading checks run on Mac."""
import hashlib
import importlib.util
import json
from pathlib import Path
import sys

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
spec = importlib.util.spec_from_file_location('mac_package', ROOT/'tools/package-native-mac.py')
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)


def payload(root):
    names = [package.GAME, 'Launch.command', package.APP+'/Contents/MacOS/TimeSplitters.Launcher',
             'Settings/pc-controls.json', 'Bin/libvulkan.1.dylib', 'Bin/libMoltenVK.dylib',
             'Bin/libdxcompiler.dylib', 'Support/Vulkan/MoltenVK_icd.json', 'Source/pc-port-source.zip']
    files = {}
    for name in names:
        path = root/name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b'fixture')
        files[name] = package.digest(path)
    (root/'BUILD.json').write_text('{}')
    return {'files': files}


def test_complete_payload_and_unlisted_file_rejection(tmp_path):
    manifest = payload(tmp_path)
    package.verify_payload(tmp_path, manifest)
    (tmp_path/'unlisted.txt').write_text('unexpected')
    with pytest.raises(ValueError, match='Unlisted'):
        package.verify_payload(tmp_path, manifest)


@pytest.mark.parametrize('name', ['Data/default.xbe', 'Saves/profile', 'Settings/runtime.conf',
                                  'Support/game.iso', 'Support/game.pak', '../outside'])
def test_personal_or_game_files_rejected_even_when_hashed(tmp_path, name):
    manifest = payload(tmp_path)
    manifest['files'][name] = 'a'*64
    with pytest.raises(ValueError, match='Protected'):
        package.verify_payload(tmp_path, manifest)


def test_corrupt_and_incomplete_payload_rejected(tmp_path):
    manifest = payload(tmp_path)
    (tmp_path/package.GAME).write_bytes(b'altered')
    with pytest.raises(ValueError, match='differs'):
        package.verify_payload(tmp_path, manifest)
    del manifest['files']['Bin/libMoltenVK.dylib']
    with pytest.raises(ValueError, match='Incomplete'):
        package.verify_payload(tmp_path, manifest)


def test_changed_cached_license_is_not_overwritten(tmp_path, monkeypatch):
    config = tmp_path/'config'
    config.mkdir()
    (config/'native-mac-runtime-licenses.json').write_text(json.dumps({
        'notice.txt': {'url': 'https://example.invalid/license', 'sha256': hashlib.sha256(b'original').hexdigest()}}))
    cache = tmp_path/'licenses'
    cache.mkdir()
    (cache/'notice.txt').write_bytes(b'changed')
    monkeypatch.setattr(package, 'ROOT', tmp_path)
    with pytest.raises(ValueError, match='differs'):
        package.prepare_licenses(cache)
    assert (cache/'notice.txt').read_bytes() == b'changed'


def test_minimum_os_is_highest_actual_binary_requirement(monkeypatch):
    outputs = iter(['cmd LC_BUILD_VERSION\n minos 27.0\n sdk 27.0',
                    'cmd LC_BUILD_VERSION\n minos 26.0\n sdk 26.0'])
    monkeypatch.setattr(package, 'run', lambda *args: next(outputs))
    assert package.minimum_macos([Path('game'), Path('driver')]) == '27.0'


def test_texture_members_stream_without_a_staging_copy(tmp_path):
    from test_build_textures import fixture
    source = tmp_path/'inputs'
    source.mkdir()
    _, rows, _, _, _ = fixture(source)
    stage = tmp_path/'stage'
    manifest = payload(stage)
    manifest['files'].update({name: sha for name, _, sha in package.zip_entries(rows)})
    package.verify_payload(stage, manifest, rows)
    assert not (stage/'Textures').exists()
    manifest['files']['Textures/0123456789abcdef.dds'] = '0'*64
    with pytest.raises(ValueError, match='Texture differs'):
        package.verify_payload(stage, manifest, rows)
