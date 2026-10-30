"""
Self-check: a re-sync drops every out-of-phase decode it covers.

Run: py -3 tools/recomp/test_resync_overlap.py

The translator decodes a function with one linear sweep and re-syncs at the
switch arms the sweep stepped over. It used to discard only the instruction
straddling the arm, so the rest of the out-of-phase run -- junk decoded from the
table bytes onward, overlapping the real instructions after the arm -- stayed
in the list. Basic blocks are built in address order, so that junk was emitted
between two real instructions and ran.

The shape is MSVC's memcpy tail dispatch: `jmp [edx*4 + table]`, a four-entry
table, then the cases, the first being `mov eax, [ebp+8]; pop esi; pop edi;
leave; ret`. At this base the sweep leaves the table two bytes into that mov
and decodes `or byte ptr [esi+0x5F], bl` over its last byte and the pops.
Hunter: The Reckoning's memcpy (sub_001EB9A0) lifted exactly that, and every
copy ending in that case OR-ed a byte into memory 0x5F past its source.
"""

import os
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))

import capstone  # noqa: E402

from tools.recomp import config  # noqa: E402
from tools.recomp.translator import FunctionTranslator  # noqa: E402

BASE = 0x00010040            # the table's bytes put the sweep two bytes into case 0
TABLE = BASE + 0x09
CASE0 = BASE + 0x19
CASE1 = CASE0 + 8
CASE2 = CASE1 + 12
CASE3 = CASE2 + 20


def _image():
    """
    +0x00  FF 24 95 <TABLE>     jmp dword ptr [edx*4 + TABLE]
    +0x07  8B FF                mov edi, edi (padding)
    +0x09  TABLE: CASE0..CASE3
    +0x19  CASE0: mov eax,[ebp+8]; pop esi; pop edi; leave; ret; nop
    +0x21  CASE1: one byte copied, then the same exit
    +0x2D  CASE2: two bytes copied, then the same exit
    +0x41  CASE3: three bytes copied, then the same exit
    """
    code = bytearray()
    code += bytes.fromhex("FF2495") + struct.pack("<I", TABLE)
    code += bytes.fromhex("8BFF")
    code += struct.pack("<IIII", CASE0, CASE1, CASE2, CASE3)
    code += bytes.fromhex("8B45085E5FC9C390")
    code += bytes.fromhex("8A0688078B45085E5FC9C390")
    code += bytes.fromhex("8A0688078A4601884701" "8B45085E5FC9C38D4900")
    code += bytes.fromhex("8A0688078A4601884701" "8A4602884702" "8B45085E5FC9C3")
    assert BASE + len(code) == CASE3 + 23
    return bytes(code)


def _setup(image):
    config._install(
        [config.Section(".text", BASE, len(image), 0x0000, len(image), True)],
        entry_point=BASE, kernel_thunk_addr=BASE, origin="resync-overlap-test")
    return image


def test_premise_sweep_is_out_of_phase():
    # Without a re-sync the stream has no instruction at case 0 and a junk
    # one two bytes into it.
    cs = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    swept = {i.address: i for i in cs.disasm(_image(), BASE)}
    assert CASE0 not in swept, sorted(hex(a) for a in swept)
    junk = swept.get(CASE0 + 2)
    assert junk is not None and junk.mnemonic == "or", junk
    print("ok  premise_sweep_is_out_of_phase")


def test_resync_drops_covered_junk():
    image = _setup(_image())
    db = {BASE: {"start": f"0x{BASE:08X}", "end": BASE + len(image),
                 "_addr": BASE, "size": len(image)}}
    ft = FunctionTranslator(image, db)
    insns = ft.disasm.disassemble_function(
        image, BASE, BASE + len(image), resync={CASE0, CASE1, CASE2, CASE3})
    have = {i.address: i for i in insns}
    assert CASE0 in have, sorted(hex(a) for a in have)
    assert CASE0 + 2 not in have, "junk decode kept inside case 0"
    # Case 0 as the CPU runs it, every instruction where the last one ended.
    a = CASE0
    for want in ("mov", "pop", "pop", "leave", "ret"):
        assert a in have and have[a].mnemonic == want, (hex(a), want)
        a = have[a].end_address
    c = ft.translate_function(BASE, db[BASE])
    assert f"loc_{CASE0:08X}:" in c, c
    assert "esi + 0x5F" not in c, c
    print("ok  resync_drops_covered_junk")


if __name__ == "__main__":
    test_premise_sweep_is_out_of_phase()
    test_resync_drops_covered_junk()
    print("\nall passed")
