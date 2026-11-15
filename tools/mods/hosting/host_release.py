"""Bounded manifest verification and exclusive copies for immutable host releases."""
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import posixpath
import re
import stat

MAX_BYTES = 512 * 1024**2
SHA = re.compile(r'[0-9a-f]{64}')


def require(value, message):
    if not value:
        raise ValueError(message)


def decode(wire, maximum):
    require(0 < len(wire) <= maximum, 'Release metadata exceeds budget')
    def pairs(items):
        result = {}
        for key, value in items:
            require(key not in result, 'Duplicate release metadata field')
            result[key] = value
        return result
    def invalid(_):
        raise ValueError('Noninteger release metadata number')
    return json.loads(wire.decode('utf-8'), object_pairs_hook=pairs, parse_float=invalid, parse_constant=invalid)


def name(value):
    require(isinstance(value, str) and value and value.isascii() and all(32 <= ord(c) < 127 for c in value)
            and '\\' not in value and ':' not in value and not value.startswith('/')
            and all(p not in ('', '.', '..') for p in value.split('/')), 'Unsafe release member path')
    return PurePosixPath(value)


def normalize(files):
    require(isinstance(files, dict) and 0 < len(files) <= 24000, 'Invalid release inventory')
    result, folded, total = {}, set(), 0
    for key, record in files.items():
        relative = name(key)
        require(key.casefold() not in folded and isinstance(record, dict), 'Ambiguous release inventory')
        folded.add(key.casefold())
        if 'symlink' in record:
            link = record['symlink']
            require(isinstance(link, str) and link and link.isascii() and not link.startswith('/')
                    and '\\' not in link and ':' not in link and all(32 <= ord(c) < 127 for c in link), 'Unsafe release link')
            resolved = posixpath.normpath(str(relative.parent / link))
            require(resolved != '..' and not resolved.startswith('../'), 'Release link escapes its component')
            result[key] = dict(symlink=link)
        else:
            require(isinstance(record.get('sha256'), str) and SHA.fullmatch(record['sha256'])
                    and type(record.get('bytes')) is int and 0 <= record['bytes'] <= 64 * 1024**2
                    and type(record.get('mode')) is int and record['mode'] in (0o644, 0o755), 'Invalid release file record')
            total += record['bytes']
            require(total <= MAX_BYTES, 'Release payload exceeds budget')
            result[key] = {k: record[k] for k in ('sha256', 'bytes', 'mode')}
    require(all(not any(str(p).casefold() in folded for p in PurePosixPath(k).parents if str(p) != '.') for k in result),
            'Release file/directory collision')
    return result


def directory(path, uid):
    path = Path(os.path.abspath(path))
    fd = os.open(path.anchor, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        for part in path.parts[1:]:
            child = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=fd)
            os.close(fd)
            fd = child
            info = os.fstat(fd)
            require(stat.S_ISDIR(info.st_mode) and info.st_uid in (0, uid) and not info.st_mode & 0o022,
                    'Release ancestor is foreign-owned or writable by others')
        return fd
    except BaseException:
        os.close(fd)
        raise


def read_file(root, relative, uid, maximum):
    relative = name(relative)
    parent = directory(Path(root) / str(relative.parent), uid)
    try:
        fd = os.open(relative.name, os.O_RDONLY | os.O_NOFOLLOW, dir_fd=parent)
    finally:
        os.close(parent)
    with os.fdopen(fd, 'rb') as stream:
        before = os.fstat(stream.fileno())
        require(stat.S_ISREG(before.st_mode) and before.st_uid == uid and before.st_nlink == 1
                and not before.st_mode & 0o6022 and 0 <= before.st_size <= maximum, 'Unsafe release input file')
        wire = stream.read(maximum + 1)
        after = os.fstat(stream.fileno())
        require(len(wire) == before.st_size and (before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns)
                == (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns), 'Release input changed while reading')
    return wire


def inventory(root, uid, directory_mode=None):
    result = set()
    pending = [Path(root)]
    visited = 0
    while pending:
        path = pending.pop()
        visited += 1
        require(visited <= 24000 and len(path.relative_to(root).parts) <= 64, 'Release directory budget exceeded')
        fd = directory(path, uid)
        try:
            if directory_mode is not None:
                require(stat.S_IMODE(os.fstat(fd).st_mode) == directory_mode, 'Installed release directory mode changed')
            with os.scandir(fd) as scan:
                for entry in scan:
                    require(len(result) < 24000 and len(pending) < 24000, 'Release tree exceeds entry budget')
                    child = path / entry.name
                    info = entry.stat(follow_symlinks=False)
                    if stat.S_ISDIR(info.st_mode):
                        pending.append(child)
                    else:
                        require(stat.S_ISREG(info.st_mode) or stat.S_ISLNK(info.st_mode), 'Special release tree entry')
                        result.add(str(child.relative_to(root)).replace(os.sep, '/'))
        finally:
            os.close(fd)
    return result


def verify(root, files, uid, readonly=False, directory_mode=None):
    require(inventory(root, uid, directory_mode) == set(files), 'Release has missing or unrecorded files')
    for key, record in files.items():
        path = Path(root) / key
        if 'symlink' in record:
            parent = directory(path.parent, uid)
            try:
                info = os.stat(path.name, dir_fd=parent, follow_symlinks=False)
                require(stat.S_ISLNK(info.st_mode) and info.st_uid == uid
                        and os.readlink(path.name, dir_fd=parent) == record['symlink'], 'Release link changed')
                require(path.resolve(strict=True).is_relative_to(Path(root).resolve(strict=True)), 'Release link escaped')
            finally:
                os.close(parent)
        else:
            wire = read_file(root, key, uid, record['bytes'])
            require(len(wire) == record['bytes'] and hashlib.sha256(wire).hexdigest() == record['sha256'], 'Release digest mismatch')
            expected_mode = record['mode'] & ~0o200 if readonly else record['mode']
            require(stat.S_IMODE(path.lstat().st_mode) == expected_mode, 'Release file mode changed')


def plan(bundle, expected):
    require(SHA.fullmatch(expected or ''), 'An explicit descriptor SHA256 is required')
    bundle = Path(os.path.abspath(bundle))
    uid = bundle.lstat().st_uid
    wire = read_file(bundle.parent, bundle.name, uid, 512 * 1024)
    require(hashlib.sha256(wire).hexdigest() == expected, 'Release descriptor digest mismatch')
    data = decode(wire, 512 * 1024)
    require(isinstance(data, dict) and set(data) == {'schema', 'producer', 'source_revision', 'owner_uid',
            'runtime_root', 'runtime_sha256', 'source_root', 'source_files'}
            and type(data['schema']) is int and data['schema'] == 1 and data['producer'] == 'tsfp-host-release-v1'
            and type(data['owner_uid']) is int and data['owner_uid'] == uid and uid > 0
            and isinstance(data['source_revision'], str) and re.fullmatch(r'[0-9a-f]{40}', data['source_revision']),
            'Invalid host release descriptor')
    for field in ('runtime_root', 'source_root'):
        require(isinstance(data[field], str) and Path(data[field]).is_absolute(), 'Release input root must be absolute')
    runtime_wire = read_file(data['runtime_root'], 'runtime.json', uid, 4 * 1024**2)
    require(isinstance(data['runtime_sha256'], str) and SHA.fullmatch(data['runtime_sha256'])
            and hashlib.sha256(runtime_wire).hexdigest() == data['runtime_sha256'],
            'Runtime receipt differs from release descriptor')
    runtime = decode(runtime_wire, 4 * 1024**2)
    require(isinstance(runtime, dict) and type(runtime.get('schema')) is int and runtime['schema'] == 1
            and runtime.get('producer') == 'tsfp-host-runtime-v1' and isinstance(runtime.get('applications'), dict)
            and isinstance(runtime['applications'].get('packages'), dict)
            and set(runtime['applications']['packages']) == {'gunicorn', 'cryptography', 'cffi', 'pycparser'},
            'A complete application runtime is required')
    runtime_files = normalize(runtime.get('files'))
    python = 'Frameworks/Python.framework/Versions/3.12/bin/python3.12'
    require(runtime.get('python') == python and {python, 'bin/clamscan', 'bin/freshclam', 'bin/caddy',
            'Certs/clamav/clamav.crt'}.issubset(runtime_files), 'Incomplete or escaping runtime entry points')
    require(all(runtime_files[key].get('mode') == 0o755 for key in (python, 'bin/clamscan', 'bin/freshclam', 'bin/caddy')),
            'Runtime entry point is not an executable regular file')
    require('runtime.json' not in runtime_files, 'Recursive runtime receipt')
    runtime_files['runtime.json'] = dict(bytes=len(runtime_wire), sha256=data['runtime_sha256'], mode=0o644)
    # Staging receipt is private; installed receipt is readable by the locked account.
    staged_runtime = dict(runtime_files, **{'runtime.json': dict(runtime_files['runtime.json'], mode=0o600)})
    source_files = normalize(data['source_files'])
    require(len(source_files) <= 256 and not any('symlink' in r for r in source_files.values())
            and all(k.startswith(('tools/mods/', 'config/mods/', 'build/scan-guardian/')) for k in source_files),
            'Invalid server source component')
    required = {'tools/mods/mod_api.py', 'tools/mods/mod_runtime_files.py', 'tools/mods/scan_guardian_mac.c',
                'tools/mods/scan_child.py', 'build/scan-guardian/scan-guardian-mac', 'build/scan-guardian/receipt.json'}
    require(required.issubset(source_files), 'Incomplete server source/guardian component')
    require(source_files['build/scan-guardian/scan-guardian-mac']['mode'] == 0o755,
            'Guardian must remain an executable regular file')
    verify(Path(data['runtime_root']), staged_runtime, uid)
    verify(Path(data['source_root']), source_files, uid, readonly=True)
    guardian_wire = read_file(data['source_root'], 'build/scan-guardian/receipt.json', uid, 4096)
    guardian = decode(guardian_wire, 4096)
    require(guardian.get('producer') == 'tsfp-scan-guardian-v1' and guardian.get('schema') == 1
            and guardian.get('source_sha256') == source_files['tools/mods/scan_guardian_mac.c']['sha256']
            and guardian.get('sha256') == source_files['build/scan-guardian/scan-guardian-mac']['sha256'],
            'Guardian source/image receipt mismatch')
    total = sum(r.get('bytes', 0) for files in (runtime_files, source_files) for r in files.values())
    require(total <= MAX_BYTES, 'Combined release exceeds storage budget')
    return dict(descriptor=data, descriptor_wire=wire, identity=expected, uid=uid, runtime=runtime,
                components=[('runtime', Path(data['runtime_root']), runtime_files),
                            ('source', Path(data['source_root']), source_files)], bytes=total)


def copy_component(source, target, files, uid, created_directory):
    created_directory(target)
    links = []
    for key, record in files.items():
        path = target / key
        for directory_path in reversed(path.parent.parents):
            if directory_path.is_relative_to(target) and directory_path != target and not directory_path.exists():
                created_directory(directory_path)
        if path.parent != target and not path.parent.exists():
            created_directory(path.parent)
        if 'symlink' in record:
            parent = directory(Path(source) / str(PurePosixPath(key).parent), uid)
            try:
                require(os.readlink(PurePosixPath(key).name, dir_fd=parent) == record['symlink'], 'Release source link changed')
            finally:
                os.close(parent)
            links.append((path, record['symlink']))
        else:
            wire = read_file(source, key, uid, record['bytes'])
            require(len(wire) == record['bytes'] and hashlib.sha256(wire).hexdigest() == record['sha256'], 'Source changed during release copy')
            with path.open('xb') as stream:
                stream.write(wire)
                stream.flush()
                os.fsync(stream.fileno())
            path.chmod(record['mode'])
    for path, link in links:
        path.symlink_to(link)
