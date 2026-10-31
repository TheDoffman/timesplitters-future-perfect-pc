"""Open isolated Windows games for manual local Online Story testing."""

# Direct-command imports share the repository's grouped helper paths.
import sys as _layout_sys
from pathlib import Path as _LayoutPath
_layout_tools = _LayoutPath(__file__).resolve().parents[1]
_layout_root = _layout_tools.parent
if (_layout_tools / 'project_paths.py').is_file():
    _layout_sys.path.insert(0, str(_layout_tools))
    from project_paths import configure as _configure_paths, source_file as repo_file
    _configure_paths()
else:
    # Standalone source fixtures retain their original flat-folder behavior.
    _layout_root = _LayoutPath(__file__).resolve().parents[1]
    def repo_file(root, area, name):
        return _LayoutPath(root) / area / name


import argparse
import hashlib
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import threading
import time

from lobby_server import Server
from testing_profile import contained, prepare_run, recycle_disks

ROOT = Path(__file__).resolve().parents[2]
LOG_LIMIT = 4 * 1024 * 1024


def verify_build(root):
    exe = root / 'build/windows/Release/futureperfect_recomp.exe'
    if not exe.is_file():
        raise ValueError('Release game not found. Run tsfp.ps1 -Action Build first.')
    subprocess.run([sys.executable, str(root / 'tools/build/network-identity.py'),
                    '--verify', '--executable', str(exe)], check=True, cwd=root)
    manifest = json.loads((root / 'build/windows/generated/network-identity.json').read_text())
    for name, digest in manifest['files']:
        if name.startswith(('src/', 'patches/', 'tools/')) or name == 'VERSION':
            source = root / name
            if name.startswith('tools/') and '/' not in name[len('tools/'):]:
                source = repo_file(root, 'tools', name[len('tools/'):])
            actual = hashlib.sha256(source.read_bytes().replace(b'\r\n', b'\n')).hexdigest()
            if actual != digest:
                raise ValueError(f'Rebuild required: {name} changed since this executable was built.')
    return exe


def drain(pipe, destination):
    # Continue draining after reaching the cap so a long manual session cannot
    # block a game on a full pipe or grow its logs without limit.
    with pipe, destination.open('wb') as output:
        remaining = LOG_LIMIT
        while chunk := pipe.read(65536):
            output.write(chunk[:remaining])
            remaining = max(0, remaining - len(chunk))


def configure(root, run, directory, role):
    env = prepare_run(root, run)
    (run / 'runtime.ini').write_text(
        'fullscreen = 0\nframe_interp = 1\nframe_target = 0\nframe_cap = 60\nrefresh_rate = 0\n')
    (run / 'display.ini').write_text('TSFP_DISPLAY_V2 960 540 55\n')
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.bind(('127.0.0.1', 0))
        port = probe.getsockname()[1]
    settings = contained(root, run / 'online-settings.json', 'private/test-saves')
    settings.write_text(json.dumps(dict(directory=directory, fingerprint='',
        name=f'Local Story - {role}', port=port, automatic_host=False,
        detect_public_address=False, public_address='')) + '\n')
    env.update(TSFP_ONLINE_SETTINGS=str(settings), RECOMP_WINDOW_BACKGROUND='1',
               RECOMP_PC_MOUSE='1', TSFP_DISCORD_CONFIG=str(run / 'discord.ini'))
    return env


def play(root, players=2, seconds=0):
    if os.name != 'nt':
        raise ValueError('Local Online Story testing currently requires Windows.')
    if players not in (2, 4) or seconds < 0:
        raise ValueError('Choose 2 or 4 players and a nonnegative duration.')
    exe = verify_build(root)
    session_lock = contained(root, root / 'private/test-saves/story-manual.lock', 'private/test-saves')
    session_lock.parent.mkdir(parents=True, exist_ok=True)
    # The exclusive marker also prevents a second launcher preparing saves while
    # the first launcher is still starting its games.
    with session_lock.open('x') as stream:
        stream.write(str(os.getpid()))
    server = None
    server_thread = None
    children = []
    threads = []
    runs = []
    try:
        server = Server(('127.0.0.1', 0))
        directory = f'http://127.0.0.1:{server.server_port}'
        server_thread = threading.Thread(target=server.serve_forever, daemon=True)
        server_thread.start()
        roles = ['host', 'guest'] + (['guest2', 'guest3'] if players == 4 else [])
        for role in roles:
            run = contained(root, root / f'private/test-saves/story-manual-{role}', 'private/test-saves')
            env = configure(root, run, directory, role)
            with (run / 'run.lock').open('x') as stream:
                stream.write(str(os.getpid()))
            runs.append(run)
            child = subprocess.Popen([str(exe)], cwd=exe.parent, env=env,
                stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                creationflags=subprocess.CREATE_NEW_PROCESS_GROUP)
            children.append(child)
            for pipe, name in ((child.stdout, 'game.out'), (child.stderr, 'game.err')):
                destination = contained(root, run / name, 'private/test-saves')
                thread = threading.Thread(target=drain, args=(pipe, destination), daemon=True)
                thread.start()
                threads.append(thread)
            print(f'{role}: game PID {child.pid}; isolated files: {run}', flush=True)
        print('\nSelect Test Lab, then Story > Online in every game.\n'
            'Host: click Start New Match in the Online browser, then Start New Match in the game.\n'
              "Guests: click Refresh, select the host's lobby, then Join selected.\n"
              'In the host game, select Time to Split and start when everyone is present.\n'
              'Alt+Tab switches games; E/Space confirms and F goes back.\n'
              'Keep this terminal open. Close ALL game windows to finish, or press Ctrl+C here.\n'
              f'Personal saves/settings are untouched. '
              f'{"No time limit." if not seconds else f"Startup smoke limit: {seconds}s."}', flush=True)
        deadline = time.monotonic() + seconds if seconds else None
        while any(child.poll() is None for child in children):
            if deadline is not None and time.monotonic() >= deadline:
                break
            time.sleep(.2)
        return int(any(child.poll() not in (None, 0) for child in children))
    except KeyboardInterrupt:
        return 0
    finally:
        # Close only the processes we started, including a partial launch.
        for child in children:
            if child.poll() is None:
                child.terminate()
        for child in children:
            try:
                child.wait(timeout=10)
            except subprocess.TimeoutExpired:
                child.kill()
                child.wait(timeout=10)
        for thread in threads:
            thread.join(timeout=10)
        if server_thread is not None:
            server.shutdown()
            server_thread.join(timeout=3)
        if server is not None:
            server.server_close()
        # Retain markers if cleanup fails, preventing unsafe preparation on rerun.
        for run in runs:
            recycle_disks(root, run)
            (run / 'run.lock').unlink()
        session_lock.unlink()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--players', type=int, choices=(2, 4), default=2)
    parser.add_argument('--seconds', type=int, default=0,
                        help='Optional startup smoke-test limit; 0 waits until you close the games')
    args = parser.parse_args()
    try:
        return play(ROOT, args.players, args.seconds)
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(f'Local Story launcher: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
