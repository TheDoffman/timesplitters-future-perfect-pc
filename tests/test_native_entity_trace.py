import importlib.util
from pathlib import Path
import struct

spec=importlib.util.spec_from_file_location(
    'native_entity_trace',Path(__file__).resolve().parents[1]/'tools/native_entity_trace.py')
trace=importlib.util.module_from_spec(spec)
spec.loader.exec_module(trace)


def test_entity_snapshot_and_incomplete_live_lists():
    first,second=0x80001000,0x80002000
    memory={first:bytearray(0x228),second:bytearray(0x228)}
    struct.pack_into('<I',memory[first],0,second)
    struct.pack_into('<3f',memory[first],0x64,1,2,3)
    struct.pack_into('<I',memory[first],0x16c,0xa84f0)
    def word(address):
        assert address==0x7b4cc0
        return first
    def read(address,size):
        assert size==0x228
        return memory.get(address)
    result=trace.snapshot_entities(word,read)
    assert result['complete']
    assert [row['entity'] for row in result['entities']]==[first,second]
    assert result['entities'][0]['position']==[1,2,3]
    assert result['entities'][0]['callback']==0xa84f0
    assert trace.snapshot_entities(word,read,limit=1)['reason']=='limit'
    struct.pack_into('<I',memory[second],0,first)
    assert trace.snapshot_entities(word,read)['reason']=='cycle'
    struct.pack_into('<I',memory[second],0,0x1234)
    assert trace.snapshot_entities(word,read)['reason']=='address'
    del memory[second]
    assert trace.snapshot_entities(word,read)['reason']=='unreadable'


def test_recycled_entity_address_and_optional_animation_reads():
    address, model = 0x80001000, 0x80002000
    entity = bytearray(0x228)
    animation = bytearray(0x140)
    struct.pack_into('<I', entity, 4, model)
    struct.pack_into('<I', entity, 0x20, 8)
    struct.pack_into('<I', entity, 0x60, 0x42474431)
    struct.pack_into('<I', entity, 0x164, 0x216e00)
    struct.pack_into('<I', entity, 0x78, 0x80003000)
    struct.pack_into('<I', animation, 4, 0x80004000)
    struct.pack_into('<I', animation, 0xd4, 2536)
    struct.pack_into('<2f', animation, 0xdc, 1.5, 1.25)
    struct.pack_into('<f', animation, 0x108, 0.5)
    memory = {address: entity, model: animation}
    def read(pointer, size):
        data = memory.get(pointer)
        assert data is None or len(data) == size
        return data
    def snapshot():
        return trace.snapshot_entities(lambda _: address, read)['entities'][0]
    first = snapshot()
    assert (first['type'], first['tag']) == (8, 0x42474431)
    assert first['update_callback'] == 0x216e00
    assert first['parent'] == 0x80003000
    assert first['animation']['definition'] == 0x80004000
    assert first['animation']['clip'] == 2536
    assert first['animation']['time'] == [1.5, 1.25]
    assert first['animation']['speed'] == 0.5
    # The same pool slot can later contain an unrelated object.
    struct.pack_into('<I', entity, 0x20, 1)
    struct.pack_into('<I', entity, 0x60, 0)
    struct.pack_into('<I', animation, 4, 0x80005000)
    second = snapshot()
    assert second['entity'] == first['entity']
    assert second['type'] != first['type']
    assert second['animation']['definition'] != first['animation']['definition']
    del memory[model]
    assert 'animation' not in snapshot()
