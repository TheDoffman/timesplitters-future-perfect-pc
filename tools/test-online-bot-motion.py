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
    parser.add_argument('--host-fps', type=int, choices=(0,120,180,240), default=0,
                        help='0 uses Original60; other values select native beta timing')
    parser.add_argument('--guest-fps', type=int, choices=(0,120,180,240), default=0)
    parser.add_argument('--no-captures', action='store_true')
    parser.add_argument('--require-results', action='store_true',
                        help='Wait up to --seconds for both peers to finish and publish score rows')
    parser.add_argument('--inspect-setup', action='store_true',
                        help='Capture the normal setup screens, then stop before gameplay')
    parser.add_argument('--short-match', action='store_true',
                        help='Select five-point Deathmatch through the stock setup menu')
    args = parser.parse_args()
    if args.inspect_setup and args.require_results:
        parser.error('--inspect-setup cannot satisfy --require-results')
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
    checkpoints = []
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
                    tick=word(peer, 0x7de318), match_state=word(peer,0x7de30c),
                    humans=config[0], bots=config[2], actors=actors))
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
            wait(.18)
            atomic(inputs[peer], '0 0\n')
            wait(.6)
    def wait_alive(slot):
        # Combat can kill the client before its scheduled input. A corpse's
        # matching coordinates are not evidence of movement replication.
        deadline=time.monotonic()+15
        while time.monotonic()<deadline:
            actors=[]
            try:
                for peer in range(2):
                    entity=word(peer,word(peer,0x7356d8)+slot*0xea0)
                    actors.append((word(peer,entity+0x258),
                                   struct.unpack('<f',read(peer,entity+0x1cc,4))[0]))
                if actors[0][0]==actors[1][0] and all(a[1]>0 for a in actors):
                    return
            except OSError:
                pass
            wait(.1)
        raise RuntimeError(f'Player {slot} did not become alive on both peers')
    def report(label):
        observe()
        states = []
        for peer in range(len(handles)):
            try:
                config=read(peer,0x7de400,0xa0)
                pool=word(peer,0x7356d8)
                actors=[]
                if pool and word(peer,0x7de30c)==7 and 0<config[0]+config[2]<=16:
                    for slot in range(config[0]+config[2]):
                        actor=pool+slot*0xea0
                        actors.append(dict(slot=slot,
                            kills=struct.unpack('<16h',read(peer,actor+0xa10,32)),
                            score=struct.unpack('<i',read(peer,actor+0xa6c,4))[0],
                            controller_flags=read(peer,0x77728c+slot*0x124,1)[0]))
                states.append(dict(peer=peer, network=word(peer, 0x76b1a4),
                    role=word(peer, 0x76b140), config=list(config[:3]),
                    match_state=word(peer,0x7de30c),results_ready=word(peer,0x6f1188),
                    score_limit=word(peer,0x7de464),
                    active_config=config.hex(),pending_config=read(peer,0x78ad58,0xa0).hex(),
                    actor_counts=[word(peer,0x790950),word(peer,0x7a2958)],actors=actors))
            except OSError:
                states.append(dict(peer=peer, unmapped=True))
        print(round(time.monotonic()-started, 1), label,
              [{k:v for k,v in state.items() if k not in ('active_config','pending_config','actors')}
               for state in states], flush=True)
        checkpoints.append(dict(label=label, seconds=time.monotonic()-started,
            states=states, observations=[next((s for s in reversed(samples) if s['peer']==peer),None)
                                         for peer in range(len(handles))]))
        # PrintWindow can block while the game is synchronously awaiting its
        # Online browser. Capture only running frontend/gameplay boundaries.
        if not args.no_captures and label in ('frontend','no-online-lobby','match','results','finished',
                                              'options-root','options-mode','options-settings','short-match-settings'):
            for peer, child in enumerate(children):
                worker=threading.Thread(target=capture,args=(child.pid,out/f'{label}-{peer}.png'),daemon=True)
                worker.start();worker.join(timeout=2)
    try:
        for peer in range(2):
            run = ROOT/'private/test-saves'/('online-bots-'+args.tag)/str(peer)
            env = prepare_run(ROOT, run)
            runs.append(run)
            if (run/'pc-frame-rate.ini').exists():
                raise ValueError('Saved FPS preference could override this test; use a fresh tag')
            input_path = run/'input.txt'
            atomic(input_path, '0 0\n')
            inputs.append(input_path)
            # Match stability_transitions_game: distinct presses for title,
            # player count and Test Lab selection. Never rely on a held Select
            # leaking through two screens at a particular frame-rate phase.
            replay = run/'startup.txt'
            initial = [(5000,69),(10000,69),(14000,40),(18000,69)]
            replay.write_text('\n'.join(f'{at+d} key {key} {value}'
                for at,key in initial for d,value in ((0,1),(180,0)))+'\n')
            command = automation/f'{peer}.json'
            command.write_text(json.dumps(dict(action='host' if peer == 0 else 'join',
                directory=url, port=host_port, automatic_host=False, detect_public_address=False)))
            env.update(TSFP_PC_INPUT_STATE=str(input_path), TSFP_NETWORK_BOTS='1',
                TSFP_PC_INPUT_REPLAY=str(replay),
                TSFP_NATIVE_FPS=str((args.host_fps,args.guest_fps)[peer]),
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
        if args.inspect_setup:report('options-root')
        key(0, 69)
        wait(2)
        if args.inspect_setup:report('options-mode')
        key(0, 69)
        wait(2)
        if args.short_match:
            # Observed stock setup: GO is row13; SCORE LIMIT is row0.
            # The fixture starts at15 points; two left taps choose5.
            key(0,*([38]*13+[37]*2+[40]*13))
            report('short-match-settings')
        if args.inspect_setup:
            report('options-settings')
            return
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
        if args.short_match:
            assert all(word(peer,0x7de464)==5 for peer in (0,1)), 'Menu replay did not select five points'
        # Movement of each human remains independently observable.
        for peer in range(2):
            wait_alive(peer)
            report(f'move-{peer}-start')
            atomic(inputs[peer], '87 1\n')
            wait(2)
            atomic(inputs[peer], '0 0\n')
            report(f'move-{peer}-released')
            wait(2)
            report(f'move-{peer}-settled')
        if args.require_results:
            deadline=time.monotonic()+args.seconds
            while time.monotonic()<deadline:
                if all(word(peer,0x7de30c)==8 and word(peer,0x6f1188)==1 for peer in (0,1)):
                    break
                wait(.1)
            else:
                report('results-timeout')
                raise RuntimeError('Both peers did not complete within the bounded results wait')
            wait(2)  # Let the existing reliable final packet settle.
            report('results')
            results=[]
            for peer in (0,1):
                rows=[]
                for index in range(8):
                    row=read(peer,0x6f0b08+index*0x68,0x68)
                    rows.append(dict(index=index,slot=row[0],
                        name=row[0x24:0x44].split(b'\0',1)[0].decode('ascii',errors='replace'),
                        kills=struct.unpack_from('<16h',row,2),
                        deaths=struct.unpack_from('<h',row,0x4e)[0],
                        rank=struct.unpack_from('<i',row,0x58)[0],
                        score=struct.unpack_from('<i',row,0x5c)[0],
                        character=struct.unpack_from('<h',row,0x64)[0],raw=row.hex()))
                results.append(dict(peer=peer,match_state=word(peer,0x7de30c),
                                    ready=word(peer,0x6f1188),rows=rows))
            (out/'results.json').write_text(json.dumps(results,indent=2))
            for result in results:
                assert sorted(r['slot'] for r in result['rows'])==list(range(8)), 'Missing results slots'
                assert all(r['name'] for r in result['rows']), 'Missing results names'
            def records(result):
                return sorted([{k:v for k,v in row.items() if k not in ('index','raw')}
                               for row in result['rows']],key=lambda row:row['slot'])
            assert records(results[0])==records(results[1]), 'Host/client final results differ'
            if args.short_match:
                assert max(r['score'] for r in results[0]['rows'])>=5, 'Five-point limit not reached'
        else:
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
        (out/'checkpoints.json').write_text(json.dumps(checkpoints, indent=2))
        (out/'provenance.json').write_text(json.dumps(dict(
            executable_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),
            runner_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            native_fps=[args.host_fps,args.guest_fps], captures=not args.no_captures,
            require_results=args.require_results,
            inspect_setup=args.inspect_setup,
            short_match=args.short_match,
            compatibility=identity['compatibility'], observation='asynchronous ReadProcessMemory',
            pids=[child.pid for child in children], guest_memory_offsets=offsets,
            completed=completed, exit_codes_before_cleanup=exit_codes,
            seconds=time.monotonic()-started, samples=len(samples)), indent=2))


if __name__ == '__main__':
    main()
