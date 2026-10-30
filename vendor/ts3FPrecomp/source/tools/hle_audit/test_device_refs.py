"""
Tests for tools.hle_audit.device_refs.

    py -3 -m pytest tools/hle_audit/test_device_refs.py

No game files and no XbSymbolDatabase: byte strings stand in for code, and a
synthetic XBE from scripts/make_test_xbe.py with code patched into it stands in
for a title. Padding is int3 (0xCC), as in real linked code; zero bytes decode
as instructions and would blur where one instruction starts.
"""

import importlib.util
import struct
from pathlib import Path

from tools.hle_audit import device_refs as dr

ROOT = Path(__file__).resolve().parents[2]
DEV = 0x002256B8
BASE = 0x00011000


def test_absolute_memory_operands_are_references():
    blob = (b"\xCC" * 0x10
            + b"\xA1" + struct.pack("<I", DEV)            # mov eax, [DEV]
            + b"\xCC" * 0x10
            + b"\x8B\x0D" + struct.pack("<I", DEV)        # mov ecx, [DEV]
            + b"\xCC" * 0x10)
    assert dr.find_references(blob, BASE, DEV) == [BASE + 0x10, BASE + 0x25]


def test_an_overlapping_decode_does_not_move_the_reference():
    # "8B 0D <DEV>" also decodes as "0D <DEV>" (or eax, DEV) one byte later.
    # Both use the address; the reference is the instruction a linear decode
    # reaches, which starts at the 8B.
    blob = b"\xCC" * 0x20 + b"\x8B\x0D" + struct.pack("<I", DEV) + b"\xC3"
    assert dr.find_references(blob, BASE, DEV) == [BASE + 0x20]


def test_the_address_as_an_immediate_is_a_reference():
    blob = b"\xCC" * 8 + b"\x68" + struct.pack("<I", DEV) + b"\xCC" * 8   # push DEV
    assert dr.find_references(blob, BASE, DEV) == [BASE + 8]


def test_matching_bytes_that_are_not_an_operand_are_ignored():
    # A call whose rel32 happens to spell the address targets somewhere else.
    blob = b"\xCC" * 8 + b"\xE8" + struct.pack("<I", DEV) + b"\xCC" * 8
    assert dr.find_references(blob, BASE, DEV) == []


def test_direct_calls_and_tail_jumps_to_a_target_are_found():
    target = BASE + 0x100
    blob = bytearray(b"\xCC" * 0x40)

    def branch(at, opcode, dest):
        blob[at] = opcode
        struct.pack_into("<i", blob, at + 1, dest - (BASE + at + 5))

    branch(0x00, 0xE8, target)          # call target
    branch(0x10, 0xE9, target)          # jmp target, a tail call
    branch(0x20, 0xE8, target + 4)      # call somewhere else
    assert dr.find_calls(bytes(blob), BASE, target) == [BASE + 0x00, BASE + 0x10]


def test_outside_means_outside_by_section_and_by_name():
    sections = [dr.Section(".text", 0x10000, 0x1000, 0, 0x1000, True),
                dr.Section("D3D", 0x20000, 0x1000, 0, 0x1000, True)]
    symbols = [
        {"library": "D3D8", "kind": "function", "address": 0x10800,
         "name": "D3DDevice_Clear"},
        {"library": "XAPILIB", "kind": "function", "address": 0x10900,
         "name": "XapiSomething"},
    ]
    ranges = dr.named_ranges(symbols, sections)
    got = dr.classify([0x20010, 0x10810, 0x10910, 0x10100], sections, ranges)
    assert got["total"] == 4
    assert got["inside_by_section"] == 1          # 0x20010, in the D3D section
    assert got["inside_by_name"] == 1             # 0x10810, in D3DDevice_Clear
    assert got["outside_sites"] == ["0x00010910", "0x00010100"]


def _make_test_xbe():
    path = ROOT / "scripts" / "make_test_xbe.py"
    spec = importlib.util.spec_from_file_location("make_test_xbe", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _synthetic_title():
    """An XBE with a .text and an executable D3D section, each padded with
    int3. Returns (bytearray, {name: Section}, header offset of each section)."""
    data = bytearray(_make_test_xbe().build(title="Probe", extra_sections=("D3D",)))
    xbe = dr.parse_bytes(bytes(data))
    sections = {s.name: s for s in dr.sections_of(xbe)}
    header_base = xbe.header.section_headers_addr - xbe.header.base_address
    index = [s.name for s in xbe.sections].index("D3D")
    flags_at = header_base + index * 56
    struct.pack_into("<I", data, flags_at,
                     struct.unpack_from("<I", data, flags_at)[0] | 0x4)   # executable
    for name in (".text", "D3D"):
        sec = sections[name]
        data[sec.raw:sec.raw + sec.raw_size] = b"\xCC" * sec.raw_size
    return data, sections


def test_a_title_that_touches_the_device_outside_d3d8_is_reported():
    data, secs = _synthetic_title()
    text, d3d, rdata = secs[".text"], secs["D3D"], secs[".data"]
    device = rdata.va + 0x10
    begin_push = d3d.va + 0x400

    def put(sec, offset, code):
        data[sec.raw + offset:sec.raw + offset + len(code)] = code

    # Inside D3D8: the library reads its own device global.
    put(d3d, 0x000, b"\xA1" + struct.pack("<I", device))
    # Game code in .text reads it too, and fills its own push buffer.
    put(text, 0x100, b"\x8B\x0D" + struct.pack("<I", device))
    put(text, 0x200, b"\xE8" + struct.pack("<i", begin_push - (text.va + 0x200 + 5)))

    symbols = [
        {"library": "D3D8", "kind": "variable", "address": device, "name": "D3D_g_pDevice"},
        {"library": "D3D8", "kind": "function", "address": d3d.va, "name": "D3DDevice_Clear"},
        {"library": "D3D8", "kind": "function", "address": begin_push,
         "name": "D3DDevice_BeginPush_4"},
    ]
    report = dr.measure(bytes(data), symbols, "probe")

    assert report["title"] == "Probe"
    assert report["device_refs"]["total"] == 2
    assert report["device_refs"]["inside_by_section"] == 1
    assert report["device_refs"]["outside_sites"] == ["0x%08X" % (text.va + 0x100)]
    assert report["begin_push"]["total"] == 1
    assert report["begin_push"]["outside"] == 1
    assert "1 device-global reference(s) outside D3D8" in report["verdict"]
    assert "1 BeginPush call site(s) outside D3D8" in report["verdict"]


def test_a_title_that_stays_behind_the_api_holds():
    data, secs = _synthetic_title()
    d3d, rdata = secs["D3D"], secs[".data"]
    device = rdata.va + 0x10
    data[d3d.raw:d3d.raw + 5] = b"\xA1" + struct.pack("<I", device)
    symbols = [
        {"library": "D3D8", "kind": "variable", "address": device, "name": "D3D_g_pDevice"},
        {"library": "D3D8", "kind": "function", "address": d3d.va + 0x400,
         "name": "D3DDevice_BeginPush_4"},
    ]
    report = dr.measure(bytes(data), symbols, "probe")
    assert report["verdict"] == "D3D8 boundary holds"


def test_an_unnamed_device_global_is_not_a_pass():
    data, _ = _synthetic_title()
    report = dr.measure(bytes(data), [], "probe")
    assert report["device_refs"] is None
    assert report["verdict"].startswith("not measured")
