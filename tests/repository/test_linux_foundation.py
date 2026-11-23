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


def test_sdk_versions_match_and_linux_archive_has_integrity_pin():
    spec = sdk.pin(ROOT)
    assert spec['platform'] == 'Linux-x86_64'
    assert len(spec['sha256']) == 64 and spec['bytes'] > 0
    assert spec['url'].startswith('https://sdk.lunarg.com/')


def linux_renderer_runner():
    spec = importlib.util.spec_from_file_location(
        'linux_rhi_gl_runner', ROOT / 'tools/testing/test-rhi-linux.py')
    runner = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(runner)
    return runner


def test_gl_driver_environment_is_child_only_and_clears_stale_controls():
    runner = linux_renderer_runner()
    parent = dict(GALLIUM_DRIVER='wrong', LIBGL_ALWAYS_SOFTWARE='1',
                  MESA_LOADER_DRIVER_OVERRIDE='wrong', TSFP_TEST_GL41_VIEWS='1',
                  MESA_D3D12_DEFAULT_ADAPTER_NAME='wrong', DISPLAY=':9',
                  LD_LIBRARY_PATH='/pinned-sdk/lib', RECOMP_SAVE_DIR='/isolated/saves')
    original = parent.copy()
    env = runner.configure_gl_environment(parent, 'd3d12', 'NVIDIA')
    assert parent == original
    assert env['GALLIUM_DRIVER'] == 'd3d12'
    assert env['MESA_D3D12_DEFAULT_ADAPTER_NAME'] == 'NVIDIA'
    assert env['LD_LIBRARY_PATH'] == '/usr/lib/wsl/lib:/pinned-sdk/lib'
    assert env['RECOMP_SAVE_DIR'] == '/isolated/saves' and env['DISPLAY'] == ':9'
    assert 'LIBGL_ALWAYS_SOFTWARE' not in env
    assert 'TSFP_TEST_GL41_VIEWS' not in env
    env = runner.configure_gl_environment(parent, 'software', restricted=True)
    assert env['GALLIUM_DRIVER'] == 'llvmpipe'
    assert env['TSFP_TEST_GL41_VIEWS'] == '1'
    assert 'MESA_D3D12_DEFAULT_ADAPTER_NAME' not in env
    env = runner.configure_gl_environment(parent, 'system')
    assert 'GALLIUM_DRIVER' not in env and 'MESA_LOADER_DRIVER_OVERRIDE' not in env


def test_gl_receipt_requires_pixels_dispatch_cleanup_and_observed_driver():
    runner = linux_renderer_runner()
    markers = ['BACKEND: opengl', 'PASS: OpenGL typed null textures',
        'PASS: shared mirror-once sampling beyond both texture edges',
        'PASS: shared BC1/BC2/BC3 sampled colors and alpha',
        'PASS: packed BGRA vertex colors preserve shader channel ordering',
               'PASS: OpenGL native main-thread dispatch',
        'PASS: native GL owner dispatch progress observed',
               'PASS: OpenGL worker/context recreation and repeated cleanup',
               'D3D12 (NVIDIA)', 'PASS: OpenGL4.1 subset/']
    output = '\n'.join(markers)
    runner.require_gl_contract(output, 'd3d12', True)
    for marker in markers:
        with pytest.raises(RuntimeError):
            runner.require_gl_contract(output.replace(marker, ''), 'd3d12', True)
    with pytest.raises(RuntimeError):
        runner.require_gl_contract(output + '\n[RHI] ERROR invalid resource', 'd3d12')
    with pytest.raises(RuntimeError):
        runner.require_gl_contract(output, 'software')
    runner.require_gl_contract(output.replace('D3D12 (NVIDIA)', 'llvmpipe'), 'software')


def test_native_recovery_faults_are_isolated_from_driver_and_settings(tmp_path):
    runner = linux_renderer_runner()
    parent = dict(VK_DRIVER_FILES='/installed/driver.json', VK_ICD_FILENAMES='/installed/driver.json',
                  RECOMP_SAVE_DIR='/isolated/saves', RECOMP_DISPLAY_CONFIG='/isolated/runtime.ini',
                  LD_LIBRARY_PATH='/pinned-sdk/lib', RECOMP_VK_VALIDATION='1')
    original = parent.copy()
    cases = dict(runner.recovery_cases(parent, tmp_path))
    assert parent == original and len(cases) == 8
    assert cases['vulkan-success']['VK_DRIVER_FILES'] == '/installed/driver.json'
    assert cases['no-driver']['VK_DRIVER_FILES'] == str(tmp_path/'runtime/missing-driver.json')
    assert cases['no-device']['VK_LOADER_VENDOR_ID_FILTER'] == '0xffff'
    assert cases['both-fail']['LD_LIBRARY_PATH'].startswith(str(tmp_path/'dxc-unavailable') + ':')
    assert cases['partial-fail']['TSFP_TEST_EMPTY_DRAWABLE'] == '1'
    for mode, env in cases.items():
        assert env['RECOMP_SAVE_DIR'] == parent['RECOMP_SAVE_DIR']
        assert env['RECOMP_DISPLAY_CONFIG'] == parent['RECOMP_DISPLAY_CONFIG']
        assert env['RECOMP_D3D8_BACKEND'] == 'vulkan'
        assert ('TSFP_TEST_EXPECT_STARTUP_FAILURE' in env) == (mode in ('both-fail', 'partial-fail', 'no-core', 'no-s3tc', 'no-mirror-once'))
    for mode,capability in [('no-core','core'),('no-s3tc','s3tc'),('no-mirror-once','mirror-once')]:
        assert cases[mode]['TSFP_TEST_GL_MISSING_CAPABILITY'] == capability
        assert cases[mode]['VK_DRIVER_FILES'] == str(tmp_path/'runtime/missing-driver.json')


def test_native_recovery_receipt_rejects_failed_window_cleanup_or_continued_render(monkeypatch):
    runner = linux_renderer_runner()
    positive = '\n'.join(('BACKEND: opengl', 'PASS: OpenGL typed null textures',
        'PASS: shared mirror-once sampling beyond both texture edges',
        'PASS: shared BC1/BC2/BC3 sampled colors and alpha',
        'PASS: packed BGRA vertex colors preserve shader channel ordering',
        'PASS: OpenGL native main-thread dispatch',
        'PASS: native GL owner dispatch progress observed',
        'PASS: OpenGL worker/context recreation and repeated cleanup',
        'D3D12 (NVIDIA)', 'startup recovery selected opengl',
        'PASS: native automatic drawable replacement', 'PASS: shared RHI'))
    runner.require_recovery_case('no-driver', positive, 'd3d12')
    with pytest.raises(RuntimeError):
        runner.require_recovery_case('no-device', positive.replace('PASS: native automatic drawable replacement', ''), 'd3d12')
    failure = ('PASS: native startup failed with no renderer ready and repeated cleanup\n'
               'Vulkan and OpenGL initialization failed\n'
               'shared DXC shader compiler is unavailable')
    runner.require_recovery_case('both-fail', failure, 'd3d12')
    for extra in ('[PRESENT]', 'PASS: shared RHI', 'startup recovery selected opengl'):
        with pytest.raises(RuntimeError):
            runner.require_recovery_case('both-fail', failure + extra, 'd3d12')
    with pytest.raises(RuntimeError):
        runner.require_recovery_case('both-fail', failure.replace('shared DXC shader compiler is unavailable', ''), 'd3d12')
    with pytest.raises(RuntimeError):
        runner.require_recovery_case('partial-fail', failure, 'd3d12')
    runner.require_recovery_case('partial-fail', failure + '\ndrawable target initialization failed\nlogical window retained', 'd3d12')
    for mode,reason in [('no-core','OpenGL4.1 core'),('no-s3tc','BC1/BC2/BC3 S3TC'),('no-mirror-once','mirror-once sampler')]:
        capability=failure+'\nrequired capability unavailable: '+reason
        with pytest.raises(RuntimeError):
            runner.require_recovery_case(mode, capability, 'd3d12')
        runner.require_recovery_case(mode, capability+'\nPASS: OpenGL capability rejection cleanup and context recreation', 'd3d12')
    validated = []
    monkeypatch.setattr(runner, 'require_clean_validation', lambda output: validated.append(output))
    primary = 'BACKEND: vulkan\nPASS: shared RHI'
    runner.require_recovery_case('vulkan-success', primary, 'd3d12')
    assert validated == [primary]
    with pytest.raises(RuntimeError):
        runner.require_recovery_case('vulkan-success', primary + '\nstartup recovery selected opengl', 'd3d12')
