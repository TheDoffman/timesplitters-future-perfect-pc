"""Real bounded child pipes, failed sender completion and fixed SSH arguments."""
import hashlib
import io
from pathlib import Path
import shlex
import subprocess
import sys
import time

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools/mods'))
import mod_backup_transport as transport
import mod_backup_exports as exports
from mod_package import ModError
from mod_trust import encode
import mod_backup_receiver as receiver
from test_backup_exports import case, ORIGIN, PIN


def test_real_pipe_is_bounded_and_delivers_multiple_chunks():
    code = "import sys;sys.stdout.buffer.write(b'A'*(3*1024*1024+19));sys.stdout.buffer.flush()"
    checksum = hashlib.sha256()
    size = 0
    with transport.process_stream([sys.executable, '-c', code], 10) as (reader, verify):
        while data := reader.read(exports.cipher.CHUNK):
            size += len(data)
            checksum.update(data)
        verify()
        with pytest.raises(ModError, match='Unbounded'):
            reader.read(-1)
    assert size == 3 * 1024**2 + 19 and checksum.hexdigest() == hashlib.sha256(b'A' * size).hexdigest()


@pytest.mark.parametrize('fault', ['exit', 'timeout', 'stderr'])
def test_valid_bytes_without_successful_bounded_sender_never_commit(case, fault, monkeypatch):
    frame = case.root / ('transport-frame-' + fault)
    frame.write_bytes(case.frame)
    code = "import sys,time;sys.stdout.buffer.write(open(sys.argv[1],'rb').read());sys.stdout.buffer.flush();sys.stdout.close();"
    if fault == 'exit':
        code += 'sys.exit(7)'
    elif fault == 'timeout':
        code += 'time.sleep(30)'
    else:
        code += "sys.stderr.buffer.write(b'E'*10000);sys.stderr.buffer.flush();time.sleep(30)"
    processes = []
    real = subprocess.Popen
    def capture(*args, **kwargs):
        process = real(*args, **kwargs)
        processes.append(process)
        return process
    monkeypatch.setattr(transport.subprocess, 'Popen', capture)
    output = case.root / ('transport-incomplete-' + fault)
    started = time.monotonic()
    with pytest.raises(ModError):
        with transport.process_stream([sys.executable, '-c', code, str(frame)], 1 if fault == 'timeout' else 10) as (reader, verify):
            exports.receive_stream(reader, output, case.public, ORIGIN, PIN, verify_transport=verify)
    assert not (output / exports.RECEIPT).exists()
    assert processes and all(process.poll() is not None for process in processes)
    assert time.monotonic() - started < 15


def test_ssh_arguments_require_pinned_host_and_quote_each_remote_argument():
    command = transport.ssh_command('fixture-mac', '/opt/python', '/private/script name.py',
                                    "/private/a'$(ignored);b", '/private/public.json', ORIGIN, PIN)
    assert '-oStrictHostKeyChecking=yes' in command and '-oBatchMode=yes' in command
    assert '-oUpdateHostKeys=no' in command and '-oClearAllForwardings=yes' in command
    assert '-a' in command and '-x' in command
    assert '-oControlPath=none' in command and '-oPermitLocalCommand=no' in command
    assert command[-2] == 'fixture-mac'
    args = shlex.split(command[-1])
    assert args == ['/opt/python', '-B', '/private/script name.py', 'send', '--bundle',
                    "/private/a'$(ignored);b", '--recipient-record', '/private/public.json',
                    '--origin', ORIGIN, '--trusted-root-sha256', PIN]
    for host in ('-oIgnore=true', 'user@host', 'host\ncommand', 'host;command'):
        with pytest.raises(ModError):
            transport.ssh_command(host, '/python', '/script', '/bundle', '/record', ORIGIN, PIN)
    for path in ('relative', '//server/path', '/private/../other', '/private/line\nfeed'):
        with pytest.raises(ModError):
            transport.remote_path(path)
    assert transport.remote_path('/private/mods/日本語') == '/private/mods/日本語'
    latest = transport.ssh_command('fixture-mac', '/python', '/entry', '/producer', '/record', ORIGIN, PIN, producer=True)
    assert shlex.split(latest[-1])[3:6] == ['send-current', '--producer', '/producer']
    with pytest.raises(ModError):
        transport.ssh_command('fixture-mac', '/python', '/entry', '/producer', '/record', ORIGIN, PIN, producer='yes')


def test_operator_cli_send_has_only_binary_stdout_and_generic_failures(case):
    entry = Path(__file__).resolve().parents[2] / 'tools/mods/hosting/backup-transfer.py'
    record = case.root / 'public.json'
    record.write_bytes(encode(case.public))
    if sys.platform != 'win32':
        record.chmod(0o600)
    args = [sys.executable, str(entry), 'send', '--bundle', str(case.folder), '--recipient-record',
            str(record), '--origin', ORIGIN, '--trusted-root-sha256', PIN]
    result = subprocess.run(args, stdin=subprocess.DEVNULL, capture_output=True, timeout=30)
    assert result.returncode == 0 and result.stdout == case.frame and result.stderr == b''
    result = subprocess.run(args + ['--snapshot', '/unexpected/private/path'],
                            stdin=subprocess.DEVNULL, capture_output=True, timeout=30)
    assert result.returncode == 2 and result.stdout == b''
    assert b'/unexpected/private/path' not in result.stderr and b'Traceback' not in result.stderr


def test_receiver_accepts_legacy_and_producer_source_bindings_but_rejects_unknown_kind(case):
    remote = dict(host='fixture-mac', python='/python', entry='/entry', bundle='/source', record='/record')
    policy = dict(schema=1, type=receiver.TYPE, pool_id='a' * 32, recipient=case.public,
                  remote=remote, keep=1, interval=86400, maximum_bytes=exports.cipher.MAX_WIRE)
    receiver.check_policy(policy, case.public, ORIGIN, PIN)
    receiver.check_policy(dict(policy, remote=dict(remote, kind='producer')), case.public, ORIGIN, PIN)
    for kind in ('unknown', True, None):
        with pytest.raises(ModError):
            receiver.check_policy(dict(policy, remote=dict(remote, kind=kind)), case.public, ORIGIN, PIN)
