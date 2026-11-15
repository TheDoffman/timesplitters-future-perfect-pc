"""Offline extraction of exactly pinned application wheels into a new runtime."""
import email.parser
import hashlib
import io
import os
from pathlib import Path, PurePosixPath
import re
import stat
import zipfile

MAX_ARCHIVE = 32 * 1024**2
MAX_EXPANDED = 64 * 1024**2
MAX_ENTRIES = 2048


def require(value, message):
    if not value:
        raise ValueError(message)


def canonical(name):
    return re.sub(r'[-_.]+', '-', name).lower()


def parse_pins(texts):
    result = {}
    for text in texts:
        pending = ''
        for raw in text.splitlines():
            line = raw.strip()
            if not line or line.startswith('#'):
                continue
            if line.endswith('\\'):
                pending += line[:-1] + ' '
                continue
            line, pending = pending + line, ''
            if line in ('--only-binary=:all:', '-r requirements-signing.txt'):
                continue
            match = re.fullmatch(r'([A-Za-z0-9_.-]+)==([0-9][A-Za-z0-9_.+]*)\s+((?:--hash=sha256:[0-9a-f]{64}\s*)+)', line)
            require(match, 'Unsupported unpinned runtime requirement')
            name, version, hashes = match.groups()
            name = canonical(name)
            require(name not in result, 'Duplicate runtime requirement')
            result[name] = dict(version=version, hashes=set(re.findall(r'sha256:([0-9a-f]{64})', hashes)))
        require(not pending, 'Incomplete runtime requirement continuation')
    require(result, 'No pinned runtime requirements')
    return result


def inspect(wire, pins):
    require(0 < len(wire) <= MAX_ARCHIVE, 'Wheel archive exceeds budget')
    digest = hashlib.sha256(wire).hexdigest()
    admitted = [name for name, pin in pins.items() if digest in pin['hashes']]
    require(len(admitted) == 1, 'Wheel digest is not an exact pinned input')
    name = admitted[0]
    entries, seen, metadata, expanded = [], set(), [], 0
    with zipfile.ZipFile(io.BytesIO(wire)) as archive:
        require(0 < len(archive.infolist()) <= MAX_ENTRIES, 'Wheel entry count exceeds budget')
        for item in archive.infolist():
            path = PurePosixPath(item.filename.rstrip('/'))
            require(item.orig_filename == item.filename and item.filename and item.filename.isascii()
                    and all(ord(c) >= 32 and ord(c) != 127 for c in item.filename)
                    and not path.is_absolute() and '\\' not in item.filename
                    and '\0' not in item.filename and all(p not in ('', '.', '..') for p in item.filename.rstrip('/').split('/'))
                    and not any(':' in p for p in path.parts), 'Unsafe wheel member path')
            require(not any(p.endswith('.data') for p in path.parts) and not str(path).endswith('.pth'),
                    'Wheel installation hooks or data schemes are unsupported')
            key = str(path).casefold()
            require(key not in seen, 'Duplicate wheel member path')
            seen.add(key)
            mode = item.external_attr >> 16
            require(stat.S_IFMT(mode) in (0, stat.S_IFREG, stat.S_IFDIR)
                    and not mode & 0o6022 and not item.flag_bits & 1
                    and item.compress_type in (zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED), 'Unsafe wheel member type/mode')
            expanded += item.file_size
            require(expanded <= MAX_EXPANDED, 'Wheel expanded size exceeds budget')
            if item.is_dir():
                continue
            content = archive.read(item)
            require(len(content) == item.file_size, 'Wheel member changed size')
            entries.append((str(path), content, 0o755 if mode & 0o111 else 0o644))
            if str(path).endswith('.dist-info/METADATA'):
                require(len(content) <= 1024**2 and len(path.parts) == 2, 'Invalid wheel metadata location/size')
                metadata.append(email.parser.BytesParser().parsebytes(content, headersonly=True))
    require(len(metadata) == 1 and metadata[0].get_all('Name') == [metadata[0].get('Name')]
            and metadata[0].get_all('Version') == [pins[name]['version']]
            and canonical(metadata[0].get('Name', '')) == name, 'Wheel metadata differs from pinned package')
    expected_info = name.replace('-', '_') + '-' + pins[name]['version'] + '.dist-info'
    require(all(not p.split('/')[0].endswith('.dist-info') or p.split('/')[0] == expected_info for p, _, _ in entries),
            'Wheel distribution metadata namespace mismatch')
    require(any(p.startswith(expected_info + '/') and p.rsplit('/', 1)[-1].lower().startswith(('license', 'copying'))
                for p, _, _ in entries), 'Wheel license notice missing')
    return dict(name=name, version=pins[name]['version'], sha256=digest, expanded_bytes=expanded), entries


def install(wheelhouse, destination, pins):
    """Validate the complete set before creating any destination or payload."""
    wheelhouse, destination = Path(wheelhouse), Path(destination)
    require(all(not p.is_symlink() for p in (wheelhouse, *wheelhouse.parents)), 'Linked wheelhouse refused')
    require(all(not p.is_symlink() for p in (destination, *destination.parents)), 'Linked installation path refused')
    require(not destination.exists() and not destination.is_symlink(), 'Choose a new wheel installation directory')
    require(wheelhouse.is_dir(), 'Missing wheelhouse')
    paths = sorted(wheelhouse.iterdir())
    require(len(paths) == len(pins), 'Wheelhouse must contain exactly one wheel per pinned package')
    payload, records, packages, seen, total = [], {}, {}, set(), 0
    for path in paths:
        info = path.lstat()
        require(path.suffix == '.whl' and stat.S_ISREG(info.st_mode) and info.st_nlink == 1
                and info.st_uid in (0, getattr(os, 'getuid', lambda: 0)()) and not info.st_mode & 0o6022
                and 0 < info.st_size <= MAX_ARCHIVE, 'Unsafe wheel input')
        with os.fdopen(os.open(path, os.O_RDONLY | getattr(os, 'O_NOFOLLOW', 0)), 'rb') as stream:
            observed = os.fstat(stream.fileno())
            require((observed.st_dev, observed.st_ino, observed.st_size) == (info.st_dev, info.st_ino, info.st_size),
                    'Wheel input changed before open')
            wire = stream.read(MAX_ARCHIVE + 1)
        package, entries = inspect(wire, pins)
        require(package['name'] not in packages, 'Duplicate pinned wheel package')
        packages[package['name']] = dict(package, filename=path.name)
        for name, content, mode in entries:
            key = name.casefold()
            require(key not in seen, 'Wheel payload collision')
            seen.add(key)
            total += len(content)
            require(len(payload) < MAX_ENTRIES and total <= MAX_EXPANDED, 'Combined wheel payload exceeds budget')
            payload.append((name, content, mode))
            records[name] = dict(wheel_sha256=package['sha256'], package=package['name'],
                                 source_sha256=hashlib.sha256(content).hexdigest(), bytes=len(content), mode=mode)
    require(set(packages) == set(pins), 'Incomplete pinned wheel set')
    # A file may never become another wheel's parent directory.
    names = {name.casefold() for name, _, _ in payload}
    require(all(not any(str(p).casefold() in names for p in PurePosixPath(name).parents if str(p) != '.')
                for name, _, _ in payload), 'Wheel file/directory collision')
    destination.mkdir(mode=0o755)
    for name, content, mode in payload:
        path = destination / name
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open('xb') as stream:
            stream.write(content)
        path.chmod(mode)
    return packages, records
