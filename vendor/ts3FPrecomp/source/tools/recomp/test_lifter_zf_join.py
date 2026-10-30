"""
A join whose predecessors disagree about what set the flags.

`cmp a, b` leaves ZF as (a == b); `test a, b` leaves it as ((a & b) == 0).
Both reach a shared `je`, and no single expression serves both, so
_merge_flag_states rightly refuses to call them the same state. The consumer
then falls back to `_flags` -- which nothing assigned, so the branch read an
uninitialised local and went whichever way the stack happened to be.

Max Payne had 221 branches in that position, 111 of them fixable this way.
The rest are functions entered with the caller's flags, where there is no
predecessor in the function to ask.

Two things have to hold, and the second is the one that bit:

  * every predecessor writes its own zero flag, not just one of them;
  * the write goes *before* the statement that leaves the block. Appended
    after the `goto` it never runs, and the join then reads what the other
    predecessor left -- which is worse than the uninitialised read it
    replaced, because it is wrong and it looks deliberate.
"""

from tools.recomp.lifter import zf_expression
from tools.recomp.translator import _before_terminator


def test_zf_expression_per_setter():
    assert zf_expression(("cmp", [object(), object()])) == "(_fa == _fb)"
    assert zf_expression(("test", [object(), object()])) == "((_fa & _fb) == 0)"
    # The result-snapshot family stores the result itself in _fa.
    assert zf_expression(("and", [object()])) == "(_fa == 0)"
    assert zf_expression(("xor", [object()])) == "(_fa == 0)"


def test_zf_expression_refuses_what_it_cannot_answer():
    """A wrong branch is much worse than an unmodelled one."""
    assert zf_expression(None) is None
    assert zf_expression(("fcom", [])) is None
    assert zf_expression(("bt", [object(), object()])) is None


def test_the_write_goes_before_the_block_leaves():
    """The bug this file exists for."""
    assert _before_terminator(["eax = 1;", "goto loc_1;"]) == 1
    assert _before_terminator(["eax = 1;", "if (x) goto loc_1;"]) == 1
    assert _before_terminator(["eax = 1;", "esp += 4; return; /* ret */"]) == 1
    # A run of them: the write belongs above the whole run.
    assert _before_terminator(["eax = 1;", "if (x) goto a;", "goto b;"]) == 1
    # Nothing leaves: fall-through, so appending is right.
    assert _before_terminator(["eax = 1;", "ecx = 2;"]) == 2
    assert _before_terminator([]) == 0
