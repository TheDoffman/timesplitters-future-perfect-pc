"""entry_pops_unsaved: does this address restore registers it never saved?

    python3 -m unittest tools.disasm.test_entry_pops

A seeded address that pops a callee-saved register it never pushed is the
middle of a function, not an entry point. Calling it runs an epilogue against
the caller's stack and hands the caller back a corrupted register, which
surfaces nowhere near the cause -- so the seeder has to reject it, and this
pins down exactly which shapes it rejects.
"""

import unittest

from .engine import DisasmEngine


class _Section:
    def __init__(self, va, size, executable=True):
        self.virtual_addr = va
        self.virtual_size = size
        self.executable = executable
        self.name = ".text"


class _Image:
    """Just enough of the image interface for the linear probe."""

    def __init__(self, va, code):
        self.va = va
        self.code = code
        self.sections = [_Section(va, len(code))]

    def get_section_at_va(self, addr):
        if self.va <= addr < self.va + len(self.code):
            return self.sections[0]
        return None

    def read_bytes_at_va(self, addr, length):
        if not (self.va <= addr < self.va + len(self.code)):
            return b""
        off = addr - self.va
        return self.code[off:off + length]


BASE = 0x00011000


def probe(code, offset=0):
    engine = DisasmEngine(_Image(BASE, code))
    return engine.entry_pops_unsaved(BASE + offset)


class ProperFunctions(unittest.TestCase):
    """A real entry point saves what it restores."""

    def test_push_pop_pairs_are_clean(self):
        # push esi ; push edi ; xor eax,eax ; pop edi ; pop esi ; ret
        code = bytes([0x56, 0x57, 0x31, 0xC0, 0x5F, 0x5E, 0xC3])
        self.assertEqual(probe(code), [])

    def test_one_push_answering_several_pops_is_clean(self):
        # A function with two return paths pops its single saved esi twice.
        # push esi ; xor eax,eax ; jz +2 ; pop esi ; ret ; pop esi ; ret
        code = bytes([0x56, 0x31, 0xC0, 0x74, 0x02, 0x5E, 0xC3, 0x5E, 0xC3])
        self.assertEqual(probe(code), [])

    def test_function_touching_no_saved_registers_is_clean(self):
        # mov eax,1 ; ret
        code = bytes([0xB8, 0x01, 0x00, 0x00, 0x00, 0xC3])
        self.assertEqual(probe(code), [])


class MidFunctionEntries(unittest.TestCase):
    """The shape the seeder has to reject."""

    def test_pop_without_push_is_reported(self):
        # pop esi ; ret   -- the tail of someone else's function
        code = bytes([0x5E, 0xC3])
        self.assertEqual(probe(code), ["esi"])

    def test_reports_every_offending_register(self):
        # pop edi ; pop esi ; pop ebx ; ret
        code = bytes([0x5F, 0x5E, 0x5B, 0xC3])
        self.assertEqual(sorted(probe(code)), ["ebx", "edi", "esi"])

    def test_entering_past_the_prologue_is_caught(self):
        # The whole function is clean, but entering after its pushes is not.
        # push esi ; push edi ; xor eax,eax ; pop edi ; pop esi ; ret
        code = bytes([0x56, 0x57, 0x31, 0xC0, 0x5F, 0x5E, 0xC3])
        self.assertEqual(probe(code), [], "the real entry is clean")
        # ...enter two bytes in, past both pushes:
        self.assertEqual(sorted(probe(code, offset=2)), ["edi", "esi"])

    def test_a_register_pushed_after_the_pop_does_not_excuse_it(self):
        # pop esi ; push esi ; ret -- the pop still ran against the caller.
        code = bytes([0x5E, 0x56, 0xC3])
        self.assertEqual(probe(code), ["esi"])


class Boundaries(unittest.TestCase):

    def test_scanning_stops_at_the_first_ret(self):
        # ret ; pop esi -- the pop belongs to whatever follows, not to us.
        code = bytes([0xC3, 0x5E])
        self.assertEqual(probe(code), [])

    def test_address_outside_the_image_is_not_reported(self):
        code = bytes([0x5E, 0xC3])
        engine = DisasmEngine(_Image(BASE, code))
        self.assertEqual(engine.entry_pops_unsaved(BASE + 0x9000), [])

    def test_eax_is_not_callee_saved(self):
        # pop eax ; ret -- caller-saved, so popping it proves nothing.
        code = bytes([0x58, 0xC3])
        self.assertEqual(probe(code), [])


class BlockExtentEnd(unittest.TestCase):
    """A forward jump inside the body is control flow, not a terminator.

    Treating every jmp as the end cut a global constructor to 20 bytes at its
    own else-skip, stranding the epilogue that restored esi -- and _initterm
    calls that constructor with esi holding its table cursor.
    """

    def test_forward_jump_does_not_end_the_run(self):
        # push esi ; jmp +2 ; nop ; nop ; pop esi ; ret
        code = bytes([0x56, 0xEB, 0x02, 0x90, 0x90, 0x5E, 0xC3])
        engine = DisasmEngine(_Image(BASE, code))
        end = engine.block_extent_end(BASE)
        self.assertEqual(end, BASE + len(code),
                         "the run should reach the ret past the forward jump")

    def test_backward_jump_still_ends_the_run(self):
        # nop ; jmp -3  (backwards: tail-call shaped)
        code = bytes([0x90, 0xEB, 0xFC])
        engine = DisasmEngine(_Image(BASE, code))
        self.assertEqual(engine.block_extent_end(BASE), BASE + 3)

    def test_a_ret_still_ends_the_run(self):
        code = bytes([0x90, 0xC3, 0x90])
        engine = DisasmEngine(_Image(BASE, code))
        self.assertEqual(engine.block_extent_end(BASE), BASE + 2)

    def test_jump_leaving_the_window_ends_the_run(self):
        # jmp far forward, past everything we can see
        code = bytes([0xE9, 0x00, 0x10, 0x00, 0x00, 0x90])
        engine = DisasmEngine(_Image(BASE, code))
        self.assertEqual(engine.block_extent_end(BASE), BASE + 5)


class BlockTailJump(unittest.TestCase):
    """The three block probes have to agree about where a block ends.

    block_extent_end learned that a forward jump inside the window is an
    else-skip rather than a terminator; block_tail_jump did not, so it named
    sub_00120307's own internal jump as that block's tail. A caller then both
    believed the block ended in a tail call and registered an alias entry at
    an address in the middle of the function.
    """

    def test_an_internal_forward_jump_is_not_the_tail(self):
        # push esi ; jmp +2 ; nop ; nop ; pop esi ; ret
        code = bytes([0x56, 0xEB, 0x02, 0x90, 0x90, 0x5E, 0xC3])
        engine = DisasmEngine(_Image(BASE, code))
        self.assertIsNone(engine.block_tail_jump(BASE),
                          "this block rets; the jmp only skips two nops")

    def test_a_jump_out_of_the_block_is_still_the_tail(self):
        # nop ; jmp far forward, past everything in the window
        code = bytes([0x90, 0xE9, 0x00, 0x10, 0x00, 0x00])
        engine = DisasmEngine(_Image(BASE, code))
        self.assertEqual(engine.block_tail_jump(BASE), BASE + 0x1006)

    def test_a_backward_jump_is_still_the_tail(self):
        # nop ; jmp -3  (rel8 is measured from the end of the jump)
        code = bytes([0x90, 0xEB, 0xFD])
        engine = DisasmEngine(_Image(BASE, code))
        self.assertEqual(engine.block_tail_jump(BASE), BASE)

    def test_a_ret_before_the_jump_still_means_not_this_shape(self):
        code = bytes([0xC3, 0xEB, 0xFC])
        engine = DisasmEngine(_Image(BASE, code))
        self.assertIsNone(engine.block_tail_jump(BASE))


if __name__ == "__main__":
    unittest.main()
