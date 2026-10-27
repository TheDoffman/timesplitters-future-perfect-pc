"""Original end-to-end analysis fixtures, with no upstream source required."""
from dataclasses import replace
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from analysis_runner import Disassembler, _load_seed_functions, main
from analysis_cache import AnalysisCache
from analysis_output import OutputWriter, atomic_text
import analysis_config
from test_xbe_image import fixture
from test_x86_functions import fixture as function_fixture


def executable(tmp_path):
    data = fixture()
    data[0x1000:0x1080] = b'\xCC' * 0x80
    data[0x1000:0x1006] = bytes.fromhex('e82b000000c3')
    data[0x1030:0x1035] = bytes.fromhex('558bec5dc3')
    path = tmp_path / 'sample.xbe'
    path.write_bytes(data)
    return path


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def test_standalone_cli_without_toolkit_and_verified_reuse(tmp_path):
    own = tmp_path / 'own'
    own.mkdir()
    for name in ('analysis_cache.py', 'analysis_config.py', 'analysis_output.py', 'analysis_runner.py',
                 'recomp_image.py', 'recomp_symbols.py', 'x86_decode.py', 'x86_flow.py',
                 'x86_recovery.py', 'x86_functions.py', 'xbe_image.py', 'xbe_kernel_exports.def'):
        shutil.copyfile(ROOT / 'tools' / name, own / name)
    xbe, output = executable(tmp_path), tmp_path / 'result'
    # -I removes the inherited Python path; only explicitly copied modules and
    # the installed standard dependency (Capstone) can supply the implementation.
    script = 'import runpy,sys; sys.path.insert(0,sys.argv.pop(1)); runpy.run_module("analysis_runner",run_name="__main__")'
    command = [sys.executable, '-I', '-c', script, str(own), str(xbe), '-o', str(output)]
    def run(*flags):
        result = subprocess.run(command + list(flags), cwd=tmp_path, capture_output=True, text=True, timeout=30)
        assert result.returncode == 0, result.stderr
        return result.stdout
    assert 'complete' in run()
    records = {entry['start']: entry for entry in read(output / 'functions.json')}
    assert records['0x00011000']['calls_to'] == ['0x00011030']
    assert records['0x00011030']['end'] == '0x00011035'
    assert read(output / 'summary.json')['sections_analyzed'][0]['name'] == '.text'
    assert (output / 'asm/text.asm').is_file()
    before = {p: p.stat().st_mtime_ns for p in output.rglob('*') if p.is_file()}
    assert 'cache verified' in run('--stats-only')
    assert before == {p: p.stat().st_mtime_ns for p in before}
    assert 'cache verified' not in run('--force', '--stats-only')
    assert before == {p: p.stat().st_mtime_ns for p in before}
    (output / 'functions.json').write_text('[]')
    assert 'cache verified' not in run()
    assert read(output / 'functions.json')
    assert 'cache verified' not in run('--force')


def test_stats_only_creates_no_output_and_layout_is_derived(tmp_path):
    xbe, output = executable(tmp_path), tmp_path / 'absent'
    assert main([str(xbe), '-o', str(output), '--stats-only']) == 0
    assert not output.exists()
    analysis_config.configure_from_xbe(xbe)
    assert analysis_config.ENTRY_POINT == 0x11000
    assert analysis_config.EXECUTABLE_SECTIONS == [('.text', 0x11000, 0x300)]
    assert analysis_config.configured_from() == str(xbe)


def test_invalid_selection_and_metadata_do_not_write(tmp_path):
    xbe, output = executable(tmp_path), tmp_path / 'absent'
    assert main([str(xbe), '-o', str(output), '--extra-sections', '.missing']) == 1
    assert not output.exists()
    xbe.with_name('sample_analysis.json').write_text('{}')
    assert main([str(xbe), '-o', str(output)]) == 1
    assert not output.exists()


def test_explicit_nonexecutable_section_applies_to_probes_and_discovery(tmp_path):
    xbe = executable(tmp_path)
    raw = bytearray(xbe.read_bytes())
    struct.pack_into('<I', raw, 0x10C, 0x3000)
    struct.pack_into('<I', raw, 0x11C, 2)
    struct.pack_into('<6I', raw, 0x438, 3, 0x12000, 0x100, 0x1200, 0x100, 0x10490)
    raw[0x490:0x497] = b'.extra\0'
    raw.extend(bytes.fromhex('558bec5dc3') + b'\xCC' * 251)
    xbe.write_bytes(raw)
    run = Disassembler(xbe, output_dir=tmp_path / 'result', extra_sections=['.extra'])
    assert run.run()
    assert run.image.get_section('.extra').executable
    assert run.engine.probes_as_prologue(0x12000)
    assert 0x12000 in run.func_detector.functions
    from recomp_image import load_image
    assert not load_image(xbe).get_section('.extra').executable


@pytest.mark.parametrize('entries', [None, {}, [True], [-1], [2**32], [1.5],
    [{'start': '0x100', 'observed': 'yes'}], [{'start': 1, 'note': None}], [{}], ['invalid']])
def test_bad_seed_records_fail(entries, tmp_path):
    path = tmp_path / 'seeds.json'
    path.write_text(json.dumps(entries))
    with pytest.raises(ValueError):
        _load_seed_functions(path)


def test_seed_observation_and_overlap(tmp_path):
    path = tmp_path / 'seeds.json'
    path.write_text(json.dumps([{'start': '0x1001', 'note': 'observed at runtime'},
                                {'start': 0x1002, 'observed': True}, '1003']))
    observed = set()
    assert _load_seed_functions(path, observed) == [0x1001, 0x1002, 0x1003]
    assert observed == {0x1001, 0x1002}
    # The original stream contains mov eax, 0x9090C301. Decoding its immediate
    # as code is rejected unless independent runtime observation authorizes it.
    for observed, expected in ((set(), (0, 1)), ({0x1002}, (1, 0))):
        detector = function_fixture({0: 'b801c39090c3'})
        run = Disassembler('unused', seed_functions=[0x1002], observed_seeds=observed)
        run.image, run.engine, run.func_detector = detector.image, detector.engine, detector
        assert run._seed() == expected


def test_failed_publication_invalidates_receipt_and_cleans_temporary_files(tmp_path, monkeypatch):
    xbe, output = executable(tmp_path), tmp_path / 'result'
    assert Disassembler(xbe, output_dir=output).run()
    assert AnalysisCache(output).is_valid(xbe, None)
    def fail(*args, **kwargs):
        with atomic_text(output / 'functions.json', output) as stream:
            stream.write('partial')
            raise OSError('disk unavailable')
    original = (output / 'functions.json').read_bytes()
    monkeypatch.setattr(OutputWriter, 'write_all', fail)
    assert main([str(xbe), '-o', str(output), '--force']) == 1
    assert not AnalysisCache(output).is_valid(xbe, None)
    assert (output / 'functions.json').read_bytes() == original
    assert not list(output.glob('*.tmp'))


def test_listing_names_full_instruction_bytes_and_selected_summary(tmp_path):
    detector = function_fixture({0: 'c7050020000001000000c3'})
    detector.detect_all()
    section = detector.image.sections[0]
    sections = [replace(section, name=name) for name in ('.text', '../text', 'TEXT', '../../')]
    writer = OutputWriter(tmp_path, detector.engine, detector, detector.xrefs,
                          detector.labels, detector.image, [])
    writer.write_all(sections)
    files = list((tmp_path / 'asm').glob('*.asm'))
    assert len(files) == 4 and len({p.name.casefold() for p in files}) == 4
    assert 'c7050020000001000000' in files[0].read_text().lower().replace(' ', '')
    assert len(read(tmp_path / 'summary.json')['sections_analyzed']) == 4
    with pytest.raises(ValueError, match='escapes'):
        with atomic_text(tmp_path.parent / 'escape.txt', tmp_path):
            pass
