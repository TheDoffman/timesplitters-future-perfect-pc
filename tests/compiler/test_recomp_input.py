"""Synthetic executable mapping and x86 operand contracts, without toolkit code."""

from project_paths import source_file as repo_file

from dataclasses import replace
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from recomp_image import (BinaryImage, SectionInfo, KernelImport, load_image,
                          _find_analysis_json, read_code_window, source_fingerprint)
from xbe_image import XBEError, XBEParser, export_json
from x86_decode import make_decoder, classify_instruction
from test_xbe_image import fixture


def image():
    return BinaryImage('synthetic', b'HEADabcdWXYZ', 0x10000, 0x3000, 0x11000, 0,
        [SectionInfo('.text', 0x11000, 8, 4, 4, False, True, 'X'),
         SectionInfo('.data', 0x12000, 4, 8, 4, True, True, 'W, X')],
        [KernelImport(8, 'DbgPrint', 0x12000)], header_size=4)


def test_exact_reads_do_not_cross_headers_sections_or_zero_fill():
    loaded = image()
    assert loaded.read_bytes_at_va(0x10000, 4) == b'HEAD'
    assert loaded.read_bytes_at_va(0x11000, 4) == b'abcd'
    assert loaded.read_u32_at_va(0x12000) == int.from_bytes(b'WXYZ', 'little')
    for address, length in ((0xFFFF, 1), (0x10003, 2), (0x10004, 1),
                            (0x11002, 4), (0x11004, 1), (0x11008, 1), (0x12004, 1)):
        assert loaded.read_bytes_at_va(address, length) is None
    assert loaded.get_section_at_va(0x11004).name == '.text'
    assert loaded.va_to_file_offset(0x11004) is None
    assert loaded.read_u32_at_va(0x11002) is None
    with pytest.raises(ValueError):
        loaded.read_bytes_at_va(0x11000, -1)


def test_code_windows_are_partial_without_using_next_file_region():
    loaded = image()
    assert read_code_window(loaded, 0x11002, 100) == b'cd'
    assert read_code_window(loaded, 0x11004, 100) == b''
    assert read_code_window(loaded, 0x10000, 100) == b''
    assert read_code_window(loaded, 0x11000, 0) == b''
    with pytest.raises(ValueError):
        read_code_window(loaded, 0x11000, -1)


def test_section_selection_and_imports():
    loaded = image()
    assert [s.name for s in loaded.get_executable_sections()] == ['.text', '.data']
    assert [s.name for s in loaded.get_code_sections()] == ['.text']
    assert loaded.get_section('missing') is None
    assert loaded.get_section_data(loaded.get_section('.data')) == b'WXYZ'
    assert loaded.get_kernel_import_at_thunk(0x12000).name == 'DbgPrint'
    assert loaded.get_kernel_import_at_thunk(0x12004) is None
    with pytest.raises(ValueError):
        loaded.get_section_data(replace(loaded.sections[0], raw_addr=200))


@pytest.mark.parametrize('changes', [dict(base_address=-1), dict(base_address=2**32),
    dict(image_size=2**32), dict(header_size=13), dict(header_size=-1)])
def test_invalid_image_extent(changes):
    with pytest.raises(XBEError):
        replace(image(), **changes)


@pytest.mark.parametrize('changes', [dict(raw_addr=-1), dict(raw_size=100),
    dict(virtual_addr=0xFFFF), dict(virtual_size=0x3000), dict(virtual_size=-1)])
def test_invalid_section_extent(changes):
    with pytest.raises(XBEError):
        loaded = image()
        replace(loaded, sections=[replace(loaded.sections[0], **changes)])


def test_overlap_and_header_alias_rejected():
    loaded = image()
    for address in (0x10000, 0x11002):
        with pytest.raises(XBEError):
            replace(loaded, sections=[loaded.sections[0], replace(loaded.sections[1], virtual_addr=address)])


def write_xbe(tmp_path):
    path = tmp_path / 'sample.xbe'
    path.write_bytes(fixture())
    return path


def test_loader_does_not_require_cached_analysis(tmp_path):
    path = write_xbe(tmp_path)
    loaded = load_image(path)
    assert loaded.entry_point == 0x11000 and loaded.header_size == 0x1000
    assert loaded.read_bytes_at_va(0x10540, 8) == b'XAPILIB\0'
    assert loaded.get_kernel_import_at_thunk(0x11080).name == 'DbgPrint'


@pytest.mark.parametrize('key', ['title_id', 'base_address', 'image_size', 'entry_point',
    'kernel_thunk_addr', 'sections', 'kernel_imports', 'tls'])
def test_rejects_foreign_or_stale_analysis(tmp_path, key):
    path = write_xbe(tmp_path)
    metadata = path.with_name('sample_analysis.json')
    export_json(XBEParser(path).parse(), metadata)
    assert _find_analysis_json(path) == metadata
    assert load_image(path).entry_point == 0x11000
    data = json.loads(metadata.read_text())
    data[key] = None if data[key] is not None else 1
    metadata.write_text(json.dumps(data))
    with pytest.raises(XBEError, match=key):
        load_image(path)


def test_explicit_analysis_and_unrelated_json(tmp_path):
    path = write_xbe(tmp_path)
    metadata = tmp_path / 'unrelated_analysis.json'
    metadata.write_text('not JSON')
    assert _find_analysis_json(path) is None
    assert load_image(path).sections
    with pytest.raises(ValueError):
        load_image(path, metadata)
    export_json(XBEParser(path).parse(), metadata)
    assert load_image(path, metadata).sections
    with pytest.raises(FileNotFoundError):
        load_image(path, tmp_path / 'missing.json')


@pytest.mark.parametrize('hex_bytes,field,target', [
    ('e805000000', 'call_target', 0x1100A), ('e9f6ffffff', 'jump_target', 0x10FFB),
    ('75fe', 'jump_target', 0x11000), ('e2fe', 'jump_target', 0x11000),
    ('e3fe', 'jump_target', 0x11000), ('ff1500200100', 'memory_ref', 0x12000),
    ('ff2500200100', 'memory_ref', 0x12000), ('ff148500200100', 'call_table', 0x12000),
    ('ff248500200100', 'jump_table', 0x12000), ('a100200100', 'memory_ref', 0x12000),
    ('6800200100', 'imm_ref', 0x12000), ('b800200100', 'imm_ref', 0x12000),
    ('c7050020010000240100', 'memory_ref', 0x12000),
    ('c7050020010000240100', 'imm_ref', 0x12400),
    ('e8fbeffeff', 'call_target', 0),
])
def test_operand_targets(hex_bytes, field, target):
    decoded = next(make_decoder().disasm(bytes.fromhex(hex_bytes), 0x11000))
    instruction = classify_instruction(decoded, 0x10000, 0x10000)
    assert getattr(instruction, field) == target
    if field in ('call_target', 'jump_target', 'memory_ref', 'imm_ref'):
        assert instruction.to_dict()[field] == f'0x{target:08X}'


@pytest.mark.parametrize('hex_bytes,flags', [
    ('90', ['is_nop']), ('c3', ['is_ret', 'is_terminator']),
    ('c20800', ['is_ret', 'is_terminator']), ('cb', ['is_ret', 'is_terminator']),
    ('ebfe', ['is_jump', 'is_branch', 'is_terminator']),
    ('75fe', ['is_cond_jump', 'is_branch']), ('ffd0', ['is_call']),
    ('e2fe', ['is_cond_jump', 'is_branch']), ('d9e8', []), ('0f58c1', []),
])
def test_classification(hex_bytes, flags):
    decoded = next(make_decoder().disasm(bytes.fromhex(hex_bytes), 0x11000))
    instruction = classify_instruction(decoded, 0x10000, 0x10000)
    for name in ('is_call', 'is_ret', 'is_jump', 'is_cond_jump', 'is_nop', 'is_branch', 'is_terminator'):
        assert getattr(instruction, name) == (name in flags)
    assert instruction.end_address == 0x11000 + len(bytes.fromhex(hex_bytes))


@pytest.mark.parametrize('hex_bytes', ['ffd0', '8b4504', 'ff1483', 'ff148500000010', 'b8ffffffff'])
def test_dynamic_and_outside_operands_are_not_static_targets(hex_bytes):
    decoded = next(make_decoder().disasm(bytes.fromhex(hex_bytes), 0x11000))
    instruction = classify_instruction(decoded, 0x10000, 0x10000)
    assert all(getattr(instruction, name) is None for name in
               ('call_target', 'jump_target', 'memory_ref', 'imm_ref', 'call_table', 'jump_table'))


def test_invalid_or_truncated_instruction_has_no_record():
    assert list(make_decoder().disasm(b'\xe8\x01', 0x11000)) == []


def test_standalone_modules_and_cache_invalidation(tmp_path):
    for name in ('recomp_image.py', 'x86_decode.py', 'x86_flow.py', 'x86_recovery.py', 'x86_functions.py', 'recomp_symbols.py', 'analysis_cache.py', 'analysis_config.py', 'analysis_output.py', 'analysis_runner.py', 'xbe_image.py', 'xbe_kernel_exports.def'):
        shutil.copyfile(repo_file(ROOT, 'tools', name), tmp_path / name)
    path = write_xbe(tmp_path)
    script = """import sys
from pathlib import Path
sys.path.insert(0, sys.argv[1])
from recomp_image import load_image, source_fingerprint
from x86_decode import make_decoder, classify_instruction
from x86_flow import InstructionTraversal
from x86_recovery import CodeRecovery
from x86_functions import FunctionDetector
from recomp_symbols import LabelManager, build_xrefs
image = load_image(sys.argv[2])
assert image.entry_point == 0x11000
flow = InstructionTraversal(image)
assert flow.decode_at(image.entry_point, 1) == 1
assert flow.recursive_descent([image.entry_point], [(0x11000, 0x11200)]) == {image.entry_point}
instruction = classify_instruction(next(make_decoder().disasm(b'\\xc3', 0x11000)), 0x10000, 0x2000)
assert instruction.is_ret
recovery = CodeRecovery(image)
assert not recovery.probes_as_constant_stub(image.entry_point)
assert recovery.resync_jump_tables() == 0
assert recovery.decode_at(image.entry_point, 1) == 1
detector = FunctionDetector(recovery, image, build_xrefs(recovery, image), LabelManager())
assert detector.detect_all() >= 1
assert detector.functions[image.entry_point].num_instructions >= 1
for name in ('recomp_image.py', 'x86_decode.py', 'x86_flow.py', 'x86_recovery.py', 'x86_functions.py', 'recomp_symbols.py', 'analysis_cache.py', 'analysis_config.py', 'analysis_output.py', 'analysis_runner.py', 'xbe_image.py', 'xbe_kernel_exports.def'):
    before = source_fingerprint()
    with (Path(sys.argv[1]) / name).open('a') as stream: stream.write('\\n')
    assert before != source_fingerprint()
assert not any(name == 'tools' or name.startswith('tools.') for name in sys.modules)
"""
    result = subprocess.run([sys.executable, '-I', '-c', script, str(tmp_path), str(path)],
                            capture_output=True, text=True)
    assert result.returncode == 0, result.stderr


def test_private_supported_us_view():
    path = ROOT / 'private/game/default.xbe'
    if not path.is_file():
        pytest.skip('Private game absent')
    loaded = load_image(path)
    assert loaded.entry_point == 0x38024D
    assert len(loaded.sections) == 23 and len(loaded.kernel_imports) == 151
    for section in loaded.get_code_sections():
        assert loaded.read_bytes_at_va(section.virtual_addr, min(section.virtual_size, section.raw_size))


def test_production_disassembler_uses_independent_inputs():
    runtime = ROOT / 'external/ts3FPrecomp'
    if not (runtime / '.git').exists():
        pytest.skip('Pinned runtime unavailable')
    script = """from tools.disasm.loader import BinaryImage
from tools.disasm.engine import Instruction, DisasmEngine
from tools.disasm import cache
assert BinaryImage.__module__ == 'recomp_image'
assert Instruction.__module__ == 'x86_decode'
assert DisasmEngine.linear_sweep.__module__ == 'x86_flow'
assert DisasmEngine.decode_at.__module__ == 'x86_flow'
assert DisasmEngine.recursive_descent.__module__ == 'x86_flow'
assert DisasmEngine.resync_jump_tables.__module__ == 'x86_recovery'
assert DisasmEngine.probes_as_function_body.__module__ == 'x86_recovery'
assert DisasmEngine.probes_as_prologue.__module__ == 'x86_recovery'
from tools.disasm.functions import FunctionDetector
from tools.disasm.labels import LabelManager
from tools.disasm.xrefs import XRefTracker
assert FunctionDetector.__module__ == 'x86_functions'
assert LabelManager.__module__ == XRefTracker.__module__ == 'recomp_symbols'
from tools.disasm.disasm import Disassembler
from tools.disasm.output import OutputWriter
from tools.disasm import config
assert Disassembler.__module__ == 'analysis_runner'
assert OutputWriter.__module__ == 'analysis_output'
assert config.__name__ == 'analysis_config'
assert cache.AnalysisCache.__module__ == 'analysis_cache'
before = cache.AnalysisCache._opts_key(False)
cache.source_fingerprint = lambda: 'changed independent dependency'
assert before != cache.AnalysisCache._opts_key(False)
"""
    result = subprocess.run([sys.executable, '-c', script], cwd=runtime,
                            env=dict(os.environ, TSFP_PROJECT_ROOT=str(ROOT)),
                            capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    result = subprocess.run([sys.executable, '-m', 'pytest', 'tools/disasm', '-q', '--tb=short'],
                            cwd=runtime, env=dict(os.environ, TSFP_PROJECT_ROOT=str(ROOT)),
                            capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stdout + result.stderr
