"""A function recomp_manual.c wraps is emitted as sub_X_gen and called as sub_X.

The wrap mechanism (manual_scan.py _WRAP_RE) renames the generated body so the
hand-written sub_X can call it. Renaming alone also renamed every call to it,
so generated callers went straight to sub_X_gen and the wrapper was never
reached -- found on the first title to use a wrap, Marvel vs Capcom 2.
"""

from tools.recomp.disasm import Instruction
from tools.recomp.lifter import Lifter


TARGET = 0x001DFB40


def _direct_call(target=TARGET):
    insn = Instruction(0x00120000, 5, "call", f"0x{target:x}", "e800000000")
    insn.call_target = target
    return insn


def test_wrapped_function_is_called_by_its_wrapper_name():
    func_db = {TARGET: {"name": "sub_001DFB40_gen", "call_name": "sub_001DFB40"}}
    generated = "\n".join(Lifter(func_db=func_db).lift_instruction(_direct_call()))

    assert "sub_001DFB40" in generated
    assert "sub_001DFB40_gen" not in generated


def test_unwrapped_function_is_called_by_its_name():
    func_db = {TARGET: {"name": "sub_001DFB40"}}
    generated = "\n".join(Lifter(func_db=func_db).lift_instruction(_direct_call()))

    assert "sub_001DFB40" in generated
    assert "_gen" not in generated
