"""Replacing XDK functions in a title built with link-time code generation.

LTCG lets the linker rewrite a function's calling convention, and the
signature database records what it did in the name:
D3DDevice_SelectVertexShader_0__LTCG_eax1_ebx2 takes both arguments in
registers and none on the stack.

Matching only the plain name meant none of these was ever replaced. Black is
the first LTCG title here and carries fifteen, including the whole
vertex-shader path, which is exactly what had to be replaced to make
TimeSplitters 2 draw.
"""

import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))

from tools.recomp.hle import parse_ltcg, plan, render_thunks  # noqa: E402


class ParseName(unittest.TestCase):
    def test_plain_name_is_left_alone(self):
        self.assertEqual(parse_ltcg("D3DDevice_Clear"), ("D3DDevice_Clear", {}))

    def test_one_register_argument(self):
        base, regs = parse_ltcg("D3DDevice_LoadVertexShader_4__LTCG_eax1")
        self.assertEqual(base, "D3DDevice_LoadVertexShader")
        self.assertEqual(regs, {1: "eax"})

    def test_two_register_arguments(self):
        base, regs = parse_ltcg("D3DDevice_SelectVertexShader_0__LTCG_eax1_ebx2")
        self.assertEqual(base, "D3DDevice_SelectVertexShader")
        self.assertEqual(regs, {1: "eax", 2: "ebx"})

    def test_registers_need_not_be_the_first_arguments(self):
        """_ecx1_eax3 leaves argument 2 on the stack, between two registers."""
        base, regs = parse_ltcg("D3DDevice_SetPixelShaderConstant_4__LTCG_ecx1_eax3")
        self.assertEqual(base, "D3DDevice_SetPixelShaderConstant")
        self.assertEqual(regs, {1: "ecx", 3: "eax"})

    def test_a_name_that_merely_contains_ltcg_is_not_mangled(self):
        self.assertEqual(parse_ltcg("SomethingLTCGish"), ("SomethingLTCGish", {}))


def _sym(name, addr):
    return {"name": name, "address": addr, "kind": "function"}


class Planning(unittest.TestCase):
    def test_plain_match_still_wins(self):
        syms = [_sym("D3DDevice_Clear", 0x1000),
                _sym("D3DDevice_Clear_4__LTCG_eax1", 0x2000)]
        replace, _ = plan(syms, {"D3DDevice_Clear"}, {0x1000, 0x2000}, set(),
                          lambda a: 4)
        self.assertEqual(replace, {0x1000: ("D3DDevice_Clear", 4, {})})

    def test_ltcg_variant_matches_a_plain_implementation(self):
        syms = [_sym("D3DDevice_SelectVertexShader_0__LTCG_eax1_ebx2", 0x3000)]
        replace, notes = plan(syms, {"D3DDevice_SelectVertexShader"}, {0x3000},
                              set(), lambda a: 0)
        self.assertEqual(replace,
                         {0x3000: ("D3DDevice_SelectVertexShader", 0,
                                   {1: "eax", 2: "ebx"})})
        self.assertTrue(any("LTCG" in n for n in notes), notes)

    def test_two_variants_are_refused_rather_than_guessed(self):
        syms = [_sym("D3DDevice_Clear_0__LTCG_eax1", 0x4000),
                _sym("D3DDevice_Clear_4__LTCG_ebx1", 0x5000)]
        replace, notes = plan(syms, {"D3DDevice_Clear"}, {0x4000, 0x5000},
                              set(), lambda a: 0)
        self.assertEqual(replace, {})
        self.assertTrue(any("2 LTCG variants" in n for n in notes), notes)


class Thunks(unittest.TestCase):
    def test_a_plain_replacement_is_unchanged(self):
        src = render_thunks({0x1000: ("D3DDevice_Swap", 4, {})})
        self.assertIn("void sub_00001000(void) { hle_D3DDevice_Swap(); "
                      "g_esp += 8; }", src)
        self.assertNotIn("RECOMP_HLE_LTCG_CALL", src)

    def test_register_arguments_are_marshalled(self):
        src = render_thunks({0x3000: ("D3DDevice_SelectVertexShader", 0,
                                      {1: "eax", 2: "ebx"})})
        # Two arguments, both from registers, nothing off the caller's stack.
        self.assertIn("RECOMP_HLE_LTCG_CALL(2, hle_D3DDevice_SelectVertexShader, 4)",
                      src)
        self.assertIn("a[0] = g_eax;", src)
        self.assertIn("a[1] = g_ebx;", src)
        self.assertNotIn("RECOMP_HLE_STACK", src)
        self.assertIn("RECOMP_HLE_LTCG_END", src)

    def test_stack_arguments_fill_the_gaps_in_order(self):
        """_ecx1_eax3 with 4 bytes of stack: argument 2 is the only stack one."""
        src = render_thunks({0x4000: ("D3DDevice_SetPixelShaderConstant", 4,
                                      {1: "ecx", 3: "eax"})})
        self.assertIn("RECOMP_HLE_LTCG_CALL(3, "
                      "hle_D3DDevice_SetPixelShaderConstant, 8)", src)
        self.assertIn("a[0] = g_ecx;", src)
        self.assertIn("a[1] = RECOMP_HLE_STACK(0);", src)
        self.assertIn("a[2] = g_eax;", src)

    def test_the_comment_records_where_each_argument_came_from(self):
        src = render_thunks({0x3000: ("D3DDevice_SelectVertexShader", 0,
                                      {1: "eax", 2: "ebx"})})
        self.assertIn("arg1 in eax, arg2 in ebx", src)


if __name__ == "__main__":
    unittest.main()
