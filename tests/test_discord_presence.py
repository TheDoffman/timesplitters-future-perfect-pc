"""Native policy plus real Windows named-pipe framing, reconnect and exit checks."""
import ctypes
import json
import os
import re
from pathlib import Path
import struct
import subprocess
import threading
import time

import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[1]


@pytest.mark.parametrize('mutation', ['none', 'retained-region', 'full-query'])
def test_guest_read_memory(tmp_path, mutation):
    from test_cutscene_letterbox import compile_and_run
    if mutation == 'full-query' and os.name != 'nt':
        pytest.skip('Windows resident-page optimization; Mac retains Mach query')
    body = re.search(r'static int discord_guest_read\(.*?\n\}',
                     (ROOT / 'src/pc_display_menu.c').read_text(), re.S)
    assert body
    function = body.group()
    if mutation == 'retained-region':
        old = 'PcGuestRegion region={0,0,0};'
        assert old in function
        function = function.replace(old, 'static ' + old)
    elif mutation == 'full-query':
        old = 'pc_guest_memory_in_span(&region,host,bytes,0)'
        assert old in function
        function = function.replace(old, 'pc_guest_memory(host,bytes,0)')
    fixture = (ROOT / 'tests/discord_guest_memory.c').read_text()
    compile_and_run(tmp_path, fixture.replace('/* PRODUCTION_READ */', function),
                    fails=mutation != 'none')


def compile_client(tmp_path, ipc=False):
    compiler, environment = native_compiler(tmp_path)
    exe = tmp_path / ('discord-ipc.exe' if ipc else 'discord-policy.exe' if os.name == 'nt' else 'discord-policy')
    sources = [str(ROOT / ('tests/discord_ipc_client.c' if ipc else 'tests/pc_discord.c')),
               str(ROOT / 'src/pc_discord_model.c')]
    if ipc:
        sources.append(str(ROOT / 'src/pc_discord.c'))
    if os.name == 'nt':
        command = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/WX',
                   '/D_CRT_SECURE_NO_WARNINGS',
                   '/DTSFP_DISCORD_INTERVAL=200', '/DTSFP_DISCORD_RETRY=200', '/DTSFP_DISCORD_TEST_PIPE',
                   '/I' + str(ROOT / 'src'), *sources, '/Fe:' + str(exe)]
    else:
        command = [compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                   '-I' + str(ROOT / 'src'), *sources, '-o', str(exe)]
    result = subprocess.run(command, cwd=tmp_path, env=environment, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


def test_presence_policy(tmp_path):
    result = subprocess.run([str(compile_client(tmp_path))], capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, result.stdout + result.stderr


@pytest.fixture(scope='module')
def ipc_client(tmp_path_factory):
    if os.name != 'nt':
        pytest.skip('Windows pipe adapter')
    return compile_client(tmp_path_factory.mktemp('discord-ipc'), ipc=True)


def config(tmp_path, enabled=1):
    path = tmp_path / 'discord.ini'
    path.write_text(f'TSFP_DISCORD_V1 {enabled} 1561627743353962566\n')
    return path


def test_worker_without_discord_and_disabled(ipc_client, tmp_path):
    # No API calls into an installed Discord instance in automated tests.
    for text in ('TSFP_DISCORD_V1 0 1561627743353962566', 'invalid config', 'TSFP_DISCORD_V1 1 1561627743353962566'):
        path = tmp_path / 'discord.ini'
        path.write_text(text)
        result = subprocess.run([str(ipc_client), str(path)], capture_output=True, text=True, timeout=6)
        assert result.returncode == 0, result.stderr
        if 'V1 1' not in text:
            assert 'Optional Rich Presence enabled' not in result.stderr


@pytest.mark.parametrize('initial', ['normal', 'disconnect', 'oversize', 'error', 'no_ready'])
def test_pipe_protocol(ipc_client, tmp_path, initial):
    from ctypes import wintypes as w
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.CreateNamedPipeW.argtypes = [w.LPCWSTR, w.DWORD, w.DWORD, w.DWORD, w.DWORD, w.DWORD, w.DWORD, ctypes.c_void_p]
    kernel.CreateNamedPipeW.restype = w.HANDLE
    kernel.ConnectNamedPipe.argtypes = [w.HANDLE, ctypes.c_void_p]
    kernel.PeekNamedPipe.argtypes = [w.HANDLE, ctypes.c_void_p, w.DWORD, ctypes.c_void_p, ctypes.POINTER(w.DWORD), ctypes.c_void_p]
    kernel.ReadFile.argtypes = [w.HANDLE, ctypes.c_void_p, w.DWORD, ctypes.POINTER(w.DWORD), ctypes.c_void_p]
    kernel.WriteFile.argtypes = kernel.ReadFile.argtypes
    kernel.CloseHandle.argtypes = [w.HANDLE]
    kernel.CancelIoEx.argtypes = [w.HANDLE, ctypes.c_void_p]
    kernel.DisconnectNamedPipe.argtypes = [w.HANDLE]
    # Isolate the pipe name at compile time, leaving real Discord untouched.
    pipe = kernel.CreateNamedPipeW('\\\\.\\pipe\\tsfp-discord-test', 3, 0, 1, 131072, 131072, 0, None)
    assert pipe != ctypes.c_void_p(-1).value, ctypes.get_last_error()
    frames, failures = [], []

    def read_exact(size):
        data = bytearray()
        while len(data) < size:
            buffer = ctypes.create_string_buffer(size - len(data))
            count = w.DWORD()
            if not kernel.ReadFile(pipe, buffer, len(buffer), ctypes.byref(count), None) or not count.value:
                raise EOFError
            data.extend(buffer.raw[:count.value])
        return bytes(data)

    def send(opcode, data, fragment=False):
        data = struct.pack('<II', opcode, len(data)) + data
        chunks = [data[:3], data[3:9], data[9:]] if fragment else [data]
        for chunk in chunks:
            count = w.DWORD()
            assert kernel.WriteFile(pipe, chunk, len(chunk), ctypes.byref(count), None)
            assert count.value == len(chunk)
            if fragment:
                time.sleep(.04)

    def server():
        try:
            def handshake():
                assert kernel.ConnectNamedPipe(pipe, None) or ctypes.get_last_error() == 535
                op, size = struct.unpack('<II', read_exact(8))
                assert op == 0
                assert json.loads(read_exact(size)) == {'v': 1, 'client_id': '1561627743353962566'}
            handshake()
            if initial == 'no_ready':
                # A nested READY string must never count as the real event.
                send(1, b'{"cmd":"DISPATCH","evt":"OTHER","data":{"evt":"READY"}}')
                time.sleep(.3)
                available = w.DWORD()
                assert kernel.PeekNamedPipe(pipe, None, 0, None, ctypes.byref(available), None)
                assert available.value == 0, 'Presence sent before READY'
                # A connected but silent peer cannot hold process shutdown.
                assert not read_exact(1)
            if initial != 'normal':
                if initial == 'error':
                    send(1, b'{"cmd":"SET_ACTIVITY","evt":"ERROR","data":{"message":"bad app"}}')
                elif initial == 'oversize':
                    count = w.DWORD()
                    header = struct.pack('<II', 1, 65537)
                    assert kernel.WriteFile(pipe, header, len(header), ctypes.byref(count), None)
                else:
                    send(2, b'{}')
                # Wait for the client to observe close/error and reconnect.
                time.sleep(.15)
                kernel.DisconnectNamedPipe(pipe)
                handshake()
            send(1, b'{"cmd": "DISPATCH", "evt": "READY", "data":{}}', True)
            send(3, b'ping-payload')
            while True:
                op, size = struct.unpack('<II', read_exact(8))
                data = read_exact(size)
                if op == 4:
                    assert data == b'ping-payload'
                    frames.append(('pong', None))
                else:
                    assert op == 1
                    frames.append(('activity', json.loads(data)))
        except EOFError:
            pass
        except BaseException as error:
            failures.append(error)

    thread = threading.Thread(target=server, daemon=True)
    thread.start()
    try:
        result = subprocess.run([str(ipc_client), str(config(tmp_path))], capture_output=True, text=True, timeout=8)
        thread.join(2)
        assert not thread.is_alive(), 'Pipe worker did not disconnect'
        assert not failures, failures
        assert result.returncode == 0, result.stdout + result.stderr
        if initial == 'no_ready':
            assert not frames
            return
        assert ('pong', None) in frames
        activities = [item['args']['activity'] for kind, item in frames if kind == 'activity']
        details = [item['details'] for item in activities if item]
        assert details == ['In menus', 'Story - Time to Split', 'Online - Deathmatch', 'Online - Deathmatch', 'In menus'], activities
        states = [item['state'] for item in activities if item]
        assert states[1] == 'Normal | Single player | In game'
        assert states[2] == 'Zeppelin | 2 players + 4 bots / 16 | Hosting | In game'
        assert states[3] == 'Zeppelin | 3 players + 4 bots / 16 | Hosting | In game'
        timestamps = {item['timestamps']['start'] for item in activities if item}
        assert len(timestamps) == 1
        assert activities[-1] is None, activities
    finally:
        kernel.CancelIoEx(pipe, None)
        kernel.DisconnectNamedPipe(pipe)
        kernel.CloseHandle(pipe)
