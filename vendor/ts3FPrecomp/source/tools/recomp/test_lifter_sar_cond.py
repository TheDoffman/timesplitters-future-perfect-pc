"""
Self-check for signed conditions after sar.

Run: py -3 tools/recomp/test_lifter_sar_cond.py

MSVC's memcpy counts its 32-byte blocks with `sar edx, 5; jle tail`. jle had
no rule after a shift and fell back to `_flags`, which is always 0, so the
block loop ran once for every copy under 32 bytes. Max Payne copied 12 bytes
of indices as 32 and overran into the next heap block, whose free-list walk
then spun forever.
"""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))

from tools.recomp.lifter import _make_condition  # noqa: E402


class _Op:
    def __init__(self, reg=None, type="reg", size=4, imm=0):
        self.type = type
        self.reg = reg
        self.size = size
        self.mem_size = None
        self.imm = imm


OPS = [_Op("edx"), _Op(type="imm", imm=5)]


def _x86(jcc, result):
    # After sar, OF = 0, so SF != OF is just SF.
    sf, zf = result < 0, result == 0
    return {"jl": sf, "jge": not sf, "jle": zf or sf, "jg": not zf and not sf}[jcc]


def test_sar_signed_conditions_have_a_rule():
    for jcc in ("jl", "jge", "jle", "jg"):
        got = _make_condition(jcc, "sar", OPS)
        assert got is not None, jcc
        assert "_flags" not in got[0], got


def test_sar_signed_conditions_match_the_hardware():
    for size in (0, 1, 12, 31, 32, 33, 64, -1, -32, -64):
        result = size >> 5              # Python >> on int is arithmetic, as sar
        for jcc in ("jl", "jge", "jle", "jg"):
            expr = _make_condition(jcc, "sar", OPS)[0]
            got = eval(expr.replace("||", " or ").replace("&&", " and "),
                       {"_fas": result})
            assert bool(got) == _x86(jcc, result), (size, jcc, expr)


def test_the_memcpy_tail_branch_is_taken_under_32_bytes():
    expr = _make_condition("jle", "sar", OPS)[0]
    assert eval(expr, {"_fas": 12 >> 5}), expr       # 12 bytes: skip the loop
    assert not eval(expr, {"_fas": 64 >> 5}), expr   # 64 bytes: run it


def test_shl_and_shr_get_no_invented_rule():
    # Their OF is not 0, so the signed conditions are not the result's sign.
    for setter in ("shl", "shr"):
        assert _make_condition("jle", setter, OPS) is None, setter


if __name__ == "__main__":
    for name, fn in list(globals().items()):
        if name.startswith("test_"):
            fn()
            print("ok", name)
