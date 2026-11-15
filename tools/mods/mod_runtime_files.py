"""Literal public library reads from an owner-controlled staged/installed runtime."""
import hashlib
import json
import os
from pathlib import Path
import re
import stat

from mod_package import require

MAX_MANIFEST = 4 * 1024**2
MAX_IMAGE = 64 * 1024**2
MAX_NATIVE = 256 * 1024**2
SHA = re.compile(r'[0-9a-f]{64}')
LIBRARY = re.compile(r'lib/[0-9a-f]{16}-[A-Za-z0-9_.+@-]+')


def trusted_path(path):
    for parent in (path.parent, *path.parent.parents):
        info = parent.lstat()
        require(stat.S_ISDIR(info.st_mode) and info.st_uid in (0, os.geteuid())
                and not info.st_mode & 0o022, 'Runtime ancestor is linked, foreign-owned or writable by others')


def checked_read(path, maximum):
    trusted_path(path)
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW)
    with os.fdopen(fd, 'rb') as stream:
        info = os.fstat(stream.fileno())
        require(stat.S_ISREG(info.st_mode) and info.st_nlink == 1 and info.st_uid in (0, os.geteuid())
                and not info.st_mode & 0o6022 and 0 < info.st_size <= maximum, 'Unsafe runtime file')
        wire = stream.read(maximum + 1)
        final = os.fstat(stream.fileno())
        require(len(wire) == info.st_size and (info.st_ino, info.st_size, info.st_mtime_ns)
                == (final.st_ino, final.st_size, final.st_mtime_ns), 'Runtime file changed while reading')
    return wire, info


def manifest(wire):
    def pairs(items):
        result = {}
        for key, value in items:
            require(key not in result, 'Duplicate runtime receipt field')
            result[key] = value
        return result

    def invalid_number(_):
        require(False, 'Runtime receipt contains a noninteger number')

    require(len(wire) <= MAX_MANIFEST, 'Runtime receipt exceeds budget')
    try:
        value = json.loads(wire.decode('utf-8'), object_pairs_hook=pairs,
                           parse_float=invalid_number, parse_constant=invalid_number)
    except (UnicodeError, ValueError, RecursionError) as error:
        require(False, 'Invalid runtime receipt: ' + type(error).__name__)
    require(isinstance(value, dict) and type(value.get('schema')) is int and value['schema'] == 1
            and value.get('producer') == 'tsfp-host-runtime-v1'
            and isinstance(value.get('files'), dict) and 0 < len(value['files']) <= 20000,
            'Unrecognized runtime receipt')
    return value


def runtime_access(executable):
    """Return verified image/certificate literals, or None for an unbundled tool.

    The caller is trusted host configuration, never an uploaded package field.
    A present but damaged marker always fails; it cannot select legacy policy.
    """
    executable = Path(os.path.abspath(executable))
    root = executable.parent.parent
    marker = root / 'runtime.json'
    if not marker.exists() and not marker.is_symlink():
        return None
    require(executable.parent.name == 'bin' and executable.name in ('clamscan', 'freshclam', 'caddy'),
            'Unexpected executable beside runtime receipt')
    wire, _ = checked_read(marker, MAX_MANIFEST)
    value = manifest(wire)
    files = value['files']
    entry = 'bin/' + executable.name
    require(entry in files, 'Runtime receipt omits selected executable')
    libraries = [name for name in files if name.startswith('lib/')]
    require(0 < len(libraries) <= 256 and all(LIBRARY.fullmatch(name) for name in libraries),
            'Invalid runtime library inventory')
    result, total = [], 0
    for name in [entry, *sorted(libraries)]:
        record = files[name]
        require(isinstance(record, dict) and 'symlink' not in record
                and isinstance(record.get('sha256'), str) and SHA.fullmatch(record['sha256'])
                and type(record.get('bytes')) is int and 0 < record['bytes'] <= MAX_IMAGE
                and type(record.get('mode')) is int and record['mode'] in (0o644, 0o755),
                'Invalid runtime image receipt')
        total += record['bytes']
        require(total <= MAX_NATIVE, 'Runtime native read budget exceeded')
        path = root / name
        image, info = checked_read(path, MAX_IMAGE)
        require(len(image) == record['bytes'] and stat.S_IMODE(info.st_mode) == record['mode']
                and hashlib.sha256(image).hexdigest() == record['sha256'], 'Runtime image differs from receipt')
        result.append(path)
    certificate = root / 'Certs/clamav/clamav.crt'
    record = files.get('Certs/clamav/clamav.crt')
    require(isinstance(record, dict) and isinstance(record.get('sha256'), str) and SHA.fullmatch(record['sha256'])
            and type(record.get('bytes')) is int and 0 < record['bytes'] <= 65536
            and record.get('mode') == 0o644 and 'symlink' not in record,
            'Runtime receipt omits the public CVD certificate')
    content, info = checked_read(certificate, 65536)
    require(len(content) == record['bytes'] and stat.S_IMODE(info.st_mode) == 0o644
            and hashlib.sha256(content).hexdigest() == record['sha256'], 'Runtime CVD certificate differs from receipt')
    return dict(images=result, certificate=certificate)
