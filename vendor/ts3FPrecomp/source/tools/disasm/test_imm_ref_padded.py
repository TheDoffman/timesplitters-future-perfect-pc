"""
Self-check for padded entries in FunctionDetector._pass_imm_ref_targets.

Run: py -3 tools/disasm/test_imm_ref_padded.py

An immediate-referenced target normally has to reach a ret within the strict
probe's window, with no jmp on the way. Real callbacks fail that -- Burnout 2's
0x000BFAD0 first returns after 77 instructions, and 0x00115140 tail-calls
through `jmp eax` first -- so a target laid out exactly like a function start
(16-byte aligned, straight after int3/nop padding) is accepted on the probe's
full reach instead.
"""

import os
import sys
from types import SimpleNamespace

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))

from tools.disasm.engine import DisasmEngine  # noqa: E402
from tools.disasm.functions import FunctionDetector  # noqa: E402
from tools.disasm.test_alias_tail_jumps import _Img  # noqa: E402
from tools.disasm.test_decode_at import _Section  # noqa: E402

BASE = 0x00010000
LONG = b"\x89\xc0" * 70 + b"\xc3"      # 70 x mov eax,eax; ret -- past the window


def _detector(target_off, before):
    """
    BASE+0x00  the installer:  mov dword [esi+0x10], BASE+target_off; ret
    BASE+target_off - 1        `before`
    BASE+target_off            a long body
    """
    code = bytearray(b"\xcc" * 0x200)
    target = BASE + target_off
    code[0x00:0x07] = b"\xc7\x46\x10" + target.to_bytes(4, "little")
    code[0x07] = 0xc3
    code[target_off - 1] = before
    code[target_off:target_off + len(LONG)] = LONG
    code = bytes(code)
    sec = _Section(BASE, code)
    sec.data = code
    engine = DisasmEngine(_Img(sec))
    engine.linear_sweep(sec)
    det = FunctionDetector.__new__(FunctionDetector)
    det.engine = engine
    det.image = engine.image
    det._candidates = {}
    det._alias_entries = {}
    det.functions = {BASE: SimpleNamespace(start=BASE, end=BASE + 8)}
    return det, sec, target


def test_an_aligned_target_after_padding_is_found():
    det, sec, target = _detector(0x40, 0xCC)
    assert not det.engine.probes_as_returning_body(target)   # the strict probe says no
    det._pass_imm_ref_targets([sec])
    assert target in det._candidates, det._candidates


def test_nop_padding_counts_too():
    det, sec, target = _detector(0x40, 0x90)
    det._pass_imm_ref_targets([sec])
    assert target in det._candidates, det._candidates


def test_an_unaligned_target_is_not():
    det, sec, target = _detector(0x41, 0xCC)
    det._pass_imm_ref_targets([sec])
    assert target not in det._candidates, det._candidates


def test_a_target_after_code_is_not():
    # Aligned, but straight after a ret rather than padding.
    det, sec, target = _detector(0x40, 0xC3)
    det._pass_imm_ref_targets([sec])
    assert target not in det._candidates, det._candidates


if __name__ == "__main__":
    for name, fn in list(globals().items()):
        if name.startswith("test_"):
            fn()
            print("ok", name)
