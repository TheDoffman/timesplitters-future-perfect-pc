"""Bounded operator SSH backup reception using pre-established host-key trust."""
from contextlib import contextmanager
import os
from pathlib import Path, PurePosixPath
import re
import queue
import shlex
import subprocess
import threading
import time

import mod_backup_exports as exports
from mod_package import ModError, require

MAX_SECONDS = 1800
MAX_STDERR = 8192


def remote_path(value):
    require(isinstance(value, str) and 0 < len(value.encode('utf-8')) <= 4096 and
            PurePosixPath(value).is_absolute() and str(PurePosixPath(value)) == value
            and '..' not in PurePosixPath(value).parts and not value.startswith('//')
            and all(ord(c) >= 32 and ord(c) != 127 and c not in '\u2028\u2029' for c in value),
            'Invalid fixed remote backup path')
    return value


def ssh_command(host, python, entry, bundle, record, service_origin, pin):
    require(isinstance(host, str) and re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]{0,252}', host),
            'Use a configured SSH host alias')
    for value in (python, entry, bundle, record):
        remote_path(value)
    executable = (Path(os.environ['SystemRoot']) / 'System32/OpenSSH/ssh.exe'
                  if os.name == 'nt' else Path('/usr/bin/ssh'))
    require(executable.is_file(), 'System OpenSSH client is unavailable')
    args = [python, '-B', entry, 'send', '--bundle', bundle, '--recipient-record', record,
            '--origin', service_origin, '--trusted-root-sha256', pin]
    command = ' '.join(shlex.quote(arg) for arg in args)
    return [str(executable), '-T', '-a', '-x', '-oBatchMode=yes', '-oStrictHostKeyChecking=yes',
            '-oUpdateHostKeys=no', '-oClearAllForwardings=yes', '-oControlMaster=no',
            '-oControlPath=none', '-oPermitLocalCommand=no', '-oForkAfterAuthentication=no', '-oConnectTimeout=10',
            '-oServerAliveInterval=10', '-oServerAliveCountMax=3', '--', host, command]


@contextmanager
def process_stream(command, seconds):
    """One bounded pipe reader plus bounded stderr; no captured ciphertext blob."""
    require(type(seconds) is int and 1 <= seconds <= MAX_SECONDS, 'Invalid backup transfer deadline')
    timed_out = threading.Event()
    too_loud = threading.Event()
    closing = threading.Event()
    packets = queue.Queue(maxsize=2)
    deadline = time.monotonic() + seconds
    process = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, shell=False)
    stdout_fd, stderr_fd = process.stdout.fileno(), process.stderr.fileno()

    def stop():
        if process.poll() is None:
            try:
                process.kill()
            except ProcessLookupError:
                pass

    def expire():
        timed_out.set()
        stop()

    def drain_errors():
        total = 0
        try:
            while not closing.is_set():
                data = os.read(stderr_fd, 1024)
                if not data:
                    break
                total += len(data)
                if total > MAX_STDERR:
                    too_loud.set()
                    stop()
                    break
        except OSError:
            if not closing.is_set():
                too_loud.set()
                stop()

    def drain_output():
        try:
            while not closing.is_set():
                data = os.read(stdout_fd, exports.cipher.CHUNK)
                while not closing.is_set():
                    try:
                        packets.put(data, timeout=0.1)
                        break
                    except queue.Full:
                        continue
                if not data:
                    break
        except OSError:
            stop()

    class Reader:
        pending = b''
        ended = False

        def read(self, length):
            require(type(length) is int and 0 < length <= exports.cipher.CHUNK, 'Unbounded SSH read')
            require(not timed_out.is_set() and time.monotonic() < deadline, 'Backup transport deadline exceeded')
            if not self.pending and not self.ended:
                try:
                    self.pending = packets.get(timeout=max(0.001, deadline - time.monotonic()))
                except queue.Empty as error:
                    raise ModError('Backup transport deadline exceeded') from error
                self.ended = not self.pending
            value, self.pending = self.pending[:length], self.pending[length:]
            return value

    timer = threading.Timer(seconds, expire)
    timer.daemon = True
    worker = threading.Thread(target=drain_errors, daemon=True)
    reader = threading.Thread(target=drain_output, daemon=True)
    timer.start()
    worker.start()
    reader.start()
    try:
        def verify():
            try:
                code = process.wait(timeout=10)
            except subprocess.TimeoutExpired as error:
                raise ModError('Backup sender did not finish') from error
            worker.join(timeout=2)
            require(not worker.is_alive() and not timed_out.is_set() and not too_loud.is_set()
                    and code == 0, 'Backup transport failed or exceeded limits')
        yield Reader(), verify
    finally:
        timer.cancel()
        closing.set()
        stop()
        process.wait(timeout=5)
        process.stdout.close()
        reader.join(timeout=2)
        worker.join(timeout=2)
        process.stderr.close()


def receive_ssh(host, python, entry, bundle, remote_record, destination, recipient,
                service_origin, pin, *, seconds=MAX_SECONDS, accept_receipt=None):
    exports.recipients.checked_public(recipient, service_origin, pin)
    command = ssh_command(host, python, entry, bundle, remote_record, service_origin, pin)
    with process_stream(command, seconds) as (stream, verify):
        return exports.receive_stream(stream, destination, recipient, service_origin, pin,
                                      verify_transport=verify, accept_receipt=accept_receipt)
