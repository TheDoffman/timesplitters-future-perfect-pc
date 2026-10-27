"""Bounded, read-only snapshots of the supported US effect lists.

Completeness means traversal succeeded, not that concurrent guest state was
read atomically. Pool addresses are recycled; an address alone is not identity.
Render eligibility describes the original type-32 dispatch gate, not a GPU draw.
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

import math
import struct

HEADS = 0x73BB08
KINDS = 40


def snapshot_effects(read, limit=256):
    if not 1 <= limit <= 2048:
        raise ValueError('Effect limit must be between 1 and 2048')

    def fetch(address, size):
        try:
            data = read(address, size)
        except (OSError, RuntimeError, ValueError):
            return None
        return data if data is not None and len(data) == size else None

    def guest_pointer(address, size):
        return 0x80000000 <= address <= 0x84000000-size

    def number(data, offset):
        value = struct.unpack_from('<f', data, offset)[0]
        return value if math.isfinite(value) else None

    rows = []
    result = dict(effects=rows, complete=False, atomic=False, player_index=None)
    heads = fetch(HEADS, KINDS*4)
    if heads is None:
        return dict(result, reason='heads_unreadable')
    result['heads'] = list(struct.unpack('<40I', heads))
    player = fetch(0x7B0CC0, 4)
    if player is not None:
        player = struct.unpack('<I', player)[0]
        if guest_pointer(player, 0x218):
            index = fetch(player+0x214, 4)
            if index is not None:
                index = struct.unpack('<I', index)[0]
                if index < 16:
                    result['player_index'] = index
    seen = set()
    for kind, address in enumerate(result['heads']):
        while address:
            if address in seen:
                return dict(result, reason='cycle_or_shared_node', address=address)
            if len(rows) >= limit:
                return dict(result, reason='limit')
            if not guest_pointer(address, 0x68):
                return dict(result, reason='address', address=address)
            seen.add(address)
            data = fetch(address, 0x68)
            if data is None:
                return dict(result, reason='unreadable', address=address)
            u = lambda offset: struct.unpack_from('<I', data, offset)[0]
            if u(0) != kind:
                return dict(result, reason='type_changed', address=address)
            row = dict(address=address, kind=kind, flags=u(8),
                       age=number(data, 0xc), lifetime=number(data, 0x10),
                       delay=number(data, 0x38), view_mask=u(0x34),
                       position=[number(data, 0x20+i*4) for i in range(3)],
                       payload=u(0x18))
            if kind == 32:
                index = result['player_index']
                flags = row['flags']
                row['beam_render_gate'] = None if index is None else bool(
                    flags & 0x10000 and row['delay'] == 0 and
                    not flags & 0x200000 and row['view_mask'] & (1 << index) and
                    (not flags & 0x80000 or flags & (1 << index)))
                payload = fetch(row['payload'], 0x994) if guest_pointer(row['payload'], 0x994) else None
                if payload is not None:
                    count = struct.unpack_from('<I', payload)[0]
                    row['beam'] = dict(
                        strands=count,
                        colors=list(struct.unpack_from('<2I', payload, 0x98c)),
                        endpoint=[number(payload, 0x964+i*4) for i in range(3)],
                        strand_delays=[number(payload, i*0x1e0+0x1d8)
                                       for i in range(1, min(count, 5))])
                else:
                    row['payload_unreadable'] = True
            rows.append(row)
            address = u(0x64)
    if fetch(HEADS, KINDS*4) != heads:
        return dict(result, reason='heads_changed')
    return dict(result, complete=True)
