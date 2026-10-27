"""Execute mixed zero-flag joins and the original US mounted companion AI."""


import hashlib
import struct

import pytest
from test_recomp_float_joins import (ROOT, compile_and_run, config,
                                    restore_section_mapping, translate)
from tools.recomp import translator
from tools.recomp.disasm import Operand
from tools.recomp.lifter import _make_condition


@pytest.mark.parametrize("alternate", ["f6c210", "80fa00"], ids=["mixed-operation", "mixed-width"])
@pytest.mark.parametrize("consumer", ["je", "jne", "sete", "setne", "cmove", "cmovne", "loope", "loopne"])
def test_mixed_zero_flag_consumers(tmp_path, consumer, alternate):
    # Each incoming edge computes a different ZF: cmp esi,0 / test dl,0x10.
    # Preserve that flag through a MOV block before consuming it.
    consumers = {
        "je": "7406b800000000c3b801000000c3",
        "jne": "7506b800000000c3b801000000c3",
        "sete": "b8000000000f94c0c3",
        "setne": "b8000000000f95c0c3",
        "cmove": "b800000000bf010000000f44c7c3",
        "cmovne": "b800000000bf010000000f45c7c3",
        "loope": "b902000000e106b800000000c3b801000000c3",
        "loopne": "b902000000e006b800000000c3b801000000c3",
    }
    image = bytes.fromhex("85c9740583fe00eb03" + alternate + "bb00000000eb00" + consumers[consumer])
    code = translate(image)
    nonzero = consumer in ("jne", "setne", "cmovne", "loopne")
    source = r'''
#include <stdio.h>
#include <stdlib.h>
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
/* CODE */
int main(void) {
    unsigned char *mem=calloc(1,0x20000);
    unsigned path,value;
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)mem;
    for(path=0;path<2;++path) for(value=0;value<256;++value) {
        int zf=path ? value==0 : /* ALTERNATE_ZF */;
        g_ecx=path;g_esi=value;g_edx=value;g_esp=0x18000;
        float_join();
        if(g_eax != (unsigned)(/* EXPECTED */) || g_esp!=0x18004) {
            fprintf(stderr,"FAIL: path %u value %u result %u zf %d\n",path,value,g_eax,zf);
            return 1;
        }
    }
    free(mem);puts("PASS: mixed ZF consumers, both incoming edges and polarities");return 0;
}
'''
    source = source.replace("/* CODE */", code).replace("/* EXPECTED */", "!zf" if nonzero else "zf")
    source = source.replace("/* ALTERNATE_ZF */", "(value&16)==0" if alternate == "f6c210" else "value==0")
    compile_and_run(tmp_path, source)


def test_joins_without_consumers_do_not_emit_undeclared_flag():
    # xor eax,eax; jmp join; inc ebx; join: mov eax,edx; ret.
    # Extracted entry points can include unreachable predecessor blocks.
    code = translate(bytes.fromhex("31c0eb014389d0c3"))
    assert "_flags" not in code


def test_unknown_edges_and_other_flags_remain_unknown():
    reg = Operand(type="reg", reg="eax", mem_size=4)
    imm = Operand(type="imm", imm=0)
    state = ("cmp", [reg, imm])
    assert translator._incoming_flag_state({1, 2}, {1: state, 2: None}, False) is None
    assert translator._incoming_flag_state({1, 2}, {1: state}, False) is None
    assert translator._incoming_flag_state({1}, {1: state}, True) is None
    for condition in ("ja", "jb", "jge", "js", "jo", "jp"):
        assert _make_condition(condition, "__zf_snapshot", []) is None


@pytest.mark.parametrize("legacy", [False, True], ids=["fixed", "pre-fix-reproduction"])
def test_original_mounted_companion(tmp_path, monkeypatch, legacy):
    xbe = ROOT / "private/game/default.xbe"
    if not xbe.is_file():
        pytest.skip("Supported US XBE required for companion AI integration")
    image = xbe.read_bytes()
    assert hashlib.sha256(image).hexdigest() == "3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc"
    config.configure_from_xbe(str(xbe))
    if legacy:
        def old_incoming(sources, known, is_entry):
            if is_entry or not sources or not all(p in known for p in sources):
                return None
            return translator._merge_flag_states([known[p] for p in sources])
        monkeypatch.setattr(translator, "_incoming_flag_state", old_incoming)
    info = {"end": 0x2A0C7A, "name": "sub_002A0500"}
    code = translator.FunctionTranslator(image, {0x2A0500: info}).translate_function(0x2A0500, info)
    # Initialize original jump tables, rather than replacing the AI dispatch.
    constants = "\n".join(
        f"MEM32(0x{address:X}) = 0x{struct.unpack_from('<I', image, config.va_to_file_offset(address))[0]:08X};"
        for address in range(0x2A0C7C, 0x2A0D10, 4))
    harness = (ROOT / "tests/gameplay/vehicle_companion.c").read_text(encoding="utf8")
    source = harness.replace("/* GENERATED_VEHICLE_ROUTINE */", code).replace("/* ORIGINAL_JUMP_TABLES */", constants)
    # The pre-fix run must exhibit the precise unwanted dismount, then exit
    # successfully to distinguish a reproduced bug from a crash or bad stub.
    source = source.replace("/* LEGACY */", "1" if legacy else "0")
    compile_and_run(tmp_path, source)
