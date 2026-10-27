"""Original miniature programs validating discovered callable ranges."""
from pathlib import Path
import struct
import sys

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from recomp_image import BinaryImage, SectionInfo
from recomp_symbols import LabelManager, populate_entry_point, build_xrefs
from x86_functions import FunctionDetector, Function, BodyIndex
from x86_recovery import CodeRecovery

BASE = 0x1000


def rel(opcode, source, destination):
    prefix = bytes.fromhex(opcode)
    return prefix + struct.pack('<i', destination - source - len(prefix) - 4)


def fixture(blocks, size=0x100, data=b'', fill=0x90):
    code = bytearray([fill] * size)
    for offset, content in blocks.items():
        if isinstance(content, str):
            content = bytes.fromhex(content)
        code[offset:offset + len(content)] = content
    raw = bytes(code) + data
    sections = [SectionInfo('.text', BASE, size, 0, size, False, True, '')]
    if data:
        sections.append(SectionInfo('.rdata', BASE + 0x2000, len(data), size, len(data), False, False, ''))
    image = BinaryImage('fixture', raw, BASE, 0x4000, BASE, 0, sections)
    engine = CodeRecovery(image)
    engine.linear_sweep(sections[0])
    engine.resync_jump_tables()
    labels = LabelManager()
    populate_entry_point(labels, image)
    return FunctionDetector(engine, image, build_xrefs(engine, image), labels)


def test_complete_discovery_exports_call_graph_and_named_entry():
    detector = fixture({0: rel('e8', BASE, BASE + 0x30) + b'\xc3', 0x30: '558bec5dc3'}, fill=0xCC)
    assert detector.detect_all() == 2
    entry, callee = detector.functions[BASE], detector.functions[BASE + 0x30]
    assert (entry.end, entry.name, entry.calls_to) == (BASE + 6, 'xbe_entry_point', [BASE + 0x30])
    assert callee.end == BASE + 0x35 and callee.called_by == [BASE]
    assert callee.has_prologue
    assert detector.get_function_at(BASE + 0x31) is callee
    assert detector.get_function_at(BASE + 0x36) is None
    assert detector.get_functions_in_section('.text') == [entry, callee]
    assert detector.summary()['total_functions'] == 2
    assert entry.to_dict()['calls_to'] == ['0x00001030']
    detector._build_call_graph()
    assert callee.called_by == [BASE]  # Rebuilding the graph cannot accumulate stale callers.


def test_shared_tail_entry_preserves_owning_body_and_its_epilogue():
    detector = fixture({0: rel('e8', BASE, BASE + 0x30) + rel('e9', BASE + 5, BASE + 0x34),
                        0x30: '568bf1905ec3'}, fill=0xCC)
    detector.detect_all()
    assert detector.functions[BASE + 0x30].end == BASE + 0x36
    tail = detector.functions[BASE + 0x34]
    assert tail.end == BASE + 0x36 and tail.detection_method == 'tail_jump_alias'
    assert detector.get_function_at(BASE + 0x34) is tail
    assert BASE + 0x34 not in detector._candidates


def test_continuation_that_pops_unsaved_register_is_not_a_new_function():
    detector = fixture({0: rel('e9', BASE, BASE + 0x31), 0x31: '5ec3'})
    detector.detect_all()
    assert BASE + 0x31 not in detector.functions


def test_forward_branch_keeps_late_return_and_respects_next_entry():
    detector = fixture({0: '7406c39090909090c3'})
    assert detector._find_function_end(BASE, None, BASE + 0x100) == BASE + 9
    assert detector._find_function_end(BASE, BASE + 8, BASE + 0x100) == BASE + 3
    assert detector._find_function_end(BASE, None, BASE + 8) == BASE + 3


def test_inline_switch_data_and_all_cases_belong_to_the_body():
    table = BASE + 7
    targets = [BASE + 0x20, BASE + 0x24, BASE + 0x28]
    detector = fixture({0: b'\xff\x24\x85' + struct.pack('<I', table),
                        7: struct.pack('<III', *targets), 0x20: 'c3', 0x24: 'c3', 0x28: 'c3'})
    end = detector._find_function_end(BASE, None, BASE + 0x100)
    assert end == BASE + 0x29
    assert detector.engine.jump_tables == {table: table + 12}
    assert detector._find_function_end(BASE, table + 8, BASE + 0x100) == table


def test_data_table_callbacks_become_aliases_but_lone_unaligned_pointer_does_not():
    detector = fixture({0: 'c3', 0x40: 'b804000000c3', 0x70: 'b805000000c3', 0x91: 'b806000000c3'},
                        data=struct.pack('<IIIII', BASE + 0x40, BASE + 0x70, 0, BASE + 0x91, 0))
    detector.detect_all()
    assert detector.functions[BASE + 0x40].end == BASE + 0x46
    assert detector.functions[BASE + 0x70].end == BASE + 0x76
    assert BASE + 0x91 not in detector.functions


def test_indexed_call_site_authorizes_a_pointer_table_in_code():
    pointers = struct.pack('<III', BASE + 0x80, BASE + 0xA0, 0)
    blocks = {0: b'\xff\x14\x85' + struct.pack('<I', BASE + 0x40) + b'\xc3',
              0x40: pointers, 0x80: 'b801000000c3', 0xA0: 'b802000000c3'}
    detector = fixture(blocks)
    detector.detect_all()
    assert {BASE + 0x80, BASE + 0xA0} <= detector.functions.keys()
    blocks[0] = b'\xc3'
    without_site = fixture(blocks)
    without_site.detect_all()
    assert BASE + 0x80 not in without_site.functions and BASE + 0xA0 not in without_site.functions


def test_long_callback_requires_a_known_table_mate_and_boundary():
    blocks = {0: 'c3', 0x1F: 'c3', 0x20: b'\xb8\x03\0\0\0' + b'\x90' * 300 + b'\xc3'}
    detector = fixture(blocks, size=0x200, data=struct.pack('<II', BASE, BASE + 0x20))
    detector.detect_all()
    assert detector.functions[BASE + 0x20].end == BASE + 0x20 + 306
    unknown = fixture(blocks, size=0x200, data=struct.pack('<II', BASE + 0x180, BASE + 0x20))
    unknown.detect_all()
    assert BASE + 0x20 not in unknown.functions


def test_installed_callback_target_is_discovered_from_the_called_slot():
    slot, callback = BASE + 0x2000, BASE + 0x61
    code = b'\xc7\x05' + struct.pack('<II', slot, callback) + b'\xff\x15' + struct.pack('<I', slot) + b'\xc3'
    detector = fixture({0: code, 0x61: '31c0c3'}, data=bytes(4))
    detector.detect_all()
    assert detector.functions[callback].end == callback + 3
    assert detector.functions[callback].detection_method == 'indirect_call_slot'


def test_alias_chain_reaches_fixed_point_without_looping():
    detector = fixture({0: 'c3', 0x30: rel('e9', BASE + 0x30, BASE + 0x60),
                        0x60: rel('e9', BASE + 0x60, BASE + 0x30)})
    detector.functions = {BASE: Function(BASE, BASE + 1, 'entry', '.text')}
    detector._alias_entries[BASE + 0x30] = BASE + 0x35
    assert detector._pass_alias_tail_jumps(detector.image.get_code_sections()) == 1
    assert detector._alias_entries == {BASE + 0x30: BASE + 0x35, BASE + 0x60: BASE + 0x65}
    assert detector._pass_alias_tail_jumps(detector.image.get_code_sections()) == 0


def test_candidate_priority_seed_aliases_and_empty_body_rejection():
    detector = fixture({0: '9090c3'})
    detector._add_candidate(BASE, 0.5, 'weak')
    detector._add_candidate(BASE, 1, 'strong')
    detector._add_candidate(BASE, 1, 'equal')
    assert detector._candidates[BASE] == (1, 'strong')
    detector.functions[BASE] = Function(BASE, BASE + 3, 'owner')
    detector._add_candidate(BASE + 1, 0.95, 'seed_vtable_thunk')
    detector._pass_seed_aliases()
    assert detector._alias_entries == {BASE + 1: BASE + 3}
    detector._alias_entries[BASE + 0x500] = BASE + 0x501
    detector._build_alias_entries()
    assert BASE + 0x500 not in detector.functions


def test_interval_snapshot_distinguishes_start_interior_and_gap():
    index = BodyIndex([(20, 25), (0, 10)])
    assert index.containing(0) == (0, 10)
    assert index.containing(0, strict=True) is None
    assert index.containing(9, strict=True) == (0, 10)
    assert index.containing(10) is None
    assert index.following(0, 30) == 20 and index.following(20, 30) == 30
