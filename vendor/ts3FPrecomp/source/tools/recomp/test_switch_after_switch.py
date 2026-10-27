"""
Self-check for a function with a second switch table past its first one.

Run: py -3 tools/recomp/test_switch_after_switch.py

The translator decodes a function with one linear sweep and then re-syncs at
the switch-table arms it could not find, because a table embedded in .text
decodes as junk and leaves the stream out of phase across the arm that
follows it. The sweep also stops dead at the first undecodable byte, so a
function with two tables only ever showed its first dispatch to the leader
collection; re-syncing from that table's arms decoded the rest of the
function, including the second dispatch, but its arms were never made
leaders, and an arm sitting right after the second table stayed inside the
junk. TimeSplitters 2's memcpy (sub_001D3340) has five such tables, and two
arms past its fourth lifted as `goto` to a label that did not exist -- so
every copy through them fell to an unresolved indirect call at run time.

The synthetic image reproduces each part of that shape:
  - table A's first word begins 0F 0A, an invalid opcode, so the sweep stops
    inside table A and never reaches dispatch B;
  - table A's second arm is a nop that falls straight into dispatch B, so the
    re-sync from table A's arms decodes dispatch B (as happened in memcpy);
  - table B's first word begins 05, `add eax, imm32`, a five-byte instruction
    that swallows the table's own second arm, the ret right after the table.
So arm B1 is only found when the leaders are collected again after the
re-sync. Without that second round its goto has no label.
"""

import os
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))

from tools.recomp import config  # noqa: E402
from tools.recomp.translator import FunctionTranslator  # noqa: E402

BASE = 0x00010000
SIZE = 0x1000

A_FAR = BASE + 0x0A0F        # arm A0: a ret far away; its word is 0F 0A 01 00
B_FAR = BASE + 0x0105        # arm B0: a ret far away; its word is 05 01 01 00


def _setup(image):
    config._install(
        [config.Section(".text", BASE, len(image), 0x0000, len(image), True)],
        entry_point=BASE, kernel_thunk_addr=BASE, origin="switch-after-switch-test")
    return image


def _image():
    """
    0x00  FF 24 85 <A>         dispatch A over table A at 0x07
    0x07  table A: A_FAR, BASE+0x0F
    0x0F  90                   arm A1: nop, falls into dispatch B
    0x10  FF 24 8D <B>         dispatch B over table B at 0x17
    0x17  table B: B_FAR, BASE+0x1F
    0x1F  C3                   arm B1: ret, right after table B
    """
    code = bytearray(b"\xC3" * SIZE)
    a_tbl = BASE + 0x07
    b_tbl = BASE + 0x17
    code[0x00:0x07] = bytes.fromhex("FF2485") + struct.pack("<I", a_tbl)
    code[0x07:0x0B] = struct.pack("<I", A_FAR)
    code[0x0B:0x0F] = struct.pack("<I", BASE + 0x0F)
    code[0x0F] = 0x90
    code[0x10:0x17] = bytes.fromhex("FF248D") + struct.pack("<I", b_tbl)
    code[0x17:0x1B] = struct.pack("<I", B_FAR)
    code[0x1B:0x1F] = struct.pack("<I", BASE + 0x1F)
    code[0x1F] = 0xC3
    # The premises: 0F 0A at 0x07 is undecodable; 05 at 0x17 starts a 5-byte add.
    assert code[0x07:0x09] == b"\x0F\x0A"
    assert code[0x17] == 0x05
    return bytes(code)


def _translate(image):
    db = {BASE: {"start": f"0x{BASE:08X}", "end": BASE + len(image),
                 "_addr": BASE, "size": len(image)}}
    ft = FunctionTranslator(image, db)
    return ft.translate_function(BASE, db[BASE])


def test_second_table_arms_are_live():
    image = _setup(_image())
    c = _translate(image)
    assert c.count("switch:") == 2, c
    arm = BASE + 0x1F
    assert f"goto loc_{arm:08X};" in c, c
    assert f"loc_{arm:08X}:" in c, c
    assert "dead code, label not in function" not in c, c
    print("ok  second_table_arms_are_live")


if __name__ == "__main__":
    test_second_table_arms_are_live()
    print("\nall passed")
