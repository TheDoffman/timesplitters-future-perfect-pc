"""Linux SDK integrity/isolation contracts runnable on every development host."""
import importlib.util
import io
import json
import shutil
import subprocess
from pathlib import Path
import sys
import tarfile

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from project_paths import configure
configure()
import linux_vulkan_sdk as sdk


@pytest.mark.skipif(sys.platform != 'linux' or not shutil.which('patch'),
                    reason='The Linux driver builder requires GNU patch')
def test_wsl_driver_patch_applies_to_pristine_source_and_preserves_repeat(tmp_path):
    spec = importlib.util.spec_from_file_location('wsl_driver_builder', ROOT / 'tools/build/build-wsl-vulkan.py')
    builder = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(builder)
    target = tmp_path / 'src/microsoft/vulkan/dzn_device.c'
    target.parent.mkdir(parents=True)
    pristine = ''.join(
        '                       VK_QUEUE_TRANSFER_BIT,\n'
        f'         .queueCount = {count},\n'
        '         .timestampValidBits = 64,\n'
        '         .minImageTransferGranularity = { 0, 0, 0 },\n'
        '      },\n'
        '      .desc = {\n'
        f'         .Type = D3D12_COMMAND_LIST_TYPE_{kind},\n'
        for count, kind in ((4, 'DIRECT'), (8, 'COMPUTE')))
    target.write_text(pristine)
    patch = ROOT / 'tools/build/mesa-dzn-granularity.diff'
    builder.apply_driver_patch(tmp_path, patch)
    expected = pristine.replace('{ 0, 0, 0 }', '{ 1, 1, 1 }')
    assert target.read_text() == expected
    builder.apply_driver_patch(tmp_path, patch)
    assert target.read_text() == expected
    unknown = pristine.replace('{ 0, 0, 0 }', '{ 9, 9, 9 }')
    target.write_text(unknown)
    with pytest.raises(subprocess.CalledProcessError):
        builder.apply_driver_patch(tmp_path, patch)
    assert target.read_text() == unknown


@pytest.mark.parametrize('driver,bridge_present', [('libvulkan_dzn.so',True),('libvulkan_dzn.so',False),('libvulkan_lvp.so',True)])
def test_explicit_driver_preserves_tunables_and_limits_wsl_workaround(tmp_path,monkeypatch,driver,bridge_present):
    bridge=tmp_path/'wsl';bridge.mkdir()
    if bridge_present:(bridge/'libd3d12.so').touch()
    manifest=tmp_path/'icd.json';manifest.write_text(json.dumps({'ICD':{'library_path':driver}}))
    monkeypatch.setattr(sdk,'Path',lambda value:bridge if str(value)=='/usr/lib/wsl/lib' else Path(value))
    env={'LD_LIBRARY_PATH':'sdk','GLIBC_TUNABLES':'glibc.malloc.trim_threshold=2048:glibc.cpu.hwcaps=-AVX512F'}
    sdk.select_driver(env,manifest)
    assert env['VK_DRIVER_FILES']==str(manifest.resolve())
    assert 'glibc.malloc.trim_threshold=2048' in env['GLIBC_TUNABLES']
    assert '-AVX512F' in env['GLIBC_TUNABLES']
    assert ('-ERMS,-FSRM' in env['GLIBC_TUNABLES'])==(bridge_present and 'dzn' in driver)
    assert env['LD_LIBRARY_PATH']==('sdk:'+str(bridge) if bridge_present else 'sdk')


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


def test_existing_full_receipt_checks_used_components_and_license(tmp_path, monkeypatch):
    fixture(tmp_path, monkeypatch)
    installed = sdk.setup(tmp_path)
    path = installed.parent / 'tsfp-receipt.json'
    receipt = json.loads(path.read_text())
    # Older receipts included optional tools; their removal must not require
    # downloading an SDK again when all build/runtime dependencies still match.
    receipt['files']['x86_64/bin/unused-sdk-tool'] = '0' * 64
    path.write_text(json.dumps(receipt))
    assert sdk.verify_install(tmp_path) == installed
    (installed.parent / 'LICENSE.txt').write_text('changed license')
    with pytest.raises(RuntimeError, match='file changed: LICENSE.txt'):
        sdk.verify_install(tmp_path)


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


def test_linux_generation_stages_only_executable_and_preserves_shared_assets(tmp_path):
    path=ROOT/'tools/build/build-native-linux.py'
    spec=importlib.util.spec_from_file_location('isolated_linux_builder',path)
    builder=importlib.util.module_from_spec(spec);spec.loader.exec_module(builder)
    shared=tmp_path/'shared';shared.mkdir();xbe=shared/'default.xbe';xbe.write_bytes(b'verified executable')
    symbols=shared/'default_xdk_symbols.json';symbols.write_bytes(b'preserve shared cache')
    expected=builder.sha256(xbe);root=tmp_path/'task';root.mkdir()
    before={p.name:(p.read_bytes(),p.stat().st_mtime_ns) for p in shared.iterdir()}
    staged=builder.stage_generation_input(root,xbe,expected)
    assert staged==root/'private/pipeline/input/default.xbe' and staged.read_bytes()==xbe.read_bytes()
    assert before=={p.name:(p.read_bytes(),p.stat().st_mtime_ns) for p in shared.iterdir()}
    stamp=staged.stat().st_mtime_ns
    assert builder.stage_generation_input(root,xbe,expected)==staged and staged.stat().st_mtime_ns==stamp
    staged.write_bytes(b'unknown existing data')
    with pytest.raises(ValueError,match='preserve and inspect'):
        builder.stage_generation_input(root,xbe,expected)
    assert staged.read_bytes()==b'unknown existing data'
    other=tmp_path/'other-task';other.mkdir()
    with pytest.raises(ValueError,match='changed before staging'):
        builder.stage_generation_input(other,xbe,'0'*64)
    assert not list((other/'private/pipeline/input').iterdir())


def test_sdk_versions_match_and_linux_archive_has_integrity_pin():
    spec = sdk.pin(ROOT)
    assert spec['platform'] == 'Linux-x86_64'
    assert len(spec['sha256']) == 64 and spec['bytes'] > 0
    assert spec['url'].startswith('https://sdk.lunarg.com/')
