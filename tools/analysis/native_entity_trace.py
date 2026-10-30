"""Bounded, read-only snapshots of the US entity update list.

Reads are supplied by the owned-child runner. A live list can change between
reads; report incomplete snapshots instead of treating them as authoritative.
Entity addresses are recycled. Type, tag and model definition help distinguish
objects, but do not establish identity across deletion/recreation.
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

import struct


def snapshot_entities(word, read, limit=2048):
    rows = []
    seen = set()
    address = word(0x7b4cc0)
    while address:
        if address in seen:
            return dict(entities=rows, complete=False, reason='cycle')
        if len(rows) >= limit:
            return dict(entities=rows, complete=False, reason='limit')
        if not 0x80000000 <= address <= 0x84000000-0x228:
            return dict(entities=rows, complete=False, reason='address')
        seen.add(address)
        data = read(address, 0x228)
        if data is None or len(data) != 0x228:
            return dict(entities=rows, complete=False, reason='unreadable')
        u = lambda offset: struct.unpack_from('<I', data, offset)[0]
        f = lambda offset, count: list(struct.unpack_from('<'+'f'*count, data, offset))
        row = dict(entity=address, model=u(4), header=list(struct.unpack_from('<16I',data)),
                   type=u(0x20), tag=u(0x60),
                   position=f(0x64,3), previous_position=f(0xec,3),
                   velocity=f(0xf8,3), callback=u(0x16c),
                   update_callback=u(0x164), parent=u(0x78),
                   angles=f(0xd0,4), rocking=f(0x220,2))
        if 0x80000000 <= row['model'] <= 0x84000000-0x140:
            model = read(row['model'], 0x140)
            if model is not None and len(model) == 0x140:
                row['animation'] = dict(
                    header=list(struct.unpack_from('<16I',model)),
                    definition=struct.unpack_from('<I',model,4)[0],
                    clip=struct.unpack_from('<I',model,0xd4)[0],
                    time=list(struct.unpack_from('<2f',model,0xdc)),
                    speed=struct.unpack_from('<f',model,0x108)[0])
        rows.append(row)
        address = u(0)
    return dict(entities=rows, complete=True)
