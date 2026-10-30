"""Portable package boundary checks; actual Mach-O/loading checks run on Mac."""


import hashlib
import importlib.util
import json
from pathlib import Path
import sys

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools'))
spec = importlib.util.spec_from_file_location('mac_package', ROOT/'tools/release/package-native-mac.py')
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)


def payload(root):
    names = [package.GAME, 'Launch.command', package.APP+'/Contents/MacOS/TimeSplitters.Launcher',
             'Settings/pc-controls.json', 'Bin/libvulkan.1.dylib', 'Bin/libMoltenVK.dylib',
             'Bin/libdxcompiler.dylib', 'Support/Vulkan/MoltenVK_icd.json', 'Source/pc-port-source.zip',
             'Support/Python/bin/python3', 'Support/Python/bin/python-real',
             'Support/Online/online.py', 'Support/Online/online_platform.py',
             'Support/Online/network-identity.json', 'Support/Online/network-build.json',
             'Support/Online/multiplayer.png']
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
    config = tmp_path/'config/build'
    config.mkdir(parents=True)
    (config/'native-mac-runtime-licenses.json').write_text(json.dumps({
        'notice.txt': {'url': 'https://example.invalid/license', 'sha256': hashlib.sha256(b'original').hexdigest()}}))
    cache = tmp_path/'licenses'
    cache.mkdir()
    (cache/'notice.txt').write_bytes(b'changed')
    monkeypatch.setattr(package, 'ROOT', tmp_path)
    with pytest.raises(ValueError, match='differs'):
        package.prepare_licenses(cache)
    assert (cache/'notice.txt').read_bytes() == b'changed'


def test_online_formula_notices_keep_bytes_in_flat_installer_layout(tmp_path):
    import bundle_online_macos
    output = tmp_path/'package'
    expected = {}
    for name in ('libtommath', 'python@3.12'):
        formula = tmp_path/'Cellar'/name/'1.0'
        formula.mkdir(parents=True)
        for notice in ('LICENSE', 'COPYING', 'Copyright.md', 'LICENSE.txt', 'sbom.spdx.json'):
            source = formula/notice
            original = (name+'\n'+notice+'\n').encode()+b'\x80\r\n'
            source.write_bytes(original)
            expected['Licenses/Online-'+name+'-'+notice+('' if notice.endswith('.txt') else '.txt')] = original
        (formula/'README.md').write_text('not a notice')
        (formula/'LICENSE-directory').mkdir()
        def copy(source, relative):
            target = output/relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(source.read_bytes())
        bundle_online_macos.copy_notices(formula, copy)
    actual = {p.relative_to(output).as_posix(): p.read_bytes() for p in output.rglob('*') if p.is_file()}
    assert actual == expected
    assert all(len(Path(name).parts) == 2 and name.endswith('.txt') for name in actual)


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


@pytest.mark.parametrize('options,bundled', [([], False), (['--without-textures'], False),
                                            (['--with-textures'], True), (['--texture-pack', 'custom'], True)])
def test_mac_packager_only_audits_textures_when_explicit(tmp_path, monkeypatch, options, bundled):
    native = tmp_path/'build/macos'
    native.mkdir(parents=True)
    executable = native/'futureperfect_recomp'
    executable.write_bytes(b'fixture')
    (native/'BUILD.json').write_text(json.dumps({'executable_sha256': package.digest(executable),
        'renderer': 'vulkan', 'vulkan_driver': 'moltenvk', 'source_revision': 'revision'}))
    monkeypatch.setattr(package, 'ROOT', tmp_path)
    monkeypatch.setattr(package.platform, 'system', lambda: 'Darwin')
    monkeypatch.setattr(package.platform, 'machine', lambda: 'arm64')
    monkeypatch.setattr(package, 'run', lambda *args: 'revision')
    monkeypatch.setattr(package.vulkan_sdk, 'verify_install', lambda: tmp_path/'sdk')
    calls = []
    monkeypatch.setattr(package, 'ensure_pack', lambda root, override: (calls.append((root, override)), [], {}))
    class PolicyChecked(Exception):
        pass
    def stop_before_host_packaging(*args):
        raise PolicyChecked()
    monkeypatch.setattr(package, 'prepare_licenses', stop_before_host_packaging)
    monkeypatch.setattr(sys, 'argv', ['package-native-mac.py', '--version', 'v1.0.0', *options])
    with pytest.raises(PolicyChecked):
        package.main()
    assert bool(calls) == bundled
    if calls:
        assert calls[0][0] == tmp_path
        assert calls[0][1] == (Path('custom') if '--texture-pack' in options else None)
