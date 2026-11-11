"""Player-side process lifetime without a server installation."""
import os
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]


def test_game_process_detects_parent_exit(tmp_path):
    """An abrupt parent exit must stop supervision without trusting a recycled PID."""
    monitor = (
        'import sys,time; from pathlib import Path; from online_platform import GameProcess; '
        'parent=GameProcess(int(sys.argv[1])); folder=Path(sys.argv[2]); '
        'folder.joinpath("ready").touch(); deadline=time.monotonic()+15\n'
        'while not parent.exited() and time.monotonic()<deadline: time.sleep(.02)\n'
        'folder.joinpath("result").write_text(str(parent.exited())); parent.close()\n')
    owner = ('import subprocess,sys,os,time; '
             'subprocess.Popen([sys.executable,"-c",sys.argv[1],str(os.getpid()),sys.argv[2]]); '
             'time.sleep(20)')
    env = dict(os.environ, PYTHONPATH=str(ROOT/'tools/network'))
    process = subprocess.Popen([sys.executable, '-c', owner, monitor, str(tmp_path)], env=env)
    try:
        deadline = time.monotonic()+10
        while not (tmp_path/'ready').exists() and time.monotonic()<deadline:
            time.sleep(.02)
        assert (tmp_path/'ready').exists()
        process.terminate(); process.wait(timeout=5)
        while not (tmp_path/'result').exists() and time.monotonic()<deadline:
            time.sleep(.02)
        assert (tmp_path/'result').read_text() == 'True'
    finally:
        if process.poll() is None:
            process.terminate(); process.wait(timeout=5)
