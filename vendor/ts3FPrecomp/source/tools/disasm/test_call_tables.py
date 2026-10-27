"""
Self-check for function-pointer tables inside code sections.

Run: py -3 tools/disasm/test_call_tables.py

D3D8's SetRenderState dispatches its complex states through
`call [state*4 + table]`, and the table lives in the D3D library section,
which is executable. _pass_data_ptr_targets scans only data sections, so the
handlers nothing calls directly were never found: Nightfire, Gauntlet and
Bloody Roar each stopped on the same 16 unresolved ICALLs after CreateDevice.

The displacement is where index 0 would be. The first real entry is 0x88
slots in, and slot 0 lands inside another function, so the table is looked
for in a window after the displacement, skipping words any body covers.
"""

import os
import sys
from types import SimpleNamespace

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))

from tools.disasm.engine import DisasmEngine  # noqa: E402
from tools.disasm.functions import FunctionDetector  # noqa: E402
from tools.disasm.test_alias_tail_jumps import _Img  # noqa: E402
from tools.disasm.test_decode_at import _Section  # noqa: E402

TEXT = 0x00010000
DISPATCH = TEXT + 0x000     # push esi; mov esi,[esp+8]; call [esi*4+disp]; ...
OTHER = TEXT + 0x040        # a known function the displacement lands inside
TABLE = TEXT + 0x100        # the used entries, in a gap after OTHER
H1, H2, H3 = TEXT + 0x200, TEXT + 0x210, TEXT + 0x220
FIRST_INDEX = 0x10          # the dispatch only ever uses indices >= this
DISP = TABLE - FIRST_INDEX * 4


class _Img2(_Img):
    @property
    def sections(self):
        return list(self._secs)


def _words(*values):
    return b"".join(v.to_bytes(4, "little") for v in values)


def _detector(dispatch_indexed=True, table_at=TABLE):
    code = bytearray(b"\xcc" * 0x300)
    if dispatch_indexed:
        call = b"\xff\x14\xb5" + DISP.to_bytes(4, "little")   # call [esi*4+DISP]
    else:
        call = b"\xff\x15" + DISP.to_bytes(4, "little") + b"\x90"  # call [DISP]
    body = b"\x56\x8b\x74\x24\x08" + call + b"\x5e\xc2\x04\x00"
    code[0x000:len(body)] = body
    code[0x040:0x0fe] = b"\x89\xc0" * 0x5f                 # OTHER's body
    code[0x0fe] = 0xc3
    off = table_at - TEXT
    code[off:off + 12] = _words(H1, H2, H3)
    for h in (H1, H2, H3):
        o = h - TEXT
        code[o:o + 5] = b"\x33\xc0\xc2\x04\x00"               # xor eax,eax; ret 4
    text = _Section(TEXT, bytes(code))
    text.data = bytes(code)

    image = _Img2(text)
    engine = DisasmEngine(image)
    engine.linear_sweep(text)
    det = FunctionDetector.__new__(FunctionDetector)
    det.engine = engine
    det.image = image
    det._candidates = {}
    det._alias_entries = {}
    det.functions = {
        DISPATCH: SimpleNamespace(start=DISPATCH, end=DISPATCH + len(body)),
        OTHER: SimpleNamespace(start=OTHER, end=OTHER + 0xbf),
    }
    return det, text


def test_the_dispatch_records_its_table():
    det, _ = _detector()
    assert DISP in det.engine.call_tables, det.engine.call_tables


def test_handlers_in_a_code_section_table_are_found():
    det, text = _detector()
    det._pass_data_ptr_targets([text])
    for h in (H1, H2, H3):
        assert h in det._alias_entries, (hex(h), det._alias_entries)


def test_a_table_no_indexed_call_names_is_not_read():
    # The same words, but the only reference is a plain `call [disp]`: nothing
    # says this region is a table, so a code section is not scanned for it.
    det, text = _detector(dispatch_indexed=False)
    det._pass_data_ptr_targets([text])
    for h in (H1, H2, H3):
        assert h not in det._alias_entries, (hex(h), det._alias_entries)


def test_words_inside_a_function_body_are_not_a_table():
    # Put the pointers inside OTHER's body: they are bytes of a function, not
    # a table, whatever they happen to look like.
    det, text = _detector(table_at=OTHER + 0x80)
    det._pass_data_ptr_targets([text])
    for h in (H1, H2, H3):
        assert h not in det._alias_entries, (hex(h), det._alias_entries)


def test_a_padding_rule_body_over_the_table_does_not_hide_it():
    # Gauntlet: the data before its table decodes to a ret and an int3, so the
    # cc_boundary pass made a "function" that covers the first 11 entries.
    det, text = _detector()
    det.functions[TABLE - 8] = SimpleNamespace(
        start=TABLE - 8, end=TABLE + 12, detection_method="cc_boundary")
    det._pass_data_ptr_targets([text])
    for h in (H1, H2, H3):
        assert h in det._alias_entries, (hex(h), det._alias_entries)


if __name__ == "__main__":
    for name, fn in list(globals().items()):
        if name.startswith("test_"):
            fn()
            print("ok", name)
