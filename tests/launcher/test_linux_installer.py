"""Exercise the single-download bootstrap without network access or downloaded code."""
import hashlib
import io
import json
from pathlib import Path
import types
import zipfile

import pytest

ROOT = Path(__file__).resolve().parents[2]


def installer():
    path = ROOT / 'tools/launcher/linux/Install-Linux.sh'
    source = path.read_text(encoding='utf-8').split("<<'TSFP_INSTALLER_PYTHON'\n", 1)[1].rsplit('\nTSFP_INSTALLER_PYTHON', 1)[0]
    module = types.ModuleType('linux_installer_fixture')
    exec(compile(source, str(path), 'exec'), module.__dict__)
    return module


def archive(suffix='linux-x64', *, machine=None, protocol=1, corrupt=False):
    version = '1.8.0'
    header = bytearray(64)
    header[:6] = b'\x7fELF\x02\x01'
    header[18:20] = (machine or (183 if suffix == 'linux-arm64' else 62)).to_bytes(2, 'little')
    files = {'TimeSplitters.Launcher': bytes(header),
             'Support/Launcher/application.json': json.dumps(dict(platform=suffix, version=version, installer_protocol=protocol)).encode()}
    manifest = dict(schema=1, version=version, files={name: hashlib.sha256(data).hexdigest() for name, data in files.items()})
    if corrupt:
        files['TimeSplitters.Launcher'] += b'changed'
    output = io.BytesIO()
    with zipfile.ZipFile(output, 'w') as bundle:
        for name, data in files.items():
            entry = zipfile.ZipInfo(name); entry.external_attr = 0o100755 << 16
            bundle.writestr(entry, data)
        bundle.writestr('Support/Launcher/manifest.json', json.dumps(manifest))
    return output.getvalue()


def release(data, suffix='linux-x64', version='1.8.0'):
    tag = 'launcher-v' + version
    name = 'TimeSplitters-Launcher-v' + version + '-' + suffix + '.zip'
    return dict(tag_name=tag, draft=False, prerelease=False, assets=[dict(
        name=name, size=len(data), digest='sha256:' + hashlib.sha256(data).hexdigest(),
        browser_download_url='https://github.com/TheDoffman/timesplitters-future-perfect-pc/releases/download/' + tag + '/' + name)])


@pytest.mark.parametrize('machine,suffix', [('x86_64','linux-x64'), ('AMD64','linux-x64'), ('x64','linux-x64'), ('aarch64','linux-arm64'), ('ARM64','linux-arm64')])
def test_architecture_uses_native_platform(machine, suffix):
    assert installer().architecture(machine) == suffix


@pytest.mark.parametrize('machine', ['armv7l', 'i686', 'riscv64'])
def test_unsupported_architecture_never_substitutes_x64(machine):
    with pytest.raises(ValueError, match='supports Linux x64 and ARM64'):
        installer().architecture(machine)


def test_releases_keep_architectures_channels_and_versions_separate():
    module = installer()
    x64 = release(archive())
    arm = release(archive('linux-arm64'), 'linux-arm64', '1.10.0')
    draft = release(archive('linux-arm64'), 'linux-arm64', '9.0.0'); draft['draft'] = True
    game = dict(tag_name='v99.0.0', draft=False, prerelease=False, assets=arm['assets'])
    assert module.select_release([draft, game, x64, arm], 'linux-arm64')[0] == '1.10.0'
    assert module.select_release([draft, game, x64, arm], 'linux-x64')[0] == '1.8.0'
    with pytest.raises(ValueError, match='No stable linux-arm64'):
        module.select_release([x64], 'linux-arm64')


def test_foreign_release_url_rejected():
    item = release(archive())
    item['assets'][0]['browser_download_url'] = 'https://example.com/launcher.zip'
    with pytest.raises(ValueError, match='does not belong'):
        installer().select_release([item], 'linux-x64')


def test_game_preview_pages_do_not_hide_a_stable_launcher(monkeypatch):
    module = installer(); arm = release(archive('linux-arm64'), 'linux-arm64')
    calls = []
    def read(url, limit):
        calls.append(url)
        return json.dumps([dict(tag_name='game-preview')] * 100 if len(calls) == 1 else [arm]).encode()
    monkeypatch.setattr(module, 'read_small', read)
    assert module.select_release(module.read_releases(), 'linux-arm64')[0] == '1.8.0'
    assert calls == [module.API+'1', module.API+'2']


def test_release_page_limit_never_installs_from_partial_catalog(monkeypatch):
    module = installer()
    monkeypatch.setattr(module, 'read_small', lambda *args: json.dumps([release(archive())] * 100).encode())
    with pytest.raises(ValueError, match='no partial launcher catalog'):
        module.read_releases()


@pytest.mark.parametrize('suffix', ['linux-x64', 'linux-arm64'])
def test_verified_bootstrap_matches_architecture(tmp_path, suffix):
    module = installer(); path = tmp_path / 'launcher.zip'; path.write_bytes(archive(suffix))
    executable = module.extract_bootstrap(path, tmp_path, suffix, '1.8.0')
    assert executable.name == 'TimeSplitters.Launcher'
    assert int.from_bytes(executable.read_bytes()[18:20], 'little') == (183 if suffix == 'linux-arm64' else 62)


@pytest.mark.parametrize('options,reason', [({'machine':183}, 'different Linux architecture'),
                                         ({'protocol':None}, 'predates automatic installation'),
                                         ({'corrupt':True}, 'checksum does not match')])
def test_wrong_cpu_legacy_command_and_corruption_never_execute(tmp_path, options, reason, monkeypatch):
    module = installer(); path = tmp_path / 'launcher.zip'; path.write_bytes(archive(**options))
    monkeypatch.setattr(module.subprocess, 'run', lambda *a, **k: pytest.fail('Unverified code executed'))
    with pytest.raises(ValueError, match=reason):
        module.extract_bootstrap(path, tmp_path, 'linux-x64', '1.8.0')


@pytest.mark.parametrize('bad', ['truncated', 'oversized', 'checksum'])
def test_download_bounds_and_hash_precede_extraction(tmp_path, monkeypatch, bad):
    module = installer(); data = b'verified package'; item = release(data)
    body = data[:-1] if bad == 'truncated' else data + b'extra' if bad == 'oversized' else b'changed package!'
    monkeypatch.setattr(module, 'request', lambda url: io.BytesIO(body))
    with pytest.raises(ValueError):
        module.download(item['assets'][0], item['tag_name'], hashlib.sha256(data).hexdigest(), tmp_path/'package.zip')


def test_sidecar_is_bound_to_exact_package_name(monkeypatch):
    module = installer(); item = release(b'package'); asset = item['assets'][0]; asset['digest'] = None
    side = dict(name=asset['name']+'.sha256', browser_download_url=asset['browser_download_url']+'.sha256')
    monkeypatch.setattr(module, 'read_small', lambda *args: (hashlib.sha256(b'package').hexdigest()+'  '+asset['name']+'\n').encode())
    assert module.checksum(asset, item['tag_name'], [side]) == hashlib.sha256(b'package').hexdigest()
    monkeypatch.setattr(module, 'read_small', lambda *args: (hashlib.sha256(b'package').hexdigest()+'  other.zip\n').encode())
    with pytest.raises(ValueError, match='checksum file'):
        module.checksum(asset, item['tag_name'], [side])


@pytest.mark.parametrize('suffix,machine', [('linux-x64','x86_64'), ('linux-arm64','aarch64')])
def test_single_installer_hands_matching_package_to_shared_launcher(tmp_path, monkeypatch, suffix, machine):
    module = installer(); data = archive(suffix); item = release(data, suffix)
    monkeypatch.setattr(module.platform, 'system', lambda: 'Linux')
    monkeypatch.setattr(module.platform, 'machine', lambda: machine)
    def request(url):
        if url == module.API + '1': return io.BytesIO(json.dumps([item]).encode())
        assert url == item['assets'][0]['browser_download_url']
        return io.BytesIO(data)
    monkeypatch.setattr(module, 'request', request)
    calls = []
    def run(command, **kwargs):
        calls.append(command)
        assert Path(command[0]).read_bytes()[18] == (183 if suffix == 'linux-arm64' else 62)
        assert Path(command[3]).read_bytes() == data
        assert command[1:] == ['--install-launcher', str((tmp_path/'installation with spaces').resolve()), command[3], '1.8.0', '--no-launch']
        return types.SimpleNamespace(returncode=0)
    monkeypatch.setattr(module.subprocess, 'run', run)
    assert module.main(['--install-dir', str(tmp_path/'installation with spaces'), '--no-launch']) == 0
    assert len(calls) == 1
    assert not (tmp_path/'installation with spaces').exists(), 'Bootstrap bypassed the launcher transaction'


def test_unpublished_arm64_leaves_installation_untouched(tmp_path, monkeypatch):
    module = installer(); item = release(archive())
    monkeypatch.setattr(module.platform, 'system', lambda: 'Linux')
    monkeypatch.setattr(module.platform, 'machine', lambda: 'aarch64')
    monkeypatch.setattr(module, 'read_small', lambda *a: json.dumps([item]).encode())
    monkeypatch.setattr(module.subprocess, 'run', lambda *a, **k: pytest.fail('x64 fallback executed'))
    with pytest.raises(ValueError, match='No stable linux-arm64'):
        module.main(['--install-dir', str(tmp_path/'installation')])
    assert not (tmp_path/'installation').exists()
