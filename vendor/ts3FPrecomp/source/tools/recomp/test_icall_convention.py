"""Where the _icall_esp capture goes depends on who cleans the arguments.

    python3 -m unittest tools.recomp.test_icall_convention

RECOMP_ICALL_SAFE rewinds g_esp to the captured value when the lookup fails, so
the capture point decides what a failed indirect call does to the stack:

  stdcall  the callee would have cleaned the arguments, so the failure path has
           to clean them. Capture above the pushes.
  cdecl    the caller cleans them a few lines later whatever happens, so
           capturing above the pushes cleans them twice. Capture below.

Getting it wrong is silent: the caller's epilogue then pops its saved registers
from the wrong slots and returns corrupted values to *its* caller, faulting
somewhere with no connection to the call that failed.
"""

import unittest

from .translator import _fixup_icall_esp_save


def capture_index(lines):
    out = _fixup_icall_esp_save(lines)
    return out, next(i for i, l in enumerate(out) if "_icall_esp = g_esp" in l)


class CdeclSites(unittest.TestCase):
    """The caller cleans, so the failure path must not."""

    def test_capture_goes_below_the_arguments(self):
        lines = [
            "    PUSH32(esp, 0);",
            "    PUSH32(esp, 0x5ADDD0u);",
            "    PUSH32(esp, 0x5ADCB0u);",
            "    PUSH32(esp, 4);",
            "    PUSH32(esp, 0x000E8C44u); RECOMP_ICALL_SAFE(MEM32(esi + 4), _icall_esp); /* indirect call */",
            "loc_000E8C44: ;",
            "    esp = esp + 0x10;",
        ]
        out, save = capture_index(lines)
        icall = next(i for i, l in enumerate(out) if "RECOMP_ICALL_SAFE(" in l)
        first_arg = next(i for i, l in enumerate(out) if l.strip() == "PUSH32(esp, 0);")
        self.assertGreater(save, first_arg,
                           "capture must sit below the argument pushes:\n" + "\n".join(out))
        self.assertLess(save, icall)

    def test_decimal_cleanup_is_recognised(self):
        lines = [
            "    PUSH32(esp, eax);",
            "    PUSH32(esp, 0x00120010u); RECOMP_ICALL_SAFE(edx, _icall_esp); /* indirect call */",
            "    esp = esp + 4;",
        ]
        out, save = capture_index(lines)
        icall = next(i for i, l in enumerate(out) if "RECOMP_ICALL_SAFE(" in l)
        arg = next(i for i, l in enumerate(out) if l.strip() == "PUSH32(esp, eax);")
        self.assertGreater(save, arg)
        self.assertLess(save, icall)


class StdcallSites(unittest.TestCase):
    """No cleanup follows, so the callee owned the arguments."""

    def test_capture_stays_above_the_arguments(self):
        lines = [
            "    PUSH32(esp, ecx);",
            "    PUSH32(esp, edx);",
            "    PUSH32(esp, 0x00120010u); RECOMP_ICALL_SAFE(eax, _icall_esp); /* indirect call */",
            "    eax = MEM32(esi + 8);",
        ]
        out, save = capture_index(lines)
        first_arg = next(i for i, l in enumerate(out) if l.strip() == "PUSH32(esp, ecx);")
        self.assertLess(save, first_arg,
                        "capture must sit above the argument pushes:\n" + "\n".join(out))

    def test_an_unrelated_esp_write_is_not_a_cleanup(self):
        # "esp = esp - N" is a frame allocation, not an argument cleanup.
        lines = [
            "    PUSH32(esp, ecx);",
            "    PUSH32(esp, 0x00120010u); RECOMP_ICALL_SAFE(eax, _icall_esp); /* indirect call */",
            "    esp = esp - 8;",
        ]
        out, save = capture_index(lines)
        first_arg = next(i for i, l in enumerate(out) if l.strip() == "PUSH32(esp, ecx);")
        self.assertLess(save, first_arg)

    def test_a_following_call_ends_the_search(self):
        # The cleanup after a *later* call says nothing about this one.
        lines = [
            "    PUSH32(esp, ecx);",
            "    PUSH32(esp, 0x00120010u); RECOMP_ICALL_SAFE(eax, _icall_esp); /* indirect call */",
            "    PUSH32(esp, 0x00120020u); sub_00130000(); /* call 0x00130000 */",
            "    esp = esp + 4;",
        ]
        out, save = capture_index(lines)
        first_arg = next(i for i, l in enumerate(out) if l.strip() == "PUSH32(esp, ecx);")
        self.assertLess(save, first_arg)


class SaveVersusArgument(unittest.TestCase):
    """The first push of a callee-saved register is the save; later ones are not.

    Both shapes below are real, and each defeats one of the simpler rules this
    code has used. They are tested together because a fix for either alone
    reintroduces the other.
    """

    def test_a_save_pushed_again_as_an_argument_is_absorbed(self):
        # sub_00135265: edi saved in the prologue AND pushed again as an
        # argument, popped once. Under "any register the function pops ends
        # the run", the run stopped at the argument push and left it outside
        # the rewind window -- the epilogue shifted and the caller got esi and
        # edi swapped.
        lines = [
            "    PUSH32(esp, ecx);",
            "    PUSH32(esp, esi);",
            "    PUSH32(esp, edi);",
            "    eax = MEM32(esi);",
            "    PUSH32(esp, edi);",
            "    ecx = esi;",
            "    PUSH32(esp, 0x00135299u); RECOMP_ICALL_SAFE(MEM32(eax + 0x68), _icall_esp); /* indirect call */",
            "    POP32(esp, edi);",
            "    POP32(esp, esi);",
            "    POP32(esp, ecx);",
        ]
        out, save = capture_index(lines)
        pushes = [i for i, l in enumerate(out) if l.strip() == "PUSH32(esp, edi);"]
        self.assertEqual(len(pushes), 2, "\n".join(out))
        self.assertGreater(save, pushes[0],
                           "the prologue save must stay above the capture:\n"
                           + "\n".join(out))
        self.assertLess(save, pushes[1],
                        "the argument push must be inside the rewind window:\n"
                        + "\n".join(out))

    def test_more_pushes_than_pops_does_not_make_the_save_an_argument(self):
        # sub_000EC240: esi saved once, passed as an argument twice more, and
        # popped in each of two epilogues -- three pushes against two pops.
        # A push/pop *count* rule reads that as "argument", swallows the save,
        # and a failed lookup rewinds g_esp past it so the epilogue pops the
        # wrong slot.
        lines = [
            "    PUSH32(esp, esi);",
            "    esi = MEM32(ecx + 4);",
            "    PUSH32(esp, esi);",
            "    PUSH32(esp, esi);",
            "    PUSH32(esp, 0x000EC271u); RECOMP_ICALL_SAFE(eax, _icall_esp); /* indirect call */",
            "    POP32(esp, esi);",
            "    return;",
            "loc_000EC280: ;",
            "    POP32(esp, esi);",
        ]
        out, save = capture_index(lines)
        pushes = [i for i, l in enumerate(out) if l.strip() == "PUSH32(esp, esi);"]
        self.assertEqual(len(pushes), 3, "\n".join(out))
        self.assertGreater(save, pushes[0],
                           "the save is the first push and must stay above the "
                           "capture:\n" + "\n".join(out))
        self.assertLess(save, pushes[1],
                        "both later pushes are arguments and belong inside the "
                        "rewind window:\n" + "\n".join(out))

    def test_a_register_never_popped_is_all_argument(self):
        # No pop anywhere, so nothing is a frame save and even the first push
        # is an argument.
        lines = [
            "    PUSH32(esp, edi);",
            "    PUSH32(esp, edi);",
            "    PUSH32(esp, 0x00120010u); RECOMP_ICALL_SAFE(eax, _icall_esp); /* indirect call */",
        ]
        out, save = capture_index(lines)
        pushes = [i for i, l in enumerate(out) if l.strip() == "PUSH32(esp, edi);"]
        self.assertLess(save, pushes[0], "\n".join(out))


class StillHonoursTheOlderRules(unittest.TestCase):
    """The convention check must not undo what the argument-run scan learned."""

    def test_a_callee_saved_push_still_ends_the_run(self):
        lines = [
            "    PUSH32(esp, esi);",
            "    PUSH32(esp, ecx);",
            "    PUSH32(esp, 0x00120010u); RECOMP_ICALL_SAFE(eax, _icall_esp); /* indirect call */",
            "    POP32(esp, esi);",
        ]
        out, save = capture_index(lines)
        esi_push = next(i for i, l in enumerate(out) if l.strip() == "PUSH32(esp, esi);")
        self.assertGreater(save, esi_push,
                           "the esi save belongs to the frame, not the call:\n" + "\n".join(out))


if __name__ == "__main__":
    unittest.main()
