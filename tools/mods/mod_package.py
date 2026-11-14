"""Portable, bounded mod packages. Validation never executes uploaded code."""
import hashlib
from decimal import Decimal, InvalidOperation
import json
import re
import stat
import struct
import zipfile
import zlib
from pathlib import Path

MAX_ARCHIVE = 256 * 1024 * 1024
MAX_TOTAL = 256 * 1024 * 1024
MAX_FILE = 64 * 1024 * 1024
MAX_FILES = 2048
MAX_MANIFEST = 256 * 1024
MAX_MAP_JSON = 4 * 1024 * 1024
MAX_SCRIPT = 1024 * 1024
ID = re.compile(r'[a-z][a-z0-9]*(?:-[a-z0-9]+)*\Z')
VERSION = re.compile(r'(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\Z')
SHA = re.compile(r'[0-9a-f]{64}\Z')
RESERVED = {'con', 'prn', 'aux', 'nul', *(f'com{i}' for i in range(1, 10)),
            *(f'lpt{i}' for i in range(1, 10))}


class ModError(ValueError):
    pass


def digest(data):
    return hashlib.sha256(data).hexdigest()


def require(condition, message):
    if not condition:
        raise ModError(message)


def portable_path(name):
    require(isinstance(name, str) and 0 < len(name) <= 180, 'Invalid path length')
    parts = name.split('/')
    for part in parts:
        require(re.fullmatch(r'[A-Za-z0-9_][A-Za-z0-9_.-]*', part) is not None
                and not part.endswith('.') and part.split('.')[0].lower() not in RESERVED,
                f'Nonportable or unsafe path: {name}')
    return name


def role(name):
    portable_path(name)
    suffix = Path(name).suffix.lower()
    prefix = name.split('/')[0]
    require('/' in name, 'Payload files must be in a content directory')
    allowed = {'textures': {'.dds'}, 'audio': {'.wav', '.ogg'},
               'maps': {'.json'}, 'scripts': {'.lua'}}
    require(prefix in allowed and suffix in allowed[prefix], f'Unsupported payload: {name}')
    if prefix == 'textures':
        require(re.fullmatch(r'textures/[0-9a-f]{16}\.dds', name) is not None,
                'Textures require a flat lowercase 16-digit game texture key')
    return prefix


def validate_payload(name, data):
    """Type sanity is an extra gate, not proof a decoder or script is safe."""
    kind = role(name)
    executable_headers = (b'MZ', b'\x7fELF', b'\xcf\xfa\xed\xfe', b'\xfe\xed\xfa\xcf',
                          b'\xce\xfa\xed\xfe', b'\xfe\xed\xfa\xce')
    require(not data.startswith(executable_headers), 'Native executable payload forbidden')
    if kind == 'scripts':
        require(len(data) <= MAX_SCRIPT, 'Script source exceeds1MiB')
        try:
            source = data.decode('utf-8')
        except UnicodeError as exc:
            raise ModError('Scripts must be UTF-8 Lua source') from exc
        require('\x00' not in source and not data.startswith(b'\x1bLua'),
                'Binary/compiled Lua forbidden')
    elif kind == 'maps':
        require(len(data) <= MAX_MAP_JSON, 'Map descriptor exceeds4MiB')
        require(isinstance(read_json(data), dict), 'Draft map payload must be a JSON object')
    elif kind == 'textures':
        validate_dds(data)
    elif kind == 'audio':
        if name.lower().endswith('.wav'):
            require(data.startswith(b'RIFF') and data[8:12] == b'WAVE', 'Audio payload must be WAV')
        else:
            require(data.startswith(b'OggS'), 'Audio payload must be Ogg')


def validate_dds(data):
    """Mirror the game's bounded DDS contract without invoking a native decoder."""
    require(128 <= len(data) <= 24 * 1024 * 1024, 'DDS payload exceeds renderer limits')
    words = struct.unpack_from('<32I', data)
    require(words[0] == 0x20534444 and words[1] == 124 and words[19] == 32
            and words[28] == 0 and words[6] == 0, 'Invalid DDS header')
    height, width, levels = words[3], words[4], words[7]
    require(0 < width <= 4096 and 0 < height <= 4096
            and not width & (width - 1) and not height & (height - 1),
            'DDS dimensions must be powers of two up to 4096')
    require(levels == max(width, height).bit_length() and levels <= 13,
            'DDS requires a complete mip chain')
    flags, fourcc = words[20], words[21]
    if flags == 4 and fourcc in (0x31545844, 0x33545844, 0x35545844):
        block = 8 if fourcc == 0x31545844 else 16
    else:
        require(flags == 0x41 and fourcc == 0 and words[22:27] ==
                (32, 0xff0000, 0xff00, 0xff, 0xff000000), 'Unsupported DDS format')
        block = 0
    expected = 128
    for _ in range(levels):
        expected += ((width + 3) // 4) * ((height + 3) // 4) * block if block else width * height * 4
        width, height = max(1, width // 2), max(1, height // 2)
    require(len(data) == expected, 'DDS mip data is truncated or has trailing bytes')


def object_pairs(pairs):
    obj = {}
    for key, value in pairs:
        require(key not in obj, f'Duplicate JSON key: {key}')
        obj[key] = value
    return obj


def json_budget(text):
    """Count tokens/depth/collection members before a JSON decoder allocates objects."""
    stack = []
    quoted = escaped = atom = False
    nodes = 0
    for char in text:
        if quoted:
            if escaped:
                escaped = False
            elif char == '\\':
                escaped = True
            elif char == '"':
                quoted = False
            continue
        if char == '"':
            quoted, atom = True, False
            nodes += 1
        elif char in '{[':
            stack.append(['}' if char == '{' else ']', 1])
            require(len(stack) <= 32, 'JSON nesting exceeds32 levels')
            nodes += 1
            atom = False
        elif char in '}]':
            require(stack and stack[-1][0] == char, 'Invalid JSON nesting')
            stack.pop()
            atom = False
        elif char == ',':
            require(stack, 'Invalid JSON collection')
            stack[-1][1] += 1
            require(stack[-1][1] <= 4096, 'JSON collection exceeds4096 members')
            atom = False
        elif char in ': \t\r\n':
            atom = False
        elif not atom:
            atom = True
            nodes += 1
        require(nodes <= 65536, 'JSON token budget exceeded')
    require(not quoted and not stack, 'Incomplete JSON')


def read_json(data):
    try:
        if isinstance(data, bytes):
            require(len(data) <= MAX_MAP_JSON, 'JSON input exceeds4MiB')
            data = data.decode('utf-8')
        require(isinstance(data, str) and len(data.encode('utf-8')) <= MAX_MAP_JSON, 'JSON input exceeds4MiB')
        require(not data.startswith('\ufeff'), 'JSON must be UTF-8 without a byte-order mark')
        json_budget(data)
        def number(value, integer=False):
            require(len(value) <= 128, 'JSON number exceeds limit')
            parsed = Decimal(value)
            require(parsed.is_finite() and parsed.copy_abs() <= Decimal('79228162514264337593543950335'),
                    'JSON number exceeds supported range')
            return int(value) if integer else parsed
        return json.loads(data, object_pairs_hook=object_pairs,
                          parse_int=lambda value: number(value, True), parse_float=number,
                          parse_constant=lambda _: require(False, 'Nonfinite JSON value'))
    except (UnicodeError, json.JSONDecodeError, RecursionError, InvalidOperation) as exc:
        raise ModError('Invalid JSON') from exc


def validate_manifest(m):
    keys = {'schema', 'id', 'version', 'name', 'author', 'description', 'game',
            'api', 'dependencies', 'files'}
    require(isinstance(m, dict) and set(m) == keys, 'Unexpected manifest fields')
    require(type(m['schema']) is int and m['schema'] == 1, 'Unsupported package schema')
    require(m['game'] == 'tsfp-us-45410066' and m['api'] == 'tsfp-mod-v1-draft',
            'Unsupported game or mod API')
    require(isinstance(m['id'], str) and len(m['id']) <= 80 and ID.fullmatch(m['id']),
            'Invalid mod ID')
    require(isinstance(m['version'], str) and len(m['version']) <= 32
            and VERSION.fullmatch(m['version']), 'Version must be major.minor.patch')
    for field, limit in [('name', 120), ('author', 120), ('description', 4000)]:
        value = m[field]
        require(isinstance(value, str) and 0 < len(value) <= limit
                and all(ord(c) >= 32 and not 127 <= ord(c) <= 159 for c in value), f'Invalid {field}')
        try:
            require(len(value.encode('utf-16-le')) // 2 <= limit, f'Invalid {field} length')
        except UnicodeError as exc:
            raise ModError(f'Invalid {field} Unicode') from exc
    deps = m['dependencies']
    require(isinstance(deps, dict) and len(deps) <= 32, 'Invalid dependencies')
    for key, value in deps.items():
        require(isinstance(key, str) and len(key) <= 80 and ID.fullmatch(key)
                and key != m['id'] and isinstance(value, str) and len(value) <= 32
                and VERSION.fullmatch(value), 'Dependencies require exact versions and unique IDs')
    files = m['files']
    require(isinstance(files, dict) and 0 < len(files) <= MAX_FILES, 'Invalid file manifest')
    total = 0
    seen = set()
    for name, entry in files.items():
        role(name)
        require(name.lower() not in seen, 'Case-colliding paths')
        seen.add(name.lower())
        require(isinstance(entry, dict) and set(entry) == {'sha256', 'size'}, 'Invalid file entry')
        require(isinstance(entry['sha256'], str) and SHA.fullmatch(entry['sha256']), 'Invalid digest')
        require(type(entry['size']) is int and 0 < entry['size'] <= MAX_FILE, 'Invalid file size')
        total += entry['size']
    require(total <= MAX_TOTAL, 'Package expands beyond the total limit')
    return m


def validate_zip_metadata(path):
    """Bound the central directory before ZipFile eagerly allocates ZipInfo objects."""
    path = Path(path)
    length = path.stat().st_size
    require(22 <= length <= MAX_ARCHIVE, 'Invalid archive size')
    with path.open('rb') as stream:
        require(stream.read(4) == b'PK\x03\x04', 'ZIP prefixes and self-extracting archives are forbidden')
        tail_size = min(length, 1046)  # EOCD plus at most1024 bytes of archive comment.
        stream.seek(length - tail_size)
        tail = stream.read(tail_size)
        end = -1
        for at in range(len(tail) - 22, -1, -1):
            if tail[at:at+4] == b'PK\x05\x06' and at + 22 + struct.unpack_from('<H', tail, at+20)[0] == len(tail):
                end = at
                break
        require(end >= 0, 'Bounded single-disk ZIP end record required')
        _, disk, start, disk_count, count, size, offset, _ = struct.unpack_from('<4s4H2IH', tail, end)
        require(disk == start == 0 and disk_count == count and 2 <= count <= MAX_FILES + 1,
                'Invalid archive entry count or disk layout')
        directory_end = length - tail_size + end
        require(4 <= offset and 0 < size <= (MAX_FILES + 1) * (46 + 180 + 4096 + 1024)
                and offset + size == directory_end, 'Invalid or excessive ZIP directory')
        stream.seek(offset)
        for _ in range(count):
            require(stream.tell() + 46 <= directory_end, 'Truncated ZIP directory')
            header = stream.read(46)
            require(len(header) == 46 and header[:4] == b'PK\x01\x02', 'Invalid ZIP directory entry')
            name, extra, comment, entry_disk = struct.unpack_from('<4H', header, 28)
            require(0 < name <= 180 and extra <= 4096 and comment <= 1024 and entry_disk == 0,
                    'ZIP entry metadata exceeds limits')
            skip = name + extra + comment
            require(stream.tell() + skip <= directory_end, 'Truncated ZIP entry metadata')
            stream.seek(skip, 1)
        require(stream.tell() == directory_end, 'ZIP directory entry count mismatch')


def inspect_package(path, destination=None):
    """Validate all entries, including actual bytes/CRC; optionally extract safely.

    Destination must be a newly created private temporary directory owned by the
    caller. No extractall(), links, directory entries, or filesystem permissions
    from the upload are used.
    """
    path = Path(path)
    require(path.stat().st_size <= MAX_ARCHIVE, 'Archive too large')
    validate_zip_metadata(path)
    try:
        with zipfile.ZipFile(path) as archive:
            infos = archive.infolist()
            require(1 < len(infos) <= MAX_FILES + 1, 'Invalid archive entry count')
            seen = set()
            for info in infos:
                portable_path(info.filename)
                require(info.orig_filename == info.filename, 'Truncated or NUL ZIP path forbidden')
                require(info.filename.lower() not in seen, 'Duplicate or case-colliding ZIP path')
                seen.add(info.filename.lower())
                mode = info.external_attr >> 16
                require(stat.S_IFMT(mode) in (0, stat.S_IFREG), 'Only regular files allowed')
                require(not info.is_dir() and not info.flag_bits & 1, 'Directories/encryption forbidden')
                require(info.compress_type in (zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED),
                        'Unsupported compression')
                limit = MAX_MANIFEST if info.filename == 'mod.json' else MAX_FILE
                require(0 < info.file_size <= limit, 'ZIP entry exceeds limit')
            for name in seen:
                parents = name.split('/')[:-1]
                require(all('/'.join(parents[:i]) not in seen for i in range(1, len(parents) + 1)),
                        'File/directory path collision')
            require('mod.json' in [i.filename for i in infos], 'Missing mod.json')
            m = validate_manifest(read_json(archive.read('mod.json')))
            require(set(m['files']) | {'mod.json'} == {i.filename for i in infos},
                    'ZIP and file manifest differ')
            for info in infos:
                limit = MAX_MANIFEST if info.filename == 'mod.json' else m['files'][info.filename]['size']
                require(info.file_size == limit or info.filename == 'mod.json', 'File size mismatch')
                # Bounded reads also force decompression/CRC verification before publication.
                with archive.open(info) as stream:
                    data = stream.read(limit + 1)
                require(len(data) == info.file_size and len(data) <= limit, 'Actual size mismatch')
                if info.filename != 'mod.json':
                    require(digest(data) == m['files'][info.filename]['sha256'], 'File digest mismatch')
                    validate_payload(info.filename, data)
                if destination is not None:
                    target = Path(destination) / info.filename
                    target.parent.mkdir(parents=True, exist_ok=True)
                    with target.open('xb') as output:
                        output.write(data)
            return m
    except (zipfile.BadZipFile, NotImplementedError, RuntimeError, EOFError, zlib.error) as exc:
        raise ModError('Invalid ZIP package') from exc


def build_package(source, output):
    """Build a deterministic package from a creator's mod.json and content tree."""
    source, output = Path(source).resolve(), Path(output).absolute()
    require(not output.is_relative_to(source), 'Package output must be outside its source tree')
    m = read_json((source / 'mod.json').read_bytes())
    require(isinstance(m, dict), 'Manifest must be an object')
    entries = {}
    total = 0
    for file in sorted(source.rglob('*')):
        require(not file.is_symlink() and not file.is_junction(), 'Source links forbidden')
        if file.is_dir():
            continue
        name = file.relative_to(source).as_posix()
        if name == 'mod.json':
            continue
        role(name)
        require(file.is_file() and 0 < file.stat().st_size <= MAX_FILE, 'Invalid source file size')
        total += file.stat().st_size
        require(total <= MAX_TOTAL and len(entries) < MAX_FILES, 'Source exceeds package limits')
        data = file.read_bytes()
        validate_payload(name, data)
        entries[name] = data
    m['files'] = {name: {'sha256': digest(data), 'size': len(data)} for name, data in entries.items()}
    validate_manifest(m)
    manifest = json.dumps(m, sort_keys=True, separators=(',', ':')).encode()
    require(len(manifest) <= MAX_MANIFEST, 'Manifest too large')
    with output.open('xb') as stream:
        with zipfile.ZipFile(stream, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
            for name, data in [('mod.json', manifest), *entries.items()]:
                info = zipfile.ZipInfo(name, (2020, 1, 1, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED
                info.external_attr = (stat.S_IFREG | 0o644) << 16
                archive.writestr(info, data)
    inspect_package(output)
    return m
