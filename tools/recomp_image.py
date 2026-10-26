"""Read-only executable view for code analysis, backed by the project XBE reader.

Addresses are Xbox virtual addresses, never host pointers. Exact reads require
one fully file-backed region; code probes instead request a bounded window.
The vocabulary exposed here is the interchange contract of existing consumers.
"""
from dataclasses import dataclass, field
import hashlib
import json
from pathlib import Path

from xbe_image import XBEParser, XBEError, analysis, format_flags

DATA_SECTION_NAMES = frozenset(('.data', '.data1', '.rdata', '.idata', '.edata', '.reloc', '.tls'))


@dataclass(frozen=True)
class SectionInfo:
    name: str
    virtual_addr: int
    virtual_size: int
    raw_addr: int
    raw_size: int
    writable: bool
    executable: bool
    flags: str


@dataclass(frozen=True)
class KernelImport:
    ordinal: int
    name: str
    thunk_addr: int


@dataclass
class BinaryImage:
    filepath: str
    raw_data: bytes
    base_address: int
    image_size: int
    entry_point: int
    kernel_thunk_addr: int
    sections: list[SectionInfo] = field(default_factory=list)
    kernel_imports: list[KernelImport] = field(default_factory=list)
    header_size: int = 0

    def __post_init__(self):
        if not 0 <= self.base_address < 2**32 or not 0 <= self.image_size <= 2**32 - self.base_address:
            raise XBEError('Invalid analysis image range')
        if not 0 <= self.header_size <= min(len(self.raw_data), self.image_size):
            raise XBEError('Invalid analysis header extent')
        for section in self.sections:
            if (min(section.virtual_size, section.raw_addr, section.raw_size) < 0 or
                section.raw_addr + section.raw_size > len(self.raw_data) or
                not self.base_address <= section.virtual_addr <= self.base_address + self.image_size or
                section.virtual_addr + section.virtual_size > self.base_address + self.image_size):
                raise XBEError('Invalid analysis section extent')
        spans = sorted((s.virtual_addr, s.virtual_addr + s.virtual_size)
                       for s in self.sections if s.virtual_size)
        if (spans and spans[0][0] < self.base_address + self.header_size) or any(
                first[1] > second[0] for first, second in zip(spans, spans[1:])):
            raise XBEError('Overlapping analysis regions')

    def get_section(self, name):
        return next((s for s in self.sections if s.name == name), None)

    def get_section_at_va(self, address):
        return next((s for s in self.sections if
                     s.virtual_addr <= address < s.virtual_addr + s.virtual_size), None)

    def _backing(self, address):
        """Return file offset and available byte count, excluding virtual zero fill."""
        section = self.get_section_at_va(address)
        if section is not None:
            relative = address - section.virtual_addr
            count = min(section.virtual_size, section.raw_size) - relative
            if count > 0:
                return section.raw_addr + relative, count
        elif self.base_address <= address < self.base_address + self.header_size:
            relative = address - self.base_address
            return relative, self.header_size - relative
        return None

    def va_to_file_offset(self, address):
        backing = self._backing(address)
        return backing[0] if backing is not None else None

    def read_bytes_at_va(self, address, size):
        if size < 0:
            raise ValueError('Read size must be nonnegative')
        backing = self._backing(address)
        if backing is None or size > backing[1]:
            return None
        offset = backing[0]
        return self.raw_data[offset:offset + size]

    def read_u32_at_va(self, address):
        raw = self.read_bytes_at_va(address, 4)
        return int.from_bytes(raw, 'little') if raw is not None else None

    def get_section_data(self, section):
        if section not in self.sections:
            raise ValueError('Section does not belong to this image')
        return self.raw_data[section.raw_addr:section.raw_addr + section.raw_size]

    def get_kernel_import_at_thunk(self, address):
        return next((item for item in self.kernel_imports if item.thunk_addr == address), None)

    def get_executable_sections(self):
        return [s for s in self.sections if s.executable]

    def get_code_sections(self):
        # Xbox images can flag conventional data sections executable as well.
        return [s for s in self.sections if s.executable and s.raw_size and s.name not in DATA_SECTION_NAMES]


def read_code_window(image, address, maximum):
    """Read at most maximum bytes within one executable section for a probe.

    Uses the public section view so small synthetic consumer fixtures work too.
    Unlike an exact data read, approaching a section boundary returns a short
    window. A probe must not decode padding from an unrelated file region.
    """
    if maximum < 0:
        raise ValueError('Probe size must be nonnegative')
    section = image.get_section_at_va(address)
    if section is None or not section.executable:
        return b''
    if isinstance(image, BinaryImage):
        backing = image._backing(address)
        if backing is None:
            return b''
        offset, available = backing
        return image.raw_data[offset:offset + min(maximum, available)]
    maximum = min(maximum, section.virtual_addr + section.virtual_size - address)
    reader = getattr(image, 'read_bytes_at_va', None)
    if reader is not None:
        return reader(address, maximum) or b''
    raw = image.get_section_data(section)
    offset = address - section.virtual_addr
    limit = min(len(raw), section.virtual_size, offset + maximum)
    return raw[offset:limit]


def _find_analysis_json(xbe_path):
    """Only the matching sibling can be implicit; unrelated JSON is never guessed."""
    path = Path(xbe_path)
    candidate = path.with_name(path.stem + '_analysis.json')
    return candidate if candidate.is_file() else None


def load_image(xbe_path, analysis_json=None):
    """Parse the binary itself; an optional analysis file must describe that image."""
    path = Path(xbe_path).resolve()
    parsed = XBEParser(path).parse()
    metadata = Path(analysis_json) if analysis_json is not None else _find_analysis_json(path)
    if metadata is not None:
        if metadata.stat().st_size > 8 * 1024 * 1024:
            raise XBEError('Analysis JSON exceeds 8 MiB')
        supplied = json.loads(metadata.read_text(encoding='utf-8'))
        expected = analysis(parsed)
        # Descriptive dates/paths and extra diagnostic fields are not layout inputs.
        for key in ('title_id', 'base_address', 'image_size', 'entry_point',
                    'kernel_thunk_addr', 'sections', 'kernel_imports', 'tls'):
            if not isinstance(supplied, dict) or supplied.get(key) != expected[key]:
                raise XBEError(f'Analysis does not match executable ({key}); regenerate it')
    header = parsed.header
    sections = [SectionInfo(s.name, s.virtual_addr, s.virtual_size, s.raw_addr,
                           s.raw_size, bool(s.flags & 1), bool(s.flags & 4), format_flags(s.flags))
                for s in parsed.sections]
    imports = [KernelImport(i.ordinal, i.name, i.thunk_addr) for i in parsed.kernel_imports]
    return BinaryImage(str(path), parsed.raw_data, header.base_address, header.image_size,
                       header.entry_point, header.kernel_thunk_addr, sections, imports, header.header_size)


def source_fingerprint():
    """Include independent dependencies in the existing disassembly cache key."""
    names = ('recomp_image.py', 'x86_decode.py', 'x86_flow.py', 'xbe_image.py', 'xbe_kernel_exports.def')
    result = hashlib.sha256()
    for name in names:
        result.update(name.encode('ascii') + b'\0')
        result.update(hashlib.sha256(Path(__file__).with_name(name).read_bytes()).digest())
    return result.hexdigest()
