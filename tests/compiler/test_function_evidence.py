"""Original programs distinguishing pointer evidence from coincidental bytes."""


from pathlib import Path
from dataclasses import replace
import struct
import sys

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from function_evidence import (FunctionRanges, collect_operands, find_pointer_tables,
                               scalar_initializers, entry_blocks, numeric_observations)
from recomp_image import BinaryImage, SectionInfo


def fixture(code, data=b'', data_va=0x4000):
    return BinaryImage('fixture', code + data, 0x1000, 0x5000, 0x1000, 0,
        [SectionInfo('.text', 0x1000, len(code), 0, len(code), False, True, ''),
         SectionInfo('.rdata', data_va, len(data), len(code), len(data), False, False, '')])


def function(start, end):
    return dict(start=hex(start), end=hex(end))


def test_owner_bounds_gaps_and_nested_aliases():
    ranges = FunctionRanges([function(0x1000, 0x1040), function(0x1010, 0x1014), function(0x1080, 0x1090)])
    assert ranges.containing(0x1011) == 0x1010
    assert ranges.containing(0x1014) == 0x1000
    assert ranges.containing(0x1040) is None and ranges.containing(0x107F) is None
    assert ranges.containing(0x1080) == 0x1080
    top = FunctionRanges([function(0xFFFFFFFE, 0x100000000)])
    assert top.containing(0xFFFFFFFF) == 0xFFFFFFFE


def test_entry_measurements_stop_at_branch_and_never_guess_a_function():
    image = fixture(bytes.fromhex('85c0740231c0c3'))
    tables = [dict(entries=[0x1000, 0x1004, 0x1000])]
    records = entry_blocks(image, tables, [])
    assert records == [dict(start=0x1000, end=0x1004, instructions=2, boundary='je', scope='entry_block'),
                       dict(start=0x1004, end=0x1007, instructions=2, boundary='ret', scope='entry_block')]
    assert entry_blocks(image, tables, [], maximum=1) == []
    assert len(entry_blocks(image, tables, [function(0x1000, 0x1007)])) == 1


def test_numeric_evidence_does_not_turn_cosine_into_integer_conversion():
    code = bytes.fromhex('d9442404d9ffc3')  # fld argument; fcos; ret
    observations = numeric_observations(fixture(code), [function(0x1000, 0x1007)])
    assert observations == {0x1000: dict(operations=['fcos'], scope='instruction_presence')}
    assert not numeric_observations(fixture(code[:-1]), [function(0x1000, 0x1007)])
    code = bytes.fromhex('0fa5c2d3e0c3')
    observations = numeric_observations(fixture(code), [function(0x1000, 0x1006)])
    assert observations[0x1000]['operations'] == ['shld']


def test_narrow_store_is_not_a_constructor_pointer_assignment():
    code = bytes.fromhex('66c7010040c3')
    evidence = collect_operands(fixture(code, b'data'), [function(0x1000, 0x1006)])
    assert not evidence.immediate_stores


@pytest.mark.parametrize('records', [[function(1, 1)], [function(2, 1)],
    [function(1, 2), function(1, 3)], [dict(start=True, end=3)]])
def test_invalid_function_bounds(records):
    with pytest.raises(ValueError):
        FunctionRanges(records)


def test_only_decoded_operands_and_real_stores_are_evidence():
    # mov [ecx],table ; mov edx,[string] ; ret. The second body only loads a
    # table address and must not become a constructor merely for mentioning it.
    code = bytes.fromhex('c701004000008b1504400000c3')
    second = len(code)
    code += bytes.fromhex('b800400000c3')
    image = fixture(code, b'table\0text\0')
    functions = [function(0x1000, 0x1000 + second), function(0x1000 + second, 0x1000 + len(code))]
    evidence = collect_operands(image, functions)
    assert evidence.immediate_stores == {0x4000: [0x1000]}
    assert evidence.data_references == {0x4000: [0x1000, 0x1000 + second], 0x4004: [0x1000]}
    # The bytes 68 00 40 00 00 embedded inside one mov immediate are not push.
    code = bytes.fromhex('b86800400000c3')
    evidence = collect_operands(fixture(code, b'data'), [function(0x1000, 0x1007)])
    assert not evidence.data_references


def test_pointer_table_includes_last_slot_and_constructor_evidence():
    code = bytearray(b'\xCC' * 0x40)
    code[:7] = bytes.fromhex('c70100400000c3')
    code[0x10:0x11] = b'\xc3'
    code[0x20:0x23] = bytes.fromhex('31c0c3')
    image = fixture(bytes(code), struct.pack('<3I', 0x1010, 0x1020, 0x1010))
    functions = [function(0x1000, 0x1007), function(0x1010, 0x1011), function(0x1020, 0x1023)]
    tables = find_pointer_tables(image, functions, collect_operands(image, functions))
    assert tables == [dict(address=0x4000, entries=[0x1010, 0x1020, 0x1010], section='.rdata', constructors=[0x1000])]


def test_repeated_values_and_uncorroborated_arrays_are_not_tables():
    code = b'\xc3' + b'\xCC' * 15 + b'\xc3'
    for words, functions in (([0x1000] * 3, [function(0x1000, 0x1001)]),
                             ([0x1000, 0x1010, 0x1000], [])):
        assert not find_pointer_tables(fixture(code, struct.pack('<3I', *words)), functions)


def test_references_in_gaps_do_not_attach_to_previous_function():
    code = bytes.fromhex('c36800400000c3') + b'\xCC' * 9 + bytes.fromhex('6804400000c3')
    evidence = collect_operands(fixture(code, b'abcdefgh'),
                                [function(0x1000, 0x1001), function(0x1010, 0x1016)])
    assert evidence.data_references == {0x4004: [0x1010]}


def test_instruction_reads_do_not_extend_into_next_raw_region():
    image = fixture(bytes.fromhex('680040'), b'\x00\x00data')
    image = replace(image, sections=[replace(image.sections[0], virtual_size=5), image.sections[1]])
    evidence = collect_operands(image, [function(0x1000, 0x1005)])
    assert not evidence.data_references


def test_pointer_slots_align_to_guest_addresses_and_probe_unknown_entries():
    code = b'\xc3' + b'\xCC' * 15 + bytes.fromhex('31c0c3')
    data = b'pad' + struct.pack('<3I', 0x1000, 0x1010, 0x1000)
    image = fixture(code, data, data_va=0x4001)
    tables = find_pointer_tables(image, [function(0x1000, 0x1001)])
    assert len(tables) == 1 and tables[0]['address'] == 0x4004
    assert tables[0]['entries'] == [0x1000, 0x1010, 0x1000]


def test_scalar_initializer_requires_real_data_read_and_write():
    code = bytes.fromhex('f30f100500400000f30f110504400000c3')
    image = fixture(code, bytes(16))
    functions = [function(0x1000, 0x1000 + len(code))]
    assert scalar_initializers(image, functions) == {0x1000: dict(
        precision='single', operations=2, reads=[0x4000], writes=[0x4004])}
    double = code.replace(b'\xf3', b'\xf2')
    facts = scalar_initializers(fixture(double, bytes(16)), functions)
    assert facts[0x1000]['precision'] == 'double'
    for malformed in (code[:-1] + b'\xc2\x04\x00',
                      code[:8] + code[:8] + b'\xc3', code[:8] + b'\x90' + code[8:]):
        assert not scalar_initializers(fixture(malformed, bytes(16)),
                                       [function(0x1000, 0x1000 + len(malformed))])
