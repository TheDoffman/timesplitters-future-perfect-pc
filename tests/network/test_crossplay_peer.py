"""The native match observer must expose readable bytes, never memory writes."""
import importlib.util
import os
from pathlib import Path
import platform
import socket
import struct
import subprocess
import time

import pytest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('crossplay_peer', ROOT/'tools/testing/crossplay_peer.py')
peer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(peer)


@pytest.mark.parametrize('failure', ['prepare', 'lock', 'after-lock'])
def test_controller_only_cleans_up_its_owned_run(tmp_path, monkeypatch, capsys, failure):
    import io
    import json
    import testing_profile

    monkeypatch.setattr(peer, 'ROOT', tmp_path)
    game = tmp_path/'private/game/default.xbe'
    game.parent.mkdir(parents=True)
    game.write_bytes(b'fixture')
    monkeypatch.setattr(testing_profile, 'XBE_SHA256', peer.hashlib.sha256(b'fixture').hexdigest())
    monkeypatch.setattr(peer.signal, 'SIGALRM', 14, raising=False)
    monkeypatch.setattr(peer.signal, 'signal', lambda *args: None)
    monkeypatch.setattr(peer.signal, 'alarm', lambda *args: None, raising=False)
    run = tmp_path/'private/test-saves/online-bots-lock-test/1'
    run.mkdir(parents=True)
    lock = run/'run.lock'
    disk = run/'Partition0.img'
    disk.write_bytes(b'active disk')
    if failure == 'prepare':
        lock.write_text('other controller')

    def prepare(root, candidate):
        assert candidate == run
        if failure == 'prepare':
            raise ValueError('Test directory is in use')
        if failure == 'lock':
            lock.write_text('other controller')
        return {}

    monkeypatch.setattr(testing_profile, 'prepare_run', prepare)
    recycled = []
    monkeypatch.setattr(testing_profile, 'recycle_disks', lambda root, path: recycled.append(path))
    # A malformed command fails immediately after lock acquisition. A second
    # start must not replace the first owned run before final cleanup.
    requests = [dict(action='start', tag='lock-test')]
    if failure == 'after-lock':
        requests.append(dict(action='start', tag='second-run'))
    monkeypatch.setattr(peer.sys, 'stdin', io.StringIO(''.join(json.dumps(r)+'\n' for r in requests)))
    peer.serve()
    replies = [json.loads(line) for line in capsys.readouterr().out.splitlines()]
    assert all('error' in reply for reply in replies)
    assert disk.read_bytes() == b'active disk'
    if failure == 'after-lock':
        assert not lock.exists()
        assert recycled == [run]
        assert 'already started' in replies[1]['error']
    else:
        assert lock.read_text() == 'other controller'
        assert recycled == []


def test_native_capture_uses_complete_frame_during_rotation(tmp_path):
    from PIL import Image
    old=tmp_path/'frame000.bmp';new=tmp_path/'frame001.bmp'
    Image.new('RGB',(8,8),(23,45,67)).save(old)
    data=old.read_bytes()
    new.write_bytes(data[:60])
    os.utime(old,(1,1));os.utime(new,(2,2))
    target=tmp_path/'capture.png'
    peer.capture_frame(tmp_path,target)
    with Image.open(target) as image:
        assert image.size==(8,8)
        assert image.getpixel((7,7))==(23,45,67)
    assert new.read_bytes()==data[:60]


def test_remote_close_requests_shutdown_before_waiting_for_ssh_eof():
    import io
    from unittest.mock import Mock
    remote=object.__new__(peer.RemotePeer)
    remote.transport=Mock()
    remote.transport.poll.return_value=None
    remote.transport.returncode=0
    remote.log=io.StringIO()
    events=[]
    remote.call=lambda action:events.append(action)
    remote.transport.communicate.side_effect=lambda timeout:events.append(('drain',timeout))
    remote.close()
    assert events==['shutdown',('drain',30)]
    assert remote.log.closed


def test_mac_recycles_only_known_disks_and_preserves_profile(tmp_path, monkeypatch):
    import testing_profile
    run=tmp_path/'private/test-saves/peer';saves=run/'saves';saves.mkdir(parents=True)
    disk=saves/'Partition0.img';disk.write_bytes(b'disposable')
    profile=saves/'profile.dat';profile.write_bytes(b'keep')
    monkeypatch.setattr(testing_profile.sys,'platform','darwin')
    monkeypatch.setattr(Path,'home',classmethod(lambda cls:tmp_path))
    testing_profile.recycle_disks(tmp_path,run)
    assert not disk.exists()
    assert profile.read_bytes()==b'keep'
    assert next((tmp_path/'.Trash').rglob('Partition0.img')).read_bytes()==b'disposable'


def test_remote_reads_across_pages_and_caches_only_one_observation():
    remote = object.__new__(peer.RemotePeer)
    remote.pages = {}
    calls = []
    def call(action, address, size):
        import base64
        calls.append((action,address,size))
        return {'data':base64.b64encode(bytes([address//4096])*size).decode()}
    remote.call = call
    assert remote.read(4094,4)==b'\0\0\1\1'
    assert len(calls)==2
    assert remote.read(4096,2)==b'\1\1'
    assert len(calls)==2
    remote.pages.clear()
    remote.read(4096,2)
    assert len(calls)==3


@pytest.mark.skipif(platform.system()!='Darwin',reason='Native Mach observer')
def test_native_observer_is_bounded_private_and_read_only(tmp_path):
    import tempfile
    # Darwin sockaddr_un is short; pytest's checkout path may exceed its limit.
    with tempfile.TemporaryDirectory(prefix='tsfp-probe-') as name:
        folder=Path(name); path=folder/'read.sock'
        library=folder/'probe.dylib'; executable=folder/'fixture'
        source=folder/'fixture.c'
        source.write_text('''#include <stdint.h>
#include <unistd.h>
intptr_t g_xbox_mem_offset;
static uint32_t value=0x12345678;
int main(void){g_xbox_mem_offset=(intptr_t)&value-0x10000; sleep(20);return 0;}
''')
        subprocess.run(['clang','-dynamiclib','-Wall','-Wextra','-Werror',
            str(ROOT/'tools/testing/native_read_probe.c'),'-o',str(library)],check=True)
        subprocess.run(['clang',str(source),'-Wl,-export_dynamic','-o',str(executable)],check=True)
        env=dict(os.environ,DYLD_INSERT_LIBRARIES=str(library),TSFP_CROSSPLAY_READ_SOCKET=str(path))
        process=subprocess.Popen([str(executable)],env=env)
        try:
            deadline=time.monotonic()+5
            while not path.exists() and time.monotonic()<deadline:time.sleep(.02)
            assert path.exists(), 'Observer failed to initialize'
            assert path.stat().st_mode & 0o077 == 0
            with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as channel:
                channel.settimeout(3);channel.connect(str(path))
                def request(address,size):
                    channel.sendall(struct.pack('<II',address,size))
                    status=struct.unpack('<I',peer.receive(channel,4))[0]
                    return status,peer.receive(channel,size) if status==0 else None
                assert request(0x10000,4)==(0,b'\x78\x56\x34\x12')
                assert request(0x10000,65537)[0]!=0
                assert request(0x10000,0)[0]!=0
                assert request(0xffffffff,4)[0]!=0
                assert request(0x10000,4)==(0,b'\x78\x56\x34\x12')
        finally:
            process.terminate();process.wait(timeout=3)
