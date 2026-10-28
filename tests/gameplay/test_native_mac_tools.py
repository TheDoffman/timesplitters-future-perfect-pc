"""Regressions for isolation and bounded cleanup, without launching the game."""


import importlib.util
import hashlib
import json
import os
from pathlib import Path
import sys
import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import native_mac_common as common
spec = importlib.util.spec_from_file_location('native_probe', ROOT / 'tools/testing/test-native-mac.py')
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)

build_spec = importlib.util.spec_from_file_location('native_build', ROOT / 'tools/build/build-native-mac.py')
builder = importlib.util.module_from_spec(build_spec)
build_spec.loader.exec_module(builder)


@pytest.mark.parametrize('game', [None, b'unsupported revision'])
def test_native_build_accepts_focused_branches_but_validates_game(tmp_path, monkeypatch, game):
    (tmp_path / 'tsfp.ps1').write_bytes((ROOT / 'tsfp.ps1').read_bytes())
    if game is not None:
        path = tmp_path / 'private/game/default.xbe'
        path.parent.mkdir(parents=True)
        path.write_bytes(game)
    monkeypatch.setattr(builder, 'ROOT', tmp_path)
    monkeypatch.setattr(builder.platform, 'system', lambda: 'Darwin')
    monkeypatch.setattr(builder.platform, 'machine', lambda: 'arm64')
    monkeypatch.setattr(sys, 'argv', ['build-native-mac.py'])
    # A focused branch is allowed; missing/wrong data must stop before setup,
    # generation or configuration, without inspecting or switching branches.
    def unexpected(*args, **kwargs):
        pytest.fail('Build touched tools or Git before verifying its game input')
    monkeypatch.setattr(builder.subprocess, 'check_output', unexpected)
    monkeypatch.setattr(builder, 'run', unexpected)
    monkeypatch.setattr(builder, 'ensure_pack', unexpected)
    message = 'Missing private/game/default.xbe' if game is None else 'Unsupported XBE'
    with pytest.raises(SystemExit, match=message):
        builder.main()
    assert not (tmp_path / '.venv').exists()
    assert not (tmp_path / 'build').exists()


@pytest.mark.parametrize('requested,version,expected', [
    ('auto', '26.0', 'vulkan'), ('auto', '27.0.1', 'vulkan'),
    ('auto', '15.7', 'gl'), ('vulkan', '26.3', 'vulkan'),
    ('gl', '27.0.1', 'gl'), ('gl', '', 'gl'),
])
def test_native_renderer_uses_shared_frontend_on_supported_hosts(requested, version, expected):
    assert builder.select_renderer(requested, version, 26) == expected


@pytest.mark.parametrize('requested,version', [('auto', ''), ('auto', 'unknown'), ('vulkan', '15.7')])
def test_native_renderer_rejects_unknown_or_unsupported_sdk_hosts(requested, version):
    with pytest.raises(RuntimeError, match='--renderer gl'):
        builder.select_renderer(requested, version, 26)


@pytest.mark.parametrize('setup', [False, True])
def test_native_builder_rejects_unverified_sdk_before_build_mutations(tmp_path, monkeypatch, setup):
    import vulkan_sdk
    # A synthetic game pin exercises preflight sequencing without private assets.
    game = tmp_path / 'private/game/default.xbe'
    game.parent.mkdir(parents=True)
    game.write_bytes(b'test preflight input')
    (tmp_path / 'tsfp.ps1').write_text("$revision = '" + 'a' * 40 + "'\n$expectedHash = '" +
                                     hashlib.sha256(game.read_bytes()).hexdigest().upper() + "'\n")
    (tmp_path / 'config/build').mkdir(parents=True)
    (tmp_path / 'config/build/vulkan-sdk.json').write_text(json.dumps(dict(minimum_macos=26)))
    monkeypatch.setattr(builder, 'ROOT', tmp_path)
    monkeypatch.setattr(builder.platform, 'system', lambda: 'Darwin')
    monkeypatch.setattr(builder.platform, 'machine', lambda: 'arm64')
    monkeypatch.setattr(builder.platform, 'mac_ver', lambda: ('27.0.1', (), 'arm64'))
    monkeypatch.setattr(sys, 'argv', ['build-native-mac.py'] + (['--setup'] if setup else []))
    calls = []
    def reject(root):
        calls.append(root)
        raise RuntimeError('SDK receipt invalid')
    def unexpected(*args, **kwargs):
        pytest.fail('Mutated build or used wrong SDK operation before verification')
    monkeypatch.setattr(vulkan_sdk, 'setup' if setup else 'verify_install', reject)
    monkeypatch.setattr(vulkan_sdk, 'verify_install' if setup else 'setup', unexpected)
    monkeypatch.setattr(builder, 'run', unexpected)
    monkeypatch.setattr(builder.subprocess, 'check_output', unexpected)
    with pytest.raises(SystemExit, match='SDK receipt invalid'):
        builder.main()
    assert calls == [tmp_path]
    assert not (tmp_path / '.venv').exists()
    assert not (tmp_path / 'build').exists()


def test_all_runtime_settings_are_isolated(tmp_path, monkeypatch):
    monkeypatch.setenv('RECOMP_LOG_FILE', '/personal/log')
    monkeypatch.setenv('RECOMP_DISPLAY_CONFIG', '/personal/display.conf')
    monkeypatch.setenv('RECOMP_ICALL_FEEDBACK_PATH', '/personal/feedback.dump')
    monkeypatch.setenv('RECOMP_INPUT_SEQ', 'stale automation')
    monkeypatch.setenv('TSFP_PC_INPUT_REPLAY', '/personal/replay')
    test = tmp_path / 'private/test-saves/native-mac'
    env = probe.isolated_environment(tmp_path, test)
    assert env['RECOMP_ICALL_FEEDBACK_PATH']==str(test/'icall_targets.dump')
    for key in ('RECOMP_SAVE_DIR', 'RECOMP_INPUT_CONFIG', 'RECOMP_DISPLAY_CONFIG',
                'TSFP_DISPLAY_CONFIG', 'XDG_CONFIG_HOME', 'XDG_DATA_HOME', 'XDG_CACHE_HOME'):
        assert Path(env[key]).is_relative_to(test)
    assert env['RECOMP_LOG_FILE'] == str(tmp_path / 'logs/native-boot.err')
    assert 'RECOMP_INPUT_SEQ' not in env
    assert 'TSFP_PC_INPUT_REPLAY' not in env


def test_cleanup_refuses_personal_directory(tmp_path):
    personal = tmp_path / 'private/saves'
    personal.mkdir(parents=True)
    image = personal / 'Partition0.img'
    image.write_bytes(b'keep')
    with pytest.raises(RuntimeError, match='unexpected test directory'):
        common.recycle_test_disks(tmp_path, personal)
    assert image.read_bytes() == b'keep'


def create_symlink(link, target):
    try:
        link.symlink_to(target)
    except OSError as error:
        if os.name == 'nt' and error.winerror == 1314:
            pytest.skip('Windows account lacks symlink creation privileges')
        raise


def test_cleanup_refuses_symlink(tmp_path):
    test = tmp_path / 'private/test-saves/native-mac'
    test.mkdir(parents=True)
    personal = tmp_path / 'important.img'
    personal.write_bytes(b'keep')
    create_symlink(test / 'Partition0.img', personal)
    with pytest.raises(RuntimeError, match='unexpected partition path'):
        common.recycle_test_disks(tmp_path, test)
    assert personal.read_bytes() == b'keep'


@pytest.mark.skipif(os.name == 'nt', reason='macOS Trash allocation requires stat.st_blocks')
def test_cleanup_preserves_metadata_and_recycles_images(tmp_path, monkeypatch):
    test = tmp_path / 'private/test-saves/native-mac'
    test.mkdir(parents=True)
    (test / 'profile.dat').write_bytes(b'keep')
    (test / 'Partition0.img').write_bytes(b'disposable')
    monkeypatch.setattr(Path, 'home', classmethod(lambda cls: tmp_path / 'home'))
    result = common.recycle_test_disks(tmp_path, test)
    assert result['files'] == 1 and result['logical_bytes'] == 10
    assert (test / 'profile.dat').read_bytes() == b'keep'
    assert not (test / 'Partition0.img').exists()
    assert (Path(result['recycled_to']) / 'Partition0.img').read_bytes() == b'disposable'


def test_capture_is_bounded_and_refuses_symlinks(tmp_path):
    env = probe.capture_environment(tmp_path)
    assert env['RECOMP_HLE_D3D8_DUMP_KEEP_LAST'] == '2'
    assert Path(env['RECOMP_HLE_D3D8_DUMP']).parent == tmp_path / 'private/captures/native-mac'
    target = tmp_path / 'keep.bmp'
    target.write_bytes(b'keep')
    link = tmp_path / 'private/captures/native-mac/frame000.bmp'
    create_symlink(link, target)
    with pytest.raises(RuntimeError, match='Unexpected native capture file'):
        probe.capture_environment(tmp_path)
    assert target.read_bytes() == b'keep'


@pytest.fixture
def unlocked_fixture(tmp_path, monkeypatch):
    import hashlib
    import struct
    import testing_profile as profile
    game = tmp_path / 'private/game/default.xbe'
    game.parent.mkdir(parents=True)
    game.write_bytes(b'test-only executable')
    monkeypatch.setattr(profile, 'XBE_SHA256', hashlib.sha256(game.read_bytes()).hexdigest())
    seed = tmp_path / 'private/test-saves/seed'
    title = seed / 'UserData/45410066'
    slot = title / '16D15BD8D404'
    slot.mkdir(parents=True)
    body = bytearray(profile.BODY_SIZE)
    struct.pack_into('<I', body, 0x1EE0, 2)
    (slot / 'tsftrpft').write_bytes(body + profile.signature(body))
    for name in ('TitleMeta.xbx', 'TitleImage.xbx', 'SaveImage.xbx'):
        (title / name).write_bytes(b'test-only metadata')
    controls = tmp_path / 'tests/fixtures/pc-controls.json'
    controls.parent.mkdir(parents=True)
    controls.write_text('{}')
    fixture = profile.create_fixture(tmp_path, seed)
    return profile, fixture


def test_native_unlocked_copy_isolated_reset_and_locked(tmp_path, monkeypatch, unlocked_fixture):
    profile, fixture = unlocked_fixture
    before = {p.relative_to(fixture):p.read_bytes() for p in fixture.rglob('*') if p.is_file()}
    monkeypatch.setenv('XDG_CONFIG_HOME', '/personal/config')
    monkeypatch.setenv('XDG_DATA_HOME', '/personal/data')
    monkeypatch.setenv('TSFP_PC_INPUT_REPLAY', '/personal/replay')
    test = tmp_path / 'private/test-saves/native-mac'
    test.mkdir(parents=True)
    old_save = test / 'existing-profile'
    old_save.write_bytes(b'preserve')
    for iteration in range(2):
        with probe.test_session(tmp_path, test, unlocked=True) as env:
            run = test / 'unlocked'
            assert env['RECOMP_ICALL_FEEDBACK_PATH']==str(run/'icall_targets.dump')
            for key in ('RECOMP_SAVE_DIR', 'RECOMP_INPUT_CONFIG', 'RECOMP_DISPLAY_CONFIG',
                        'TSFP_DISPLAY_CONFIG', 'XDG_CONFIG_HOME', 'XDG_DATA_HOME', 'XDG_CACHE_HOME'):
                assert Path(env[key]).is_relative_to(run)
            assert 'TSFP_PC_INPUT_REPLAY' not in env
            assert env['RECOMP_XMV_PLAY'] == '0'
            assert env['TSFP_TEXTURE_PACK_DIR'] == str(tmp_path / 'assets/textures')
            assert env['TSFP_TEXTURE_MODE'] == 'classic'
            saved = next(Path(env['RECOMP_SAVE_DIR']).rglob('tsftrpft'))
            assert saved.read_bytes() == next(fixture.rglob('tsftrpft')).read_bytes()
            saved.write_bytes(b'progress from disposable session')
            controls = Path(env['RECOMP_INPUT_CONFIG'])
            if iteration:
                assert controls.read_text() == '{"custom":true}'
            controls.write_text('{"custom":true}')
            with pytest.raises(FileExistsError):
                with probe.test_session(tmp_path, test, unlocked=True):
                    pytest.fail('Concurrent session was allowed')
            with pytest.raises(ValueError, match='in use'):
                profile.prepare_run(tmp_path, run)
            assert (test / 'run.lock').exists()
        assert not (test / 'run.lock').exists()
        assert not (test / 'unlocked/run.lock').exists()
    assert before == {p.relative_to(fixture):p.read_bytes() for p in fixture.rglob('*') if p.is_file()}
    assert old_save.read_bytes() == b'preserve'


def test_native_missing_fixture_does_not_launch_or_leave_lock(tmp_path):
    test = tmp_path / 'private/test-saves/native-mac'
    with pytest.raises(FileNotFoundError):
        with probe.test_session(tmp_path, test, unlocked=True):
            pytest.fail('Missing fixture was accepted')
    assert not (test / 'run.lock').exists()


def test_native_session_failure_cleans_disks_before_releasing_lock(tmp_path, monkeypatch, unlocked_fixture):
    test = tmp_path / 'private/test-saves/native-mac'
    calls = []
    def cleanup(root, directory):
        assert (test / 'run.lock').exists()
        assert (test / 'unlocked/run.lock').exists()
        calls.append(directory)
    monkeypatch.setattr(probe, 'recycle_test_disks', cleanup)
    with pytest.raises(RuntimeError, match='child startup failed'):
        with probe.test_session(tmp_path, test, unlocked=True):
            raise RuntimeError('child startup failed')
    assert calls == [test]
    assert not (test / 'run.lock').exists()
    assert not (test / 'unlocked/run.lock').exists()


def test_native_fixture_destination_refuses_symlink(tmp_path, unlocked_fixture):
    test = tmp_path / 'private/test-saves/native-mac'
    test.mkdir(parents=True)
    personal = tmp_path / 'private/saves'
    personal.mkdir(parents=True)
    create_symlink(test / 'unlocked', personal)
    with pytest.raises(ValueError, match='linked'):
        with probe.test_session(tmp_path, test, unlocked=True):
            pytest.fail('Linked destination was accepted')
    assert not list(personal.iterdir())
    assert not (test / 'run.lock').exists()


def test_native_fixture_rejects_redirected_xdg_config(tmp_path, unlocked_fixture):
    test = tmp_path / 'private/test-saves/native-mac'
    with probe.test_session(tmp_path, test, unlocked=True):
        pass
    personal = tmp_path / 'personal-config'
    personal.mkdir()
    create_symlink(test / 'unlocked/xdg-config', personal)
    with pytest.raises(ValueError, match='linked'):
        with probe.test_session(tmp_path, test, unlocked=True):
            pytest.fail('XDG redirect was accepted')
    assert not list(personal.iterdir())
    assert not (test / 'run.lock').exists()


def test_present_counts_require_actual_backend_evidence():
    assert probe.present_counts('[NATIVE-GUEST] completed frame 600') is None
    assert probe.present_counts('[PRESENT] backend=vulkan total=300 guest=180 interpolated=119 other=1 skipped=4 failed=2') == dict(total=300,guest=180,interpolated=119,other=1,skipped=4,failed=2)


def test_disabling_validation_requires_explicit_profiling(monkeypatch,capsys):
    monkeypatch.setattr(probe.platform,'system',lambda:'Darwin')
    monkeypatch.setattr(probe.platform,'machine',lambda:'arm64')
    monkeypatch.setattr(sys,'argv',['test-native-mac.py','--profile-without-validation'])
    with pytest.raises(SystemExit) as stopped:
        probe.main()
    assert stopped.value.code==2
    assert 'requires --profile-frames' in capsys.readouterr().err
