"""Original symbol, reference and bounded-string contracts."""


from pathlib import Path
import sys

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from recomp_image import BinaryImage, SectionInfo, KernelImport
from recomp_symbols import (Label, LabelType, LabelManager, XRef, XRefType, XRefTracker,
                            build_xrefs, extract_strings, populate_entry_point,
                            populate_kernel_labels, populate_string_labels)
from x86_recovery import CodeRecovery


def sample(data, name='.rdata'):
    section = SectionInfo(name, 0x2000, len(data), 0, len(data), False, name == '.text', '')
    return BinaryImage('fixture', data, 0x1000, 0x3000, 0x2000, 0, [section],
                       [KernelImport(7, 'ExampleKernel', 0x3000)])


def test_confidence_priority_and_name_collision_replacement():
    labels = LabelManager()
    labels.add(Label(0, 'sub_00000000', LabelType.FUNCTION, confidence=0.8))
    labels.add(Label(0, 'named', LabelType.FUNCTION, confidence=0.7))
    assert labels.get_name(0) == 'sub_00000000'
    labels.add(Label(0, 'named', LabelType.FUNCTION, confidence=0.8))
    labels.add(Label(0x20, 'named', LabelType.DATA))
    assert labels.get_by_name('named').address == 0x20
    labels.add(Label(0, 'new', LabelType.FUNCTION, confidence=1))
    assert labels.get_by_name('named').address == 0x20  # Updating one address cannot erase another's name.
    labels.add(Label(0x20, 'next', LabelType.DATA, confidence=2))
    assert labels.get_by_name('named') is None
    assert labels.get_by_name('new').address == 0
    assert labels.get_by_name('missing') is None
    assert labels.get_display_name(0x42) == '0x00000042'


def test_helpers_preserve_named_entries_and_sorted_exports():
    image = sample(b'Hello/world!\0')
    labels = LabelManager()
    populate_entry_point(labels, image)
    assert populate_kernel_labels(labels, image) == 1
    labels.auto_name_function(0x2000, '.text', 0.5)
    assert labels.get_name(0x2000) == 'xbe_entry_point'
    refs = extract_strings(image)
    assert populate_string_labels(labels, refs) == 1
    assert labels.get_name(0x2000) == 'xbe_entry_point'
    labels.auto_name_function(0x2200)
    labels.auto_name_function(0x2100)
    assert [label.address for label in labels.all_labels()] == [0x2000, 0x2100, 0x2200, 0x3000]
    assert {label.address for label in labels.labels_in_range(0x2100, 0x2200)} == {0x2100}
    assert labels.count() == 4 and labels.count_by_type(LabelType.FUNCTION) == 2
    assert labels.has(0x2000) and not labels.has(0x2001)
    assert labels.to_list()[-1]['name'] == 'xbox_ExampleKernel'
    populate_string_labels(labels, [{'address': 0x2300, 'string': 'hello/world!'},
                                   {'address': 0x2400, 'string': '!!!'}])
    assert labels.get_name(0x2300) == 'str_hello_world'
    assert labels.get_name(0x2400) == 'str_00002400'


def test_string_runs_require_terminators_and_obey_whole_run_limits():
    raw = b'abc\0valid\0\xffab\tcd\0' + b'x' * 9 + b'\0tail\0unfinished'
    found = extract_strings(sample(raw), 4, 8)
    assert [item['string'] for item in found] == ['valid', 'ab\tcd', 'tail']
    assert all(raw[item['address'] - 0x2000 + item['length']] == 0 for item in found)
    assert extract_strings(sample(raw, '.data')) == []


@pytest.mark.parametrize('minimum,maximum', [(0, 1), (-1, 4), (5, 4)])
def test_string_bounds_are_validated(minimum, maximum):
    with pytest.raises(ValueError):
        extract_strings(sample(b'valid\0'), minimum, maximum)


def test_reference_indices_and_query_copies():
    refs = XRefTracker()
    refs.add(XRef(5, 0, XRefType.CALL))
    refs.add(XRef(1, 0, XRefType.KERNEL_CALL, 'ExampleKernel'))
    refs.add(XRef(5, 0, XRefType.CALL))  # Preserve occurrences; graph consumers decide on deduplication.
    assert refs.get_callers(0) == [5, 5]
    assert refs.get_callees(5) == [0, 0]
    assert refs.get_kernel_call_sites(0) == [1]
    assert refs.count_by_type() == {'call': 2, 'kernel_call': 1}
    refs.get_refs_from(5).clear()
    refs.get_refs_to(0).clear()
    refs.all_kernel_calls()[0].clear()
    assert refs.count() == 3 and refs.get_kernel_call_sites(0) == [1]
    assert [row['from'] for row in refs.to_list()] == ['0x00000001', '0x00000005', '0x00000005']


def test_references_come_from_operand_evidence_without_guessing_dynamic_targets():
    code = bytes.fromhex('e8fbdfffff ff1500300000 ff2500300000 ff1500310000 '
                         'eb00 7500 a100220000 6800220000 ffd0 ffe0')
    image = sample(code, '.text')
    engine = CodeRecovery(image)
    engine.linear_sweep(image.sections[0])
    refs = build_xrefs(engine, image)
    assert refs.count_by_type() == {'call': 2, 'kernel_call': 2, 'jump': 1,
                                   'cond_jump': 1, 'data_read': 1, 'data_imm': 1}
    assert refs.get_callees(0x2000) == [0]
    assert refs.get_kernel_call_sites(0x3000) == [0x2005, 0x200B]
    assert refs.get_callees(0x2011) == [0x3100]
    assert len(refs.get_refs_to(0x2200)) == 2
