"""
Hold src/kernel/nv2a_vsh.c's microcode field table to a known-good program.

    python3 -m pytest tools/vsh_audit/test_vsh_encoding.py

The NV2A vertex program encoding is packed into four dwords with no slack, and
a field table that is wrong does not fail loudly: it decodes to *some* program,
and the shader that comes out is garbage with no error anywhere. The previous
table placed the opcodes in dword0, which is zero in every instruction of every
program observed -- so every instruction read as MAC=NOP, ILU=NOP, and both the
HLSL generator and the CPU interpreter produced nothing while reporting success.

This test parses the VSH_FIELD_* constants straight out of the C source and
decodes Burnout 2's frontend shader with them, so the constants themselves are
what is under test. It is the same technique tools/kernel_audit uses against
kernel_bridge.c.

Ground truth is docs/technical/nv2a-vertex-program-encoding.md, whose
disassembly was produced by abaire/nv2a_vsh_asm -- an independent assembler for
this instruction set -- and is the canonical Xbox pass-through shader:

     0  MOV R1.xyzw, v0
     1  MOV oD0.xyzw, v3      + RCP R1.w, R1.w
     2  RCP oFog.xyzw, v0.w
     3  MUL R2.xyzw, R1, c[0] + MOV oD1.xyzw, v4
     4  ADD oPos.xyzw, R2, c[1]
     5  MOV oPts.xyzw, v1.x
     6  MOV oB0.xyzw, v7          9  MOV oT1.xyzw, v10
     7  MOV oB1.xyzw, v8         10  MOV oT2.xyzw, v11
     8  MOV oT0.xyzw, v9         11  MOV oT3.xyzw, v12
"""

import os
import re

import pytest

ROOT = os.path.join(os.path.dirname(__file__), "..", "..")
VSH_C = os.path.join(ROOT, "src", "kernel", "nv2a_vsh.c")

# Burnout 2's frontend program, as RECOMP_VP_DUMP wrote it.
PROGRAM = [
    (0x00000000, 0x0020001B, 0x0836106C, 0x2F100FF8),
    (0x00000000, 0x0420061B, 0x083613FC, 0x5011F818),
    (0x00000000, 0x0400001B, 0x083613FC, 0x2070F82C),
    (0x00000000, 0x0240081B, 0x1436186C, 0x2F20F824),
    (0x00000000, 0x0060201B, 0x2436106C, 0x3070F800),
    (0x00000000, 0x00200200, 0x0836106C, 0x2070F830),
    (0x00000000, 0x00200E1B, 0x0836106C, 0x2070F838),
    (0x00000000, 0x0020101B, 0x0836106C, 0x2070F840),
    (0x00000000, 0x0020121B, 0x0836106C, 0x2070F848),
    (0x00000000, 0x0020141B, 0x0836106C, 0x2070F850),
    (0x00000000, 0x0020161B, 0x0836106C, 0x2070F858),
    (0x00000000, 0x0020181B, 0x0836106C, 0x2070F861),
]

MAC_OPS = ("NOP", "MOV", "MUL", "ADD", "MAD", "DP3", "DPH", "DP4",
           "DST", "MIN", "MAX", "SLT", "SGE", "ARL")
ILU_OPS = ("NOP", "MOV", "RCP", "RCC", "RSQ", "EXP", "LOG", "LIT")
OUT_REGS = {0: "oPos", 3: "oD0", 4: "oD1", 5: "oFog", 6: "oPts", 7: "oB0",
            8: "oB1", 9: "oT0", 10: "oT1", 11: "oT2", 12: "oT3", 0xFF: None}

# Banks. Zero is not one; reading a bank of 0 means the table is displaced.
BANK_TEMP, BANK_INPUT, BANK_CONST = 1, 2, 3

# (mac, ilu, input, const, output, ilu_drives_output, temp, mac_mask,
#  ilu_mask, out_mask, final)
EXPECTED = [
    ("MOV", "NOP",  0, 0, None,   0,  1, 0xF, 0x0, 0x0, 0),
    ("MOV", "RCP",  3, 0, "oD0",  0,  1, 0x0, 0x1, 0xF, 0),
    ("NOP", "RCP",  0, 0, "oFog", 1,  7, 0x0, 0x0, 0xF, 0),
    ("MUL", "MOV",  4, 0, "oD1",  1,  2, 0xF, 0x0, 0xF, 0),
    ("ADD", "NOP",  0, 1, "oPos", 0,  7, 0x0, 0x0, 0xF, 0),
    ("MOV", "NOP",  1, 0, "oPts", 0,  7, 0x0, 0x0, 0xF, 0),
    ("MOV", "NOP",  7, 0, "oB0",  0,  7, 0x0, 0x0, 0xF, 0),
    ("MOV", "NOP",  8, 0, "oB1",  0,  7, 0x0, 0x0, 0xF, 0),
    ("MOV", "NOP",  9, 0, "oT0",  0,  7, 0x0, 0x0, 0xF, 0),
    ("MOV", "NOP", 10, 0, "oT1",  0,  7, 0x0, 0x0, 0xF, 0),
    ("MOV", "NOP", 11, 0, "oT2",  0,  7, 0x0, 0x0, 0xF, 0),
    ("MOV", "NOP", 12, 0, "oT3",  0,  7, 0x0, 0x0, 0xF, 1),
]


def fields():
    """The VSH_FIELD_* constants, read from the C source."""
    with open(VSH_C, encoding="utf-8", errors="replace") as fh:
        src = fh.read()
    f = {m.group(1): int(m.group(2))
         for m in re.finditer(r"#define\s+(VSH_FIELD_\w+)\s+(\d+)", src)}
    assert f, "no VSH_FIELD_* constants found in " + VSH_C
    return f


def extract(words, start, count):
    """The same bit extraction vsh_extract() does, LSB-first per dword."""
    idx, ofs = start // 32, start % 32
    value = words[idx] >> ofs
    if ofs + count > 32:
        value |= words[idx + 1] << (32 - ofs)
    return value & ((1 << count) - 1)


@pytest.fixture(scope="module")
def F():
    return fields()


def test_dword0_carries_no_fields(F):
    """Every field lives at bit 32 or above.

    This is the specific failure the old table had, and it is worth its own
    test: dword0 is zero in every instruction, so anything read from it is
    silently zero rather than wrong-looking.
    """
    low = sorted((n, v) for n, v in F.items()
                 if n.endswith("_START") and v < 32)
    assert not low, "fields placed in the always-zero dword0: %r" % (low,)


def test_no_field_overlaps_another(F):
    """Sized fields must not share bits.

    The old table had MAC_DST_OUT spanning bits 70-77 with MAC_DST_MASK
    (72-75) and MAC_DST_TEMP (76-79) inside it, and REL_ADDR at bit 109
    inside ILU_DST_TEMP (108-111). A table that overlaps itself cannot be
    partially right.
    """
    sizes = {
        "VSH_FIELD_INPUT_IDX_START": 4, "VSH_FIELD_CONST_IDX_START": 8,
        "VSH_FIELD_MAC_OP_START": 4, "VSH_FIELD_ILU_OP_START": 3,
        "VSH_FIELD_OUT_ADDRESS_START": 8, "VSH_FIELD_OUT_O_MASK_START": 4,
        "VSH_FIELD_OUT_ILU_MASK_START": 4, "VSH_FIELD_OUT_TEMP_START": 4,
        "VSH_FIELD_OUT_MAC_MASK_START": 4, "VSH_FIELD_SRC_A_TEMP_START": 4,
        "VSH_FIELD_SRC_B_TEMP_START": 4, "VSH_FIELD_SRC_A_MUX_START": 2,
        "VSH_FIELD_SRC_B_MUX_START": 2, "VSH_FIELD_SRC_C_MUX_START": 2,
        "VSH_FIELD_SRC_C_TEMP_HI_START": 2, "VSH_FIELD_SRC_C_TEMP_LO_START": 2,
        "VSH_FIELD_FINAL_START": 1, "VSH_FIELD_A0X_START": 1,
        "VSH_FIELD_OUT_MUX_START": 1, "VSH_FIELD_OUT_ORB_START": 1,
        "VSH_FIELD_SRC_A_NEG_START": 1, "VSH_FIELD_SRC_B_NEG_START": 1,
        "VSH_FIELD_SRC_C_NEG_START": 1,
    }
    for axis in ("A", "B", "C"):
        for chan in ("X", "Y", "Z", "W"):
            sizes["VSH_FIELD_SRC_%s_SWZ_%s_START" % (axis, chan)] = 2

    owner = {}
    clashes = []
    for name, size in sizes.items():
        assert name in F, "missing field constant " + name
        for bit in range(F[name], F[name] + size):
            if bit in owner:
                clashes.append("bit %d: %s and %s" % (bit, owner[bit], name))
            owner[bit] = name
    assert not clashes, "\n  ".join(clashes)


def decode(F, words):
    c_temp = ((extract(words, F["VSH_FIELD_SRC_C_TEMP_HI_START"], 2) << 2)
              | extract(words, F["VSH_FIELD_SRC_C_TEMP_LO_START"], 2))
    return {
        "mac": MAC_OPS[extract(words, F["VSH_FIELD_MAC_OP_START"], 4)],
        "ilu": ILU_OPS[extract(words, F["VSH_FIELD_ILU_OP_START"], 3)],
        "input": extract(words, F["VSH_FIELD_INPUT_IDX_START"], 4),
        "const": extract(words, F["VSH_FIELD_CONST_IDX_START"], 8),
        "out": OUT_REGS.get(
            extract(words, F["VSH_FIELD_OUT_ADDRESS_START"], 8), "?"),
        "ilu_drives_out": extract(words, F["VSH_FIELD_OUT_MUX_START"], 1),
        "temp": extract(words, F["VSH_FIELD_OUT_TEMP_START"], 4),
        "mac_mask": extract(words, F["VSH_FIELD_OUT_MAC_MASK_START"], 4),
        "ilu_mask": extract(words, F["VSH_FIELD_OUT_ILU_MASK_START"], 4),
        "out_mask": extract(words, F["VSH_FIELD_OUT_O_MASK_START"], 4),
        "final": extract(words, F["VSH_FIELD_FINAL_START"], 1),
        "a_bank": extract(words, F["VSH_FIELD_SRC_A_MUX_START"], 2),
        "a_temp": extract(words, F["VSH_FIELD_SRC_A_TEMP_START"], 4),
        "c_temp": c_temp,
    }


def test_the_frontend_program_decodes_to_its_disassembly(F):
    for i, (words, want) in enumerate(zip(PROGRAM, EXPECTED)):
        got = decode(F, words)
        assert (got["mac"], got["ilu"], got["input"], got["const"],
                got["out"], got["ilu_drives_out"], got["temp"],
                got["mac_mask"], got["ilu_mask"], got["out_mask"],
                got["final"]) == want, (
            "instruction %d decoded as %r, expected %r" % (i, got, want))


def test_exactly_one_instruction_is_final(F):
    finals = [i for i, w in enumerate(PROGRAM)
              if extract(w, F["VSH_FIELD_FINAL_START"], 1)]
    assert finals == [11], (
        "a valid program ends once; FINAL set on %r" % (finals,))


def test_source_banks_are_one_two_three(F):
    """Never bank 0, and the two temp reads name the registers just written."""
    seen = set()
    for words in PROGRAM:
        for axis in ("A", "B", "C"):
            seen.add(extract(words, F["VSH_FIELD_SRC_%s_MUX_START" % axis], 2))
    assert 0 not in seen, (
        "bank 0 does not exist on this hardware; reading one means the "
        "table is displaced (banks seen: %r)" % (sorted(seen),))

    # Instruction 3 is "MUL R2, R1, c[0]" and 4 is "ADD oPos, R2, c[1]", so
    # source A is a temp naming R1 then R2 -- the registers instructions 0
    # and 3 wrote. Under the old zero-based banking both read as inputs.
    for insn, temp in ((3, 1), (4, 2)):
        bank = extract(PROGRAM[insn], F["VSH_FIELD_SRC_A_MUX_START"], 2)
        idx = extract(PROGRAM[insn], F["VSH_FIELD_SRC_A_TEMP_START"], 4)
        assert (bank, idx) == (BANK_TEMP, temp), (
            "instruction %d source A should be temp R%d, got bank %d index %d"
            % (insn, temp, bank, idx))


def test_the_rcp_writes_only_w(F):
    """Instruction 1 is "RCP R1.w, R1.w" -- the ILU mask is w alone.

    It is the one instruction in the program with a partial write mask, which
    is evidence that MAC and ILU carry separate masks. It is not evidence of a
    shared temp index: the slot is paired, and a paired ILU writes R1 whatever
    OUT_TEMP says, so R1 is the answer either way.
    """
    words = PROGRAM[1]
    assert extract(words, F["VSH_FIELD_OUT_ILU_MASK_START"], 4) == 0x1
    assert extract(words, F["VSH_FIELD_OUT_MAC_MASK_START"], 4) == 0x0
    assert extract(words, F["VSH_FIELD_OUT_TEMP_START"], 4) == 1
