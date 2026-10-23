"""Bounded local Online host/guest regression; never uses personal settings.

Uses the production Online directory/browser/bridge and file-driven input.
Observations are asynchronous guest-memory reads, not rendered/scanout frames.
"""
import argparse
import ctypes as C
import hashlib
import json
import os
from pathlib import Path
import struct
import shutil
import socket
import re
import subprocess
import threading
import time

from lobby_server import Directory, Server
from testing_profile import prepare_run, recycle_disks, contained, XBE_SHA256
import testing_profile

ROOT = Path(__file__).resolve().parents[1]
K = C.WinDLL('kernel32', use_last_error=True)
K.OpenProcess.restype = C.c_void_p
K.ReadProcessMemory.argtypes = [C.c_void_p, C.c_void_p, C.c_void_p, C.c_size_t, C.c_void_p]
K.CloseHandle.argtypes = [C.c_void_p]


def capture(pid, destination):
    """Capture only the test process's window, never the desktop."""
    from PIL import Image
    U, G = C.windll.user32, C.windll.gdi32
    for name in ('GetDC',):
        getattr(U, name).restype = C.c_void_p
    for name in ('CreateCompatibleDC', 'CreateCompatibleBitmap', 'SelectObject'):
        getattr(G, name).restype = C.c_void_p
    G.CreateCompatibleDC.argtypes = [C.c_void_p]
    G.CreateCompatibleBitmap.argtypes = [C.c_void_p, C.c_int, C.c_int]
    G.SelectObject.argtypes = [C.c_void_p, C.c_void_p]
    G.GetDIBits.argtypes = [C.c_void_p,C.c_void_p,C.c_uint,C.c_uint,C.c_void_p,C.c_void_p,C.c_uint]
    G.DeleteObject.argtypes = [C.c_void_p]
    G.DeleteDC.argtypes = [C.c_void_p]
    U.PrintWindow.argtypes = [C.c_void_p,C.c_void_p,C.c_uint]
    U.ReleaseDC.argtypes = [C.c_void_p,C.c_void_p]
    windows=[]
    @C.WINFUNCTYPE(C.c_bool,C.c_void_p,C.c_void_p)
    def callback(hwnd, unused):
        owner=C.c_ulong()
        U.GetWindowThreadProcessId(C.c_void_p(hwnd),C.byref(owner))
        if owner.value==pid and U.IsWindowVisible(C.c_void_p(hwnd)):
            windows.append(hwnd)
        return True
    U.EnumWindows(callback,0)
    if not windows:
        return
    hwnd=windows[0]
    rect=(C.c_long*4)()
    U.GetClientRect(C.c_void_p(hwnd),rect)
    width,height=rect[2],rect[3]
    dc=U.GetDC(C.c_void_p(hwnd)); memory=G.CreateCompatibleDC(dc)
    bitmap=G.CreateCompatibleBitmap(dc,width,height); old=G.SelectObject(memory,bitmap)
    try:
        U.PrintWindow(hwnd,memory,3)
        info=struct.pack('<IiiHHIIiiII',40,width,-height,1,32,0,width*height*4,0,0,0,0)
        header=C.create_string_buffer(info)
        pixels=C.create_string_buffer(width*height*4)
        G.GetDIBits(memory,bitmap,0,height,pixels,header,0)
        Image.frombytes('RGB',(width,height),pixels.raw,'raw','BGRX').save(destination)
    finally:
        G.SelectObject(memory,old);G.DeleteObject(bitmap);G.DeleteDC(memory);U.ReleaseDC(hwnd,dc)


def atomic(path, text):
    temporary = path.with_suffix('.tmp')
    temporary.write_text(text)
    for _ in range(30):
        try:
            temporary.replace(path)
            return
        except PermissionError:
            time.sleep(.01)
    raise RuntimeError('Input file is busy')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tag', required=True)
    parser.add_argument('--seconds', type=int, default=60)
    args = parser.parse_args()
    if not args.tag.replace('-', '').isalnum() or not 10 <= args.seconds <= 180:
        parser.error('Use an alphanumeric tag and 10..180 seconds')
    assert hashlib.sha256((ROOT/'private/game/default.xbe').read_bytes()).hexdigest() == XBE_SHA256
    exe = ROOT/'build/windows/Release/futureperfect_recomp.exe'
    # Benchmark checkouts expose the canonical fixture through a read-only
    # junction. Stage its five manifest files locally so the normal runner's
    # no-links and hash validation remain in effect for every write.
    fixture = ROOT/'private/test-fixtures/unlocked'
    if fixture.is_junction():
        local = contained(ROOT,ROOT/'private/test-fixtures/online-bots','private/test-fixtures')
        local.mkdir(exist_ok=True)
        manifest = json.loads((fixture/'fixture.json').read_text())
        for name in manifest['files']:
            relative = Path(name)
            if relative.is_absolute() or relative.drive or '..' in relative.parts:
                raise ValueError('Unsafe fixture path')
            destination = contained(ROOT,local/relative,'private/test-fixtures')
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(fixture/relative, destination)
        shutil.copyfile(fixture/'fixture.json', local/'fixture.json')
        testing_profile.FIXTURE = Path('private/test-fixtures/online-bots')
    identity = json.loads((ROOT/'build/windows/generated/network-identity.json').read_text())
    subprocess.run([str(ROOT/'.venv/Scripts/python.exe'), str(ROOT/'tools/network-identity.py'),
                    '--verify', '--executable', str(exe)], check=True)
    out = ROOT/'logs'/('online-bots-'+args.tag)
    out.mkdir(parents=True, exist_ok=True)
    automation = ROOT/'private/network-validation'/args.tag
    automation.mkdir(parents=True, exist_ok=True)
    server = Server(('127.0.0.1', 0), Directory(public_address='127.0.0.1'))
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    url = 'http://127.0.0.1:'+str(server.server_port)
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as available:
        available.bind(('127.0.0.1',0))
        host_port=available.getsockname()[1]
    children, handles, runs, inputs = [], [], [], []
    drains=[]
    offsets={}
    samples = []
    completed=False
    started = time.monotonic()
    def read(peer, address, size):
        data = C.create_string_buffer(size)
        if peer not in offsets:
            output=(out/f'{peer}.out').read_text(errors='replace')
            match=re.search(r'Xbox memory mapped. Offset: 0x([0-9a-fA-F]+)',output)
            if not match:raise OSError('Guest memory not mapped yet')
            offsets[peer]=int(match[1],16)
        if not K.ReadProcessMemory(handles[peer], address+offsets[peer], data, size, None):
            raise OSError(C.get_last_error(), hex(address))
        return data.raw
    def word(peer, address):
        return struct.unpack('<I', read(peer, address, 4))[0]
    def observe():
        for peer in range(len(handles)):
            try:
                network = word(peer, 0x76b1a4)
                config = read(peer, 0x7de400, 3)
                if network != 15 or not config[2]:
                    continue
                pool = word(peer, 0x7356d8)
                actors = []
                for slot in range(config[0]+config[2]):
                    actor = pool+slot*0xea0
                    entity = word(peer, actor)
                    if not entity:
                        actors.append(None)
                        continue
                    actors.append(dict(slot=slot, entity=entity,
                        position=struct.unpack('<3f', read(peer, entity+0x64, 12)),
                        spawn=word(peer, entity+0x258),
                        health=struct.unpack('<f', read(peer, entity+0x1cc, 4))[0]))
                samples.append(dict(seconds=time.monotonic()-started, peer=peer,
                    tick=word(peer, 0x7de318), humans=config[0], bots=config[2], actors=actors))
            except OSError:
                pass
    def wait(seconds):
        until = time.monotonic()+seconds
        while time.monotonic() < until:
            if any(p.poll() is not None for p in children):
                raise RuntimeError('A game exited early')
            observe()
            time.sleep(.008)
    def key(peer, *keys):
        for value in keys:
            atomic(inputs[peer], f'{value} 1\n')
            wait(.3)
            atomic(inputs[peer], '0 0\n')
            wait(.6)
    def report(label):
        states = []
        for peer in range(len(handles)):
            try:
                states.append(dict(peer=peer, network=word(peer, 0x76b1a4),
                    role=word(peer, 0x76b140), config=list(read(peer, 0x7de400, 3))))
            except OSError:
                states.append(dict(peer=peer, unmapped=True))
        print(round(time.monotonic()-started, 1), label, states, flush=True)
        # PrintWindow can block while the game is synchronously awaiting its
        # Online browser. Capture only running frontend/gameplay boundaries.
        if label in ('startup','profiles','frontend','match','finished'):
            for peer, child in enumerate(children):
                worker=threading.Thread(target=capture,args=(child.pid,out/f'{label}-{peer}.png'),daemon=True)
                worker.start();worker.join(timeout=2)
    try:
        for peer in range(2):
            run = ROOT/'private/test-saves'/('online-bots-'+args.tag)/str(peer)
            env = prepare_run(ROOT, run)
            runs.append(run)
            input_path = run/'input.txt'
            atomic(input_path, '0 0\n')
            inputs.append(input_path)
            command = automation/f'{peer}.json'
            command.write_text(json.dumps(dict(action='host' if peer == 0 else 'join',
                directory=url, port=host_port, automatic_host=False, detect_public_address=False)))
            env.update(TSFP_PC_INPUT_STATE=str(input_path), TSFP_NETWORK_BOTS='1',
                TSFP_ONLINE_TEST_COMMAND=str(command), TSFP_ONLINE_SETTINGS=str(run/'online.json'),
                RECOMP_WINDOW_BACKGROUND='1', TSFP_DISCORD_ENABLED='0')
            (run/'run.lock').write_text(str(os.getpid()))
            children.append(subprocess.Popen([str(exe)], cwd=ROOT, env=env,
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, creationflags=subprocess.CREATE_NO_WINDOW))
            def drain(pipe,path):
                with path.open('wb') as output:
                    remaining=4*1024*1024
                    while chunk:=pipe.read1(65536):
                        output.write(chunk[:remaining]);output.flush()
                        remaining=max(0,remaining-len(chunk))
            for pipe,suffix in ((children[-1].stdout,'out'),(children[-1].stderr,'err')):
                worker=threading.Thread(target=drain,args=(pipe,out/f'{peer}.{suffix}'),daemon=True)
                worker.start();drains.append(worker)
            handles.append(K.OpenProcess(0x10, False, children[-1].pid))
        wait(25)
        report('startup')
        for peer in range(2):
            key(peer,69)
        wait(2)
        report('profiles')
        for peer in range(2):
            key(peer,40,69)
        wait(3)
        report('frontend')
        for peer in range(2):
            key(peer, 40, 40, 40, 69)
        wait(3)
        key(0, 69)  # Online is above System Link.
        wait(8)
        with server.directory.lock:
            codes = list(server.directory.online)
        if not codes:
            report('no-online-lobby')
            raise RuntimeError('Online browser did not create a lobby')
        command = automation/'1.json'
        command.write_text(json.dumps(dict(action='join', directory=url, code=codes[0])))
        key(1, 69)
        wait(8)
        report('online-browsers')
        key(0, 69)
        wait(5)
        # Select Deathmatch using the stock Options flow. Keep fixture bots.
        key(0, 89)
        wait(3)
        key(0, 69)
        wait(2)
        key(0, 69)
        wait(2)
        key(0, 69)
        wait(5)
        key(1, 82)
        wait(3)
        key(1, 40, 69)
        wait(5)
        key(1, 69)
        wait(4)
        report('paired-lobby')
        key(0, 69)
        wait(18)
        report('match')
        # Movement of each human remains independently observable.
        for peer in range(2):
            atomic(inputs[peer], '87 1\n')
            wait(2)
            atomic(inputs[peer], '0 0\n')
            wait(2)
        wait(args.seconds)
        report('finished')
        assert all(any(s['peer']==peer and s['humans']==2 and s['bots']>0 for s in samples)
                   for peer in (0,1)), 'No paired bot gameplay'
        completed=True
    finally:
        exit_codes=[child.poll() for child in children]
        for p in children:
            if p.poll() is None:
                p.terminate()
        for p in children:
            try:
                p.wait(timeout=10)
            except subprocess.TimeoutExpired:
                p.kill(); p.wait()
        for h in handles:
            K.CloseHandle(h)
        for worker in drains:
            worker.join(timeout=5)
        for run in runs:
            (run/'run.lock').unlink(missing_ok=True)
            recycle_disks(ROOT, run)
        server.shutdown(); server.server_close(); thread.join()
        (out/'observations.json').write_text(json.dumps(samples))
        (out/'provenance.json').write_text(json.dumps(dict(
            executable_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),
            compatibility=identity['compatibility'], observation='asynchronous ReadProcessMemory',
            pids=[child.pid for child in children], guest_memory_offsets=offsets,
            completed=completed, exit_codes_before_cleanup=exit_codes,
            seconds=time.monotonic()-started, samples=len(samples)), indent=2))


if __name__ == '__main__':
    main()
