"""Self-check that `movsx` sign-extends from every 16-bit source register.

`_lift_movsx` matched its 16-bit source against ("ax", "bx", "cx", "dx",
"si", "di"). `bp` and `sp` were missing, while `_lift_movzx` ten lines above
listed all eight. A register that matched neither list fell through to the
plain operand read -- which is LO16(), and therefore zero-extended -- so
`movsx eax, bp` lifted to `eax = LO16(ebp);` and the sign was dropped with no
diagnostic anywhere.

What that costs, measured in TimeSplitters 2: its pad handler reads the four
thumbstick axes out of the XINPUT_STATE it has just filled, and the compiler
parked exactly one of them, right-stick X, in `bp`. -32767 is 0x8001, which
zero-extends to +32769, so a left push read as a hard right push. The title's
own deadzone made it worse rather than absorbing it: the deadzone is an abs
(cdq/xor/sub) followed by `cmp eax, 0x20`, and on a value that is never
negative the cdq yields 0, the abs is a no-op, and a stick resting one unit
below centre (0xFFFF = 65535) cleared the threshold as maximum deflection. The
axis saturated and the right stick was unusable. Right-stick Y went through
`bx` and was lifted correctly, which is why the fault was X-shaped but
presented as "the right stick does nothing".

Five `movsx r32, bp` sites appear in that title's .text alone, so this was not
a one-instruction curiosity.

The checks below compile the lifter's own output and sweep it against the
honest x86 answer, each paired with a negative control that feeds the same
sweep the pre-fix expression and requires it to be rejected -- a sweep that
passes against both spellings would be testing nothing. The last check is the
structural one: movsx and movzx must accept the same source registers, which
is the invariant whose violation this was.
"""

import os
import shutil
import subprocess
import tempfile
import unittest

from .disasm import Instruction, Operand
from .lifter import Lifter, _fmt_operand_read, _fmt_operand_write

# Source registers movsx and movzx can take, by width, and the 32-bit register
# each one lives in.
REG8 = {"al": "eax", "bl": "ebx", "cl": "ecx", "dl": "edx",
        "ah": "eax", "bh": "ebx", "ch": "ecx", "dh": "edx"}
REG16 = {"ax": "eax", "bx": "ebx", "cx": "ecx", "dx": "edx",
         "si": "esi", "di": "edi", "bp": "ebp", "sp": "esp"}


def _lift(mnemonic, dst, src):
    """Lift one `movsx`/`movzx dst, src` and return its statement."""
    insn = Instruction(0, 3, mnemonic, "", "00")
    insn.operands = [dst, src]
    lifted = Lifter().lift_instruction(insn)
    # Neither instruction writes flags, so there is no result snapshot and the
    # write is the whole lifting.
    assert len(lifted) == 1, lifted
    return lifted[0]


def _legacy(dst, src):
    """The pre-fix statement for a source register that matched no branch.

    Built from the lifter's own operand helpers rather than hand-written C, so
    the control is the real old code path -- the fall-through that emitted the
    plain read -- and not an approximation of it.
    """
    return _fmt_operand_write(dst, _fmt_operand_read(src))


# The accessors, verbatim from templates/runtime/recomp_types.h. The zero
# extension in LO16/LO8 is the whole point: it is what the missing branch left
# in place of a sign extension.
PRELUDE = r"""
#include <stdint.h>
#include <stdio.h>
#define ZX8(v)   ((uint32_t)(uint8_t)(v))
#define ZX16(v)  ((uint32_t)(uint16_t)(v))
#define SX8(v)   ((uint32_t)(int32_t)(int8_t)(v))
#define SX16(v)  ((uint32_t)(int32_t)(int16_t)(v))
#define LO8(r)  ((uint8_t)((r) & 0xFF))
#define HI8(r)  ((uint8_t)(((r) >> 8) & 0xFF))
#define LO16(r) ((uint16_t)((r) & 0xFFFF))
static uint32_t eax, ebx, ecx, edx, esi, edi, ebp, esp;
"""

# One sweep over every 16-bit value. SEED puts v into the source register's
# host register, leaving the upper half dirty so a lifting that reads 32 bits
# instead of 16 cannot pass by accident.
BODY16 = r"""
int main(void) {
    for (uint32_t v = 0; v <= 0xFFFF; v++) {
        eax = ebx = ecx = edx = esi = edi = ebp = esp = 0xDEAD0000u;
        SEED;
        STMT;
        uint32_t want = (uint32_t)(int32_t)(int16_t)v;
        if (DEST != want) {
            printf("FAIL v=0x%04X got=0x%08X want=0x%08X\n", v, DEST, want);
            return 1;
        }
    }
    printf("OK\n");
    return 0;
}
"""


def _find_cc():
    return shutil.which("clang") or shutil.which("gcc") or shutil.which("cc")


class MovsxRegisterTest(unittest.TestCase):
    def _sweep(self, src_reg, stmt, dest="eax"):
        """Compile and run the 16-bit sweep. Returns (returncode, output).

        Skips the calling test when there is no C compiler. The skip lives
        here and not in setUp so the text assertions below -- which are the
        ones that pin the actual regression -- still run on a machine with no
        compiler on PATH, which is the normal case on this project's Windows
        box.
        """
        if not _find_cc():
            self.skipTest("no C compiler on PATH")
        host = REG16[src_reg]
        seed = f"{host} = ({host} & 0xFFFF0000u) | v"
        src = PRELUDE + (BODY16
                         .replace("SEED", seed)
                         .replace("STMT", stmt.rstrip(";"))
                         .replace("DEST", dest))
        cc = _find_cc()
        with tempfile.TemporaryDirectory() as tmp:
            c = os.path.join(tmp, "t.c")
            with open(c, "w") as f:
                f.write(src)
            exe = os.path.join(tmp, "t.exe")
            r = subprocess.run([cc, "-w", "-O2", c, "-o", exe],
                               capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stderr[-1500:] + "\n" + src)
            r = subprocess.run([exe], capture_output=True, text=True)
            return r.returncode, r.stdout + r.stderr

    def test_movsx_sign_extends_from_every_16bit_register(self):
        """Every 16-bit source must equal (int32_t)(int16_t)value.

        `bp` and `sp` are the two this was written for, but the sweep runs the
        whole set so a future edit cannot drop a different one.
        """
        for src_reg in REG16:
            # `sp` into `eax` is fine; a destination that aliased the source
            # would make the sweep read back what it just wrote.
            dst_reg = "ecx" if REG16[src_reg] == "eax" else "eax"
            with self.subTest(source=src_reg):
                stmt = _lift("movsx",
                             Operand(type="reg", reg=dst_reg),
                             Operand(type="reg", reg=src_reg))
                self.assertIn("SX16(", stmt)
                rc, out = self._sweep(src_reg, stmt, dest=dst_reg)
                self.assertEqual(rc, 0, f"{src_reg}: {out}\n  stmt: {stmt}")
                self.assertTrue(out.startswith("OK"), out)

    def test_pre_fix_expression_is_rejected(self):
        """Negative control: the fall-through must fail the same sweep.

        Restricted to `bp` and `sp`, which are the registers that actually
        took the fall-through before the fix.
        """
        for src_reg in ("bp", "sp"):
            with self.subTest(source=src_reg):
                dst = Operand(type="reg", reg="eax")
                src = Operand(type="reg", reg=src_reg)
                stmt = _legacy(dst, src)
                self.assertNotEqual(stmt, _lift("movsx", dst, src))
                rc, out = self._sweep(src_reg, stmt)
                self.assertNotEqual(
                    rc, 0,
                    f"{src_reg}: the pre-fix expression passed the sweep, so "
                    f"the sweep does not test anything: {out}")
                self.assertIn("FAIL", out)

    def test_movsx_bp_is_not_a_bare_read(self):
        """The measured case, pinned on its own.

        `movsx eax, bp` lifted to `eax = LO16(ebp);` in shipped output. This is
        the exact text assertion, so the regression is caught without needing a
        compiler.
        """
        stmt = _lift("movsx",
                     Operand(type="reg", reg="eax"),
                     Operand(type="reg", reg="bp"))
        self.assertEqual(stmt, "eax = SX16(LO16(ebp));")
        self.assertNotEqual(stmt, "eax = LO16(ebp);")

    def test_movsx_and_movzx_accept_the_same_registers(self):
        """The structural invariant this bug broke.

        The two lists were maintained by hand and drifted apart. Rather than
        assert one list's contents, require that neither instruction silently
        falls through on a source the other handles -- which is the property
        that actually failed, and stays true if the lists are ever rewritten.
        """
        for src_reg, width in [(r, 8) for r in REG8] + [(r, 16) for r in REG16]:
            dst_reg = "ecx" if REG16.get(src_reg, REG8.get(src_reg)) == "eax" \
                else "eax"
            with self.subTest(source=src_reg):
                sx = _lift("movsx", Operand(type="reg", reg=dst_reg),
                           Operand(type="reg", reg=src_reg))
                zx = _lift("movzx", Operand(type="reg", reg=dst_reg),
                           Operand(type="reg", reg=src_reg))
                self.assertIn(f"SX{width}(", sx,
                              f"movsx fell through on {src_reg}: {sx}")
                self.assertIn(f"ZX{width}(", zx,
                              f"movzx fell through on {src_reg}: {zx}")

    def test_unhandled_source_register_is_loud(self):
        """A source matching neither list must announce itself in the output.

        movsx's source is always narrower than its destination, so there is no
        legitimate unmatched case: reaching this branch means the lists have a
        gap. Emitting the marker is what turns the next occurrence of this bug
        into something a grep finds, rather than a wrong number in a game.
        """
        stmt = _lift("movsx",
                     Operand(type="reg", reg="eax"),
                     Operand(type="reg", reg="mm0"))
        self.assertIn("movsx: unhandled source register", stmt)


if __name__ == "__main__":
    unittest.main()
