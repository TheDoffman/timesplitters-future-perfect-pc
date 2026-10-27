"""
Self-check for CF after repe cmps / repne scas.

Run: py -3 tools/recomp/test_lifter_rep_cmps_cf.py

MSVC's std::string compare is

    xor eax, eax
    repe cmpsb
    je  equal
    sbb eax, eax
    sbb eax, -1        ; -1 if [esi] < [edi] at the mismatch, else +1

The lifted loop set only ZF, so CF kept the 0 from the xor and every
mismatch came out +1. Max Payne's script loader asked its std::set of included
files whether a name was there, got "yes" for every name, and threw
MultipleInclusion on its first include.
"""

import os
import sys
from types import SimpleNamespace

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))

from tools.recomp.lifter import Lifter, _make_condition  # noqa: E402
from tools.recomp.translator import FunctionTranslator  # noqa: E402


def _lift(m, needs_cf):
    lifter = Lifter()
    lifter.needs_cf = needs_cf
    return "\n".join(lifter._lift_rep_string(SimpleNamespace(), m))


def test_the_loop_records_cf_when_a_consumer_needs_it():
    for m, expect in (("repe cmpsb", "_cf = (MEM8(esi) < MEM8(edi));"),
                      ("repne scasb", "_cf = (LO8(eax) < MEM8(edi));"),
                      ("repe cmpsd", "_cf = (MEM32(esi) < MEM32(edi));"),
                      ("repne scasw", "_cf = (LO16(eax) < MEM16(edi));")):
        code = _lift(m, needs_cf=True)
        assert expect in code, (m, code)


def test_no_cf_write_when_nothing_declares_it():
    # _cf is only declared in functions that read it.
    assert "_cf" not in _lift("repe cmpsb", needs_cf=False)


def test_carry_conditions_after_a_rep_compare():
    # Capstone gives repe cmpsb its two memory operands.
    ops = [SimpleNamespace(type="mem", reg=None, size=1, mem_size=1,
                           mem_base="esi", mem_index=None, mem_scale=1,
                           mem_disp=0, mem_segment=None, imm=0),
           SimpleNamespace(type="mem", reg=None, size=1, mem_size=1,
                           mem_base="edi", mem_index=None, mem_scale=1,
                           mem_disp=0, mem_segment="es", imm=0)]
    assert _make_condition("jb", "repe cmpsb", ops)[0] == "_cf"
    assert _make_condition("jae", "repe cmpsb", ops)[0] == "!_cf"
    assert _make_condition("ja", "repe cmpsb", ops)[0] == "(!_cf && _flags == 0)"
    assert _make_condition("jbe", "repe cmpsb", ops)[0] == "(_cf || _flags != 0)"


def test_the_translator_counts_a_rep_compare_as_a_cf_producer():
    insns = [SimpleNamespace(mnemonic="repe cmpsb"),
             SimpleNamespace(mnemonic="jb")]
    assert FunctionTranslator._function_needs_cf(insns)


if __name__ == "__main__":
    for name, fn in list(globals().items()):
        if name.startswith("test_"):
            fn()
            print("ok", name)
