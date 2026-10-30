"""Opt-in bounded Windows game/Discord smoke test using the isolated Test Lab.

Run with --live-discord to publish this test session to the running desktop
client. Never use personal saves or settings. Retains bounded logs/captures.
"""

# Direct-command imports share the repository's grouped helper paths.
import sys as _layout_sys
from pathlib import Path as _LayoutPath
_layout_tools = _LayoutPath(__file__).resolve().parents[2] / 'tools'
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
import os
from pathlib import Path
import subprocess
import sys
import threading

ROOT = _layout_root
sys.path.insert(0, str(ROOT / 'tools'))
from testing_profile import XBE_SHA256, prepare_run, recycle_disks, check_fixture


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--live-discord', action='store_true')
    parser.add_argument('--scenario', choices=('story', 'arcade', 'league', 'lan'), default='story')
    args = parser.parse_args()
    if os.name != 'nt' or not args.live_discord:
        parser.error('Requires Windows and explicit --live-discord (publishes presence)')
    assert hashlib.sha256((ROOT / 'private/game/default.xbe').read_bytes()).hexdigest() == XBE_SHA256
    run = ROOT / 'private/test-saves/discord-presence'
    env = prepare_run(ROOT, run)
    (run / 'pc-discord.ini').write_text('TSFP_DISCORD_V1 1 1561627743353962566\n')
    events = []
    # Title, player count and Test Lab profile. Remaining input stays inside
    # this disposable session; no personal controls or saves are loaded.
    sequence = [(5000, 69), (10000, 69), (14000, 40), (18000, 69)]
    if args.scenario == 'story':
        sequence += [(22000, 69), (26000, 69), (30000, 69), (34000, 69),
                     (45000, 27), (63000, 27)]
    elif args.scenario == 'lan':
        sequence += [(22000, 40), (22500, 40), (23000, 40), (24000, 69),
                     (26000, 40), (28000, 69), (32000, 69), (60000, 69), (64000, 69)]
    else:
        sequence += [(22000, 40), (24000, 69)]
        if args.scenario == 'league':
            sequence += [(26000, 40)]
        sequence += [(28000, 69), (32000, 69), (36000, 69), (40000, 69),
                     (44000, 69), (48000, 69), (52000, 69), (56000, 69), (70000, 27)]
    for at, key in sequence:
        events.extend([f'{at} key {key} 1', f'{at + 180} key {key} 0'])
    prefix = 'verified' if args.scenario == 'story' else args.scenario
    (run / 'input.txt').write_text('\n'.join(events) + '\n')
    env.update(TSFP_DISCORD_CONFIG=str(run / 'pc-discord.ini'),
               TSFP_PC_INPUT_REPLAY=str(run / 'input.txt'),
               RECOMP_WINDOW_BACKGROUND='1', RECOMP_PC_MOUSE='1',
               RECOMP_HLE_D3D8_DUMP=str(run / prefix),
               RECOMP_HLE_D3D8_DUMP_EVERY='1200')
    if args.scenario == 'lan':
        env.update(TSFP_NET_BIND='127.0.0.50', TSFP_NET_PEERS='')
    lock = run / 'run.lock'
    with lock.open('x') as marker:
        marker.write(str(os.getpid()))
    process = None
    threads = []

    def drain(pipe, path):
        with path.open('wb') as output:
            remaining = 4 * 1024 * 1024
            while chunk := pipe.read(4096):
                output.write(chunk[:remaining])
                remaining = max(0, remaining - len(chunk))

    try:
        process = subprocess.Popen([str(ROOT / 'build/windows/Release/futureperfect_recomp.exe')],
                                   cwd=ROOT, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                   creationflags=subprocess.CREATE_NO_WINDOW)
        print(f'Isolated Discord smoke test PID {process.pid}, limit 95 seconds', flush=True)
        for pipe, name in [(process.stdout, prefix + '.out'), (process.stderr, prefix + '.err')]:
            thread = threading.Thread(target=drain, args=(pipe, run / name), daemon=True)
            thread.start()
            threads.append(thread)
        try:
            process.wait(timeout=95)
            raise AssertionError(f'Game exited early: {process.returncode}')
        except subprocess.TimeoutExpired:
            pass
    finally:
        if process is not None and process.poll() is None:
            process.terminate()
            process.wait(timeout=10)
        for thread in threads:
            thread.join(10)
        lock.unlink()
        recycle_disks(ROOT, run)
    check_fixture(ROOT)
    log = (run / (prefix + '.err')).read_text(errors='replace')
    lines = [line for line in log.splitlines() if '[PC-DISCORD]' in line]
    print('\n'.join(lines), flush=True)
    assert '[PC-DISCORD] Discord accepted Rich Presence.' in log
    if args.scenario == 'lan':
        assert 'LAN - Hosting lobby / Deathmatch | Subway | 1 player / 16' in log
        assert 'LAN - Deathmatch / Subway | 1 player / 16 | Hosting | In game' in log
        assert '[CRASH]' not in log
        print('PASS: real Discord accepted LAN lobby and match map/type/population')
        return
    if args.scenario != 'story':
        mode = 'Arcade Custom' if args.scenario == 'arcade' else 'Arcade League'
        assert f'[PC-DISCORD] Activity: {mode} - ' in log
        expected = ('Arcade Custom - Deathmatch / Vietnam | 1 player + 6 bots'
                    if args.scenario == 'arcade' else
                    'Arcade League - Rockets 101 / Zeppelin | 1 player + 7 bots')
        assert expected in log
        assert ' | In game' in '\n'.join(lines)
        assert '[CRASH]' not in log
        print(f'PASS: real Discord accepted {mode} match details')
        return
    assert '[PC-DISCORD] Activity: In menus / Story' in log
    assert 'Story - Time to Split /' in log
    assert 'Normal | Single player | Watching a cutscene' in log
    assert 'Single player | In game' in log
    assert 'Single player | Paused' in log
    assert '[PC-DISCORD] Activity: Arcade Custom' not in log
    assert '[CRASH]' not in log
    print('PASS: real Discord accepted menu, Story, cutscene, gameplay and pause states')


if __name__ == '__main__':
    main()
