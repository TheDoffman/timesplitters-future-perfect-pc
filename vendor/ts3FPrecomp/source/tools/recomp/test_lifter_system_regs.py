"""Debug and control registers lift to a stub, not to an undeclared name.

`mov eax, dr0` used to come out of the lifter as `eax = dr0;`. Nothing
declares `dr0`, so the C compiler rejected it -- and because the generated
files are split by size rather than by function, one instruction in one
misidentified region failed a whole translation unit and blocked the whole
title. Panzer Dragoon Orta (XDK 4928) stopped there: two lines in
sub_0021118E, which is data the disassembler walked as code.

The contract these tests pin is the one the segment registers already had:
a read is a defined constant, a write is a no-op, and the generated C says
which register it was.
"""

from tools.recomp.disasm import Instruction, Operand
from tools.recomp.lifter import Lifter


def _reg(name):
    op = Operand("reg")
    op.reg = name
    return op


def _mov(dst, src):
    insn = Instruction(0x1000, 3, "mov", f"{dst}, {src}", "0f21c0")
    insn.operands = [_reg(dst), _reg(src)]
    return insn


def _lift(dst, src):
    return "".join(Lifter().lift_instruction(_mov(dst, src)))


def test_debug_register_read_is_a_defined_zero():
    body = _lift("eax", "dr0")
    assert "dr0" not in body.split("/*")[0]
    assert body.startswith("eax = 0")
    assert "dr0" in body


def test_debug_register_write_is_a_no_op():
    body = _lift("dr7", "eax")
    assert body.lstrip().startswith("/*")
    assert "dr7" in body
    assert "dr7 =" not in body


def test_control_register_read_is_a_defined_zero():
    body = _lift("eax", "cr0")
    assert body.startswith("eax = 0")
    assert "cr0" in body


def test_control_register_write_is_a_no_op():
    body = _lift("cr4", "eax")
    assert body.lstrip().startswith("/*")
    assert "cr4 =" not in body


def test_no_system_register_reaches_the_output_as_an_identifier():
    # Capstone names dr0-dr15 and cr0-cr15; every one of them has to stub,
    # not just the two Panzer Dragoon Orta happened to hit.
    for i in range(16):
        for prefix in ("dr", "cr"):
            name = f"{prefix}{i}"
            read = _lift("eax", name)
            assert read.startswith("eax = 0"), (name, read)
            write = _lift(name, "eax")
            assert f"{name} = " not in write, (name, write)


def test_ordinary_registers_still_move():
    assert _lift("eax", "ecx").startswith("eax = ecx")
