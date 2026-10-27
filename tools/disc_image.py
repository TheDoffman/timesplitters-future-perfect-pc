"""Project-owned Xbox disc reader and importer, using only the Python stdlib.

Binary layout references (no upstream implementation imported):
https://xboxdevwiki.net/XDVDFS
https://multimedia.cx/xdvdfs.html
https://purelogiccode.github.io/XISOStudio/XDVDFS-Technical-Documentation.html

This replaces disc import only. It is not an independent Xbox runtime.
"""
from dataclasses import dataclass
import hashlib
import os
from pathlib import Path, PurePosixPath
import shutil
import struct
import tempfile

SECTOR = 2048
MAGIC = b'MICROSOFT*XBOX*MEDIA'
PARTITION_BASES = (0, 0x18300000, 0x0FD90000, 0x02080000)
SUPPORTED_XBE_SHA256 = '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
SUPPORTED_XBE_SIZE = 6270976
CHUNK = 1024 * 1024
MAX_ENTRIES = 100000
MAX_DEPTH = 64
MAX_TABLE = 16 * 1024 * 1024
MAX_METADATA = 64 * 1024 * 1024


class DiscError(ValueError):
    """The image or output cannot be imported without losing data."""


@dataclass(frozen=True)
class Entry:
    path: str
    offset: int
    size: int
    directory: bool


def safe_name(raw):
    # The supported title uses ASCII. Do not guess a platform-dependent code
    # page for other titles or let host filename normalization alias entries.
    try:
        name = raw.decode('ascii')
    except UnicodeDecodeError as error:
        raise DiscError('Non-ASCII disc filename is unsupported') from error
    if (not name or name in ('.', '..') or name[-1] in '. '
            or any(ord(c) < 32 or ord(c) == 127 or c in '/\\:<>"|?*' for c in name)):
        raise DiscError(f'Unsafe disc filename: {name!r}')
    stem = name.split('.')[0].upper()
    if stem in {'CON', 'PRN', 'AUX', 'NUL', 'CLOCK$',
                *(f'{prefix}{n}' for prefix in ('COM', 'LPT') for n in range(1, 10))}:
        raise DiscError(f'Reserved disc filename: {name!r}')
    return name


class DiscImage:
    def __init__(self, path):
        self.path = Path(path)
        self.file = self.path.open('rb')
        self.size = os.fstat(self.file.fileno()).st_size
        try:
            candidates = []
            for base in PARTITION_BASES:
                address = base + 32 * SECTOR
                if address + SECTOR > self.size:
                    continue
                header = self.read(address, SECTOR)
                if header[:20] == MAGIC and header[-20:] == MAGIC:
                    sector, length = struct.unpack_from('<II', header, 20)
                    candidates.append((base, sector, length))
            if len(candidates) != 1:
                raise DiscError('Expected exactly one supported XDVDFS partition')
            self.base, root_sector, root_size = candidates[0]
            self.entries = self._catalog(root_sector, root_size)
        except BaseException:
            self.file.close()
            raise

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.file.close()

    def _bounds(self, offset, size):
        if offset < 0 or size < 0 or offset > self.size or size > self.size - offset:
            raise DiscError('Disc extent is outside the image')

    def read(self, offset, size):
        self._bounds(offset, size)
        self.file.seek(offset)
        data = self.file.read(size)
        if len(data) != size:
            raise DiscError('Disc image was truncated while reading')
        return data

    def _catalog(self, root_sector, root_size):
        entries, names, tables = [], set(), set()
        metadata_bytes = 0
        directories = [('', root_sector, root_size)]
        while directories:
            parent, sector, length = directories.pop()
            if not length:
                continue
            if length > MAX_TABLE or len(PurePosixPath(parent).parts) >= MAX_DEPTH:
                raise DiscError('Directory exceeds import limits')
            metadata_bytes += length
            if metadata_bytes > MAX_METADATA:
                raise DiscError('Directory metadata exceeds import limits')
            address = self.base + sector * SECTOR
            if address in tables:
                raise DiscError('Directory table cycle or alias')
            tables.add(address)
            data = self.read(address, length)
            pending, nodes, spans = [0], set(), []
            while pending:
                node = pending.pop()
                if node in nodes:
                    raise DiscError('Directory node cycle or alias')
                nodes.add(node)
                if node + 14 > length:
                    raise DiscError('Directory node outside its table')
                header = data[node:node + 14]
                if node == 0 and header in (b'\0' * 14, b'\xff' * 14):
                    break  # Empty table, not an entry named with zero bytes.
                left, right, block, size, attributes, name_size = struct.unpack('<HHIIBB', header)
                end = node + 14 + name_size
                if end > length:
                    raise DiscError('Directory filename outside its table')
                name = safe_name(data[node + 14:end])
                spans.append((node, end))
                relative = f'{parent}/{name}' if parent else name
                folded = relative.casefold()
                if folded in names:
                    raise DiscError(f'Duplicate disc path: {relative}')
                names.add(folded)
                if len(names) > MAX_ENTRIES:
                    raise DiscError('Too many disc entries')
                directory = bool(attributes & 0x10)
                offset = self.base + block * SECTOR
                self._bounds(offset, size)
                entries.append(Entry(relative, offset, size, directory))
                if directory:
                    directories.append((relative, block, size))
                for pointer in (left, right):
                    if pointer not in (0, 0xffff):
                        pending.append(pointer * 4)
            spans.sort()
            if any(a[1] > b[0] for a, b in zip(spans, spans[1:])):
                raise DiscError('Overlapping directory records')
        return tuple(sorted(entries, key=lambda entry: entry.path.casefold()))

    def chunks(self, entry):
        remaining, offset = entry.size, entry.offset
        while remaining:
            count = min(CHUNK, remaining)
            yield self.read(offset, count)
            offset += count
            remaining -= count

    def digest(self, entry):
        result = hashlib.sha256()
        for block in self.chunks(entry):
            result.update(block)
        return result.hexdigest()

    def verify_supported(self):
        by_name = {entry.path.casefold(): entry for entry in self.entries}
        xbe = by_name.get('default.xbe')
        if (xbe is None or xbe.directory or xbe.size != SUPPORTED_XBE_SIZE
                or self.digest(xbe) != SUPPORTED_XBE_SHA256):
            raise DiscError('Image does not contain the supported US Xbox executable')
        for name in ('pak', 'xmv'):
            if name not in by_name or not by_name[name].directory:
                raise DiscError(f'Missing game directory: {name}')


def local_path(root, relative):
    """Reject aliases before touching an existing extraction on either host."""
    parts = PurePosixPath(relative).parts
    target = root
    for part in parts:
        safe_name(part.encode('ascii'))
        target /= part
        if target.is_symlink() or target.is_junction():
            raise DiscError(f'Redirected output path: {target}')
        if target.parent.exists():
            # Case-insensitive disc names must behave the same on NTFS/APFS.
            aliases = [p.name for p in target.parent.iterdir() if p.name.casefold() == part.casefold()]
            if aliases and aliases != [part]:
                raise DiscError(f'Conflicting output spelling: {target}')
    if not target.resolve().is_relative_to(root.resolve()):
        raise DiscError('Output path escapes the game directory')
    return target


def _output_root(destination):
    root = Path(destination).absolute()
    for path in (root, *root.parents):
        if path.is_symlink() or path.is_junction():
            raise DiscError(f'Redirected output directory: {path}')
    if root.exists() and not root.is_dir():
        raise DiscError('Output is not a directory')
    return root


def _same_file(image, entry, target):
    if not target.is_file() or target.stat().st_size != entry.size:
        return False
    with target.open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest() == image.digest(entry)


def extract(image, destination, *, verify_supported=True):
    """Import without replacing existing files; identical files may be reused.

Preflight every extent and existing destination before writing. New files are
streamed to private temporary names, verified, then published with an exclusive
hard link so a concurrent file is never overwritten. Completed files survive an
interruption and are checked on retry. No personal files are removed on failure.
"""
    if verify_supported:
        image.verify_supported()
    root = _output_root(destination)
    missing, reused = [], 0
    for entry in image.entries:
        path = local_path(root, entry.path)
        if entry.directory:
            if path.exists() and not path.is_dir():
                raise DiscError(f'Output directory conflicts with a file: {path}')
        elif path.exists():
            if not _same_file(image, entry, path):
                raise DiscError(f'Existing file differs; nothing replaced: {path}')
            reused += 1
        else:
            missing.append(entry)
    ancestor = root
    while not ancestor.exists():
        ancestor = ancestor.parent
    required = sum(entry.size for entry in missing)
    if shutil.disk_usage(ancestor).free < required + 16 * CHUNK:
        raise DiscError('Insufficient free space for game import')
    root.mkdir(parents=True, exist_ok=True)
    for entry in image.entries:
        if entry.directory:
            local_path(root, entry.path).mkdir(parents=True, exist_ok=True)
    copied = 0
    for entry in missing:
        target = local_path(root, entry.path)
        target.parent.mkdir(parents=True, exist_ok=True)
        fd, name = tempfile.mkstemp(prefix='.tsfp-import-', dir=target.parent)
        temporary = Path(name)
        try:
            expected = hashlib.sha256()
            with os.fdopen(fd, 'wb') as output:
                for block in image.chunks(entry):
                    output.write(block)
                    expected.update(block)
                output.flush()
                os.fsync(output.fileno())
            with temporary.open('rb') as source:
                if hashlib.file_digest(source, 'sha256').hexdigest() != expected.hexdigest():
                    raise DiscError(f'Imported bytes failed verification: {entry.path}')
            local_path(root, entry.path)
            # NTFS/APFS support hard links. Unsupported destinations fail
            # explicitly instead of falling back to an overwriting rename.
            os.link(temporary, target)
            copied += 1
        finally:
            temporary.unlink(missing_ok=True)
    return {'copied': copied, 'reused': reused, 'files': copied + reused,
            'bytes_copied': required, 'partition_offset': image.base}
