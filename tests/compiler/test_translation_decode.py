"""Original small programs for the project translation decoder."""

from project_paths import source_file as repo_file

from pathlib import Path
import sys
import os
import shutil
import subprocess
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from translation_decode import Disassembler, Instruction


def decode(hexadecimal, start=0x1000, **kwargs):
    raw = bytes.fromhex(hexadecimal)
    return Disassembler().disassemble_function(raw, start, start + len(raw), **kwargs)


def test_register_width_segment_index_and_signed_displacement():
    instructions = decode('648b05000000008b448efc66b8ffff')
    fs, indexed, immediate = instructions
    assert fs.operands[1].mem_seg == 'fs' and fs.operands[1].mem_disp == 0
    assert fs.operands[1].mem_size == 4
    operand = indexed.operands[1]
    assert (operand.mem_base, operand.mem_index, operand.mem_scale, operand.mem_disp) == ('esi', 'ecx', 4, -4)
    assert immediate.operands[0].reg == 'ax' and immediate.operands[1].imm == 65535


def test_call_and_conditional_targets_wrap_to_guest_width():
    instructions = decode('e80000000075f9', start=0xFFFFF000)
    assert instructions[0].call_target == 0xFFFFF005
    assert instructions[1].jump_target == 0xFFFFF000
    assert instructions[1].is_cond_jump and not instructions[1].is_terminator


def test_restart_anchor_removes_a_straddling_instruction():
    # A linear mov immediate covers the true xor entry at byte two.
    instructions = decode('b89931c0c3', resync=[0x1002])
    assert [(item.address, item.mnemonic) for item in instructions] == [(0x1002, 'xor'), (0x1004, 'ret')]
    assert all(item.end_address <= 0x1005 for item in instructions)


@pytest.mark.parametrize('start,end', [(0x1000, 0x1000), (0x1000, 0x1002),
                                      (-1, 0), (0xFFFFFFFF, 0x100000001), (True, 2)])
def test_invalid_windows_do_not_decode_outside_supplied_bytes(start, end):
    assert Disassembler().disassemble_function(b'\xc3', start, end) == []


def test_cfg_does_not_decode_unreachable_bytes_or_follow_a_call():
    raw = bytes.fromhex('eb02ffffe801000000c3c3')
    result = Disassembler().disassemble_cfg(raw, 0x1000, 0x100B, [0x1000])
    assert [item.address for item in result] == [0x1000, 0x1004, 0x1009]
    assert result[1].call_target == 0x100A


def test_cfg_loops_terminate_and_explicit_entries_can_start_at_stops():
    raw = bytes.fromhex('9075fdc3')
    decoder = Disassembler()
    result = decoder.disassemble_cfg(raw, 0x1000, 0x1004, [0x1000], {0x1003})
    assert [item.address for item in result] == [0x1000, 0x1001]
    result = decoder.disassemble_cfg(raw, 0x1000, 0x1004, [0x1000, 0x1003], {0x1003})
    assert [item.address for item in result] == [0x1000, 0x1001, 0x1003]


def test_blocks_follow_decoded_targets_and_stop_at_returns():
    instructions = decode('85c074013390c3')
    # The target at byte five is inside another instruction; without a restart
    # it is not a real decoded leader and must not become a phantom edge.
    blocks = Disassembler().build_basic_blocks(instructions, 0x1000, 0x1007)
    assert all(target in {block.start for block in blocks} for block in blocks for target in block.successors)


def test_blocks_do_not_bridge_missing_instruction_ranges():
    instructions = [Instruction(0x1000, 1, 'nop', '', '90'), Instruction(0x1010, 1, 'ret', '', 'c3')]
    blocks = Disassembler().build_basic_blocks(instructions, 0x1000, 0x1011)
    assert [block.start for block in blocks] == [0x1000]
    assert all(not block.successors for block in blocks)
    blocks = Disassembler().build_basic_blocks(instructions, 0x1000, 0x1011, extra_leaders=[0x1010])
    assert [block.start for block in blocks] == [0x1000, 0x1010]
    assert all(not block.successors for block in blocks)


def test_return_does_not_nominate_unreferenced_trailing_bytes():
    instructions = decode('c3b878563412c3')
    blocks = Disassembler().build_basic_blocks(instructions, 0x1000, 0x1007)
    assert [(block.start, block.end) for block in blocks] == [(0x1000, 0x1001)]
    blocks = Disassembler().build_basic_blocks(instructions, 0x1000, 0x1007, extra_leaders=[0x1001])
    assert [(block.start, block.end) for block in blocks] == [(0x1000, 0x1001), (0x1001, 0x1007)]


def test_overlapping_entries_share_a_convergence_block():
    instructions = [Instruction(0x1000, 5, 'mov', '', 'b800000000'),
        Instruction(0x1002, 2, 'xor', '', '31c0'), Instruction(0x1004, 1, 'nop', '', '90'),
        Instruction(0x1005, 1, 'ret', '', 'c3')]
    blocks = Disassembler().build_basic_blocks(instructions, 0x1000, 0x1006, extra_leaders=[0x1002])
    assert [(b.start, [i.address for i in b.instructions], b.successors) for b in blocks] == [
        (0x1000, [0x1000], [0x1005]), (0x1002, [0x1002, 0x1004], [0x1005]), (0x1005, [0x1005], [])]


def test_restart_recovery_can_supply_the_first_decodable_block():
    instructions = decode('b89931c0c3', resync=[0x1002])
    blocks = Disassembler().build_basic_blocks(instructions, 0x1000, 0x1005)
    assert [block.start for block in blocks] == [0x1002]


def test_copied_decoder_runs_without_toolkit_imports(tmp_path):
    root = Path(__file__).resolve().parents[2]
    for name in ('translation_decode.py', 'x86_decode.py'):
        shutil.copyfile(repo_file(root, 'tools', name), tmp_path / name)
    code = '''from translation_decode import Disassembler
raw=bytes.fromhex('85c0740231c0c3')
d=Disassembler();ins=d.disassemble_cfg(raw,0x1000,0x1007,[0x1000])
b=d.build_basic_blocks(ins,0x1000,0x1007)
assert [(x.start,x.successors) for x in b]==[(0x1000,[0x1004,0x1006]),(0x1004,[0x1006]),(0x1006,[])]
'''
    result = subprocess.run([sys.executable, '-c', code], cwd=tmp_path,
                            capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stdout + result.stderr


def test_production_decoder_and_existing_translation_consumers(tmp_path):
    root = Path(__file__).resolve().parents[2]
    runtime = root / 'external/ts3FPrecomp'
    if not (runtime / 'tools/recomp/disasm.py').is_file():
        pytest.skip('Pinned runtime unavailable')
    code = '''from tools.recomp.disasm import Disassembler
assert Disassembler.__module__=='translation_decode'
import pytest
raise SystemExit(pytest.main(['tools/recomp','-q','--tb=short']))
'''
    result = subprocess.run([sys.executable, '-c', code], cwd=runtime,
                            env=dict(os.environ, TSFP_PROJECT_ROOT=str(root)),
                            capture_output=True, text=True, timeout=300)
    assert result.returncode == 0, result.stdout + result.stderr
