"""
Self-check for indexed jump tables whose low slots are never used.

Run: py -3 tools/recomp/test_jump_table_leading_slots.py

MSVC's memcpy dispatches its trailing bytes with `and eax, 3` on a count it
already knows is non-zero, then `jmp [eax*4 + disp]`. eax is 1..3, so slot 0 is
never read, and the compiler lets it overlap the bytes of the jump itself. The
lifter's table reader stopped at that slot, found nothing below the
displacement, and lifted the dispatch as RECOMP_ITAIL. Its arms are inside
memcpy rather than function starts, so they failed to resolve at runtime:
Burnout 2 stalled loading its first race on 0x0011F04C and 0x0011F1F8.

The synthetic image reproduces that shape: the table's slot 0 is the tail of
the jump's own displacement plus padding, slots 1..3 name three arms inside the
function.
"""

import os
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))

from tools.recomp import config  # noqa: E402
from tools.recomp.translator import FunctionTranslator  # noqa: E402

BASE = 0x00010000


def _setup(image):
    # One code section, VA BASE -> file offset 0, so byte n is at VA BASE+n.
    config._install(
        [config.Section(".text", BASE, len(image), 0x0000, len(image), True)],
        entry_point=BASE, kernel_thunk_addr=BASE, origin="leading-slot-test")
    return image


def _image(arm_words):
    """
    0x00  83 E0 03             and eax, 3
    0x03  FF 24 85 <disp32>    jmp [eax*4 + BASE+0x08]
    0x0A  90 90                padding
    0x0C  table slots 1..3     (slot 0 at 0x08 is disp bytes + padding)
    0x18  C3 C3 C3             three one-byte arms
    """
    disp = BASE + 0x08
    code = bytes.fromhex("83E003") + bytes.fromhex("FF2485") + struct.pack("<I", disp)
    code += bytes.fromhex("9090")
    code += b"".join(struct.pack("<I", w) for w in arm_words)
    code += bytes.fromhex("C3C3C3")
    assert len(code) == 0x1B, hex(len(code))
    return code


def _translate(image):
    db = {BASE: {"start": f"0x{BASE:08X}", "end": BASE + len(image),
                 "_addr": BASE, "size": len(image)}}
    ft = FunctionTranslator(image, db)
    return ft.translate_function(BASE, db[BASE])


def test_unused_leading_slot_still_lifts_as_a_switch():
    arms = [BASE + 0x18, BASE + 0x19, BASE + 0x1A]
    image = _setup(_image(arms))
    # Slot 0 really is not an address: the premise of the test.
    slot0 = struct.unpack_from("<I", image, 0x08)[0]
    assert not (BASE <= slot0 < BASE + len(image)), hex(slot0)

    c = _translate(image)
    assert "switch:" in c, c
    for arm in arms:
        assert f"0x{arm:08X}u" in c, (hex(arm), c)
    assert "RECOMP_ITAIL(MEM32(eax * 4" not in c, c
    print("ok  unused_leading_slot_still_lifts_as_a_switch")


def test_no_addresses_after_the_slot_stays_indirect():
    # The same dispatch over words that are not code addresses: skipping
    # slots must not invent a table where there is none.
    image = _setup(_image([0x7F000000, 0x7F000004, 0x7F000008]))
    c = _translate(image)
    assert "switch:" not in c, c
    assert "RECOMP_ITAIL(" in c, c
    print("ok  no_addresses_after_the_slot_stays_indirect")


if __name__ == "__main__":
    test_unused_leading_slot_still_lifts_as_a_switch()
    test_no_addresses_after_the_slot_stays_indirect()
    print("\nall passed")
