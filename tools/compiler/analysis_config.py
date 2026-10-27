"""Compatibility layout settings derived from the selected executable.

No title's addresses are used as fallback facts. Consumers must configure a
binary before using its layout; the standalone runner derives its own view.
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

from recomp_image import load_image, DATA_SECTION_NAMES

DEFAULT_OUTPUT_DIR = 'tools/disasm/output'
CC_PADDING = 0xCC
CACHE_FILENAME = '.disasm_cache.json'
CACHE_VERSION = 2
MIN_STRING_LENGTH, MAX_STRING_LENGTH = 4, 256
XBE_BASE_ADDRESS = XBE_IMAGE_SIZE = ENTRY_POINT = KERNEL_THUNK_ADDR = None
EXECUTABLE_SECTIONS, DATA_SECTIONS, ALL_SECTIONS = [], [], {}
_origin = None


def configured_from():
    return _origin


def configure_from_xbe(xbe_path):
    image = load_image(xbe_path)
    sections = [section for section in image.sections if section.raw_size]
    code = [(section.name, section.virtual_addr, section.virtual_size) for section in sections
            if section.executable and section.name not in DATA_SECTION_NAMES]
    data = [(section.name, section.virtual_addr, section.virtual_size) for section in sections
            if not section.executable or section.name in DATA_SECTION_NAMES]
    globals().update(XBE_BASE_ADDRESS=image.base_address, XBE_IMAGE_SIZE=image.image_size,
                     ENTRY_POINT=image.entry_point, KERNEL_THUNK_ADDR=image.kernel_thunk_addr,
                     EXECUTABLE_SECTIONS=code, DATA_SECTIONS=data,
                     ALL_SECTIONS={section.name: dict(va=section.virtual_addr, size=section.virtual_size)
                                   for section in sections}, _origin=str(xbe_path))
