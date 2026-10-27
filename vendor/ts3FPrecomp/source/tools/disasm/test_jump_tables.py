"""resync_jump_tables: the index into a switch table can be negative.

    python3 -m unittest tools.disasm.test_jump_tables

`jmp [reg*4 + disp]` says where index 0 lands, not where the table begins.
MSVC's memcpy tail dispatch counts remaining bytes *down*, so disp names the
last entry and the rest of the table sits below it. Scanning forward from disp
finds one entry, rejects the table as too short, and leaves the sweep's
hallucinated instructions in place -- which ends the enclosing function inside
its own table, stranding the epilogue.
"""

import unittest
import struct

from .engine import DisasmEngine


class _Section:
    def __init__(self, va, size, executable=True):
        self.virtual_addr = va
        self.virtual_size = size
        self.executable = executable
        self.name = ".text"


class _Image:
    def __init__(self, va, code):
        self.va = va
        self.code = bytearray(code)
        self.base_address = va
        self.image_size = len(code)
        self.sections = [_Section(va, len(code))]

    def get_section_at_va(self, addr):
        if self.va <= addr < self.va + len(self.code):
            return self.sections[0]
        return None

    def read_bytes_at_va(self, addr, length):
        if not (self.va <= addr < self.va + len(self.code)):
            return b""
        off = addr - self.va
        return bytes(self.code[off:off + length])

    def get_section_data(self, section):
        return bytes(self.code)

    def read_u32_at_va(self, addr):
        raw = self.read_bytes_at_va(addr, 4)
        if len(raw) < 4:
            return None
        return struct.unpack("<I", raw)[0]


BASE = 0x00011000


def build(table_entries, disp_index):
    """A section holding a table of `table_entries` plus somewhere to point.

    The dispatch's displacement names entry `disp_index`, so a table addressed
    with a negative index has disp_index at the end.
    """
    size = 0x400
    img = _Image(BASE, bytes(size))

    table_va = BASE + 0x100
    for i, target in enumerate(table_entries):
        off = (table_va - BASE) + i * 4
        img.code[off:off + 4] = struct.pack("<I", target)

    disp = table_va + disp_index * 4
    # jmp dword ptr [ecx*4 + disp]  ->  ff 24 8d <disp>
    jmp_off = 0x20
    img.code[jmp_off:jmp_off + 3] = b"\xff\x24\x8d"
    img.code[jmp_off + 3:jmp_off + 7] = struct.pack("<I", disp)
    return img, table_va, BASE + jmp_off


class BackwardIndexedTables(unittest.TestCase):

    def test_a_table_addressed_from_its_last_entry_is_found(self):
        targets = [BASE + 0x200 + i * 8 for i in range(8)]
        img, table_va, jmp_va = build(targets, disp_index=len(targets) - 1)
        engine = DisasmEngine(img)
        engine.linear_sweep(img.sections[0])
        engine.resync_jump_tables()

        self.assertIn(table_va, engine.jump_tables,
                      "the table starts below the displacement, not at it")
        self.assertEqual(engine.jump_tables[table_va],
                         table_va + len(targets) * 4)
        self.assertEqual(engine.jump_table_entries(table_va), targets)

    def test_a_forward_table_still_works(self):
        targets = [BASE + 0x200 + i * 8 for i in range(6)]
        img, table_va, _ = build(targets, disp_index=0)
        engine = DisasmEngine(img)
        engine.linear_sweep(img.sections[0])
        engine.resync_jump_tables()

        self.assertIn(table_va, engine.jump_tables)
        self.assertEqual(engine.jump_table_entries(table_va), targets)


def build_inline(table_entries, disp_index):
    """The same, but with the table immediately after the dispatch.

    This is where MSVC actually puts it, and the distance is what matters:
    `build()` above leaves 0xE0 bytes between the jump and the table, so the
    backward scan runs out of plausible pointers long before it reaches the
    jump. Inline, the four bytes below the table *are* the jump's
    displacement, and its value is the table address -- a perfectly good code
    address for the scan to accept.
    """
    size = 0x400
    img = _Image(BASE, bytes(size))

    jmp_off = 0x20
    table_va = BASE + jmp_off + 7          # straight after ff 24 8d <disp32>
    for i, target in enumerate(table_entries):
        off = (table_va - BASE) + i * 4
        img.code[off:off + 4] = struct.pack("<I", target)

    disp = table_va + disp_index * 4
    img.code[jmp_off:jmp_off + 3] = b"\xff\x24\x8d"
    img.code[jmp_off + 3:jmp_off + 7] = struct.pack("<I", disp)
    for target in table_entries:
        img.code[target - BASE] = 0xC3     # ret, so the arms decode
    return img, table_va, BASE + jmp_off


class InlineTables(unittest.TestCase):
    """A table placed immediately after its dispatch, which is the usual case.

    Both of these failed before the backward scan learned to stop at the
    instruction that owns the bytes: the scan read the jump's own displacement
    as entry -1, moved the table start back a slot, and the cleanup loop then
    deleted the jump -- so _find_function_end saw neither a table nor an
    instruction at the dispatch and ended the function there. Every function
    with an inline switch was truncated, on every title.
    """

    def test_a_table_directly_after_its_dispatch_is_not_extended_backwards(self):
        targets = [BASE + 0x200 + i * 0x10 for i in range(4)]
        img, table_va, jmp_va = build_inline(targets, disp_index=0)
        engine = DisasmEngine(img)
        engine.linear_sweep(img.sections[0])
        engine.resync_jump_tables()

        self.assertIn(table_va, engine.jump_tables,
                      "the table starts where the jump ends, not four bytes below")
        self.assertEqual(engine.jump_tables[table_va], table_va + len(targets) * 4)
        self.assertEqual(engine.jump_table_entries(table_va), targets)
        self.assertIsNotNone(engine.instructions.get(jmp_va),
                             "the dispatch jump must survive the resync")

    def test_an_inline_table_addressed_from_its_last_entry_still_resolves(self):
        # The negative-index case the backward scan exists for, in the layout
        # it will actually meet: scanning down from the displacement has to
        # reach the table start and stop exactly at the end of the jump.
        targets = [BASE + 0x200 + i * 0x10 for i in range(4)]
        img, table_va, jmp_va = build_inline(targets, disp_index=len(targets) - 1)
        engine = DisasmEngine(img)
        engine.linear_sweep(img.sections[0])
        engine.resync_jump_tables()

        self.assertIn(table_va, engine.jump_tables)
        self.assertEqual(engine.jump_table_entries(table_va), targets)
        self.assertIsNotNone(engine.instructions.get(jmp_va),
                             "the dispatch jump must survive the resync")


class EpilogueBeforeTheTable(unittest.TestCase):
    """The dispatch is early in the function; the table sits after its
    epilogue. Burnout 2's sub_00092AE0: `jmp [eax*4+0x92D8C]` at 0x92C25, and
    `add esp,0x218; ret 4` immediately below the table.

    The epilogue's small immediates read as .text addresses -- C4 18 02 00 is
    0x000218C4 and 00 C2 04 00 is 0x0004C200 -- so the backward scan took them
    as entries -1 and -2 and the cleanup deleted the epilogue. The function
    then never freed its frame, and the title booted to the dashboard."""

    def test_the_epilogue_is_not_read_as_table_entries(self):
        size = 0x40000                     # big enough that both values are in .text
        img = _Image(BASE, bytes(size))
        jmp_off = 0x20
        table_va = BASE + 0x100
        epilogue = b"\x81\xc4\x18\x02\x00\x00\xc2\x04\x00"
        img.code[0x100 - len(epilogue):0x100] = epilogue
        targets = [BASE + 0x200 + i * 0x10 for i in range(4)]
        for i, target in enumerate(targets):
            img.code[0x100 + i * 4:0x100 + i * 4 + 4] = struct.pack("<I", target)
            img.code[target - BASE] = 0xC3
        img.code[jmp_off:jmp_off + 3] = b"\xff\x24\x8d"
        img.code[jmp_off + 3:jmp_off + 7] = struct.pack("<I", table_va)
        self.assertEqual(struct.unpack_from("<I", img.code, 0xFC)[0], 0x0004C200)
        self.assertEqual(struct.unpack_from("<I", img.code, 0xF8)[0], 0x000218C4)

        engine = DisasmEngine(img)
        engine.linear_sweep(img.sections[0])
        engine.decode_at(table_va - len(epilogue))
        engine.resync_jump_tables()

        self.assertIn(table_va, engine.jump_tables,
                      "the table starts where the ret ends")
        self.assertEqual(engine.jump_table_entries(table_va), targets)
        add = engine.instructions.get(table_va - 9)
        ret = engine.instructions.get(table_va - 3)
        self.assertIsNotNone(add, "add esp, 0x218 must survive the resync")
        self.assertIsNotNone(ret, "ret 4 must survive the resync")
        self.assertTrue(ret.is_ret)


class CodeBelowTheTableThatLooksNearby(unittest.TestCase):
    """Burnout 2's sub_0003EB80: a table at 0x0003EC28 preceded by
    `pop ebp; ret 4` (the target of a jne). Those bytes read as 0x0004C25D --
    inside .text and only 55 KB from the dispatch, so a coarse "near the
    dispatch" test accepts it. Case labels cluster; this one is nowhere near
    the table's other targets."""

    def test_an_epilogue_that_reads_as_a_nearby_address_is_not_an_entry(self):
        size = 0x40000
        img = _Image(BASE, bytes(size))
        disp_off = 0x2F000                 # dispatch 0x40000: 0x4C25D is 49 KB away
        table_off = disp_off + 0x100
        table_va = BASE + table_off
        epilogue = b"\x5d\xc2\x04\x00"     # pop ebp; ret 4  ->  0x0004C25D
        img.code[table_off - 4:table_off] = epilogue
        targets = [BASE + disp_off + 0x10 + i * 0xC for i in range(3)]
        for i, target in enumerate(targets):
            img.code[table_off + i * 4:table_off + i * 4 + 4] = struct.pack("<I", target)
            img.code[target - BASE] = 0xC3
        img.code[disp_off:disp_off + 3] = b"\xff\x24\x8d"
        img.code[disp_off + 3:disp_off + 7] = struct.pack("<I", table_va)
        self.assertEqual(struct.unpack_from("<I", img.code, table_off - 4)[0],
                         0x0004C25D)
        self.assertLess(0x0004C25D - (BASE + disp_off), 0x10000)

        engine = DisasmEngine(img)
        engine.linear_sweep(img.sections[0])
        engine.decode_at(table_va - 4)
        engine.resync_jump_tables()

        self.assertIn(table_va, engine.jump_tables)
        self.assertEqual(engine.jump_table_entries(table_va), targets)
        ret = engine.instructions.get(table_va - 3)
        self.assertIsNotNone(ret, "ret 4 must survive the resync")
        self.assertTrue(ret.is_ret)


class StillRejectsNonTables(unittest.TestCase):
    """Scanning both ways must not make a coincidence into a table."""

    def test_too_few_entries_either_way(self):
        # Two plausible pointers is under min_entries however you count them.
        targets = [BASE + 0x200, BASE + 0x208]
        img, table_va, _ = build(targets, disp_index=1)
        engine = DisasmEngine(img)
        engine.linear_sweep(img.sections[0])
        engine.resync_jump_tables()
        self.assertNotIn(table_va, engine.jump_tables)

    def test_entries_outside_the_section_bound_the_scan(self):
        # A run of three valid entries, then one pointing out of the section:
        # the table ends there rather than swallowing the rest.
        targets = [BASE + 0x200, BASE + 0x208, BASE + 0x210]
        img, table_va, _ = build(targets, disp_index=0)
        off = (table_va - BASE) + 3 * 4
        img.code[off:off + 4] = struct.pack("<I", 0xDEADBEEF)
        engine = DisasmEngine(img)
        engine.linear_sweep(img.sections[0])
        engine.resync_jump_tables()
        self.assertEqual(engine.jump_tables.get(table_va),
                         table_va + 3 * 4)


class DispatchExposedAfterTheSweep(unittest.TestCase):
    """TimeSplitters 2's sub_000DDD70: the sweep arrived at the function out
    of phase, so its `jmp [eax*4+0xDDEB8]` was junk until _pass_call_targets
    realigned the call target with decode_at(). By then resync_jump_tables()
    had already run, the new candidate was never measured, and the five case
    bodies (which the same misphase had covered) were never decoded. The
    function ended on the dispatch and the title span on the lifted jump's
    unknown target."""

    def _build(self):
        size = 0x400
        img = _Image(BASE, bytes(size))
        jmp_off = 0x20
        table_va = BASE + 0x100
        # Odd addresses: the sweep over zero bytes decodes 2-byte `add` at
        # every even address, so nothing starts an instruction at an odd one
        # unless something decodes there on purpose.
        targets = [BASE + 0x201 + i * 0x10 for i in range(5)]
        for i, target in enumerate(targets):
            img.code[0x100 + i * 4:0x100 + i * 4 + 4] = struct.pack("<I", target)
            img.code[target - BASE] = 0xC3
        img.code[jmp_off:jmp_off + 3] = b"\xff\x24\x8d"
        img.code[jmp_off + 3:jmp_off + 7] = struct.pack("<I", table_va)
        # `00 B8 <imm32>` at 0x1E swallows the first bytes of the dispatch,
        # which puts the sweep out of phase exactly where the title's was.
        img.code[0x1F] = 0xB8
        return img, table_va, BASE + jmp_off, targets

    def test_a_dispatch_the_sweep_missed_is_measured_on_the_next_pass(self):
        img, table_va, jmp_va, targets = self._build()
        engine = DisasmEngine(img)
        engine.linear_sweep(img.sections[0])
        engine.resync_jump_tables()
        self.assertNotIn(table_va, engine.jump_tables,
                         "the sweep must really be out of phase at the dispatch")
        self.assertIsNone(engine.instructions.get(jmp_va))

        engine.decode_at(jmp_va)               # what _pass_call_targets does
        self.assertIsNotNone(engine.instructions.get(jmp_va))
        self.assertEqual(engine.resync_jump_tables(), 1)
        self.assertIn(table_va, engine.jump_tables)
        self.assertEqual(engine.jump_table_entries(table_va), targets)

    def test_the_case_bodies_are_decoded_with_the_table(self):
        img, table_va, jmp_va, targets = self._build()
        engine = DisasmEngine(img)
        engine.linear_sweep(img.sections[0])
        engine.decode_at(jmp_va)
        engine.resync_jump_tables()
        for target in targets:
            insn = engine.instructions.get(target)
            self.assertIsNotNone(insn, f"case body at 0x{target:08X} not decoded")
            self.assertTrue(insn.is_ret)

    def test_a_second_pass_does_not_measure_a_table_twice(self):
        img, table_va, jmp_va, targets = self._build()
        engine = DisasmEngine(img)
        engine.linear_sweep(img.sections[0])
        engine.decode_at(jmp_va)
        self.assertEqual(engine.resync_jump_tables(), 1)
        self.assertEqual(engine.resync_jump_tables(), 0)
        self.assertEqual(engine.jump_tables[table_va], table_va + len(targets) * 4)


class EntriesByDisplacement(unittest.TestCase):
    """_find_function_end asks for a table's entries by the displacement in
    the dispatch instruction. When the table starts below that displacement
    (memcpy's downward count) the two differ, and the lookup found nothing."""

    def test_a_shifted_table_is_found_by_the_displacement_it_was_named_by(self):
        targets = [BASE + 0x200 + i * 8 for i in range(8)]
        img, table_va, _ = build(targets, disp_index=len(targets) - 1)
        disp = table_va + (len(targets) - 1) * 4
        engine = DisasmEngine(img)
        engine.linear_sweep(img.sections[0])
        engine.resync_jump_tables()
        self.assertEqual(engine.jump_table_entries(disp), targets)
        self.assertEqual(engine.jump_table_entries(table_va), targets)


if __name__ == "__main__":
    unittest.main()
