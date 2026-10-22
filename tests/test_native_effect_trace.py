import importlib.util
from pathlib import Path
import struct

import pytest

spec = importlib.util.spec_from_file_location(
    'native_effect_trace', Path(__file__).resolve().parents[1]/'tools/native_effect_trace.py')
trace = importlib.util.module_from_spec(spec)
spec.loader.exec_module(trace)


def scene():
    effect, payload, player = 0x80001000, 0x80002000, 0x80004000
    heads, header, beam = bytearray(160), bytearray(0x68), bytearray(0x994)
    struct.pack_into('<I', heads, 32*4, effect)
    struct.pack_into('<I', header, 0, 32)
    struct.pack_into('<I', header, 8, 0x90001)
    struct.pack_into('<2f', header, 0xc, .125, 1.2)
    struct.pack_into('<I', header, 0x18, payload)
    struct.pack_into('<3f', header, 0x20, 10, 20, 30)
    struct.pack_into('<I', header, 0x34, 1)
    struct.pack_into('<I', beam, 0, 4)
    struct.pack_into('<2I', beam, 0x98c, 0xff0080ff, 0xff5522ff)
    for i in range(1, 4):
        struct.pack_into('<f', beam, i*0x1e0+0x1d8, i*.125)
    memory = {trace.HEADS: heads, effect: header, payload: beam,
              0x7b0cc0: struct.pack('<I', player),
              player+0x214: struct.pack('<I', 0)}

    def read(address, size):
        data = memory.get(address)
        assert data is None or len(data) == size
        return data
    return memory, read, effect, payload


def test_effect_snapshot_age_visibility_and_payload():
    memory, read, effect, _ = scene()
    result = trace.snapshot_effects(read)
    assert result['complete'] and not result['atomic']
    assert result['player_index'] == 0
    row = result['effects'][0]
    assert row['age'] == .125 and row['lifetime'] == pytest.approx(1.2)
    assert row['position'] == [10, 20, 30] and row['beam_render_gate']
    assert row['beam']['colors'] == [0xff0080ff, 0xff5522ff]
    assert row['beam']['strand_delays'] == [.125, .25, .375]
    header = memory[effect]
    # Keep each rejection cause independent: hidden, unprepared, wrong view,
    # no visible room, or a deferred start.
    for flags, mask, delay in [(0x290001, 1, 0), (0x80001, 1, 0),
                               (0x90001, 2, 0), (0x90000, 1, 0),
                               (0x90001, 1, .3)]:
        struct.pack_into('<I', header, 8, flags)
        struct.pack_into('<I', header, 0x34, mask)
        struct.pack_into('<f', header, 0x38, delay)
        assert not trace.snapshot_effects(read)['effects'][0]['beam_render_gate']


def test_live_list_faults_and_global_budget():
    memory, read, effect, _ = scene()
    header = memory[effect]
    struct.pack_into('<I', header, 0x64, effect)
    assert trace.snapshot_effects(read)['reason'] == 'cycle_or_shared_node'
    second = effect+0x100
    struct.pack_into('<I', header, 0x64, second)
    memory[second] = bytearray(header)
    struct.pack_into('<I', memory[second], 0x64, 0)
    assert trace.snapshot_effects(read, limit=1)['reason'] == 'limit'
    assert len(trace.snapshot_effects(read)['effects']) == 2
    struct.pack_into('<I', header, 0x64, 0x1234)
    assert trace.snapshot_effects(read)['reason'] == 'address'
    struct.pack_into('<I', header, 0x64, 0)
    struct.pack_into('<I', header, 0, 31)
    assert trace.snapshot_effects(read)['reason'] == 'type_changed'
    del memory[effect]
    assert trace.snapshot_effects(read)['reason'] == 'unreadable'
    del memory[trace.HEADS]
    assert trace.snapshot_effects(read)['reason'] == 'heads_unreadable'
    with pytest.raises(ValueError):
        trace.snapshot_effects(read, limit=0)


def test_changed_heads_short_reads_and_unavailable_optional_fields():
    memory, read, effect, payload = scene()
    del memory[payload]
    memory[0x7b0cc0] = b'\0\0'
    struct.pack_into('<f', memory[effect], 0xc, float('nan'))
    result = trace.snapshot_effects(read=lambda a, n: memory.get(a))
    row = result['effects'][0]
    assert result['complete'] and result['player_index'] is None
    assert row['age'] is None and row['beam_render_gate'] is None
    assert row['payload_unreadable']
    calls = 0
    def changed(address, size):
        nonlocal calls
        if address == trace.HEADS:
            calls += 1
            if calls > 1:
                return bytes(160)
        return memory.get(address)
    assert trace.snapshot_effects(changed)['reason'] == 'heads_changed'
    def inaccessible(address, size):
        raise OSError('process exited')
    assert trace.snapshot_effects(inaccessible)['reason'] == 'heads_unreadable'
