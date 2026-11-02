"""Linux SDK integrity/isolation contracts runnable on every development host."""
import importlib.util
import io
from pathlib import Path
import sys
import tarfile

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from project_paths import configure
configure()
import linux_vulkan_sdk as sdk


def fixture(root, monkeypatch, extra=()):
    download = root / 'external/vulkan-sdk/download'
    download.mkdir(parents=True)
    archive = download / 'sdk.tar.xz'
    files = {name: ('fixture ' + name).encode() for name in sdk.CRITICAL}
    files['LICENSE.txt'] = b'SDK license fixture'
    files.update(extra)
    with tarfile.open(archive, 'w:xz') as bundle:
        for name, data in files.items():
            entry = tarfile.TarInfo(name if name.startswith('/') else '1.0/' + name)
            entry.size = len(data)
            bundle.addfile(entry, io.BytesIO(data))
    spec = dict(version='1.0', archive=archive.name, bytes=archive.stat().st_size,
                sha256=sdk.sha256(archive), url='https://example.invalid/no-network')
    monkeypatch.setattr(sdk, 'pin', lambda root=root: spec)
    return archive, spec


def test_verified_sdk_install_retains_licenses_and_rejects_changed_files(tmp_path, monkeypatch):
    archive, spec = fixture(tmp_path, monkeypatch)
    installed = sdk.setup(tmp_path)
    assert installed == tmp_path / 'external/vulkan-sdk/linux/1.0/x86_64'
    assert not archive.exists()
    assert (installed.parent / 'LICENSE.txt').read_bytes() == b'SDK license fixture'
    assert sdk.verify_install(tmp_path) == installed
    (installed / 'lib/libdxcompiler.so').write_bytes(b'changed')
    with pytest.raises(RuntimeError, match='file changed'):
        sdk.verify_install(tmp_path)
    with pytest.raises(RuntimeError, match='file changed'):
        sdk.setup(tmp_path)  # Do not overwrite an existing installation.


def test_sdk_rejects_corrupt_download_before_extract(tmp_path, monkeypatch):
    archive, spec = fixture(tmp_path, monkeypatch)
    archive.write_bytes(b'bad download')
    with pytest.raises(RuntimeError, match='publisher'):
        sdk.setup(tmp_path)
    assert not sdk.install_path(tmp_path).exists()


@pytest.mark.parametrize('name', ['../../outside', '/absolute', 'x86_64/../../../outside'])
def test_sdk_rejects_unsafe_archive_paths(tmp_path, monkeypatch, name):
    fixture(tmp_path, monkeypatch, [(name, b'escape')])
    with pytest.raises((RuntimeError, tarfile.FilterError)):
        sdk.setup(tmp_path)
    assert not (tmp_path / 'outside').exists()


def test_probe_isolates_user_state_and_keeps_desktop_session(tmp_path, monkeypatch):
    for name, value in dict(RECOMP_SAVE_DIR='personal-save', TSFP_DISPLAY_CONFIG='personal.ini',
                            VK_DRIVER_FILES='wrong-driver', LD_PRELOAD='wrong-library',
                            DISPLAY=':9', WAYLAND_DISPLAY='wayland-1', XDG_RUNTIME_DIR='/run/user/1000').items():
        monkeypatch.setenv(name, value)
    env = sdk.probe_environment(tmp_path / 'sdk', tmp_path)
    assert 'VK_DRIVER_FILES' not in env and 'LD_PRELOAD' not in env
    assert env['DISPLAY'] == ':9' and env['WAYLAND_DISPLAY'] == 'wayland-1'
    assert env['XDG_RUNTIME_DIR'] == '/run/user/1000'
    for name in ('RECOMP_SAVE_DIR', 'RECOMP_INPUT_CONFIG', 'RECOMP_DISPLAY_CONFIG',
                 'TSFP_DISPLAY_CONFIG', 'XDG_CACHE_HOME', 'XDG_CONFIG_HOME', 'XDG_DATA_HOME'):
        assert Path(env[name]).is_relative_to(tmp_path / 'build/linux/rhi-tests/runtime')
    assert env['RECOMP_VK_VALIDATION'] == '1'


def test_linux_host_guard_exits_before_sdk_or_runtime_changes(monkeypatch):
    path = ROOT / 'tools/testing/test-rhi-linux.py'
    spec = importlib.util.spec_from_file_location('linux_rhi_runner', path)
    runner = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(runner)
    monkeypatch.setattr(sys, 'argv', [str(path), '--prepare-runtime', '--setup-sdk'])
    monkeypatch.setattr(runner.platform, 'system', lambda: 'Windows')
    monkeypatch.setattr(sdk, 'setup', lambda: pytest.fail('SDK mutated on unsupported host'))
    with pytest.raises(SystemExit) as error:
        runner.main()
    assert error.value.code == 2


def test_sdk_versions_match_and_linux_archive_has_integrity_pin():
    spec = sdk.pin(ROOT)
    assert spec['platform'] == 'Linux-x86_64'
    assert len(spec['sha256']) == 64 and spec['bytes'] > 0
    assert spec['url'].startswith('https://sdk.lunarg.com/')
