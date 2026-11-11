"""Windows dashboard shortcut entry point; reuse or start a local SSH viewer."""
import argparse
import ctypes
import json
from pathlib import Path
import subprocess
import sys
import time
import urllib.request
import webbrowser


ROOT = Path(__file__).resolve().parent
URL = 'http://127.0.0.1:41822'


def ready():
    try:
        opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
        with opener.open(URL + '/api/status', timeout=1) as response:
            return isinstance(json.load(response).get('rooms'), list)
    except (OSError, ValueError):
        return False


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ssh', required=True, help='Locally configured dashboard SSH alias')
    args = parser.parse_args()
    if not ready():
        python = Path(sys.executable).with_name('python.exe')
        with (ROOT / 'dashboard.log').open('w', encoding='utf-8') as log:
            process = subprocess.Popen(
                [str(python), str(ROOT / 'view_online_dashboard.py'),
                 '--ssh', args.ssh, '--no-browser'],
                stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT,
                creationflags=subprocess.CREATE_NO_WINDOW, cwd=str(ROOT))
        deadline = time.monotonic() + 16
        while not ready():
            if process.poll() is not None or time.monotonic() >= deadline:
                raise RuntimeError(
                    'Could not connect to the Mac mini dashboard. Make sure the mini '
                    'is on and reachable on your network.\n\nDetails: '
                    + str(ROOT / 'dashboard.log'))
            time.sleep(0.2)
    if not webbrowser.open(URL):
        raise RuntimeError('The dashboard is connected. Open ' + URL + ' in your browser.')


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        ctypes.windll.user32.MessageBoxW(None, str(error), 'Mac Mini Dashboard', 0x10)
        sys.exit(1)
