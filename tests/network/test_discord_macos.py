"""Exercise the actual native worker against an isolated Discord Unix socket."""


import json
import os
from pathlib import Path
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
import pytest
ROOT=Path(__file__).resolve().parents[2]
pytestmark=pytest.mark.skipif(sys.platform!='darwin',reason='Darwin IPC adapter')
@pytest.fixture(scope='module')
def client(tmp_path_factory):
    exe=tmp_path_factory.mktemp('discord-native')/'client'
    subprocess.run(['clang','-std=c11','-O2','-Wall','-Wextra','-Werror','-DTSFP_DISCORD_INTERVAL=200','-DTSFP_DISCORD_RETRY=100','-DTSFP_DISCORD_TEST_SOCKET','-I'+str(ROOT/'src'),'-I'+str(ROOT/'src/network'),str(ROOT/'tests/network/discord_ipc_client.c'),str(ROOT/'src/network/pc_discord.c'),str(ROOT/'src/network/pc_discord_model.c'),'-o',str(exe)],check=True)
    return exe
@pytest.mark.parametrize('initial',['normal','disconnect','oversize','error','no_ready','disabled','invalid','absent'])
def test_native_protocol(client,tmp_path,initial):
    config=tmp_path/'discord.ini';config.write_text('invalid' if initial=='invalid' else f'TSFP_DISCORD_V1 {0 if initial=="disabled" else 1} 1561627743353962566\n')
    frames=[];failures=[]
    with tempfile.TemporaryDirectory(prefix='tsfp-ipc-',dir='/tmp') as folder:
        path=str(Path(folder)/'ipc');env=dict(os.environ,TSFP_DISCORD_TEST_SOCKET=path)
        with socket.socket(socket.AF_UNIX) as listener:
            listener.bind(path);listener.listen();listener.settimeout(5)
            def read(c,n):
                value=b''
                while len(value)<n:
                    part=c.recv(n-len(value))
                    if not part:raise EOFError
                    value+=part
                return value
            def receive(c):
                op,n=struct.unpack('<II',read(c,8));return op,read(c,n)
            def send(c,op,body,fragment=False):
                data=struct.pack('<II',op,len(body))+body
                for part in ([data[:3],data[3:9],data[9:]] if fragment else [data]):c.sendall(part);time.sleep(.02)
            def server():
                try:
                    for attempt in range(2 if initial in ('disconnect','oversize','error') else 1):
                        with listener.accept()[0] as c:
                            c.settimeout(4);op,body=receive(c);assert op==0 and json.loads(body)['client_id']=='1561627743353962566'
                            if attempt==0 and initial in ('disconnect','oversize','error'):
                                if initial=='oversize':c.sendall(struct.pack('<II',1,65537))
                                if initial=='error':send(c,1,b'{"cmd":"SET_ACTIVITY","evt":"ERROR"}')
                                continue
                            if initial=='no_ready':
                                send(c,1,b'{"cmd":"DISPATCH","evt":"OTHER","data":{"evt":"READY"}}')
                                try:receive(c);raise AssertionError('activity before READY')
                                except EOFError:return
                            send(c,1,b'{"cmd":"DISPATCH","evt":"READY"}',True);send(c,3,b'ping')
                            while True:
                                try:op,body=receive(c)
                                except EOFError:break
                                if op==4:assert body==b'ping';continue
                                item=json.loads(body);assert item['cmd']=='SET_ACTIVITY';frames.append(item['args']['activity'])
                except Exception as exc:failures.append(exc)
            thread=None
            if initial not in ('disabled','invalid','absent'):
                thread=threading.Thread(target=server);thread.start()
            started=time.monotonic();result=subprocess.run([str(client),str(config)],env=env,capture_output=True,text=True,timeout=6)
            if thread:thread.join(5);assert not thread.is_alive()
            assert result.returncode==0,result.stderr
            assert time.monotonic()-started<5
            assert not failures,failures
            if initial in ('normal','disconnect','oversize','error'):
                values=json.dumps(frames);assert 'Time to Split' in values and 'Zeppelin' in values and '3 players' in values,values
            else:assert not frames
