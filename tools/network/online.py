"""Online browser and bridge supervisor, opened by the game's Online entry."""

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
import ctypes
import importlib.util
import ipaddress
import json
import os
from pathlib import Path
import queue
import threading
import time
import tkinter as tk
from tkinter import messagebox, ttk

from online_platform import GameProcess, show_error
from online_client import Client, Session
from online_host import fetch_public_ipv4, invitation, parse_invitation, start_match
from online_paths import ROOT, HANDSHAKES, SETTINGS, MANIFEST, TOOLS
from online_theme import OnlineTheme, build_browser, build_invitation




def wait_for_content(window, parent, stop_path, verify):
    """Keep the desktop responsive while strict content verification reads disk.

    The worker never touches Tk or publishes a bridge. Cancellation may abandon
    a disk read; a daemon worker cannot keep the companion/game disabled.
    """
    panel = ttk.Frame(window, padding=28)
    panel.pack(fill='both', expand=True)
    ttk.Label(panel, text='Preparing Online', style='Title.TLabel').pack(anchor='w')
    status = tk.StringVar(value='Checking game files for multiplayer compatibility...')
    label = ttk.Label(panel, textvariable=status, wraplength=680)
    label.pack(anchor='w', fill='x', pady=18)
    panel.bind('<Configure>', lambda event: label.configure(wraplength=max(120, event.width-56)))
    progress = ttk.Progressbar(panel, mode='indeterminate')
    progress.pack(fill='x', pady=12)
    progress.start(20)
    ttk.Label(panel, text='Large game files or slower drives can take longer. You can cancel and return to the game.',
              wraplength=680).pack(anchor='w', pady=12)
    ttk.Button(panel, text='Cancel / return to game', command=window.destroy).pack(anchor='e', pady=12)
    results = queue.Queue()
    accepted = [False]
    started = time.monotonic()

    def run():
        try:
            verify()
            results.put(None)
        except Exception as error:
            results.put(str(error) or type(error).__name__)

    def poll():
        if parent.exited() or stop_path.exists():
            window.destroy()
            return
        try:
            error = results.get_nowait()
        except queue.Empty:
            window.after(100, poll)
            return
        progress.stop()
        print(f'[ONLINE] Content verification {"failed" if error else "passed"} in '
              f'{time.monotonic()-started:.2f}s', flush=True)
        if error:
            status.set('Unable to open Online.\n\n'+error)
            # Keep monitoring game exit even while the error is visible.
            window.after(100, monitor)
        else:
            accepted[0] = True
            window.quit()

    def monitor():
        if parent.exited() or stop_path.exists():
            window.destroy()
        else:
            window.after(100, monitor)

    # Paint the loading window before starting any game-file reads.
    window.after_idle(lambda: threading.Thread(target=run, daemon=True).start())
    window.after(100, poll)
    window.mainloop()
    if accepted[0]:
        panel.destroy()
    return accepted[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pid', type=int, required=True)
    parser.add_argument('--result', type=Path, required=True)
    parser.add_argument('--compatibility', required=True)
    parser.add_argument('--mode', choices=('arcade', 'story'), default='arcade')
    args = parser.parse_args()
    result = args.result.resolve()
    private = HANDSHAKES.resolve()
    if result.parent != private or result.name != f'{args.pid}.ready':
        raise ValueError('Unexpected Online handshake path')
    parent = GameProcess(args.pid)
    window = tk.Tk()
    window.title(('Online Story' if args.mode == 'story' else 'Online')+' - TimeSplitters Future Perfect')
    window.geometry('820x750')
    window.minsize(760, 720)
    theme = OnlineTheme(window)

    def verify():
        identity_path = TOOLS/'network-identity.py'
        if not identity_path.is_file():
            identity_path = repo_file(ROOT, 'tools', 'network-identity.py')
        spec = importlib.util.spec_from_file_location('network_identity', identity_path)
        identity = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(identity)
        manifest = json.loads(MANIFEST.read_text())
        identity.verify_content(Path(os.environ['RECOMP_GAME_DIR']), manifest)
        if manifest['compatibility'] != args.compatibility:
            raise ValueError('Online tools and executable have different build identities; rebuild first')

    if not wait_for_content(window, parent, result.with_suffix('.stop'), verify):
        parent.close()
        for path in (result, result.with_suffix('.stop')):
            path.unlink(missing_ok=True)
        return
    state = Path(os.environ.get('TSFP_ONLINE_SETTINGS', SETTINGS))
    try:
        saved = json.loads(state.read_text())
    except (OSError, ValueError):
        saved = {}
    address = tk.StringVar(value=saved.get('directory', 'https://127.0.0.1:30820'))
    pin = tk.StringVar(value=saved.get('fingerprint', ''))
    name = tk.StringVar(value=saved.get('name', 'TimeSplitters lobby'))
    port = tk.StringVar(value=str(saved.get('port', 30821)))
    code = tk.StringVar()
    listed = tk.BooleanVar(value=True)
    automatic = tk.BooleanVar(value=saved.get('automatic_host', True))
    detect_ip = tk.BooleanVar(value=saved.get('detect_public_address', True))
    inferred_public = ''
    try:
        previous = ipaddress.IPv4Address(Client(address.get()).url.hostname)
        if not previous.is_loopback:
            inferred_public = str(previous)
    except ValueError:
        pass
    public_ip = tk.StringVar(value=saved.get('public_address', inferred_public))
    status = tk.StringVar(value='Ready when you are. Paste an invitation or start a new match.')
    session = [None]
    automation = os.environ.get('TSFP_ONLINE_TEST_COMMAND')
    busy = [False]
    stopping = threading.Event()
    replies = queue.Queue()
    reply_lock = threading.Lock()
    def client():
        return Client(address.get().strip(), pin.get().strip())

    def save():
        state.parent.mkdir(exist_ok=True)
        temporary = state.with_suffix('.json.tmp')
        temporary.write_text(json.dumps(dict(directory=address.get().strip(), fingerprint=pin.get().strip(),
                                            name=name.get().strip(), port=int(port.get()),
                                            automatic_host=automatic.get(), public_address=public_ip.get().strip(),
                                            detect_public_address=detect_ip.get()), indent=2)+'\n')
        temporary.replace(state)

    def work(action, done):
        if busy[0]:
            return
        busy[0] = True
        status.set('Connecting...')
        def run():
            try:
                value = action()
                with reply_lock:
                    cancelled = stopping.is_set()
                    if not cancelled:
                        replies.put((done, value, None))
                if cancelled and isinstance(value, Session):
                    value.close()
            except Exception as error:
                replies.put((done, None, str(error)))
        # Finish cleanup even if Cancel closes the window during a request.
        threading.Thread(target=run).start()

    def refreshed(reply):
        browser.delete(*browser.get_children())
        for lobby in reply.get('lobbies', []):
            if (lobby.get('compatibility') == args.compatibility and
                lobby.get('mode', 'arcade') == args.mode and
                (args.mode != 'story' or lobby.get('capacity') == 4)):
                browser.insert('', 'end', iid=lobby['code'], values=(lobby['name'],
                    f"{lobby['players']}/{lobby['capacity']}", lobby['code']))
        status.set(f'{len(browser.get_children())} compatible lobbies. Private lobbies can be joined by code.')

    def connected(active):
        session[0] = active
        if active.local_directory:
            address.set(active.local_directory.address)
            pin.set(active.local_directory.fingerprint)
            public_ip.set(Client(active.local_directory.public_url).url.hostname)
        try:
            save()
            temporary = result.with_suffix('.tmp')
            temporary.write_text(active.bridge.config)
            temporary.replace(result)
        except Exception:
            active.close()
            session[0] = None
            raise
        window.withdraw()
        if active.host and not automation:
            show_invitation(active)
        # Returning to the game keeps this process alive as its network bridge.

    def refresh():
        try:
            directory = client()
            work(lambda: directory.request('GET', '/v2/lobbies?compatibility='+args.compatibility+'&mode='+args.mode), refreshed)
        except Exception as error:
            status.set(str(error))

    def join(join_code):
        try:
            if join_code.startswith('tsfp-online:'):
                directory_address, fingerprint, join_code = parse_invitation(join_code)
                address.set(directory_address)
                pin.set(fingerprint)
            directory = client()
            work(lambda: Session.join(directory, args.compatibility, join_code, mode=args.mode), connected)
        except Exception as error:
            status.set(str(error))

    def join_selected():
        selection = browser.selection()
        if selection:
            join(selection[0])
        else:
            status.set('Select a lobby first.')

    def host():
        try:
            host_name, host_port, public = name.get().strip(), int(port.get()), listed.get()
            if not 1024 <= host_port <= 65535:
                raise ValueError('Choose a UDP port between 1024 and 65535')
            if automatic.get():
                public_address = None if detect_ip.get() else public_ip.get().strip()
                work(lambda: start_match(ROOT, public_address, args.compatibility, host_name, host_port, public, mode=args.mode), connected)
                status.set('Detecting public IPv4 and starting the directory...' if public_address is None else
                           'Starting the directory...')
            else:
                directory = client()
                work(lambda: Session.create(directory, args.compatibility, host_name, host_port, public, mode=args.mode), connected)
        except Exception as error:
            status.set(str(error))

    def show_invitation(active):
        share = tk.Toplevel(window)
        theme.track(share)
        share.title('Online match hosted')
        value = getattr(active, 'invitation', invitation(address.get().strip(), pin.get().strip(), active.lobby['code']))
        build_invitation(share, value, active.lobby['code'], active.lobby['port'])

    def detected(value):
        public_ip.set(value)
        status.set('Public IPv4 detected. Forward TCP 30820 and your selected UDP port to this PC.')

    browser = build_browser(window, dict(address=address, pin=pin, name=name, port=port,
        code=code, listed=listed, automatic=automatic, detect_ip=detect_ip,
        public_ip=public_ip, status=status), dict(refresh=refresh, join_selected=join_selected,
        join_code=lambda: join(code.get().strip()), host=host,
        detect=lambda: work(fetch_public_ipv4, detected), cancel=window.destroy))

    def poll():
        if parent.exited() or result.with_suffix('.stop').exists():
            window.destroy()
            return
        try:
            while True:
                done, value, error = replies.get_nowait()
                busy[0] = False
                if error:
                    status.set(error)
                else:
                    try:
                        done(value)
                    except Exception as failure:
                        status.set(str(failure))
        except queue.Empty:
            pass
        if session[0] and session[0].closed.is_set():
            messagebox.showerror('Online disconnected', session[0].error or 'The lobby closed.', parent=window)
            window.destroy()
            return
        window.after(100, poll)
    window.after(100, poll)
    if automation:
        test_path = Path(automation).resolve()
        test_root = (ROOT/'private/network-validation').resolve()
        if not test_path.is_relative_to(test_root):
            raise ValueError('Online test command must be inside the isolated network-validation directory')
        command = json.loads(test_path.read_text())
        address.set(command['directory'])
        pin.set(command.get('fingerprint', ''))
        if command['action'] == 'host':
            automatic.set(command.get('automatic_host', False))
            detect_ip.set(command.get('detect_public_address', False))
            public_ip.set(command.get('public_address', ''))
            port.set(str(command['port']))
            window.after(300, host)
        elif command['action'] == 'join':
            window.after(300, lambda: join(command['code']))
        elif command['action'] == 'browse':
            def choose(reply):
                refreshed(reply)
                browser.selection_set(command['code'])
                window.after(300, join_selected)
            directory = client()
            window.after(300, lambda: work(lambda: directory.request(
                'GET', '/v2/lobbies?compatibility='+args.compatibility), choose))
        elif command['action'] == 'cancel':
            window.after(300, window.destroy)
        else:
            raise ValueError('Unknown online test action')
    try:
        window.mainloop()
    finally:
        with reply_lock:
            stopping.set()
        while True:
            try:
                _, value, _ = replies.get_nowait()
                if isinstance(value, Session) and value is not session[0]:
                    value.close()
            except queue.Empty:
                break
        if session[0]:
            session[0].close()
        for path in (result, result.with_suffix('.stop')):
            path.unlink(missing_ok=True)
        parent.close()


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        import traceback
        logs = ROOT / 'Logs'
        logs.mkdir(parents=True, exist_ok=True)
        (logs/'online-error.log').write_text(traceback.format_exc(), encoding='utf-8')
        show_error(error)
        raise
