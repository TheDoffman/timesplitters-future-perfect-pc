"""Tests for calls to functions replaced through manual dispatch."""

import tempfile

from tools.recomp.disasm import BasicBlock, Instruction, Operand
from tools.recomp.lifter import Lifter, lift_basic_block
from tools.recomp.translator import BatchTranslator, _fixup_icall_esp_save


TARGET = 0x001E9100


def _direct_call(target=TARGET):
    insn = Instruction(0x00120000, 5, "call", f"0x{target:x}", "e800000000")
    insn.call_target = target
    return insn


def _tail_jump(target=TARGET):
    insn = Instruction(0x00120000, 5, "jmp", f"0x{target:x}", "e900000000")
    insn.jump_target = target
    return insn


def test_selected_direct_call_uses_manual_lookup():
    lifted = Lifter(manual_functions={TARGET}).lift_instruction(_direct_call())
    generated = "\n".join(lifted)

    assert "PUSH32(esp, 0x00120005u);" in generated
    assert "RECOMP_ICALL_SAFE(0x001E9100u, _icall_esp)" in generated
    assert "sub_001E9100();" not in generated


def test_unselected_direct_call_remains_direct():
    lifted = Lifter().lift_instruction(_direct_call())
    generated = "\n".join(lifted)

    # A direct call goes through RECOMP_ABI_CALL, which expands to a plain
    # (fn)() unless -DRECOMP_ABI_CHECK is set. What matters here is that it
    # names the function rather than routing through the manual lookup.
    assert "sub_001E9100" in generated
    assert "RECOMP_ICALL_SAFE" not in generated
    assert "RECOMP_ICALL_SAFE" not in generated


def test_selected_tail_jump_uses_manual_lookup():
    lifter = Lifter(manual_functions={TARGET})
    lifter.func_start = 0x00120000
    lifter.func_end = 0x00120100

    generated = "\n".join(lifter.lift_instruction(_tail_jump()))

    assert "RECOMP_ITAIL(0x001E9100u)" in generated
    assert "sub_001E9100();" not in generated


def test_selected_tail_jump_keeps_exit_trace():
    lifter = Lifter(manual_functions={TARGET})
    lifter.func_start = 0x00120000
    lifter.func_end = 0x00120100
    lifter.trace_exit_name = "sub_00120000"

    generated = "\n".join(lifter.lift_instruction(_tail_jump()))

    assert 'RECOMP_TRACE_ESP("sub_00120000", "tail 0x001E9100")' in generated


def test_selected_conditional_tail_uses_manual_lookup():
    compare = Instruction(0x00120000, 2, "cmp", "eax, ecx", "39c8")
    compare.operands = [
        Operand(type="reg", reg="eax"),
        Operand(type="reg", reg="ecx"),
    ]
    jump = Instruction(
        0x00120002, 6, "je", f"0x{TARGET:x}", "0f8400000000")
    jump.jump_target = TARGET
    lifter = Lifter(manual_functions={TARGET})
    lifter.func_start = 0x00120000
    lifter.func_end = 0x00120100

    generated, _ = lift_basic_block(
        lifter, BasicBlock(start=0x00120000, instructions=[compare, jump]))
    output = "\n".join(generated)

    assert "RECOMP_ITAIL(0x001E9100u)" in output
    assert "sub_001E9100();" not in output


def test_direct_manual_call_saves_esp_before_argument_pushes():
    lines = [
        "    PUSH32(esp, ecx);",
        "    PUSH32(esp, 0x00120005u); "
        "RECOMP_ICALL_SAFE(0x001E9100u, _icall_esp); "
        "/* manual call 0x001E9100 */",
    ]

    fixed = _fixup_icall_esp_save(lines)
    save = next(i for i, line in enumerate(fixed) if "_icall_esp = g_esp" in line)
    argument = next(i for i, line in enumerate(fixed) if line.strip() == "PUSH32(esp, ecx);")

    assert save < argument


def test_split_translation_passes_manual_set_to_lifter():
    class FakeTranslator:
        def __init__(self):
            self.owned_function_starts = set()
            self.lifter = Lifter()
            self.seen_manual = None

        def translate_function(self, addr, func_info):
            self.seen_manual = self.lifter.manual_functions
            return f"void {func_info['name']}(void) {{}}"

    batch = BatchTranslator.__new__(BatchTranslator)
    batch.translator = FakeTranslator()
    functions = [
        (0x00120000, {"name": "sub_00120000"}),
        (TARGET, {"name": "sub_001E9100"}),
    ]

    with tempfile.TemporaryDirectory() as output_dir:
        batch.translate_batch_split(
            functions, output_dir, manual={TARGET})

    assert batch.translator.seen_manual == {TARGET}


def test_kept_original_is_lifted_once_and_left_out_of_dispatch():
    import glob
    import os

    class FakeTranslator:
        def __init__(self):
            self.owned_function_starts = set()
            self.lifter = Lifter()

        def translate_function(self, addr, func_info):
            return f"void {func_info['name']}(void) {{}}"

    batch = BatchTranslator.__new__(BatchTranslator)
    batch.translator = FakeTranslator()
    functions = [
        (0x00120000, {"name": "sub_00120000"}),
        (TARGET, {"name": "sub_001E9100"}),
    ]
    kept = "sub_001E9100_hle_original"

    with tempfile.TemporaryDirectory() as output_dir:
        batch.translate_batch_split(
            functions, output_dir, manual={TARGET},
            keep_bodies={TARGET: kept})
        sources = {}
        for path in glob.glob(os.path.join(output_dir, "*")):
            with open(path, encoding="utf-8") as fh:
                sources[os.path.basename(path)] = fh.read()

    chunks = "".join(text for name, text in sources.items()
                     if name.endswith(".c") and not name.endswith("_dispatch.c"))
    dispatch = next(text for name, text in sources.items()
                    if name.endswith("_dispatch.c"))
    # By name: the output folder holds other headers too (recomp_types.h), and
    # glob order differs between Windows and Linux.
    header = sources["recomp_funcs.h"]

    # The body is emitted once, under its kept name, and never under the
    # address's own name -- that one belongs to the replacement.
    assert chunks.count(f"void {kept}(void) {{}}") == 1
    assert "void sub_001E9100(void) {}" not in chunks
    assert f"void {kept}(void);" in header
    # The address dispatches to the replacement, never to the kept body.
    assert "sub_001E9100 }" in dispatch
    assert kept not in dispatch
