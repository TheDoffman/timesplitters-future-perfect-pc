"""Manual Story launcher ownership, local routing and partial-launch cleanup."""
import io
import hashlib
import json
from pathlib import Path
import sys
from types import SimpleNamespace

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
import story_coop_local as launcher


@pytest.mark.parametrize('tool', ['online_client.py', 'online_theme.py'])
def test_build_verifier_resolves_portable_tool_names_and_rejects_edits(tmp_path, monkeypatch, tool):
    exe = tmp_path / 'build/windows/Release/futureperfect_recomp.exe'
    exe.parent.mkdir(parents=True)
    exe.write_bytes(b'fixture executable')
    source = tmp_path / 'tools/network' / tool
    source.parent.mkdir(parents=True)
    source.write_bytes(b'original\r\n')
    generated = tmp_path / 'build/windows/generated'
    generated.mkdir()
    (generated / 'network-identity.json').write_text(json.dumps({'files': [
        ['tools/' + tool, hashlib.sha256(b'original\n').hexdigest()]]}))
    calls = []
    monkeypatch.setattr(launcher.subprocess, 'run', lambda command, **kwargs: calls.append(command))
    assert launcher.verify_build(tmp_path) == exe
    assert '--verify' in calls[0] and '--executable' in calls[0]
    source.write_bytes(b'changed\n')
    with pytest.raises(ValueError, match='Rebuild required: tools/' + tool):
        launcher.verify_build(tmp_path)


def test_configuration_is_local_and_has_no_synthetic_input(tmp_path, monkeypatch):
    run = tmp_path / 'private/test-saves/story-manual-host'
    def prepare(root, target):
        assert root == tmp_path
        target.mkdir(parents=True)
        return {'RECOMP_SAVE_DIR': str(target / 'saves'), 'TSFP_ONLINE_TEST_COMMAND': 'legacy'}
    monkeypatch.setattr(launcher, 'prepare_run', prepare)
    env = launcher.configure(tmp_path, run, 'https://127.0.0.1:12345', 'host', 'a'*64)
    settings = json.loads(Path(env['TSFP_ONLINE_SETTINGS']).read_text())
    assert settings['directory'] == 'https://127.0.0.1:12345'
    assert settings['fingerprint'] == 'a'*64
    assert env['TSFP_ONLINE_DIRECTORY'] == settings['directory']
    assert env['TSFP_ONLINE_FINGERPRINT'] == settings['fingerprint']
    assert not settings['automatic_host'] and not settings['detect_public_address']
    assert settings['public_address'] == ''
    assert 1024 <= settings['port'] <= 65535
    assert not any('INPUT_REPLAY' in key or 'TEST_COMMAND' in key for key in env)
    assert 'frame_cap = 60' in (run / 'runtime.ini').read_text()


def test_logs_remain_capped_and_pipe_is_drained(tmp_path):
    destination = tmp_path / 'game.err'
    pipe = io.BytesIO(b'x' * (launcher.LOG_LIMIT + 500))
    launcher.drain(pipe, destination)
    assert destination.stat().st_size == launcher.LOG_LIMIT
    assert pipe.closed


@pytest.mark.parametrize('fail_second', [False, True])
@pytest.mark.parametrize('players', [2, 3, 4])
def test_session_closes_only_owned_games_and_recycles_after_exit(tmp_path, monkeypatch, fail_second, players):
    events = []
    games = []
    class Game:
        pid = 123
        def __init__(self):
            self.alive = True
            self.stdout = io.BytesIO()
            self.stderr = io.BytesIO()
        def poll(self):
            return None if self.alive else 0
        def terminate(self):
            events.append('terminate')
            self.alive = False
        def wait(self, timeout):
            assert not self.alive
            events.append('wait')
            return 0
    class Directory:
        address = 'https://127.0.0.1:12345'
        fingerprint = 'a'*64
        def __init__(self, root):
            assert root == tmp_path
        def close(self):
            events.append('server_close')
    def configure(root, run, directory, role, fingerprint):
        run.mkdir(parents=True)
        return {'role': role}
    def spawn(command, **kwargs):
        if fail_second and games:
            raise OSError('second game failed')
        game = Game()
        games.append(game)
        return game
    def recycle(root, run):
        assert all(not game.alive for game in games)
        assert (run / 'run.lock').exists()
        events.append('recycle')
    monkeypatch.setattr(launcher, 'os', SimpleNamespace(name='nt', getpid=lambda: 456))
    monkeypatch.setattr(launcher, 'verify_build', lambda root: root / 'game.exe')
    monkeypatch.setattr(launcher, 'configure', configure)
    monkeypatch.setattr(launcher, 'LocalTestDirectory', Directory)
    monkeypatch.setattr(launcher.subprocess, 'Popen', spawn)
    monkeypatch.setattr(launcher.subprocess, 'CREATE_NEW_PROCESS_GROUP', 0, raising=False)
    monkeypatch.setattr(launcher, 'recycle_disks', recycle)
    # A bounded smoke run shares the manual lifecycle, without waiting for input.
    clock = iter([0, 2])
    monkeypatch.setattr(launcher.time, 'monotonic', lambda: next(clock))
    if fail_second:
        with pytest.raises(OSError, match='second game failed'):
            launcher.play(tmp_path, players=players, seconds=1)
    else:
        assert launcher.play(tmp_path, players=players, seconds=1) == 0
    assert events.count('wait') == len(games)
    assert events.count('recycle') == (2 if fail_second else players)
    assert 'server_close' in events
    assert not list((tmp_path / 'private/test-saves').rglob('*.lock'))


def test_existing_session_lock_is_preserved(tmp_path, monkeypatch):
    lock = tmp_path / 'private/test-saves/story-manual.lock'
    lock.parent.mkdir(parents=True)
    lock.write_text('other runner')
    monkeypatch.setattr(launcher, 'os', SimpleNamespace(name='nt'))
    monkeypatch.setattr(launcher, 'verify_build', lambda root: root / 'game.exe')
    with pytest.raises(FileExistsError):
        launcher.play(tmp_path)
    assert lock.read_text() == 'other runner'


def test_current_directory_tls_and_story_relay(tmp_path):
    from online_client import Client, Session
    from online_menu import Controller, BROWSE, CREATE, JOIN, CONNECTED, LIST
    import time
    directory = launcher.LocalTestDirectory(tmp_path)
    controllers = [Controller('a'*64), Controller('a'*64)]
    def action(controller, kind, sequence=1, code=''):
        controller.community=(directory.address,directory.fingerprint)
        controller.begin(dict(sequence=sequence,action=kind,mode=2,source=0,
                              public=True,port=30821,name='Story test',code=code,address=''))
        deadline=time.monotonic()+10
        while controller.status==1 and time.monotonic()<deadline:
            time.sleep(.02)
    try:
        host, guest = controllers
        action(host, CREATE)
        assert host.status == CONNECTED, host.message
        assert isinstance(host.session, Session) and host.session.bridge.relay is not None
        action(guest, BROWSE)
        assert guest.status == LIST and len(guest.lobbies)==1, guest.message
        assert guest.lobbies[0]['mode']=='story'
        action(guest, JOIN, 2, guest.lobbies[0]['code'])
        assert guest.status == CONNECTED, guest.message
        assert guest.session.bridge.relay is not None
        with pytest.raises(ValueError):
            Client(directory.address, 'b'*64).request('GET','/v2/lobbies?compatibility='+'a'*64)
    finally:
        for controller in reversed(controllers):controller.close()
        directory.close()


def test_story_live_route_uses_native_menus_without_legacy_override():
    source=(Path(__file__).resolve().parents[1]/'gameplay/stability_transitions_game.py').read_text(encoding='utf-8')
    start=source.index("    if args.scenario in ('story-host','story-host-controls','story-pair'):")
    route=source[start:source.index("    if args.scenario in ('crowd-vietnam'",start)]
    assert 'from lobby_server' not in route
    assert "env['TSFP_ONLINE_TEST_COMMAND']=" not in route
    assert "env.pop('TSFP_ONLINE_TEST_COMMAND',None)" in route
    assert all(label in route for label in ('network-chooser','native-match-browser','native-create-options'))
