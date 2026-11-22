"""Open a mini's private dashboard through an owned SSH tunnel; Ctrl+C closes it."""
import argparse
from collections import deque
import json
import os
import re
import socket
import subprocess
import threading
import time
import urllib.request
import webbrowser


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ssh',required=True,help='Existing SSH name or user@address for the mini')
    parser.add_argument('--local-port',type=int,default=41822)
    parser.add_argument('--remote-port',type=int,default=41822)
    parser.add_argument('--no-browser',action='store_true')
    args=parser.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9_.@:-]+',args.ssh) or args.ssh.startswith('-'):
        parser.error('Use an SSH name or user@address, not command options')
    if not all(1<=port<=65535 for port in (args.local_port,args.remote_port)):
        parser.error('Ports must be from 1 to 65535')
    with socket.socket() as check:
        try:check.bind(('127.0.0.1',args.local_port))
        except OSError:parser.error('Local port is occupied. Use the existing dashboard or choose --local-port.')
    command=['ssh','-N','-o','BatchMode=yes','-o','ExitOnForwardFailure=yes','-o','ConnectTimeout=8',
        '-o','ServerAliveInterval=30','-o','ServerAliveCountMax=2','-L',
        f'127.0.0.1:{args.local_port}:127.0.0.1:{args.remote_port}',args.ssh]
    process=subprocess.Popen(command,stdin=subprocess.DEVNULL,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE,
        text=True,creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
    errors=deque(maxlen=20)
    def drain():
        for line in process.stderr:errors.append(line.rstrip())
    thread=threading.Thread(target=drain,daemon=True);thread.start()
    url=f'http://127.0.0.1:{args.local_port}'
    try:
        deadline=time.monotonic()+12
        while True:
            if process.poll() is not None:raise RuntimeError('SSH exited: '+'; '.join(errors))
            try:
                # The readiness check is loopback-only, bypassing proxy settings.
                opener=urllib.request.build_opener(urllib.request.ProxyHandler({}))
                with opener.open(url+'/api/status',timeout=1) as response:
                    state=json.load(response)
                    if not isinstance(state.get('rooms'),list):raise ValueError('Unexpected dashboard reply')
                break
            except OSError:
                if time.monotonic()>deadline:
                    detail='; '.join(list(errors)[-3:])
                    raise RuntimeError('Dashboard did not answer. '+(detail or 'Check that it is enabled on the mini.'))
                time.sleep(.1)
        print(f'Dashboard: {url}\nKeep this process open; Ctrl+C closes its SSH tunnel.',flush=True)
        if not args.no_browser:webbrowser.open(url)
        while process.poll() is None:time.sleep(.5)
        if process.returncode:raise RuntimeError('SSH disconnected: '+'; '.join(errors))
    except KeyboardInterrupt:
        pass
    finally:
        if process.poll() is None:
            process.terminate()
            try:process.wait(timeout=3)
            except subprocess.TimeoutExpired:process.kill();process.wait()
        thread.join(timeout=1)
        process.stderr.close()


if __name__=='__main__':
    try:main()
    except (RuntimeError,OSError) as error:raise SystemExit(str(error))
