"""
Self-check for FunctionDetector._pass_alias_tail_jumps.

Run: py -3 tools/disasm/test_alias_tail_jumps.py

A C++ static initialiser is an entry in the CRT's initialiser table shaped
`mov ecx, <object>; jmp <constructor>`. The initialiser is found as an alias
entry by the data-pointer pass, which runs after the last tail-jump round -- so
its jump was never followed, and the constructor lifted as a stub that pops the
return address and does nothing. Burnout 2's sound manager was built that way
and never constructed.
"""

import os
import sys
from types import SimpleNamespace

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))

from tools.disasm.engine import DisasmEngine  # noqa: E402
from tools.disasm.functions import FunctionDetector  # noqa: E402
from tools.disasm.test_decode_at import _Image, _Section  # noqa: E402

BASE = 0x00010000


class _Img(_Image):
    """_Image plus the byte read entry_pops_unsaved needs."""
    def read_bytes_at_va(self, va, n):
        s = self.get_section_at_va(va)
        if s is None:
            return b""
        off = va - s.virtual_addr
        return s.data[off:off + n]


def _rel32(src_end, target):
    return (target - src_end).to_bytes(4, "little", signed=True)


def _layout(target_body):
    """
    BASE+0x00  a real function:  push esi; mov esi, ecx; pop esi; ret
    BASE+0x10  the initialiser:  mov ecx, 0x12345678; jmp BASE+0x30
    BASE+0x30  the jump target, in a gap -- `target_body`
    BASE+0x50  a real function:  ret
    """
    code = bytearray(b"\xcc" * 0x60)
    code[0x00:0x05] = b"\x56\x8b\xf1\x5e\xc3"
    code[0x10:0x15] = b"\xb9\x78\x56\x34\x12"
    code[0x15:0x1a] = b"\xe9" + _rel32(BASE + 0x1a, BASE + 0x30)
    code[0x30:0x30 + len(target_body)] = target_body
    code[0x50] = 0xc3
    return bytes(code)


def _detector(code):
    sec = _Section(BASE, code)
    sec.data = code
    image = _Img(sec)
    engine = DisasmEngine(image)
    engine.linear_sweep(sec)
    det = FunctionDetector.__new__(FunctionDetector)
    det.engine = engine
    det.image = image
    det._candidates = {}
    det._alias_entries = {BASE + 0x10: BASE + 0x1a}
    det.functions = {
        BASE: SimpleNamespace(start=BASE, end=BASE + 5),
        BASE + 0x50: SimpleNamespace(start=BASE + 0x50, end=BASE + 0x51),
    }
    return det, sec


def test_a_constructor_reached_from_an_initialiser_is_found():
    # mov eax, ecx; mov dword [eax+4], 0; ret
    det, sec = _detector(_layout(b"\x8b\xc1\xc7\x40\x04\x00\x00\x00\x00\xc3"))
    added = det._pass_alias_tail_jumps([sec])
    assert added == 1, added
    assert det._alias_entries.get(BASE + 0x30) == BASE + 0x3a, \
        {hex(k): hex(v) for k, v in det._alias_entries.items()}


def test_a_continuation_is_not_an_entry():
    # pop esi; ret -- restores a register it never saved, so it is the rest of
    # the jumping function rather than a function of its own
    det, sec = _detector(_layout(b"\x5e\xc3"))
    assert det._pass_alias_tail_jumps([sec]) == 0
    assert BASE + 0x30 not in det._alias_entries


def test_a_jump_into_a_function_shares_its_end():
    # Aim the initialiser's jmp into the middle of the first function instead.
    code = bytearray(_layout(b"\xc3"))
    code[0x15:0x1a] = b"\xe9" + _rel32(BASE + 0x1a, BASE + 0x01)
    det, sec = _detector(bytes(code))
    assert det._pass_alias_tail_jumps([sec]) == 1
    assert det._alias_entries.get(BASE + 0x01) == BASE + 5


def test_a_loop_back_from_an_alias_gets_an_entry():
    # An alias inside the first function whose body jumps back to a label
    # before the alias -- a loop head. The host lifts that as a goto, but the
    # alias is lifted as its own copy, where the label is outside the body and
    # the jump becomes a tail call. So the label needs an entry of its own,
    # sharing the host's end.
    #   BASE+0x00  push esi; mov esi, ecx; [alias at +3] jmp BASE+1
    code = bytearray(_layout(b"\xc3"))
    code[0x00:0x08] = b"\x56\x8b\xf1\xe9" + _rel32(BASE + 0x08, BASE + 0x01)
    det, sec = _detector(bytes(code))
    det.functions[BASE] = SimpleNamespace(start=BASE, end=BASE + 8)
    det._alias_entries = {BASE + 3: BASE + 8}
    assert det._pass_alias_tail_jumps([sec]) == 1, det._alias_entries
    assert det._alias_entries.get(BASE + 1) == BASE + 8


def test_a_real_function_jumping_into_another_gets_an_entry():
    # No aliases at all: a function found late (by immediate) jumps into the
    # middle of another function. The tail-jump rounds ran before it existed,
    # so this pass is the one that has to follow the jump.
    #   BASE+0x00  host:   push esi; mov esi, ecx; pop esi; ret
    #   BASE+0x50  jumper: jmp BASE+3
    code = bytearray(_layout(b"\xc3"))
    code[0x50:0x55] = b"\xe9" + _rel32(BASE + 0x55, BASE + 0x03)
    det, sec = _detector(bytes(code))
    det._alias_entries = {}
    det.functions[BASE + 0x50] = SimpleNamespace(start=BASE + 0x50, end=BASE + 0x55)
    assert det._pass_alias_tail_jumps([sec]) >= 1, det._alias_entries
    assert det._alias_entries.get(BASE + 3) == BASE + 5


def test_a_conditional_jump_into_a_function_gets_an_entry():
    # A conditional branch out of a body lifts like a tail jmp, so a label it
    # lands on inside another function needs an entry too. Burnout 2's
    # split-off loop tail sub_000C0A07 closes its loop with `jne 0x000C0960`
    # back into sub_000C08F0; without an entry that was an empty stub.
    #   BASE+0x00  host:   push esi; mov esi, ecx; pop esi; ret
    #   BASE+0x50  jumper: jne BASE+3; ret
    code = bytearray(_layout(b"\xc3"))
    code[0x50:0x57] = b"\x0f\x85" + _rel32(BASE + 0x56, BASE + 0x03) + b"\xc3"
    det, sec = _detector(bytes(code))
    det._alias_entries = {}
    det.functions[BASE + 0x50] = SimpleNamespace(start=BASE + 0x50, end=BASE + 0x57)
    assert det._pass_alias_tail_jumps([sec]) >= 1, det._alias_entries
    assert det._alias_entries.get(BASE + 3) == BASE + 5


def test_a_conditional_jump_into_a_gap_is_not_an_entry():
    # The same conditional branch aimed at a gap: weaker evidence of a function
    # start than a jmp, so no entry is invented there, even over a body the
    # probe would accept from a jmp (see the first test).
    code = bytearray(_layout(b"\x8b\xc1\xc7\x40\x04\x00\x00\x00\x00\xc3"))
    code[0x50:0x57] = b"\x0f\x85" + _rel32(BASE + 0x56, BASE + 0x30) + b"\xc3"
    det, sec = _detector(bytes(code))
    det._alias_entries = {}
    det.functions[BASE + 0x50] = SimpleNamespace(start=BASE + 0x50, end=BASE + 0x57)
    det._pass_alias_tail_jumps([sec])
    assert BASE + 0x30 not in det._alias_entries, det._alias_entries


if __name__ == "__main__":
    for name, fn in list(globals().items()):
        if name.startswith("test_"):
            fn()
            print("ok", name)
