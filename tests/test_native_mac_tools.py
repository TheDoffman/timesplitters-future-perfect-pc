"""Regressions for isolation and bounded cleanup, without launching the game."""
import importlib.util
import os
from pathlib import Path
import sys
import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import native_mac_common as common
spec = importlib.util.spec_from_file_location('native_probe', ROOT / 'tools/test-native-mac.py')
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)


def test_all_runtime_settings_are_isolated(tmp_path, monkeypatch):
    monkeypatch.setenv('RECOMP_DISPLAY_CONFIG', '/personal/display.conf')
    monkeypatch.setenv('RECOMP_INPUT_SEQ', 'stale automation')
    monkeypatch.setenv('TSFP_PC_INPUT_REPLAY', '/personal/replay')
    test = tmp_path / 'private/test-saves/native-mac'
    env = probe.isolated_environment(tmp_path, test)
    for key in ('RECOMP_SAVE_DIR', 'RECOMP_INPUT_CONFIG', 'RECOMP_DISPLAY_CONFIG',
                'TSFP_DISPLAY_CONFIG', 'XDG_CONFIG_HOME', 'XDG_DATA_HOME'):
        assert Path(env[key]).is_relative_to(test)
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


def test_cleanup_refuses_symlink(tmp_path):
    test = tmp_path / 'private/test-saves/native-mac'
    test.mkdir(parents=True)
    personal = tmp_path / 'important.img'
    personal.write_bytes(b'keep')
    (test / 'Partition0.img').symlink_to(personal)
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
    link.symlink_to(target)
    with pytest.raises(RuntimeError, match='Unexpected native capture file'):
        probe.capture_environment(tmp_path)
    assert target.read_bytes() == b'keep'
