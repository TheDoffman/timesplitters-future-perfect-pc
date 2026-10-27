"""
Self-check for backwards jump tables whose first arm is the displacement.

Run: py -3 tools/recomp/test_jump_table_backward_self.py

MSVC's memcpy dispatches `jmp [ecx*4 + disp]` with ecx -4..-1, so disp is the
table's END. Max Payne's table at 0x2C67D8 ends at 0x2C67E8, and its first
entry is 0x2C67E8 itself: the "copy 0 more bytes" arm right after the table.
The backward scan treated any word equal to disp as the jump's own
displacement and stopped, so that arm was never lifted, and the path through
it left memcpy through an unresolved jump.

The backstop still has a real job: a backwards table inline after its jump
has the jump's displacement right below its first entry, and that word equals
disp too. What tells them apart is the FF 24 <SIB> in front of it.
"""

import os
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))

from tools.recomp import config  # noqa: E402
from tools.recomp.translator import FunctionTranslator  # noqa: E402

BASE = 0x00010000


def _setup(image):
    config._install(
        [config.Section(".text", BASE, len(image), 0x0000, len(image), True)],
        entry_point=BASE, kernel_thunk_addr=BASE, origin="backward-self-test")
    return image


def _translate(image):
    db = {BASE: {"start": f"0x{BASE:08X}", "end": BASE + len(image),
                 "_addr": BASE, "size": len(image)}}
    ft = FunctionTranslator(image, db)
    return ft.translate_function(BASE, db[BASE])


def test_an_arm_equal_to_the_displacement_is_kept():
    """
    0x00  FF 24 8D <disp32>    jmp [ecx*4 + BASE+0x18]     ecx is -4..-1
    0x07  8B                   a byte that is not part of a jmp opcode
    0x08  table: arms 0x18, 0x19, 0x1A, 0x1B                (to 0x18)
    0x18  C3 C3 C3 C3

    The first entry, at 0x08, equals disp and is preceded by 00 00 8B: a
    real arm.
    """
    disp = BASE + 0x18
    code = bytes.fromhex("FF248D") + struct.pack("<I", disp)   # 0x00..0x07
    code += bytes.fromhex("8B")                                # 0x07
    arms = [BASE + 0x18, BASE + 0x19, BASE + 0x1A, BASE + 0x1B]
    code += b"".join(struct.pack("<I", a) for a in arms)       # 0x08..0x18
    code += bytes.fromhex("C3C3C3C3")                          # 0x18..0x1C
    image = _setup(code)
    # The premise: the first entry equals the displacement.
    assert struct.unpack_from("<I", image, 0x08)[0] == disp
    c = _translate(image)
    assert "switch:" in c, c
    for arm in arms:
        assert f"if (_jt == 0x{arm:08X}u)" in c, (hex(arm), c)


def test_the_jumps_own_displacement_is_not_an_arm():
    """
    0x00  FF 24 8D <disp32>    jmp [ecx*4 + BASE+0x13]
    0x07  table: arms 0x13, 0x14, 0x15                      (to 0x13)
    0x13  C3 C3 C3

    The word below the first entry (0x03..0x07) is the displacement, value
    0x13, and it is preceded by FF 24 8D: it belongs to the jump.
    """
    disp = BASE + 0x13
    code = bytes.fromhex("FF248D") + struct.pack("<I", disp)   # 0x00..0x07
    arms = [BASE + 0x13, BASE + 0x14, BASE + 0x15]
    code += b"".join(struct.pack("<I", a) for a in arms)       # 0x07..0x13
    code += bytes.fromhex("C3C3C3")
    image = _setup(code)
    c = _translate(image)
    assert "switch:" in c, c
    assert "switch: 3 entries" in c, c


if __name__ == "__main__":
    for name, fn in list(globals().items()):
        if name.startswith("test_"):
            fn()
            print("ok", name)
