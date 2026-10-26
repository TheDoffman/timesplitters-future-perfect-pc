"""Original end-to-end classification cases using synthetic executable images."""
from dataclasses import replace
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import function_catalog as catalog
from function_categories import contextual_labels
from test_function_evidence import fixture, function


def test_catalog_keeps_exact_bounds_and_diagnostic_evidence():
    code = bytearray(b'\xcc' * 0x40)
    code[:7] = bytes.fromhex('c70100400000c3')
    code[0x10:0x17] = bytes.fromhex('d9442404d9ffc3')
    code[0x20:0x23] = bytes.fromhex('31c0c3')
    image = fixture(bytes(code), struct.pack('<3I', 0x1010, 0x1020, 0x1010))
    functions = [function(0x1000, 0x1007), function(0x1010, 0x1017)]
    before = json.dumps(functions)
    result = catalog.classify(image, functions, [], [])
    entries = result['identified_functions.json']
    assert len(entries) == 2 and [entry['size'] for entry in entries] == [7, 7]
    assert entries[0]['method'] == 'pointer_table_store'
    assert entries[1]['method'] == 'pointer_table_entry'
    assert entries[1]['numeric_observations']['operations'] == ['fcos']
    assert 'identified_name' not in entries[1]
    assert result['crt_functions.json'] == []
    assert result['entry_blocks.json'][0]['start'] == 0x1020
    assert result['entry_blocks.json'][0]['end'] == 0x1023
    assert result['summary.json']['total_functions'] == 2
    assert json.dumps(functions) == before


def test_xref_gap_is_ignored_and_duplicates_do_not_weight_a_category():
    image = fixture(bytes.fromhex('c3') + b'\xcc' * 15 + b'\xc3', b'audio shader')
    functions = [function(0x1000, 0x1001), function(0x1010, 0x1011)]
    xrefs = [dict(type='data_read', **{'from': '0x1008', 'to': '0x4000'}),
             dict(type='data_read', **{'from': '0x1010', 'to': '0x4000'})]
    references = catalog.merge_data_references(image, functions, {}, xrefs * 3)
    assert references == {0x4000: [0x1010]}
    strings = [dict(address='0x4000', string='audio shader')]
    assert not contextual_labels(image, functions, strings, references)
    strings[0]['string'] = 'audio audiovisual sounding'  # Only whole tokens count.
    labels = contextual_labels(image, functions, strings, references)
    assert labels[0x1010]['category'] == 'game_audio'
    assert len(labels[0x1010]['evidence']['audio']) == 1


def test_calls_use_actual_section_bounds_and_distinct_targets():
    image = fixture(b'\xc3', b'\xc3\xc3')
    image.sections[1] = replace(image.sections[1], name='DSOUND', executable=True)
    functions = [dict(start='0x1000', calls_to=['0x4000', '0x4000', '0x4002'])]
    labels = contextual_labels(image, functions, [], {})
    assert labels[0x1000]['category'] == 'game_audio'
    assert labels[0x1000]['evidence']['audio'] == [dict(kind='call', address=0x4000)]


def test_source_paths_remain_distinct_modules_and_labels_are_stable():
    image = fixture(bytes.fromhex('68004000006804400000c3'), b'abcdefghi')
    functions = [function(0x1000, 0x100B)]
    strings = [dict(address='0x4000', string='//RenderWare/test/rwsdk/src/one/common.c#1'),
               dict(address='0x4004', string='//RenderWare/test/rwsdk/src/two/common.c#1')]
    result = catalog.classify(image, functions, strings, [])
    assert set(result['rw_modules.json']) == {'src/one/common.c', 'src/two/common.c'}
    assert result['identified_functions.json'][0]['category'] == 'rw_one'
    assert result == catalog.classify(image, functions, list(reversed(strings)), [])


@pytest.mark.parametrize('functions,strings,xrefs', [
    ([function(0x1000, 0x1002)], [], []),
    ([dict(start='0x1000', end='0x1001', name=8)], [], []),
    ([dict(start='0x1000', end='0x1001', calls_to='0x1000')], [], []),
    ([], [dict(address='0x1000', string=4)], []),
    ([], [dict(address='0x8000', string='hello')], []),
    ([], [], [dict(type=1, **{'from': '0x1000', 'to': '0x1000'})]),
])
def test_bad_metadata_fails_before_classification(functions, strings, xrefs):
    with pytest.raises(ValueError):
        catalog.classify(fixture(b'\xc3'), functions, strings, xrefs)


def test_atomic_outputs_and_failure_preserve_existing_results(tmp_path, monkeypatch):
    monkeypatch.setattr(catalog, 'load_image', lambda path: fixture(b'\xc3'))
    image = tmp_path / 'default.xbe'
    image.write_bytes(b'original')
    paths = [tmp_path / f'{name}.json' for name in ('functions', 'strings', 'xrefs')]
    for path, value in zip(paths, ([function(0x1000, 0x1001)], [], [])):
        path.write_text(json.dumps(value))
    output = tmp_path / 'results'
    catalog.run(image, *paths, output_dir=output)
    original = {path.name: path.read_bytes() for path in output.iterdir()}
    paths[0].write_text('{}')
    with pytest.raises(ValueError):
        catalog.run(image, *paths, output_dir=output)
    assert {path.name: path.read_bytes() for path in output.iterdir()} == original
    with pytest.raises(ValueError, match='replace an input'):
        catalog.run(image, output / 'identified_functions.json', *paths[1:], output_dir=output)
    assert image.read_bytes() == b'original'


def test_standalone_copied_modules_do_not_need_runtime_checkout(tmp_path):
    from test_xbe_image import fixture as make_xbe
    own = tmp_path / 'own'
    own.mkdir()
    source = Path(catalog.__file__).parent
    for name in ('function_catalog.py', 'function_categories.py', 'function_evidence.py',
                 'analysis_output.py', 'recomp_image.py', 'xbe_image.py', 'xbe_kernel_exports.def',
                 'x86_decode.py', 'x86_flow.py', 'x86_recovery.py'):
        shutil.copyfile(source / name, own / name)
    image = tmp_path / 'default.xbe'
    raw = make_xbe()
    raw[0x1000:0x1007] = bytes.fromhex('d9442404d9ffc3')
    image.write_bytes(raw)
    inputs = []
    for name in ('functions', 'strings', 'xrefs'):
        path = tmp_path / f'{name}.json'
        path.write_text(json.dumps([function(0x11000, 0x11007)] if name == 'functions' else []))
        inputs.extend(['--' + name, str(path)])
    output = tmp_path / 'catalog'
    result = subprocess.run([sys.executable, str(own / 'function_catalog.py'), str(image),
                             *inputs, '-o', str(output)], cwd=tmp_path, capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    assert json.loads((output / 'summary.json').read_text())['total_functions'] == 1
    entry = json.loads((output / 'identified_functions.json').read_text())[0]
    assert entry['numeric_observations']['operations'] == ['fcos']
    assert entry['category'] == 'unknown' and 'identified_name' not in entry
    assert {path.name for path in output.iterdir()} == set(catalog.OUTPUT_NAMES)
