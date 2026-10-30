"""Manual Story launcher ownership, local routing and partial-launch cleanup."""
import io
import json
from pathlib import Path
import sys
from types import SimpleNamespace

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
import story_coop_local as launcher


def test_configuration_is_local_and_has_no_synthetic_input(tmp_path, monkeypatch):
    run = tmp_path / 'private/test-saves/story-manual-host'
    def prepare(root, target):
        assert root == tmp_path
        target.mkdir(parents=True)
        return {'RECOMP_SAVE_DIR': str(target / 'saves')}
    monkeypatch.setattr(launcher, 'prepare_run', prepare)
    env = launcher.configure(tmp_path, run, 'http://127.0.0.1:12345', 'host')
    settings = json.loads(Path(env['TSFP_ONLINE_SETTINGS']).read_text())
    assert settings['directory'] == 'http://127.0.0.1:12345'
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
def test_session_closes_only_owned_games_and_recycles_after_exit(tmp_path, monkeypatch, fail_second):
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
        server_port = 12345
        def __init__(self, address):
            assert address == ('127.0.0.1', 0)
        def serve_forever(self):
            pass
        def shutdown(self):
            events.append('shutdown')
        def server_close(self):
            events.append('server_close')
    def configure(root, run, directory, role):
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
    monkeypatch.setattr(launcher, 'Server', Directory)
    monkeypatch.setattr(launcher.subprocess, 'Popen', spawn)
    monkeypatch.setattr(launcher.subprocess, 'CREATE_NEW_PROCESS_GROUP', 0, raising=False)
    monkeypatch.setattr(launcher, 'recycle_disks', recycle)
    # A bounded smoke run shares the manual lifecycle, without waiting for input.
    clock = iter([0, 2])
    monkeypatch.setattr(launcher.time, 'monotonic', lambda: next(clock))
    if fail_second:
        with pytest.raises(OSError, match='second game failed'):
            launcher.play(tmp_path, seconds=1)
    else:
        assert launcher.play(tmp_path, seconds=1) == 0
    assert events.count('wait') == len(games)
    assert events.count('recycle') == 2
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
