#!/usr/bin/env python3
"""Stdlib-only admission for an independent private Windows backup runtime."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import sys
import threading

TYPE = 'tsfp-windows-backup-runtime-v1'
MAX_FILES = 4096
MAX_FILE = 32 * 1024**2
MAX_TOTAL = 128 * 1024**2
MAX_MANIFEST = 1024**2
MANIFEST = 'runtime.json'


def need(ok, message):
    if not ok:
        raise ValueError(message)


def canonical(value):
    return (json.dumps(value, sort_keys=True, separators=(',', ':')) + '\n').encode('ascii')


def decode(data):
    def pairs(items):
        result = {}
        for key, value in items:
            need(key not in result, 'Duplicate runtime field')
            result[key] = value
        return result
    return json.loads(data, object_pairs_hook=pairs)


def sha(value):
    need(isinstance(value, str) and re.fullmatch('[0-9a-f]{64}', value), 'Invalid runtime fingerprint')
    return value


def plain(path, directory=False):
    info = path.lstat()
    need(not path.is_symlink() and not getattr(info, 'st_file_attributes', 0) & 0x400,
         'Runtime aliases are refused')
    need(stat.S_ISDIR(info.st_mode) if directory else stat.S_ISREG(info.st_mode) and info.st_nlink == 1,
         'Unsafe runtime object')
    return info


def fingerprint(path, maximum):
    before = plain(path)
    need(0 <= before.st_size <= maximum, 'Runtime file exceeds budget')
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        stamp = lambda s: (s.st_dev, s.st_ino, s.st_size, s.st_mtime_ns)
        need(stamp(os.fstat(stream.fileno())) == stamp(before), 'Runtime file changed before opening')
        remaining = before.st_size
        while remaining:
            data = stream.read(min(1024**2, remaining))
            need(data, 'Runtime file was truncated')
            digest.update(data)
            remaining -= len(data)
        need(not stream.read(1) and stamp(os.fstat(stream.fileno())) == stamp(before)
             and stamp(plain(path)) == stamp(before), 'Runtime file changed during inspection')
    return dict(bytes=before.st_size, sha256=digest.hexdigest())


def verify(root, expected_sha256):
    sha(expected_sha256)
    root = Path(os.path.abspath(root))
    for ancestor in (root, *root.parents):
        plain(ancestor, directory=True)
    descriptor = root / MANIFEST
    record = fingerprint(descriptor, MAX_MANIFEST)
    need(record['sha256'] == expected_sha256, 'Runtime manifest differs')
    with descriptor.open('rb') as stream:
        wire = stream.read(MAX_MANIFEST + 1)
    need(len(wire) == record['bytes'] and hashlib.sha256(wire).hexdigest() == expected_sha256,
         'Runtime manifest changed')
    value = decode(wire)
    need(isinstance(value, dict) and set(value) == {'schema', 'type', 'source_revision', 'python_version', 'files'}
         and type(value['schema']) is int and value['schema'] == 1 and value['type'] == TYPE
         and canonical(value) == wire, 'Unknown runtime manifest')
    need(isinstance(value['source_revision'], str) and re.fullmatch('[0-9a-f]{40}', value['source_revision'])
         and isinstance(value['python_version'], list) and len(value['python_version']) == 3
         and all(type(n) is int and 0 <= n < 1000 for n in value['python_version'])
         and value['python_version'][:2] == [3, 12], 'Unknown runtime source or Python version')
    files = value['files']
    need(isinstance(files, dict) and 0 < len(files) <= MAX_FILES, 'Runtime inventory exceeds budget')
    folded = set()
    total = 0
    directories = {''}
    for name, item in files.items():
        need(isinstance(name, str) and name.isascii() and '\\' not in name and ':' not in name
             and name != MANIFEST and not name.startswith('/') and all(
                 part not in ('', '.', '..') and re.fullmatch('[A-Za-z0-9_.+-]+', part)
                 and not part.endswith('.') and part.split('.')[0].lower() not in
                 {'con', 'prn', 'aux', 'nul', *(f'com{i}' for i in range(1, 10)), *(f'lpt{i}' for i in range(1, 10))}
                 for part in name.split('/')), 'Unsafe runtime inventory name')
        need(name.lower() not in folded, 'Colliding runtime inventory names')
        folded.add(name.lower())
        need(isinstance(item, dict) and set(item) == {'bytes', 'sha256'} and type(item['bytes']) is int
             and 0 <= item['bytes'] <= MAX_FILE, 'Invalid runtime file record')
        sha(item['sha256'])
        total += item['bytes']
        need(total <= MAX_TOTAL, 'Runtime payload exceeds budget')
        directories.update(str(p) for p in PurePosixPath(name).parents if str(p) != '.')
    required = {'bootstrap.py', 'runtime/python.exe', 'runtime/pythonw.exe', 'runtime/python3.dll',
                'runtime/python312.dll', 'runtime/python312._pth', 'runtime/LICENSE.txt',
                'source/mod_windows_job.py', 'source/mod_windows_recovery.py', 'source/mod_backup_worker.py',
                'source/hosting/private-recovery-windows.ps1'}
    need(required <= files.keys(), 'Incomplete runtime inventory')
    pending = [root]
    observed = set()
    count = 0
    while pending:
        directory = pending.pop()
        with os.scandir(directory) as entries:
            for entry in entries:
                count += 1
                need(count <= 8191, 'Runtime tree exceeds budget')
                path = Path(entry.path)
                name = path.relative_to(root).as_posix()
                if entry.is_dir(follow_symlinks=False):
                    plain(path, directory=True)
                    need(name in directories, 'Foreign runtime directory preserved')
                    pending.append(path)
                else:
                    need(name == MANIFEST or name in files, 'Foreign runtime file preserved')
                    if name != MANIFEST:
                        need(fingerprint(path, MAX_FILE) == files[name], 'Runtime file fingerprint differs')
                        observed.add(name)
    need(observed == set(files), 'Runtime file inventory differs')
    with (root / 'runtime/python312._pth').open('rb') as stream:
        need(stream.read(32) == b'Lib\nDLLs\n.\n', 'Runtime import isolation differs')
    need(fingerprint(descriptor, MAX_MANIFEST) == record, 'Runtime manifest changed during verification')
    return value


def admit(root, expected_sha256):
    need(os.name == 'nt', 'The independent operator runtime requires Windows')
    value = verify(root, expected_sha256)
    runtime = root / 'runtime'
    need(Path(sys.executable).absolute().parent == runtime and Path(sys.executable).name.lower() in ('python.exe', 'pythonw.exe')
         and list(sys.version_info[:3]) == value['python_version'] and sys.flags.isolated == 1
         and sys.flags.no_site == 1 and sys.flags.dont_write_bytecode == 1,
         'Use the isolated frozen interpreter')
    need([os.path.normcase(os.path.abspath(p)) for p in sys.path] ==
         [os.path.normcase(str(p)) for p in (runtime / 'Lib', runtime / 'DLLs', runtime)],
         'Runtime initial search path differs')
    # Only already fingerprinted source enters the search path. It attaches the
    # lifetime job before the first ACL helper; crypto packages follow ACL admission.
    sys.path.append(str(root / 'source'))
    from mod_windows_job import attach
    attach()
    from mod_windows_recovery import private
    private(root, tree=True)
    sys.path.append(str(root / 'packages'))
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime-sha256', required=True)
    parser.add_argument('--check-runtime', action='store_true')
    parser.add_argument('--control')
    parser.add_argument('--binding-sha256')
    args = parser.parse_args()
    # Covers admission, ACL helpers, SSH, retention and output shutdown. It is
    # independent of the Task Scheduler's slightly larger hard-stop deadline.
    timer = threading.Timer(1920, lambda: os._exit(124))
    timer.daemon = True
    timer.start()
    try:
        need(args.check_runtime != bool(args.control or args.binding_sha256), 'Choose runtime check or a bound worker')
        root = Path(os.path.abspath(__file__)).parent
        value = admit(root, args.runtime_sha256)
        if args.check_runtime:
            from cryptography.hazmat.primitives.ciphers.aead import AESGCM
            key = AESGCM(bytes(32))
            data = key.encrypt(bytes(12), b'disposable runtime admission', None)
            need(key.decrypt(bytes(12), data, None) == b'disposable runtime admission', 'Runtime crypto check failed')
            if sys.stdout is not None:
                print(json.dumps(dict(schema=1, phase='backup-runtime-verified', source_revision=value['source_revision'],
                                      files=len(value['files']), isolated=True, services_started=False)))
                sys.stdout.flush()
            return 0
        need(args.control and args.binding_sha256, 'Supply the pinned worker control')
        from mod_backup_worker import tick
        return tick(args.control, args.binding_sha256)
    except Exception:
        if sys.stderr is not None:
            print('Frozen backup worker admission or execution failed; preserve its private state.', file=sys.stderr)
            sys.stderr.flush()
        return 2


if __name__ == '__main__':
    os._exit(main())  # Closes the job on every outcome, without interpreter reflush.
