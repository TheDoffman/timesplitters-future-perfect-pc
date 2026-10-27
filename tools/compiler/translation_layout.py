"""Project executable layout for translation, with no captured-game fallback.

Queries are empty until a caller installs a layout. Each replacement is validated
before publication, so failed parses and malformed layouts preserve prior state.
Compatibility globals describe the same snapshot as the address helpers.
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

from dataclasses import dataclass

from xbe_image import XBEParser, SECTION_EXECUTABLE

FORCE_CODE_SECTIONS = frozenset(('XIPS',))
FORCE_DATA_SECTIONS = frozenset(('.data', '.data1', '.rdata', 'EnglishXlate',
    'JapaneseXlate', 'GermanXlate', 'FrenchXlate', 'SpanishXlate', 'ItalianXlate'))


@dataclass(frozen=True)
class Section:
    name: str
    va: int
    va_size: int
    raw_addr: int
    raw_size: int
    is_code: bool


_SECTIONS = ()
SECTIONS = []
_configured_from = None
TEXT_VA_START = TEXT_VA_END = 0
RDATA_VA_START = RDATA_VA_END = 0
DATA_VA_START = DATA_VA_END = 0
KERNEL_THUNK_ADDR = ENTRY_POINT = 0
GAME_NAME = 'Xbox Game'
_game_name_explicit = False


def _classify(name, flags):
    return name not in FORCE_DATA_SECTIONS and (name in FORCE_CODE_SECTIONS or bool(flags & SECTION_EXECUTABLE))


def _unsigned(value, limit=2**32):
    return type(value) is int and 0 <= value < limit


def _install(sections, entry_point, kernel_thunk_addr, origin):
    records = tuple(sorted(sections, key=lambda section: section.va))
    if not records:
        raise ValueError('Translation requires at least one section')
    for section in records:
        if (not isinstance(section.name, str) or not section.name or
            not all(_unsigned(value) for value in (section.va, section.raw_addr)) or
            not all(_unsigned(value, 2**32 + 1) for value in (section.va_size, section.raw_size)) or
            section.va + section.va_size > 2**32 or section.raw_addr + section.raw_size > 2**32):
            raise ValueError('Invalid translation section extent')
    spans = [(section.va, section.va + section.va_size) for section in records if section.va_size]
    if any(left[1] > right[0] for left, right in zip(spans, spans[1:])):
        raise ValueError('Overlapping translation sections')
    if not _unsigned(entry_point) or not _unsigned(kernel_thunk_addr):
        raise ValueError('Invalid translation entry or thunk address')

    def named(name):
        return next((section for section in records if section.name == name), None)

    text = named('.text') or next((section for section in records if section.is_code), None)
    data, readonly = named('.data'), named('.rdata')
    snapshot = dict(_SECTIONS=records, SECTIONS=[(s.name, s.va, s.va_size, s.raw_addr) for s in records],
                    _configured_from=origin, ENTRY_POINT=entry_point, KERNEL_THUNK_ADDR=kernel_thunk_addr)
    for prefix, section in (('TEXT', text), ('DATA', data or readonly), ('RDATA', readonly or data)):
        snapshot[prefix + '_VA_START'] = section.va if section else 0
        snapshot[prefix + '_VA_END'] = section.va + section.va_size if section else 0
    globals().update(snapshot)


def configure_from_xbe(xbe_path):
    image = XBEParser(xbe_path).parse()
    _install([Section(s.name, s.virtual_addr, s.virtual_size, s.raw_addr, s.raw_size,
                      _classify(s.name, s.flags)) for s in image.sections],
             image.header.entry_point, image.header.kernel_thunk_addr, str(xbe_path))


def configured_from():
    return _configured_from


def _section_at(address):
    if not _unsigned(address):
        return None
    return next((section for section in _SECTIONS if section.va <= address < section.va + section.va_size), None)


def va_to_file_offset(address):
    section = _section_at(address)
    if section is None or address - section.va >= section.raw_size:
        return None
    return section.raw_addr + address - section.va


def is_code_address(address):
    section = _section_at(address)
    return section is not None and section.is_code


def is_data_address(address):
    section = _section_at(address)
    return section is not None and not section.is_code


def set_game_name(name):
    global GAME_NAME, _game_name_explicit
    if name:
        GAME_NAME, _game_name_explicit = name, True


def banner_name(title=None):
    return GAME_NAME if _game_name_explicit else title or GAME_NAME
