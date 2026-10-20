"""Interactive launcher preserves settings; tests never touch personal paths."""
import importlib.util
import io
import sys
from pathlib import Path
import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
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
                                    'RECOMP_LOG_FILE':'/personal/log', 'TSFP_NATIVE_TEST_KEYS':'stale', 'TSFP_NATIVE_TRACE_RENDER':'stale'}, tmp_path)
    assert env['RECOMP_LOG_FILE'] == str(tmp_path / 'logs/native-play.err')
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


@pytest.mark.parametrize('driver,filename',[(None,'libkosmickrisp_icd.json'),
    ('moltenvk','MoltenVK_icd.json'),('kosmickrisp','libkosmickrisp_icd.json')])
def test_manifest_gpu_routing_preserves_personal_paths_without_writes(tmp_path,monkeypatch,driver,filename):
    import vulkan_sdk
    calls=[]
    def verify(root):
        calls.append(root)
        return root/'sdk'
    monkeypatch.setattr(vulkan_sdk,'verify_install',verify)
    base=launcher.game_environment({'DYLD_INSERT_LIBRARIES':'stale','VK_DRIVER_FILES':'stale'},tmp_path,120)
    manifest={'renderer':'vulkan'}
    if driver is not None:manifest['vulkan_driver']=driver
    env=launcher.renderer_environment(base,tmp_path,manifest)
    assert calls==[tmp_path]
    assert env['VK_DRIVER_FILES']==str(tmp_path/'sdk/share/vulkan/icd.d'/filename)
    assert 'DYLD_INSERT_LIBRARIES' not in env
    for key in ('RECOMP_SAVE_DIR','RECOMP_INPUT_CONFIG','RECOMP_DISPLAY_CONFIG','TSFP_DISPLAY_CONFIG',
                'XDG_CONFIG_HOME','XDG_DATA_HOME','RECOMP_FRAME_INTERP','RECOMP_FPS_CAP'):
        assert env[key]==base[key]
    assert not list(tmp_path.iterdir())
    for manifest in ({},{'renderer':'gl'}):
        gl=launcher.renderer_environment(base,tmp_path,manifest)
        assert 'VK_DRIVER_FILES' not in gl and 'DYLD_INSERT_LIBRARIES' not in gl
    assert calls==[tmp_path]  # GL needs no Vulkan SDK
    with pytest.raises(ValueError,match='Unsupported native renderer'):
        launcher.renderer_environment(base,tmp_path,{'renderer':'unknown'})


def test_vulkan_receipt_failure_stops_before_settings_creation(tmp_path,monkeypatch):
    import vulkan_sdk
    def reject(root):raise RuntimeError('installed file changed')
    monkeypatch.setattr(vulkan_sdk,'verify_install',reject)
    with pytest.raises(ValueError,match='installed file changed'):
        launcher.renderer_environment({},tmp_path,{'renderer':'vulkan'})
    assert not list(tmp_path.iterdir())


def test_check_validates_vulkan_without_preparing_or_starting_game(tmp_path,monkeypatch):
    import hashlib
    import json
    import vulkan_sdk
    build=tmp_path/'build/macos';build.mkdir(parents=True)
    exe=build/'futureperfect_recomp';exe.write_bytes(b'fixture executable')
    (build/'BUILD.json').write_text(json.dumps(dict(renderer='vulkan',
        executable_sha256=hashlib.sha256(exe.read_bytes()).hexdigest())))
    config=tmp_path/'config';config.mkdir()
    (config/'pc-controls.default.json').write_text('{}')
    before={p.relative_to(tmp_path):p.read_bytes() for p in tmp_path.rglob('*') if p.is_file()}
    monkeypatch.setattr(launcher,'ROOT',tmp_path)
    monkeypatch.setattr(launcher,'check_game',lambda root:None)
    monkeypatch.setattr(launcher.platform,'system',lambda:'Darwin')
    monkeypatch.setattr(launcher.platform,'machine',lambda:'arm64')
    monkeypatch.setattr(launcher.subprocess,'check_output',lambda *a,**kw:'Mach-O 64-bit executable arm64')
    checks=[]
    def verify(root):checks.append(root);return root/'sdk'
    monkeypatch.setattr(vulkan_sdk,'verify_install',verify)
    def unexpected(*a,**kw):pytest.fail('--check must not prepare settings or run a child')
    monkeypatch.setattr(launcher,'prepare_settings',unexpected)
    monkeypatch.setattr(launcher.subprocess,'Popen',unexpected)
    monkeypatch.setattr(sys,'argv',['play-native-mac.py','--check'])
    monkeypatch.setattr(sys,'dont_write_bytecode',False)
    assert launcher.main()==0 and checks==[tmp_path]
    assert before=={p.relative_to(tmp_path):p.read_bytes() for p in tmp_path.rglob('*') if p.is_file()}
    assert not (tmp_path/'private').exists()
