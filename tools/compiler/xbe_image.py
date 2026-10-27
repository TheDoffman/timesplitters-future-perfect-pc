"""Project-owned, bounded XBE reader; see docs/development/independent-runtime.md for sources.

The public attribute/JSON vocabulary is retained for the current generation
consumers. This module imports no runtime-toolkit code. Certificate secrets and
the executable signature are deliberately excluded from the analysis model.
"""
from __future__ import annotations

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


import argparse
from dataclasses import dataclass
from datetime import datetime, timezone
import json
from pathlib import Path
import re
import struct
from types import SimpleNamespace

SECTION_WRITABLE = 1
SECTION_EXECUTABLE = 4
KEYS = {'retail': (0xA8FC57AB, 0x5B6D40B6),
        'debug': (0x94859D4B, 0xEFB1F152)}


class XBEError(ValueError):
    """The file cannot safely describe a supported Xbox executable."""


@dataclass(frozen=True)
class Section:
    name: str
    flags: int
    virtual_addr: int
    virtual_size: int
    raw_addr: int
    raw_size: int


def format_flags(flags):
    return ', '.join(name for bit, name in ((1, 'W'), (2, 'PRE'), (4, 'X'),
                    (8, 'INS'), (16, 'HRO'), (32, 'TRO')) if flags & bit) or 'none'


def kernel_names():
    """Read the separately credited CC0 nxdk export declarations, offline."""
    declarations = Path(__file__).with_name('xbe_kernel_exports.def').read_text('utf-8')
    return {int(ordinal): name for name, ordinal in re.findall(
        r'^\s*@?(\w+)(?:@\d+)?\s+@\s*(\d+)\b', declarations, re.M)}


class XBEParser:
    MAX_FILE = 256 * 1024 * 1024

    def __init__(self, path):
        self.path = Path(path)
        with self.path.open('rb') as stream:
            self.data = stream.read(self.MAX_FILE + 1)
        if len(self.data) > self.MAX_FILE:
            raise XBEError('XBE exceeds the 256 MiB reader limit')
        self.sections = []
        self.header = None

    def block(self, offset, size):
        if offset < 0 or size < 0 or offset + size > len(self.data):
            raise XBEError(f'File extent outside XBE: {offset:#x}+{size:#x}')
        return self.data[offset:offset + size]

    def words(self, offset, count=1):
        return struct.unpack('<' + 'I' * count, self.block(offset, count * 4))

    def va_to_offset(self, address, size=1):
        """Resolve only file-backed bytes; zero-filled virtual tails are not data."""
        h = self.header
        if h.base_address <= address and address + size <= h.base_address + h.header_size:
            return address - h.base_address
        for section in self.sections:
            delta = address - section.virtual_addr
            if 0 <= delta and delta + size <= section.raw_size:
                return section.raw_addr + delta
        raise XBEError(f'Unmapped file-backed address: {address:#x}+{size:#x}')

    def at(self, address, size):
        return self.block(self.va_to_offset(address, size), size)

    def string(self, address):
        if not address:
            return ''
        result = bytearray()
        for index in range(4096):
            value = self.at(address + index, 1)[0]
            if not value:
                return result.decode('ascii', errors='strict')
            result.append(value)
        raise XBEError('Unterminated XBE string')

    def parse(self):
        self.sections = []
        if self.block(0, 4) != b'XBEH':
            raise XBEError('Expected XBEH executable magic')
        fields = ('base_address header_size image_size image_header_size timestamp '
                  'certificate_addr num_sections section_headers_addr init_flags '
                  'entry_point_encoded tls_addr pe_stack_commit pe_heap_reserve '
                  'pe_heap_commit pe_base_address pe_image_size pe_checksum pe_timestamp '
                  'debug_pathname_addr debug_filename_addr debug_unicode_filename_addr '
                  'kernel_thunk_addr_encoded non_kernel_import_dir_addr '
                  'num_library_versions library_versions_addr kernel_library_version_addr '
                  'xapi_library_version_addr logo_bitmap_addr logo_bitmap_size').split()
        h = self.header = SimpleNamespace(**dict(zip(fields, self.words(0x104, len(fields)))))
        if not 0x178 <= h.image_header_size <= h.header_size <= h.image_size:
            raise XBEError('Invalid XBE header/image sizes')
        if h.base_address + h.image_size > 0x100000000:
            raise XBEError('XBE image overflows the 32-bit address space')
        self.block(0, h.header_size)
        if not 1 <= h.num_sections <= 4096:
            raise XBEError('Invalid section count')
        table = self.at(h.section_headers_addr, h.num_sections * 56)
        pending = []
        for index in range(h.num_sections):
            flags, va, size, raw, length, name = struct.unpack_from('<6I', table, index * 56)
            end = va + max(size, length)
            if va < h.base_address + h.header_size or end > h.base_address + h.image_size:
                raise XBEError('Section outside the image or overlapping headers')
            if length:
                if raw < h.header_size:
                    raise XBEError('Section payload overlaps headers')
                self.block(raw, length)
            pending.append((name, flags, va, size, raw, length))
        for axis, extent in ((2, lambda s: max(s[3], s[5])), (4, lambda s: s[5])):
            spans = sorted((s[axis], s[axis] + extent(s)) for s in pending if extent(s))
            if any(a[1] > b[0] for a, b in zip(spans, spans[1:])):
                raise XBEError('Overlapping section extents')
        # Names may reside in file-backed sections as well as the headers.
        self.sections = [Section('', *s[1:]) for s in pending]
        self.sections = [Section(self.string(s[0]), *s[1:]) for s in pending]
        candidates = []
        for kind, (entry_key, thunk_key) in KEYS.items():
            entry, thunk = h.entry_point_encoded ^ entry_key, h.kernel_thunk_addr_encoded ^ thunk_key
            if any(s.flags & SECTION_EXECUTABLE and s.virtual_addr <= entry <
                   s.virtual_addr + min(s.raw_size, s.virtual_size) for s in self.sections):
                try:
                    if thunk:
                        self.at(thunk, 4)
                    candidates.append((kind, entry, thunk))
                except XBEError:
                    pass
        if len(candidates) != 1:
            raise XBEError('No unique retail/debug entry and kernel-thunk decoding')
        kind, h.entry_point, h.kernel_thunk_addr = candidates[0]
        h.is_debug = kind == 'debug'
        cert_size = struct.unpack('<I', self.at(h.certificate_addr, 4))[0]
        if not 0x1D0 <= cert_size <= 4096:
            raise XBEError('Invalid certificate size')
        certificate = self.at(h.certificate_addr, cert_size)
        size, stamp, title_id = struct.unpack_from('<3I', certificate)
        allowed, region, ratings, disc, version = struct.unpack_from('<5I', certificate, 0x9C)
        cert = SimpleNamespace(size=size, timestamp=stamp, title_id=title_id,
            title_name=certificate[12:92].decode('utf-16-le').split('\0', 1)[0],
            alt_title_ids=list(struct.unpack_from('<16I', certificate, 0x5C)),
            allowed_media=allowed, game_region=region, game_ratings=ratings,
            disc_number=disc, version=version)
        if h.num_library_versions > 4096:
            raise XBEError('Invalid library count')
        libraries = []
        if h.num_library_versions:
            table = self.at(h.library_versions_addr, h.num_library_versions * 16)
            for index in range(h.num_library_versions):
                name, major, minor, build, flags = struct.unpack_from('<8s4H', table, index * 16)
                libraries.append(SimpleNamespace(name=name.split(b'\0', 1)[0].decode('ascii'),
                                 major=major, minor=minor, build=build, flags=flags))
        imports = []
        names = kernel_names()
        if h.kernel_thunk_addr:
            for index in range(4096):
                address = h.kernel_thunk_addr + index * 4
                value = struct.unpack('<I', self.at(address, 4))[0]
                if not value:
                    break
                if value & 0x80000000 == 0 or not value & 0x7FFFFFFF:
                    raise XBEError('Invalid kernel import ordinal')
                ordinal = value & 0x7FFFFFFF
                imports.append(SimpleNamespace(ordinal=ordinal,
                    name=names.get(ordinal, f'Unknown_{ordinal}'), thunk_addr=address))
            else:
                raise XBEError('Unterminated kernel import table')
        tls = None
        if h.tls_addr:
            fields = 'raw_data_start raw_data_end tls_index_addr callbacks_addr zero_fill_size characteristics'.split()
            tls = SimpleNamespace(**dict(zip(fields, struct.unpack('<6I', self.at(h.tls_addr, 24)))))
            if tls.raw_data_end < tls.raw_data_start:
                raise XBEError('Reversed TLS initialization range')
            if tls.raw_data_end > tls.raw_data_start:
                self.at(tls.raw_data_start, tls.raw_data_end - tls.raw_data_start)
        return SimpleNamespace(header=h, certificate=cert, sections=self.sections,
            libraries=libraries, kernel_imports=imports, tls=tls,
            debug_pathname=self.string(h.debug_pathname_addr),
            debug_filename=self.string(h.debug_filename_addr), raw_data=self.data)


def analysis(xbe):
    """Stable interchange schema consumed by disassembly and code generation."""
    h, c = xbe.header, xbe.certificate
    hex32 = lambda value: f'0x{value:08X}'
    return dict(title=c.title_name, title_id=hex32(c.title_id),
        type='debug' if h.is_debug else 'retail',
        build_date=datetime.fromtimestamp(h.timestamp, timezone.utc).strftime('%Y-%m-%d %H:%M:%S UTC'),
        xdk_version=xbe.libraries[0].build if xbe.libraries else 0,
        build_path=xbe.debug_pathname, base_address=hex32(h.base_address), image_size=h.image_size,
        entry_point=hex32(h.entry_point), kernel_thunk_addr=hex32(h.kernel_thunk_addr),
        sections=[dict(name=s.name, virtual_addr=hex32(s.virtual_addr), virtual_size=s.virtual_size,
            raw_addr=hex32(s.raw_addr), raw_size=s.raw_size, flags=format_flags(s.flags),
            writable=bool(s.flags & 1), executable=bool(s.flags & 4)) for s in xbe.sections],
        libraries=[dict(name=v.name, version=f'{v.major}.{v.minor}.{v.build}',
            flags=f'0x{v.flags:04X}') for v in xbe.libraries],
        kernel_imports=[dict(ordinal=i.ordinal, name=i.name, thunk_addr=hex32(i.thunk_addr))
                        for i in xbe.kernel_imports],
        tls=dict(index_addr=hex32(xbe.tls.tls_index_addr), zero_fill_size=xbe.tls.zero_fill_size)
            if xbe.tls else None)


def export_json(xbe, output_path):
    Path(output_path).write_text(json.dumps(analysis(xbe), indent=2) + '\n', encoding='utf-8')


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument('xbe_path', type=Path)
    cli.add_argument('--json', type=Path)
    cli.add_argument('--quiet', action='store_true')
    args = cli.parse_args()
    try:
        xbe = XBEParser(args.xbe_path).parse()
        if args.json:
            export_json(xbe, args.json)
        if not args.quiet:
            print(f'{xbe.certificate.title_name}: {len(xbe.sections)} sections, '
                  f'{len(xbe.kernel_imports)} kernel imports; project-owned XBE reader')
    except (OSError, ValueError) as error:
        cli.exit(1, f'XBE analysis failed: {error}\n')


if __name__ == '__main__':
    main()
