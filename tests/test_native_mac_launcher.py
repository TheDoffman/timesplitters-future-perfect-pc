"""Interactive launcher preserves settings; tests never touch personal paths."""
import importlib.util
import io
from pathlib import Path
import pytest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('play_native', ROOT / 'tools/play-native-mac.py')
launcher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(launcher)


@pytest.mark.parametrize('value', ['NaN', 'Infinity', '-1', '0', '10.1', 'true', '"0.1"'])
def test_invalid_mouse_settings_stop_before_launch(tmp_path, value):
    path = tmp_path / 'controls.json'
    path.write_text('{"mouse":{"sensitivity":' + value + '}}')
    with pytest.raises(ValueError, match='Mouse sensitivity'):
        launcher.read_controls(path)


def test_existing_fixture_is_supported():
    assert launcher.read_controls(ROOT / 'tests/fixtures/pc-controls.json') == (0.1, True, False)


def test_native_routes_and_stale_automation(tmp_path):
    env = launcher.game_environment({'PATH':'/usr/bin', 'RECOMP_INPUT_SEQ':'stale',
                                    'TSFP_NATIVE_TEST_KEYS':'stale', 'TSFP_NATIVE_TRACE_RENDER':'stale'}, tmp_path)
    assert env['RECOMP_SAVE_DIR'] == str(tmp_path / 'private/saves')
    assert env['RECOMP_INPUT_CONFIG'] == str(tmp_path / 'config/pc-controls.json')
    assert env['TSFP_DISPLAY_CONFIG'] == str(tmp_path / 'config/pc-display.ini')
    assert Path(env['RECOMP_DISPLAY_CONFIG']).is_relative_to(tmp_path)
    assert not any(k in env for k in ('RECOMP_INPUT_SEQ', 'TSFP_NATIVE_TEST_KEYS', 'TSFP_NATIVE_TRACE_RENDER'))


def test_existing_settings_survive_launch_preparation(tmp_path):
    (tmp_path / 'config').mkdir()
    (tmp_path / 'config/pc-controls.default.json').write_text('default')
    launcher.prepare_settings(tmp_path)
    controls = tmp_path / 'config/pc-controls.json'
    assert controls.read_text() == 'default'
    controls.write_text('custom controls')
    display = tmp_path / 'config/pc-display.ini'
    display.write_text('custom display')
    runtime = tmp_path / 'private/native-settings/runtime.conf'
    runtime.write_text('custom runtime')
    save = tmp_path / 'private/saves/profile'
    save.write_bytes(b'keep')
    launcher.prepare_settings(tmp_path)
    assert controls.read_text() == 'custom controls'
    assert display.read_text() == 'custom display'
    assert runtime.read_text() == 'custom runtime'
    assert save.read_bytes() == b'keep'


def test_log_cap_still_drains_pipe(tmp_path):
    stream = io.BytesIO(b'x'*200000)
    path = tmp_path / 'log'
    launcher.drain_log(stream, path, budget=100)
    assert path.read_bytes() == b'x'*100
    assert stream.closed


def test_fps_override_does_not_change_console_clock(tmp_path):
    for fps,multiple in ((60,'1'),(120,'2')):
        env=launcher.game_environment({'RECOMP_VBLANK_HZ':'144'},tmp_path,fps)
        assert env['RECOMP_FRAME_INTERP']==multiple
        assert env['RECOMP_FPS_CAP']==('60' if fps==120 else 'adaptive')
        assert 'RECOMP_VBLANK_HZ' not in env
    assert 'RECOMP_FRAME_INTERP' not in launcher.game_environment({},tmp_path)
