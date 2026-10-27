"""Original small programs exercising the independent instruction traversal."""


from pathlib import Path
import sys

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from recomp_image import BinaryImage, SectionInfo
from x86_decode import Instruction
from x86_flow import InstructionTraversal

BASE = 0x1000


def engine(code, executable=True):
    if isinstance(code, str):
        code = bytes.fromhex(code)
    section = SectionInfo('.text', BASE, len(code), 0, len(code), False, executable, 'X')
    image = BinaryImage('synthetic', code, BASE, 0x3000, BASE, 0, [section])
    return InstructionTraversal(image), section


def test_sweep_skips_invalid_bytes_and_reports_monotonic_progress():
    traversal, section = engine('0f 04 90 c3')
    progress = []
    assert traversal.linear_sweep(section, lambda position, size: progress.append((position, size))) == 2
    assert set(traversal.instructions) == {BASE + 1, BASE + 3}
    assert progress[-1] == (4, 4)
    assert [position for position, _ in progress] == sorted(position for position, _ in progress)
    assert all(0 <= position <= size for position, size in progress)
    assert traversal.linear_sweep(section) == 2  # Counts decoded records, not new addresses.


def test_empty_sweep_does_not_report_zero_denominator():
    traversal, section = engine(b'')
    assert traversal.linear_sweep(section, lambda *_: pytest.fail('empty progress callback')) == 0


def test_calls_branches_fallthrough_and_dead_bytes():
    traversal, section = engine('e807000000 7402 90 c3 eb03 90 90 c3 c3')
    traversal.linear_sweep(section)
    expected = {BASE + offset for offset in (0, 5, 7, 8, 9, 12, 13, 14)}
    before = dict(traversal.instructions)
    assert traversal.recursive_descent([BASE], [(BASE, BASE + 15)]) == expected
    assert traversal.instructions == before
    assert traversal.get_call_targets() == {BASE + 12}
    # Split/overlapping intervals describe the same union; arbitrary input order.
    assert traversal.recursive_descent([BASE, BASE, BASE + 100],
        [(BASE + 12, BASE + 15), (BASE + 7, BASE + 10), (BASE, BASE + 9)]) == expected


@pytest.mark.parametrize('code,offsets', [
    ('ebfe', (0,)),                  # Unconditional self-cycle.
    ('e2fe c3', (0, 2)),             # Conditional self-cycle with fall-through.
    ('e8fbffffff c3', (0, 5)),       # Recursive direct call plus return edge.
    ('ffe0 90 c3', (0,)),            # Unknown indirect jump has no fall-through.
    ('ffd0 c3', (0, 2)),             # Unknown indirect call retains fall-through.
    ('eb7f 90 c3', (0,)),            # Out-of-range direct jump terminates.
    ('c20800 90', (0,)),             # Callee-popped return terminates.
    ('7401 c3 c3', (0, 2, 3)),       # Both conditional arms.
])
def test_reachability_terminates_and_obeys_edges(code, offsets):
    traversal, section = engine(code)
    traversal.linear_sweep(section)
    assert traversal.recursive_descent([BASE], [(BASE, BASE + section.raw_size)]) == {
        BASE + offset for offset in offsets}


def test_reachability_never_decodes_a_hole_or_invents_indirect_targets():
    traversal, section = engine('90 90 c3')
    traversal.decode_at(BASE, 1)
    assert traversal.recursive_descent([BASE], [(BASE, BASE + 3)]) == {BASE}
    assert traversal.instruction_count() == 1
    assert traversal.recursive_descent([BASE + 1], [(BASE, BASE + 3)]) == set()
    assert traversal.recursive_descent([BASE], []) == set()
    assert traversal.recursive_descent([BASE], [(BASE + 1, BASE)]) == set()


def test_decode_limit_convergence_and_existing_root():
    traversal, section = engine('90 90 90 c3 90')
    assert traversal.decode_at(BASE, 0) == 0
    assert traversal.decode_at(BASE, -1) == 0
    assert traversal.decode_at(BASE, 1) == 1
    assert traversal.decode_at(BASE, 2) == 2  # Root exists; add its missing suffix.
    assert traversal.decode_at(BASE) == 0    # Converges at the next known start.
    assert traversal.decode_at(BASE + 3) == 1
    assert BASE + 4 not in traversal.instructions  # Stops at ret.
    assert traversal.decode_at(BASE + 4) == 1
    assert traversal.decode_at(BASE + 5) == 0


def test_decode_does_not_skip_invalid_data_or_decode_noncode():
    traversal, _ = engine('0f')
    assert traversal.decode_at(BASE) == 0
    traversal, _ = engine('90 c3', executable=False)
    assert traversal.decode_at(BASE) == 0


def test_realignment_retains_overlaps_and_invalidates_sorted_index():
    traversal, _ = engine('90 b978563412 33c0 c3')
    assert traversal.decode_at(BASE + 1, 1) == 1
    original = traversal.get_instruction(BASE + 1)
    assert traversal.get_instructions_in_range(BASE, BASE + 20) == [original]
    assert traversal.decode_at(BASE + 2, 1) == 1
    assert traversal.get_instruction(BASE + 1) is original
    assert traversal.instruction_covering(BASE + 2) is original
    assert len(traversal.get_instructions_in_range(BASE, BASE + 20)) == 2


def test_covering_checks_older_overlap_and_excludes_exact_end():
    traversal, _ = engine('90' * 20)
    long = Instruction(BASE, 10, 'mov', '', '90' * 10)
    short = Instruction(BASE + 4, 1, 'nop', '', '90')
    traversal.instructions = {BASE: long, BASE + 4: short}
    assert traversal.instruction_covering(BASE) is None
    assert traversal.instruction_covering(BASE + 4) is long
    assert traversal.instruction_covering(BASE + 7) is long
    assert traversal.instruction_covering(BASE + 10) is None
    assert traversal.get_instructions_in_range(BASE, BASE + 4) == [long]
    assert traversal.get_instructions_in_range(BASE + 4, BASE + 5) == [short]
    assert traversal.get_instructions_in_range(BASE + 10, BASE) == []


def test_maximum_length_instruction_boundary():
    traversal, section = engine(b'\x66' * 14 + b'\x90\xc3')
    assert traversal.linear_sweep(section) == 2
    assert traversal.instruction_covering(BASE + 14).size == 15
    assert traversal.instruction_covering(BASE + 15) is None


def test_import_references_and_indexed_candidates_are_separate():
    traversal, section = engine('ff1500200000 ff1500200000 ff148500200000 ff248500200000')
    traversal.linear_sweep(section)
    assert traversal.get_indirect_call_refs() == {0x2000: [BASE, BASE + 6]}
    assert traversal.call_tables == {0x2000}
    assert traversal._jt_candidates == {0x2000}
    assert traversal.jump_tables == {}  # No table proof from syntax alone.
    assert traversal.get_call_targets() == set()


def test_address_zero_is_a_real_target():
    traversal, _ = engine('90')
    instruction = Instruction(BASE, 5, 'call', '', 'e8', is_call=True, call_target=0)
    traversal.instructions = {BASE: instruction, 0: Instruction(0, 1, 'ret', '', 'c3', is_ret=True)}
    assert traversal.get_call_targets() == {0}
    assert traversal.recursive_descent([BASE], [(0, 1), (BASE, BASE + 5)]) == {0, BASE}
