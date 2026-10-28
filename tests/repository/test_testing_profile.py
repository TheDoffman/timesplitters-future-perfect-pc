"""Test save integrity, collectible preservation and isolation without game data."""


import importlib.util
from pathlib import Path
import struct
import hashlib

import pytest

spec=importlib.util.spec_from_file_location('testing_profile',Path(__file__).resolve().parents[2]/'tools/testing/testing_profile.py')
profile=importlib.util.module_from_spec(spec)
spec.loader.exec_module(profile)


def fresh():
    body=bytearray((i*17+3)%256 for i in range(profile.BODY_SIZE))
    struct.pack_into('<I',body,0x1EE0,2)
    return bytes(body)+profile.signature(body)


def test_collectibles_and_settings_preserved():
    original=fresh()
    unlocked=profile.unlock_profile(original)
    allowed=set(range(profile.PROGRESS+0xBF4,profile.PROGRESS+0xC4C))
    for i in range(48):allowed.update(range(profile.PROGRESS+0x49C+i*0x18,profile.PROGRESS+0x4A0+i*0x18))
    assert all(original[i]==unlocked[i] for i in range(profile.BODY_SIZE) if i not in allowed)
    assert unlocked[profile.PROGRESS+0xBF0:profile.PROGRESS+0xBF4]==original[profile.PROGRESS+0xBF0:profile.PROGRESS+0xBF4]
    assert profile.signature(unlocked[:-20])==unlocked[-20:]
    assert profile.unlock_profile(unlocked)==unlocked


@pytest.mark.parametrize('raw',[b'',fresh()[:-1],fresh()[:-20]+bytes(20)])
def test_corrupt_save_rejected(raw):
    with pytest.raises(ValueError):profile.unlock_profile(raw)


def test_unsupported_version_rejected():
    body=bytearray(fresh()[:-20]);struct.pack_into('<I',body,0x1EE0,3)
    with pytest.raises(ValueError,match='version'):profile.unlock_profile(bytes(body)+profile.signature(body))


def test_personal_seed_refused(tmp_path):
    with pytest.raises(ValueError,match='Personal'):
        profile.create_fixture(tmp_path,tmp_path/'private/saves')


def test_paths_cannot_escape_test_root(tmp_path):
    with pytest.raises(ValueError):
        profile.contained(tmp_path,tmp_path/'private/test-saves/../../personal','private/test-saves')


def test_unowned_run_not_overwritten(tmp_path,monkeypatch):
    run=tmp_path/'private/test-saves/session';run.mkdir(parents=True)
    note=run/'keep.txt';note.write_text('keep')
    monkeypatch.setattr(profile,'check_fixture',lambda root:(tmp_path/profile.FIXTURE,{'files':{}}))
    with pytest.raises(ValueError,match='not owned'):profile.prepare_run(tmp_path,run)
    assert note.read_text()=='keep'


def test_fixture_and_disposable_copy_lifecycle(tmp_path,monkeypatch):
    game=tmp_path/'private/game';game.mkdir(parents=True)
    (game/'default.xbe').write_bytes(b'test executable')
    monkeypatch.setattr(profile,'XBE_SHA256',hashlib.sha256(b'test executable').hexdigest())
    seed=tmp_path/'private/test-saves/fresh/saves'
    title=seed/'UserData/45410066';slot=title/'16D15BD8D404';slot.mkdir(parents=True)
    body=bytearray(fresh()[:-20]);struct.pack_into('<I',body,profile.PROGRESS+0xBF0,0)
    (slot/'tsftrpft').write_bytes(bytes(body)+profile.signature(body))
    for name in ('SaveImage.xbx','TitleImage.xbx','TitleMeta.xbx'):(title/name).write_bytes(b'fixture')
    controls=tmp_path/'tests/fixtures/pc-controls.json';controls.parent.mkdir(parents=True)
    controls.write_text('{}')
    canonical=profile.create_fixture(tmp_path,seed)
    with pytest.raises(ValueError,match='exists'):profile.create_fixture(tmp_path,seed)
    run=tmp_path/'private/test-saves/session'
    monkeypatch.setenv('TSFP_TEXTURE_PACK_DIR',str(tmp_path/'personal-pack'))
    monkeypatch.setenv('TSFP_TEXTURE_MODE','4k')
    monkeypatch.setenv('TSFP_TEXTURE_DUMP_DIR',str(tmp_path/'personal-dumps'))
    monkeypatch.setenv('TSFP_SAVE_LOCATION_LOCK','0')
    monkeypatch.setenv('RECOMP_ICALL_FEEDBACK_PATH',str(tmp_path/'personal-feedback.dump'))
    env=profile.prepare_run(tmp_path,run)
    assert env['RECOMP_ICALL_FEEDBACK_PATH']==str(run/'icall_targets.dump')
    relative=Path('UserData/45410066')/profile.TEST_SLOT/'tsftrpft'
    saved=(canonical/relative).read_bytes()
    assert not (run/'saves/UserData/45410066/16D15BD8D404').exists()
    (run/'saves'/relative).write_bytes(b'changed during gameplay')
    assert (canonical/relative).read_bytes()==saved
    profile.prepare_run(tmp_path,run)
    assert (run/'saves'/relative).read_bytes()==saved
    assert env['TSFP_SAVE_LOCATION_LOCK']=='1'
    preference=run/'save-location.txt'
    preference.write_text(str(tmp_path/'chosen-drive/Saves'))
    profile.prepare_run(tmp_path,run)
    assert preference.read_text()==str(tmp_path/'chosen-drive/Saves')
    assert not (tmp_path/'chosen-drive').exists()
    assert env['RECOMP_XMV_PLAY']=='0'
    assert env['TSFP_TEXTURE_PACK_DIR']==str(tmp_path/'assets/textures')
    assert env['TSFP_TEXTURE_MODE']=='classic'
    assert 'TSFP_TEXTURE_DUMP_DIR' not in env
    assert all(str(run) in env[key] for key in ('RECOMP_SAVE_DIR','RECOMP_INPUT_CONFIG','RECOMP_DISPLAY_CONFIG','TSFP_DISPLAY_CONFIG','TSFP_CRASH_DIR'))
    # Legacy Test Lab fixtures kept the Player 1 folder while changing the
    # display name, making a new Player 1 fail with ERROR_ALREADY_EXISTS.
    import json
    legacy='16D15BD8D404'
    current_base=Path('UserData/45410066')/profile.TEST_SLOT
    old_base=current_base.with_name(legacy)
    (canonical/current_base).rename(canonical/old_base)
    manifest=json.loads((canonical/'fixture.json').read_text())
    manifest['files']={(old_base/Path(name).name).as_posix() if Path(name).parent==current_base else name:digest
        for name,digest in manifest['files'].items()}
    (canonical/'fixture.json').write_text(json.dumps(manifest))
    legacy_run=tmp_path/'private/test-saves/legacy'
    profile.prepare_run(tmp_path,legacy_run)
    assert (legacy_run/'saves'/relative).read_bytes()==saved
    assert not (legacy_run/'saves'/old_base).exists()
    # An untouched old test copy is retained outside the game's UserData tree.
    old=legacy_run/'saves'/old_base;old.mkdir()
    for name in ('SaveMeta.xbx','tsftrpft'):(old/name).write_bytes((canonical/old_base/name).read_bytes())
    profile.prepare_run(tmp_path,legacy_run)
    assert not old.exists()
    assert (legacy_run/'legacy-fixture-backup'/legacy/'tsftrpft').read_bytes()==saved
    (run/'run.lock').write_text('in use')
    with pytest.raises(ValueError,match='in use'):profile.prepare_run(tmp_path,run)


@pytest.mark.parametrize('name',['icall_targets.dump','icall_sites.dump'])
def test_feedback_dump_rejects_linked_outputs(tmp_path,name):
    run=tmp_path/'private/test-saves/feedback';run.mkdir(parents=True)
    personal=tmp_path/'personal-dump';personal.write_text('preserve')
    try:
        (run/name).symlink_to(personal)
    except OSError:
        pytest.skip('Account lacks symlink creation privileges')
    with pytest.raises(ValueError,match='linked'):
        profile.feedback_dump_path(tmp_path,run)
    assert personal.read_text()=='preserve'


@pytest.mark.skipif(profile.os.name!='nt',reason='Windows bounded play runner')
@pytest.mark.parametrize('manual',[False,True])
def test_manual_drive_selection_is_explicit(tmp_path,monkeypatch,manual):
    import io
    game=tmp_path/'private/game';game.mkdir(parents=True)
    (game/'default.xbe').write_bytes(b'test executable')
    monkeypatch.setattr(profile,'XBE_SHA256',hashlib.sha256(b'test executable').hexdigest())
    run=tmp_path/'private/test-saves/manual';run.mkdir(parents=True)
    monkeypatch.setattr(profile,'prepare_run',lambda root,path:{'TSFP_SAVE_LOCATION_LOCK':'1'})
    monkeypatch.setattr(profile,'recycle_disks',lambda root,path:None)
    captured={}
    class Child:
        pid=123;returncode=0
        stdout=io.BytesIO();stderr=io.BytesIO()
        def wait(self,timeout=None):return 0
        def poll(self):return 0
    def launch(*args,**kwargs):
        captured.update(kwargs['env']);return Child()
    monkeypatch.setattr(profile.subprocess,'Popen',launch)
    assert profile.play(tmp_path,run,1,allow_save_drive_changes=manual)==0
    assert captured['TSFP_SAVE_LOCATION_LOCK']==('0' if manual else '1')
    assert not (run/'run.lock').exists()


def test_input_publication_keeps_old_state_until_complete_replace(tmp_path, monkeypatch):
    path = tmp_path / 'private/test-saves/run/input-state.txt'
    path.parent.mkdir(parents=True)
    path.write_text('87 1\n')
    replace = profile.os.replace
    attempts = []
    def sharing_reader(source, destination):
        assert path.read_text() == '87 1\n'
        assert Path(source).read_text() == '0 0\n'
        attempts.append(source)
        if len(attempts) < 3:
            raise PermissionError('reader still open')
        replace(source, destination)
    monkeypatch.setattr(profile.os, 'replace', sharing_reader)
    profile.publish_input_state(tmp_path, path, '0 0\n')
    assert path.read_text() == '0 0\n' and len(attempts) == 3
    assert list(path.parent.iterdir()) == [path]


def test_input_publication_failure_preserves_state_and_removes_temp(tmp_path, monkeypatch):
    path = tmp_path / 'private/test-saves/run/input-state.txt'
    path.parent.mkdir(parents=True)
    path.write_text('87 1\n')
    def deny(*args):
        raise PermissionError('replacement denied')
    ticks = iter([0, 1])
    monkeypatch.setattr(profile.os, 'replace', deny)
    monkeypatch.setattr(profile.time, 'monotonic', lambda: next(ticks))
    with pytest.raises(PermissionError):
        profile.publish_input_state(tmp_path, path, '0 0\n')
    assert path.read_text() == '87 1\n'
    assert list(path.parent.iterdir()) == [path]
    with pytest.raises(ValueError):
        profile.publish_input_state(tmp_path, tmp_path/'personal.txt', '0 0\n')
def test_grouped_windows_replay_entry_point_help():
    """Direct invocation must find the input helper outside pytest's import paths."""
    import os
    import subprocess
    import sys
    from pathlib import Path
    import pytest
    if os.name != 'nt':
        pytest.skip('Windows replay uses user32')
    root = Path(__file__).resolve().parents[2]
    result = subprocess.run(
        [sys.executable, '-I', str(root / 'tests/gameplay/stability_transitions_game.py'), '--help'],
        cwd=root, capture_output=True, text=True, timeout=20)
    assert result.returncode == 0, result.stdout + result.stderr
    assert '--run-name' in result.stdout and '--seconds' in result.stdout
