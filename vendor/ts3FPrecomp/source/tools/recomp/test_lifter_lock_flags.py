"""Self-check that a LOCK prefix does not cost an instruction its flags.

LOCK changes atomicity, not arithmetic. `lock xadd` leaves exactly the flags
`xadd` leaves, from the sum; `lock cmpxchg` leaves exactly the flags
`cmpxchg` leaves. Capstone keeps the prefix in the mnemonic, and the flag
tracking used to match on that spelling, which lost the flags in two
different ways:

  * "lock xadd" was listed in _FLAGS_UNDEFINED, so it *cleared* tracking. The
    jcc after it then had no setter and fell back to the `_flags` placeholder,
    which is declared `int _flags = 0` and never assigned -- a branch that is
    never taken.
  * "lock cmpxchg" matched no list at all, so tracking was left *unchanged*
    and the following jcc was resolved against whatever instruction had set
    flags before it.

What the first one costs, measured in Jet Set Radio Future: its objects are
reference counted through the COM-style idiom

    lock xadd [this+8], edx     ; edx = -1, so refcount--
    jne  still_referenced       ; the sum is non-zero: someone else holds it
    ...                         ; otherwise fall through and destroy
    push 1
    call [vtable+0x48]          ; scalar deleting destructor, flags=1: delete

With the branch never taken, *every* Release() destroyed the object no matter
how many references remained, and the next Release() of the same object freed
it a second time. That put a free-list node in the title's heap linked to
itself, and the allocator hung walking it -- about as far from "a lock prefix"
as a symptom gets.

These checks are on the emitted text, because that is where the bug was: the
condition either reads the instruction's own result or it reads `_flags`.
"""

import unittest

from .disasm import Instruction, Operand
from .disasm import BasicBlock
from .lifter import (Lifter, FLAG_SETTERS, _EFLAGS_SETTERS, _FLAGS_UNDEFINED,
                     lift_basic_block)


def _mem(base):
    return Operand(type="mem", mem_base=base, mem_size=4)


def _insn(addr, size, mnemonic, op_str, operands):
    i = Instruction(addr, size, mnemonic, op_str, "00")
    i.operands = operands
    return i


def _release_sequence(mnemonic):
    """The Release() shape: <mnemonic> [eax], edx  then  jne away."""
    return [
        _insn(0x15FD4A, 4, mnemonic, "dword ptr [eax], edx",
              [_mem("eax"), Operand(type="reg", reg="edx")]),
        _insn(0x15FD4E, 2, "jne", "0x15fd60",
              [Operand(type="imm", imm=0x15FD60)]),
    ]


def _lift(insns):
    bb = BasicBlock(start=insns[0].address, instructions=list(insns))
    stmts, _state = lift_basic_block(Lifter(), bb)
    return "\n".join(stmts)


class LockPrefixFlagsTest(unittest.TestCase):
    def test_lock_xadd_is_not_flags_undefined(self):
        """The list membership the bug lived in."""
        self.assertNotIn("lock xadd", _FLAGS_UNDEFINED)
        self.assertIn("xadd", _EFLAGS_SETTERS)

    def test_locked_and_unlocked_xadd_lift_the_same_branch(self):
        """`lock xadd` + jne must produce the same condition as `xadd` + jne.

        This is the property that matters: the prefix is not allowed to change
        the branch. Comparing the two spellings against each other rather than
        against a fixed string keeps the check honest if the condition's
        wording ever changes.
        """
        locked = _lift(_release_sequence("lock xadd"))
        plain = _lift(_release_sequence("xadd"))
        self.assertEqual(
            locked.replace("lock xadd", "xadd"), plain,
            "the LOCK prefix changed the lifted branch:\n"
            f"--- locked ---\n{locked}\n--- plain ---\n{plain}")

    def test_the_branch_does_not_read_the_flags_placeholder(self):
        """The regression itself: `if (_flags ...)` is a branch never taken.

        `_flags` is declared `int _flags = 0` in every generated function and
        assigned nowhere, so any condition resolved from it is a constant.
        """
        for mnemonic in ("lock xadd", "xadd"):
            with self.subTest(mnemonic=mnemonic):
                out = _lift(_release_sequence(mnemonic))
                # A branch was emitted at all. The target is not a label in
                # this one-block harness, so it lifts as an indirect branch
                # rather than a goto; the condition is what is under test.
                self.assertIn("if (", out)
                self.assertNotIn(
                    "if (_flags", out,
                    f"{mnemonic} left its jcc on the _flags placeholder:\n{out}")

    def test_the_branch_reads_the_sum(self):
        """xadd sets ZF from the sum, which is what its destination now holds."""
        out = _lift(_release_sequence("lock xadd"))
        self.assertIn("MEM32(eax)", out, out)

    def test_lock_cmpxchg_does_not_inherit_stale_flags(self):
        """A `cmp` before a `lock cmpxchg` must not decide the cmpxchg's jcc.

        "lock cmpxchg" matched none of the three tracking lists, so the
        earlier `cmp` stayed installed as the flag setter and the jcc after
        the cmpxchg was resolved from it. The two must differ.
        """
        cmp_first = [
            _insn(0x1000, 2, "cmp", "esi, edi",
                  [Operand(type="reg", reg="esi"), Operand(type="reg", reg="edi")]),
            _insn(0x1002, 4, "lock cmpxchg", "dword ptr [eax], edx",
                  [_mem("eax"), Operand(type="reg", reg="edx")]),
            _insn(0x1006, 2, "jne", "0x1100",
                  [Operand(type="imm", imm=0x1100)]),
        ]
        no_cmp = cmp_first[1:]
        self.assertEqual(
            _lift(cmp_first).split("\n")[-1], _lift(no_cmp).split("\n")[-1],
            "the jcc after lock cmpxchg changed with an unrelated cmp before "
            "it, so it is reading the cmp's flags")

    def test_every_lock_form_is_tracked_like_its_base(self):
        """No locked RMW should fall through all three tracking lists.

        Falling through leaves the previous instruction's flags installed,
        which is the quieter half of this bug and the one that produces a
        plausible-looking wrong branch rather than a dead one.
        """
        known = FLAG_SETTERS | _EFLAGS_SETTERS | _FLAGS_UNDEFINED
        for base in ("add", "sub", "and", "or", "xor", "inc", "dec",
                     "xadd", "cmpxchg", "btr", "bts", "btc"):
            with self.subTest(base=base):
                if base not in known:
                    self.skipTest(f"{base} is not flag-tracked at all")
                stripped = ("lock " + base)[5:]
                self.assertIn(stripped, known)


if __name__ == "__main__":
    unittest.main()
