"""SSH-controlled native peer for test-online-bot-motion.py; no game writes.

Input uses the normal SDL test-event path. Guest observations use a separately
loaded read-only test probe; neither probe nor controller ships in game packages.
"""
import base64
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time

ROOT = Path(__file__).resolve().parents[2]


class RemotePeer:
    def __init__(self, host, root, log):
        command = f'cd {shlex.quote(root)} && .venv/bin/python tools/testing/crossplay_peer.py'
        self.log = log.open('wb')
        self.transport = subprocess.Popen(['ssh', '-T', '-o', 'BatchMode=yes',
            '-o', 'ConnectTimeout=8', host, command], stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, stderr=self.log, text=True, encoding='utf-8')
        self.pages = {}
        self.last_poll = 0
        self.returncode = None

    def call(self, action, **values):
        self.transport.stdin.write(json.dumps(dict(action=action, **values))+'\n')
        self.transport.stdin.flush()
        line = self.transport.stdout.readline()
        if not line:
            raise RuntimeError('Mac peer controller exited; inspect its log')
        reply = json.loads(line)
        if 'error' in reply:
            raise OSError(reply['error'])
        return reply

    def read(self, address, size):
        result = bytearray()
        while size:
            page = address & ~4095
            if page not in self.pages:
                self.pages[page] = base64.b64decode(self.call('read', address=page, size=4096)['data'])
            chunk = min(size, 4096-(address-page))
            result.extend(self.pages[page][address-page:address-page+chunk])
            address += chunk
            size -= chunk
        return bytes(result)

    def refresh(self):
        reply=self.call('snapshot')
        self.pages={int(address):base64.b64decode(data) for address,data in reply['pages'].items()}

    def poll(self):
        if self.transport.poll() is not None:
            return self.transport.returncode or 1
        if time.monotonic()-self.last_poll > 2:
            self.returncode = self.call('poll')['returncode']
            self.last_poll = time.monotonic()
        return self.returncode

    def terminate(self):
        self.call('stop')

    def wait(self, timeout=10):
        return self.call('poll')['returncode']

    def close(self):
        if self.transport.poll() is None:
            self.transport.stdin.close()
            self.transport.wait(timeout=15)
        self.log.close()


def receive(sock, count):
    data = bytearray()
    while len(data) < count:
        part = sock.recv(count-len(data))
        if not part:
            raise OSError('Native observation socket closed')
        data.extend(part)
    return bytes(data)


def serve():
    sys.path.insert(0, str(ROOT/'tools'))
    from project_paths import configure
    configure()
    from testing_profile import prepare_run, recycle_disks, XBE_SHA256
    import vulkan_sdk
    child = None
    run = None
    reader = None
    held = set()
    workers = []
    socket_dir = tempfile.TemporaryDirectory(prefix='tsfp-read-')
    started = 0
    peer_index=1
    def stop():
        nonlocal child
        if child and child.poll() is None:
            child.terminate()
            try:
                child.wait(timeout=8)
            except subprocess.TimeoutExpired:
                child.kill(); child.wait()
        for worker in workers:
            worker.join(timeout=2)

    def keys(text):
        desired = {int(line.split()[0]) for line in text.splitlines()
                   if len(line.split()) == 2 and int(line.split()[1]) and int(line.split()[0])}
        names = {37:'Left',38:'Up',39:'Right',40:'Down',27:'Escape',32:'Space'}
        def name(key):
            return names.get(key, chr(key))
        with (run/'test-keys.txt').open('a', encoding='utf-8') as stream:
            for key in sorted(held-desired):stream.write('-'+name(key)+'\n')
            for key in sorted(desired-held):stream.write('+'+name(key)+'\n')
        held.clear();held.update(desired)

    def alarm(signum, frame):
        raise TimeoutError('Crossplay peer exceeded 20 minute safety limit')
    signal.signal(signal.SIGALRM, alarm)
    signal.alarm(1200)
    try:
        for line in sys.stdin:
            try:
                request = json.loads(line)
                action = request['action']
                reply = {}
                if action == 'start':
                    if child:raise ValueError('Peer already started')
                    tag = request['tag']
                    if not re.fullmatch('[A-Za-z0-9-]+', tag):raise ValueError('Invalid test tag')
                    xbe = ROOT/'private/game/default.xbe'
                    if hashlib.sha256(xbe.read_bytes()).hexdigest()!=XBE_SHA256:raise ValueError('Unsupported XBE')
                    peer_index=int(request.get('peer',1))
                    if peer_index not in (0,1):raise ValueError('Invalid peer')
                    run = ROOT/'private/test-saves'/('online-bots-'+tag)/str(peer_index)
                    env = prepare_run(ROOT, run)
                    (run/'run.lock').write_text(str(os.getpid()))
                    out = ROOT/'logs'/('online-bots-'+tag)
                    out.mkdir(parents=True, exist_ok=True)
                    commands=ROOT/'private/network-validation'/tag
                    commands.mkdir(parents=True,exist_ok=True)
                    command = commands/'mac.json'
                    command.write_text(json.dumps(request['command']))
                    (run/'test-keys.txt').write_text('')
                    probe = ROOT/'build/network-port-tests/native-read.dylib'
                    probe.parent.mkdir(parents=True,exist_ok=True)
                    subprocess.run(['clang','-dynamiclib','-Wall','-Wextra','-Werror',
                        str(ROOT/'tools/testing/native_read_probe.c'),'-o',str(probe)],check=True,stdout=sys.stderr)
                    subprocess.run(['codesign','--force','--sign','-',str(probe)],check=True,stdout=sys.stderr)
                    read_path = str(Path(socket_dir.name)/'memory.sock')
                    manifest = json.loads((ROOT/'build/macos/BUILD.json').read_text())
                    env = vulkan_sdk.with_probe_gpu_environment(env,vulkan_sdk.verify_install(),
                        vulkan_sdk.select_driver(manifest,None))
                    captures = out/'frames';captures.mkdir(exist_ok=True)
                    env.update(TSFP_NATIVE_TEST_KEYS=str(run/'test-keys.txt'),
                        TSFP_CROSSPLAY_READ_SOCKET=read_path,DYLD_INSERT_LIBRARIES=str(probe),
                        TSFP_NETWORK_BOTS='1',TSFP_ONLINE_TEST_COMMAND=str(command),
                        TSFP_ONLINE_SETTINGS=str(run/'online.json'),TSFP_DISCORD_ENABLED='0',
                        RECOMP_WINDOW_BACKGROUND='1',RECOMP_VK_VALIDATION='1',
                        RECOMP_TRACE_BUDGET='0',RECOMP_HLE_D3D8_DUMP=str(captures/'frame'),
                        RECOMP_HLE_D3D8_DUMP_EVERY='60',RECOMP_HLE_D3D8_DUMP_KEEP_LAST='2',
                        RECOMP_LOG_FILE=str(out/(str(peer_index)+'.err')))
                    exe = ROOT/'build/macos/futureperfect_recomp'
                    identity=json.loads((ROOT/'build/macos/generated/network-identity.json').read_text())
                    if identity['compatibility']!=request['compatibility']:raise ValueError('Windows/Mac build identities differ')
                    child=subprocess.Popen([str(exe)],cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
                    started=time.monotonic()
                    def drain(pipe,path):
                        with path.open('wb') as stream:
                            remaining=8*1024*1024
                            while chunk:=pipe.read1(65536):
                                stream.write(chunk[:remaining]);stream.flush();remaining=max(0,remaining-len(chunk))
                    for pipe,suffix in ((child.stdout,'out'),(child.stderr,'err')):
                        worker=threading.Thread(target=drain,args=(pipe,out/(str(peer_index)+'.'+suffix)),daemon=True)
                        worker.start();workers.append(worker)
                    def startup():
                        for at,key in ((5,69),(10,69),(14,40),(18,69)):
                            time.sleep(max(0,started+at-time.monotonic()))
                            keys(f'{key} 1\n');time.sleep(.18);keys('0 0\n')
                    threading.Thread(target=startup,daemon=True).start()
                    reply=dict(pid=child.pid,compatibility=identity['compatibility'],
                        executable_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),
                        probe_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),
                        source_revision=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip())
                elif action in ('read','snapshot'):
                    if reader is None:
                        reader=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM);reader.settimeout(5)
                        try:reader.connect(read_path)
                        except OSError:
                            reader.close();reader=None;raise
                    def read(address,size):
                        if not 0<size<=65536 or not 0<=address<=0xffffffff:raise ValueError('Invalid observation range')
                        reader.sendall(struct.pack('<II',address,size))
                        status=struct.unpack('<I',receive(reader,4))[0]
                        if status:raise OSError(f'Unreadable guest memory {address:x}: {status}')
                        return receive(reader,size)
                    if action=='read':
                        reply={'data':base64.b64encode(read(int(request['address']),int(request['size']))).decode('ascii')}
                    else:
                        pages={}
                        def page(address):
                            base=address&~4095
                            if base not in pages:pages[base]=read(base,4096)
                            return pages[base],address-base
                        def word(address):
                            data,at=page(address)
                            return struct.unpack_from('<I',data,at)[0]
                        for address in (0x76b1a4,0x7de400,0x7356d8,0x6f1188,0x6f0b08,
                                        0x77728c,0x78ad58,0x790950,0x7a2958):page(address)
                        count=word(0x7de400)
                        humans=count&255;bots=(count>>16)&255
                        pool=word(0x7356d8)
                        if pool and 0<humans+bots<=16 and word(0x7de30c)==7:
                            for index in range(humans+bots):
                                actor=pool+index*0xea0
                                entity=word(actor)
                                if entity:page(entity+0x64);page(entity+0x258)
                        reply={'pages':{str(address):base64.b64encode(data).decode('ascii') for address,data in pages.items()}}
                elif action == 'input':keys(request['text'])
                elif action == 'command':command.write_text(json.dumps(request['command']))
                elif action == 'capture':
                    label=request['label']
                    if not re.fullmatch('[a-z0-9-]+',label):raise ValueError('Invalid capture name')
                    from PIL import Image
                    files=sorted(captures.glob('frame*.bmp'),key=lambda p:p.stat().st_mtime)
                    if files:
                        with Image.open(files[-1]) as img:img.save(out/(label+f'-{peer_index}.png'))
                elif action == 'poll':reply={'returncode':child.poll() if child else None}
                elif action == 'stop':stop()
                else:raise ValueError('Unknown peer action')
                print(json.dumps(reply),flush=True)
            except Exception as error:
                print(json.dumps({'error':str(error)}),flush=True)
    finally:
        stop()
        if reader:reader.close()
        if run and (run/'run.lock').exists():
            (run/'run.lock').unlink()
            recycle_disks(ROOT,run)
        socket_dir.cleanup()


if __name__=='__main__':
    serve()
