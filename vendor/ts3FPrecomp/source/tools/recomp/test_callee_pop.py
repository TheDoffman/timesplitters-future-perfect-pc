"""
Self-check for the exact esp check on direct calls (RECOMP_ABI_CALL_POP).

Run: py -3 tools/recomp/test_callee_pop.py

RECOMP_ABI_CALL only knows the callee pops at least its return address, so a
stack that comes back too HIGH passes it. When the callee's own `ret N` is
known from its bytes, the lifter emits RECOMP_ABI_CALL_POP with N, and the
runtime checks esp exactly. Burnout 2's CDirectSoundBuffer_GetStatus came back
8 bytes high once in 4.5 million calls; the one-sided check never said so.
"""

import os
import sys
from unittest import mock

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))

import tools.recomp.lifter as lifter_mod  # noqa: E402
from tools.recomp.lifter import Lifter  # noqa: E402
from tools.recomp.test_call_retaddr import _Insn, _Op  # noqa: E402

BASE = 0x00100000
TARGET = BASE + 0x40


def _lift_call_to(body):
    """Lift `call TARGET` with the callee's bytes set to `body`."""
    image = bytearray(0x100)
    if body is not None:
        image[0x40:0x40 + len(body)] = body
    func_db = {} if body is None else {TARGET: {"end": TARGET + len(body)}}
    lifter = Lifter(func_db=func_db, xbe_data=bytes(image),
                    seh_prolog=0, seh_epilog=0)
    insn = _Insn("call", [_Op(hex(TARGET), type="imm", imm=TARGET)],
                 address=BASE, size=5, call_target=TARGET)
    with mock.patch.object(lifter_mod, "va_to_file_offset",
                           lambda va: va - BASE if BASE <= va < BASE + 0x100
                           else None):
        return "\n".join(lifter.lift_instruction(insn))


def test_a_known_ret_n_gets_the_exact_check():
    out = _lift_call_to(bytes.fromhex("33c0c20800"))       # xor eax,eax; ret 8
    assert f"RECOMP_ABI_CALL_POP(0x{TARGET:08X}u," in out, out
    assert ", 8u);" in out, out


def test_disagreeing_rets_fall_back_to_the_one_sided_check():
    out = _lift_call_to(bytes.fromhex("c20400c20800"))     # ret 4 ... ret 8
    assert "RECOMP_ABI_CALL(" in out, out
    assert "RECOMP_ABI_CALL_POP" not in out, out


def test_an_unknown_callee_keeps_the_one_sided_check():
    out = _lift_call_to(None)
    assert "RECOMP_ABI_CALL(" in out, out
    assert "RECOMP_ABI_CALL_POP" not in out, out


if __name__ == "__main__":
    for name, fn in list(globals().items()):
        if name.startswith("test_"):
            fn()
            print("ok", name)
