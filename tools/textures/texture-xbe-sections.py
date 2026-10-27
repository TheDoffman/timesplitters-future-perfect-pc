"""Verify the five disc-read error textures in the pinned US executable.

The language dispatch copies a named section and passes it to the already
verified texture wrapper. No other header-shaped executable data is promoted.
"""

# Direct-command imports share the repository's grouped helper paths.
import sys as _layout_sys
from pathlib import Path as _LayoutPath
_layout_tools = _LayoutPath(__file__).resolve().parents[1]
_layout_root = _layout_tools.parent
if (_layout_tools / 'project_paths.py').is_file():
    _layout_sys.path.insert(0, str(_layout_tools))
    from project_paths import configure as _configure_paths, source_file as repo_file
    _configure_paths()
else:
    # Standalone source fixtures retain their original flat-folder behavior.
    _layout_root = _LayoutPath(__file__).resolve().parents[1]
    def repo_file(root, area, name):
        return _LayoutPath(root) / area / name

import hashlib
import importlib.util
from pathlib import Path
import struct

spec = importlib.util.spec_from_file_location('formats', Path(__file__).with_name('texture-formats.py'))
formats = importlib.util.module_from_spec(spec)
spec.loader.exec_module(formats)

LANGUAGES = ('en', 'fr', 'ge', 'it', 'sp')


def sections(data):
    if len(data) < 0x124 or data[:4] != b'XBEH':
        raise ValueError('Invalid XBE header')
    base = struct.unpack_from('<I', data, 0x104)[0]
    count, address = struct.unpack_from('<II', data, 0x11c)
    table = address-base
    if count > 4096 or table < 0 or table+count*56 > len(data):
        raise ValueError('XBE section table out of bounds')
    result = []
    for i in range(count):
        flags, va, virtual_size, raw, size, name_va = struct.unpack_from('<6I', data, table+i*56)
        name_offset = name_va-base
        if raw+size > len(data) or not 0 <= name_offset < len(data):
            raise ValueError('XBE section data or name out of bounds')
        end = data.find(b'\0', name_offset, min(len(data), name_offset+256))
        if end < 0:
            raise ValueError('Unterminated XBE section name')
        name = data[name_offset:end].decode('ascii')
        result.append(dict(name=name, virtual_address=va, virtual_size=virtual_size,
                           offset=raw, length=size, flags=flags))
    return result


def section_payload(data, section):
    pos, length = section['offset'], section['length']
    if pos < 0 or pos+length > len(data) or length != 128+131072:
        raise ValueError('Error texture section length changed')
    payload = data[pos:pos+length]
    if struct.unpack_from('<7I', payload) != (512,512,512,512,0,0,0xffffffff):
        raise ValueError('Error texture header changed')
    if any(payload[28:128]):
        raise ValueError('Error texture header reserved bytes changed')
    return payload


def verified_sections(path):
    mapping = formats.verify_mapping(path)
    data = path.read_bytes()
    table = sections(data)
    def read(va, n):
        for row in table:
            start, size = row['virtual_address'], row['length']
            if start <= va and va+n <= start+size:
                off = row['offset']+va-start
                return data[off:off+n]
        raise ValueError('XBE proof address out of bounds')
    def call(va, target):
        code = read(va,5)
        if code[0] != 0xe8 or va+5+struct.unpack_from('<i',code,1)[0] != target:
            raise ValueError('Error texture call chain changed')
    references = []
    for i, language in enumerate(LANGUAGES):
        va = 0x2367d+7*i
        instruction = read(va,5)
        if instruction[0] != 0x68:
            raise ValueError('Language section dispatch changed')
        name_va = struct.unpack_from('<I',instruction,1)[0]
        name = 'xbreadfail_'+language
        if read(name_va,len(name)+1) != name.encode()+b'\0':
            raise ValueError('Language section name reference changed')
        references.append(dict(section=name, instruction_va=hex(va), name_va=hex(name_va)))
    for va,target in [(0x2369e,0x37c914),(0x236a6,0x37c97b),(0x236ae,0x37c9ba),
                      (0x236bd,0x3e0b0),(0x236dc,0x18a30),(0x18a3f,0x18390)]:
        call(va,target)
    # Section address -> ESI, size -> EDI, allocate aligned copy -> EAX;
    # copy then push EAX as the texture wrapper's source argument.
    proof_bytes = {
        0x236a3:'8bf857', 0x236ab:'578bf0',
        0x236b3:'8bf86a00576880000000',
        0x236c2:'8bcf8bd1c1e9028bf8f3a58bca83e103f3a48d4c2414516a0050',
    }
    for va, encoded in proof_bytes.items():
        expected = bytes.fromhex(encoded)
        if read(va,len(expected)) != expected:
            raise ValueError('Error texture copy proof changed')
    chosen = []
    for language in LANGUAGES:
        matches = [row for row in table if row['name'] == 'xbreadfail_'+language]
        if len(matches) != 1:
            raise ValueError('Missing or duplicate error texture section')
        row = dict(matches[0])
        payload = section_payload(data,row)
        row['sha256'] = hashlib.sha256(payload).hexdigest()
        row['payload'] = payload
        chosen.append(row)
    proof = dict(xbe_sha256=mapping['xbe_sha256'], language_references=references,
                 copy_range='0x236a3..0x236db', texture_call_va='0x236dc',
                 wrapper_va='0x18a30', loader_va='0x18390',
                 scope='Five named disc-read error sections only; no runtime scene validation')
    return chosen, proof
