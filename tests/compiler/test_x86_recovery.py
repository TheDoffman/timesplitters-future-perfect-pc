"""Original bounded fixtures for table ownership and code-shape evidence."""


from copy import deepcopy
from pathlib import Path
import struct
import sys

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from recomp_image import BinaryImage, SectionInfo
from x86_recovery import CodeRecovery

BASE = 0x4000


def program(code, executable=True, virtual_size=None):
    raw = bytes.fromhex(code) if isinstance(code, str) else bytes(code)
    section = SectionInfo('code', BASE, virtual_size or len(raw), 0, len(raw), False, executable, '')
    return CodeRecovery(BinaryImage('fixture', raw, BASE, section.virtual_size, BASE, 0, [section]))


def switch(inline=False, named_slot=0, omitted=0, entries=3, size=0x600, preceding=None):
    raw = bytearray(b'\xcc' * size)
    dispatch = BASE + 0x50
    table = dispatch + 7 if inline else BASE + 0x100
    displacement = table + 4 * named_slot - 4 * omitted
    raw[0x50:0x57] = b'\xff\x24\x85' + struct.pack('<I', displacement)
    targets = [BASE + 0x280 + 16 * index for index in range(entries)]
    for index, target in enumerate(targets):
        struct.pack_into('<I', raw, table - BASE + 4 * index, target)
        raw[target - BASE:target - BASE + 2] = b'\x90\xc3'
    if preceding is not None:
        struct.pack_into('<I', raw, table - BASE - 4, preceding)
    raw[table - BASE + 4 * entries] = 0xC3
    recovery = program(raw)
    recovery.linear_sweep(recovery.image.sections[0])
    # Make the dispatch an explicit known instruction; alignment through other
    # bytes is not the property these table tests are exercising.
    recovery.decode_at(dispatch)
    return recovery, dispatch, table, displacement, targets


@pytest.mark.parametrize('inline,named_slot', [(False, 0), (False, 2), (True, 0), (True, 2)])
def test_table_extent_aliases_and_dispatch_ownership(inline, named_slot):
    recovery, dispatch, table, displacement, targets = switch(inline, named_slot)
    assert recovery.resync_jump_tables() == 1
    assert recovery.jump_tables == {table: table + 12}
    assert recovery.jump_table_entries(table) == targets
    assert recovery.jump_table_entries(displacement) == targets
    assert recovery.instructions[dispatch].jump_table == displacement
    assert all(not (instruction.address < table + 12 and instruction.end_address > table)
               for instruction in recovery.instructions.values())
    assert recovery.get_instruction(table + 12).is_ret
    assert all(recovery.get_instruction(target) is not None for target in targets)
    before = deepcopy(recovery.instructions)
    assert recovery.resync_jump_tables() == 0
    assert recovery.instructions == before


@pytest.mark.parametrize('omitted', [1, 2, 3])
def test_unused_leading_indices_preserve_non_table_bytes(omitted):
    recovery, dispatch, table, displacement, targets = switch(omitted=omitted)
    before = recovery.get_instruction(displacement)
    assert recovery.resync_jump_tables() == 1
    assert recovery.jump_table_entries(displacement) == targets
    assert recovery.get_instruction(displacement) == before
    assert recovery.get_instruction(dispatch) is not None
    assert recovery.jump_tables == {table: table + 12}


def test_backward_extension_rejects_distant_in_section_false_pointer():
    recovery, _, table, _, _ = switch(size=0xA000, preceding=BASE + 0x8000)
    assert recovery.resync_jump_tables() == 1
    assert recovery.jump_tables == {table: table + 12}


def test_rejected_candidate_is_not_deleted_or_marked_done():
    recovery, _, _, displacement, _ = switch(entries=2)
    before = deepcopy(recovery.instructions)
    assert recovery.resync_jump_tables() == 0
    assert recovery.instructions == before
    assert displacement not in recovery._jt_done
    assert recovery.jump_table_entries(displacement) == []
    assert recovery.resync_jump_tables(min_entries=2) == 1


def test_table_bounds_noncode_and_virtual_zero_fill():
    recovery, _, table, _, _ = switch()
    recovery = program(recovery.image.raw_data, executable=False)
    recovery._jt_candidates.add(table)
    assert recovery.resync_jump_tables() == 0
    recovery = program(struct.pack('<II', BASE, BASE), virtual_size=4096)
    recovery._jt_candidates.add(BASE)
    assert recovery.resync_jump_tables() == 0  # The virtual tail supplies no third word.
    assert recovery._table_span(BASE - 4, None, 1, 10) is None
    assert recovery._table_span(BASE + 4094, None, 1, 10) is None


def test_limits_bound_table_measurement_and_empty_requests():
    recovery, _, table, _, targets = switch(entries=6)
    assert recovery.resync_jump_tables(3, 0) == 0
    assert recovery.resync_jump_tables(0, 512) == 0
    assert recovery.resync_jump_tables(3, 3) == 1
    assert recovery.jump_tables == {table: table + 12}
    assert recovery.jump_table_entries(table) == targets[:3]


@pytest.mark.parametrize('code,expected', [
    ('558bec', True), ('5589e5', True), ('558d6c2404', True),
    ('53', True), ('56', True), ('57', True), ('83ec20', True), ('8bff', True),
    ('6a306878563412e800000000', True),
    ('55c3', False), ('6a30687856341290', False), ('6a01c3', False),
    ('6aff6878563412e800000000', False), ('89c0c3', False),
])
def test_prologue_shapes(code, expected):
    assert program(code).probes_as_prologue(BASE) is expected


@pytest.mark.parametrize('code,constant,thunk', [
    ('b988776655c20800', True, False), ('b801000000c3', True, False),
    ('31c0c3', False, False), ('b80100000090c3', False, False),
    ('8b11ff6230', False, True), ('8b01ff20', False, True),
    ('8b11ff6008', False, False), ('8b5104ff6230', False, False),
    ('8b11ff649a30', False, False), ('8b11e900000000', False, False),
])
def test_exact_accessor_and_virtual_dispatch_shapes(code, constant, thunk):
    recovery = program(code)
    assert recovery.probes_as_constant_stub(BASE) is constant
    assert recovery.probes_as_vcall_thunk(BASE) is thunk


@pytest.mark.parametrize('code,tail,extent,returns', [
    ('90c3', None, 2, True), ('ebfe', BASE, 2, False),
    ('eb7f90c3', BASE + 129, 2, False), ('ffe090c3', None, 2, False),
    ('eb0190c3', None, 4, True), ('740190c3', None, 4, True),
    ('e800000000c3', None, 6, True), ('90900f', None, None, False),
])
def test_block_evidence_keeps_local_forward_transfers(code, tail, extent, returns):
    recovery = program(code)
    assert recovery.block_tail_jump(BASE) == tail
    assert recovery.block_extent_end(BASE) == (BASE + extent if extent is not None else None)
    assert recovery.probes_as_returning_body(BASE) is returns


def test_limits_and_general_body_are_distinct_from_return_evidence():
    recovery = program('9090ffe0')
    assert recovery.probes_as_function_body(BASE)
    assert not recovery.probes_as_returning_body(BASE)
    assert not recovery.probes_as_function_body(BASE, 2)
    assert recovery.block_extent_end(BASE, 2) is None
    for count in (0, -1):
        assert not recovery.probes_as_function_body(BASE, count)
        assert not recovery.probes_as_returning_body(BASE, count)
        assert recovery.block_extent_end(BASE, count) is None
        assert recovery.block_tail_jump(BASE, count) is None
        assert recovery.entry_pops_unsaved(BASE, count) == []


def test_measured_switch_arm_is_evidence_unknown_dispatch_is_not():
    recovery, dispatch, table, displacement, targets = switch(inline=True)
    assert not recovery.probes_as_returning_body(dispatch)
    recovery.resync_jump_tables()
    assert recovery.probes_as_returning_body(dispatch)
    assert recovery._first_arm_after(displacement, dispatch) == targets[0]
    assert recovery._first_arm_after(table, targets[-1]) is None
    assert not recovery.probes_as_returning_body(dispatch, 2)


def test_saved_register_evidence_is_ordered_and_stops_at_return():
    recovery = program('5b5b535e565f575d5558c35b')
    assert recovery.entry_pops_unsaved(BASE) == ['ebx', 'esi', 'edi', 'ebp']
    assert program('535b5b565ec35f').entry_pops_unsaved(BASE) == []
    assert program('535beb015b5ec3').entry_pops_unsaved(BASE) == ['esi']


def test_all_probes_are_read_only_and_reject_unbacked_or_noncode_bytes():
    recovery = program('cc558bec535b5dc3', virtual_size=32)
    recovery.decode_at(BASE + 1)
    methods = ('probes_as_function_body', 'probes_as_returning_body', 'probes_as_prologue',
               'probes_as_constant_stub', 'probes_as_vcall_thunk', 'follows_padding',
               'block_tail_jump', 'block_extent_end', 'entry_pops_unsaved')
    state = deepcopy((recovery.instructions, recovery.jump_tables, recovery._jt_candidates,
                      recovery._jt_by_disp, recovery._jt_done, recovery._sorted_addrs))
    for method in methods:
        getattr(recovery, method)(BASE + 1)
        for address in (BASE - 1, BASE + 8, BASE + 32):
            assert not getattr(recovery, method)(address)
    assert state == (recovery.instructions, recovery.jump_tables, recovery._jt_candidates,
                     recovery._jt_by_disp, recovery._jt_done, recovery._sorted_addrs)
    assert recovery.follows_padding(BASE + 1)
    assert not recovery.follows_padding(BASE)
    recovery = program(recovery.image.raw_data, executable=False)
    for method in methods:
        assert not getattr(recovery, method)(BASE + 1)
